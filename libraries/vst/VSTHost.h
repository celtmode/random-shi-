#pragma once

#include "VSTPlugin.h"
#include <vector>
#include <memory>
#include <string>

class VSTHost {
private:
    std::vector<std::unique_ptr<VSTPlugin>> plugins;
    
public:
    VSTHost();
    ~VSTHost();
    
    int loadPlugin(const std::string& path);
    void unloadPlugin(int index);
    void unloadAllPlugins();
    
    VSTPlugin* getPlugin(int index);
    int getPluginCount() const { return static_cast<int>(plugins.size()); }

    void setGlobalSampleRate(float sr);
    void setGlobalBlockSize(int bs);
    bool processInterleaved(float* interleavedIn, float* interleavedOut, int numFrames, int numChannels);
    
    std::vector<std::string> scanDirectory(const std::string& directory);
    bool isVSTFile(const std::string& path);
    std::vector<std::string> getCommonVSTDirectories();
};

extern std::unique_ptr<VSTHost> g_vstHost;