#pragma once
// ============================================================================
// SpidermanHook - by zion btw / blasphemyw
// ThemeSystem.h - theme definitions, animated transitions, settings persistence
// Include ONCE (from HookGui.cpp).
// ============================================================================
#include "imgui/imgui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>

#ifndef SPOOK
#define SPOOK static
#endif

enum BgFx {
    FX_NONE = 0, FX_WEBS, FX_CONSTELLATION, FX_NEBULA, FX_BUTTERFLY, FX_RAIN,
    FX_LIGHTNING, FX_FIRE, FX_SNOW, FX_OCEAN, FX_VIOLET, FX_MATRIX,
    FX_SAKURA, FX_ECLIPSE, FX_AURORA
};

struct ThemeDef {
    const char* id;
    const char* name;
    const char* cat;      // category chip
    ImVec4 bg;            // base background
    ImVec4 panel;         // panels / children
    ImVec4 accent;        // primary accent
    ImVec4 accent2;       // secondary accent
    ImVec4 text;
    int    fx;            // BgFx
};

inline ThemeDef g_Themes[] = {
    {"spider",       "Spider",        "Spider-Man", {0.020f,0.020f,0.030f,1}, {0.075f,0.055f,0.065f,1}, {0.890f,0.180f,0.180f,1}, {0.520f,0.080f,0.080f,1}, {0.940f,0.900f,0.900f,1}, FX_WEBS},
    {"constellation","Constellation", "Space",      {0.010f,0.018f,0.048f,1}, {0.060f,0.085f,0.150f,1}, {0.380f,0.580f,1.000f,1}, {0.640f,0.450f,1.000f,1}, {0.880f,0.920f,1.000f,1}, FX_CONSTELLATION},
    {"nebula",       "Nebula",        "Space",      {0.032f,0.016f,0.052f,1}, {0.110f,0.065f,0.160f,1}, {0.760f,0.420f,0.980f,1}, {0.320f,0.600f,0.980f,1}, {0.950f,0.880f,1.000f,1}, FX_NEBULA},
    {"butterfly",    "Butterfly",     "Nature",     {0.048f,0.026f,0.050f,1}, {0.120f,0.075f,0.125f,1}, {0.960f,0.520f,0.820f,1}, {0.550f,0.750f,1.000f,1}, {1.000f,0.930f,0.970f,1}, FX_BUTTERFLY},
    {"rain",         "Rain",          "Weather",    {0.030f,0.038f,0.048f,1}, {0.090f,0.110f,0.135f,1}, {0.450f,0.700f,0.900f,1}, {0.300f,0.480f,0.650f,1}, {0.870f,0.930f,0.970f,1}, FX_RAIN},
    {"lightning",    "Lightning",     "Weather",    {0.022f,0.024f,0.032f,1}, {0.075f,0.080f,0.105f,1}, {0.620f,0.780f,1.000f,1}, {0.980f,0.900f,0.400f,1}, {0.910f,0.950f,1.000f,1}, FX_LIGHTNING},
    {"inferno",      "Inferno",       "Elemental",  {0.050f,0.022f,0.012f,1}, {0.140f,0.065f,0.035f,1}, {1.000f,0.480f,0.140f,1}, {1.000f,0.220f,0.220f,1}, {1.000f,0.900f,0.820f,1}, FX_FIRE},
    {"frost",        "Frost",         "Nature",     {0.028f,0.042f,0.060f,1}, {0.095f,0.125f,0.165f,1}, {0.480f,0.820f,1.000f,1}, {0.720f,0.900f,1.000f,1}, {0.890f,0.960f,1.000f,1}, FX_SNOW},
    {"ocean",        "Ocean",         "Nature",     {0.008f,0.038f,0.062f,1}, {0.045f,0.115f,0.165f,1}, {0.150f,0.800f,0.860f,1}, {0.200f,0.480f,0.980f,1}, {0.850f,0.980f,1.000f,1}, FX_OCEAN},
    {"violet",       "Violet",        "Neon",       {0.028f,0.014f,0.046f,1}, {0.100f,0.055f,0.155f,1}, {0.680f,0.280f,1.000f,1}, {0.920f,0.300f,0.780f,1}, {0.950f,0.880f,1.000f,1}, FX_VIOLET},
    {"matrix",       "Matrix",        "Neon",       {0.008f,0.022f,0.008f,1}, {0.035f,0.085f,0.040f,1}, {0.180f,1.000f,0.380f,1}, {0.500f,0.900f,0.250f,1}, {0.820f,1.000f,0.850f,1}, FX_MATRIX},
    {"sakura",       "Sakura",        "Nature",     {0.052f,0.030f,0.044f,1}, {0.135f,0.085f,0.110f,1}, {1.000f,0.600f,0.760f,1}, {0.900f,0.400f,0.600f,1}, {1.000f,0.940f,0.960f,1}, FX_SAKURA},
    {"eclipse",      "Eclipse",       "Space",      {0.012f,0.010f,0.014f,1}, {0.060f,0.048f,0.058f,1}, {1.000f,0.310f,0.230f,1}, {0.760f,0.160f,0.120f,1}, {0.960f,0.880f,0.880f,1}, FX_ECLIPSE},
    {"aurora",       "Aurora",        "Space",      {0.008f,0.020f,0.030f,1}, {0.050f,0.090f,0.110f,1}, {0.240f,0.950f,0.700f,1}, {0.450f,0.550f,1.000f,1}, {0.870f,1.000f,0.950f,1}, FX_AURORA},
};
inline const int THEME_COUNT = (int)(sizeof(g_Themes)/sizeof(g_Themes[0]));

