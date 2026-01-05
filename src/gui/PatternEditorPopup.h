#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Pattern.h"
#include <functional>

class PatternEditorPopup : public juce::Component {
public:
    PatternEditorPopup(Pattern* pattern, std::function<void(const Pattern&)> onSave)
        : editingPattern(pattern), onSaveCallback(onSave) {
        
        setSize(400, 350);
        
        // Title
        titleLabel.setText("Edit Pattern", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);
        
        // Name field
        nameLabel.setText("Name:", juce::dontSendNotification);
        nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(nameLabel);
        nameEditor.setText(pattern ? pattern->name : "");
        addAndMakeVisible(nameEditor);
        
        // Sample path
        sampleLabel.setText("Sample:", juce::dontSendNotification);
        sampleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(sampleLabel);
        sampleEditor.setText(pattern ? pattern->samplePath : "");
        addAndMakeVisible(sampleEditor);
        
        browseButton.setButtonText("...");
        browseButton.onClick = [this]() { browseSample(); };
        addAndMakeVisible(browseButton);
        
        // BPM
        bpmLabel.setText("BPM:", juce::dontSendNotification);
        bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(bpmLabel);
        bpmEditor.setText(pattern ? juce::String(pattern->bpm) : "120");
        bpmEditor.setInputRestrictions(3, "0123456789");
        addAndMakeVisible(bpmEditor);
        
        // Steps
        stepsLabel.setText("Steps:", juce::dontSendNotification);
        stepsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(stepsLabel);
        stepsEditor.setText(pattern ? juce::String(pattern->steps) : "16");
        stepsEditor.setInputRestrictions(2, "0123456789");
        stepsEditor.onTextChange = [this]() { updateGrid(); };
        addAndMakeVisible(stepsEditor);
        
        // Initialize step states
        if (pattern) {
            for (int s : pattern->activeSteps) {
                if (s > 0 && s <= 64) stepStates[s - 1] = true;
            }
        }
        
        // Save/Cancel buttons
        saveButton.setButtonText("Save");
        saveButton.onClick = [this]() { save(); };
        addAndMakeVisible(saveButton);
        
        cancelButton.setButtonText("Cancel");
        cancelButton.onClick = [this]() {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(0);
        };
        addAndMakeVisible(cancelButton);
    }
    
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff1e1e1e));
        
        // Draw step grid
        auto gridArea = getLocalBounds()
            .reduced(20)
            .withTop(170)
            .withBottom(getHeight() - 60);
        
        g.setColour(juce::Colour(0xff2a2a2a));
        g.fillRoundedRectangle(gridArea.toFloat(), 4.0f);
        
        int steps = stepsEditor.getText().getIntValue();
        if (steps <= 0) steps = 16;
        steps = std::min(steps, 64);
        
        int cols = std::min(16, steps);
        int rows = (steps + cols - 1) / cols;
        
        float cellW = gridArea.getWidth() / (float)cols;
        float cellH = gridArea.getHeight() / (float)rows;
        float size = std::min(cellW, cellH) * 0.8f;
        
        for (int i = 0; i < steps; ++i) {
            int col = i % cols;
            int row = i / cols;
            
            float x = gridArea.getX() + col * cellW + (cellW - size) / 2;
            float y = gridArea.getY() + row * cellH + (cellH - size) / 2;
            
            if (stepStates[i]) {
                g.setColour(juce::Colour(0xffff6b6b));
                g.fillRoundedRectangle(x, y, size, size, 3.0f);
            } else {
                g.setColour(juce::Colour(0xff444444));
                g.fillRoundedRectangle(x, y, size, size, 3.0f);
                g.setColour(juce::Colour(0xff555555));
                g.drawRoundedRectangle(x, y, size, size, 3.0f, 1.0f);
            }
        }
    }
    
    void resized() override {
        auto bounds = getLocalBounds().reduced(20);
        
        titleLabel.setBounds(bounds.removeFromTop(30));
        bounds.removeFromTop(10);
        
        auto row = bounds.removeFromTop(25);
        nameLabel.setBounds(row.removeFromLeft(60));
        nameEditor.setBounds(row);
        bounds.removeFromTop(8);
        
        row = bounds.removeFromTop(25);
        sampleLabel.setBounds(row.removeFromLeft(60));
        browseButton.setBounds(row.removeFromRight(30));
        row.removeFromRight(5);
        sampleEditor.setBounds(row);
        bounds.removeFromTop(8);
        
        row = bounds.removeFromTop(25);
        bpmLabel.setBounds(row.removeFromLeft(60));
        bpmEditor.setBounds(row.removeFromLeft(60));
        row.removeFromLeft(20);
        stepsLabel.setBounds(row.removeFromLeft(50));
        stepsEditor.setBounds(row.removeFromLeft(40));
        bounds.removeFromTop(8);
        
        // Bottom buttons
        auto buttonRow = getLocalBounds().reduced(20).removeFromBottom(35);
        cancelButton.setBounds(buttonRow.removeFromRight(80));
        buttonRow.removeFromRight(10);
        saveButton.setBounds(buttonRow.removeFromRight(80));
    }
    
    void mouseDown(const juce::MouseEvent& e) override {
        // Check if click is in grid area
        auto gridArea = getLocalBounds()
            .reduced(20)
            .withTop(170)
            .withBottom(getHeight() - 60);
        
        if (!gridArea.contains(e.getPosition())) return;
        
        int steps = stepsEditor.getText().getIntValue();
        if (steps <= 0) steps = 16;
        steps = std::min(steps, 64);
        
        int cols = std::min(16, steps);
        float cellW = gridArea.getWidth() / (float)cols;
        float cellH = gridArea.getHeight() / (float)((steps + cols - 1) / cols);
        
        int col = (int)((e.x - gridArea.getX()) / cellW);
        int row = (int)((e.y - gridArea.getY()) / cellH);
        int stepIndex = row * cols + col;
        
        if (stepIndex >= 0 && stepIndex < steps) {
            stepStates[stepIndex] = !stepStates[stepIndex];
            repaint();
        }
    }
    
private:
    Pattern* editingPattern;
    std::function<void(const Pattern&)> onSaveCallback;
    bool stepStates[64] = {false};
    
    juce::Label titleLabel;
    juce::Label nameLabel, sampleLabel, bpmLabel, stepsLabel;
    juce::TextEditor nameEditor, sampleEditor, bpmEditor, stepsEditor;
    juce::TextButton browseButton, saveButton, cancelButton;
    
    void updateGrid() {
        repaint();
    }
    
    void browseSample() {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Sample",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.wav;*.mp3;*.aif;*.aiff;*.flac");
            
        chooser->launchAsync(juce::FileBrowserComponent::openMode | 
                            juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                if (fc.getResults().size() > 0) {
                    sampleEditor.setText(fc.getResult().getFullPathName());
                }
            });
    }
    
    void save() {
        Pattern newPattern;
        newPattern.name = nameEditor.getText().toStdString();
        newPattern.samplePath = sampleEditor.getText().toStdString();
        newPattern.bpm = bpmEditor.getText().getIntValue();
        newPattern.steps = stepsEditor.getText().getIntValue();
        
        if (newPattern.steps <= 0) newPattern.steps = 16;
        newPattern.steps = std::min(newPattern.steps, 64);
        
        for (int i = 0; i < newPattern.steps; ++i) {
            if (stepStates[i]) {
                newPattern.activeSteps.push_back(i + 1);
            }
        }
        
        if (onSaveCallback) onSaveCallback(newPattern);
        
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(1);
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternEditorPopup)
};
