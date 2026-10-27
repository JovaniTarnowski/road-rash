/* GDI text ground-truth probe (plan 13 Task 6 Step 2 GDI research).
 * Builds with i686-w64-mingw32-gcc, runs under Wine headless (memory DC only).
 * Installs the disc fonts, creates the game's menu LOGFONTs exactly
 * (0x004239e5 table: positive cell heights, weight 0 except Courier 700,
 * OUT_TT_ONLY_PRECIS, DEFAULT_QUALITY), then dumps TEXTMETRIC, ABC widths,
 * GetTextExtentPoint32A and DrawTextExA CALCRECT for the golden strings,
 * plus 1-bit renders of each line for pixel comparison. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static void dump_metrics(HDC dc, const char* tag) {
    TEXTMETRICA tm;
    GetTextMetricsA(dc, &tm);
    printf("METRIC %s tmHeight=%ld tmAscent=%ld tmDescent=%ld tmInternalLeading=%ld "
           "tmExternalLeading=%ld tmAveCharWidth=%ld tmMaxCharWidth=%ld tmWeight=%ld "
           "tmOverhang=%ld tmPitchAndFamily=%d tmCharSet=%d\n",
           tag, tm.tmHeight, tm.tmAscent, tm.tmDescent, tm.tmInternalLeading,
           tm.tmExternalLeading, tm.tmAveCharWidth, tm.tmMaxCharWidth, tm.tmWeight,
           tm.tmOverhang, tm.tmPitchAndFamily, tm.tmCharSet);
}

static void dump_abc(HDC dc, const char* tag) {
    ABC abc[224];
    if (!GetCharABCWidthsA(dc, 32, 255, abc)) {
        printf("ABC %s FAILED\n", tag);
        return;
    }
    printf("ABC %s", tag);
    for (int i = 0; i < 224; ++i) printf(" %d,%d,%d", abc[i].abcA, abc[i].abcB, abc[i].abcC);
    printf("\n");
}

static void dump_extent(HDC dc, const char* tag, const char* s) {
    SIZE sz;
    GetTextExtentPoint32A(dc, s, (int)strlen(s), &sz);
    printf("EXTENT %s cx=%ld cy=%ld [%s]\n", tag, sz.cx, sz.cy, s);
}

static void dump_calcrect(HDC dc, const char* tag, const char* s, unsigned fmt, int w) {
    RECT r = {0, 0, w, 0};
    char buf[1024];
    strncpy(buf, s, sizeof buf - 1);
    buf[sizeof buf - 1] = 0;
    int h = DrawTextExA(dc, buf, -1, &r, fmt | DT_CALCRECT, NULL);
    printf("CALCRECT %s fmt=0x%x w=%d h=%d rect=%ld,%ld,%ld,%ld [%s]\n", tag, fmt, w, h,
           r.left, r.top, r.right, r.bottom, s);
}

/* 1-bit render of one line at 0,0 into a freshly allocated buffer; dumps rows as hex. */
static void dump_render(HDC dc, const char* tag, const char* s) {
    SIZE sz;
    GetTextExtentPoint32A(dc, s, (int)strlen(s), &sz);
    int w = (int)sz.cx + 8, h = (int)sz.cy + 8;
    BITMAPINFO bi;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 1;
    void* bits = NULL;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) {
        printf("RENDER %s FAILED\n", tag);
        return;
    }
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(mem, bmp);
    SetTextColor(mem, RGB(255, 255, 255));
    SetBkColor(mem, RGB(0, 0, 0));
    SetBkMode(mem, OPAQUE);
    TextOutA(mem, 0, 0, s, (int)strlen(s));
    SelectObject(mem, old);
    DeleteDC(mem);
    printf("RENDER %s w=%d h=%d\n", tag, w, h);
    int stride = ((w + 31) / 32) * 4;
    for (int y = 0; y < h; ++y) {
        printf("ROW");
        for (int x = 0; x < stride; ++x) printf(" %02x", ((unsigned char*)bits)[y * stride + x]);
        printf("\n");
    }
    DeleteObject(bmp);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: textprobe.exe <TEXT-dir-with-trailing-backslash>\n");
        return 1;
    }
    const char* files[] = {"FUTR.DLL", "FUTB.DLL", "FUTD.DLL", "BADLOC.DLL"};
    char path[MAX_PATH];
    for (int i = 0; i < 4; ++i) {
        snprintf(path, sizeof path, "%s%s", argv[1], files[i]);
        printf("ADDFONT %s -> %d\n", path, AddFontResourceA(path));
    }
    struct {
        const char* face;
        int height, weight;
        const char* tag;
    } fonts[] = {
        {"Futura Condensed Regular", 32, 0, "desc32"},
        {"Futura Condensed Regular", 28, 0, "dlg28"},
        {"Futura Condensed Regular", 36, 0, "schmooze36"},
        {"Futura Condensed Bold", 24, 0, "button24"},
        {"Futura Condensed Bold", 20, 0, "stand20"},
        {"Futura Condensed Demi Regular", 24, 0, "demi24"},
        {"Badloc", 72, 0, "title72"},
        {"Badloc", 42, 0, "qual42"},
        {"New Courier Bold", 18, 700, "courier18"},
    };
    HDC screen = GetDC(NULL);
    HDC dc = CreateCompatibleDC(screen);
    ReleaseDC(NULL, screen);
    if (!dc) {
        printf("NODC\n");
        return 1;
    }
    /* Height sweep for the ppem/line-height rule fit. */
    const char* sweep[] = {"Futura Condensed Regular", "Futura Condensed Bold",
                           "Futura Condensed Demi Regular", "Badloc"};
    for (unsigned f = 0; f < 4; ++f)
        for (int h = 8; h <= 80; ++h) {
            HFONT hf = CreateFontA(h, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, sweep[f]);
            if (!hf) continue;
            SelectObject(dc, hf);
            TEXTMETRICA tm;
            GetTextMetricsA(dc, &tm);
            printf("SWEEP %s %d %ld %ld %ld %ld\n", sweep[f], h, tm.tmHeight, tm.tmAscent,
                   tm.tmDescent, tm.tmInternalLeading);
            DeleteObject(hf);
        }
    const char* descs[] = {
        "A model bad boy, cruel and ruthless.",
        "Don't try to get on Axle's good side",
        "Run your race at the blazing speed of 320x240 or the stunning visual realism of 640x480.",
        "Load a previously saved game from disk.",
        "Choose from 2 exciting tunes by Hammerbox",
        "Single race on City",
    };
    for (unsigned f = 0; f < sizeof fonts / sizeof fonts[0]; ++f) {
        HFONT h = CreateFontA(fonts[f].height, 0, 0, 0, fonts[f].weight, 0, 0, 0, 0, 4, 0, 0,
                              0, fonts[f].face);
        if (!h) {
            printf("CREATEFONT %s FAILED\n", fonts[f].tag);
            continue;
        }
        SelectObject(dc, h);
        dump_metrics(dc, fonts[f].tag);
        dump_abc(dc, fonts[f].tag);
        if (!strcmp(fonts[f].tag, "desc32")) {
            for (unsigned i = 0; i < sizeof descs / sizeof descs[0]; ++i) {
                char tag[64];
                snprintf(tag, sizeof tag, "desc32-%u", i);
                dump_extent(dc, tag, descs[i]);
                dump_calcrect(dc, tag, descs[i], 0x10 /*wordbreak*/, 300);
            }
        }
        if (!strcmp(fonts[f].tag, "button24")) {
            dump_extent(dc, "button24", "Accelerate");
            dump_render(dc, "button24-Accelerate", "Accelerate");
            dump_render(dc, "desc32-Height", "Height: 6'1\"");
        }
        if (!strcmp(fonts[f].tag, "desc32")) dump_render(dc, "desc32-Height", "Height: 6'1\"");
        DeleteObject(h);
    }
    DeleteDC(dc);
    printf("DONE\n");
    return 0;
}