struct SpiderSettings {
    int   themeIndex        = 0;
    int   activeTab         = 0;
    bool  particles         = true;
    bool  glow              = true;
    bool  perfMode          = false;
    bool  rainbow           = false;
    bool  soundFx           = false;
    bool  useCustomAccent   = false;
    bool  useCustomBg       = false;
    float particleAmount    = 1.00f;   // 0 .. 2
    float animSpeed         = 1.00f;   // 0.1 .. 3
    float bgOpacity         = 0.35f;   // dim layer 0 .. 0.9
    float blurStrength      = 0.45f;   // glass translucency 0 .. 1
    float uiScale           = 1.00f;   // spacing scale 0.8 .. 1.4
    int   fontSize          = 18;      // applied on next inject
    float customAccent[4]   = {0.89f,0.21f,0.21f,1.0f};
    float customBg[4]       = {0.02f,0.02f,0.03f,1.0f};
    bool  favorites[32]     = {};
    int   recentThemes[3]   = {-1,-1,-1};
};
inline SpiderSettings g_ui;
inline void defaults(){ g_ui = SpiderSettings(); }

// ---------------------------------------------------------------- utilities
inline float spClampf(float v, float lo, float hi){ return v < lo ? lo : (v > hi ? hi : v); }
inline ImVec4 spLerpV(const ImVec4& a, const ImVec4& b, float t){
    t = spClampf(t,0.f,1.f);
    return ImVec4(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t, a.w+(b.w-a.w)*t);
}
inline ImU32 spCol(const ImVec4& c, float alphaMul = 1.0f){
    float a = spClampf(c.w * alphaMul, 0.f, 1.f);
    return IM_COL32((int)(spClampf(c.x,0.f,1.f)*255), (int)(spClampf(c.y,0.f,1.f)*255),
                    (int)(spClampf(c.z,0.f,1.f)*255), (int)(a*255));
}
inline ImU32 spFadeAlpha(ImU32 col, float mul){
    int a = (int)(((col >> IM_COL32_A_SHIFT) & 0xFF) * spClampf(mul,0.f,1.f));
    if (a > 255) a = 255;
    return (col & ~IM_COL32_A_MASK) | ((ImU32)a << IM_COL32_A_SHIFT);
}

