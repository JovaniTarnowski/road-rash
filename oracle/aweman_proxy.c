/* Oracle hook DLL for ROADRASH.EXE (SHA-256 73d7f974...). Replaces AWEMAN32.DLL.
   The game statically imports AWEMAN32.dll ordinal 2; this DLL forwards it to the
   renamed original (AWEMAN_REAL.DLL) and patches call sites to dump game state. */
#include <stdint.h>
#include <string.h>

/* Verify an E8 rel32 call at site targeting expected_target, then redirect it to hook. */
static int rr_patch_bytes(uint8_t* site, uint32_t expected_target, void* hook) {
    int32_t rel;
    if (site[0] != 0xE8) return 0;
    memcpy(&rel, site + 1, 4);
    if ((uint32_t)((uintptr_t)(site + 5) + rel) != expected_target) return 0;
    rel = (int32_t)((uintptr_t)hook - (uintptr_t)(site + 5));
    memcpy(site + 1, &rel, 4);
    return 1;
}

#ifdef RR_PATCH_NO_WINDOWS
int rr_patch_call(uint8_t* site, uint32_t expected_target, void* hook) { return rr_patch_bytes(site, expected_target, hook); }
#else
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static FILE* g_out;
static char g_scenario[256] = "unnamed";

/* Ordinal 2 of the original AWEMAN32.DLL, loaded from AWEMAN_REAL.DLL.
   GNU ld cannot forward by ordinal, so export a jump trampoline instead. */
void* g_real2;
__attribute__((naked)) void Forward2(void) { __asm__("jmp *_g_real2"); }

int rr_patch_call(uint8_t* site, uint32_t expected_target, void* hook) {
    DWORD old;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old)) return 0;
    int ok = rr_patch_bytes(site, expected_target, hook);
    VirtualProtect(site, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    return ok;
}

static void env(const char* name, char* buf, DWORD size, const char* fallback) {
    DWORD count = GetEnvironmentVariableA(name, buf, size);
    if (!count || count >= size) { strncpy(buf, fallback, size - 1); buf[size - 1] = 0; }
}

void rr_log(const char* key, long long value) { if (g_out) fprintf(g_out, "%s\t%lld\n", key, value); }

typedef void (__cdecl *rr_frame_fn)(void);
static rr_frame_fn const orig_frame = (rr_frame_fn)0x004439f0;
static uint32_t g_frame;
static int g_quit_after = -1;
static int g_pixels, g_pixel_every=30, g_pixel_written;
static int g_pixel_width, g_pixel_height, g_pixel_pitch;
static char g_output[MAX_PATH];
#define RD32(a) (*(volatile int32_t*)(uintptr_t)(a))
#define RD8(a) (*(volatile uint8_t*)(uintptr_t)(a))

static int readable(uintptr_t p, size_t size) {
    MEMORY_BASIC_INFORMATION info;
    if (!p || !VirtualQuery((void*)p, &info, sizeof info)) return 0;
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
    return size <= (uintptr_t)info.BaseAddress + info.RegionSize - p;
}

// 0x0044d300 locks the surface while drawing and stores its view-origin pixel
// pointer/pitch at 0x004b7ec0/0x004b7ec4. Capture before 0x0044c780 unlocks it.
static void __cdecl hook_draw(void) {
    ((rr_frame_fn)0x00450420)();
    g_pixel_written=0;
    if (!g_pixels || !g_out || g_frame % (uint32_t)g_pixel_every || !RD8(0x00475620)) return;
    const uintptr_t base=(uint32_t)RD32(0x004b7ec0);
    const int width=RD32(0x0047539c), height=RD32(0x004753a0), pitch=RD32(0x004b7ec4);
    if (width<1 || width>1280 || height<1 || height>960 || pitch<width || pitch>16384) return;
    if (!readable(base,(size_t)(height-1)*pitch+width)) return;
    char path[MAX_PATH];
    const char* slash=strrchr(g_output,'\\');
    const size_t prefix=slash ? (size_t)(slash-g_output)+1 : 0;
    if (prefix>sizeof path-64) return;
    memcpy(path,g_output,prefix);
    int count=snprintf(path+prefix,sizeof path-prefix,"%s-f%u.raw",g_scenario,g_frame);
    if (count<0 || (size_t)count>=sizeof path-prefix) return;
    FILE* pixels=fopen(path,"wb");
    if (!pixels) return;
    int ok=1;
    for (int row=0;row<height;++row)
        if (fwrite((void*)(base+(size_t)row*pitch),1,width,pixels)!=(size_t)width) { ok=0; break; }
    if (fclose(pixels)) ok=0;
    if (ok) {
        g_pixel_written=1;
        g_pixel_width=width; g_pixel_height=height; g_pixel_pitch=pitch;
    }
}

