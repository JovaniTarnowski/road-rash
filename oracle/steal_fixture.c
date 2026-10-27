/* Component oracle for the unmodified disc's 0x004096f0 hit resolver.
 * Loaded as AWEMAN32.DLL in a PRIVATE install/prefix, exits before game startup.
 * Synthetic objects/animations; original selection/reach/transfer/queue code.
 * The original mode-3 RNG override supplies controlled rolls. Only the call
 * at 0x40981c is redirected to count and forward to the original 0x441750.
 * This is not a free-running race, CRT RNG, asset or damage oracle. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define W(p, o) (*(int32_t*)((uint8_t*)(p) + (o)))
#define B(p, o) (*((uint8_t*)(p) + (o)))
#define G(a) (*(volatile int32_t*)(uintptr_t)(a))
#define GB(a) (*(volatile uint8_t*)(uintptr_t)(a))
#define PTR(p) ((int32_t)(uintptr_t)(p))

static unsigned rolls;
static int __cdecl count_roll(void) {
    ++rolls;
    return ((int(__cdecl*)(void))0x00441750)();
}

/* Imported by the EXE but never called: the fixture exits during attachment. */
int __cdecl Forward2(void) {
    return 0;
}

struct Case {
    const char* name;
    int victim_frame, attacker_frame, descriptor_flags;
    int attacker_weapon, victim_weapon, victim_inventory, victim_animation;
    uint32_t tick, victim_deadline, attacker_deadline;
    int dpos, lateral, height, roll, available;
};

