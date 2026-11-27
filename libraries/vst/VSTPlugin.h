#pragma once

#include "../vstsdk24/pluginterfaces/vst2.x/aeffect.h"
#include "../vstsdk24/pluginterfaces/vst2.x/aeffectx.h"
#include <Windows.h>
#include <string>

class VSTPlugin {
private:
    HMODULE dllHandle;
    AEffect* effect;
    HWND editorWindow;
    std::string pluginPath;
    std::string effectName;
    std::string vendorName;
    bool bypassed = false;
    
    static VstIntPtr VSTCALLBACK hostCallback(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
    
public:
    VSTPlugin();
    ~VSTPlugin();
    
    bool loadPlugin(const std::string& path);
    void unloadPlugin();
    bool isLoaded() const { return effect != nullptr; }
    
    const std::string& getPluginPath() const { return pluginPath; }
    const std::string& getEffectName() const { return effectName; }
    const std::string& getVendorName() const { return vendorName; }
    int getNumInputs() const { return effect ? effect->numInputs : 0; }
    int getNumOutputs() const { return effect ? effect->numOutputs : 0; }
    int getNumParameters() const { return effect ? effect->numParams : 0; }
    
    bool hasEditor() const;
    bool openEditor(HWND parent);
    void closeEditor();
    bool isEditorOpen() const { return editorWindow != nullptr; }
    HWND getEditorWindow() const { return editorWindow; }
    
    void setSampleRate(float sampleRate);
    void setBlockSize(VstInt32 blockSize);
    void resume();
    void suspend();

    void setBypassed(bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }

    bool processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames);
    
    float getParameter(VstInt32 index);
    void setParameter(VstInt32 index, float value);
    std::string getParameterName(VstInt32 index);
    std::string getParameterLabel(VstInt32 index);
    std::string getParameterDisplay(VstInt32 index);
    
private:
    void getPluginInfo();
};
