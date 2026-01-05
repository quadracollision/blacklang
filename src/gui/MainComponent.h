#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PatternColumn.h"
#include "TransportBar.h"
#include "PatternEditorPopup.h"
#include "../AudioEngine.h"
#include <memory>
#include <vector>

class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer {
public:
    MainComponent(AudioEngine& engine) : audioEngine(engine) {
        setSize(900, 600);
        
        // Add column button
        addColumnButton.setButtonText("+ Column");
        addColumnButton.onClick = [this]() { addColumn(); };
        addAndMakeVisible(addColumnButton);
        
        // Transport bar
        transportBar.onPlay = [this]() { play(); };
        transportBar.onPause = [this]() { pause(); };
        transportBar.onStop = [this]() { stop(); };
        transportBar.onBpmChanged = [this](int bpm) { currentBpm = bpm; };
        addAndMakeVisible(transportBar);
        
        // Create initial column
        addColumn();
        
        // Add some demo patterns if available
        refreshPatternsFromEngine();
        
        startTimer(100); // Update playback state
    }
    
    void paint(juce::Graphics& g) override {
        // Background gradient
        juce::ColourGradient gradient(
            juce::Colour(0xff0f0f0f), 0, 0,
            juce::Colour(0xff1a1a1a), 0, (float)getHeight(),
            false);
        g.setGradientFill(gradient);
        g.fillRect(getLocalBounds());
        
        // Title
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(24.0f, juce::Font::bold));
        g.drawText("BlackLang", 20, 10, 200, 30, juce::Justification::left);
    }
    
    void resized() override {
        auto bounds = getLocalBounds();
        
        // Transport at bottom
        transportBar.setBounds(bounds.removeFromBottom(50));
        
        // Top bar with add column button
        auto topBar = bounds.removeFromTop(50);
        addColumnButton.setBounds(topBar.removeFromRight(100).reduced(10));
        
        // Columns area
        bounds = bounds.reduced(10);
        int columnWidth = 150;
        int x = 0;
        
        for (auto& col : columns) {
            col->setBounds(x, bounds.getY(), columnWidth, bounds.getHeight());
            x += columnWidth + 10;
        }
    }
    
    void addColumn() {
        auto col = std::make_unique<PatternColumn>("Track " + juce::String(columns.size() + 1).toStdString());
        
        col->onPatternSelectionChanged = [this](PatternBox* box, bool selected) {
            handlePatternSelection(box, selected);
        };
        col->onPatternDoubleClicked = [this](PatternBox* box) {
            openPatternEditor(box);
        };
        col->onPatternDropped = [this](const std::string& name, PatternColumn* targetCol) {
            handlePatternDrop(name, targetCol);
        };
        col->onAddPatternClicked = [this](PatternColumn* targetCol) {
            createNewPattern(targetCol);
        };
        
        addAndMakeVisible(*col);
        columns.push_back(std::move(col));
        resized();
    }
    
    void refreshPatternsFromEngine() {
        // Load existing patterns from engine into first column
        if (columns.empty()) return;
        
        auto& firstCol = columns[0];
        firstCol->clearPatterns();
        
        for (const auto& [name, pattern] : audioEngine.getPatterns()) {
            firstCol->addPattern(name, &pattern);
        }
        resized();
    }
    
private:
    AudioEngine& audioEngine;
    std::vector<std::unique_ptr<PatternColumn>> columns;
    TransportBar transportBar;
    juce::TextButton addColumnButton;
    
    std::vector<std::string> selectedPatterns;
    int currentBpm = 120;
    bool isPaused = false;
    
    void handlePatternSelection(PatternBox* box, bool selected) {
        const std::string& name = box->getName();
        
        if (selected) {
            // Deselect other patterns in the same column
            for (auto& col : columns) {
                for (const auto& b : col->getPatternBoxes()) {
                    if (b.get() != box && b->isSelected()) {
                        // Check if in same column
                        if (box->getParentComponent() == b->getParentComponent()) {
                            b->setSelected(false);
                        }
                    }
                }
            }
            
            // Add to selected if not already
            if (std::find(selectedPatterns.begin(), selectedPatterns.end(), name) == selectedPatterns.end()) {
                selectedPatterns.push_back(name);
            }
        } else {
            selectedPatterns.erase(
                std::remove(selectedPatterns.begin(), selectedPatterns.end(), name),
                selectedPatterns.end());
        }
    }
    
    void handlePatternDrop(const std::string& name, PatternColumn* targetCol) {
        Pattern* pattern = audioEngine.getPattern(name);
        if (pattern) {
            targetCol->addPattern(name, pattern);
        }
    }
    
    void openPatternEditor(PatternBox* box) {
        Pattern* pattern = audioEngine.getPattern(box->getName());
        
        auto editor = std::make_unique<PatternEditorPopup>(pattern, 
            [this, box](const Pattern& updated) {
                Pattern newPattern = updated;
                if (audioEngine.loadSample(newPattern)) {
                    audioEngine.addPattern(newPattern);
                    box->updatePattern(audioEngine.getPattern(newPattern.name));
                }
            });
        
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(editor.release());
        options.dialogTitle = "Edit Pattern";
        options.dialogBackgroundColour = juce::Colour(0xff1e1e1e);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = false;
        options.resizable = false;
        options.launchAsync();
    }
    
    void createNewPattern(PatternColumn* targetCol) {
        Pattern newPattern;
        newPattern.name = "Pattern" + std::to_string(rand() % 1000);
        newPattern.bpm = currentBpm;
        newPattern.steps = 16;
        
        auto editor = std::make_unique<PatternEditorPopup>(&newPattern,
            [this, targetCol](const Pattern& created) {
                Pattern p = created;
                if (!p.samplePath.empty() && audioEngine.loadSample(p)) {
                    audioEngine.addPattern(p);
                    targetCol->addPattern(p.name, audioEngine.getPattern(p.name));
                }
            });
        
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(editor.release());
        options.dialogTitle = "Create Pattern";
        options.dialogBackgroundColour = juce::Colour(0xff1e1e1e);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = false;
        options.resizable = false;
        options.launchAsync();
    }
    
    void play() {
        if (selectedPatterns.empty()) {
            // Play first selected from each column
            for (auto& col : columns) {
                if (auto* sel = col->getSelectedPattern()) {
                    selectedPatterns.push_back(sel->getName());
                }
            }
        }
        
        if (!selectedPatterns.empty()) {
            audioEngine.playMultiplePatterns(selectedPatterns);
            transportBar.setPlaying(true);
            isPaused = false;
        }
    }
    
    void pause() {
        audioEngine.pause();
        isPaused = true;
        transportBar.setPlaying(false);
    }
    
    void stop() {
        audioEngine.stop();
        transportBar.setPlaying(false);
        isPaused = false;
    }
    
    void timerCallback() {
        transportBar.setPlaying(audioEngine.isPlaying());
    }
    
    void startTimer(int intervalMs) {
        juce::Timer::callAfterDelay(intervalMs, [this, intervalMs]() {
            if (this) {
                timerCallback();
                startTimer(intervalMs);
            }
        });
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
