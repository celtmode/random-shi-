#include "VSTHost.h"
#include <Windows.h>
#include <iostream>
#include <algorithm>

using std::min;

#include "VSTHost.h"

VSTPlugin* VSTHost::getPlugin(int index) {
    if (index < 0 || index >= static_cast<int>(plugins.size())) {
        return nullptr;
    }
    return plugins[index].get(); // return raw pointer from unique_ptr
}


std::unique_ptr<VSTHost> g_vstHost = nullptr;

VSTHost::VSTHost() {
}

VSTHost::~VSTHost() {
    unloadAllPlugins();
}

void VSTHost::setGlobalSampleRate(float sr) {
    for (auto& p : plugins) {
        p->setSampleRate(sr);
    }
}

void VSTHost::setGlobalBlockSize(int bs) {
    for (auto& p : plugins) {
        p->setBlockSize(bs);
    }
}

int VSTHost::loadPlugin(const std::string& path) {
    auto plugin = std::make_unique<VSTPlugin>();
    
    if (plugin->loadPlugin(path)) {
        plugins.push_back(std::move(plugin));
        return static_cast<int>(plugins.size() - 1);
    }
    
    return -1;
}

void VSTHost::unloadPlugin(int index) {
    if (index >= 0 && index < static_cast<int>(plugins.size())) {
        plugins.erase(plugins.begin() + index);
    }
}

void VSTHost::unloadAllPlugins() {
    plugins.clear();
}

std::vector<std::string> VSTHost::scanDirectory(const std::string& directory) {
    std::vector<std::string> vstFiles;
    
    std::string searchPath = directory + "\\*.dll";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string filePath = directory + "\\" + findData.cFileName;
                if (isVSTFile(filePath)) {
                    vstFiles.push_back(filePath);
                }
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    
    return vstFiles;
}

bool VSTHost::isVSTFile(const std::string& path) {
    HMODULE hModule = LoadLibraryA(path.c_str());
    if (!hModule) {
        return false;
    }
    
    bool isVST = false;
    if (GetProcAddress(hModule, "VSTPluginMain") || GetProcAddress(hModule, "main")) {
        isVST = true;
    }
    
    FreeLibrary(hModule);
    return isVST;
}

std::vector<std::string> VSTHost::getCommonVSTDirectories() {
    std::vector<std::string> directories;
    
    directories.push_back("C:\\Program Files\\VSTPlugins");
    directories.push_back("C:\\Program Files (x86)\\VSTPlugins");
    directories.push_back("C:\\Program Files\\Steinberg\\VSTPlugins");
    directories.push_back("C:\\Program Files (x86)\\Steinberg\\VSTPlugins");
    directories.push_back("C:\\Program Files\\Common Files\\VST2");
    directories.push_back("C:\\Program Files (x86)\\Common Files\\VST2");
    directories.push_back("C:\\Program Files\\EqualizerAPO\\VSTPlugins");
    
    return directories;
}

bool VSTHost::processInterleaved(float* interleavedIn, float* interleavedOut, int numFrames, int numChannels) {
    if (plugins.empty() || !interleavedIn || !interleavedOut || numFrames <= 0 || numChannels <= 0)
        return false;

    const int chans = numChannels;

    std::vector<std::vector<float>> inBuf(chans, std::vector<float>(numFrames));
    std::vector<std::vector<float>> outBuf(chans, std::vector<float>(numFrames));

    for (int f = 0; f < numFrames; ++f) {
        for (int c = 0; c < chans; ++c) {
            inBuf[c][f] = interleavedIn[f * chans + c];
        }
    }

    std::vector<float*> inPtrs(chans);
    std::vector<float*> outPtrs(chans);

    for (int c = 0; c < chans; ++c) {
        inPtrs[c] = inBuf[c].data();
        outPtrs[c] = outBuf[c].data();
    }

    std::vector<std::vector<float>> scratchA = outBuf;
    std::vector<std::vector<float>> scratchB(chans, std::vector<float>(numFrames));
    std::vector<float*> curIn(chans), curOut(chans);

    for (int c = 0; c < chans; ++c) {
        curIn[c] = inBuf[c].data();
        curOut[c] = scratchA[c].data();
    }

    bool used = false;

    for (size_t i = 0; i < plugins.size(); ++i) {
        VSTPlugin* p = plugins[i].get();
        if (!p) continue;

        p->setBlockSize(numFrames);

        int pIn = p->getNumInputs();
        int pOut = p->getNumOutputs();
        int usedChans = min(chans, min(pIn, pOut));

        std::vector<float*> procIn(usedChans), procOut(usedChans);
        for (int c = 0; c < usedChans; ++c) { procIn[c] = curIn[c]; procOut[c] = curOut[c]; }

        p->processReplacing(procIn.data(), procOut.data(), numFrames);
        used = true;

        if ((i + 1) < plugins.size()) {
            for (int c = 0; c < chans; ++c) {
                curIn[c] = curOut[c];
            }
            std::vector<std::vector<float>>& nextScratch = (curOut[0] == scratchA[0].data()) ? scratchB : scratchA;
            for (int c = 0; c < chans; ++c) {
                curOut[c] = nextScratch[c].data();
            }
        }
    }

    if (!used) {
        memcpy(interleavedOut, interleavedIn, sizeof(float) * numFrames * chans);
        return false;
    }

    for (int f = 0; f < numFrames; ++f) {
        for (int c = 0; c < chans; ++c) {
            interleavedOut[f * chans + c] = curOut[c][f];
        }
    }

    return true;
}


