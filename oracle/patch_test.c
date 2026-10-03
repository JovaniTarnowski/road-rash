/* Host-side test of the call-site patch verifier in aweman_proxy.c. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define RR_PATCH_NO_WINDOWS 1
#include "aweman_proxy.c"

int main(void) {
    int failures = 0;
    uint8_t code[16] = {0};
    uintptr_t base = (uintptr_t)code;
    /* call to target 0x1000 bytes ahead of the next instruction */
    uint32_t target = (uint32_t)(base + 5 + 0x1000);
    code[0] = 0xE8; int32_t rel = 0x1000; memcpy(code + 1, &rel, 4);
    uint8_t hook_storage[4];
    if (!rr_patch_call(code, target, hook_storage)) { puts("FAIL patch valid site"); failures++; }
    memcpy(&rel, code + 1, 4);
    if ((uintptr_t)(code + 5) + rel != (uintptr_t)hook_storage) { puts("FAIL rel32 rewritten"); failures++; }
    uint8_t other[5] = {0x90, 0, 0, 0, 0};
    if (rr_patch_call(other, target, hook_storage)) { puts("FAIL accepted non-call"); failures++; }
    if (other[0] != 0x90) { puts("FAIL wrote on mismatch"); failures++; }
    printf("%d failures\n", failures);
    return failures != 0;
}
