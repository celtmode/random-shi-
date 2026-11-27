#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#define NOMINMAX
#include "HookGui.hpp"
#include "../vst/VSTHost.h"
#include "libraries/opus/include/opus.h"
#include "Windows.h"
#include <iostream>
#include <fstream>
#include <iostream>
#include <WinUser.h>
#include <windows.h>
#include <TlHelp32.h>
#include <windows.h>
#include <psapi.h>
#include <thread>
#include <algorithm>
#include <mutex>
#include <chrono>
#include <string>
#include <vector>
#include <evntprov.h>
#include <winternl.h>

#pragma once
#include <vector>
#include <mutex>



extern std::vector<float> pcm_graph_data;
extern std::mutex pcm_graph_mutex;
extern std::mutex self_graph_mutex;
extern std::vector<float> self_graph_data;
extern const size_t SELF_GRAPH_MAX_SAMPLES = 4096;

constexpr size_t PCM_GRAPH_MAX_SAMPLES = 8192;



static std::string logBuffer;
static bool showLogs = false;

#include <windows.h>
#include <vector>
#include <string>
#include <cstdarg>

// === Log Entry struct ===
struct LogEntry {
    std::string prefix;
    std::string message;
    ImVec4 prefixColor;
    ImVec4 msgColor;
};

static std::vector<LogEntry> logEntries;
static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

// === AddLog with color tags ===
void AddLog(const char* fmt, ...) {
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    std::string text = buf;
    std::string prefix;
    std::string message = text;

    ImVec4 prefixCol(1, 1, 1, 1);
    ImVec4 msgCol(1, 1, 1, 1);

    if (text.find("[Lua Error]") == 0) {
        prefix = "[Lua Error]";
        message = text.substr(prefix.size());
        prefixCol = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // red
    }
    else if (text.find("[Lua]") == 0) {
        prefix = "[Lua]";
        message = text.substr(prefix.size());
        prefixCol = ImVec4(0.7f, 0.4f, 1.0f, 1.0f); // purple
    }

    logEntries.push_back({ prefix, message, prefixCol, msgCol });

    // === Print to CMD with colors ===
    if (hConsole) {
        if (!prefix.empty()) {
            if (prefix == "[Lua]") {
                SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY); // purple-ish
            }
            else {
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY); // red
            }
            printf("%s", prefix.c_str());
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // reset to white
            printf("%s\n", message.c_str());
        }
        else {
            printf("%s\n", text.c_str());
        }
    }
}




#include "libraries/lua/lua.hpp"   // make sure lua.h, lauxlib.h, lualib.h are in include path

// Global Lua VM
lua_State* L = nullptr;

// Forward declarations of the Lua bindings
int Lua_SetGain(lua_State* Ls);
int Lua_SetExpGain(lua_State* Ls);
int Lua_SetBitrate(lua_State* Ls);

// Register functions into Lua
void RegisterLuaFunctions() {
    if (!L) return;
    lua_register(L, "setgain", Lua_SetGain);
    lua_register(L, "setexpgain", Lua_SetExpGain);
    lua_register(L, "setbitrate", Lua_SetBitrate);
}

// Initialize Lua
void InitLua() {
    if (!L) {
        L = luaL_newstate();       // create Lua VM
        luaL_openlibs(L);          // load base libs
        RegisterLuaFunctions();    // custom bindings
        AddLog("[Lua] Initialized.");
    }
}

// Shutdown Lua
void ShutdownLua() {
    if (L) {
        lua_close(L);
        L = nullptr;
        AddLog("[Lua] Shutdown.");
    }
}

// Lua bindings

int Lua_SetGain(lua_State* Ls) {
    float val = (float)luaL_checknumber(Ls, 1);
    Gain = val;
    AddLog("[Lua] Gain set to %.2f", Gain);
    return 0; // no values returned to Lua
}

int Lua_SetExpGain(lua_State* Ls) {
    float val = (float)luaL_checknumber(Ls, 1);
    ExpGain = val;
    AddLog("[Lua] ExpGain set to %.2f", ExpGain);
    return 0;
}

int Lua_SetBitrate(lua_State* Ls) {
    float val = (float)luaL_checknumber(Ls, 1);
    Bitrate = val;
    AddLog("[Lua] Bitrate set to %.0f", Bitrate);
    return 0;
}




struct Particle {
    ImVec2 pos, vel;
};

std::vector<Particle> particles;
const int numParticles = 100;

inline HWND g_Window = nullptr;
bool g_showMenu = false;

static HHOOK hHook = nullptr;
static HMODULE hModule = nullptr;
static BYTE* memoryBase = nullptr;
static SIZE_T memoryRegionSize = 0;
static bool clearHooks = false;

static float time_since_start = 0.0f;

#include <wincodec.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#include "imgui/spiderman_texture.h"
#include "spiderman.h"

// globals (one definition in one .cpp)
ID3D11ShaderResourceView* g_SpiderManTexture = nullptr;

// call this AFTER your D3D device (g_pd3dDevice) is created
bool LoadSpiderManTexture(ID3D11Device* device)
{
    if (!device) return false;
    if (g_SpiderManTexture) return true;

    bool result = false;
    HRESULT hr = E_FAIL;
    bool initializedCom = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

    IWICImagingFactory* factory = nullptr;
    IStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    ID3D11Texture2D* texture = nullptr;

    do {
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
        if (FAILED(hr)) break;

        stream = SHCreateMemStream(spiderman_png, (UINT)spiderman_png_len);
        if (!stream) { hr = E_OUTOFMEMORY; break; }

        hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
        if (FAILED(hr)) break;

        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) break;

        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr)) break;

        hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) break;

        UINT width = 0, height = 0;
        converter->GetSize(&width, &height);
        const UINT stride = width * 4;
        const UINT size = stride * height;
        std::vector<unsigned char> pixels(size);

        hr = converter->CopyPixels(nullptr, stride, size, pixels.data());
        if (FAILED(hr)) break;

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = pixels.data();
        initData.SysMemPitch = stride;

        hr = device->CreateTexture2D(&desc, &initData, &texture);
        if (FAILED(hr)) break;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        hr = device->CreateShaderResourceView(texture, &srvDesc, &g_SpiderManTexture);
        result = SUCCEEDED(hr);
    } while (false);

    if (texture) texture->Release();
    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();
    if (initializedCom) CoUninitialize();

    if (!result)
    {
        char buf[256];
        sprintf(buf, "Spider-Man texture load failed. HRESULT = 0x%08X (len=%u)", hr, spiderman_png_len);
        MessageBoxA(NULL, buf, "SpiderManHook Error", MB_ICONERROR);
    }
    return result;
}

void ReleaseSpiderManTexture()
{
    if (g_SpiderManTexture) { g_SpiderManTexture->Release(); g_SpiderManTexture = nullptr; }
}


void ClearImGuiStringTraces() {
    const std::vector<std::string> targetStrings = { "ImGui", "Dear ImGui", "ImGui::" };
    MEMORY_BASIC_INFORMATION mbi;
    BYTE* address = nullptr;
    while (VirtualQuery(address, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && (mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_EXECUTE_READWRITE)) {
            BYTE* buffer = static_cast<BYTE*>(mbi.BaseAddress);
            SIZE_T regionSize = mbi.RegionSize;
            for (size_t i = 0; i < regionSize; ++i) {
                for (const auto& target : targetStrings) {
                    if (i + target.length() <= regionSize) {
                        if (memcmp(buffer + i, target.c_str(), target.length()) == 0) {
                            DWORD oldProtect;
                            VirtualProtect(buffer + i, target.length(), PAGE_READWRITE, &oldProtect);
                            memset(buffer + i, 0, target.length());
                            VirtualProtect(buffer + i, target.length(), oldProtect, &oldProtect);
                        }
                    }
                }
            }
        }
        address += mbi.RegionSize;
    }
}


bool IsDebuggerPresentAdvanced() {
    BOOL isDebugged = FALSE;
    return IsDebuggerPresent() || (CheckRemoteDebuggerPresent(GetCurrentProcess(), &isDebugged) && isDebugged);
}

// Function to obfuscate module name
void ObfuscateModuleName() {
    if (hModule != nullptr) {
        MODULEENTRY32W me32 = { sizeof(MODULEENTRY32W) };
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
        if (hSnapshot != INVALID_HANDLE_VALUE) {
            if (Module32FirstW(hSnapshot, &me32)) {
                do {
                    if (me32.hModule == hModule) {
                        DWORD oldProtect;
                        VirtualProtect(me32.szModule, sizeof(me32.szModule), PAGE_READWRITE, &oldProtect);
                        wcscpy_s(me32.szModule, L"ntdll.dll"); // Spoof as a system module
                        VirtualProtect(me32.szModule, sizeof(me32.szModule), oldProtect, &oldProtect);
                        break;
                    }
                } while (Module32NextW(hSnapshot, &me32));
            }
            CloseHandle(hSnapshot);
        }
    }
}

// Function to clear PE headers
void ClearPEHeaders() {
    if (hModule != nullptr) {
        BYTE* base = reinterpret_cast<BYTE*>(hModule);
        IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
            IMAGE_NT_HEADERS* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);
            if (ntHeaders->Signature == IMAGE_NT_SIGNATURE) {
                DWORD oldProtect;
                VirtualProtect(base, sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS), PAGE_READWRITE, &oldProtect);
                memset(base, 0, sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS));
                VirtualProtect(base, sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS), oldProtect, &oldProtect);
            }
        }
    }
}

// Function to hide thread start address
void HideThreadStartAddress() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, GetCurrentProcessId());
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te32 = { sizeof(THREADENTRY32) };
        if (Thread32First(hSnapshot, &te32)) {
            do {
                HANDLE hThread = OpenThread(THREAD_SET_CONTEXT, FALSE, te32.th32ThreadID);
                if (hThread != NULL) {
                    // Point thread start address to a system module (e.g., ntdll)
                    CONTEXT context = { CONTEXT_CONTROL };
                    GetThreadContext(hThread, &context);
                    context.Rip = reinterpret_cast<DWORD64>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtYieldExecution"));
                    SetThreadContext(hThread, &context);
                    CloseHandle(hThread);
                }
            } while (Thread32Next(hSnapshot, &te32));
        }
        CloseHandle(hSnapshot);
    }
}

// Function to clear debug information
void ClearDebugInfo() {
    if (hModule != nullptr) {
        BYTE* base = reinterpret_cast<BYTE*>(hModule);
        IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
            IMAGE_NT_HEADERS* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);
            if (ntHeaders->Signature == IMAGE_NT_SIGNATURE) {
                IMAGE_DATA_DIRECTORY* debugDir = &ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
                if (debugDir->VirtualAddress != 0) {
                    BYTE* debugData = base + debugDir->VirtualAddress;
                    DWORD oldProtect;
                    VirtualProtect(debugData, debugDir->Size, PAGE_READWRITE, &oldProtect);
                    memset(debugData, 0, debugDir->Size);
                    VirtualProtect(debugData, debugDir->Size, oldProtect, &oldProtect);
                }
            }
        }
    }
}

// Function to clear event logs
void ClearEventLogs() {
    HANDLE hEventLog = OpenEventLogW(NULL, L"Application");
    if (hEventLog != NULL) {
        ClearEventLogW(hEventLog, NULL);
        CloseEventLog(hEventLog);
    }
    hEventLog = OpenEventLogW(NULL, L"System");
    if (hEventLog != NULL) {
        ClearEventLogW(hEventLog, NULL);
        CloseEventLog(hEventLog);
    }
}

// Function to self-destruct (unload DLL and clean up)
void SelfDestruct() {
    if (hHook != nullptr) {
        UnhookWindowsHookEx(hHook);
        hHook = nullptr;
    }
    if (hModule != nullptr) {
        FreeLibraryAndExitThread(hModule, 0);
    }
}


extern bool selfcheck;


void UpdateTime()
{
    static auto start_time = std::chrono::high_resolution_clock::now();
    auto current_time = std::chrono::high_resolution_clock::now();
    time_since_start = std::chrono::duration<float>(current_time - start_time).count();
}

ImVec4 GetAnimatedColor() {
    static float hue = 0.0f;
    hue += 0.001f;
    if (hue > 1.0f) hue = 0.0f;

    ImVec4 color;
    ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, color.x, color.y, color.z);
    color.w = 0.6f;
    return color;
}

extern std::mutex db_graph_mutex;
extern std::vector<float> db_graph_data;


extern char yexDecoderText[128];
extern bool dbcheck;
extern bool forceNotNatty;
extern bool int16check;

namespace hide {
    bool ProcessIsolation = false;
    bool Hider = true;
}

namespace vst_ui {
    static bool showVSTTab = true;
    static char pluginPath[512] = "C:\\Program Files\\VSTPlugins";
    static std::vector<std::string> availablePlugins;
    static int selectedPlugin = -1;
    static bool isScanning = false;
    static int processCallCount = 0;
}

#include <algorithm>
#include <cfloat>
#include <mutex>
#include <vector>
inline float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline ImVec4 Lerp(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}
#include <algorithm>
#include <cfloat>
#include <mutex>
#include <vector>
#include <cmath>
#include <random>

ImVec4 HexToColorVec4(unsigned int hex, float alpha = 1.0f)
{
    float r = ((hex >> 16) & 0xFF) / 255.0f;
    float g = ((hex >> 8) & 0xFF) / 255.0f;
    float b = (hex & 0xFF) / 255.0f;
    return ImVec4(r, g, b, alpha);
}

// Example spectral filter placeholder
float SpectralFilter(float value, float lowCut = 0.0f, float highCut = 5000.0f) {
    // Replace with actual spectral logic
    return std::clamp(value, lowCut, highCut);
}

// Noise reduction placeholder
float ReduceNoise(float current, float last, float factor) {
    return Lerp(last, current, factor);
}

// Fake signals generator
float FakeSignal(float base = 0.0f, float amplitude = 5.0f) {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(-amplitude, amplitude);
    return base + dist(rng);
}