// ------------------------------------------------------- settings persistence
SPOOK const char* SettingsDir(){ return "SpiderManHook"; }
inline void SaveUISettings(){
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s\\%s", getenv("APPDATA") ? getenv("APPDATA") : ".", SettingsDir());
    _mkdir(path);
    strncat_s(path, "\\settings.cfg", sizeof(path) - strlen(path) - 1);
    FILE* f = nullptr;
    if (fopen_s(&f, path, "w") != 0 || !f) return;
    fprintf(f, "theme=%d\nparticles=%d\nglow=%d\nperf=%d\nrainbow=%d\nsoundfx=%d\n",
            g_ui.themeIndex, g_ui.particles?1:0, g_ui.glow?1:0, g_ui.perfMode?1:0,
            g_ui.rainbow?1:0, g_ui.soundFx?1:0);
    fprintf(f, "customAccent=%d\ncustomBg=%d\n", g_ui.useCustomAccent?1:0, g_ui.useCustomBg?1:0);
    fprintf(f, "accent=%.4f,%.4f,%.4f\nbg=%.4f,%.4f,%.4f\n",
            g_ui.customAccent[0],g_ui.customAccent[1],g_ui.customAccent[2],
            g_ui.customBg[0],g_ui.customBg[1],g_ui.customBg[2]);
    fprintf(f, "pamount=%.3f\nspeed=%.3f\nbgop=%.3f\nblur=%.3f\nscale=%.3f\nfont=%d\n",
            g_ui.particleAmount,g_ui.animSpeed,g_ui.bgOpacity,g_ui.blurStrength,g_ui.uiScale,g_ui.fontSize);
    fprintf(f, "recent=%d,%d,%d\n", g_ui.recentThemes[0], g_ui.recentThemes[1], g_ui.recentThemes[2]);
    fprintf(f, "fav=");
    for (int i = 0; i < THEME_COUNT && i < 32; ++i) fputc(g_ui.favorites[i] ? '1' : '0', f);
    fputc('\n', f);
    fclose(f);
}
inline void LoadUISettings(){
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s\\%s\\settings.cfg", getenv("APPDATA") ? getenv("APPDATA") : ".", SettingsDir());
    FILE* f = nullptr;
    if (fopen_s(&f, path, "r") != 0 || !f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char key[64] = "", val[384] = "";
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0; strncpy_s(key, line, _TRUNCATE); strncpy_s(val, eq+1, _TRUNCATE);
        char* nl = strchr(val, '\n'); if (nl) *nl = 0;
        if      (!strcmp(key,"theme"))    g_ui.themeIndex = atoi(val);
        else if (!strcmp(key,"particles"))g_ui.particles = atoi(val) != 0;
        else if (!strcmp(key,"glow"))     g_ui.glow = atoi(val) != 0;
        else if (!strcmp(key,"perf"))     g_ui.perfMode = atoi(val) != 0;
        else if (!strcmp(key,"rainbow"))  g_ui.rainbow = atoi(val) != 0;
        else if (!strcmp(key,"soundfx"))  g_ui.soundFx = atoi(val) != 0;
        else if (!strcmp(key,"customAccent")) g_ui.useCustomAccent = atoi(val) != 0;
        else if (!strcmp(key,"customBg"))     g_ui.useCustomBg = atoi(val) != 0;
        else if (!strcmp(key,"accent")) sscanf(val, "%f,%f,%f", &g_ui.customAccent[0],&g_ui.customAccent[1],&g_ui.customAccent[2]);
        else if (!strcmp(key,"bg"))     sscanf(val, "%f,%f,%f", &g_ui.customBg[0],&g_ui.customBg[1],&g_ui.customBg[2]);
        else if (!strcmp(key,"pamount")) g_ui.particleAmount = (float)atof(val);
        else if (!strcmp(key,"speed"))   g_ui.animSpeed = (float)atof(val);
        else if (!strcmp(key,"bgop"))    g_ui.bgOpacity = (float)atof(val);
        else if (!strcmp(key,"blur"))    g_ui.blurStrength = (float)atof(val);
        else if (!strcmp(key,"scale"))   g_ui.uiScale = (float)atof(val);
        else if (!strcmp(key,"font"))    g_ui.fontSize = atoi(val);
        else if (!strcmp(key,"recent")) sscanf(val, "%d,%d,%d", &g_ui.recentThemes[0],&g_ui.recentThemes[1],&g_ui.recentThemes[2]);
        else if (!strcmp(key,"fav"))
            for (int i = 0; i < THEME_COUNT && i < 32 && val[i]; ++i) g_ui.favorites[i] = val[i]=='1';
    }
    fclose(f);
    g_ui.themeIndex = spClampf((float)g_ui.themeIndex, 0, THEME_COUNT-1);
}

// ------------------------------------------------------------ live palette
struct SpPalette { ImVec4 bg, panel, accent, accent2, text; };
inline SpPalette g_palCur = {};
inline bool       g_palInit = false;
inline float      g_rainbowT = 0.0f;

