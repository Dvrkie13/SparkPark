/* The friends list on a second screen (the AYN Thor's lower one), beside
 * the minimap: every Pokémon in the PokéPark Pad's order, with its
 * number, icon and name, and the pink book of a friend or the rainbow
 * one of a best friend; one not yet befriended is "???", as the Pad shows
 * it. It is drawn with the game's pictures and font (hle_friends.c), on
 * the Pad's cream page, each Pokémon on one of its blue tags, and
 * scrolled by dragging it.
 *
 * Each row is drawn once into a strip of its own, kept for the rows last
 * shown, so scrolling only copies them; while nothing changes, the rows
 * just above and below are drawn ahead, one a frame. */
#include <stdio.h>

#include "canvas.h"
#include "host_settings.h"

int hle_friends_rows(void); /* hle_friends.c */
unsigned hle_friends_changes(void);
const char* hle_friends_name(int i);
int hle_friends_status(int i);
const uint32_t* hle_friends_icon(int i, int* w, int* h);
const uint32_t* hle_friends_picture(const char* name, int* w, int* h);

/* In picture units: the list's height is 1080. */
#define REF_H 1080.0f
#define MARGIN 22.0f
#define HEADER 112.0f /* the title and count, above the rows */
#define ROW_PITCH 112.0f
#define ROW_HEIGHT 100.0f
#define PAGE 0xFFEEDEBDu  /* the Pad's page */
#define EDGE 0xFFA48D62u  /* and its shadow */
#define INK 0xFF5A3A1Au
#define NAVY 0xFF1E3C78u

/* The blue tag (PmFriendsMat): one of its bands, its ends kept and its
 * middle stretched. */
static const Rect kTagLeft = {45, 40, 64, 74}, kTagMiddle = {64, 40, 232, 74}, kTagRight = {232, 40, 252, 74};
/* The digits, "/0123456789" (PmFriendListNumMat). */
static const float kDigitX[11][2] = {{8, 18},    {19, 39},   {46, 54},   {63, 81},   {84, 101}, {105, 125},
                                     {127, 145}, {148, 167}, {171, 190}, {193, 213}, {217, 236}};
#define DIGIT_Y0 3.0f
#define DIGIT_Y1 30.0f
/* The scroll bar's thumb (PmFriendsMat's top left). */
static const Rect kThumb = {3, 3, 36, 66};

static float sScroll, sVelocity; /* picture units */
static int sDragging;
static float sDragY, sDragScroll, sLastY;
static unsigned sMoves; /* scrolling's changes */

static float list_width(void) { return sW / sScale; }

static float max_scroll(int rows) {
    float m = rows * ROW_PITCH + MARGIN - (REF_H - HEADER);
    return m > 0 ? m : 0;
}

static void fill(uint32_t c) {
    int k;
    for (k = 0; k < sW * sH; k++) sPx[k] = c;
}

/* A number in the game's digits, its right end at x, its middle at y. */
static float draw_number(const char* s, float x, float y, float height) {
    int w, h, n = (int)strlen(s), i;
    const uint32_t* img = hle_friends_picture("PmFriendListNumMat", &w, &h);
    float k = height / (DIGIT_Y1 - DIGIT_Y0), width = 0;
    for (i = 0; i < n; i++) {
        int d = s[i] == '/' ? 0 : s[i] - '0' + 1;
        width += (kDigitX[d][1] - kDigitX[d][0] + 1) * k;
    }
    x -= width;
    for (i = 0; i < n && img; i++) {
        int d = s[i] == '/' ? 0 : s[i] - '0' + 1;
        float dw = (kDigitX[d][1] - kDigitX[d][0]) * k;
        Rect src = {kDigitX[d][0], DIGIT_Y0, kDigitX[d][1], DIGIT_Y1};
        Rect dst = {x, y - height / 2, x + dw, y + height / 2};
        draw_image(img, w, h, src, dst, 0xFFFFFF, 0);
        x += dw + k;
    }
    return width;
}

