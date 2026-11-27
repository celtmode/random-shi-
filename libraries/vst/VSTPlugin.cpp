#include "VSTPlugin.h"
#include <windows.h>     // For VST API types if needed
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>


using std::min;

VSTPlugin::VSTPlugin() : dllHandle(nullptr), effect(nullptr), editorWindow(nullptr) {
}

VSTPlugin::~VSTPlugin() {
    unloadPlugin();
}

bool VSTPlugin::loadPlugin(const std::string& path) {
    unloadPlugin();
    
    dllHandle = LoadLibraryA(path.c_str());
    if (!dllHandle) {
        return false;
    }
    
    typedef AEffect* (*VSTPluginMain)(audioMasterCallback audioMaster);
    VSTPluginMain vstMain = (VSTPluginMain)GetProcAddress(dllHandle, "VSTPluginMain");
    if (!vstMain) {
        vstMain = (VSTPluginMain)GetProcAddress(dllHandle, "main");
    }
    
    if (!vstMain) {
        FreeLibrary(dllHandle);
        dllHandle = nullptr;
        return false;
    }
    
    effect = vstMain(hostCallback);
    if (!effect) {
        FreeLibrary(dllHandle);
        dllHandle = nullptr;
        return false;
    }
    
    if (effect->magic != kEffectMagic) {
        FreeLibrary(dllHandle);
        dllHandle = nullptr;
        effect = nullptr;
        return false;
    }
    
    pluginPath = path;
    
    effect->dispatcher(effect, effOpen, 0, 0, nullptr, 0);
    
    setSampleRate(44100.0f);
    setBlockSize(512);
    
    getPluginInfo();
    
    effect->resvd1 = (VstIntPtr)this;
    
    return true;
}

void VSTPlugin::unloadPlugin() {
    if (effect) {
        closeEditor();
        effect->dispatcher(effect, effClose, 0, 0, nullptr, 0);
        effect = nullptr;
    }
    
    if (dllHandle) {
        FreeLibrary(dllHandle);
        dllHandle = nullptr;
    }
    
    pluginPath.clear();
    effectName.clear();
    vendorName.clear();
}

bool VSTPlugin::hasEditor() const {
    if (!effect) return false;
    return (effect->flags & effFlagsHasEditor) != 0;
}

bool VSTPlugin::openEditor(HWND parent) {
    if (!effect || !hasEditor() || editorWindow) {
        return false;
    }
    
    ERect* rect = nullptr;
    effect->dispatcher(effect, effEditGetRect, 0, 0, &rect, 0);
    
    if (!rect) {
        return false;
    }
    
    VstInt32 width = rect->right - rect->left;
    VstInt32 height = rect->bottom - rect->top;
    
    if (width <= 0 || height <= 0) {
        width = 400;
        height = 300;
    }
    
    std::string windowTitle = effectName.empty() ? "VST Editor" : effectName + " - VST Plugin";
    editorWindow = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        "STATIC",
        windowTitle.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE, 
        CW_USEDEFAULT, CW_USEDEFAULT,
        width + GetSystemMetrics(SM_CXFRAME) * 2, 
        height + GetSystemMetrics(SM_CYFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION),
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    
    if (!editorWindow) {
        return false;
    }
    
    SetWindowPos(editorWindow, HWND_TOPMOST, 0, 0, 0, 0, 
                SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    
    VstIntPtr result = effect->dispatcher(effect, effEditOpen, 0, 0, editorWindow, 0);
    if (!result) {
        DestroyWindow(editorWindow);
        editorWindow = nullptr;
        return false;
    }
    
    ShowWindow(editorWindow, SW_SHOW);
    UpdateWindow(editorWindow);
    
    return true;
}

void VSTPlugin::closeEditor() {
    if (effect && editorWindow) {
        effect->dispatcher(effect, effEditClose, 0, 0, nullptr, 0);
        DestroyWindow(editorWindow);
        editorWindow = nullptr;
    }
}

void VSTPlugin::setSampleRate(float sampleRate) {
    if (effect) {
        effect->dispatcher(effect, effSetSampleRate, 0, 0, nullptr, sampleRate);
    }
}

void VSTPlugin::setBlockSize(VstInt32 blockSize) {
    if (effect) {
        effect->dispatcher(effect, effSetBlockSize, 0, blockSize, nullptr, 0);
    }
}

void VSTPlugin::resume() {
    if (effect) {
        effect->dispatcher(effect, effMainsChanged, 0, 1, nullptr, 0);
    }
}

void VSTPlugin::suspend() {
    if (effect) {
        effect->dispatcher(effect, effMainsChanged, 0, 0, nullptr, 0);
    }
}

bool VSTPlugin::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) {
    if (!effect) return false;

    if (bypassed) {
        int numOut = getNumOutputs();
        int numIn = getNumInputs();
        int chans = min(numOut, numIn);
        for (int c = 0; c < chans; ++c) {
            if (outputs[c] && inputs[c]) {
                memcpy(outputs[c], inputs[c], sizeof(float) * sampleFrames);
            }
        }
        for (int c = chans; c < numOut; ++c) {
            if (outputs[c]) memset(outputs[c], 0, sizeof(float) * sampleFrames);
        }
        return true;
    }

    if (effect->processReplacing) {
        effect->processReplacing(effect, inputs, outputs, sampleFrames);
        return true;
    }
    return false;
}