inline ImVec4 EffectiveAccent(float timeNow){
    if (g_ui.rainbow) {
        float h = fmodf(timeNow * 0.06f, 1.0f);
        float r,g,b; ImGui::ColorConvertHSVtoRGB(h, 0.65f, 1.0f, r, g, b);
        return ImVec4(r,g,b,1.0f);
    }
    if (g_ui.useCustomAccent) return ImVec4(g_ui.customAccent[0],g_ui.customAccent[1],g_ui.customAccent[2],1.0f);
    return g_Themes[g_ui.themeIndex].accent;
}
inline ImVec4 EffectiveBg(){
    if (g_ui.useCustomBg) return ImVec4(g_ui.customBg[0],g_ui.customBg[1],g_ui.customBg[2],1.0f);
    return g_Themes[g_ui.themeIndex].bg;
}

inline void PushRecent(int idx){
    if (idx < 0 || idx >= THEME_COUNT) return;
    for (int k = 0; k < 3; ++k) if (g_ui.recentThemes[k] == idx) return;
    g_ui.recentThemes[2] = g_ui.recentThemes[1]; g_ui.recentThemes[1] = g_ui.recentThemes[0]; g_ui.recentThemes[0] = idx;
}

inline void SetTheme(int idx){
    idx = spClampf((float)idx, 0, THEME_COUNT-1);
    g_ui.themeIndex = idx;
    PushRecent(idx);
    SaveUISettings();
}

inline ImVec4 Dimmed(const ImVec4& c, float m){ return ImVec4(c.x*m, c.y*m, c.z*m, c.w); }

