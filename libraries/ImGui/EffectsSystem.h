#pragma once
// ============================================================================
// SpidermanHook - by zion btw / blasphemyw
// EffectsSystem.h - unified animated background engine for every theme.
// Particles live in normalized [0..1] space so one pool renders anywhere
// (full window background or small theme-card previews).
// Include ONCE (from HookGui.cpp).
// ============================================================================
#include "imgui/imgui.h"
#include "ThemeSystem.h"
#include <math.h>
#include <string.h>

inline const int   FX_MAX = 720;
inline const float SPTAU  = 6.2831853f;

struct FxP {
    ImVec2 n;        // normalized position
    ImVec2 v;        // normalized velocity (units/sec)
    float  size;
    float  phase;
    float  aux;
};
inline FxP  g_fp[FX_MAX];
inline int  g_fn = 0;
inline int  g_fxCur = -1;

inline float g_boltTimer = 2.0f, g_flash = 0.0f;
inline ImVec2 g_bolt[24]; inline int g_bn = 0;
inline ImVec2 g_branch[10]; inline int g_brn = 0;
struct SpSplash { float x, ttl; };
inline SpSplash g_spl[48];

inline unsigned int g_seed = 0x9e3779b9u;
inline float fxrand(){ g_seed = g_seed*1664525u + 1013904223u; return (float)((g_seed >> 8) & 0xFFFFFF) / 16777216.0f; }

static void FxInit(int fx){
    if (g_fxCur == fx && g_fn > 0) return;
    g_fxCur = fx; g_fn = 0;
    memset(g_spl, 0, sizeof(g_spl));
    auto add = [&](float nx,float ny,float vx,float vy,float size,float phase,float aux){
        if (g_fn >= FX_MAX) return;
        FxP& p = g_fp[g_fn++];
        p.n.x=nx; p.n.y=ny; p.v.x=vx; p.v.y=vy; p.size=size; p.phase=phase; p.aux=aux;
    };
    switch (fx) {
    case FX_WEBS:          for (int i=0;i<9;i++)   add(fxrand(),fxrand(),(fxrand()-0.5f)*0.02f,(fxrand()-0.5f)*0.02f, 3+fxrand()*3, fxrand()*SPTAU, 0); break;
    case FX_CONSTELLATION: for (int i=0;i<110;i++) add(fxrand(),fxrand(),(fxrand()-0.5f)*0.010f,(fxrand()-0.5f)*0.010f, 1+fxrand()*1.6f, fxrand()*SPTAU, fxrand()); break;
    case FX_NEBULA:        for (int i=0;i<26;i++)  add(fxrand(),fxrand(),(fxrand()-0.5f)*0.006f,(fxrand()-0.5f)*0.004f, 60+fxrand()*130, fxrand()*SPTAU, fxrand());
                           for (int i=0;i<90;i++)  add(fxrand(),fxrand(),(fxrand()-0.5f)*0.004f,(fxrand()-0.5f)*0.003f, 1+fxrand()*1.4f, fxrand()*SPTAU, 0); break;
    case FX_BUTTERFLY:     for (int i=0;i<12;i++)  add(fxrand(),fxrand(),0,0, 8+fxrand()*7, fxrand()*SPTAU, 0.6f+fxrand()*0.8f);
                           for (int i=0;i<50;i++)  add(fxrand(),fxrand(),(fxrand()-0.5f)*0.01f,-0.006f-fxrand()*0.01f, 1+fxrand()*1.5f, fxrand()*SPTAU, 0); break;
    case FX_RAIN:          for (int i=0;i<170;i++) add(fxrand(),fxrand(),0,0.9f+fxrand()*0.7f, 6+fxrand()*10, fxrand()*SPTAU, 0); break;
    case FX_LIGHTNING:     for (int i=0;i<40;i++)  add(fxrand(),fxrand(),(fxrand()-0.5f)*0.02f,(fxrand()-0.5f)*0.02f, 1+fxrand()*1.5f, fxrand()*SPTAU, 0); break;
    case FX_FIRE:          for (int i=0;i<150;i++) add(fxrand(),1.05f, (fxrand()-0.5f)*0.03f,-0.25f-fxrand()*0.45f, 2+fxrand()*4, fxrand()*SPTAU, fxrand()); break;
    case FX_SNOW:          for (int i=0;i<150;i++) add(fxrand(),fxrand(),0,0.02f+fxrand()*0.04f, 1+fxrand()*2.6f, fxrand()*SPTAU, fxrand()); break;
    case FX_OCEAN:         for (int i=0;i<70;i++)  add(fxrand(),1.0f+fxrand()*0.2f,0,-0.02f-fxrand()*0.05f, 2+fxrand()*5, fxrand()*SPTAU, fxrand()); break;
    case FX_VIOLET:        for (int i=0;i<80;i++)  add(fxrand(),fxrand(),(fxrand()-0.5f)*0.014f,(fxrand()-0.5f)*0.014f, 1.2f+fxrand()*2.0f, fxrand()*SPTAU, fxrand()); break;
    case FX_MATRIX:        for (int i=0;i<64;i++)  add((float)i/64.0f+0.008f*fxrand(), fxrand(), 0, 0.10f+fxrand()*0.16f, 12+fxrand()*10, fxrand()*SPTAU, 0); break;
    case FX_SAKURA:        for (int i=0;i<95;i++)  add(fxrand(),fxrand(),0,0.03f+fxrand()*0.05f, 3+fxrand()*4, fxrand()*SPTAU, fxrand()); break;
    case FX_ECLIPSE:       for (int i=0;i<60;i++)  add(fxrand(),fxrand(),(fxrand()-0.5f)*0.008f,(fxrand()-0.5f)*0.008f, 0.7f+fxrand()*1.3f, fxrand()*SPTAU, fxrand()); break;
    case FX_AURORA:        for (int i=0;i<70;i++)  add(fxrand(),fxrand(),(fxrand()-0.5f)*0.002f,(fxrand()-0.5f)*0.002f, 0.8f+fxrand()*1.4f, fxrand()*SPTAU, fxrand()); break;
    default: break;
    }
}