float VSTPlugin::getParameter(VstInt32 index) {
    if (!effect || index < 0 || index >= effect->numParams) {
        return 0.0f;
    }
    return effect->getParameter(effect, index);
}

void VSTPlugin::setParameter(VstInt32 index, float value) {
    if (!effect || index < 0 || index >= effect->numParams) {
        return;
    }
    effect->setParameter(effect, index, value);
}

std::string VSTPlugin::getParameterName(VstInt32 index) {
    if (!effect || index < 0 || index >= effect->numParams) {
        return "";
    }
    
    char name[256] = {0};
    effect->dispatcher(effect, effGetParamName, index, 0, name, 0);
    return std::string(name);
}

std::string VSTPlugin::getParameterLabel(VstInt32 index) {
    if (!effect || index < 0 || index >= effect->numParams) {
        return "";
    }
    
    char label[256] = {0};
    effect->dispatcher(effect, effGetParamLabel, index, 0, label, 0);
    return std::string(label);
}

std::string VSTPlugin::getParameterDisplay(VstInt32 index) {
    if (!effect || index < 0 || index >= effect->numParams) {
        return "";
    }
    
    char display[256] = {0};
    effect->dispatcher(effect, effGetParamDisplay, index, 0, display, 0);
    return std::string(display);
}

void VSTPlugin::getPluginInfo() {
    if (!effect) return;
    
    char name[256] = {0};
    effect->dispatcher(effect, effGetEffectName, 0, 0, name, 0);
    effectName = std::string(name);
    
    char vendor[256] = {0};
    effect->dispatcher(effect, effGetVendorString, 0, 0, vendor, 0);
    vendorName = std::string(vendor);
    
    if (effectName.empty()) {
        size_t lastSlash = pluginPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            effectName = pluginPath.substr(lastSlash + 1);
        } else {
            effectName = pluginPath;
        }
        
        size_t lastDot = effectName.find_last_of(".");
        if (lastDot != std::string::npos) {
            effectName = effectName.substr(0, lastDot);
        }
    }
}

VstIntPtr VSTCALLBACK VSTPlugin::hostCallback(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    switch (opcode) {
    case audioMasterVersion:
        return kVstVersion;
        
    case audioMasterCurrentId:
        return 'MYID';
        
    case audioMasterGetSampleRate:
        return 44100;
        
    case audioMasterGetBlockSize:
        return 512;
        
    case audioMasterGetCurrentProcessLevel:
        return kVstProcessLevelRealtime;
        
    case audioMasterGetAutomationState:
        return kVstAutomationWriting;
        
    case audioMasterUpdateDisplay:
        return 1;
        
    case audioMasterBeginEdit:
        return 1;
        
    case audioMasterEndEdit:
        return 1;
        
    case audioMasterSizeWindow:
        if (effect && effect->resvd1) {
            VSTPlugin* plugin = (VSTPlugin*)effect->resvd1;
            if (plugin->editorWindow) {
                SetWindowPos(plugin->editorWindow, nullptr, 0, 0, index, value, 
                           SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                return 1;
            }
        }
        return 0;
        
    default:
        return 0;
    }
}