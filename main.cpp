#pragma warning(suppress : 6387)

#include "windows.h"
#include "tlhelp32.h"

#include "thread"

#include "minhook/include/minhook.h"
#include "libraries/ImGui/Hookgui.hpp"

#include "opus.h"
#include <vector>
#include <mutex>
#include <cmath>
#include <cstdio>
#include "libraries/vst/VSTHost.h"
#include <unordered_map>

// includes + M_PI
#include <complex>
#include <valarray>
#include <vector>
#include <algorithm>
#include <cmath>
#include <mutex>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Globals for encoder graph ---
std::mutex self_graph_mutex;
std::vector<float> self_graph_data;
const size_t SELF_GRAPH_MAX_SAMPLES = 4096;

using Complex = std::complex<float>;
using CArray = std::valarray<Complex>;

// thread-safe PCM buffer for spectrum (mono, linear float samples)
std::vector<float> pcm_graph_data;
std::mutex pcm_graph_mutex;
const size_t PCM_GRAPH_MAX_SAMPLES = 16384; // keep ~16k samples max


static std::unordered_map<uint64_t, float> s_smoothedInt16ByStream;

// SpiderManHook - by zion btw / blasphemyw
static HMODULE(WINAPI* True_LoadLibraryExW)(LPCWSTR, HANDLE, DWORD) = nullptr;

static void(__fastcall* Original_SplitFilter)(__int64*, unsigned __int64, __int64) = nullptr;
static void(__fastcall* Original_MatchFilter)(__int64, __int64, int, __int64, __int64, int, __int64, int, int, int, int, char) = nullptr;

// Forward declaration for ProcessingV2 encoder
extern "C" int opus_encode_with_processing(OpusEncoder* st, const opus_int16* pcm, int frame_size, unsigned char* data, opus_int32 max_data_bytes);

static int __cdecl opus_encode_with_vst(OpusEncoder* st, const opus_int16* pcm, int frame_size, unsigned char* data, opus_int32 max_data_bytes)
{
    if (!g_vstHost)
        g_vstHost = std::make_unique<VSTHost>();

    bool hasActiveVST = false;
    if (g_vstHost && g_vstHost->getPluginCount() > 0) {
        for (int i = 0; i < g_vstHost->getPluginCount(); ++i) {
            VSTPlugin* p = g_vstHost->getPlugin(i);
            if (p && !p->isBypassed()) {
                hasActiveVST = true;
                break;
            }
        }
    }

    if (hasActiveVST)
    {
        int channels = 2;
        const int samples = frame_size * channels;

        std::vector<float> floatIn(samples);
        std::vector<float> floatOut(samples);

        for (int i = 0; i < samples; ++i)
            floatIn[i] = static_cast<float>(pcm[i]) / 32768.0f;

        g_vstHost->setGlobalSampleRate(48000.0f);
        g_vstHost->setGlobalBlockSize(frame_size);
        bool processed = g_vstHost->processInterleaved(floatIn.data(), floatOut.data(), frame_size, channels);

        if (processed) {
            std::vector<opus_int16> vstProcessed(samples);
            for (int i = 0; i < samples; ++i)
            {
                float s = floatOut[i] * 32768.0f;
                if (s > 32767.0f) s = 32767.0f;
                if (s < -32768.0f) s = -32768.0f;
                vstProcessed[i] = static_cast<opus_int16>(s);
            }
            return opus_encode_with_processing(st, vstProcessed.data(), frame_size, data, max_data_bytes);
        }
    }

    return opus_encode_with_processing(st, pcm, frame_size, data, max_data_bytes);
}

char yexDecoderText[128] = "";
std::vector<float> db_graph_data(100, 0.0f);
std::mutex db_graph_mutex;

bool int16check = false;
bool dbcheck = false;
bool forceNotNatty = false;
bool selfcheck = false;

__int64(__fastcall* opusdecode_orig)(
    uint32_t*,
    uint8_t*,
    int,
    __int64*,
    int,
    unsigned int,
    int,
    __int64,
    int);

// === Console helper ===
void CreateConsole() {
    AllocConsole();
    SetConsoleTitleA(" "); // Set console title
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
}