// ------------------------------------------------------------------ helpers
static ImVec2 FXmap(const FxP& p, const ImVec2& mn, const ImVec2& sz){
    return ImVec2(mn.x + p.n.x * sz.x, mn.y + p.n.y * sz.y);
}
static void WrapN(FxP& p){
    if (p.n.x < -0.05f) p.n.x += 1.10f; if (p.n.x > 1.05f) p.n.x -= 1.10f;
    if (p.n.y < -0.10f) p.n.y += 1.20f; if (p.n.y > 1.10f) p.n.y -= 1.20f;
}
static void DrawGlowDot(ImDrawList* dl, const ImVec2& c, float r, ImU32 col, bool glow){
    dl->AddCircleFilled(c, r, col);
    if (glow) {
        ImU32 g = spFadeAlpha(col, 0.25f);
        dl->AddCircle(c, r*2.2f, g, 12, 1.4f);
        dl->AddCircle(c, r*3.6f, spFadeAlpha(col, 0.10f), 12, 1.2f);
    }
}
// small stylized spider used by the Spider theme
static void DrawMiniSpider(ImDrawList* dl, const ImVec2& c, float s, float t, float ph, ImU32 body, ImU32 leg){
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 0; i < 4; ++i) {
            float baseA = -0.55f + i * 0.42f;
            float wob   = sinf(t * 6.0f + ph + i * 1.7f) * 0.18f;
            float a1 = baseA + wob, a2 = baseA * 0.45f - 0.5f + wob * 0.5f;
            float l1 = s * 2.1f, l2 = s * 1.9f;
            ImVec2 j1(c.x + side * cosf(a1) * l1, c.y - fabsf(sinf(a1)) * l1 * 0.9f - s * 0.4f);
            j1.y = c.y - (0.35f + 0.30f * (i / 3.0f)) * l1 * 0.8f;
            ImVec2 tip(j1.x + side * cosf(a2) * l2, j1.y + sinf(fabsf(a2)) * l2 * 0.8f + s * 0.9f);
            dl->AddLine(c, j1, leg, 1.4f);
            dl->AddLine(j1, tip, leg, 1.1f);
        }
    }
    dl->AddCircleFilled(ImVec2(c.x, c.y - s*0.55f), s*0.62f, body);
    dl->AddCircleFilled(ImVec2(c.x, c.y + s*0.55f), s*0.85f, body);
}

