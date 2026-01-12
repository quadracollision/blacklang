#include "AudioRecorder.h"
#include <thread>
#include <chrono>
#include <iostream>

AudioRecorder::AudioRecorder() {
    // Allocation deferred to start()
}

AudioRecorder::~AudioRecorder() {
    stop();
}

void AudioRecorder::start(const std::string& filePath, double sampleRate, int numChannels) {
    if (workerRunning) stop();
    
    // Allocate buffer if needed (lazy alloc)
    if (buffer.size() != BUFFER_SAMPLES) {
        buffer.resize(BUFFER_SAMPLES, 0.0f);
    }
    
    // Reset buffer
    writeIndex = 0;
    readIndex = 0;
    currentNumChannels = numChannels;
    
    recording = true;
    workerRunning = true;
    workerThread = std::thread(&AudioRecorder::backgroundTask, this, filePath, sampleRate, numChannels);
}

void AudioRecorder::stop() {
    if (recording) {
        recording = false;
        // Wait for worker to finish writing remaining buffer
        if (workerThread.joinable()) {
            workerThread.join();
        }
        workerRunning = false;
    }
}

bool AudioRecorder::isRecording() const {
    return recording;
}

void AudioRecorder::writeBlock(const float* const* channelData, int numSamples, int numIncomingChannels) {
    if (!recording) return;
    
    int channels = currentNumChannels; // File channels (e.g. 2)
    size_t currentWrite = writeIndex.load(std::memory_order_relaxed);
    
    for (int i = 0; i < numSamples; ++i) {
        for (int ch = 0; ch < channels; ++ch) {
            float sample = 0.0f;
            
            // Map incoming to file channels
            if (ch < numIncomingChannels) {
                sample = channelData[ch][i];
            } else if (numIncomingChannels == 1 && ch == 1) {
                // Duplicate Mono to Stereo Right
                sample = channelData[0][i];
            }
            
            buffer[currentWrite & (BUFFER_SAMPLES - 1)] = sample;
            currentWrite++;
        }
    }
    
    writeIndex.store(currentWrite, std::memory_order_release);
}

void AudioRecorder::backgroundTask(const std::string& path, double sr, int channels) {
    juce::File file(path);
    // Ensure parent directory exists
    file.getParentDirectory().createDirectory();
    file.deleteFile(); // Overwrite

    std::unique_ptr<juce::FileOutputStream> fileStream(file.createOutputStream());
    if (!fileStream) {
        std::cerr << "Failed to open file for recording: " << path << std::endl;
        recording = false;
        return;
    }

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(fileStream.get(), sr, channels, 24, {}, 0));
    
    if (!writer) {
        std::cerr << "Failed to create WAV writer" << std::endl;
        recording = false;
        return;
    }
    
    // release stream ownership to writer
    fileStream.release(); 
    
    // Temp buffer for de-interleaving/writing
    // JUCE writer takes AudioBuffer<float> or float**
    // We can write chunks of e.g. 2048 samples
    const int CHUNK_SIZE = 2048;
    juce::AudioBuffer<float> writeBuffer(channels, CHUNK_SIZE);
    
    while (recording || readIndex < writeIndex) {
        size_t w = writeIndex.load(std::memory_order_acquire);
        size_t r = readIndex.load(std::memory_order_relaxed);
        
        size_t availableFloats = w - r;
        int availableFrames = availableFloats / channels;
        
        if (availableFrames >= CHUNK_SIZE) {
            // Process chunk
            for (int i = 0; i < CHUNK_SIZE; ++i) {
                for (int ch = 0; ch < channels; ++ch) {
                    float sample = buffer[(r) & (BUFFER_SAMPLES - 1)];
                    writeBuffer.setSample(ch, i, sample);
                    r++;
                }
            }
            
            writer->writeFromAudioSampleBuffer(writeBuffer, 0, CHUNK_SIZE);
            readIndex.store(r, std::memory_order_release);
        } else {
            // Wait for more data
            if (!recording && (w == r)) break; // Done
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    
    // Finish any remainder
    size_t w = writeIndex.load(std::memory_order_acquire);
    size_t r = readIndex.load(std::memory_order_relaxed);
    if (w > r) {
        int remFrames = (w - r) / channels;
        if (remFrames > 0) {
             juce::AudioBuffer<float> remBuffer(channels, remFrames);
              for (int i = 0; i < remFrames; ++i) {
                for (int ch = 0; ch < channels; ++ch) {
                    float sample = buffer[(r) & (BUFFER_SAMPLES - 1)];
                    remBuffer.setSample(ch, i, sample);
                    r++;
                }
            }
            writer->writeFromAudioSampleBuffer(remBuffer, 0, remFrames);
        }
    }
    
    writer.reset(); // flushes and closes
}