/* Row i at y (its top). */
static void draw_row(int i, float y) {
    int w, h, status = hle_friends_status(i);
    const uint32_t* tag = hle_friends_picture("PmFriendsMat", &w, &h);
    const uint32_t* icon;
    float x0 = MARGIN, x1 = list_width() - MARGIN - 34, k = ROW_HEIGHT / (kTagLeft.y1 - kTagLeft.y0);
    float lx = x0 + (kTagLeft.x1 - kTagLeft.x0) * k, rx = x1 - (kTagRight.x1 - kTagRight.x0) * k, cy = y + ROW_HEIGHT / 2;
    char number[8];
    if (tag) {
        Rect l = {x0, y, lx, y + ROW_HEIGHT}, m = {lx, y, rx, y + ROW_HEIGHT}, r = {rx, y, x1, y + ROW_HEIGHT};
        draw_image(tag, w, h, kTagLeft, l, 0xFFFFFF, 0);
        draw_image(tag, w, h, kTagMiddle, m, 0xFFFFFF, 0);
        draw_image(tag, w, h, kTagRight, r, 0xFFFFFF, 0);
    } else {
        Rect b = {x0, y, x1, y + ROW_HEIGHT};
        draw_box(b, 12, 0xFF62A5EEu);
    }
    snprintf(number, sizeof(number), "%03d", i + 1);
    draw_number(number, x0 + 112, cy, 34);
    icon = status ? hle_friends_icon(i, &w, &h) : NULL;
    if (!icon) icon = hle_friends_picture("IcQuestion01", &w, &h);
    if (icon) {
        Rect src = {0, 0, (float)w, (float)h}, dst = {x0 + 124, cy - 44, x0 + 212, cy + 44};
        draw_image(icon, w, h, src, dst, 0xFFFFFF, 0);
    }
    draw_text(status && hle_friends_name(i)[0] ? hle_friends_name(i) : "???", x0 + 228, cy, 46, ALIGN_LEFT, 0xFFFFFFFFu,
              NAVY, 3.5f, 0, rx - 80 - (x0 + 228));
    if (status) {
        const uint32_t* book = hle_friends_picture("FriendIcon", &w, &h);
        if (book) {
            Rect src = {status == 2 ? w / 2.0f : 0, 0, status == 2 ? (float)w : w / 2.0f, (float)h};
            Rect dst = {rx - 76, cy - 34, rx - 8, cy + 34};
            draw_image(book, w, h, src, dst, 0xFFFFFF, 0);
        }
    }
}

/* Rows drawn, each a strip as wide as the list. */
#define STRIPS 40
#define AHEAD 16 /* rows drawn ahead, each way from the middle (fewer than STRIPS in all) */
static struct {
    int row, status, w;
    float scale;
    uint32_t* px;
} sStrips[STRIPS];
static unsigned sStripUse[STRIPS], sUses;

/* Row i's strip, if drawn; else -1, and the one to draw it in. */
static int strip_of(int i, int* oldest) {
    int k, status = hle_friends_status(i);
    *oldest = 0;
    for (k = 0; k < STRIPS; k++) {
        if (sStrips[k].px && sStrips[k].row == i && sStrips[k].status == status && sStrips[k].w == sW &&
            sStrips[k].scale == sScale)
            return k;
        if (sStripUse[k] < sStripUse[*oldest]) *oldest = k;
    }
    return -1;
}

static const uint32_t* strip(int i, int rowPx) {
    int k, oldest, status = hle_friends_status(i);
    uint32_t* saved = sPx;
    int savedH = sH;
    float savedY = sOffY;
    if ((k = strip_of(i, &oldest)) >= 0) {
        sStripUse[k] = ++sUses;
        return sStrips[k].px;
    }
    k = oldest;
    free(sStrips[k].px);
    sStrips[k].px = (uint32_t*)malloc((size_t)sW * rowPx * 4);
    if (!sStrips[k].px) return NULL;
    sStrips[k].row = i;
    sStrips[k].status = status;
    sStrips[k].w = sW;
    sStrips[k].scale = sScale;
    sStripUse[k] = ++sUses;
    sPx = sStrips[k].px;
    sH = rowPx;
    sOffY = 0;
    fill(PAGE);
    draw_row(i, (rowPx / sScale - ROW_HEIGHT) / 2);
    sPx = saved;
    sH = savedH;
    sOffY = savedY;
    return sStrips[k].px;
}

static void forget_strips(void) {
    int k;
    for (k = 0; k < STRIPS; k++) {
        free(sStrips[k].px);
        sStrips[k].px = NULL;
    }
}

void friends_touch(int action, float x, float y) {
    (void)x;
    if (sScale <= 0) return;
    y /= sScale;
    if (action == 0) {
        sDragging = 1;
        sDragY = sLastY = y;
        sDragScroll = sScroll;
        sVelocity = 0;
    } else if (sDragging) {
        float scroll = sDragScroll - (y - sDragY);
        sVelocity = sVelocity * 0.5f + (sLastY - y) * 0.5f;
        sLastY = y;
        if (scroll != sScroll) {
            sScroll = scroll;
            sMoves++;
        }
        if (action == 1) sDragging = 0;
    }
}

