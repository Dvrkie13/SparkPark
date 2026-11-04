/* SparkPark: the frame rate counter, the movie skip prompt, and
 * screenshots.
 *
 * Each is a small image the device lays over the picture, drawn with GDI
 * in white on a see-through dark box sized to the window's height, and
 * drawn again only when what it shows or the size changes:
 * - the counter (the Show FPS setting, F3), top left: the frames shown in
 *   the last second;
 * - while + is held during a movie that can be skipped (hle_game.c),
 *   bottom right: "Skipping" over a bar that fills until it skips.
 *
 * A screenshot (F12) is the next frame shown, as the window shows it
 * (menus included, the counter not), saved as a PNG in
 * Pictures\SparkPark\SparkPark_<date>_<time>.png. The device hands over
 * its pixels; they are copied and encoded by WIC on a thread of their own,
 * so the game doesn't wait for the file. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host_settings.h"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "windowscodecs.lib")

int hle_movie_skip_progress(void); /* hle_game.c: 0-100, or -1 */

/* ------------------------------------------------------------------------ */
/* The images */

static volatile int sFps = -1; /* the last second's, rounded; -1 before the first */

void hud_set_fps(double fps) { sFps = (int)(fps + 0.5); }

typedef struct {
    uint32_t* px;
    int w, h;
    int drawnValue, drawnWinH; /* what it shows, for drawing it again only on a change */
} Image;

static Image sImage[HUD_IMAGES] = {{NULL, 0, 0, -2, 0}, {NULL, 0, 0, -2, 0}};

/* Draws text (as wide as sizer, so the box keeps its size as it changes)
 * into im: white on a see-through dark box, and under it, if progress is
 * 0-100, a bar filled that far. */
static void draw_label(Image* im, const char* text, const char* sizer, int progress, int winH) {
    BITMAPINFO bi;
    HDC dc;
    HBITMAP bmp, old;
    HFONT font, oldFont;
    void* bits;
    RECT r;
    int size = winH / 30 < 14 ? 14 : winH / 30, x, y, barH = progress >= 0 ? size / 4 + 2 : 0, pad = size / 2;
    SIZE ext;
    dc = CreateCompatibleDC(NULL);
    font = CreateFontA(-size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    oldFont = (HFONT)SelectObject(dc, font);
    GetTextExtentPoint32A(dc, sizer, (int)strlen(sizer), &ext);
    im->w = ext.cx + 2 * pad;
    im->h = ext.cy + size / 3 + (barH ? barH + pad / 2 : 0);
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = im->w;
    bi.bmiHeader.biHeight = -im->h; /* top row first */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    free(im->px);
    im->px = NULL;
    if (bmp) {
        old = (HBITMAP)SelectObject(dc, bmp);
        memset(bits, 0, (size_t)im->w * im->h * 4);
        SetBkMode(dc, TRANSPARENT);
        SetRect(&r, 0, 0, im->w, ext.cy + size / 3);
        SetTextColor(dc, RGB(255, 255, 255));
        DrawTextA(dc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        GdiFlush();
        im->px = (uint32_t*)malloc((size_t)im->w * im->h * 4);
        if (im->px) {
            /* GDI leaves alpha 0: the grey level is the text's coverage. The
             * box is black, 40% opaque, the text white over it. */
            const uint32_t* src = (const uint32_t*)bits;
            int barTop = im->h - barH - pad / 2, barRight = pad + (im->w - 2 * pad) * progress / 100;
            for (y = 0; y < im->h; y++) {
                for (x = 0; x < im->w; x++) {
                    float cov = (src[y * im->w + x] & 0xFF) / 255.0f, a;
                    uint32_t v;
                    if (barH && y >= barTop && y < barTop + barH && x >= pad && x < im->w - pad)
                        cov = x < barRight ? 1.0f : 0.25f; /* the bar: filled, and the rest of its track */
                    a = 0.4f + 0.6f * cov;
                    v = (uint32_t)(255.0f * cov / a + 0.5f);
                    im->px[y * im->w + x] = (uint32_t)(a * 255.0f + 0.5f) << 24 | v << 16 | v << 8 | v;
                }
            }
        }
        SelectObject(dc, old);
        DeleteObject(bmp);
    }
    SelectObject(dc, oldFont);
    DeleteObject(font);
    DeleteDC(dc);
}

const uint32_t* hud_draw(int which, int winW, int winH, int* x, int* y, int* w, int* h) {
    Image* im;
    int value;
    if (which < 0 || which >= HUD_IMAGES || winW <= 0 || winH <= 0) return NULL;
    im = &sImage[which];
    if (which == HUD_COUNTER) {
        if (!g_settings.show_fps) return NULL;
        value = sFps;
    } else {
        /* Shown while + is held; a little held is enough to show it. */
        value = hle_movie_skip_progress();
        if (value <= 0) return NULL;
        value = value / 2 * 2; /* drawn again every 2% */
    }
    if (value != im->drawnValue || winH != im->drawnWinH || !im->px) {
        char text[32];
        if (which == HUD_COUNTER) {
            if (value < 0) snprintf(text, sizeof(text), "-- FPS");
            else snprintf(text, sizeof(text), "%d FPS", value);
            draw_label(im, text, "000 FPS", -1, winH);
        } else {
            draw_label(im, "Skipping", "Skipping movie", value, winH);
        }
        im->drawnValue = value;
        im->drawnWinH = winH;
    }
    if (!im->px || im->w > winW || im->h > winH) return NULL;
    if (which == HUD_COUNTER) {
        *x = *y = winH / 60;
    } else {
        *x = winW - im->w - winH / 30;
        *y = winH - im->h - winH / 30;
    }
    *w = im->w;
    *h = im->h;
    return im->px;
}

/* ------------------------------------------------------------------------ */
/* Screenshots */

static volatile LONG sRequested;

void screenshot_request(void) { InterlockedExchange(&sRequested, 1); }

int screenshot_pending(void) { return InterlockedExchange(&sRequested, 0) != 0; }

typedef struct {
    uint8_t* bgra; /* top row first, w * 4 bytes a row */
    int w, h;
} Shot;

/* Pictures\SparkPark\SparkPark_<date>_<time>.png, the folder made if it
 * isn't there; another number on the end if two are taken in a second. */
static int shot_path(wchar_t* out, size_t n) {
    wchar_t dir[MAX_PATH];
    SYSTEMTIME t;
    int i;
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_MYPICTURES | CSIDL_FLAG_CREATE, NULL, 0, dir))) return 0;
    if (wcslen(dir) + 12 >= MAX_PATH) return 0;
    wcscat(dir, L"\\SparkPark");
    CreateDirectoryW(dir, NULL);
    GetLocalTime(&t);
    for (i = 1; i < 100; i++) {
        if (i == 1)
            _snwprintf(out, n, L"%ls\\SparkPark_%04u-%02u-%02u_%02u-%02u-%02u.png", dir, t.wYear, t.wMonth, t.wDay,
                       t.wHour, t.wMinute, t.wSecond);
        else
            _snwprintf(out, n, L"%ls\\SparkPark_%04u-%02u-%02u_%02u-%02u-%02u_%d.png", dir, t.wYear, t.wMonth, t.wDay,
                       t.wHour, t.wMinute, t.wSecond, i);
        out[n - 1] = 0;
        if (GetFileAttributesW(out) == INVALID_FILE_ATTRIBUTES) return 1;
    }
    return 0;
}