// === dB checker with DC Reject ===
auto __fastcall dbchecker(uint32_t* st, unsigned __int8* data, int len,
    __int64* pcm, int frame_size, int decode_fec,
    int self_delimited, __int64 packet_offset, int soft_clip)
{
    // force-disable soft clipping for analysis
    soft_clip = 0;

    // decode
    int OriginalResult = opusdecode_orig(st, data, len, pcm, frame_size,
        decode_fec, self_delimited,
        packet_offset, soft_clip);
    if (OriginalResult <= 0)
        return OriginalResult;

    // IMPORTANT: use stereo sample count (matches the white logger)
    const int TotalSamples = 2 * OriginalResult;

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // ensure console attached for printing when requested
    if ((dbcheck || int16check || selfcheck) && !GetConsoleWindow()) {
        if (AllocConsole()) {
            FILE* f;
            freopen_s(&f, "CONOUT$", "w", stdout);
        }
    }

    float* pcmF = reinterpret_cast<float*>(pcm);

    // --- Normal dB checker ---
    if (dbcheck && TotalSamples > 0) {
        float MaxAmplitude = 0.0f;
        for (int i = 0; i < TotalSamples; ++i) {
            MaxAmplitude = fmaxf(MaxAmplitude, fabsf(pcmF[i]));
        }

        if (MaxAmplitude > 0.0f) {
            float DbValue = log10f(MaxAmplitude) * 20.0f; // 20*log10 for amplitude
            {
                std::lock_guard<std::mutex> lock(db_graph_mutex);
                if (!db_graph_data.empty()) db_graph_data.erase(db_graph_data.begin());
                db_graph_data.push_back(DbValue);
            }

            bool clientDetected = (MaxAmplitude > 3.0f) || forceNotNatty;
            SetConsoleTextAttribute(hConsole, clientDetected ? 4 : 2);
            printf("[dB Checker] %.2f dB | Fs: %i %s\n",
                DbValue, frame_size,
                clientDetected ? "| Client Detected" : "| Natty");
            SetConsoleTextAttribute(hConsole, 2);
        }
    }

    // --- NEW: DC Reject Checker ---
    if (selfcheck && TotalSamples > 0) {
        double sum = 0.0;
        for (int i = 0; i < TotalSamples; ++i) {
            sum += pcmF[i];
        }
        double mean = sum / (double)TotalSamples;

        // Reject value is deviation from zero
        double dcReject = fabs(mean);

        // Smooth across frames
        static double smoothedReject = 0.0;
        smoothedReject = smoothedReject * 0.9 + dcReject * 0.1;

        {
            std::lock_guard<std::mutex> lock(db_graph_mutex);
            if (!db_graph_data.empty()) db_graph_data.erase(db_graph_data.begin());
            db_graph_data.push_back((float)smoothedReject); // push reject values to graph
        }

        bool suspicious = (smoothedReject > 0.001) || forceNotNatty;
        SetConsoleTextAttribute(hConsole, suspicious ? 4 : 2);
        printf("[SelfCheck DC Reject] %.6f (smoothed: %.6f)\n",
            dcReject, smoothedReject);
        SetConsoleTextAttribute(hConsole, 2);
    }

    // --- INT16 CHECK (your original) ---
    if (int16check && OriginalResult > 0) {
        const double SCALE = 32767.0;
        const long long PEAK_THRESHOLD = 31366LL;

        double sumAbs = 0.0;
        unsigned int int16PeakCount = 0u;

        const int sampleCount = OriginalResult; // per-channel count

        for (int i = 0; i < sampleCount; ++i) {
            double sampleDouble = static_cast<double>(pcmF[i]) * SCALE;
            long long sample = static_cast<long long>(sampleDouble);
            long long absSample = (sample >= 0) ? sample : -sample;
            sumAbs += static_cast<double>(absSample);

            if (absSample > PEAK_THRESHOLD)
                ++int16PeakCount;
        }

        float int16AvgAbs = (sampleCount > 0)
            ? static_cast<float>(sumAbs / static_cast<double>(sampleCount))
            : 0.0f;

        if (std::isnan(int16AvgAbs) || std::isinf(int16AvgAbs))
            int16AvgAbs = 0.0f;

        // smoothing (prev*0.8 + avg*0.2)
        const uint64_t streamKey = reinterpret_cast<uint64_t>(st);
        float smoothed = int16AvgAbs;
        auto it = s_smoothedInt16ByStream.find(streamKey);
        if (it == s_smoothedInt16ByStream.end()) {
            s_smoothedInt16ByStream.emplace(streamKey, int16AvgAbs);
        }
        else {
            it->second = it->second * 0.8f + int16AvgAbs * 0.2f;
            if (std::isnan(it->second) || std::isinf(it->second))
                it->second = int16AvgAbs;
            smoothed = it->second;
        }

        // console print
        bool unnatty = (int16PeakCount > 0) || forceNotNatty;
        SetConsoleTextAttribute(hConsole, unnatty ? 4 : 2);
        printf("[INT16 Checker] Avg: %.0f // Smooth: %.0f // Framesize: %i // Peaks: %i\n",
            int16AvgAbs, smoothed, frame_size, int16PeakCount);

        SetConsoleTextAttribute(hConsole, 2);
    }

    // === PCM graphing (unchanged) ===
    if (OriginalResult > 0) {
        std::lock_guard<std::mutex> lock(pcm_graph_mutex);
        for (int frame = 0; frame < OriginalResult; ++frame) {
            int idx = frame * 2;
            float left = pcmF[idx];
            float right = pcmF[idx + 1];
            float mono = 0.5f * (left + right);
            pcm_graph_data.push_back(mono);
        }
        if (pcm_graph_data.size() > PCM_GRAPH_MAX_SAMPLES) {
            size_t removeCount = pcm_graph_data.size() - PCM_GRAPH_MAX_SAMPLES;
            pcm_graph_data.erase(pcm_graph_data.begin(), pcm_graph_data.begin() + removeCount);
        }
    }

    return OriginalResult;
}