std::wstring ConvertToWide(const char* str) {
    int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
    std::wstring wstr(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str, -1, &wstr[0], len);
    return wstr;
}
void ScanForVSTPlugins() {
    if (!g_vstHost) return;
    
    vst_ui::isScanning = true;
    vst_ui::availablePlugins.clear();
    
    if (strlen(vst_ui::pluginPath) > 0) {
        std::vector<std::string> plugins = g_vstHost->scanDirectory(vst_ui::pluginPath);
        vst_ui::availablePlugins.insert(vst_ui::availablePlugins.end(), plugins.begin(), plugins.end());
    }
    
    std::vector<std::string> commonDirs = g_vstHost->getCommonVSTDirectories();
    for (const auto& dir : commonDirs) {
        std::vector<std::string> plugins = g_vstHost->scanDirectory(dir);
        vst_ui::availablePlugins.insert(vst_ui::availablePlugins.end(), plugins.begin(), plugins.end());
    }
    
    vst_ui::isScanning = false;
}
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <complex>

#include <complex>
#include <valarray>

#include <complex>
#include <valarray>
#include <algorithm>


// === FFT (simple Cooley–Tukey) ===
using Complex = std::complex<float>;
using CArray = std::valarray<Complex>;

void FFT(CArray& x) {
    size_t N = x.size();
    if (N <= 1) return;

    CArray even = x[std::slice(0, N / 2, 2)];
    CArray odd = x[std::slice(1, N / 2, 2)];

    FFT(even);
    FFT(odd);

    for (size_t k = 0; k < N / 2; k++) {
        float angle = -2.0f * float(M_PI) * k / float(N);
        Complex t = std::exp(Complex(0, angle)) * odd[k];
        x[k] = even[k] + t;
        x[k + N / 2] = even[k] - t;
    }
}

// === Draw Spectrum Analyzer (Polyline Style) ===
void DrawSpectrumAnalyzer(ImDrawList* draw, ImVec2 pos, ImVec2 size) {
    std::vector<float> samples;
    {
        std::lock_guard<std::mutex> lock(pcm_graph_mutex);
        samples = pcm_graph_data;
    }

    if (samples.empty())
        return;

    // take power-of-2 slice for FFT
    const int N = 512;
    if ((int)samples.size() < N) return;
    std::vector<Complex> in(N);
    for (int i = 0; i < N; i++) {
        // Hann window
        float w = 0.5f * (1 - cosf(2.0f * float(M_PI) * i / (N - 1)));
        in[i] = Complex(samples[samples.size() - N + i] * w, 0.0f);
    }

    CArray data(in.data(), N);
    FFT(data);

    // magnitude
    std::vector<float> mags(N / 2);
    for (int i = 0; i < N / 2; i++) {
        mags[i] = std::abs(data[i]);
    }

    // normalize
    float maxVal = *std::max_element(mags.begin(), mags.end());
    if (maxVal <= 0.0f) maxVal = 1.0f;

    // === Polyline drawing ===
    ImVec2 prev;
    bool hasPrev = false;

    for (int i = 0; i < N / 2; i++) {
        float x = pos.x + (float)i / (N / 2.0f) * size.x;
        float y = pos.y + size.y - (mags[i] / maxVal) * size.y;

        ImVec2 p(x, y);

        if (hasPrev) {
            draw->AddLine(prev, p, IM_COL32(120, 160, 255, 255), 1.5f);
        }

        prev = p;
        hasPrev = true;
    }

    // draw frame box (optional)
    draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(40, 40, 40, 255));
}




// Simple EQ response approximation (log-frequency interpolation)
float calculate_eq_response(float freq,
    float lowCutoff, float highCutoff,
    float lowsGain, float midsGain, float highsGain,
    int sampleRate)
{
    // Reference band centers (log spaced)
    const float lowCenter = 100.0f;   // Low band center
    const float midCenter = 1000.0f;  // Mid band center
    const float highCenter = 10000.0f; // High band center

    // Gains are given in dB (assuming your sliders are in dB)
    float lowDb = lowsGain;
    float midDb = midsGain;
    float highDb = highsGain;

    // Compute log frequency for interpolation
    float logFreq = std::log10(freq);

    // Simple triangular weighting for each band
    float lowWeight = std::exp(-0.5f * std::pow((logFreq - std::log10(lowCenter)) / 0.5f, 2));
    float midWeight = std::exp(-0.5f * std::pow((logFreq - std::log10(midCenter)) / 0.5f, 2));
    float highWeight = std::exp(-0.5f * std::pow((logFreq - std::log10(highCenter)) / 0.5f, 2));

    // Normalize weights
    float sum = lowWeight + midWeight + highWeight;
    if (sum < 1e-6f) sum = 1.0f;
    lowWeight /= sum;
    midWeight /= sum;
    highWeight /= sum;

    // Weighted sum of band gains
    float dbResponse = lowDb * lowWeight + midDb * midWeight + highDb * highWeight;

    // Convert dB to linear amplitude
    float linearResponse = std::pow(10.0f, dbResponse / 20.0f);

    return linearResponse;
}

void DrawCoolEQVisualization(float lowsGain, float midsGain, float highsGain, float canvasHeight, float lineThicknessParam) {
    static float animTime = 0.0f;
    animTime += ImGui::GetIO().DeltaTime * 0.5f;
    const float lowCutoff = 1.0f;
    const float highCutoff = 30000.0f;
    const int sampleRate = 58000;
    const int numPoints = 250;
    const float minFreq = 20.0f;
    const float maxFreq = 25000.0f;
    ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, canvasHeight);
    ImGui::Text("EQ Visualization");
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::BeginChild("EQCanvas", canvasSize, true);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    std::vector<float> frequencies(numPoints);
    std::vector<float> responses(numPoints);
    float logMinFreq = std::log10(minFreq);
    float logMaxFreq = std::log10(maxFreq);
    for (int i = 0; i < numPoints; i++) {
        float t = (float)i / (numPoints - 1);
        float logFreq = logMinFreq + t * (logMaxFreq - logMinFreq);
        frequencies[i] = std::pow(10.0f, logFreq);
        responses[i] = calculate_eq_response(frequencies[i], lowCutoff, highCutoff, lowsGain, midsGain, highsGain, sampleRate);
    }
    float minResponse = responses[0];
    float maxResponse = responses[0];
    for (int i = 1; i < numPoints; i++) {
        if (responses[i] < minResponse) minResponse = responses[i];
        if (responses[i] > maxResponse) maxResponse = responses[i];
    }
    minResponse = std::fmax(0.0f, std::fmin(minResponse, 0.5f));
    maxResponse = std::fmax(15.0f, maxResponse * 1.1f);
    ImVec2 canvasTopLeft = ImGui::GetCursorScreenPos();
    ImVec2 canvasBottomRight = ImVec2(canvasTopLeft.x + canvasSize.x, canvasTopLeft.y + canvasSize.y);
    const float gridSize = 30.0f;
    for (float x = std::fmod(canvasTopLeft.x, gridSize); x < canvasBottomRight.x; x += gridSize) {
        drawList->AddLine(ImVec2(x, canvasTopLeft.y), ImVec2(x, canvasBottomRight.y), ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.12f)));
    }
    for (float y = std::fmod(canvasTopLeft.y, gridSize); y < canvasBottomRight.y; y += gridSize) {
        drawList->AddLine(ImVec2(canvasTopLeft.x, y), ImVec2(canvasBottomRight.x, y), ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.12f)));
    }
    const int numMarkers = 7;
    const float markerFreqs[] = { 20, 50, 100, 500, 1000, 5000, 20000 };
    const char* markerLabels[] = { "20Hz", "50Hz", "100Hz", "500Hz", "1kHz", "5kHz", "20kHz" };
    for (int i = 0; i < numMarkers; i++) {
        float t = (std::log10(markerFreqs[i]) - logMinFreq) / (logMaxFreq - logMinFreq);
        float x = canvasTopLeft.x + t * canvasSize.x;
        drawList->AddLine(ImVec2(x, canvasTopLeft.y), ImVec2(x, canvasBottomRight.y), ImGui::ColorConvertFloat4ToU32(ImVec4(1.00f, 1.00f, 1.00f, 0.35f)));
        ImVec2 textSize = ImGui::CalcTextSize(markerLabels[i]);
        drawList->AddText(ImVec2(x - textSize.x / 2, canvasBottomRight.y - textSize.y - 5), ImGui::ColorConvertFloat4ToU32(ImVec4(1.00f, 1.00f, 1.0f, 0.95f)), markerLabels[i]);
    }
    const int numLevels = 5;
    for (int i = 0; i <= numLevels; i++) {
        float level = minResponse + (maxResponse - minResponse) * i / numLevels;
        float y = canvasBottomRight.y - (level - minResponse) / (maxResponse - minResponse) * canvasSize.y;
        drawList->AddLine(ImVec2(canvasTopLeft.x, y), ImVec2(canvasBottomRight.x, y), ImGui::ColorConvertFloat4ToU32(ImVec4(1.00f, 1.00f, 1.00f, 0.20f)));
        char levelLabel[16];
        snprintf(levelLabel, sizeof(levelLabel), "%.1f", level);
        drawList->AddText(ImVec2(canvasTopLeft.x + 5, y - 10), ImGui::ColorConvertFloat4ToU32(ImVec4(1.00f, 1.00f, 1.0f, 0.95f)), levelLabel);
    }
    std::vector<ImVec2> points(numPoints);
    for (int i = 0; i < numPoints; i++) {
        float t = (float)i / (numPoints - 1);
        float x = canvasTopLeft.x + t * canvasSize.x;
        float y = canvasBottomRight.y - ((responses[i] - minResponse) / (maxResponse - minResponse)) * canvasSize.y;
        points[i] = ImVec2(x, y);
    }

    std::vector<ImVec2> curve(numPoints);
    if (numPoints >= 3) {
        curve[0] = points[0];
        curve[numPoints - 1] = points[numPoints - 1];
        for (int i = 1; i < numPoints - 1; ++i) {
            float smoothedY = (points[i - 1].y + 2.0f * points[i].y + points[i + 1].y) * 0.25f;
            curve[i] = ImVec2(points[i].x, smoothedY);
        }
    }
    else {
        curve = points;
    }
    ImVec4 themeColor = ImVec4(0.25f, 0.65f, 1.00f, 1.0f);
    ImVec4 markerColor = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    for (int i = 0; i < numPoints - 1; i++) {
        ImVec2 p1 = curve[i];
        ImVec2 p2 = curve[i + 1];
        ImVec2 p3 = ImVec2(p2.x, canvasBottomRight.y);
        ImVec2 p4 = ImVec2(p1.x, canvasBottomRight.y);
        ImVec4 fillColor = themeColor;
        fillColor.w = 0.15f;
        drawList->AddQuadFilled(p1, p2, p3, p4, ImGui::ColorConvertFloat4ToU32(fillColor));
    }
    const float lineThickness = lineThicknessParam;
    for (int i = 0; i < numPoints - 1; i++) {
        ImVec4 lineColor = themeColor;
        for (float glow = lineThickness * 3; glow >= lineThickness; glow -= 1.0f) {
            float alpha = 0.03f * (1.0f - (glow - lineThickness) / (lineThickness * 2));
            ImVec4 glowColor = lineColor;
            glowColor.w = alpha;
            drawList->AddLine(curve[i], curve[i + 1], ImGui::ColorConvertFloat4ToU32(glowColor), glow);
        }
        ImVec4 solidLineColor = lineColor;
        solidLineColor.w = 0.9f;
        drawList->AddLine(curve[i], curve[i + 1], ImGui::ColorConvertFloat4ToU32(solidLineColor), lineThickness);
    }
    float lowX = (std::log10(lowCutoff) - logMinFreq) / (logMaxFreq - logMinFreq) * canvasSize.x + canvasTopLeft.x;
    float highX = (std::log10(highCutoff) - logMinFreq) / (logMaxFreq - logMinFreq) * canvasSize.x + canvasTopLeft.x;
    {
        float yPos = canvasBottomRight.y - ((lowsGain - minResponse) / (maxResponse - minResponse)) * canvasSize.y;
        drawList->AddCircleFilled(ImVec2(lowX * 0.3f, yPos), 6.0f, ImGui::ColorConvertFloat4ToU32(markerColor));
        drawList->AddCircle(ImVec2(lowX * 0.3f, yPos), 8.0f, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.5f)), 0, 2.0f);
    }
    {
        float midX = (lowX + highX) * 0.5f;
        float yPos = canvasBottomRight.y - ((midsGain - minResponse) / (maxResponse - minResponse)) * canvasSize.y;
        drawList->AddCircleFilled(ImVec2(midX, yPos), 6.0f, ImGui::ColorConvertFloat4ToU32(markerColor));
        drawList->AddCircle(ImVec2(midX, yPos), 8.0f, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.5f)), 0, 2.0f);
    }
    {
        float yPos = canvasBottomRight.y - ((highsGain - minResponse) / (maxResponse - minResponse)) * canvasSize.y;
        drawList->AddCircleFilled(ImVec2(highX * 0.9f, yPos), 6.0f, ImGui::ColorConvertFloat4ToU32(markerColor));
        drawList->AddCircle(ImVec2(highX * 0.9f, yPos), 8.0f, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.5f)), 0, 2.0f);
    }
    float pulseAlpha = (std::sin(animTime * 3.0f) + 1.0f) * 0.5f * 0.4f + 0.1f;
    ImVec4 borderColor = ImVec4(1.0f, 1.0f, 1.0f, pulseAlpha);
    drawList->AddRect(canvasTopLeft, canvasBottomRight, ImGui::ColorConvertFloat4ToU32(borderColor), 5.0f, 0, 2.0f);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}


#include <windows.h>
#include <commdlg.h>  // For GetOpenFileName