// Advance transition & push the themed ImGui style. Call once per frame.
inline void UpdateAndApplyStyle(float dt, float timeNow){
    ThemeDef& th = g_Themes[g_ui.themeIndex];
    SpPalette tgt;
    tgt.bg     = EffectiveBg();
    tgt.panel  = th.panel;
    tgt.accent = EffectiveAccent(timeNow);
    tgt.accent2= th.accent2;
    tgt.text   = th.text;

    if (!g_palInit) { g_palCur = tgt; g_palInit = true; }
    float k = 1.0f - expf(-dt * 5.0f);            // smooth animated transitions
    g_palCur.bg     = spLerpV(g_palCur.bg,     tgt.bg,     k);
    g_palCur.panel  = spLerpV(g_palCur.panel,  tgt.panel,  k);
    g_palCur.accent = spLerpV(g_palCur.accent, tgt.accent, k);
    g_palCur.accent2= spLerpV(g_palCur.accent2,tgt.accent2,k);
    g_palCur.text   = spLerpV(g_palCur.text,   tgt.text,   k);

    ImGuiStyle& st = ImGui::GetStyle();
    float sc  = spClampf(g_ui.uiScale, 0.75f, 1.5f);
    float bl  = spClampf(g_ui.blurStrength, 0.f, 1.f);

    ImVec4 winBg   = g_palCur.bg;   winBg.w   = 1.00f;
    ImVec4 childBg = g_palCur.panel; childBg.w = 0.86f - 0.34f*bl;

    st.WindowPadding   = ImVec2(10*sc,10*sc);
    st.FramePadding    = ImVec2(8*sc,4*sc);
    st.ItemSpacing     = ImVec2(8*sc,6*sc);
    st.ItemInnerSpacing= ImVec2(6*sc,4*sc);
    st.ScrollbarSize   = 10*sc;
    st.GrabMinSize     = 8*sc;
    st.WindowRounding  = 10; st.ChildRounding = 8; st.FrameRounding = 5;
    st.PopupRounding   = 8; st.GrabRounding   = 5; st.TabRounding   = 6;
    st.WindowBorderSize = 0; st.ChildBorderSize = 1; st.FrameBorderSize = 0;
    st.WindowTitleAlign = ImVec2(0.5f,0.5f);
    st.ButtonTextAlign  = ImVec2(0.5f,0.5f);

    SpPalette& c = g_palCur;
    st.Colors[ImGuiCol_Text]          = c.text;
    st.Colors[ImGuiCol_TextDisabled]  = Dimmed(c.text, 0.45f);
    st.Colors[ImGuiCol_TextSelectedBg]= ImVec4(c.accent.x,c.accent.y,c.accent.z,0.35f);
    st.Colors[ImGuiCol_WindowBg]      = winBg;
    st.Colors[ImGuiCol_ChildBg]       = childBg;
    st.Colors[ImGuiCol_PopupBg]       = ImVec4(winBg.x*0.8f,winBg.y*0.8f,winBg.z*0.8f,0.97f);
    st.Colors[ImGuiCol_Border]        = ImVec4(c.accent.x,c.accent.y,c.accent.z,0.16f);
    st.Colors[ImGuiCol_BorderShadow]  = ImVec4(0,0,0,0);
    st.Colors[ImGuiCol_FrameBg]       = Dimmed(childBg, 1.35f);
    st.Colors[ImGuiCol_FrameBgHovered]= ImVec4(Dimmed(childBg,1.6f).x,Dimmed(childBg,1.6f).y,Dimmed(childBg,1.6f).z,0.95f);
    st.Colors[ImGuiCol_FrameBgActive] = ImVec4(c.accent.x*0.5f+childBg.x*0.5f, c.accent.y*0.5f+childBg.y*0.5f, c.accent.z*0.5f+childBg.z*0.5f, 0.95f);
    st.Colors[ImGuiCol_TitleBg]       = winBg;
    st.Colors[ImGuiCol_TitleBgActive] = winBg;
    st.Colors[ImGuiCol_MenuBarBg]     = winBg;
    st.Colors[ImGuiCol_ScrollbarBg]   = ImVec4(winBg.x,winBg.y,winBg.z,0.4f);
    st.Colors[ImGuiCol_ScrollbarGrab] = Dimmed(c.accent,0.6f);
    st.Colors[ImGuiCol_CheckMark]     = c.accent;
    st.Colors[ImGuiCol_SliderGrab]    = c.accent;
    st.Colors[ImGuiCol_SliderGrabActive]= ImVec4(c.accent.x*1.2f>1?1:c.accent.x*1.2f, c.accent.y*1.2f>1?1:c.accent.y*1.2f, c.accent.z*1.2f>1?1:c.accent.z*1.2f, 1);
    st.Colors[ImGuiCol_Button]        = Dimmed(childBg, 1.5f);
    st.Colors[ImGuiCol_ButtonHovered] = ImVec4(c.accent.x*0.35f+Dimmed(childBg,1.6f).x*0.65f, c.accent.y*0.35f+Dimmed(childBg,1.6f).y*0.65f, c.accent.z*0.35f+Dimmed(childBg,1.6f).z*0.65f, 1);
    st.Colors[ImGuiCol_ButtonActive]  = ImVec4(c.accent.x*0.6f+childBg.x*0.4f, c.accent.y*0.6f+childBg.y*0.4f, c.accent.z*0.6f+childBg.z*0.4f, 1);
    st.Colors[ImGuiCol_Header]        = Dimmed(childBg, 1.5f);
    st.Colors[ImGuiCol_HeaderHovered] = ImVec4(c.accent.x*0.4f+childBg.x*0.6f, c.accent.y*0.4f+childBg.y*0.6f, c.accent.z*0.4f+childBg.z*0.6f, 1);
    st.Colors[ImGuiCol_HeaderActive]  = ImVec4(c.accent.x*0.6f+childBg.x*0.4f, c.accent.y*0.6f+childBg.y*0.4f, c.accent.z*0.6f+childBg.z*0.4f, 1);
    st.Colors[ImGuiCol_Separator]     = ImVec4(c.accent.x,c.accent.y,c.accent.z,0.22f);
    st.Colors[ImGuiCol_Tab]           = Dimmed(childBg,1.3f);
    st.Colors[ImGuiCol_TabHovered]    = ImVec4(c.accent.x*0.4f+childBg.x*0.6f, c.accent.y*0.4f+childBg.y*0.6f, c.accent.z*0.4f+childBg.z*0.6f,1);
    st.Colors[ImGuiCol_TabSelected]   = Dimmed(c.accent,0.8f);
    st.Colors[ImGuiCol_ModalWindowDimBg]= ImVec4(0,0,0,0.6f);
    st.Colors[ImGuiCol_PlotLines]     = c.accent;
    st.Colors[ImGuiCol_PlotHistogram] = c.accent2;
    st.Colors[ImGuiCol_ResizeGrip]    = ImVec4(0,0,0,0);
    st.Colors[ImGuiCol_NavCursor]     = c.accent;
}
