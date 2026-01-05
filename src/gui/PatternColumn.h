#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PatternBox.h"
#include <vector>
#include <memory>
#include <functional>

class PatternColumn : public juce::Component,
                      public juce::DragAndDropTarget {
public:
    PatternColumn(const std::string& title = "Column")
        : columnTitle(title) {
        addButton.setButtonText("+");
        addButton.onClick = [this]() {
            if (onAddPatternClicked) onAddPatternClicked(this);
        };
        addAndMakeVisible(addButton);
    }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds();
        
        // Background
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
        
        // Border
        g.setColour(juce::Colour(0xff333333));
        g.drawRoundedRectangle(bounds.toFloat().reduced(1), 8.0f, 1.0f);
        
        // Title
        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText(columnTitle, bounds.removeFromTop(30), juce::Justification::centred);
        
        // Drop indicator
        if (isDragOver) {
            g.setColour(juce::Colour(0x443a7bd5));
            g.fillRoundedRectangle(bounds.reduced(4).toFloat(), 4.0f);
        }
    }
    
    void resized() override {
        auto bounds = getLocalBounds().reduced(8);
        bounds.removeFromTop(30); // Title area
        
        // Position pattern boxes
        int y = bounds.getY();
        for (auto& box : patternBoxes) {
            box->setBounds(bounds.getX(), y, bounds.getWidth(), 80);
            y += 85;
        }
        
        // Add button at bottom
        addButton.setBounds(bounds.getX(), getHeight() - 40, bounds.getWidth(), 30);
    }
    
    PatternBox* addPattern(const std::string& name, const Pattern* pattern = nullptr) {
        auto box = std::make_unique<PatternBox>(name, pattern);
        box->onSelectionChanged = [this](PatternBox* b, bool sel) {
            if (onPatternSelectionChanged) onPatternSelectionChanged(b, sel);
        };
        box->onDoubleClick = [this](PatternBox* b) {
            if (onPatternDoubleClicked) onPatternDoubleClicked(b);
        };
        box->onPatternDropped = [this](const std::string& src, PatternBox* target) {
            if (onPatternDroppedOnBox) onPatternDroppedOnBox(src, target, this);
        };
        
        PatternBox* ptr = box.get();
        addAndMakeVisible(*box);
        patternBoxes.push_back(std::move(box));
        resized();
        return ptr;
    }
    
    void removePattern(PatternBox* box) {
        auto it = std::find_if(patternBoxes.begin(), patternBoxes.end(),
            [box](const auto& p) { return p.get() == box; });
        if (it != patternBoxes.end()) {
            removeChildComponent(it->get());
            patternBoxes.erase(it);
            resized();
        }
    }
    
    void clearPatterns() {
        for (auto& box : patternBoxes) {
            removeChildComponent(box.get());
        }
        patternBoxes.clear();
        resized();
    }
    
    // DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails&) override { return true; }
    
    void itemDragEnter(const SourceDetails&) override {
        isDragOver = true;
        repaint();
    }
    
    void itemDragExit(const SourceDetails&) override {
        isDragOver = false;
        repaint();
    }
    
    void itemDropped(const SourceDetails& details) override {
        isDragOver = false;
        repaint();
        if (onPatternDropped) {
            onPatternDropped(details.description.toString().toStdString(), this);
        }
    }
    
    const std::vector<std::unique_ptr<PatternBox>>& getPatternBoxes() const { 
        return patternBoxes; 
    }
    
    PatternBox* getSelectedPattern() const {
        for (const auto& box : patternBoxes) {
            if (box->isSelected()) return box.get();
        }
        return nullptr;
    }
    
    void setTitle(const std::string& title) { 
        columnTitle = title; 
        repaint(); 
    }
    
    // Callbacks
    std::function<void(PatternBox*, bool)> onPatternSelectionChanged;
    std::function<void(PatternBox*)> onPatternDoubleClicked;
    std::function<void(const std::string&, PatternColumn*)> onPatternDropped;
    std::function<void(const std::string&, PatternBox*, PatternColumn*)> onPatternDroppedOnBox;
    std::function<void(PatternColumn*)> onAddPatternClicked;
    
private:
    std::string columnTitle;
    std::vector<std::unique_ptr<PatternBox>> patternBoxes;
    juce::TextButton addButton;
    bool isDragOver = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternColumn)
};
