#include "ProjectFile.h"
#include <juce_core/juce_core.h>
#include <fstream>
#include <iostream>

bool ProjectFile::save(const std::string& filename,
                       const std::map<std::string, Pattern>& patterns,
                       const PatternChain& currentChain,
                       const std::vector<SerializedColumn>& columns) {
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    
    // Save Column Layout
    juce::Array<juce::var> colsArray;
    for (const auto& col : columns) {
        juce::DynamicObject::Ptr cObj = new juce::DynamicObject();
        cObj->setProperty("title", juce::String(col.title));
        
        juce::Array<juce::var> pNames;
        for (const auto& pName : col.patternNames) {
            pNames.add(juce::String(pName));
        }
        cObj->setProperty("patterns", pNames);
        colsArray.add(juce::var(cObj.get()));
    }
    root->setProperty("layout", colsArray);

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
        
        // Slice Markers
        juce::Array<juce::var> slicesArray;
        for (int marker : pattern.sliceMarkers) {
            slicesArray.add(marker);
        }
        p->setProperty("sliceMarkers", slicesArray);

        // Pitches
        juce::DynamicObject::Ptr pitchesObj = new juce::DynamicObject();
        for (const auto& [step, pitch] : pattern.stepPitches) {
            pitchesObj->setProperty(juce::String(step), pitch);
        }
        p->setProperty("stepPitches", pitchesObj.get());

        // Velocities
        juce::DynamicObject::Ptr velsObj = new juce::DynamicObject();
        for (const auto& [step, vel] : pattern.stepVelocities) {
            velsObj->setProperty(juce::String(step), vel);
        }
        p->setProperty("stepVelocities", velsObj.get());

        // Step FX (Map<int, vector<int>>)
        juce::DynamicObject::Ptr fxObj = new juce::DynamicObject();
        for (const auto& [step, fxList] : pattern.stepFX) {
            juce::Array<juce::var> list;
            for (int fxId : fxList) list.add(fxId);
            fxObj->setProperty(juce::String(step), list);
        }
        p->setProperty("stepFX", fxObj.get());

        // Step FX Params (Map<int, Map<int, float>>)
        juce::DynamicObject::Ptr paramsObj = new juce::DynamicObject();
        for (const auto& [step, paramMap] : pattern.stepFXParams) {
            juce::DynamicObject::Ptr stepParams = new juce::DynamicObject();
            for (const auto& [paramId, val] : paramMap) {
                stepParams->setProperty(juce::String(paramId), val);
            }
            paramsObj->setProperty(juce::String(step), stepParams.get());
        }
        p->setProperty("stepFXParams", paramsObj.get());
        
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
                       PatternChain& currentChain,
                       std::vector<SerializedColumn>& columns) {
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
            
            // Slices
            juce::var slicesVar = p["sliceMarkers"];
            if (slicesVar.isArray()) {
                for (int j = 0; j < slicesVar.size(); ++j) {
                    pattern.sliceMarkers.push_back((int)slicesVar[j]);
                }
            }

            // Pitches
            if (p["stepPitches"].isObject()) {
                auto* obj = p["stepPitches"].getDynamicObject();
                auto props = obj->getProperties();
                for (auto& prop : props) {
                    pattern.stepPitches[prop.name.toString().getIntValue()] = (int)prop.value;
                }
            }

            // Velocities
            if (p["stepVelocities"].isObject()) {
                auto* obj = p["stepVelocities"].getDynamicObject();
                auto props = obj->getProperties();
                for (auto& prop : props) {
                    pattern.stepVelocities[prop.name.toString().getIntValue()] = (float)prop.value;
                }
            }

            // FX
            if (p["stepFX"].isObject()) {
                auto* obj = p["stepFX"].getDynamicObject();
                auto props = obj->getProperties();
                for (auto& prop : props) {
                    int step = prop.name.toString().getIntValue();
                    if (prop.value.isArray()) {
                        for (int k = 0; k < prop.value.size(); ++k) {
                            pattern.stepFX[step].push_back((int)prop.value[k]);
                        }
                    }
                }
            }

            // FX Params
            if (p["stepFXParams"].isObject()) {
                auto* obj = p["stepFXParams"].getDynamicObject();
                auto props = obj->getProperties();
                for (auto& prop : props) {
                    int step = prop.name.toString().getIntValue();
                    if (prop.value.isObject()) {
                        auto* pObj = prop.value.getDynamicObject();
                        auto pProps = pObj->getProperties();
                        for (auto& pProp : pProps) {
                            pattern.stepFXParams[step][pProp.name.toString().getIntValue()] = (float)pProp.value;
                        }
                    }
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
    
    // Load Columns
    columns.clear();
    juce::var layoutVar = json["layout"];
    if (layoutVar.isArray()) {
        for (int i=0; i < layoutVar.size(); ++i) {
            juce::var c = layoutVar[i];
            if (c.isObject()) {
                SerializedColumn col;
                col.title = c["title"].toString().toStdString();
                
                juce::var pNames = c["patterns"];
                if (pNames.isArray()) {
                    for (int j=0; j < pNames.size(); ++j) {
                        col.patternNames.push_back(pNames[j].toString().toStdString());
                    }
                }
                columns.push_back(col);
            }
        }
    }
    
    return true;
}
