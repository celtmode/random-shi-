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
#include "imgui/spiderman_texture.h"
#include "spiderman.h"

// globals (one definition in one .cpp)
ID3D11ShaderResourceView* g_SpiderManTexture = nullptr;

// call this AFTER your D3D device (g_pd3dDevice) is created
bool LoadSpiderManTexture(ID3D11Device* device)
{
    if (!device) return false;
    if (g_SpiderManTexture) return true;

    bool initializedCom = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) goto cleanup;

    hr = factory->CreateDecoderFromMemory(spiderman_png, (DWORD)spiderman_png_len, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) goto cleanup;

    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) goto cleanup;

    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) goto cleanup;

    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) goto cleanup;

    UINT width = 0, height = 0;
    converter->GetSize(&width, &height);
    const UINT stride = width * 4;
    const UINT size = stride * height;
    std::vector<unsigned char> pixels(size);
    hr = converter->CopyPixels(nullptr, stride, size, pixels.data());
    if (FAILED(hr)) goto cleanup;

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
    if (FAILED(hr)) goto cleanup;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(texture, &srvDesc, &g_SpiderManTexture);

cleanup:
    if (texture) texture->Release();
    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    if (initializedCom) CoUninitialize();

    if (FAILED(hr))
    {
        char buf[256];
        sprintf(buf, "Spider-Man texture load failed. HRESULT = 0x%08X (len=%u)", hr, spiderman_png_len);
        MessageBoxA(NULL, buf, "SpiderManHook Error", MB_ICONERROR);
        return false;
    }
    return true;
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




