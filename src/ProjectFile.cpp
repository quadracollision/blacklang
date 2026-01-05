#include "ProjectFile.h"
#include <juce_core/juce_core.h>
#include <fstream>
#include <iostream>

bool ProjectFile::save(const std::string& filename,
                       const std::map<std::string, Pattern>& patterns,
                       const PatternChain& currentChain) {
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    
    // Save patterns
    juce::Array<juce::var> patternsArray;
    for (const auto& [name, pattern] : patterns) {
        juce::DynamicObject::Ptr p = new juce::DynamicObject();
        p->setProperty("name", juce::String(pattern.name));
        p->setProperty("sample", juce::String(pattern.samplePath));
        p->setProperty("bpm", pattern.bpm);
        p->setProperty("steps", pattern.steps);
        
        juce::Array<juce::var> stepsArray;
        for (int step : pattern.activeSteps) {
            stepsArray.add(step);
        }
        p->setProperty("activeSteps", stepsArray);
        
        patternsArray.add(juce::var(p.get()));
    }
    root->setProperty("patterns", patternsArray);
    
    // Save chain
    juce::Array<juce::var> chainArray;
    for (const auto& name : currentChain.getPatterns()) {
        chainArray.add(juce::String(name));
    }
    root->setProperty("chain", chainArray);
    
    // Write to file
    juce::var json(root.get());
    juce::String jsonStr = juce::JSON::toString(json);
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot write to: " << filename << std::endl;
        return false;
    }
    
    file << jsonStr.toStdString();
    return true;
}

bool ProjectFile::load(const std::string& filename,
                       std::map<std::string, Pattern>& patterns,
                       PatternChain& currentChain) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open: " << filename << std::endl;
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    juce::var json = juce::JSON::parse(juce::String(content));
    if (!json.isObject()) {
        std::cerr << "Invalid project file" << std::endl;
        return false;
    }
    
    patterns.clear();
    
    // Load patterns
    juce::var patternsVar = json["patterns"];
    if (patternsVar.isArray()) {
        for (int i = 0; i < patternsVar.size(); ++i) {
            juce::var p = patternsVar[i];
            Pattern pattern;
            pattern.name = p["name"].toString().toStdString();
            pattern.samplePath = p["sample"].toString().toStdString();
            pattern.bpm = (int)p["bpm"];
            pattern.steps = (int)p["steps"];
            
            juce::var stepsVar = p["activeSteps"];
            if (stepsVar.isArray()) {
                for (int j = 0; j < stepsVar.size(); ++j) {
                    pattern.activeSteps.push_back((int)stepsVar[j]);
                }
            }
            
            patterns[pattern.name] = pattern;
        }
    }
    
    // Load chain
    currentChain.clear();
    juce::var chainVar = json["chain"];
    if (chainVar.isArray()) {
        std::string chainStr;
        for (int i = 0; i < chainVar.size(); ++i) {
            if (i > 0) chainStr += ", ";
            chainStr += chainVar[i].toString().toStdString();
        }
        currentChain.parse(chainStr);
    }
    
    return true;
}