const uint32_t* friends_draw(int w, int h, int* changed) {
    static uint32_t* px;
    static unsigned drawnChanges = ~0u, drawnMoves = ~0u;
    static int drawnW, drawnH;
    int rows, cellW, rowPx, i, first, last, count = 0;
    float top, limit;
    char text[16];
    *changed = 0;
    if (w <= 0 || h <= 0 || !hle_font_metrics(&cellW, &sCellH, &sBaseline) || !(rows = hle_friends_rows())) return NULL;
    sCellW = cellW;
    /* Thrown, it slows. */
    if (!sDragging && (sVelocity > 0.5f || sVelocity < -0.5f)) {
        sScroll += sVelocity;
        sVelocity *= 0.92f;
        sMoves++;
    }
    limit = max_scroll(rows);
    if (sScroll < 0 || sScroll > limit) {
        sScroll = sScroll < 0 ? 0 : limit;
        sVelocity = 0;
    }
    if (px && w == drawnW && h == drawnH && drawnChanges == hle_friends_changes() && drawnMoves == sMoves) {
        /* Nothing changed: a row near those shown drawn ahead. */
        int oldest, middle = (int)(sScroll / ROW_PITCH) + 4, d;
        for (d = 0; d <= 2 * AHEAD; d++) {
            int i = middle + (d % 2 ? -(d + 1) / 2 : d / 2);
            if (i >= 0 && i < rows && strip_of(i, &oldest) < 0) {
                strip(i, (int)ceilf(ROW_PITCH * sScale));
                break;
            }
        }
        return px;
    }
    if (w != drawnW || h != drawnH || !px) {
        free(px);
        px = (uint32_t*)malloc((size_t)w * h * 4);
        if (!px) return NULL;
        forget_strips();
    }
    if (drawnChanges != hle_friends_changes()) forget_strips();
    drawnW = w;
    drawnH = h;
    drawnChanges = hle_friends_changes();
    drawnMoves = sMoves;
    *changed = 1;
    sPx = px;
    sW = w;
    sH = h;
    sScale = h / REF_H;
    sOffX = sOffY = 0;
    fill(PAGE);
    /* The rows, below the title. */
    rowPx = (int)ceilf(ROW_PITCH * sScale);
    top = HEADER - sScroll;
    first = (int)(sScroll / ROW_PITCH);
    last = (int)((sScroll + REF_H - HEADER) / ROW_PITCH) + 1;
    for (i = first < 0 ? 0 : first; i <= last && i < rows; i++) {
        const uint32_t* s = strip(i, rowPx);
        int y0 = (int)floorf(Y(top + i * ROW_PITCH)), y;
        if (!s) continue;
        for (y = 0; y < rowPx; y++) {
            if (y0 + y >= (int)Y(HEADER) && y0 + y < h) memcpy(px + (size_t)(y0 + y) * w, s + (size_t)y * w, (size_t)w * 4);
        }
    }
    /* The title: "Friends", and how many of them. */
    {
        Rect line = {MARGIN, HEADER - 12, list_width() - MARGIN, HEADER - 6};
        draw_box(line, 3, EDGE);
    }
    for (i = 0; i < rows; i++) count += hle_friends_status(i) != 0;
    draw_text("Friends", MARGIN + 8, (HEADER - 12) / 2, 58, ALIGN_LEFT, INK, 0, 0, 1, 0);
    snprintf(text, sizeof(text), "%d/%d", count, rows);
    draw_number(text, list_width() - MARGIN - 40, (HEADER - 12) / 2, 44);
    /* The scroll bar. */
    if (limit > 0) {
        int tw, th;
        const uint32_t* img = hle_friends_picture("PmFriendsMat", &tw, &th);
        float x1 = list_width() - 8, x0 = x1 - 22, y0 = HEADER, y1 = REF_H - MARGIN;
        float size = (y1 - y0) * (REF_H - HEADER) / (rows * ROW_PITCH + MARGIN);
        float at = y0 + (y1 - y0 - (size < 60 ? 60 : size)) * sScroll / limit;
        Rect track = {x0 + 7, y0, x1 - 7, y1};
        draw_box(track, 4, 0x40000000u | (EDGE & 0xFFFFFF));
        if (size < 60) size = 60;
        if (img) {
            float cap = (x1 - x0) * 12 / (kThumb.x1 - kThumb.x0);
            Rect t = {kThumb.x0, kThumb.y0, kThumb.x1, kThumb.y0 + 12}, m = {kThumb.x0, kThumb.y0 + 12, kThumb.x1, kThumb.y1 - 12};
            Rect b = {kThumb.x0, kThumb.y1 - 12, kThumb.x1, kThumb.y1};
            Rect dt = {x0, at, x1, at + cap}, dm = {x0, at + cap, x1, at + size - cap}, db = {x0, at + size - cap, x1, at + size};
            draw_image(img, tw, th, t, dt, 0xFFFFFF, 0);
            draw_image(img, tw, th, m, dm, 0xFFFFFF, 0);
            draw_image(img, tw, th, b, db, 0xFFFFFF, 0);
        } else {
            Rect thumb = {x0, at, x1, at + size};
            draw_box(thumb, 8, EDGE);
        }
    }
    return px;
}