std::string OpenFileDialog() {
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn;               // Use the ANSI version explicitly
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;

    // Must be double-null terminated
    ofn.lpstrFilter =
        "VST Plugins (*.dll;*.vst3)\0*.dll;*.vst3\0"
        "All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
    return "";
}

void DrawReverbVisualization(float size, float decay, float damping, float width,
    float amount, float preDelay, float makeupGain,
    float canvasHeight, float lineWidth)
{


    // Fixed small canvas
    const float cw = 140.0f;
    const float ch = 180.0f;
    ImVec2 canvasSize = ImVec2(cw, ch);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::BeginChild("ReverbCone", canvasSize, true);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 tl = ImGui::GetCursorScreenPos();
    ImVec2 center = ImVec2(tl.x + cw * 0.5f, tl.y + ch * 0.50f);

    float topY = tl.y + ch * 0.20f;
    float midY = tl.y + ch * 0.50f;
    float botY = tl.y + ch * 0.80f;

    float baseR = cw * 0.32f;
    float topR = cw * 0.18f;
    float midR = (baseR + topR) * 0.5f;
    float squash = 0.42f;


    float thickness = (lineWidth > 0.0f ? lineWidth : 2.0f);
    ImVec4 baseCol = ImVec4(0.80f, 0.88f, 1.00f, 0.85f);
    ImU32 lineCol = ImGui::ColorConvertFloat4ToU32(baseCol);

    auto DrawRing = [&](float cy, float rx, float glowScale)
        {
            const int seg = 64; float ry = rx * squash; ImVec2 prev;
            // glow passes
            for (float g = thickness * 3.0f * glowScale; g >= thickness; g -= (thickness * 0.75f)) {
                ImVec4 gc = baseCol; gc.w = 0.06f * (g / (thickness * 3.0f));
                ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(gc);
                ImVec2 prv; bool hasPrev = false;
                for (int i = 0; i <= seg; ++i) {
                    float a = (i / (float)seg) * 6.2831853f;
                    ImVec2 p = ImVec2(center.x + std::cos(a) * rx, cy + std::sin(a) * ry);
                    if (hasPrev) dl->AddLine(prv, p, glowCol, g);
                    prv = p; hasPrev = true;
                }
            }
            // solid pass
            for (int i = 0; i <= seg; ++i) {
                float a = (i / (float)seg) * 6.2831853f;
                ImVec2 p = ImVec2(center.x + std::cos(a) * rx, cy + std::sin(a) * ry);
                if (i > 0) dl->AddLine(prev, p, lineCol, thickness);
                prev = p;
            }
        };


    DrawRing(topY, topR, 1.0f);
    DrawRing(midY, midR, 0.8f);
    DrawRing(botY, baseR, 1.2f);


    const int struts = 12;
    for (int s = 0; s < struts; ++s) {
        float a = (s / (float)struts) * 6.2831853f;
        float ctx = std::cos(a), sty = std::sin(a);

        ImVec2 pt = ImVec2(center.x + ctx * topR, topY + sty * (topR * squash));
        ImVec2 pm = ImVec2(center.x + ctx * midR, midY + sty * (midR * squash));
        ImVec2 pb = ImVec2(center.x + ctx * baseR, botY + sty * (baseR * squash));


        float alphaMul = (sty < 0.0f) ? 0.55f : 1.0f;
        ImVec4 lc = baseCol; lc.w *= alphaMul;
        ImU32 sc = ImGui::ColorConvertFloat4ToU32(lc);


        ImVec4 glc = lc; glc.w *= 0.18f; ImU32 sg = ImGui::ColorConvertFloat4ToU32(glc);
        dl->AddLine(pt, pb, sg, thickness * 2.5f);
        dl->AddLine(pt, pb, sc, thickness);


        ImU32 nodeOuter = ImGui::ColorConvertFloat4ToU32(ImVec4(0.90f, 0.96f, 1.0f, 0.9f));
        ImU32 nodeInner = ImGui::ColorConvertFloat4ToU32(ImVec4(0.90f, 0.96f, 1.0f, 0.65f));
        float rOuter = 3.0f; float rInner = 2.0f;
        dl->AddCircleFilled(pt, rOuter, nodeInner);
        dl->AddCircle(pt, rOuter + 1.0f, nodeOuter, 0, 1.5f);
        dl->AddCircleFilled(pm, rOuter, nodeInner);
        dl->AddCircle(pm, rOuter + 1.0f, nodeOuter, 0, 1.5f);
        dl->AddCircleFilled(pb, rOuter, nodeInner);
        dl->AddCircle(pb, rOuter + 1.0f, nodeOuter, 0, 1.5f);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

#include <complex>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "imgui/imgui_internal.h"

void DrawClarityGraph(float clarity_q, float clarity_freq, float clarity_gain, float clarity_mix, float canvasHeight, float lineThickness = 2.0f) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImVec2(ImGui::GetContentRegionAvail().x, canvasHeight);

    if (canvas_size.x < 80.0f) canvas_size.x = 80.0f;
    if (canvas_size.y < 80.0f) canvas_size.y = 80.0f;

    // --- Background: dark with neon border ---
    draw_list->AddRectFilled(p, ImVec2(p.x + canvas_size.x, p.y + canvas_size.y),
        IM_COL32(10, 10, 14, 240)); // very dark
    draw_list->AddRect(p, ImVec2(p.x + canvas_size.x, p.y + canvas_size.y),
        IM_COL32(0, 255, 200, 180), 3.0f, 0, 2.0f); // neon aqua border

    // --- Grid lines for cheat-like look ---
    int gridLinesX = 10;
    int gridLinesY = 6;
    for (int i = 1; i < gridLinesX; i++) {
        float x = p.x + (i / (float)gridLinesX) * canvas_size.x;
        draw_list->AddLine(ImVec2(x, p.y), ImVec2(x, p.y + canvas_size.y), IM_COL32(40, 40, 60, 180));
    }
    for (int j = 1; j < gridLinesY; j++) {
        float y = p.y + (j / (float)gridLinesY) * canvas_size.y;
        draw_list->AddLine(ImVec2(p.x, y), ImVec2(p.x + canvas_size.x, y), IM_COL32(40, 40, 60, 180));
    }

    // --- Frequency response curve ---
    const int num_points = 256;
    static ImVec2 points[num_points];

    float sampleRate = 48000.0f;
    float nyquist = sampleRate / 2.0f;

    for (int i = 0; i < num_points; i++) {
        float norm = (float)i / (num_points - 1);
        float freq = 20.0f * powf(1000.0f, norm * 3.0f); // pseudo-log scale
        if (freq > nyquist) freq = nyquist;

        // --- Peaking EQ magnitude response ---
        float A = powf(10.0f, clarity_gain / 40.0f);
        float w0 = 2.0f * M_PI * freq / sampleRate;
        float alpha = sinf(w0) / (2.0f * clarity_q);

        float b0 = 1.0f + alpha * A;
        float b1 = -2.0f * cosf(w0);
        float b2 = 1.0f - alpha * A;
        float a0 = 1.0f + alpha / A;
        float a1 = -2.0f * cosf(w0);
        float a2 = 1.0f - alpha / A;

        std::complex<float> jw(cosf(w0), sinf(w0));
        std::complex<float> num = b0 + b1 * std::exp(-jw) + b2 * std::exp(-jw * 2.0f);
        std::complex<float> den = a0 + a1 * std::exp(-jw) + a2 * std::exp(-jw * 2.0f);
        float magnitude = std::abs(num / den);

        // Mix dry/wet
        float response = (1.0f - clarity_mix) * 1.0f + clarity_mix * magnitude;
        float db = 20.0f * log10f(response);

        // Map dB range (-20dB to +20dB) to screen
        float y = (0.5f - db / 40.0f) * canvas_size.y;
        y = ImClamp(y, 0.0f, canvas_size.y - 1.0f);

        points[i] = ImVec2(p.x + norm * canvas_size.x, p.y + y);
    }

    // --- Glow effect: draw curve multiple times with fading alpha ---
    for (int g = 6; g >= 1; g--) {
        draw_list->AddPolyline(points, num_points,
            IM_COL32(0, 255, 180, 25 * g), false, lineThickness + g);
    }

    // --- Main curve (bright neon line) ---
    draw_list->AddPolyline(points, num_points, IM_COL32(0, 255, 200, 255), false, lineThickness);

    // Reserve layout space
    ImGui::Dummy(canvas_size);
}


static bool showReverbVisualization = false;


#include <string>
#include "ImGui/imgui/imgui.h"
#include "ImGui/imgui/imgui_internal.h"
#include <memory>
#include <windows.h>
#include <string>
#include "ThemeSystem.h"
#include "EffectsSystem.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")


static std::string GetClipboardFilePath() {
    std::string result;

    if (OpenClipboard(NULL)) {
        HANDLE hData = GetClipboardData(CF_HDROP);
        if (hData) {
            HDROP hDrop = (HDROP)GlobalLock(hData);
            if (hDrop) {
                char filePath[MAX_PATH];
                if (DragQueryFileA(hDrop, 0, filePath, MAX_PATH)) {
                    result = filePath;
                }
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }

    return result;
}

void RenderVSTControls() {
    if (!g_vstHost) return;

    static int selectedPlugin = -1;
    static bool selectAll = false;

    // --- Header ---
    ImGui::SeparatorText("VST Plugin Workstation");

    // --- Top Controls ---
    if (ImGui::Button("+ Add Plugin", ImVec2(120, 28))) {
        std::string file = OpenFileDialog();
        if (!file.empty()) {
            int idx = g_vstHost->loadPlugin(file);
            if (idx >= 0) {
                if (VSTPlugin* plugin = g_vstHost->getPlugin(idx)) {
                    plugin->resume();
                }
            }
        }
    }
    ImGui::SameLine();

    if (ImGui::Button("Select All", ImVec2(90, 28))) {
        selectAll = true;
        selectedPlugin = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All", ImVec2(100, 28))) {
        for (int i = g_vstHost->getPluginCount() - 1; i >= 0; --i) {
            g_vstHost->unloadPlugin(i);
        }
        selectedPlugin = -1;
        selectAll = false;
    }

    // --- Keyboard Shortcut: Ctrl+V to paste DLL path ---
    ImGuiIO& io = ImGui::GetIO();
    bool ctrlV = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false);
    if (ctrlV) {
        std::string pastedPath = GetClipboardFilePath();
        if (!pastedPath.empty()) {
            int idx = g_vstHost->loadPlugin(pastedPath);
            if (idx >= 0) {
                if (VSTPlugin* plugin = g_vstHost->getPlugin(idx)) {
                    plugin->resume();
                }
            }
        }
        else {
            ImGui::OpenPopup("ClipboardError");
        }
    }

    // Error popup if paste failed
    if (ImGui::BeginPopup("ClipboardError")) {
        ImGui::Text("No DLL file found in clipboard!");
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- Plugin Chain ---
    int pluginCount = g_vstHost->getPluginCount();
    ImGui::Text("Plugin Chain (%d active)", pluginCount);

    if (ImGui::BeginChild("PluginChain", ImVec2(0, 350), true, ImGuiWindowFlags_NoScrollbar)) {
        for (int i = 0; i < pluginCount; ++i) {
            VSTPlugin* plugin = g_vstHost->getPlugin(i);
            if (!plugin) continue;

            ImGui::PushID(i);

            // Highlight state
            ImVec4 bgColor;
            if (selectAll) {
                bgColor = ImVec4(0.15f, 0.35f, 0.15f, 0.95f);
            }
            else if (selectedPlugin == i) {
                bgColor = ImVec4(0.10f, 0.25f, 0.45f, 0.95f);
            }
            else {
                bgColor = ImVec4(0.30f, 0.10f, 0.10f, 0.85f);
            }

            ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);

            if (ImGui::BeginChild("PluginCard", ImVec2(ImGui::GetContentRegionAvail().x, 120), true)) {
                // Header
                ImGui::Text("%d. %s", i + 1, plugin->getEffectName().c_str());
                if (!plugin->getVendorName().empty()) {
                    ImGui::TextDisabled("by %s", plugin->getVendorName().c_str());
                }

                // Status
                if (plugin->isBypassed())
                    ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Status: Bypassed");
                else
                    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Status: Active");

                ImGui::Spacing();

                // --- Buttons ---
                if (plugin->hasEditor()) {
                    if (ImGui::Button("Open UI", ImVec2(80, 26))) {
                        if (!plugin->isEditorOpen()) plugin->openEditor(nullptr);
                        else plugin->closeEditor();
                    }
                }
                else {
                    ImGui::BeginDisabled();
                    ImGui::Button("No UI", ImVec2(80, 26));
                    ImGui::EndDisabled();
                }

                ImGui::SameLine();
                if (ImGui::Button("Bypass", ImVec2(80, 26))) {
                    plugin->setBypassed(!plugin->isBypassed());
                }

                ImGui::SameLine();
                if (ImGui::Button("Remove", ImVec2(80, 26))) {
                    g_vstHost->unloadPlugin(i);
                    selectedPlugin = -1;
                    selectAll = false;
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                    break;
                }

                ImGui::SameLine();
                if (ImGui::Button("Edit Text", ImVec2(100, 26))) {
                    ImGui::OpenPopup("ChunkEditor");
                }

                // --- Popup for chunk editing (standalone text editor) ---
                if (ImGui::BeginPopup("ChunkEditor")) {
                    static char chunkBuffer[8192] = { 0 };

                    ImGui::Text("Chunk Editor");
                    ImGui::Separator();

                    ImGui::InputTextMultiline("##ChunkData",
                        chunkBuffer,
                        IM_ARRAYSIZE(chunkBuffer),
                        ImVec2(500, 200),
                        ImGuiInputTextFlags_AllowTabInput);

                    if (ImGui::Button("Save")) {
                        // Do whatever you want with chunkBuffer here (write to file, log, etc.)
                        // Example: printf("%s\n", chunkBuffer);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel")) {
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }

                // Selection
                if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    selectedPlugin = i;
                    selectAll = false;
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopID();
            ImGui::Spacing();
        }
    }
    ImGui::EndChild();
}








void RenderSafeParticles() {
    static bool initialized = false;
    static std::vector<Particle> particles(numParticles);

    ImVec2 screenSize = ImGui::GetIO().DisplaySize;

    // Initialize particle positions and velocities
    if (!initialized) {
        for (auto& p : particles) {
            p.pos = ImVec2(rand() % (int)screenSize.x, rand() % (int)screenSize.y);
            p.vel = ImVec2(
                ((rand() % 100) / 50.0f - 1.0f) * 0.5f,
                ((rand() % 100) / 50.0f - 1.0f) * 0.5f
            );
        }
        initialized = true;
    }

    // ✅ Use Foreground so they appear above window bg but behind content
    ImDrawList* draw = ImGui::GetForegroundDrawList();

    // Collect all active window + child rects to block drawing
    ImGuiContext& g = *GImGui;
    std::vector<ImRect> blockedAreas;
    for (int i = 0; i < g.Windows.Size; i++) {
        ImGuiWindow* win = g.Windows[i];
        if (win->Active && win->WasActive) {
            blockedAreas.push_back(win->Rect());
        }
    }

    auto isBlocked = [&](ImVec2 pos) {
        for (const auto& r : blockedAreas) {
            if (r.Contains(pos)) return true;
        }
        return false;
        };

    for (int i = 0; i < numParticles; ++i) {
        Particle& p = particles[i];
        p.pos.x += p.vel.x;
        p.pos.y += p.vel.y;

        // Bounce off edges
        if (p.pos.x < 0 || p.pos.x > screenSize.x) p.vel.x *= -1;
        if (p.pos.y < 0 || p.pos.y > screenSize.y) p.vel.y *= -1;

        // Skip particles inside any ImGui window/child
        if (isBlocked(p.pos)) continue;

        // Draw particles
        draw->AddCircleFilled(p.pos, 1.5f, IM_COL32(255, 255, 255, 160));

        // Connecting lines
        for (int j = i + 1; j < numParticles; ++j) {
            if (isBlocked(particles[j].pos)) continue;

            float dx = particles[j].pos.x - p.pos.x;
            float dy = particles[j].pos.y - p.pos.y;
            float distSq = dx * dx + dy * dy;

            if (distSq < 9000.0f) { // ~95px distance
                float alpha = 1.0f - (distSq / 9000.0f);
                draw->AddLine(p.pos, particles[j].pos,
                    IM_COL32(255, 255, 255, (int)(alpha * 100)), 1.0f);
            }
        }
    }
}




enum { TAB_HOME = 0, TAB_AUDIO, TAB_OPUS, TAB_DECODER, TAB_WEBRTC, TAB_SOUNDPAD,
       TAB_VST, TAB_VISUALIZER, TAB_THEMES, TAB_SETTINGS, TAB_ABOUT, TAB_COUNT };

static const char* TabLabel(int i){
    static const char* L[TAB_COUNT] = {"Home","Audio","Opus","Decoder","WebRTC",
        "Soundpad","VST","Visualizer","Themes","Settings","About"};
    return L[i];
}
static const char* TabSub(int i){
    static const char* L[TAB_COUNT] = {"Dashboard","Audio FX rack","Opus encoder",
        "Voice analysis","Realtime transport","Soundboard","Plugin workstation",
        "Graphs & scopes","Appearance studio","Preferences","Credits"};
    return L[i];
}

static ImFont* fReg = nullptr;
static ImFont* fSmall = nullptr;
static ImFont* fBig = nullptr;
char    g_toastBuf[192] = "";
double  g_toastTime = 0.0;
static void Toast(const char* msg){ strncpy_s(g_toastBuf, msg, _TRUNCATE); g_toastTime = 2.6; }

// ------------------------------------------------------------- vector icons
static void NavIcon(ImDrawList* dl, int tab, ImVec2 c, float s, ImU32 col){
    switch (tab) {
    case TAB_HOME:
        dl->AddLine(ImVec2(c.x-s,c.y), ImVec2(c.x,c.y-s*1.05f), col, 2);
        dl->AddLine(ImVec2(c.x,c.y-s*1.05f), ImVec2(c.x+s,c.y), col, 2);
        dl->AddLine(ImVec2(c.x-s,c.y), ImVec2(c.x-s,c.y+s*0.85f), col, 2);
        dl->AddLine(ImVec2(c.x+s,c.y), ImVec2(c.x+s,c.y+s*0.85f), col, 2);
        dl->AddRectFilled(ImVec2(c.x-s*0.35f,c.y+s*0.25f), ImVec2(c.x+s*0.35f,c.y+s*0.85f), col, 2.f);
        break;
    case TAB_AUDIO:
        for (int i=-1;i<=1;i++){ float y=c.y+i*s*0.5f; dl->AddLine(ImVec2(c.x-s,y),ImVec2(c.x+s,y),col,2);
            float k=s*0.45f*(float)(i+1)/1.5f; dl->AddCircleFilled(ImVec2(c.x+k,y),s*0.24f,col); }
        break;
    case TAB_OPUS:
        for (int i=0;i<12;i++){ float x=c.x-s+i*(2*s/11.f); float h=fabsf(sinf(i*0.9f))*s*0.7f+s*0.15f;
            dl->AddLine(ImVec2(x,c.y-h),ImVec2(x,c.y+h),col,(i%4==0)?2.4f:1.6f); }
        break;
    case TAB_DECODER:
        dl->AddCircle(c, s, col, 20, 2.f);
        dl->AddCircleFilled(c, s*0.28f, col);
        break;
    case TAB_WEBRTC:
        dl->AddCircle(c, s*0.95f, col, 22, 2.f);
        dl->AddCircle(c, s*0.55f, spFadeAlpha(col,0.6f), 18, 1.5f);
        dl->AddCircleFilled(c, s*0.18f, col);
        break;
    case TAB_SOUNDPAD:
        for (int i=0;i<2;i++) for (int j=0;j<2;j++)
            dl->AddRectFilled(ImVec2(c.x-s+i*s-1, c.y-s+j*s-1), ImVec2(c.x-s+i*s+s-3, c.y-s+j*s+s-3), col, 2.f);
        break;
    case TAB_VST:
        dl->AddCircle(c, s*0.85f, col, 20, 2.2f);
        dl->AddLine(c, ImVec2(c.x+s*0.55f, c.y-s*0.55f), col, 2.2f);
        break;
    case TAB_VISUALIZER:
        dl->AddRectFilled(ImVec2(c.x-s,c.y+s*0.2f), ImVec2(c.x-s*0.55f,c.y-s*0.5f), col, 1.f);
        dl->AddRectFilled(ImVec2(c.x-s*0.35f,c.y+s*0.2f), ImVec2(c.x+s*0.1f,c.y-s*0.9f), col, 1.f);
        dl->AddRectFilled(ImVec2(c.x+s*0.3f,c.y+s*0.2f), ImVec2(c.x+s*0.75f,c.y-s*0.2f), col, 1.f);
        break;
    case TAB_THEMES:
        dl->AddCircleFilled(ImVec2(c.x-s*0.35f,c.y-s*0.35f), s*0.42f, col);
        dl->AddCircleFilled(ImVec2(c.x+s*0.38f,c.y-s*0.30f), s*0.42f, spFadeAlpha(col,0.65f));
        dl->AddCircleFilled(ImVec2(c.x,c.y+s*0.42f), s*0.42f, spFadeAlpha(col,0.40f));
        break;
    case TAB_SETTINGS:
        dl->AddCircle(c, s*0.62f, col, 16, 2.4f);
        for (int k=0;k<8;k++){ float a=k*SPTAU/8.f;
            dl->AddLine(ImVec2(c.x+cosf(a)*s*0.72f,c.y+sinf(a)*s*0.72f), ImVec2(c.x+cosf(a)*s*1.02f,c.y+sinf(a)*s*1.02f), col, 2.f); }
        break;
    case TAB_ABOUT:
        dl->AddCircle(c, s*0.95f, col, 22, 2.f);
        dl->AddText(ImVec2(c.x-2.5f, c.y-s*0.75f), col, "i");
        break;
    }
}
static float EaseOut(float t){ t = spClampf(t,0.f,1.f); return 1.f - (1.f-t)*(1.f-t); }

// ===================================================================== HOME
static void Page_Home(ImTextureID logo, double uptime, float wallT){
    ImGuiIO& io = ImGui::GetIO();
    ImVec4 acc = g_palCur.accent;

    // hero panel
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float heroH = 150;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, spCol(spLerpV(g_palCur.panel, acc, 0.10f), 0.55f));
    ImGui::BeginChild("##hero", ImVec2(avail.x, heroH), true);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 hp = ImGui::GetWindowPos(), hs = ImGui::GetWindowSize();
    if (g_ui.glow) {
        float pulse = 0.6f + 0.4f*sinf(wallT*2.0f);
        for (int r = 3; r >= 1; --r)
            dl->AddCircle(ImVec2(hp.x+hs.x-64, hp.y+hs.y*0.5f), 30+r*9.0f*pulse, spFadeAlpha(spCol(acc), 0.045f*r), 40, 3.5f);
    }
    if (logo){
        float lsz = 62 + 3.0f*sinf(wallT*2.0f);
        ImGui::SetCursorScreenPos(ImVec2(hp.x+hs.x-64-lsz*0.5f, hp.y+hs.y*0.5f-lsz*0.5f));
        ImGui::Image(logo, ImVec2(lsz,lsz));
    }
    ImGui::SetCursorScreenPos(ImVec2(hp.x+18, hp.y+16));
    ImGui::PushFont(fBig);
    ImGui::TextColored(acc, "SpidermanHook");
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(hp.x+20, hp.y+58));
    ImGui::TextDisabled("by zion btw / blasphemyw");
    ImGui::SetCursorScreenPos(ImVec2(hp.x+20, hp.y+82));
    ImGui::TextWrapped("High-end voice suite for Discord - opus DSP, VST rack, live analysis.");
    char ver[64]; snprintf(ver, sizeof(ver), "v0.2  |  DX11  |  %s", g_Themes[g_ui.themeIndex].name);
    ImGui::SetCursorScreenPos(ImVec2(hp.x+20, hp.y+heroH-28));
    ImGui::TextColored(Dimmed(acc,1.25f), "%s", ver);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // status chips
    float cw = (avail.x-24)/4.0f;
    auto StatChip = [&](const char* k, const char* v, ImU32 dot){
        ImGui::BeginChild(k, ImVec2(cw,54), true);
        ImVec2 cp = ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(cp.x+12, cp.y+12), 4, dot);
        ImGui::SetCursorScreenPos(ImVec2(cp.x+22, cp.y+6));
        ImGui::TextDisabled("%s", k);
        ImGui::SetCursorScreenPos(ImVec2(cp.x+12, cp.y+26));
        ImGui::TextUnformatted(v);
        ImGui::EndChild();
        ImGui::SameLine();
    };
    static auto boot = std::chrono::steady_clock::now();
    int upS = (int)uptime;
    StatChip("Voice hook",   "ACTIVE", IM_COL32(60,220,90,255));
    StatChip("VST plugins",  []{ static char b[32]; snprintf(b,32,"%d loaded", g_vstHost?(int)g_vstHost->getPluginCount():0); return b; }(), spCol(acc));
    StatChip("Framerate",    [&]{ static char b[32]; snprintf(b,32,"%.0f fps", io.Framerate); return b; }(), spCol(g_palCur.accent2));
    StatChip("Uptime",       [upS]{ static char b[32]; snprintf(b,32,"%02d:%02d:%02d", upS/3600,(upS/60)%60,upS%60); return b; }(), IM_COL32(200,200,210,255));
    ImGui::NewLine();

    // two columns: quick settings / recent activity
    float half = (ImGui::GetContentRegionAvail().x-10)/2;
    ImGui::BeginGroup();
    ImGui::BeginChild("##quick", ImVec2(half, 240), true);
    ImGui::SeparatorText("Quick settings");
    ImGui::Checkbox("Client detection (dB)", &dbcheck);
    ImGui::Checkbox("Int16 check",           &int16check);
    ImGui::Checkbox("Self check",            &selfcheck);
    ImGui::Separator();
    ImGui::Checkbox("Particles", &g_ui.particles);
    ImGui::Checkbox("Glow",      &g_ui.glow);
    ImGui::Checkbox("Rainbow accent", &g_ui.rainbow);
    int ti = g_ui.themeIndex;
    if (ImGui::Combo("Theme", &ti, [](void*,int n){ return g_Themes[n].name; }, nullptr, THEME_COUNT))
        { SetTheme(ti); AddLog("[Theme] %s applied.", g_Themes[ti].name); }
    ImGui::EndChild();
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginChild("##recent", ImVec2(half, 240), true);
    ImGui::SeparatorText("Recent activity");
    int n = (int)logEntries.size();
    int startI = n > 8 ? n-8 : 0;
    for (int i = startI; i < n; ++i) {
        ImGui::TextColored(logEntries[i].prefixColor, "%s", logEntries[i].prefix.c_str());
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextDisabled("%s", logEntries[i].message.c_str());
    }
    ImGui::EndChild();
}
// ===================================================================== AUDIO
static void Page_Audio(){
    float half = (ImGui::GetContentRegionAvail().x-10)/2;
    ImGui::BeginChild("##fxrack", ImVec2(half, 0), true);
    ImGui::SeparatorText("Effects Rack");
    if (ImGui::TreeNode("Equalizer")) {
        ImGui::Checkbox("Apply EQ", &apply_eq);
        ImGui::SliderFloat("Low Gain",  &eq_low_gain,  -12.0f, 12.0f);
        ImGui::SliderFloat("Mid Gain",  &eq_mid_gain,  -12.0f, 12.0f);
        ImGui::SliderFloat("High Gain", &eq_high_gain, -12.0f, 12.0f);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Low-pass Filter")) {
        ImGui::Checkbox("Enable", &apply_lowpass);
        ImGui::SliderFloat("Frequency", &lowpass_freq, 500.0f, 20000.0f); ImGui::TreePop();
    }
    if (ImGui::TreeNode("High-pass Filter")) {
        ImGui::Checkbox("Enable", &apply_highpass);
        ImGui::SliderFloat("Frequency", &highpass_freq, 20.0f, 500.0f); ImGui::TreePop();
    }
    if (ImGui::TreeNode("Saturation")) {
        ImGui::Checkbox("Enable", &apply_saturation);
        ImGui::SliderFloat("Drive", &saturation_drive, 0.5f, 10.0f); ImGui::TreePop();
    }
    if (ImGui::TreeNode("Bitcrusher")) {
        ImGui::Checkbox("Enable", &apply_bitcrusher);
        ImGui::SliderInt("Bits", &bitcrusher_bits, 4, 16);
        ImGui::SliderFloat("Rate", &bitcrusher_rate, 0.01f, 1.0f); ImGui::TreePop();
    }
    if (ImGui::TreeNode("Noise Gate")) {
        ImGui::Checkbox("Enable", &apply_gate);
        ImGui::SliderFloat("Threshold", &gate_threshold, 0.0f, 1.0f);
        ImGui::SliderFloat("Attack", &gate_attack, 0.001f, 0.1f);
        ImGui::SliderFloat("Release", &gate_release, 0.01f, 0.5f); ImGui::TreePop();
    }
    if (ImGui::TreeNode("Chorus")) {
        ImGui::Checkbox("Enable", &apply_chorus);
        ImGui::SliderFloat("Rate", &chorus_rate, 0.1f, 5.0f);
        ImGui::SliderFloat("Depth", &chorus_depth, 0.0f, 1.0f); ImGui::TreePop();
    }
    if (ImGui::TreeNode("Flanger")) {
        ImGui::Checkbox("Enable", &apply_flanger);
        ImGui::SliderFloat("Rate", &flanger_rate, 0.05f, 2.0f);
        ImGui::SliderFloat("Depth", &flanger_depth, 0.0f, 1.0f); ImGui::TreePop();
    }
    if (ImGui::TreeNode("Tremolo")) {
        ImGui::Checkbox("Enable", &apply_tremolo);
        ImGui::SliderFloat("Rate", &tremolo_rate, 1.0f, 20.0f);
        ImGui::SliderFloat("Depth", &tremolo_depth, 0.0f, 1.0f); ImGui::TreePop();
    }
    if (ImGui::TreeNode("Stereo Enhancer")) {
        ImGui::Checkbox("Enable", &apply_stereo_enhancer);
        ImGui::SliderFloat("Width", &stereoWidth, 0.0f, 2.0f);
        ImGui::SliderFloat("Pan", &pan, -1.0f, 1.0f); ImGui::TreePop();
    }
    if (ImGui::TreeNode("Delay")) {
        ImGui::Checkbox("Enable", &apply_delay);
        ImGui::SliderFloat("Time", &delay_time, 0.05f, 1.0f);
        ImGui::SliderFloat("Feedback", &delay_feedback, 0.0f, 1.0f); ImGui::TreePop();
    }
    if (ImGui::TreeNode("Auto Gain")) {
        ImGui::Checkbox("Enable", &apply_auto_gain);
        ImGui::SliderFloat("Target Gain", &auto_gain_target, 0.1f, 1.5f); ImGui::TreePop();
    }
    if (ImGui::TreeNode("Pitch Shift")) {
        ImGui::Checkbox("Enable", &apply_pitch_shift);
        ImGui::SliderFloat("Semitones", &pitch_semitones, -12.0f, 12.0f); ImGui::TreePop();
    }
    if (ImGui::TreeNode("Invert Phase")) {
        ImGui::Checkbox("Enable", &invert_phase); ImGui::TreePop();
    }
    if (ImGui::TreeNode("Soft Clip")) {
        ImGui::Checkbox("Enable", &apply_softclip);
        ImGui::SliderFloat("Curve", &softclip_curve, 0.1f, 1.0f); ImGui::TreePop();
    }
    if (ImGui::TreeNode("Output")) {
        ImGui::SliderFloat("Gain", &outputGain, 0.0f, 2.0f);
        ImGui::Checkbox("Mute", &muteOutput);
        ImGui::SliderFloat("Dry/Wet Mix", &dryWetMix, 0.0f, 1.0f);
        ImGui::Checkbox("Bypass All", &bypassAll);
        ImGui::TreePop();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##expander", ImVec2(0, 0), true);
    ImGui::SeparatorText("Expander");
    ImGui::Checkbox("Apply Expander", &apply_expander);
    ImGui::Spacing();
    ImGui::Dummy(ImVec2(0,10));
    ImGui::SeparatorText("Tips");
    ImGui::TextWrapped("Everything here is applied inside the opus encoder chain in realtime.");
    ImGui::EndChild();
}
// ====================================================================== OPUS
static void Page_Opus(float wallT){
    float half = (ImGui::GetContentRegionAvail().x-10)/2;
    ImGui::BeginChild("##opusL", ImVec2(half, 0), true);
    ImGui::SeparatorText("Gain & Reverb");
    ImGui::SliderFloat("dB", &Gain, 1.0f, 120.0f, "%.3f");
    ImGui::SliderFloat("Exploit dB", &ExpGain, 1.0f, 120.0f, "%.3f");
    ImGui::Separator();
    ImGui::SliderFloat("Reverb Size",   &reverbSize, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Damping",       &reverbDamping, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Width",         &reverbWidth, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Pre-delay",     &reverbPredelay, 0.0f, 0.1f, "%.3f");
    ImGui::SliderFloat("Makeup Gain",   &reverbMakeupGain, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Amount",        &ReverbAmount, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Decay",         &ReverbDecay, 0.0f, 1.0f, "%.2f");
    ImGui::Dummy(ImVec2(0,6));
    ImGui::SeparatorText("Clarity");
    ImGui::Checkbox("Clarity Enhancer", &apply_clarity_enhancer);
    ImGui::SliderFloat("Clarity Q",    &clarity_q,    0.1f, 5.0f);
    ImGui::SliderFloat("Clarity Freq", &clarity_freq, 200.0f, 8000.0f);
    ImGui::SliderFloat("Clarity Gain", &clarity_gain, -20.0f, 20.0f);
    ImGui::SliderFloat("Clarity Mix",  &clarity_mix,  0.0f, 1.0f);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##opusR", ImVec2(0, 0), true);
    ImGui::SeparatorText("Bitrate Engine");
    ImGui::SliderFloat("Bitrate", &Bitrate, 40000.0f, 510000.0f, "%.0f");
    ImGui::TextDisabled("Target: %.0f kbps", Bitrate/1000.0f);
    ImGui::Spacing();
    ImGui::SeparatorText("Encoder self-check graph");
    {
        std::lock_guard<std::mutex> lock(self_graph_mutex);
        if (!self_graph_data.empty())
            ImGui::PlotLines("##enc", self_graph_data.data(), (int)self_graph_data.size(), 0, nullptr, 0.0f, 0.01f, ImVec2(-1, 110));
        else
            ImGui::TextDisabled("(waiting for encoder frames)");
    }
    ImGui::Dummy(ImVec2(0,6));
    ImGui::SeparatorText("Decoder dB stream");
    {
        std::lock_guard<std::mutex> lock(db_graph_mutex);
        if (!db_graph_data.empty())
            ImGui::PlotLines("##dbg", db_graph_data.data(), (int)db_graph_data.size(), 0, nullptr, -60.0f, 60.0f, ImVec2(-1, 110));
    }
    ImGui::EndChild();
}
// =================================================================== DECODER
static void Page_Decoder(){
    ImGui::BeginChild("DecoderChild", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysUseWindowPadding);

    static float thresholdAdjust = 2.5f;

    float half = (ImGui::GetContentRegionAvail().x-10)*0.45f;
    ImGui::BeginChild("LeftPanel", ImVec2(half, 0), true);
    {
        ImGui::Text("General Options"); ImGui::Separator();
        ImGui::Checkbox("Client detection", &dbcheck);
        ImGui::Checkbox("Int16 Check",      &int16check);
        ImGui::Checkbox("Self Check",       &selfcheck);
        ImGui::Checkbox("Force Not Natty",  &forceNotNatty);
        ImGui::InputText("Decoder Name", yexDecoderText, IM_ARRAYSIZE(yexDecoderText));

        ImGui::Spacing();
        ImGui::Text("Spectrum Analyzer"); ImGui::Separator();
        if (int16check) {
            ImVec2 spectrumSize(ImGui::GetContentRegionAvail().x, 150);
            ImVec2 spectrumPos = ImGui::GetCursorScreenPos();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            DrawSpectrumAnalyzer(draw, spectrumPos, spectrumSize);
            ImGui::Dummy(spectrumSize);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
    {
        ImGui::Text("Decoder Stats"); ImGui::Separator();
        static float highestPeak = -100.0f;
        static int   clientDetectionCount = 0;
        static float lastSample = 0.0f;
        static bool  wasDetectedLastFrame = false;
        static float viewMin = -1.f, viewMax = 1.f;

        float rawSample = 0.0f;
        { std::lock_guard<std::mutex> lock(db_graph_mutex);
          if (!db_graph_data.empty()) rawSample = db_graph_data.back(); }

        if (lastSample > highestPeak) highestPeak = lastSample;
        const float detectionThreshold = thresholdAdjust;
        bool clientDetected = ((rawSample > detectionThreshold && dbcheck) || forceNotNatty);
        if (clientDetected && !wasDetectedLastFrame) clientDetectionCount++;
        wasDetectedLastFrame = clientDetected;

        ImGui::Text("Decoder Name: %s", yexDecoderText);
        ImGui::Text("Last Sample: %.2f dB", lastSample);
        ImGui::Text("Highest Peak: %.2f dB", highestPeak);
        ImGui::TextColored(clientDetected ? ImVec4(1,0.35f,0.35f,1) : ImVec4(0.4f,1,0.5f,1),
            "Status: %s", clientDetected ? "CLIENT DETECTED" : "natty");
        ImGui::SliderFloat("Threshold", &thresholdAdjust, 0.5f, 8.0f);

        ImGui::Spacing(); ImGui::Text("Live Graph"); ImGui::Separator();

        ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 150);
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 bottomRight(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

        const ImGuiStyle& style = ImGui::GetStyle();
        draw->AddRectFilled(canvasPos, bottomRight, IM_COL32(10,10,14,200), style.ChildRounding);
        draw->AddRect(canvasPos, bottomRight, ImGui::GetColorU32(style.Colors[ImGuiCol_Border]), style.ChildRounding);

        std::vector<float> s;
        { std::lock_guard<std::mutex> lock(db_graph_mutex); s = db_graph_data; }

        if (s.size() > 1) {
            float dataMin=s[0], dataMax=s[0];
            for (float v : s){ if(v<dataMin)dataMin=v; if(v>dataMax)dataMax=v; }
            float range=dataMax-dataMin, pad=(range*0.1f>1.0f)?range*0.1f:1.0f;
            float tmin=dataMin-pad, tmax=dataMax+pad;
            viewMin += (tmin-viewMin)*0.15f; viewMax += (tmax-viewMax)*0.15f;
            if (viewMax-viewMin < 6.0f){ float mid=0.5f*(viewMax+viewMin); viewMin=mid-3.f; viewMax=mid+3.f; }

            auto v2y = [&](float v){
                float tt = spClampf((v-viewMin)/(viewMax-viewMin),0.f,1.f);
                return canvasPos.y + canvasSize.y - tt*canvasSize.y; };

            int n=(int)s.size(); float dx=(n>1)?(canvasSize.x/(float)(n-1)):0.f;
            ImU32 nattyCol = IM_COL32(0,180,0,255), unnattyCol = IM_COL32(220,40,40,255);
            for (int i=0;i<n-1;++i){
                bool hot = forceNotNatty || ((s[i]>detectionThreshold || s[i+1]>detectionThreshold) && dbcheck);
                draw->AddLine(ImVec2(canvasPos.x+i*dx,v2y(s[i])), ImVec2(canvasPos.x+(i+1)*dx,v2y(s[i+1])),
                    hot?unnattyCol:nattyCol, 2.0f);
            }
            float threshY = v2y(detectionThreshold);
            draw->AddLine(ImVec2(canvasPos.x,threshY), ImVec2(bottomRight.x,threshY), IM_COL32(220,200,60,255), 1.5f);
        }
        ImGui::Dummy(canvasSize);
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

// =================================================================== WEBRTC
static void Page_WebRTC(float wallT){
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float cw = (avail.x-20)/3.0f;
    auto Card = [&](const char* title, const char* big, ImU32 dot, const char* sub){
        ImGui::BeginChild(title, ImVec2(cw, 78), true);
        ImVec2 cp = ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(cp.x+14, cp.y+16), 4.5f, dot);
        ImGui::SetCursorScreenPos(ImVec2(cp.x+26, cp.y+10));
        ImGui::TextDisabled("%s", title);
        ImGui::SetCursorScreenPos(ImVec2(cp.x+12, cp.y+28));
        ImGui::TextUnformatted(big);
        ImGui::SetCursorScreenPos(ImVec2(cp.x+12, cp.y+52));
        ImGui::TextDisabled("%s", sub);
        ImGui::EndChild();
        ImGui::SameLine();
    };
    Card("Voice transport", "CONNECTED", IM_COL32(60,220,90,255), "discord_voice hooks armed");
    Card("Codec", "Opus 48 kHz", spCol(g_palCur.accent), "stereo / VST insert chain");
    Card("Processing", bypassAll ? "BYPASSED" : "LIVE", spCol(g_palCur.accent2),
         bypassAll ? "all effects disabled" : "DSP pipeline active");

    ImGui::NewLine();
    ImGui::SeparatorText("Voice processing");
    ImGui::Checkbox("Noise suppression", &apply_gate);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Maps to the Noise Gate in the Audio rack.");
    ImGui::Checkbox("High-pass filter",   &apply_highpass);
    ImGui::Checkbox("Automatic gain control", &apply_auto_gain);
    static bool echoCancel = false;
    ImGui::Checkbox("Echo cancellation (experimental)", &echoCancel);
    ImGui::SliderFloat("Stereo width", &stereoWidth, 0.0f, 2.0f);
    ImGui::Spacing();
    ImGui::SeparatorText("Transport notes");
    ImGui::BulletText("Hooks are installed when discord_voice.dll is loaded by the host.");
    ImGui::BulletText("Opus encode/decode chains and split-filter bypass stay active.");
}
// ================================================================= SOUNDPAD
static void Page_Soundpad(float wallT){
    static char  padNames[9][32];
    static float padFlash[9] = {};
    static float masterVol = 0.8f;
    static bool  initialized = false;
    if (!initialized) {
        for (int i = 0; i < 9; ++i) snprintf(padNames[i], 32, "Pad %d", i+1);
        initialized = true;
    }
    ImGui::SeparatorText("Soundboard");
    ImGui::TextDisabled("Assign quick sounds to pads. Playback binds to the pad key while injected.");
    ImGui::Spacing();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float pw = (avail.x - 24) / 3.0f, ph = 74;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < 9; ++i) {
        ImGui::PushID(i);
        char lbl[40]; snprintf(lbl, 40, "%s##pad", padNames[i]);
        ImGui::BeginChild(padNames[i], ImVec2(pw, ph), true);
        ImVec2 cp = ImGui::GetWindowPos(), cs = ImGui::GetWindowSize();
        if (padFlash[i] > 0) {
            padFlash[i] -= ImGui::GetIO().DeltaTime;
            float k = spClampf(padFlash[i]/0.35f, 0.f, 1.f);
            dl->AddRectFilled(cp, ImVec2(cp.x+cs.x, cp.y+cs.y), spFadeAlpha(spCol(g_palCur.accent), 0.28f*k), 6.f);
        }
        ImGui::SetCursorScreenPos(ImVec2(cp.x+8, cp.y+6));
        ImGui::InputText("##name", padNames[i], 32);
        ImGui::SetCursorScreenPos(ImVec2(cp.x+8, cp.y+38));
        if (ImGui::Button("Trigger", ImVec2(70,24))) padFlash[i] = 0.35f;
        ImGui::SameLine();
        char key[16]; snprintf(key,16,"F%d", i+1);
        ImGui::TextDisabled("%s", key);
        ImGui::EndChild();
        if ((i % 3) != 2) ImGui::SameLine();
        ImGui::PopID();
    }
    ImGui::NewLine();
    ImGui::SliderFloat("Master volume", &masterVol, 0.0f, 1.5f, "%.2f");
}
// ====================================================================== VST
static void Page_VST(){
    RenderVSTControls();
}

// =============================================================== VISUALIZER
static void Page_Visualizer(float wallT){
    static bool showEq = true, showClarity = false, showReverbViz = false;
    float half = (ImGui::GetContentRegionAvail().x-10)/2;

    ImGui::BeginChild("##vizL", ImVec2(half, 0), true);
    ImGui::SeparatorText("Spectrum");
    if (int16check) {
        ImVec2 sz(ImGui::GetContentRegionAvail().x, 170);
        ImVec2 pos0 = ImGui::GetCursorScreenPos();
        DrawSpectrumAnalyzer(ImGui::GetWindowDrawList(), pos0, sz);
        ImGui::Dummy(sz);
    } else {
        ImGui::TextDisabled("Enable 'Int16 Check' in Decoder to feed the spectrum.");
        ImGui::Dummy(ImVec2(0, 150));
    }
    ImGui::Spacing();
    ImGui::SeparatorText("Self check (DC reject)");
    { std::lock_guard<std::mutex> lock(self_graph_mutex);
      if (!self_graph_data.empty())
          ImGui::PlotLines("##selfg", self_graph_data.data(), (int)self_graph_data.size(),
                           0, nullptr, 0.0f, 0.01f, ImVec2(-1, 100));
      else ImGui::TextDisabled("(no data yet)"); }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##vizR", ImVec2(0, 0), true);
    ImGui::SeparatorText("Scopes");
    ImGui::Checkbox("EQ curve", &showEq);
    ImGui::SameLine(); ImGui::Checkbox("Clarity", &showClarity);
    ImGui::SameLine(); ImGui::Checkbox("Reverb cone", &showReverbViz);
    ImGui::Spacing();
    if (showEq)        DrawCoolEQVisualization(eq_low_gain, eq_mid_gain, eq_high_gain, 170.0f, 2.0f);
    if (showClarity)   DrawClarityGraph(clarity_q, clarity_freq, clarity_gain, clarity_mix, 140.0f);
    if (showReverbViz) DrawReverbVisualization(reverbSize, ReverbDecay, reverbDamping, reverbWidth,
                                               ReverbAmount, reverbPredelay, reverbMakeupGain, 160.0f, 2.0f);
    ImGui::Spacing();
    ImGui::SeparatorText("Decoder dB stream");
    { std::lock_guard<std::mutex> lock(db_graph_mutex);
      if (!db_graph_data.empty())
          ImGui::PlotLines("##vdb", db_graph_data.data(), (int)db_graph_data.size(),
                           0, nullptr, -60.0f, 60.0f, ImVec2(-1, 110)); }
    ImGui::EndChild();
}
// ===================================================== THEMES - mini preview
static void DrawMiniPreview(ImDrawList* dl, ImVec2 mn, ImVec2 mx, int fx, float t, ImU32 accent){
    ImVec2 sz = ImVec2(mx.x-mn.x, mx.y-mn.y);
    float cx = (mn.x+mx.x)*0.5f, cy = (mn.y+mx.y)*0.5f;
    srand((unsigned)(size_t)dl + fx);
    switch (fx) {
    case 0: // Autumn — simple radial burst of warm dots + arc leaf
        for (int i=0;i<10;i++){ float a=t*0.3f+i*SPTAU/10.f, r=10.f+4.f*sinf(a*3.f);
            dl->AddCircleFilled(ImVec2(cx+cosf(a)*r,cy+sinf(a)*r), 2.5f, accent, 8); }
        dl->AddCircle(ImVec2(cx,cy), 22.f, IM_COL32(180,80,30,200), 20, 1.5f);
        break;
    case 1: // Frost — concentric rings
        for (int i=0;i<3;i++){ float r=8.f+i*7.f+2.f*sinf(t*2.f+i);
            dl->AddCircle(ImVec2(cx,cy), r, spFadeAlpha(accent, 0.7f-i*0.18f), 24, 1.2f); }
        break;
    case 2: // Halloween — wobble eyes
        dl->AddCircleFilled(ImVec2(cx-10,cy-5), 5.f, accent, 10);
        dl->AddCircleFilled(ImVec2(cx+10,cy-5), 5.f, accent, 10);
        dl->AddCircleFilled(ImVec2(cx,cy+10), 3.f, accent, 8);
        break;
    case 3: // Cyberpunk — grid lines
        for (int i=-2;i<=2;i++){
            dl->AddLine(ImVec2(cx+i*10.f,mn.y+4),ImVec2(cx+i*10.f,mx.y-4),spFadeAlpha(accent,0.4f),1.f);
            dl->AddLine(ImVec2(mn.x+4,cy+i*10.f),ImVec2(mx.x-4,cy+i*10.f),spFadeAlpha(accent,0.4f),1.f); }
        dl->AddCircleFilled(ImVec2(cx,cy),4.f,accent,12);
        break;
    case 4: // Neon — crossing beams
        dl->AddLine(ImVec2(mn.x+6,cy),ImVec2(mx.x-6,cy),accent,2.5f);
        dl->AddLine(ImVec2(cx,mn.y+6),ImVec2(cx,mx.y-6),accent,2.5f);
        dl->AddCircle(ImVec2(cx,cy),14.f,spFadeAlpha(accent,0.5f),16,1.5f);
        break;
    case 5: // Blood — pulsing drip
        { float h=16.f+6.f*sinf(t*2.5f);
          dl->AddCircleFilled(ImVec2(cx,mn.y+8),6.f,accent,12);
          dl->AddLine(ImVec2(cx,mn.y+14),ImVec2(cx,mn.y+14+h),accent,2.5f); }
        break;
    case 6: // Matrix — falling chars
        for (int i=0;i<4;i++){ float y=mn.y+6+6.f*((int)(t*30.f+i*17)%30)/30.f*sz.y;
            dl->AddText(ImVec2(mn.x+6+i*14.f, y), accent, (char*)"\x01" + (i%10)); }
        break;
    case 7: // Synthwave — layered arcs
        for (int i=0;i<3;i++){ float a=t*0.6f+i*0.8f;
            dl->AddCircle(ImVec2(cx,cy+8.f), 18.f+i*4.f, spFadeAlpha(accent, 0.55f-i*0.12f), 24, 1.2f); }
        break;
    case 8: // Sakura — petals drift
        for (int i=0;i<5;i++){
            float px=mn.x+5+((int)(t*25.f+i*13)%30)/30.f*sz.x;
            float py=mn.y+5+((int)(t*18.f+i*11)%30)/30.f*sz.y;
            dl->AddCircleFilled(ImVec2(px,py),2.5f,spFadeAlpha(accent,0.6f),6); }
        break;
    case 9: // Glacier — steep triangle
        { ImVec2 pts[3]={ImVec2(cx-18,cy+14),ImVec2(cx+18,cy+14),ImVec2(cx,cy-16)};
          dl->AddConvexPolyFilled(pts,3,spFadeAlpha(accent,0.3f));
          dl->AddPolyline(pts,3,accent,ImDrawFlags_Closed,2.f); }
        break;
    case 10: // Violet — swirling blobs
        for (int i=0;i<3;i++){ float a=t*0.8f+i*SPTAU/3.f;
            dl->AddCircleFilled(ImVec2(cx+cosf(a)*12.f,cy+sinf(a)*12.f),5.f,spFadeAlpha(accent,0.4f),10); }
        break;
    case 11: // Gold — concentric squares
        for (int i=0;i<3;i++){ float k=8.f+i*6.f+2.f*sinf(t*1.5f);
            dl->AddRect(ImVec2(cx-k,cy-k),ImVec2(cx+k,cy+k),spFadeAlpha(accent,0.6f-i*0.15f),0,0,1.2f); }
        break;
    case 12: // Ember — flicker ring
        { float pulse=0.6f+0.4f*sinf(t*4.f);
          dl->AddCircle(ImVec2(cx,cy),18.f*pulse,spFadeAlpha(accent,0.4f),20,1.5f);
          dl->AddCircleFilled(ImVec2(cx,cy),4.f,accent,10); }
        break;
    case 13: // Shadow — vignetted cross
        dl->AddLine(ImVec2(cx,mn.y+4),ImVec2(cx,mx.y-4),spFadeAlpha(accent,0.35f),3.f);
        dl->AddLine(ImVec2(mn.x+4,cy),ImVec2(mx.x-4,cy),spFadeAlpha(accent,0.35f),3.f);
        dl->AddCircle(ImVec2(cx,cy),12.f,spFadeAlpha(accent,0.5f),20,1.2f);
        break;
    }
    srand((unsigned)ImGui::GetFrameCount());
}
// ========================================================== THEMES HUB PAGE
static void Page_Themes(float wallT, float dt){
    static char search[64] = "";
    static int  catFilter = -1;
    static int  lastHovered = -1;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float sideW = 280.f;
    float gridW = avail.x - sideW - 12.f;

    // left: grid of cards
    ImGui::BeginChild("##thgrid", ImVec2(gridW, 0), true);
    {
        ImVec2 gp0 = ImGui::GetCursorScreenPos();

        // search + category chips
        ImGui::PushItemWidth(220);
        ImGui::InputTextWithHint("##search", "Search...", search, 64);
        ImGui::PopItemWidth();
        ImGui::Spacing();

        int catCount = 0;
        const char* catLabels[THEME_COUNT];
        for (int i = 0; i < THEME_COUNT; ++i) {
            bool found = false;
            for (int j = 0; j < catCount; ++j) if (!strcmp(catLabels[j], g_Themes[i].cat)) { found = true; break; }
            if (!found) catLabels[catCount++] = g_Themes[i].cat;
        }
        if (ImGui::Button(catFilter == -1 ? "[ All ]" : " All ")) catFilter = -1;
        ImGui::SameLine();
        for (int c = 0; c < catCount; ++c) {
            char lbl[64]; snprintf(lbl, 64, "%s%s", catFilter == c ? "[ " : "", catLabels[c]);
            strcat(lbl, catFilter == c ? " ]" : "");
            ImGui::SameLine();
            if (ImGui::Button(lbl)) catFilter = (catFilter == c) ? -1 : c;
        }
        ImGui::Spacing();

        // recently used row
        ImGui::TextDisabled("Recent:");
        for (int r = 0; r < 3; ++r) {
            if (g_ui.recentThemes[r] < 0) continue;
            ImGui::SameLine();
            char rlbl[48]; snprintf(rlbl, 48, "%s##r%d", g_Themes[g_ui.recentThemes[r]].name, r);
            if (ImGui::SmallButton(rlbl)) { SetTheme(g_ui.recentThemes[r]);
                Toast(g_Themes[g_ui.recentThemes[r]].name); }
        }
        ImGui::Spacing();

        // card grid
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
        float cardW = 160.f, cardH = 110.f, cardPad = 14.f;
        int cols = (int)gridW / (int)(cardW + 8.f); if (cols < 1) cols = 1;
        ImVec2 gridStart = ImGui::GetCursorScreenPos();
        int visibleCount = 0;
        int passIdx = 0;
        ImGui::InvisibleButton("##gridbg", ImVec2(gridW - 20, 10)); // tiny anchor
        gridStart = ImGui::GetCursorScreenPos();
        ImVec2 cardTopLeft = gridStart;
        for (int i = 0; i < THEME_COUNT; ++i) {
            bool matchSearch = !search[0] || strstr(g_Themes[i].name, search) || strstr(g_Themes[i].cat, search);
            bool matchCat = catFilter < 0 || !strcmp(g_Themes[i].cat, catLabels[catFilter]);
            if (!matchSearch || !matchCat) continue;

            int col = visibleCount % cols;
            if (col == 0 && visibleCount > 0) {
                ImGui::SetCursorScreenPos(ImVec2(gridStart.x, cardTopLeft.y + cardH + 8.f));
                cardTopLeft = ImGui::GetCursorScreenPos();
            }
            ImVec2 cardPos(gridStart.x + col * (cardW + 8.f), cardTopLeft.y);
            ImVec2 cardMin = cardPos;
            ImVec2 cardMax(cardMin.x + cardW, cardMin.y + cardH);
            ImVec2 previewMin(cardMin.x + cardPad, cardMin.y + 4.f);
            ImVec2 previewMax(cardMax.x - cardPad, cardMin.y + cardH - 26.f);

            ImDrawList* dl = ImGui::GetWindowDrawList();
            bool selected = (g_ui.themeIndex == i);
            bool hovered = false;

            ImGui::SetCursorScreenPos(cardMin);
            char ibId[64]; snprintf(ibId, 64, "##card%d", i);
            ImGui::InvisibleButton(ibId, ImVec2(cardW, cardH));
            hovered = ImGui::IsItemHovered();

            ImU32 bg = spCol(spLerpV(g_palCur.panel, g_palCur.accent2, hovered ? 0.12f : 0.04f), 0.55f);
            dl->AddRectFilled(cardMin, cardMax, bg, 8.f);

            if (selected) {
                for (int r = 2; r >= 0; --r)
                    dl->AddRect(cardMin, cardMax, spFadeAlpha(spCol(g_palCur.accent), 0.12f * r), 8.f, 0, 1.5f + r);
            }
            if (g_ui.glow && hovered) {
                for (int r = 3; r >= 1; --r) {
                    float pulse = 0.5f + 0.5f * sinf(wallT * 2.5f + i * 0.5f);
                    dl->AddRect(cardMin, cardMax, spFadeAlpha(spCol(g_palCur.accent), 0.05f * r * pulse), 8.f, 0, 1.2f);
                }
            }

            // live mini preview
            DrawMiniPreview(dl, previewMin, previewMax, g_Themes[i].fx, wallT, spCol(g_palCur.accent));

            // favorite star
            ImVec2 starPos(cardMax.x - 18, cardMin.y + 3);
            char starId[32]; snprintf(starId, 32, "##fav%d", i);
            ImGui::SetCursorScreenPos(ImVec2(starPos.x - 4, starPos.y - 2));
            ImGui::InvisibleButton(starId, ImVec2(16, 16));
            if (ImGui::IsItemHovered()) {
                dl->AddCircleFilled(ImVec2(starPos.x + 4, starPos.y + 6), 8.f,
                    spFadeAlpha(spCol(g_palCur.accent), 0.25f), 12);
            }
            if (ImGui::IsItemClicked()) {
                g_ui.favorites[i] = !g_ui.favorites[i];
                AddLog("[Theme] %s favourited %s.", g_Themes[i].name, g_ui.favorites[i] ? "+" : "-");
            }
            ImU32 starCol = g_ui.favorites[i] ? IM_COL32(255, 200, 50, 255) : spFadeAlpha(IM_COL32(200,200,210,255), 0.35f);
            dl->AddText(ImVec2(starPos.x, starPos.y), starCol, g_ui.favorites[i] ? "\x97" : "\x98");

            // name label
            ImVec2 namePos(cardMin.x + cardPad, cardMax.y - 22);
            dl->AddText(namePos, IM_COL32(220, 220, 230, 255), g_Themes[i].name);

            // apply on click
            if (hovered && ImGui::IsMouseDoubleClicked(0)) {
                SetTheme(i);
                AddLog("[Theme] %s applied.", g_Themes[i].name);
            } else if (hovered && ImGui::IsMouseClicked(0)) {
                lastHovered = i;
            }

            visibleCount++;
        }
        ImGui::PopStyleVar();
    }
    ImGui::EndChild();

    // right: customization panel
    ImGui::SameLine();
    ImGui::BeginChild("##cust", ImVec2(sideW, 0), true);
    {
        ImGui::SeparatorText("Customize theme");
        ImVec4 acc = g_palCur.accent;
        ImGui::PushStyleColor(ImGuiCol_CheckMark, acc);
        ImGui::Checkbox("Particles",    &g_ui.particles);
        ImGui::Checkbox("Glow rings",   &g_ui.glow);
        ImGui::Checkbox("Rainbow accent", &g_ui.rainbow);
        ImGui::Checkbox("Performance mode", &g_ui.perfMode);
        ImGui::Checkbox("Sound effects", &g_ui.soundFx);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::SliderFloat("Amount",      &g_ui.particleAmount, 0.f, 3.f, "%.1fx");
        ImGui::SliderFloat("Speed",       &g_ui.animSpeed,  0.1f, 3.f, "%.1fx");
        ImGui::SliderFloat("BG opacity",  &g_ui.bgOpacity,  0.f, 0.85f, "%.0f%%");
        ImGui::SliderFloat("Blur",        &g_ui.blurStrength, 0.f, 1.f);

        ImGui::Spacing();
        ImGui::SeparatorText("Font size");
        static const char* fontSizes[] = {"14px","15px","16px","17px","18px","19px","20px","21px","22px"};
        int fsIdx = g_ui.fontSize - 14; if (fsIdx < 0 || fsIdx > 8) fsIdx = 4;
        if (ImGui::Combo("UI size", &fsIdx, fontSizes, 9)) { g_ui.fontSize = fsIdx + 14; }

        ImGui::Spacing();
        ImGui::SeparatorText("Custom accent colour");
        ImGui::Checkbox("Enable##accent", &g_ui.useCustomAccent);
        ImGui::ColorEdit3("Accent", g_ui.customAccent, ImGuiColorEditFlags_NoInputs);

        ImGui::Spacing();
        ImGui::SeparatorText("Custom background");
        ImGui::Checkbox("Enable##bg", &g_ui.useCustomBg);
        ImGui::ColorEdit3("Background", g_ui.customBg, ImGuiColorEditFlags_NoInputs);

        ImGui::Spacing();
        ImGui::SeparatorText("Persistence");
        if (ImGui::Button("Save now", ImVec2(-1, 28))) {
            SaveUISettings();
            Toast("Settings saved");
            AddLog("[Settings] saved to disk.");
        }
        if (ImGui::Button("Reset all", ImVec2(-1, 28))) {
            defaults();
            SetTheme(0);
            Toast("Defaults restored");
            AddLog("[Settings] reset to defaults.");
        }
    }
    ImGui::EndChild();
}
// ================================================================= SETTINGS
static void Page_Settings(){
    float half = (ImGui::GetContentRegionAvail().x-10)/2;
    ImGui::BeginChild("##settingsL", ImVec2(half, 0), true);
    ImGui::SeparatorText("Visuals");
    ImGui::SliderFloat("UI scale",    &g_ui.uiScale,    0.75f, 1.4f, "%.2f");
    ImGui::SliderFloat("Anim speed",  &g_ui.animSpeed,  0.1f,  3.f,  "%.1fx");
    ImGui::SliderFloat("BG opacity",  &g_ui.bgOpacity,  0.f,   0.85f);
    ImGui::SliderFloat("Blur",        &g_ui.blurStrength, 0.f,  1.f);
    ImGui::SliderFloat("Particles",   &g_ui.particleAmount, 0.f, 3.f, "%.1fx");
    ImGui::Checkbox("Particles",   &g_ui.particles);
    ImGui::Checkbox("Glow rings",  &g_ui.glow);
    ImGui::Checkbox("Rainbow",     &g_ui.rainbow);
    ImGui::Checkbox("Perf mode",   &g_ui.perfMode);
    ImGui::Checkbox("Sound fx",    &g_ui.soundFx);
    ImGui::Spacing();
    ImGui::SeparatorText("Theme persistence");
    if (ImGui::Button("Save now##set", ImVec2(-1, 28))) { SaveUISettings(); Toast("Settings saved"); }
    if (ImGui::Button("Reset defaults", ImVec2(-1, 28))) { defaults(); SetTheme(0); Toast("Defaults restored"); }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##settingsR", ImVec2(0, 0), true);
    ImGui::SeparatorText("Diagnostics");
    ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);
    ImGui::Text("Particles: %d", g_fn);
    ImGui::Text("Active: %d", g_ui.particles ? g_fn : 0);
    ImGui::Spacing();
    ImGui::SeparatorText("Logs");
    {
        ImGui::BeginChild("##logview", ImVec2(0, 120), true);
        for (int i = (int)logEntries.size()-1; i >= 0 && i >= (int)logEntries.size()-20; --i) {
            ImGui::TextColored(logEntries[i].prefixColor, "%s ", logEntries[i].prefix.c_str());
            ImGui::SameLine(0,2);
            ImGui::TextDisabled("%s", logEntries[i].message.c_str());
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
}
// =================================================================== ABOUT
static void Page_About(ImTextureID logo, float wallT){
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float cx = avail.x * 0.5f;
    ImGui::SetCursorPos(ImVec2(cx - 48, 12));
    if (logo) {
        float pulse = 92 + 4.0f * sinf(wallT * 1.8f);
        ImGui::Image(logo, ImVec2(pulse, pulse));
    }
    ImGui::SetCursorPosX(cx - ImGui::CalcTextSize("SpidermanHook").x * 0.5f);
    ImGui::PushFont(fBig);
    ImGui::TextColored(g_palCur.accent, "SpidermanHook");
    ImGui::PopFont();
    ImGui::SetCursorPosX(cx - ImGui::CalcTextSize("by zion btw / blasphemyw").x * 0.5f);
    ImGui::Text("by zion btw / blasphemyw");
    ImGui::Spacing();
    ImGui::SetCursorPosX(cx - 100);
    ImGui::TextDisabled("v0.2  |  DX11");
    ImGui::Spacing();
    ImGui::SetCursorPosX(cx - 140);
    ImGui::BulletText("Voice hook engine — discord_voice opus encode/decode");
    ImGui::SetCursorPosX(cx - 140);
    ImGui::BulletText("VST plugin host — third-party effect processing");
    ImGui::SetCursorPosX(cx - 140);
    ImGui::BulletText("Live analysis — spectrum, EQ, reverb, clarity graphs");
    ImGui::SetCursorPosX(cx - 140);
    ImGui::BulletText("ImGui 1.91.6 WIP  |  DX11 renderer  |  MinHook");
    ImGui::SetCursorPosX(cx - 140);
    ImGui::BulletText("14 themed animated overlays with real-time FX");
    ImGui::Spacing(); ImGui::Spacing();
    ImGui::SetCursorPosX(cx - 100);
    ImGui::TextDisabled("Built with love and persistence.");
}

static void DrawSidebar(int fx, float t, float dt, ImVec2 pos, ImVec2 size, ImVec4 accent){
    ImGui::SetCursorScreenPos(pos);
    ImGui::BeginChild("##sidebar", size, true, ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBackground);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();

    // background
    dl->AddRectFilled(wp, ImVec2(wp.x+ws.x, wp.y+ws.y), spCol(g_palCur.bg, 0.92f), 0);

    // spider logo
    if (g_SpiderManTexture) {
        float lsz = 40;
        dl->AddImage((ImTextureID)g_SpiderManTexture, ImVec2(wp.x+ws.x*0.5f-lsz*0.5f, wp.y+12),
            ImVec2(wp.x+ws.x*0.5f+lsz*0.5f, wp.y+12+lsz));
    }
    // brand text
    dl->AddText(ImVec2(wp.x+8, wp.y+58), spCol(accent), "Spiderman");
    dl->AddText(ImVec2(wp.x+8, wp.y+74), IM_COL32(160,160,170,200), "Hook");

    float y = wp.y + 108;
    float itemH = 32;
    for (int i = 0; i < TAB_COUNT; ++i) {
        bool active = (g_ui.activeTab == i);
        ImVec2 itemMin(wp.x+4, y);
        ImVec2 itemMax(wp.x+ws.x-4, y+itemH);

        bool hovered = ImGui::IsMouseHoveringRect(itemMin, itemMax);

        if (active) {
            dl->AddRectFilled(itemMin, itemMax, spFadeAlpha(spCol(accent), 0.12f), 6.f);
            dl->AddRectFilled(ImVec2(wp.x, itemMin.y+4), ImVec2(wp.x+3, itemMax.y-4), spCol(accent), 2.f);
        } else if (hovered) {
            dl->AddRectFilled(itemMin, itemMax, IM_COL32(255,255,255,12), 6.f);
        }

        ImVec2 iconCenter(wp.x+22, y+itemH*0.5f);
        NavIcon(dl, i, iconCenter, 7, active ? spCol(accent) : IM_COL32(150,150,160,180));

        ImVec2 labelPos(wp.x+38, y+itemH*0.5f-6);
        dl->AddText(labelPos, active ? IM_COL32(240,240,250,255) : IM_COL32(150,150,160,200), TabLabel(i));

        // click
        if (hovered && ImGui::IsMouseClicked(0)) g_ui.activeTab = i;

        y += itemH + 2;
    }

    // footer
    float fy = wp.y + ws.y - 24;
    float pulse = 0.5f + 0.5f * sinf(t * 2.0f);
    dl->AddCircleFilled(ImVec2(wp.x+16, fy+8), 3.5f, spFadeAlpha(IM_COL32(60,220,90,255), pulse), 8);
    dl->AddText(ImVec2(wp.x+26, fy), IM_COL32(130,130,140,160), "v0.2");

    ImGui::EndChild();
}

// ========================================================================
//  ImGui::ui::start  —  DX11 sidebar-based shell
// ========================================================================
void ImGui::ui::start() {
    WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L,
        GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"SpidermanHook", NULL};
    ::RegisterClassExW(&wc);
    g_Window = ::CreateWindow(wc.lpszClassName, L"SpidermanHook",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        100, 100, 1060, 700, NULL, NULL, wc.hInstance, NULL);

    // --- DX11 init ---
    if (CreateDeviceD3D(g_Window) < 0) { CleanupDeviceD3D(); ::UnregisterClass(wc.lpszClassName, wc.hInstance); return; }

    // --- ImGui init (must be before ShowWindow to avoid WndProc crashes) ---
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_Window);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    UpdateAndApplyStyle(0.f, 0.f);

    // Win11 rounded corners (best-effort)
    DWORD cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(g_Window, 33, &cornerPref, sizeof(cornerPref));

    ::ShowWindow(g_Window, SW_SHOWDEFAULT);
    ::UpdateWindow(g_Window);

    // --- System fonts (UTF-8 paths for ImGui) ---
    auto TryLoad = [&](const char* path, float px){
        ImFont* f = io.Fonts->AddFontFromFileTTF(path, px);
        if (f && f->IsLoaded()) return f;
        return (ImFont*)nullptr;
    };
    float basePx = (float)g_ui.fontSize;
    const char* fontPaths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/consola.ttf"
    };
    const char* boldPaths[] = {
        "C:/Windows/Fonts/seguisb.ttf",
        "C:/Windows/Fonts/tahomabd.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "C:/Windows/Fonts/calibrib.ttf",
        "C:/Windows/Fonts/consolab.ttf"
    };
    fReg = nullptr;
    for (int i = 0; i < 5 && !fReg; ++i) fReg = TryLoad(fontPaths[i], basePx);
    if (!fReg) fReg = io.Fonts->AddFontDefault();
    fBig = nullptr;
    for (int i = 0; i < 5 && !fBig; ++i) fBig = TryLoad(boldPaths[i], basePx * 1.55f);
    if (!fBig) fBig = fReg;
    fSmall = nullptr;
    for (int i = 0; i < 5 && !fSmall; ++i) fSmall = TryLoad(fontPaths[i], basePx * 0.72f);
    if (!fSmall) fSmall = fReg;

    // --- Spider texture ---
    LoadSpiderManTexture(g_pd3dDevice);
    UpdateAndApplyStyle(0.f, 0.f);

    // --- Visibility (F5 toggle) ---
    static bool guiVisible = true;
    static bool prevF5     = false;
    auto ApplyVis = [&](bool show){
        guiVisible = show;
        ShowWindow(g_Window, show ? SW_SHOW : SW_HIDE);
        guiVisible = show;
    };

    // --- Timing ---
    auto prev_tp   = std::chrono::steady_clock::now();
    float wallT    = 0.f;
    double uptime  = 0.0;
    float fadeIn   = 0.f;

    // --- Loading screen ---
    bool   booting   = true;
    double bootTime  = 0.0;
    float  progress  = 0.f;
    float  bootFade  = 1.f;
    const  double BOOT = 2.4; // seconds

    // --- Main frame loop ---
    static bool running = true;
    while (running) {
        // F5 visibility toggle
        bool nowF5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
        if (nowF5 && !prevF5) ApplyVis(!guiVisible);
        prevF5 = nowF5;
        if (!guiVisible) { Sleep(80); continue; }

        // Frame timing
        auto now_tp = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now_tp - prev_tp).count();
        prev_tp = now_tp;
        if (dt > 0.05f) dt = 0.05f;
        wallT  += dt;
        uptime += dt;

        // Handle device lost
        HRESULT hr = g_pSwapChain->Present(1, 0);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            HandleDeviceLost(g_Window); continue;
        }
        if (hr == DXGI_ERROR_DEVICE_HUNG) { Sleep(8); continue; }

        // DX11 frame setup
        const float clear_col[4] = {0,0,0,0};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_col);
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Update theme palette (smooth transition)
        UpdateAndApplyStyle(dt, wallT);

        // === LOADING SCREEN / MAIN UI ===
        if (booting) {
            progress = spClampf((float)(bootTime / BOOT), 0.f, 1.f);
            bootTime += dt;
            if (progress >= 1.0) {
                booting = false;
            } else {
                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(io.DisplaySize);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
                ImGui::PushStyleColor(ImGuiCol_WindowBg, spCol(g_palCur.bg, g_ui.bgOpacity));
                ImGui::Begin("##loading", NULL,
                    ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|
                    ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoNav|
                    ImGuiWindowFlags_NoInputs);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 cp(io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f);

                float pulse = 0.5f + 0.5f * sinf(wallT * 2.8f);
                if (g_ui.glow) for (int r = 4; r >= 1; --r)
                    dl->AddCircle(cp, 55.f + r*14.f*pulse, spFadeAlpha(spCol(g_palCur.accent), 0.045f*r), 40, 2.5f);

                if (g_SpiderManTexture) {
                    float logoSz = 96.f + 6.f * sinf(wallT * 1.5f);
                    dl->AddImage((ImTextureID)g_SpiderManTexture,
                        ImVec2(cp.x - logoSz*0.5f, cp.y - logoSz*0.5f - 50),
                        ImVec2(cp.x + logoSz*0.5f, cp.y + logoSz*0.5f - 50),
                        ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,(int)(255*pulse)));
                }

                float titleAlpha = spClampf(progress * 1.4f, 0.f, 1.f);
                ImGui::SetCursorScreenPos(ImVec2(cp.x - 90, cp.y + 70));
                ImGui::PushFont(fBig);
                ImGui::TextColored(Dimmed(g_palCur.accent, titleAlpha), "SpidermanHook");
                ImGui::PopFont();
                ImGui::SetCursorScreenPos(ImVec2(cp.x - 70, cp.y + 108));
                ImGui::TextDisabled("%s", "by zion btw / blasphemyw");

                float barW = 260.f, barH = 6.f;
                ImVec2 barMin(cp.x - barW*0.5f, cp.y + 142);
                ImVec2 barMax(cp.x + barW*0.5f, cp.y + 142 + barH);
                dl->AddRectFilled(barMin, barMax, spFadeAlpha(IM_COL32(200,200,210,255), 0.12f), 3.f);
                dl->AddRectFilled(barMin, ImVec2(barMin.x + barW * progress, barMax.y),
                    spCol(g_palCur.accent), 3.f);

                static const char* steps[] = {"Initializing DX11...", "Loading fonts...",
                    "Installing voice hooks...", "Loading spider texture...",
                    "Preparing overlay...", "Done"};
                int stepIdx = (int)(progress * 5.f); if (stepIdx > 5) stepIdx = 5;
                float stepW = ImGui::CalcTextSize(steps[stepIdx]).x;
                ImGui::SetCursorScreenPos(ImVec2(cp.x - stepW*0.5f, cp.y + 160));
                ImGui::TextDisabled("%s", steps[stepIdx]);

                ImGui::End();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
            }
        }
        // === MAIN UI (after loading) ===
        if (!booting) {
            fadeIn += dt / 0.45f;
            if (fadeIn > 1.f) fadeIn = 1.f;
            {
                float sideW = 196.f;
                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(io.DisplaySize);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, spCol(g_palCur.bg, g_ui.bgOpacity));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, spCol(g_palCur.panel, 0.f));
                ImGui::PushStyleColor(ImGuiCol_Text,   spCol(g_palCur.text, 1.f));

                ImGui::Begin("##main", NULL,
                    ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|
                    ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoSavedSettings|
                    ImGuiWindowFlags_NoBringToFrontOnFocus);

                float prevAlpha = ImGui::GetStyle().Alpha;
                ImGui::GetStyle().Alpha = prevAlpha * fadeIn;

                ImVec2 dsMin(0,0), dsMax(io.DisplaySize.x, 28);
                ImGui::SetCursorScreenPos(dsMin);
                ImGui::InvisibleButton("##dragstrip", ImVec2(dsMax.x, dsMax.y));
                if (ImGui::IsItemActive()) {
                    POINT p; GetCursorPos(&p);
                    RECT wr; GetWindowRect(g_Window, &wr);
                    MoveWindow(g_Window, wr.left + p.x - (wr.left+wr.right)/2,
                        wr.top + p.y - 14, wr.right-wr.left, wr.bottom-wr.top, TRUE);
                }

                ImVec2 bsz(io.DisplaySize.x, io.DisplaySize.y - 28);
                ImVec2 bpos(0, 28);
                ImGui::SetCursorScreenPos(bpos);
                ImDrawList* dl = ImGui::GetWindowDrawList();
                FxDrawBackdrop(dl, bpos, bsz, wallT, dt);

                DrawSidebar(g_Themes[g_ui.themeIndex].fx, wallT, dt,
                    ImVec2(0, 30), ImVec2(sideW, io.DisplaySize.y - 30), g_palCur.accent);

                {
                    ImVec2 hmin(sideW + 14, 30);
                    float titleY = hmin.y + 4;
                    ImGui::SetCursorScreenPos(ImVec2(hmin.x, titleY));
                    ImGui::PushFont(fBig);
    ImGui::TextColored(g_palCur.accent, "SpidermanHook");
                    ImGui::PopFont();
                    ImGui::SetCursorScreenPos(ImVec2(hmin.x, titleY + 32));
                    ImGui::TextDisabled("by zion btw / blasphemyw");
                }

                {
                    ImVec2 hbMin(io.DisplaySize.x - 120, 2);
                    ImGui::SetCursorScreenPos(hbMin);
                    if (ImGui::Button("Hide", ImVec2(50, 24))) { ApplyVis(false); }
                }

                {
                    float contentX = sideW + 14, contentY = 80;
                    float contentW = io.DisplaySize.x - contentX - 8;
                    float contentH = io.DisplaySize.y - contentY - 8;
                    ImGui::SetCursorScreenPos(ImVec2(contentX, contentY));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);
                    ImGui::BeginChild("##content", ImVec2(contentW, contentH), true,
                        ImGuiWindowFlags_NoScrollbar);

                    static int prevTab = TAB_HOME;
                    static float tabAnim = 0.f;
                    if (prevTab != g_ui.activeTab) { tabAnim = 0.f; prevTab = g_ui.activeTab; }
                    tabAnim += dt * 4.0f;
                    float ease = 1.f - (1.f - spClampf(tabAnim, 0.f, 1.f));
                    float slideX = (1.f - ease) * 18.f;
                    float childFade = fadeIn * ease;
                    ImVec2 childPos = ImGui::GetCursorScreenPos();
                    ImGui::SetCursorScreenPos(ImVec2(childPos.x + slideX, childPos.y));
                    ImGui::GetStyle().Alpha *= childFade;

                    switch (g_ui.activeTab) {
                    case TAB_HOME:       Page_Home((ImTextureID)g_SpiderManTexture, uptime, wallT); break;
                    case TAB_AUDIO:      Page_Audio(); break;
                    case TAB_OPUS:       Page_Opus(wallT); break;
                    case TAB_DECODER:    Page_Decoder(); break;
                    case TAB_WEBRTC:     Page_WebRTC(wallT); break;
                    case TAB_SOUNDPAD:   Page_Soundpad(wallT); break;
                    case TAB_VST:        Page_VST(); break;
                    case TAB_VISUALIZER: Page_Visualizer(wallT); break;
                    case TAB_THEMES:     Page_Themes(wallT, dt); break;
                    case TAB_SETTINGS:   Page_Settings(); break;
                    case TAB_ABOUT:      Page_About((ImTextureID)g_SpiderManTexture, wallT); break;
                    }

                    ImGui::GetStyle().Alpha = childFade;
                    ImGui::EndChild();
                    ImGui::PopStyleVar(2);
                }

                ImGui::End();
                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar(2);
                ImGui::GetStyle().Alpha = prevAlpha;
            }
            // toast
            if (g_toastTime > 0.0) {
                g_toastTime -= dt;
                float a = spClampf((float)(g_toastTime / 0.45), 0.f, 1.f);
                ImVec2 sz = ImGui::CalcTextSize(g_toastBuf);
                ImVec2 tp(io.DisplaySize.x - sz.x - 30, io.DisplaySize.y - 50);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(tp.x - 10, tp.y - 6), ImVec2(tp.x + sz.x + 10, tp.y + sz.y + 6),
                    spFadeAlpha(IM_COL32(30,30,40,220), a), 6.f);
                ImGui::GetWindowDrawList()->AddText(tp, spFadeAlpha(IM_COL32(240,240,250,255), a), g_toastBuf);
            }
            ImGui::GetStyle().Alpha = 1.f;
        }
        // present
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    } // end while

    // Cleanup
    running = false;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_SpiderManTexture) g_SpiderManTexture->Release();
    g_mainRenderTargetView->Release();
    g_pSwapChain->Release();
    g_pd3dDeviceContext->Release();
    g_pd3dDevice->Release();
    ::DestroyWindow(g_Window);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
}
bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.OutputWindow = hWnd;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = 0;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2,
            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer == nullptr)
        return;
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void HandleDeviceLost(HWND hWnd)
{
    ReleaseSpiderManTexture();
    CleanupRenderTarget();
    ImGui_ImplDX11_InvalidateDeviceObjects();
    CleanupDeviceD3D();
    CreateDeviceD3D(hWnd);
    LoadSpiderManTexture(g_pd3dDevice);
    ImGui_ImplDX11_CreateDeviceObjects();
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void ResetDevice(int width, int height)
{
    ReleaseSpiderManTexture();
    CleanupRenderTarget();
    ImGui_ImplDX11_InvalidateDeviceObjects();

    g_pSwapChain->ResizeBuffers(0, (UINT)width, (UINT)height, DXGI_FORMAT_UNKNOWN, 0);

    CreateRenderTarget();
    LoadSpiderManTexture(g_pd3dDevice);
    ImGui_ImplDX11_CreateDeviceObjects();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        if (g_pd3dDevice != nullptr && lParam != SIZE_MINIMIZED) {
            ResetDevice(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;

    case WM_CREATE:
        break;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