// ------------------------------------------------------------- scene render
static void FxDrawScene(ImDrawList* dl, const ImVec2& mn, const ImVec2& mx,
                        int fx, float t, float dt, float density, bool particlesOn,
                        bool glow, bool perf)
{
    const ImVec2 sz(mx.x - mn.x, mx.y - mn.y);
    const float  aspect = sz.x / (sz.y > 1 ? sz.y : 1);
    if (fx != g_fxCur || g_fn == 0) FxInit(fx);

    // count scaling: particleAmount slider + perf mode + preview scale
    float dmul = spClampf(density, 0.15f, 2.0f) * (perf ? 0.45f : 1.0f);
    int   live = (int)(g_fn * spClampf(dmul, 0.1f, 1.0f));
    float dtc  = dt * spClampf(g_ui.animSpeed, 0.05f, 3.0f);
    const ImVec4& A  = g_palCur.accent;
    const ImVec4& A2 = g_palCur.accent2;

    switch (fx) {
    case FX_WEBS: {
        // corner webs with breathing radius
        for (int corner = 0; corner < 2; ++corner) {
            ImVec2 o(corner ? mx.x : mn.x, mn.y);
            float breathe = 1.0f + 0.04f * sinf(t * 1.3f + corner * 2.1f);
            for (int ring = 1; ring <= 5; ++ring) {
                float rr = ring * sz.x * 0.075f * breathe;
                dl->PathClear();
                for (int a = 0; a <= 8; ++a) {
                    float ang = (corner ? 0.5f : 0.0f) * 3.14159f + a * (3.14159f * 0.5f) / 8.0f;
                    dl->PathLineTo(ImVec2(o.x + cosf(ang)*rr, o.y + sinf(ang)*rr));
                }
                dl->PathStroke(spCol(A, 0.16f), 0, 1.2f);
            }
            for (int a = 0; a <= 4; ++a) {
                float ang = (corner ? 0.5f : 0.0f) * 3.14159f + a * (3.14159f * 0.5f) / 4.0f;
                dl->AddLine(o, ImVec2(o.x + cosf(ang)*sz.x*0.40f*breathe, o.y + sinf(ang)*sz.x*0.40f*breathe), spCol(A, 0.13f), 1.2f);
            }
        }
        if (!particlesOn) break;
        for (int i = 0; i < live; ++i) {
            FxP& p = g_fp[i];
            p.n.x += p.v.x * dtc; p.n.y += p.v.y * dtc; WrapN(p);
            ImVec2 pos = FXmap(p, mn, sz);
            DrawMiniSpider(dl, pos, p.size, t, p.phase, spCol(A, 0.85f), spCol(A, 0.45f));
            if (glow && !perf) dl->AddCircle(pos, p.size * 2.6f, spFadeAlpha(spCol(A), 0.10f + 0.06f*sinf(t*3+p.phase)), 10, 1.2f);
        }
    } break;

    case FX_CONSTELLATION: {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 m(io.MousePos.x, io.MousePos.y);
        for (int pass = 0; pass < 2; ++pass) {
            for (int i = 0; i < live; ++i) {
                FxP& p = g_fp[i];
                if (pass == 0) { p.n.x += p.v.x*dtc*(0.5f+p.aux); p.n.y += p.v.y*dtc*(0.5f+p.aux); WrapN(p); continue; }
                ImVec2 pi = FXmap(p, mn, sz);
                float tw = 0.55f + 0.45f * sinf(t * (1.2f + p.aux) + p.phase);
                // links to nearby stars
                for (int j = i+1; j < live; ++j) {
                    FxP& q = g_fp[j];
                    float dx = (q.n.x - p.n.x) * sz.x, dy = (q.n.y - p.n.y) * sz.y;
                    float d2 = dx*dx + dy*dy;
                    if (d2 < 12100.0f) {                       // ~110 px
                        float a = (1.0f - sqrtf(d2)/110.0f) * 0.38f * tw;
                        ImVec2 pj = FXmap(q, mn, sz);
                        dl->AddLine(pi, pj, spCol(A, a), 1.1f);
                    }
                }
                float mdx = pi.x-m.x, mdy = pi.y-m.y;
                float md2 = mdx*mdx + mdy*mdy;
                float boost = (md2 < 22500.0f) ? (1.0f - sqrtf(md2)/150.0f) : 0.0f;
                ImU32 sc = spCol(boost > 0 ? A : ImVec4(0.85f,0.90f,1.0f,1.0f), 0.55f*tw + boost*0.5f);
                DrawGlowDot(dl, pi, p.size, sc, glow && !perf);
            }
        }
    } break;

    case FX_NEBULA: {
        int blobs = live > 90 ? 26 : (int)(live * 0.22f);
        for (int i = 0; i < blobs; ++i) {
            FxP& p = g_fp[i];
            p.n.x += p.v.x*dtc; p.n.y += p.v.y*dtc; WrapN(p);
            ImVec2 c = FXmap(p, mn, sz);
            float hue = fmodf(p.aux + t*0.01f, 1.0f);
            float r,g,b; ImGui::ColorConvertHSVtoRGB(hue, 0.55f, 1.0f, r,g,b);
            float pulse = 0.75f + 0.25f*sinf(t*0.5f + p.phase);
            dl->AddCircleFilled(c, p.size*pulse, IM_COL32((int)(r*255),(int)(g*255),(int)(b*255),14));
            dl->AddCircleFilled(c, p.size*pulse*0.55f, IM_COL32((int)(r*255),(int)(g*255),(int)(b*255),18));
        }
        if (!particlesOn) break;
        for (int i = 26; i < live; ++i) {
            FxP& p = g_fp[i];
            p.n.x += p.v.x*dtc; p.n.y += p.v.y*dtc; WrapN(p);
            float tw = 0.5f + 0.5f*sinf(t*1.4f + p.phase);
            DrawGlowDot(dl, FXmap(p, mn, sz), p.size, spCol(ImVec4(0.9f,0.92f,1.0f,1), 0.65f*tw), false);
        }
    } break;

    case FX_BUTTERFLY: {
        int nb = live > 50 ? 12 : (live/4 + 1);
        for (int i = 0; i < nb; ++i) {
            FxP& p = g_fp[i];
            float sp = p.aux;
            float nx = p.n.x + 0.13f*sinf(t*0.21f*sp + p.phase);
            float ny = p.n.y + 0.10f*sinf(t*0.33f*sp*1.7f + p.phase*2.0f);
            ImVec2 c(mn.x + nx*sz.x, mn.y + ny*sz.y);
            float flap = sinf(t*7.0f*sp + p.phase);
            float w = p.size * (0.35f + 0.65f*fabsf(flap));
            float dir = (cosf(t*0.21f*sp + p.phase) >= 0) ? 1.0f : -1.0f;
            ImU32 wc = spCol(A2, 0.75f), wb = spCol(A, 0.95f);
            if (glow && !perf) {
                dl->AddCircleFilled(c, p.size*1.9f, spFadeAlpha(wc, 0.08f));
            }
            // wings as filled triangles
            dl->AddTriangleFilled(c, ImVec2(c.x - dir*w, c.y - w*0.85f), ImVec2(c.x - dir*w*0.72f, c.y + w*0.28f), wc);
            dl->AddTriangleFilled(c, ImVec2(c.x + dir*w, c.y - w*0.85f), ImVec2(c.x + dir*w*0.72f, c.y + w*0.28f), wc);
            dl->AddTriangleFilled(c, ImVec2(c.x - dir*w*0.66f, c.y + w*0.1f), ImVec2(c.x - dir*w*0.5f, c.y + w*0.95f), spFadeAlpha(wc,0.7f));
            dl->AddTriangleFilled(c, ImVec2(c.x + dir*w*0.66f, c.y + w*0.1f), ImVec2(c.x + dir*w*0.5f, c.y + w*0.95f), spFadeAlpha(wc,0.7f));
            dl->AddLine(ImVec2(c.x, c.y - w*0.7f), ImVec2(c.x, c.y + w*0.9f), wb, 2.0f);
        }
        if (!particlesOn) break;
        for (int i = 12; i < live; ++i) {
            FxP& p = g_fp[i];
            p.n.x += p.v.x*dtc; p.n.y += p.v.y*dtc; WrapN(p);
            dl->AddCircleFilled(FXmap(p, mn, sz), p.size, spFadeAlpha(spCol(A2), 0.5f));
        }
    } break;

    case FX_RAIN: {
        if (!particlesOn) break;
        float floorY = mx.y - 4.0f;
        for (int i = 0; i < live; ++i) {
            FxP& p = g_fp[i];
            p.n.y += p.v.y * dtc * (0.5f + p.aux*0); // v.y holds speed
            p.n.x += 0.06f * dtc;                     // slight wind
            if (p.n.y > 1.0f) {
                if ((int)(p.aux*47) % 3 == 0 && g_spl[(i % 48)].ttl <= 0) { g_spl[i % 48].x = p.n.x; g_spl[i % 48].ttl = 0.35f; }
                p.n.y -= 1.05f + 0.2f*fxrand();
            }
            ImVec2 a(mn.x + p.n.x*sz.x, mn.y + p.n.y*sz.y - p.size);
            ImVec2 b(a.x + 2.0f, a.y + p.size);
            dl->AddLine(a, b, spCol(ImVec4(0.62f,0.78f,0.92f,1), 0.34f), 1.2f);
        }
        for (auto& s : g_spl) {
            if (s.ttl <= 0) continue;
            s.ttl -= dtc;
            float k = 1.0f - spClampf(s.ttl/0.35f, 0.f, 1.f);
            dl->AddCircle(ImVec2(mn.x + s.x*sz.x, floorY), 2.0f + 7.0f*k, spCol(A, 0.35f*(1.0f-k)), 10, 1.1f);
        }
    } break;

    case FX_LIGHTNING: {
        g_boltTimer -= dtc;
        g_flash    -= dtc * 3.0f;
        if (g_boltTimer <= 0) {
            g_boltTimer = 1.6f + fxrand()*3.4f;
            g_flash = 0.85f + fxrand()*0.15f;
            g_bn = 12 + (int)(fxrand()*8);
            float bx = mn.x + sz.x*(0.15f + fxrand()*0.7f);
            for (int i = 0; i < g_bn; ++i) {
                float fy = (float)i/(g_bn-1);
                g_bolt[i] = ImVec2(bx + sinf(fy*9.0f + fxrand()*2.0f)*sz.x*0.03f*(1-fy) + (fxrand()-0.5f)*14.0f,
                                   mn.y + fy*sz.y*(0.55f+0.25f*fxrand()));
            }
            g_brn = 5 + (int)(fxrand()*4);
            ImVec2 jp = g_bolt[g_bn/2];
            for (int i = 0; i < g_brn; ++i)
                g_branch[i] = ImVec2(jp.x + (fxrand()-0.5f)*70.0f, jp.y + (float)i*sz.y*0.03f);
        }
        if (g_flash > 0) {
            ImU32 boltCol = spCol(A, spClampf(g_flash,0,1));
            for (int w = 4; w >= 1; --w)
                dl->AddPolyline(g_bolt, g_bn, spFadeAlpha(boltCol, 0.30f*w/4.0f), false, (float)w);
            if (g_brn > 1) dl->AddPolyline(g_branch, g_brn, spFadeAlpha(boltCol, 0.55f), false, 1.5f);
            dl->AddRectFilled(mn, mx, IM_COL32(180,200,255,(int)(spClampf(g_flash,0,1)*26)));
        }
        if (!particlesOn) break;
        for (int i = 0; i < live; ++i) {
            FxP& p = g_fp[i];
            p.n.x += p.v.x*dtc; p.n.y += p.v.y*dtc; WrapN(p);
            float tw = 0.4f + 0.6f*sinf(t*3.0f+p.phase);
            DrawGlowDot(dl, FXmap(p, mn, sz), p.size, spFadeAlpha(spCol(A2), 0.45f*tw + g_flash*0.4f), glow && !perf);
        }
    } break;

    case FX_FIRE: {
        if (!particlesOn) break;
        // heat floor glow
        float pulse = 0.75f + 0.25f*sinf(t*2.2f);
        dl->AddRectFilled(ImVec2(mn.x, mx.y-70), mx, spFadeAlpha(spCol(ImVec4(1.0f,0.42f,0.10f,1)), 0.10f*pulse));
        dl->AddRectFilled(ImVec2(mn.x, mx.y-36), mx, spFadeAlpha(spCol(ImVec4(1.0f,0.60f,0.16f,1)), 0.13f*pulse));
        for (int i = 0; i < live; ++i) {
            FxP& p = g_fp[i];
            p.n.y += p.v.y*dtc;
            p.n.x += sinf(t*3.0f + p.phase)*0.0009f;
            float lifeN = spClampf((p.n.y - 1.0f) / -0.9f + p.aux*0.2f, 0.f, 1.f);
            if (p.n.y < -0.05f) { p.n.y = 1.02f + fxrand()*0.1f; p.n.x = fxrand(); }
            ImVec2 c(mn.x + p.n.x*sz.x, mn.y + p.n.y*sz.y);
            float fade = spClampf(p.n.y, 0.f, 1.f);
            float r,g,b; ImGui::ColorConvertHSVtoRGB(fmodf(0.06f - lifeN*0.05f,1.0f), 0.95f, 1.0f - lifeN*0.35f, r,g,b);
            dl->AddCircleFilled(c, p.size*(0.5f+fade*0.8f), IM_COL32((int)(r*255),(int)(g*255),(int)(b*255),(int)(150*fade)));
        }
    } break;

    case FX_SNOW: {
        if (!particlesOn) break;
        for (int i = 0; i < live; ++i) {
            FxP& p = g_fp[i];
            p.n.y += p.v.y*dtc;
            p.n.x += sinf(t*0.9f + p.phase)*0.0012f;
            if (p.n.y > 1.05f) { p.n.y = -0.05f; p.n.x = fxrand(); }
            ImVec2 c = FXmap(p, mn, sz);
            float tw = 0.6f + 0.4f*sinf(t*2.0f + p.phase);
            ImU32 sc = spCol(ImVec4(0.88f,0.94f,1.0f,1), 0.65f*tw);
            if (p.size > 2.8f) { dl->AddCircle(c, p.size, sc, 6, 1.0f); dl->AddCircleFilled(c, 1.2f, sc); }
            else DrawGlowDot(dl, c, p.size, sc, false);
        }
        // frost tint
        dl->AddRectFilled(mn, mx, IM_COL32(160,205,255,10));
    } break;

    case FX_OCEAN: {
        // layered sine waves
        for (int wv = 0; wv < 3; ++wv) {
            dl->PathClear();
            float base = mx.y - sz.y*(0.10f + 0.09f*wv);
            for (float x = mn.x; x <= mx.x+8; x += 10.0f) {
                float y = base + sinf(x*0.014f + t*(0.9f + 0.25f*wv) + wv*1.7f)*7.0f;
                dl->PathLineTo(ImVec2(x, y));
            }
            dl->PathLineTo(ImVec2(mx.x, mx.y)); dl->PathLineTo(ImVec2(mn.x, mx.y));
            dl->PathFillConvex(spCol(A, 0.055f - 0.012f*wv));
        }
        if (!particlesOn) break;
        for (int i = 0; i < live; ++i) {
            FxP& p = g_fp[i];
            p.n.y += p.v.y*dtc;
            p.n.x += sinf(t*2.4f + p.phase)*0.0011f;
            if (p.n.y < -0.06f) { p.n.y = 1.04f; p.n.x = fxrand(); }
            ImVec2 c(mn.x + p.n.x*sz.x, mn.y + p.n.y*sz.y);
            dl->AddCircle(c, p.size, spCol(ImVec4(0.55f,0.92f,1.0f,1), 0.45f), 10, 1.1f);
            if (glow && !perf && p.size > 4)
                dl->AddCircle(c, p.size*1.9f, spFadeAlpha(spCol(ImVec4(0.55f,0.92f,1.0f,1)), 0.12f), 10, 1.0f);
        }
    } break;

    case FX_VIOLET: {
        for (int pass = 0; pass < 2; ++pass) {
            for (int i = 0; i < live; ++i) {
                FxP& p = g_fp[i];
                if (pass == 0) { p.n.x += p.v.x*dtc; p.n.y += p.v.y*dtc; WrapN(p); continue; }
                ImVec2 pi = FXmap(p, mn, sz);
                for (int j = i+1; j < live; ++j) {
                    FxP& q = g_fp[j];
                    float dx=(q.n.x-p.n.x)*sz.x, dy=(q.n.y-p.n.y)*sz.y, d2=dx*dx+dy*dy;
                    if (d2 < 6400.0f)
                        dl->AddLine(pi, FXmap(q, mn, sz), spCol(A, (1.0f-sqrtf(d2)/80.0f)*0.30f), 1.0f);
                }
                float tw = 0.5f+0.5f*sinf(t*2.2f+p.phase);
                DrawGlowDot(dl, pi, p.size, spCol(i%3 ? A : A2, 0.55f*tw), glow && !perf);
            }
        }
    } break;

    case FX_MATRIX: {
        static const char* GLYPHS[] = {"0","1","{","}","<",">","/","\\","$","#","%","&","*","+","=","?",";",":"};
        int cols = live; if (cols > 64) cols = 64;
        for (int i = 0; i < cols; ++i) {
            FxP& p = g_fp[i];
            p.n.y += p.v.y * dtc * (aspect > 1 ? 1.0f : 0.6f);
            if (p.n.y > 1.15f) { p.n.y = -0.15f - fxrand()*0.3f; p.aux = fxrand(); }
            float x = mn.x + p.n.x*sz.x;
            float headY = mn.y + p.n.y*sz.y;
            int len = 6 + (int)(p.size);
            ImFont* fnt = ImGui::GetFont();
            float fs = ImGui::GetFontSize()*0.78f;
            for (int k = 0; k < len; ++k) {
                float yk = headY - k*fs;
                if (yk < mn.y || yk > mx.y) continue;
                float a = (1.0f - (float)k/len);
                a *= a;
                const char* gl = GLYPHS[(i*31 + k*17 + (int)(t*3.0f)) % (sizeof(GLYPHS)/sizeof(GLYPHS[0]))];
                ImU32 colr = (k == 0) ? IM_COL32(210,255,220,(int)(235*a))
                                      : spFadeAlpha(spCol(ImVec4(0.16f,0.95f,0.35f,1)), 0.75f*a);
                dl->AddText(fnt, fs, ImVec2(x, yk), colr, gl);
            }
        }
    } break;

    case FX_SAKURA: {
        if (!particlesOn) break;
        for (int i = 0; i < live; ++i) {
            FxP& p = g_fp[i];
            p.n.y += p.v.y*dtc;
            p.n.x += sinf(t*1.1f + p.phase)*0.0016f;
            if (p.n.y > 1.05f) { p.n.y = -0.05f; p.n.x = fxrand(); }
            ImVec2 c(mn.x + p.n.x*sz.x + sinf(t*2.0f+p.phase)*6.0f, mn.y + p.n.y*sz.y);
            float rot = t*1.4f + p.phase;
            float r = p.size;
            ImVec2 pts[5];
            for (int k = 0; k < 5; ++k) {
                float a = rot + k*SPTAU/5.0f;
                pts[k] = ImVec2(c.x + cosf(a)*r, c.y + sinf(a)*r*0.62f);
            }
            float pink = 0.85f + 0.15f*sinf(p.phase*3.f);
            dl->AddCircleFilled(c, r*0.55f, spCol(ImVec4(1.0f,0.72f*pink,0.82f,1), 0.65f));
            dl->AddCircle(c, r*0.55f, spCol(ImVec4(1.0f,0.55f,0.70f,1), 0.35f), 10, 1.0f);
            (void)pts;
        }
    } break;

    case FX_ECLIPSE: {
        ImVec2 cc(mn.x + sz.x*0.76f, mn.y + sz.y*0.24f);
        float R = sz.y*0.20f;
        float pulse = 0.8f + 0.2f*sinf(t*0.9f);
        if (glow) {
            for (int ring = 4; ring >= 1; --ring)
                dl->AddCircle(cc, R + ring*7.0f*pulse, spFadeAlpha(spCol(ImVec4(1.0f,0.30f,0.18f,1)), 0.035f*ring), 48, 5.0f);
        }
        dl->AddCircleFilled(cc, R, IM_COL32(12,8,10,255));
        dl->AddCircle(cc, R+1.0f, spCol(ImVec4(1.0f,0.42f,0.26f,1), 0.85f), 64, 2.4f);
        if (!particlesOn) break;
        for (int i = 0; i < live; ++i) {
            FxP& p = g_fp[i];
            p.n.x += p.v.x*dtc; p.n.y += p.v.y*dtc; WrapN(p);
            float tw = 0.4f + 0.6f*sinf(t*1.7f+p.phase);
            DrawGlowDot(dl, FXmap(p, mn, sz), p.size, spFadeAlpha(spCol(ImVec4(0.95f,0.75f,0.70f,1)), 0.40f*tw), false);
        }
    } break;

    case FX_AURORA: {
        for (int band = 0; band < 4; ++band) {
            float hueB = fmodf(band*0.16f + t*0.02f, 1.0f);
            for (float x = mn.x; x < mx.x; x += 26.0f) {
                float hgt = sz.y*(0.16f + 0.11f*sinf(x*0.006f + t*0.5f + band*1.9f)
                                       + 0.06f*sinf(x*0.017f - t*0.33f + band));
                float yTop = mn.y + sz.y*0.06f + 14.0f*sinf(x*0.004f + t*0.21f + band*2.4f);
                float r,g,b; ImGui::ColorConvertHSVtoRGB(hueB, 0.60f, 1.0f, r,g,b);
                int alpha = 13;
                dl->AddRectFilledMultiColor(ImVec2(x,yTop), ImVec2(x+26.0f,yTop+hgt),
                    IM_COL32((int)(r*255),(int)(g*255),(int)(b*255),alpha),
                    IM_COL32((int)(r*255),(int)(g*255),(int)(b*255),alpha),
                    IM_COL32((int)(r*255),(int)(g*255),(int)(b*255),0),
                    IM_COL32((int)(r*255),(int)(g*255),(int)(b*255),0));
            }
        }
        if (!particlesOn) break;
        for (int i = 0; i < live; ++i) {
            FxP& p = g_fp[i];
            p.n.x += p.v.x*dtc; p.n.y += p.v.y*dtc; WrapN(p);
            float tw = 0.5f + 0.5f*sinf(t*2.0f+p.phase);
            DrawGlowDot(dl, FXmap(p, mn, sz), p.size, spCol(ImVec4(0.88f,0.97f,1.0f,1), 0.5f*tw), false);
        }
    } break;

    default: break;
    }
}

// Convenience: full themed backdrop used by the main window.
static void FxDrawBackdrop(ImDrawList* dl, const ImVec2& mn, const ImVec2& mx,
                           float t, float dt){
    ThemeDef& th = g_Themes[g_ui.themeIndex];
    // vertical gradient wash
    ImVec4 top = Dimmed(g_palCur.bg, 1.55f);
    dl->AddRectFilledMultiColor(mn, mx, spCol(top), spCol(top), spCol(g_palCur.bg), spCol(g_palCur.bg));
    if (g_ui.particles || th.fx == FX_MATRIX)
        FxDrawScene(dl, mn, mx, th.fx, t, dt, g_ui.particleAmount, g_ui.particles || th.fx == FX_MATRIX, g_ui.glow, g_ui.perfMode);
    // dim overlay
    if (g_ui.bgOpacity > 0.01f) {
        ImVec4 d(0,0,0, spClampf(g_ui.bgOpacity, 0.f, 0.9f));
        dl->AddRectFilled(mn, mx, spCol(d));
    }
}