static int fixture(FILE* out) {
    uint8_t* call = (uint8_t*)0x0040981c;
    int32_t rel;
    memcpy(&rel, call + 1, 4);
    if (call[0] != 0xe8 || (uintptr_t)(call + 5) + rel != 0x00441750) return 0;
    DWORD old;
    if (!VirtualProtect(call, 5, PAGE_EXECUTE_READWRITE, &old)) return 0;
    rel = (int32_t)((uintptr_t)count_roll - (uintptr_t)(call + 5));
    memcpy(call + 1, &rel, 4);
    if (!VirtualProtect(call, 5, old, &old) || !FlushInstructionCache(GetCurrentProcess(), call, 5)) return 0;

    /* Each case changes a gate independently from the successful baseline. */
    static const struct Case cases[] = {
        {"success-club", 0, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"success-chain", 0, 0, 0x200000, 0, 2, 2, -1, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"victim-frame-one", 1, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"victim-frame-two", 2, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"victim-frame-negative", -1, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"victim-frame-127", 127, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"victim-deadline-equal", 0, 0, 0x200000, 0, 1, 1, -1, 100, 100, 0, 1, 1000, 0, 3, 1},
        {"victim-deadline-past", 0, 0, 0x200000, 0, 1, 1, -1, 100, 99, 0, 1, 1000, 0, 3, 1},
        {"victim-deadline-high-bit", 0, 0, 0x200000, 0, 1, 1, -1, 100, 0x80000000u, 0, 1, 1000, 0, 3, 1},
        {"attacker-deadline-future", 0, 0, 0x200000, 0, 1, 1, -1, 100, 0, 999, 1, 1000, 0, 3, 1},
        {"attacker-armed", 0, 0, 0x200000, 1, 1, 1, -1, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"victim-fists", 0, 0, 0x200000, 0, 0, 1, -1, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"victim-kick", 0, 0, 0x200000, 0, 3, 1, -1, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"inventory-differs", 0, 0, 0x200000, 0, 1, 2, -1, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"victim-not-attacking", 0, 0, 0x200000, 0, 1, 1, 2, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"victim-animation-nine", 0, 0, 0x200000, 0, 1, 1, 9, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"no-hit-event", 0, 0, 0, 0, 1, 1, -1, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"attacker-frame-two", 0, 2, 0, 0, 1, 1, -1, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"other-descriptor-bit", 0, 0, 0x400000, 0, 1, 1, -1, 100, 0, 0, 1, 1000, 0, 3, 1},
        {"position-inside", 0, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 63, 1000, 0, 3, 1},
        {"position-boundary", 0, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 64, 1000, 0, 3, 1},
        {"lateral-inside", 0, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 1, 20479, 0, 3, 1},
        {"lateral-boundary", 0, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 1, 20480, 0, 3, 1},
        {"side-mismatch", 0, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 1, -1000, 0, 3, 1},
        {"height-inside", 0, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 1, 1000, 12287, 3, 1},
        {"height-boundary", 0, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 1, 1000, 12288, 3, 1},
        {"roll-zero", 0, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 1, 1000, 0, 0, 1},
        {"roll-one", 0, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 1, 1000, 0, 1, 1},
        {"roll-two", 0, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 1, 1000, 0, 2, 1},
        {"no-scan-slice", 0, 0, 0x200000, 0, 1, 1, -1, 100, 0, 0, 1, 1000, 0, 3, 0},
        {"unsigned-tick-high", 0, 0, 0x200000, 0, 1, 1, -1, 0x80000001u, 0x80000000u, 0, 1, 1000, 0, 3, 1}};
    G(0x004642d8) = 0;                   /* neither synthetic bike is the player: no audio */
    GB(0x004c5b00) = GB(0x004c5af9) = 0; /* offline, no network event */
    G(0x004753b0) = 3;                   /* original fixed-roll mode, bypasses uninitialized CRT */
    for (unsigned c = 0; c < sizeof cases / sizeof cases[0]; ++c) {
        const struct Case* v = &cases[c];
        int32_t self[256] = {0}, victim[256] = {0};
        int32_t sh[256] = {0}, vh[256] = {0}, brain[256] = {0};
        int32_t first[256] = {0}, last[256] = {0}, slice[256] = {0};
        int32_t bank[256], sequence[8] = {0}, selected[8] = {0}, graphics[8] = {0};
        W(sequence, 0x10) = 1;
        W(selected, 0x10) = PTR(graphics);
        W(graphics, 4) = v->descriptor_flags;
        for (unsigned i = 0; i < 256; ++i) bank[i] = PTR(sequence);
        W(self, 0x2fc) = PTR(sh);
        W(victim, 0x2fc) = PTR(vh);
        W(sh, 0x1d8) = PTR(brain);
        W(brain, 0x168) = PTR(victim);
        W(sh, 0x1c4) = PTR(self);
        W(vh, 0x1c4) = PTR(victim);
        W(sh, 0x78) = W(vh, 0x78) = PTR(bank);
        W(sh, 0x7c) = W(vh, 0x7c) = PTR(selected);
        W(sh, 0) = PTR(vh);
        W(sh, 4) = PTR(first);
        W(vh, 0) = PTR(last);
        W(vh, 4) = PTR(sh);
        W(sh, 0x2c) = W(vh, 0x2c) = v->available ? PTR(slice) : 0;
        B(slice, 0x36) = 3;
        W(sh, 0xc4) = W(vh, 0xc4) = 8;
        W(self, 0x1c) = W(sh, 0x1c) = 1000;
        W(victim, 0x1c) = W(vh, 0x1c) = 1000 + v->dpos;
        W(vh, 0x28) = v->lateral;
        W(vh, 0x20) = v->height;
        W(self, 0x29c) = W(victim, 0x29c) = 100000;
        W(sh, 0x1c8) = W(sh, 0x1d4) = v->attacker_weapon;
        W(vh, 0x1c8) = v->victim_inventory;
        W(vh, 0x1d4) = v->victim_weapon;
        W(sh, 0x1cc) = v->attacker_deadline;
        W(vh, 0x1cc) = v->victim_deadline;
        W(vh, 0x1d0) = -1; /* suppress normal damage/follow-on RNG */
        B(sh, 0x80) = (uint8_t)G(0x00464518 + v->attacker_weapon * 4);
        B(sh, 0x81) = (uint8_t)v->attacker_frame;
        B(vh, 0x80) = v->victim_animation < 0 ? (uint8_t)G(0x004644f8 + v->victim_weapon * 4)
                                              : (uint8_t)v->victim_animation;
        B(vh, 0x81) = (uint8_t)v->victim_frame;
        B(sh, 0x82) = B(vh, 0x82) = 0xff;
        G(0x004b7ecc) = v->tick;
        G(0x0049bce8) = v->roll;
        rolls = 0;
        ((void(__fastcall*)(int32_t*))0x004096f0)(self);
        fprintf(out, "%s\t%u\t%d\t%d\t%d\t%d\t%u\t%u\t%d\t%d\t%d\t%d\t%d\n", v->name, rolls, W(sh, 0x1c8),
                W(sh, 0x1d4), W(vh, 0x1c8), W(vh, 0x1d4), (uint32_t)W(sh, 0x1cc), (uint32_t)W(vh, 0x1cc),
                (int8_t)B(sh, 0x80), (int8_t)B(vh, 0x80), (int8_t)B(sh, 0x82), (int8_t)B(vh, 0x82),
                (int8_t)G(0x00464538 + v->victim_inventory * 4));
    }
    fprintf(out, "end\t%u\n", (unsigned)(sizeof cases / sizeof cases[0]));
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)instance;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        char path[MAX_PATH];
        if (!GetEnvironmentVariableA("RR_STEAL_FIXTURE_OUT", path, sizeof path)) return FALSE;
        FILE* out = fopen(path, "wb");
        if (!out) return FALSE;
        fputs("# rr-steal-fixture 1\n"
              "case\trolls\tself_inventory\tself_active\tvictim_inventory\tvictim_active\t"
              "self_deadline\tvictim_deadline\tself_anim\tvictim_anim\tself_queue\tvictim_queue\t"
              "expected_steal_anim\n",
              out);
        const int ok = fixture(out);
        fclose(out);
        ExitProcess(ok ? 0 : 2);
    }
    return TRUE;
}