void ImGui::ui::start() {
    if (!g_vstHost) {
        g_vstHost = std::make_unique<VSTHost>();
    }
    
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"SpiderManHook", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"SpiderManHook", WS_POPUP, 100, 100, 788, 650, nullptr, nullptr, wc.hInstance, nullptr);
    g_Window = hwnd;

    int windowWidth = 958;


    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);
    SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
    SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoe.ttf", 18.0f);

    ImVec4 clear_color = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    LoadSpiderManTexture(g_pd3dDevice);


    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        InitLua();
        RenderSafeParticles();

        RECT Rect;
        GetClientRect(hwnd, &Rect);

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2((float)(Rect.right - Rect.left), (float)(Rect.bottom - Rect.top)), ImGuiCond_Always);



        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::StyleColorsDark();

        // --- Rounding ---
        style.WindowRounding = 2;
        style.ChildRounding = 2;
        style.FrameRounding = 2;
        style.PopupRounding = 2;
        style.ScrollbarRounding = 2;
        style.TabRounding = 2;

        // --- Alignment ---
        style.ButtonTextAlign = { 0.5f, 0.5f };
        style.WindowTitleAlign = { 0.5f, 0.5f };

        // --- Padding & Spacing ---
        style.FramePadding = { 5.0f, 3.0f };
        style.WindowPadding = { 6.0f, 6.0f };
        style.ItemSpacing = { 6.0f, 4.0f };
        style.ItemInnerSpacing = { 4.0f, 4.0f };

        // --- Borders & sizes ---
        style.WindowBorderSize = 1;
        style.FrameBorderSize = 1;
        style.PopupBorderSize = 1;
        style.ScrollbarSize = 6.f;
        style.GrabMinSize = 6;

        // --- Colors ---
        ImVec4 bgBlack = HexToColorVec4(0x000000); // absolute black
        ImVec4 bgDark = HexToColorVec4(0x0A0A0A); // slightly lighter than black
        ImVec4 bgHover = HexToColorVec4(0x1A1A1A); // hover highlight
        ImVec4 accentGray = HexToColorVec4(0x333333); // borders, subtle accents
        ImVec4 textColor = HexToColorVec4(0xE0E0E0); // light gray text
        ImVec4 textDim = HexToColorVec4(0x666666); // disabled text

        style.Colors[ImGuiCol_Text] = textColor;
        style.Colors[ImGuiCol_TextDisabled] = textDim;
        style.Colors[ImGuiCol_WindowBg] = bgBlack;
        style.Colors[ImGuiCol_ChildBg] = bgDark;
        style.Colors[ImGuiCol_PopupBg] = bgDark;
        style.Colors[ImGuiCol_Border] = accentGray;
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
        style.Colors[ImGuiCol_FrameBg] = bgDark;
        style.Colors[ImGuiCol_FrameBgHovered] = bgHover;
        style.Colors[ImGuiCol_FrameBgActive] = bgHover;
        style.Colors[ImGuiCol_TitleBg] = bgDark;
        style.Colors[ImGuiCol_TitleBgActive] = bgHover;
        style.Colors[ImGuiCol_TitleBgCollapsed] = bgDark;
        style.Colors[ImGuiCol_MenuBarBg] = bgBlack;
        style.Colors[ImGuiCol_ScrollbarBg] = bgDark;
        style.Colors[ImGuiCol_ScrollbarGrab] = accentGray;
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = bgHover;
        style.Colors[ImGuiCol_ScrollbarGrabActive] = bgHover;
        style.Colors[ImGuiCol_CheckMark] = textColor;
        style.Colors[ImGuiCol_SliderGrab] = accentGray;
        style.Colors[ImGuiCol_SliderGrabActive] = bgHover;
        style.Colors[ImGuiCol_Button] = bgDark;
        style.Colors[ImGuiCol_ButtonHovered] = bgHover;
        style.Colors[ImGuiCol_ButtonActive] = bgHover;
        style.Colors[ImGuiCol_Header] = bgDark;
        style.Colors[ImGuiCol_HeaderHovered] = bgHover;
        style.Colors[ImGuiCol_HeaderActive] = bgHover;
        style.Colors[ImGuiCol_Separator] = accentGray;
        style.Colors[ImGuiCol_SeparatorHovered] = bgHover;
        style.Colors[ImGuiCol_SeparatorActive] = bgHover;
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
        style.Colors[ImGuiCol_ResizeGripHovered] = accentGray;
        style.Colors[ImGuiCol_ResizeGripActive] = bgHover;
        style.Colors[ImGuiCol_Tab] = bgDark;
        style.Colors[ImGuiCol_TabHovered] = bgHover;
        style.Colors[ImGuiCol_TabActive] = bgHover;
        style.Colors[ImGuiCol_TabUnfocused] = bgDark;
        style.Colors[ImGuiCol_TabUnfocusedActive] = bgHover;
        style.Colors[ImGuiCol_PlotLines] = textColor;
        style.Colors[ImGuiCol_PlotLinesHovered] = textColor;
        style.Colors[ImGuiCol_PlotHistogram] = textColor;
        style.Colors[ImGuiCol_PlotHistogramHovered] = textColor;
        style.Colors[ImGuiCol_TextSelectedBg] = bgHover;
        style.Colors[ImGuiCol_ModalWindowDimBg] = HexToColorVec4(0x000000, 0.7f);
        style.Colors[ImGuiCol_DragDropTarget] = bgHover;
        style.Colors[ImGuiCol_NavHighlight] = bgHover;
        style.Colors[ImGuiCol_NavWindowingHighlight] = HexToColorVec4(0xFFFFFF, 0.1f);




        ImGui::Begin(" ", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoScrollbar);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 win_pos = ImGui::GetWindowPos();
        ImVec2 win_size = ImGui::GetWindowSize();
        ImVec2 p0 = win_pos;
        ImVec2 p1 = ImVec2(win_pos.x + win_size.x, win_pos.y + win_size.y);

        // === Dragging Logic ===
        static bool isDragging = false;
        static POINT dragOffset{};

        ImVec2 dragBarSize = ImVec2(ImGui::GetWindowWidth(), 30);
        ImVec2 cursorStart = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton("##draggable", dragBarSize);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            if (!isDragging) {
                isDragging = true;
                POINT cursor;
                GetCursorPos(&cursor);
                RECT rect;
                GetWindowRect(g_Window, &rect);
                dragOffset.x = cursor.x - rect.left;
                dragOffset.y = cursor.y - rect.top;
            }
            else {
                POINT cursor;
                GetCursorPos(&cursor);
                SetWindowPos(g_Window, nullptr,
                    cursor.x - dragOffset.x,
                    cursor.y - dragOffset.y,
                    0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
        }
        else if (!ImGui::IsMouseDown(0)) {
            isDragging = false;
        }

        // === Title / Version Text ===
        ImVec2 barMin = p0;
        ImVec2 barMax = ImVec2(p1.x, p0.y + dragBarSize.y);

        // title left (precise centering)
        ImVec2 cursor = ImVec2(barMin.x + 8, barMin.y + 7);
        ImGui::SetCursorScreenPos(cursor);

        const float imageSize = 20.0f;
        const float textHeight = ImGui::GetFont()->FontSize; // font height
        const float desiredY = cursor.y + (imageSize - textHeight) * 0.5f; // center text to image

        if (g_SpiderManTexture)
        {
            ImGui::Image((ImTextureID)(intptr_t)g_SpiderManTexture, ImVec2(imageSize, imageSize));
            ImGui::SameLine();
        }
        // move cursor to vertically centered text baseline
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, desiredY));
        ImGui::TextUnformatted("SpiderManHook - by zion btw / blasphemyw");

        // version right
        std::string version = "v0.1";
        ImVec2 textSize = ImGui::CalcTextSize(version.c_str());
        ImGui::SetCursorScreenPos(ImVec2(barMax.x - textSize.x - 10, barMin.y + 7));
        ImGui::TextUnformatted(version.c_str());

        // Reset cursor
        ImGui::SetCursorScreenPos(ImVec2(cursorStart.x, cursorStart.y + dragBarSize.y));
        ImGui::Separator();

        if (ImGui::BeginTabBar("MainTabBar")) {

            // === ENCODER TAB ===
            if (ImGui::BeginTabItem("Encoder")) {
                ImVec2 fullSize = ImGui::GetContentRegionAvail();
                ImVec2 halfSize = ImVec2(fullSize.x * 0.5f, fullSize.y * 0.5f);

                // ---------------- PARENT 1 ----------------
                ImGui::BeginChild("Parent1", ImVec2(fullSize.x, halfSize.y), true);

                // --- Top Left: Gain and Core FX ---
                ImGui::BeginChild("TopLeft", halfSize, true);
                ImGui::SeparatorText("Gain, Reverb");
                ImGui::SliderFloat("dB", &Gain, 1.0f, 120.0f, "%.3f");
                ImGui::SliderFloat("Exploit dB", &ExpGain, 1.0f, 120.0f, "%.3f");
                ImGui::SliderFloat("Bitrate", &Bitrate, 40000.0f, 510000.0f, "%.0f");

                ImGui::Separator();

                ImGui::SliderFloat("Reverb Size", &reverbSize, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Damping", &reverbDamping, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Width", &reverbWidth, 0.0f, 2.0f, "%.2f");
                ImGui::SliderFloat("Pre-delay", &reverbPredelay, 0.0f, 0.1f, "%.3f");
                ImGui::SliderFloat("Makeup Gain", &reverbMakeupGain, 0.0f, 2.0f, "%.2f");
                ImGui::SliderFloat("Amount", &ReverbAmount, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Decay", &ReverbDecay, 0.0f, 1.0f, "%.2f");

                ImGui::Checkbox("Expander", &apply_expander);
                ImGui::Checkbox("Apply Expander", &apply_expander);
                ImGui::EndChild();


                ImGui::SameLine();

                // --- Top Right: Extra Main Controls ---
                ImGui::BeginChild("TopRight", halfSize, true);
                ImGui::SeparatorText("Clarity + Clarity Enhancer");
                ImGui::Checkbox("Clarity Enhancer", &apply_clarity_enhancer);
                ImGui::SliderFloat("Clarity Q", &clarity_q, 0.1f, 5.0f);
                ImGui::SliderFloat("Clarity Freq", &clarity_freq, 200.0f, 8000.0f);
                ImGui::SliderFloat("Clarity Gain", &clarity_gain, -20.0f, 20.0f);
                ImGui::SliderFloat("Clarity Mix", &clarity_mix, 0.0f, 1.0f);
                ImGui::EndChild();

                ImGui::EndChild(); // ---- END PARENT 1 ----


                // ---------------- PARENT 2 ----------------
                ImGui::BeginChild("Parent2", ImVec2(fullSize.x, halfSize.y), true);

                // --- Bottom Left: Reserved / Reverb Visualization ---
                ImGui::BeginChild("BottomLeft", halfSize, true);
                ImGui::SeparatorText("Graphs Etc");

                ImGui::Checkbox("Show Reverb Visualization", &showReverbVisualization);

                static bool showEqGraph = false;  // <-- Add this
                ImGui::Checkbox("Show EQ Graph", &showEqGraph);

                if (showReverbVisualization) {
                    DrawReverbVisualization(
                        reverbSize,
                        ReverbDecay,
                        reverbDamping,
                        reverbWidth,
                        ReverbAmount,
                        reverbPredelay,
                        reverbMakeupGain,
                        halfSize.y,   // use available height
                        2.0f          // line width
                    );
                }

                static bool show_clarity_graph = false; // default off
                ImGui::Checkbox("Show Clarity Graph", &show_clarity_graph);

                if (show_clarity_graph) {
                    DrawClarityGraph(clarity_q, clarity_freq, clarity_gain, clarity_mix, 160.0f);
                }

                if (showEqGraph) {
                    DrawCoolEQVisualization(eq_low_gain, eq_mid_gain, eq_high_gain, halfSize.y, 2.0f);
                }

                // === NEW: Self-checker graph ===
                static bool showSelfGraph = false;
                ImGui::Checkbox("Show Self dB/DC Graph", &showSelfGraph);

                if (showSelfGraph) {
                    std::lock_guard<std::mutex> lock(self_graph_mutex);
                    if (!self_graph_data.empty()) {
                        ImGui::PlotLines("SelfCheck DC Reject",
                            self_graph_data.data(),
                            (int)self_graph_data.size(),
                            0,
                            nullptr,
                            0.0f,
                            0.01f,
                            ImVec2(0, halfSize.y * 0.4f));
                    }
                }

                ImGui::EndChild();






                ImGui::SameLine();

                // --- Bottom Right: Effects Rack ---
                ImGui::BeginChild("BottomRight", halfSize, true);
                ImGui::SeparatorText("Effects Rack");

                // ==== Equalizer & Filters ====
                if (ImGui::TreeNode("Equalizer")) {
                    ImGui::Checkbox("Apply EQ", &apply_eq);
                    ImGui::SliderFloat("Low Gain", &eq_low_gain, -12.0f, 12.0f);
                    ImGui::SliderFloat("Mid Gain", &eq_mid_gain, -12.0f, 12.0f);
                    ImGui::SliderFloat("High Gain", &eq_high_gain, -12.0f, 12.0f);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Low-pass Filter")) {
                    ImGui::Checkbox("Enable", &apply_lowpass);
                    ImGui::SliderFloat("Frequency", &lowpass_freq, 500.0f, 20000.0f);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("High-pass Filter")) {
                    ImGui::Checkbox("Enable", &apply_highpass);
                    ImGui::SliderFloat("Frequency", &highpass_freq, 20.0f, 500.0f);
                    ImGui::TreePop();
                }

                // ==== Bit FX ====
                if (ImGui::TreeNode("Saturation")) {
                    ImGui::Checkbox("Enable", &apply_saturation);
                    ImGui::SliderFloat("Drive", &saturation_drive, 0.5f, 10.0f);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Bitcrusher")) {
                    ImGui::Checkbox("Enable", &apply_bitcrusher);
                    ImGui::SliderInt("Bits", &bitcrusher_bits, 4, 16);
                    ImGui::SliderFloat("Rate", &bitcrusher_rate, 0.01f, 1.0f);
                    ImGui::TreePop();
                }

                // ==== Dynamics & Modulation ====
                if (ImGui::TreeNode("Noise Gate")) {
                    ImGui::Checkbox("Enable", &apply_gate);
                    ImGui::SliderFloat("Threshold", &gate_threshold, 0.0f, 1.0f);
                    ImGui::SliderFloat("Attack", &gate_attack, 0.001f, 0.1f);
                    ImGui::SliderFloat("Release", &gate_release, 0.01f, 0.5f);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Chorus")) {
                    ImGui::Checkbox("Enable", &apply_chorus);
                    ImGui::SliderFloat("Rate", &chorus_rate, 0.1f, 5.0f);
                    ImGui::SliderFloat("Depth", &chorus_depth, 0.0f, 1.0f);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Flanger")) {
                    ImGui::Checkbox("Enable", &apply_flanger);
                    ImGui::SliderFloat("Rate", &flanger_rate, 0.05f, 2.0f);
                    ImGui::SliderFloat("Depth", &flanger_depth, 0.0f, 1.0f);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Tremolo")) {
                    ImGui::Checkbox("Enable", &apply_tremolo);
                    ImGui::SliderFloat("Rate", &tremolo_rate, 1.0f, 20.0f);
                    ImGui::SliderFloat("Depth", &tremolo_depth, 0.0f, 1.0f);
                    ImGui::TreePop();
                }

                // ==== Spatial, Compression ====
                if (ImGui::TreeNode("Stereo Enhancer")) {
                    ImGui::Checkbox("Enable", &apply_stereo_enhancer);
                    ImGui::SliderFloat("Width", &stereoWidth, 0.0f, 2.0f);
                    ImGui::SliderFloat("Pan", &pan, -1.0f, 1.0f);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Delay")) {
                    ImGui::Checkbox("Enable", &apply_delay);
                    ImGui::SliderFloat("Time", &delay_time, 0.05f, 1.0f);
                    ImGui::SliderFloat("Feedback", &delay_feedback, 0.0f, 1.0f);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Auto Gain")) {
                    ImGui::Checkbox("Enable", &apply_auto_gain);
                    ImGui::SliderFloat("Target Gain", &auto_gain_target, 0.1f, 1.5f);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Pitch Shift")) {
                    ImGui::Checkbox("Enable", &apply_pitch_shift);
                    ImGui::SliderFloat("Semitones", &pitch_semitones, -12.0f, 12.0f);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Invert Phase")) {
                    ImGui::Checkbox("Enable", &invert_phase);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Soft Clip")) {
                    ImGui::Checkbox("Enable", &apply_softclip);
                    ImGui::SliderFloat("Curve", &softclip_curve, 0.1f, 1.0f);
                    ImGui::TreePop();
                }

                // ==== Output ====
                if (ImGui::TreeNode("Output")) {
                    ImGui::SliderFloat("Gain", &outputGain, 0.0f, 2.0f);
                    ImGui::Checkbox("Mute", &muteOutput);
                    ImGui::SliderFloat("Dry/Wet Mix", &dryWetMix, 0.0f, 1.0f);
                    ImGui::Checkbox("Bypass All", &bypassAll);
                    ImGui::TreePop();
                }

                ImGui::EndChild(); // BottomRight
                ImGui::EndChild(); // ---- END PARENT 2 ----
                ImGui::EndTabItem();
            }








            if (ImGui::BeginTabItem("Decoder")) {
                ImGui::Separator();
                ImGui::Text("DB Checking");
                ImGui::Separator();

                ImGui::BeginChild("DecoderChild", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysUseWindowPadding);

                // --- Declare static options here so they are visible throughout the function ---
                static bool opt1 = false, opt2 = true, opt3 = false, opt4 = true;
                static bool opt5 = false, opt6 = false, opt7 = true, opt8 = false;
                static bool opt9 = false, opt10 = false;

                static float thresholdAdjust = 2.5f;
                static int qualityMode = 0;
                static float gain = 1.0f, sens = 0.5f;
                static int algoType = 1;
                static int repeatCount = 5;

                // ---------------------- Left Panel ----------------------
                ImGui::BeginChild("LeftPanel", ImVec2(ImGui::GetContentRegionAvail().x * 0.45f, 0), true);
                {
                    ImGui::Text("General Options");
                    ImGui::Separator();

                    ImGui::Checkbox("Client detection", &dbcheck);
                    ImGui::Checkbox("Int16 Check", &int16check);
                    ImGui::Checkbox("Self Check", &selfcheck);
                    ImGui::Checkbox("Force Not Natty", &forceNotNatty);
                    ImGui::InputText("Decoder Name", yexDecoderText, IM_ARRAYSIZE(yexDecoderText));

                    ImGui::Text("Spectrum Analyzer");
                    ImGui::Separator();

                    if (int16check) {
                        ImVec2 spectrumSize(ImGui::GetContentRegionAvail().x, 150);
                        ImVec2 spectrumPos = ImGui::GetCursorScreenPos();
                        ImDrawList* draw = ImGui::GetWindowDrawList();

                        DrawSpectrumAnalyzer(draw, spectrumPos, spectrumSize);

                        // reserve layout space
                        ImGui::Dummy(spectrumSize);
                    }



                    ImGui::Spacing();
                }
                ImGui::EndChild();

                ImGui::SameLine();

                // ---------------------- Right Panel ----------------------
                ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
                {
                    ImGui::Text("Decoder Stats");
                    ImGui::Separator();

                    static float highestPeak = -100.0f;
                    static int clientDetectionCount = 0;
                    static float lastSample = 0.0f;
                    static bool wasDetectedLastFrame = false;

                    // Pull latest sample
                    float rawSample = 0.0f;
                    {
                        std::lock_guard<std::mutex> lock(db_graph_mutex);
                        if (!db_graph_data.empty()) rawSample = db_graph_data.back();
                    }

                    // Apply extended option logic
                    if (opt6) rawSample = FakeSignal(rawSample);          // Fake signals
                    if (opt8) rawSample = SpectralFilter(rawSample);      // Spectral filter
                    if (opt7) rawSample = ReduceNoise(rawSample, lastSample, 0.2f); // Noise reduction

                    if (opt4) lastSample = Lerp(lastSample, rawSample, 0.35f); // High precision smoothing
                    else lastSample = rawSample;

                    if (lastSample > highestPeak) highestPeak = lastSample;

                    const float detectionThreshold = thresholdAdjust;
                    bool clientDetected = ((lastSample > detectionThreshold && dbcheck) || forceNotNatty);
                    if (clientDetected && !wasDetectedLastFrame) clientDetectionCount++;
                    wasDetectedLastFrame = clientDetected;

                    ImGui::Text("Decoder Name: %s", yexDecoderText);
                    ImGui::Text("Last Sample: %.2f dB", lastSample);
                    ImGui::Text("Highest Peak: %.2f dB", highestPeak);
                    ImGui::Text("Client Detections: %d", clientDetectionCount);

                    if (opt3) { // Debug overlay
                        ImGui::Text("Raw Sample: %.2f", rawSample);
                        ImGui::Text("Threshold: %.2f", detectionThreshold);
                    }

                    ImGui::Spacing();
                    ImGui::Text("Live Graph");
                    ImGui::Separator();

                    // === Graph drawing ===
                    ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 150);
                    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                    ImDrawList* draw = ImGui::GetWindowDrawList();
                    ImVec2 bottomRight(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

                    const ImGuiStyle& style = ImGui::GetStyle();
                    ImU32 bgCol = ImGui::GetColorU32(style.Colors[ImGuiCol_WindowBg]);
                    ImU32 border = ImGui::GetColorU32(style.Colors[ImGuiCol_Border]);
                    ImU32 threshCol = IM_COL32(200, 200, 50, 255);
                    ImU32 nattyCol = IM_COL32(0, 180, 0, 255);
                    ImU32 unnattyCol = IM_COL32(220, 40, 40, 255);

                    draw->AddRectFilled(canvasPos, bottomRight, bgCol, style.ChildRounding);
                    draw->AddRect(canvasPos, bottomRight, border, style.ChildRounding);

                    // Grid lines
                    const int gridLines = 6;
                    for (int i = 1; i < gridLines; ++i) {
                        float y = canvasPos.y + (canvasSize.y / gridLines) * i;
                        draw->AddLine(ImVec2(canvasPos.x, y), ImVec2(bottomRight.x, y), IM_COL32(255, 255, 255, 25));
                    }

                    std::vector<float> s;
                    {
                        std::lock_guard<std::mutex> lock(db_graph_mutex);
                        s = db_graph_data;
                    }

                    if (s.size() > 1) {
                        float dataMin = FLT_MAX, dataMax = -FLT_MAX;
                        for (float v : s) { if (v < dataMin) dataMin = v; if (v > dataMax) dataMax = v; }
                        if (dataMin == FLT_MAX) { dataMin = -1.f; dataMax = 1.f; }

                        float range = dataMax - dataMin;
                        float pad = (range * 0.1f > 1.0f) ? range * 0.1f : 1.0f;
                        float targetMin = dataMin - pad, targetMax = dataMax + pad;

                        static float viewMin = targetMin, viewMax = targetMax;
                        viewMin = Lerp(viewMin, targetMin, 0.15f);
                        viewMax = Lerp(viewMax, targetMax, 0.15f);

                        const float minSpan = 6.0f;
                        if (viewMax - viewMin < minSpan) {
                            float mid = 0.5f * (viewMax + viewMin);
                            viewMin = mid - 0.5f * minSpan;
                            viewMax = mid + 0.5f * minSpan;
                        }

                        auto v2y = [&](float v) {
                            float t = (v - viewMin) / (viewMax - viewMin);
                            t = std::clamp(t, 0.0f, 1.0f);
                            return canvasPos.y + canvasSize.y - t * canvasSize.y;
                            };

                        const int n = (int)s.size();
                        const float dx = (n > 1) ? (canvasSize.x / (float)(n - 1)) : 0.f;
                        const float threshY = v2y(detectionThreshold);
                        const float thickness = 2.0f;

                        for (int i = 0; i < n - 1; ++i) {
                            float x0 = canvasPos.x + i * dx, x1 = canvasPos.x + (i + 1) * dx;
                            float v0 = s[i], v1 = s[i + 1];
                            float y0 = v2y(v0), y1 = v2y(v1);

                            if (forceNotNatty) { draw->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), unnattyCol, thickness); continue; }

                            bool hot0 = (v0 > detectionThreshold) && dbcheck;
                            bool hot1 = (v1 > detectionThreshold) && dbcheck;

                            if (hot0 == hot1 || v0 == v1) {
                                draw->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), hot0 ? unnattyCol : nattyCol, thickness);
                            }
                            else {
                                float t = (detectionThreshold - v0) / (v1 - v0);
                                t = std::clamp(t, 0.0f, 1.0f);
                                float xm = x0 + t * (x1 - x0);
                                float ym = v2y(detectionThreshold);
                                draw->AddLine(ImVec2(x0, y0), ImVec2(xm, ym), hot0 ? unnattyCol : nattyCol, thickness);
                                draw->AddLine(ImVec2(xm, ym), ImVec2(x1, y1), hot1 ? unnattyCol : nattyCol, thickness);
                            }
                        }

                        float styleThickness = style.FrameBorderSize > 0 ? style.FrameBorderSize : 1.5f;
                        draw->AddLine(ImVec2(canvasPos.x, threshY), ImVec2(bottomRight.x, threshY), threshCol, styleThickness);
                    }

                    ImGui::Dummy(canvasSize);
                }
                ImGui::EndChild(); // RightPanel
                ImGui::EndChild(); // Main Wrapper
                ImGui::EndTabItem();
            }

            // === MISC TAB ===
            if (ImGui::BeginTabItem("Misc")) {
                ImGui::BeginChild("MiscChild", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysUseWindowPadding);
                if (ImGui::Checkbox("Hide Window", &hide::Hider)) {
                    if (hide::Hider)
                        SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
                    else
                        SetWindowDisplayAffinity(hwnd, WDA_NONE);
                }
                ImGui::Separator();
                ImGui::Text("EXE's");

                // Kill specific injector process by name
                static char processName[128] = "";
                ImGui::InputText("Injector Process Name", processName, IM_ARRAYSIZE(processName));
                if (ImGui::Button("Kill Injector Process")) {
                    std::wstring wProcessName = std::wstring(processName, &processName[strlen(processName)]);
                    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                    if (hSnapshot != INVALID_HANDLE_VALUE) {
                        PROCESSENTRY32W pe32;
                        pe32.dwSize = sizeof(PROCESSENTRY32W);
                        if (Process32FirstW(hSnapshot, &pe32)) {
                            do {
                                if (_wcsicmp(pe32.szExeFile, wProcessName.c_str()) == 0) {
                                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                                    if (hProcess != NULL) {
                                        TerminateProcess(hProcess, 0);
                                        CloseHandle(hProcess);
                                    }
                                }
                            } while (Process32NextW(hSnapshot, &pe32));
                        }
                        CloseHandle(hSnapshot);
                    }
                }

                ImGui::SeparatorText("Spoofing");

                if (ImGui::Button("Clear Hook Traces")) {
                    // Clear memory traces of hooks
                    UnhookWindowsHookEx(hHook);
                    FreeLibrary(hModule);
                    // Clear registry entries if any
                    RegDeleteKey(HKEY_CURRENT_USER, L"Software\\Injector");
                }

                if (ImGui::Button("Randomize Memory Signatures")) {
                    if (memoryBase != nullptr && memoryRegionSize > 0) {
                        // Modify memory patterns to avoid detection
                        for (size_t i = 0; i < memoryRegionSize; i += sizeof(DWORD)) {
                            DWORD oldProtect;
                            VirtualProtect((LPVOID)(memoryBase + i), sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect);
                            *(DWORD*)(memoryBase + i) ^= 0xDEADBEEF; // Simple XOR obfuscation
                            VirtualProtect((LPVOID)(memoryBase + i), sizeof(DWORD), oldProtect, &oldProtect);
                        }
                    }
                }

                if (ImGui::Button("Flush Logs")) {
                    DeleteFileW(L"injector.log");
                    DeleteFileW(L"hook_trace.log");
                }

                // anti debug shyt
                if (ImGui::Button("Enable Anti-Debug")) {
                    if (IsDebuggerPresentAdvanced()) {
                        ExitProcess(0); // kill debugger if its detected
                    }
                }

                if (ImGui::Button("Obfuscate Module Name")) {
                    ObfuscateModuleName();
                }

                if (ImGui::Button("Clear PE Headers")) {
                    ClearPEHeaders();
                }

                if (ImGui::Button("Hide Thread Start Address")) {
                    HideThreadStartAddress();
                }

                if (ImGui::Button("Clear Debug Information")) {
                    ClearDebugInfo();
                }

                if (ImGui::Button("Clear Event Logs")) {
                    ClearEventLogs();
                }


                // Self-destruct
                if (ImGui::Button("Self-Destruct")) {
                    SelfDestruct();
                }

                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("VST")) {
                ImGui::BeginChild("VSTChild", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysUseWindowPadding);
                RenderVSTControls();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Lua")) {
                static char scriptBuf[8192] = ""; // start empty so placeholder shows

                // === Console Controls ===
                ImGui::BeginChild("ConsoleControls", ImVec2(0, 50), true);
                if (ImGui::Button("Open Console")) {
                    if (!GetConsoleWindow()) {
                        AllocConsole();
                        FILE* f;
                        freopen_s(&f, "CONOUT$", "w", stdout);
                        freopen_s(&f, "CONIN$", "r", stdin);
                        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                    }
                    if (!L) {
                        InitLua();
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button("Close Console")) {
                    HWND hwnd = GetConsoleWindow();
                    if (hwnd) {
                        FreeConsole();
                        PostMessage(hwnd, WM_CLOSE, 0, 0);
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button("Show Logs")) {
                    showLogs = !showLogs;
                }
                ImGui::EndChild();

                ImGui::Separator();

                // === Run / Reset / Clear ===
                ImGui::BeginChild("LuaControls", ImVec2(0, 50), true);
                if (ImGui::Button("Run Script")) {
                    if (L) {
                        AddLog("[Lua] Running script...");
                        int result = luaL_dostring(L, scriptBuf);
                        if (result != LUA_OK) {
                            const char* err = lua_tostring(L, -1);
                            AddLog("[Lua Error] %s", err);
                            lua_pop(L, 1);
                        }
                    }
                    else {
                        AddLog("[Lua Error] Lua state not initialized!");
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear Logs")) {
                    logEntries.clear();
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset Lua")) {
                    ShutdownLua();
                    InitLua();
                    RegisterLuaFunctions();
                    AddLog("[Lua] State reset.");
                }
                ImGui::EndChild();

                ImGui::Separator();

                // === Editor ===
                ImGui::BeginChild("LuaEditor", ImVec2(0, -ImGui::GetTextLineHeightWithSpacing() * 10), true);
                ImGui::Text("Editor:");

                const char* placeholder =
                    "-- Write real Lua here\n"
                    "print('Lua ready!')\n"
                    "setgain(1.0)\n";

                if (ImGui::InputTextMultiline("##LuaEditor", scriptBuf, sizeof(scriptBuf),
                    ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 20),
                    ImGuiInputTextFlags_AllowTabInput))
                {
                    // user typed -> handled normally
                }

                // draw placeholder when buffer is empty
                if (scriptBuf[0] == '\0') {
                    ImVec2 pos = ImGui::GetItemRectMin();
                    pos.x += 5;
                    pos.y += 5;
                    ImGui::GetWindowDrawList()->AddText(
                        pos,
                        ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.5f, 0.7f)), // grey/transparent
                        placeholder
                    );
                }

                ImGui::EndChild();

                // === Logs window ===
                if (showLogs) {
                    ImGui::Begin("Lua Console Logs", &showLogs);
                    ImGui::BeginChild("LogTextArea", ImVec2(0, -30), true);

                    for (auto& entry : logEntries) {
                        if (!entry.prefix.empty()) {
                            ImGui::TextColored(entry.prefixColor, "%s", entry.prefix.c_str());
                            ImGui::SameLine(0.0f, 0.0f);
                            ImGui::TextColored(entry.msgColor, "%s", entry.message.c_str());
                        }
                        else {
                            ImGui::TextColored(entry.msgColor, "%s", entry.message.c_str());
                        }
                    }

                    ImGui::EndChild();

                    if (ImGui::Button("Clear Logs")) {
                        logEntries.clear();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Refresh Logs")) {
                        // optional: reload/reprocess logs
                    }

                    ImGui::End();
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Info")) {
                ImGui::BeginChild("InfoChild", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysUseWindowPadding);
                ImGui::TextWrapped("SpiderManHook - by zion btw / blasphemyw");
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
        ImGui::EndFrame();

        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        const float clear_col_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_col_with_alpha);
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT result = g_pSwapChain->Present(1, 0);
        if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
            HandleDeviceLost(hwnd);
    }

    ReleaseSpiderManTexture();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
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
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
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