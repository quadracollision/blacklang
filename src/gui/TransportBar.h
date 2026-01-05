#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class TransportBar : public juce::Component {
public:
    TransportBar() {
        // Play button
        playButton.setButtonText("▶");
        playButton.onClick = [this]() {
            if (onPlay) onPlay();
        };
        addAndMakeVisible(playButton);
        
        // Pause button
        pauseButton.setButtonText("⏸");
        pauseButton.onClick = [this]() {
            if (onPause) onPause();
        };
        addAndMakeVisible(pauseButton);
        
        // Stop button
        stopButton.setButtonText("⏹");
        stopButton.onClick = [this]() {
            if (onStop) onStop();
        };
        addAndMakeVisible(stopButton);
        
        // BPM label
        bpmLabel.setText("BPM:", juce::dontSendNotification);
        bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(bpmLabel);
        
        // BPM editor
        bpmEditor.setText("120");
        bpmEditor.setInputRestrictions(3, "0123456789");
        bpmEditor.onReturnKey = [this]() {
            int bpm = bpmEditor.getText().getIntValue();
            if (bpm > 0 && onBpmChanged) onBpmChanged(bpm);
        };
        addAndMakeVisible(bpmEditor);
        
        // Style buttons
        auto styleButton = [](juce::TextButton& btn) {
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
            btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff3a7bd5));
        };
        styleButton(playButton);
        styleButton(pauseButton);
        styleButton(stopButton);
    }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds();
        
        // Background
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRect(bounds);
        
        // Top border
        g.setColour(juce::Colour(0xff333333));
        g.drawLine(0, 0, (float)getWidth(), 0, 1.0f);
        
        // Status indicator
        auto statusRect = bounds.removeFromRight(20).reduced(5);
        g.setColour(isPlaying ? juce::Colour(0xff4caf50) : juce::Colour(0xff666666));
        g.fillEllipse(statusRect.toFloat());
    }
    
    void resized() override {
        auto bounds = getLocalBounds().reduced(10, 5);
        
        int buttonSize = 40;
        playButton.setBounds(bounds.removeFromLeft(buttonSize));
        bounds.removeFromLeft(5);
        pauseButton.setBounds(bounds.removeFromLeft(buttonSize));
        bounds.removeFromLeft(5);
        stopButton.setBounds(bounds.removeFromLeft(buttonSize));
        
        bounds.removeFromLeft(20);
        bpmLabel.setBounds(bounds.removeFromLeft(40));
        bpmEditor.setBounds(bounds.removeFromLeft(50));
    }
    
    void setPlaying(bool playing) { 
        isPlaying = playing; 
        repaint(); 
    }
    
    void setBpm(int bpm) {
        bpmEditor.setText(juce::String(bpm), juce::dontSendNotification);
    }
    
    // Callbacks
    std::function<void()> onPlay;
    std::function<void()> onPause;
    std::function<void()> onStop;
    std::function<void(int)> onBpmChanged;
    
private:
    juce::TextButton playButton;
    juce::TextButton pauseButton;
    juce::TextButton stopButton;
    juce::Label bpmLabel;
    juce::TextEditor bpmEditor;
    bool isPlaying = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