void __fastcall Bypass_SplitFilter(__int64* x, unsigned __int64 y, __int64 z)
{
    if (y && z == 2) return;
    if ((z & 0xFFFFFFFFFFFFFFFEuLL) != 2) return;
    Original_SplitFilter(x, y, z);
}


void __fastcall MatchFilter(__int64 a1, __int64 a2, int a3, __int64 a4, __int64 a5,
    int a6, __int64 a7, int a8, int a9, int a10, int a11, char a12)
{
    if (a11) a11 = 0;
    if (a12) a12 = 0;
    Original_MatchFilter(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12);
}


int __stdcall ReturnZero() {
    return 0;
}

// Main loader hook
HMODULE WINAPI Hooked_LoadLibraryExW(LPCWSTR moduleName, HANDLE hFile, DWORD dwFlags)
{
    if (!wcsstr(moduleName, L"discord_voice"))
        return True_LoadLibraryExW(moduleName, hFile, dwFlags);

    const auto modBase = True_LoadLibraryExW(moduleName, hFile, dwFlags);
    if (!modBase) return nullptr;

    // Hook points
    const struct {
        DWORD offset;
        LPVOID detour;
        LPVOID* original;
    } hooks[] = {
        { 0x863E90, reinterpret_cast<LPVOID>(opus_encode_with_vst), nullptr },
        { 0x867BA0, reinterpret_cast<LPVOID>(dbchecker), reinterpret_cast<LPVOID*>(&opusdecode_orig) }, // dbchecker hook
        { 0x46869C, reinterpret_cast<LPVOID>(ReturnZero), nullptr },
        { 0x2EF820, reinterpret_cast<LPVOID>(ReturnZero), nullptr },
        { 0x2EDFC0, reinterpret_cast<LPVOID>(ReturnZero), nullptr },  // ProcessStream_StreamConfig
        { 0x2F2648, reinterpret_cast<LPVOID>(ReturnZero), nullptr },  // SendProcessedData
        { 0x5D8750, reinterpret_cast<LPVOID>(ReturnZero), nullptr },
        { 0x2E923C, reinterpret_cast<LPVOID>(Bypass_SplitFilter), reinterpret_cast<LPVOID*>(&Original_SplitFilter) } // Splitting Filter
    };

    for (auto& hk : hooks)
    {
        const auto addr = reinterpret_cast<LPVOID>((uintptr_t)modBase + hk.offset);
        if (MH_CreateHook(addr, hk.detour, hk.original) != MH_OK)
        {

        }
    }

    MH_EnableHook(MH_ALL_HOOKS);

    std::thread([] { ImGui::ui::start(); }).detach();

    return modBase;
}

void SetupHooks()
{
    auto k32 = GetModuleHandleA("kernel32.dll");
    if (!k32) return;

    auto entry = GetProcAddress(k32, "LoadLibraryExW");
    if (!entry) return;

    if (MH_Initialize() != MH_OK)
        return;

    if (MH_CreateHook(entry, Hooked_LoadLibraryExW, reinterpret_cast<LPVOID*>(&True_LoadLibraryExW)) != MH_OK)
        return;

    MH_EnableHook(MH_ALL_HOOKS);
}

// Entry point for DLL
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        std::thread([] { SetupHooks(); }).detach();
    }
    return TRUE;
}