static int write_png(const wchar_t* path, const Shot* s) {
    IWICImagingFactory* f = NULL;
    IWICStream* stream = NULL;
    IWICBitmapEncoder* enc = NULL;
    IWICBitmapFrameEncode* frame = NULL;
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    int ok = 0;
    if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory,
                                (void**)&f)))
        return 0;
    if (SUCCEEDED(IWICImagingFactory_CreateStream(f, &stream)) &&
        SUCCEEDED(IWICStream_InitializeFromFilename(stream, path, GENERIC_WRITE)) &&
        SUCCEEDED(IWICImagingFactory_CreateEncoder(f, &GUID_ContainerFormatPng, NULL, &enc)) &&
        SUCCEEDED(IWICBitmapEncoder_Initialize(enc, (IStream*)stream, WICBitmapEncoderNoCache)) &&
        SUCCEEDED(IWICBitmapEncoder_CreateNewFrame(enc, &frame, NULL)) &&
        SUCCEEDED(IWICBitmapFrameEncode_Initialize(frame, NULL)) &&
        SUCCEEDED(IWICBitmapFrameEncode_SetSize(frame, (UINT)s->w, (UINT)s->h)) &&
        SUCCEEDED(IWICBitmapFrameEncode_SetPixelFormat(frame, &fmt)) &&
        IsEqualGUID(&fmt, &GUID_WICPixelFormat32bppBGRA) &&
        SUCCEEDED(IWICBitmapFrameEncode_WritePixels(frame, (UINT)s->h, (UINT)s->w * 4, (UINT)s->w * s->h * 4,
                                                    s->bgra)) &&
        SUCCEEDED(IWICBitmapFrameEncode_Commit(frame)) && SUCCEEDED(IWICBitmapEncoder_Commit(enc)))
        ok = 1;
    if (frame) IWICBitmapFrameEncode_Release(frame);
    if (enc) IWICBitmapEncoder_Release(enc);
    if (stream) IWICStream_Release(stream);
    IWICImagingFactory_Release(f);
    return ok;
}

static DWORD WINAPI save_thread(LPVOID p) {
    Shot* s = (Shot*)p;
    wchar_t path[MAX_PATH + 64];
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (shot_path(path, sizeof(path) / sizeof(path[0])) && write_png(path, s))
        fprintf(stderr, "screenshot: %ls\n", path);
    else
        fprintf(stderr, "screenshot: could not save it\n");
    CoUninitialize();
    free(s->bgra);
    free(s);
    return 0;
}

void screenshot_save(const uint8_t* px, int w, int h, size_t stride, int rgba, int bottom_up) {
    Shot* s;
    HANDLE t;
    int x, y;
    if (!px || w <= 0 || h <= 0) return;
    s = (Shot*)malloc(sizeof(*s));
    if (!s) return;
    s->bgra = (uint8_t*)malloc((size_t)w * h * 4);
    if (!s->bgra) {
        free(s);
        return;
    }
    s->w = w;
    s->h = h;
    for (y = 0; y < h; y++) {
        const uint8_t* in = px + (size_t)(bottom_up ? h - 1 - y : y) * stride;
        uint8_t* out = s->bgra + (size_t)y * w * 4;
        for (x = 0; x < w; x++) {
            out[x * 4 + 0] = in[x * 4 + (rgba ? 2 : 0)];
            out[x * 4 + 1] = in[x * 4 + 1];
            out[x * 4 + 2] = in[x * 4 + (rgba ? 0 : 2)];
            out[x * 4 + 3] = 255;
        }
    }
    t = CreateThread(NULL, 0, save_thread, s, 0, NULL);
    if (t) CloseHandle(t);
    else {
        free(s->bgra);
        free(s);
    }
}
