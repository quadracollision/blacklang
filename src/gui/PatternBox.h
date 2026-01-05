#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Pattern.h"
#include <functional>

class PatternBox : public juce::Component, 
                   public juce::DragAndDropContainer,
                   public juce::DragAndDropTarget {
public:
    PatternBox(const std::string& patternName, const Pattern* pattern = nullptr)
        : name(patternName), patternData(pattern) {
        setSize(120, 80);
    }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().reduced(2);
        
        // Background
        if (selected) {
            g.setColour(juce::Colour(0xff3a7bd5));
        } else if (isMouseOver()) {
            g.setColour(juce::Colour(0xff404040));
        } else {
            g.setColour(juce::Colour(0xff2a2a2a));
        }
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
        
        // Border
        g.setColour(selected ? juce::Colour(0xff5a9bf5) : juce::Colour(0xff555555));
        g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 1.5f);
        
        // Pattern name
        g.setColour(juce::Colours::white);
        g.setFont(12.0f);
        g.drawText(name, bounds.removeFromTop(18), juce::Justification::centred);
        
        // Draw step grid
        drawStepGrid(g, bounds.reduced(4, 2));
    }
    
    void drawStepGrid(juce::Graphics& g, juce::Rectangle<int> area) {
        if (!patternData) return;
        
        int steps = patternData->steps;
        int cols = std::min(16, steps);
        int rows = (steps + cols - 1) / cols;
        
        float cellW = area.getWidth() / (float)cols;
        float cellH = area.getHeight() / (float)rows;
        float size = std::min(cellW, cellH) * 0.8f;
        
        for (int step = 1; step <= steps; ++step) {
            int col = (step - 1) % cols;
            int row = (step - 1) / cols;
            
            float x = area.getX() + col * cellW + (cellW - size) / 2;
            float y = area.getY() + row * cellH + (cellH - size) / 2;
            
            bool active = patternData->shouldTriggerAt(step);
            
            if (active) {
                g.setColour(juce::Colour(0xffff6b6b));
                g.fillRoundedRectangle(x, y, size, size, 2.0f);
            } else {
                g.setColour(juce::Colour(0xff444444));
                g.drawRoundedRectangle(x, y, size, size, 2.0f, 1.0f);
            }
        }
    }
    
    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isLeftButtonDown()) {
            dragStartPos = e.getPosition();
        }
    }
    
    void mouseDrag(const juce::MouseEvent& e) override {
        if (e.getDistanceFromDragStart() > 5) {
            auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this);
            if (container && !container->isDragAndDropActive()) {
                container->startDragging(juce::String(name), this);
            }
        }
    }
    
    void mouseUp(const juce::MouseEvent& e) override {
        if (e.getDistanceFromDragStart() < 5) {
            setSelected(!selected);
            if (onSelectionChanged) onSelectionChanged(this, selected);
        }
    }
    
    void mouseDoubleClick(const juce::MouseEvent&) override {
        if (onDoubleClick) onDoubleClick(this);
    }
    
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    
    // DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails&) override { return true; }
    void itemDropped(const SourceDetails& details) override {
        if (onPatternDropped) {
            onPatternDropped(details.description.toString().toStdString(), this);
        }
    }
    
    void setSelected(bool sel) { selected = sel; repaint(); }
    bool isSelected() const { return selected; }
    const std::string& getName() const { return name; }
    
    void updatePattern(const Pattern* p) { patternData = p; repaint(); }
    
    // Callbacks
    std::function<void(PatternBox*, bool)> onSelectionChanged;
    std::function<void(PatternBox*)> onDoubleClick;
    std::function<void(const std::string&, PatternBox*)> onPatternDropped;
    
private:
    std::string name;
    const Pattern* patternData = nullptr;
    bool selected = false;
    juce::Point<int> dragStartPos;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternBox)
};