static void dump_frame(uint32_t site) {
    char key[96];
    fprintf(g_out, "frame\t%u\n", g_frame);
    rr_log("site", site);
    rr_log("course", RD8(0x00467a6d));
    rr_log("view.high", RD8(0x00475380));
    rr_log("camera.position", RD32(0x004bc7e4));
    rr_log("camera.x", RD32(0x004bc7d8));
    rr_log("camera.height", RD32(0x004bc7dc));
    rr_log("camera.half", RD8(0x00473498));
    rr_log("view.coordinate_shift", RD32(0x00481fa8));
    if (g_pixel_written) {
        rr_log("pixels.file",g_frame);
        rr_log("pixels.width",g_pixel_width); rr_log("pixels.height",g_pixel_height);
        rr_log("pixels.pitch",g_pixel_pitch);
        for (int i=0;i<256;++i) {
            const uintptr_t p=0x0049e138+i*4;
            snprintf(key,sizeof key,"palette.%d",i);
            rr_log(key,((uint32_t)RD8(p)<<16)|((uint32_t)RD8(p+1)<<8)|RD8(p+2));
        }
    }
    for (int s = 0; s < 33; ++s) {
        uintptr_t entry = 0x004bc800 + (uintptr_t)s * 548;
        int kind = RD32(entry + 4);
        snprintf(key, sizeof key, "slice.%d.kind", s); rr_log(key, kind);
        snprintf(key, sizeof key, "slice.%d.sample", s); rr_log(key, RD32(entry + 8));
        for (int r = 0; r < kind && r < 2; ++r) {
            uintptr_t p = (uint32_t)RD32(entry + (5 + r) * 4);
            if (!readable(p, 0x104)) continue;
            static const struct { const char* name; int off, byte; } fields[] = {
                {"depth",0x18,0},{"scale",0x20,0},{"x",0x10,0},{"y",0x14,0},{"centre",0x48,0},
                {"screen_y",0x4c,0},{"left",0x50,0},{"right",0x58,0},{"lane_left",0x40,0},{"lane_right",0x44,0},
                {"shoulder_left",0x60,0},{"shoulder_right",0x68,0},{"height_left_row",0x54,0},
                {"height_right_row",0x5c,0},{"visible",0x100,1}
            };
            for (unsigned f = 0; f < sizeof fields / sizeof fields[0]; ++f) {
                snprintf(key, sizeof key, "slice.%d.road.%d.%s", s, r, fields[f].name);
                rr_log(key, fields[f].byte ? RD8(p + fields[f].off) : RD32(p + fields[f].off));
            }
            uintptr_t sub = (uint32_t)RD32(p + 0x0c);
            snprintf(key, sizeof key, "slice.%d.road.%d.type", s, r);
            rr_log(key, readable(sub, 0x3d) ? RD8(sub + 0x3c) : -1);
            if (readable(sub,0x164)) {
                for (int offset=0xf0;offset<=0x160;offset+=4) {
                    snprintf(key,sizeof key,"slice.%d.road.%d.sub.%d",s,r,offset);
                    rr_log(key,RD32(sub+offset));
                }
            }
        }
    }
    fflush(g_out);
}

#define FRAME_HOOK(n, site) \
    static void __cdecl hook_frame_##n(void) { \
        orig_frame(); \
        if (g_out) dump_frame(site); \
        ++g_frame; \
        if (g_quit_after > 0 && g_frame >= (uint32_t)g_quit_after) { \
            if (g_out) { fputs("end\n", g_out); fclose(g_out); g_out = NULL; } \
            ExitProcess(0); \
        } \
    }
