/* Host-side test of the call-site patch verifier in aweman_proxy.c. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define RR_PATCH_NO_WINDOWS 1
#include "aweman_proxy.c"

/* Preflight hook sites against the reference executable: parse the PE
 * section table, map each site VA to a file offset, and check the expected
 * call bytes and target. Skips (exit 77 via caller) when the disc is absent. */
static const struct {
    uint32_t site, target;
    uint8_t bytes[5];
    const char* name;
} g_sites[] = {
    {0x0044b324, 0x0044b3b0, {0xe8, 0x87, 0, 0, 0}, "step"},
    {0x00442ea8, 0x0044f800, {0xe8, 0x53, 0xc9, 0, 0}, "controls"},
};

static uint32_t read_u32(const uint8_t* p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}
static uint16_t read_u16(const uint8_t* p) {
    uint16_t v;
    memcpy(&v, p, 2);
    return v;
}

static int preflight_reference_sites(void) {
    char path[1024];
#ifdef RR_SOURCE_DIR
    snprintf(path, sizeof path, "%s/../cdrom/ROADRASH/ROADRASH.EXE", RR_SOURCE_DIR);
#else
    snprintf(path, sizeof path, "../cdrom/ROADRASH/ROADRASH.EXE");
#endif
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("SKIP no reference executable\n");
        exit(77);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* exe = malloc((size_t)size);
    if (!exe || fread(exe, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        puts("FAIL read exe");
        return 0;
    }
    fclose(f);
    int ok = 1;
    if (read_u16(exe) != 0x5a4d) {
        puts("FAIL MZ");
        ok = 0;
    }
    uint32_t pe = read_u32(exe + 0x3c);
    if (ok && read_u32(exe + pe) != 0x4550) {
        puts("FAIL PE");
        ok = 0;
    }
    uint16_t sections = read_u16(exe + pe + 6);
    uint16_t opt_size = read_u16(exe + pe + 20);
    uint32_t image_base = read_u32(exe + pe + 24 + 28);
    size_t table = pe + 24 + opt_size;
    for (unsigned s = 0; s < sizeof g_sites / sizeof g_sites[0]; ++s) {
        uint32_t rva = g_sites[s].site - image_base;
        long offset = -1;
        for (int i = 0; i < sections; ++i) {
            const uint8_t* sh = exe + table + 40 * i;
            uint32_t va = read_u32(sh + 12), vsz = read_u32(sh + 8);
            uint32_t raw = read_u32(sh + 20), rsz = read_u32(sh + 16);
            if (rva >= va && rva + 5 <= va + vsz && rva + 5 <= va + rsz) {
                offset = (long)(raw + rva - va);
                break;
            }
        }
        if (offset < 0 || offset + 5 > size) {
            printf("FAIL %s unmapped\n", g_sites[s].name);
            ok = 0;
            continue;
        }
        if (memcmp(exe + offset, g_sites[s].bytes, 5)) {
            printf("FAIL %s bytes\n", g_sites[s].name);
            ok = 0;
            continue;
        }
        int32_t rel;
        memcpy(&rel, exe + offset + 1, 4);
        if ((uint32_t)(g_sites[s].site + 5 + rel) != g_sites[s].target) {
            printf("FAIL %s target\n", g_sites[s].name);
            ok = 0;
        }
    }
    free(exe);
    if (ok) puts("reference sites preflight ok");
    return ok;
}

int main(void) {
    int failures = 0;
    uint8_t code[16] = {0};
    uintptr_t base = (uintptr_t)code;
    /* call to target 0x1000 bytes ahead of the next instruction */
    uint32_t target = (uint32_t)(base + 5 + 0x1000);
    code[0] = 0xE8;
    int32_t rel = 0x1000;
    memcpy(code + 1, &rel, 4);
    uint8_t hook_storage[4];
    if (!rr_patch_call(code, target, hook_storage)) {
        puts("FAIL patch valid site");
        failures++;
    }
    memcpy(&rel, code + 1, 4);
    if ((uintptr_t)(code + 5) + rel != (uintptr_t)hook_storage) {
        puts("FAIL rel32 rewritten");
        failures++;
    }
    uint8_t other[5] = {0x90, 0, 0, 0, 0};
    if (rr_patch_call(other, target, hook_storage)) {
        puts("FAIL accepted non-call");
        failures++;
    }
    if (other[0] != 0x90) {
        puts("FAIL wrote on mismatch");
        failures++;
    }
    if (!preflight_reference_sites()) {
        puts("FAIL reference site preflight");
        failures++;
    }
    printf("%d failures\n", failures);
    return failures != 0;
}
