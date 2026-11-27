#ifndef __aeffect__
#define __aeffect__

// VST 2.4 SDK Headers
// Based on official Steinberg VST 2.4 SDK

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#pragma pack(push, 8)
#endif

typedef long VstInt32;
typedef intptr_t VstIntPtr;

#define VSTCALLBACK 

// Effect magic number
#define kEffectMagic CCONST('V', 's', 't', 'P')

// Host to Plugin OpCodes
enum AEffectOpcodes {
    effOpen = 0,
    effClose,
    effSetProgram,
    effGetProgram,
    effSetProgramName,
    effGetProgramName,
    effGetParamLabel,
    effGetParamDisplay,
    effGetParamName,
    effSetSampleRate = 10,
    effSetBlockSize,
    effMainsChanged,
    effEditGetRect,
    effEditOpen,
    effEditClose,
    effProcessEvents = 25,
    effCanBeAutomated,
    effString2Parameter,
    effGetProgramNameIndexed,
    effGetInputProperties = 33,
    effGetOutputProperties,
    effGetPlugCategory,
    effSetSpeakerArrangement = 42,
    effSetBypass = 44,
    effGetEffectName,
    effGetVendorString,
    effGetProductString,
    effGetVendorVersion,
    effCanDo,
    effGetVstVersion = 58
};

// Plugin to Host OpCodes
enum AudioMasterOpcodes {
    audioMasterAutomate = 0,
    audioMasterVersion,
    audioMasterCurrentId,
    audioMasterIdle,
    audioMasterPinConnected,
    audioMasterWantMidi = 6,
    audioMasterGetTime,
    audioMasterProcessEvents,
    audioMasterIOChanged = 13,
    audioMasterSizeWindow = 15,
    audioMasterGetSampleRate,
    audioMasterGetBlockSize,
    audioMasterGetInputLatency,
    audioMasterGetOutputLatency,
    audioMasterGetCurrentProcessLevel = 23,
    audioMasterGetAutomationState,
    audioMasterUpdateDisplay = 42,
    audioMasterBeginEdit,
    audioMasterEndEdit
};

// Plugin Categories
enum VstPlugCategory {
    kPlugCategUnknown = 0,
    kPlugCategEffect,
    kPlugCategSynth,
    kPlugCategAnalysis,
    kPlugCategMastering,
    kPlugCategSpacializer,
    kPlugCategRoomFx,
    kPlugCategSurroundFx,
    kPlugCategRestoration,
    kPlugCategOfflineProcess,
    kPlugCategShell,
    kPlugCategGenerator
};

// Plugin Flags
enum AEffectFlags {
    effFlagsHasEditor = 1,
    effFlagsCanReplacing = 1 << 4,
    effFlagsProgramChunks = 1 << 5,
    effFlagsIsSynth = 1 << 8,
    effFlagsNoSoundInStop = 1 << 9,
    effFlagsCanDoubleReplacing = 1 << 12
};

// Forward declarations
struct AEffect;
typedef VstIntPtr (VSTCALLBACK *audioMasterCallback)(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);

// Main AEffect structure
struct AEffect {
    VstInt32 magic;
    VstIntPtr (VSTCALLBACK *dispatcher)(AEffect* effect, VstInt32 opCode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
    void (VSTCALLBACK *process)(AEffect* effect, float** inputs, float** outputs, VstInt32 sampleFrames);
    void (VSTCALLBACK *setParameter)(AEffect* effect, VstInt32 index, float parameter);
    float (VSTCALLBACK *getParameter)(AEffect* effect, VstInt32 index);
    VstInt32 numPrograms;
    VstInt32 numParams;
    VstInt32 numInputs;
    VstInt32 numOutputs;
    VstInt32 flags;
    VstIntPtr resvd1;
    VstIntPtr resvd2;
    VstInt32 initialDelay;
    VstInt32 realQualities;
    VstInt32 offQualities;
    float ioRatio;
    void* object;
    void* user;
    VstInt32 uniqueID;
    VstInt32 version;
    void (VSTCALLBACK *processReplacing)(AEffect* effect, float** inputs, float** outputs, VstInt32 sampleFrames);
    void (VSTCALLBACK *processDoubleReplacing)(AEffect* effect, double** inputs, double** outputs, VstInt32 sampleFrames);
    char future[56];
};

// Plugin main entry point
typedef AEffect* (*VSTPluginMain)(audioMasterCallback audioMaster);

// Rectangle for editor
struct ERect {
    short top;
    short left;
    short bottom;
    short right;
};

#define CCONST(a, b, c, d) ((((int)a) << 24) | (((int)b) << 16) | (((int)c) << 8) | (((int)d) << 0))

#ifdef WIN32
#pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif

#endif // __aeffect__