FRAME_HOOK(0, 0x004163fa)
FRAME_HOOK(1, 0x00416406)
FRAME_HOOK(2, 0x0041656d)
FRAME_HOOK(3, 0x00443249)

static int rr_install_hooks(void) {
    char buf[32];
    if (GetEnvironmentVariableA("RR_ORACLE_FRAMES", buf, sizeof buf)) g_quit_after = atoi(buf);
    if (GetEnvironmentVariableA("RR_ORACLE_PIXELS",buf,sizeof buf)) g_pixels=atoi(buf)!=0;
    if (GetEnvironmentVariableA("RR_ORACLE_SHOT_EVERY",buf,sizeof buf) && atoi(buf)>0) g_pixel_every=atoi(buf);
    static const struct { uint32_t site; void* hook; uint8_t bytes[5]; } calls[] = {
        {0x004163fa, hook_frame_0, {0xe8,0xf1,0xd5,0x02,0}},
        {0x00416406, hook_frame_1, {0xe8,0xe5,0xd5,0x02,0}},
        {0x0041656d, hook_frame_2, {0xe8,0x7e,0xd4,0x02,0}},
        {0x00443249, hook_frame_3, {0xe8,0xa2,0x07,0,0}},
        {0x004439f5, hook_draw, {0xe8,0x26,0xca,0,0}}
    };
    const unsigned count=sizeof calls/sizeof calls[0];
    // Validate ALL sites before changing ANY. A foreign binary is never patched.
    for (unsigned i = 0; i < count; ++i) {
        if (memcmp((void*)(uintptr_t)calls[i].site, calls[i].bytes, 5)) {
            if (g_out) fprintf(g_out, "# patch verification failed at %08x\n", calls[i].site);
            return 0;
        }
    }
    for (unsigned i = 0; i < count; ++i) {
        const uint32_t target=calls[i].site==0x004439f5 ? 0x00450420 : 0x004439f0;
        if (!rr_patch_call((uint8_t*)(uintptr_t)calls[i].site, target, calls[i].hook)) {
            // Restore any earlier call sites on a protection failure.
            for (unsigned j = 0; j < i; ++j) {
                DWORD old;
                if (VirtualProtect((void*)(uintptr_t)calls[j].site, 5, PAGE_EXECUTE_READWRITE, &old)) {
                    memcpy((void*)(uintptr_t)calls[j].site, calls[j].bytes, 5);
                    VirtualProtect((void*)(uintptr_t)calls[j].site, 5, old, &old);
                    FlushInstructionCache(GetCurrentProcess(), (void*)(uintptr_t)calls[j].site, 5);
                }
            }
            if (g_out) fprintf(g_out, "# patch installation failed at %08x\n", calls[i].site);
            return 0;
        }
    }
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)inst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        char path[MAX_PATH];
        env("RR_ORACLE_OUT", path, sizeof path, "C:\\oracle.tsv");
        memcpy(g_output,path,sizeof path);
        env("RR_ORACLE_SCENARIO", g_scenario, sizeof g_scenario, "unnamed");
        g_out = fopen(path, "wb");
        if (g_out) {
            setvbuf(g_out, NULL, _IOFBF, 1 << 20);
            fprintf(g_out, "# rr-oracle 1\nexe\t73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2\nscenario\t%s\n", g_scenario);
        }
        HMODULE real = LoadLibraryA("AWEMAN_REAL.DLL");
        g_real2 = real ? (void*)GetProcAddress(real, MAKEINTRESOURCEA(2)) : NULL;
        if (!g_real2) {
            if (g_out) { fputs("# AWEMAN_REAL.DLL ordinal 2 not found\n", g_out); fclose(g_out); g_out = NULL; }
            return FALSE;
        }
        if (!rr_install_hooks()) {
            if (g_out) { fclose(g_out); g_out = NULL; }
            return FALSE;
        }
        if (g_out) fflush(g_out);
    } else if (reason == DLL_PROCESS_DETACH && g_out) {
        fputs("end\n", g_out);
        fclose(g_out);
        g_out = NULL;
    }
    return TRUE;
}
#endif
