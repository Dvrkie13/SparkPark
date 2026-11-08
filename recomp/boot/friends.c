/* A second screen (the AYN Thor's lower one) as the PokéPark Pad, open:
 * on its left page the friends list, every Pokémon in the Pad's order,
 * with its number, icon and name, and the pink book of a friend or the
 * rainbow one of a best friend (one not yet befriended is "???", as the
 * Pad shows it); on its right page the minimap, as a photo stuck in: the
 * frame drawn here, the map in it by vk_backend.c, where friends_draw
 * says. All in the game's pictures and font
 * (hle_friends.c, hle_pause.c), each Pokémon on one of the Pad's blue
 * tags; the list scrolls by dragging it.
 *
 * The pages are drawn once. Each row is drawn once into a strip of its
 * own, kept for the rows last shown, so scrolling only lays them over the
 * pages again; while nothing changes, the rows just above and below are
 * drawn ahead, one a frame. */
#include <stdio.h>

#include "canvas.h"
#include "host_settings.h"

int hle_friends_rows(void); /* hle_friends.c */
unsigned hle_friends_changes(void);
const char* hle_friends_name(int i);
int hle_friends_status(int i);
const uint32_t* hle_friends_icon(int i, int* w, int* h);
const uint32_t* hle_friends_picture(const char* name, int* w, int* h);
const uint32_t* hle_pause_image(const char* name, const char* alpha, int* w, int* h); /* hle_pause.c */
double second_map_aspect(void); /* vk_backend.c */

/* In picture units: the screen is 1080 high, W() wide. */
#define REF_H 1080.0f
#define ROW_PITCH 104.0f
#define ROW_HEIGHT 94.0f
#define BAR 26.0f /* the scroll bar's width */
#define BEHIND 0xFF0C0F17u /* past the pages */
#define EDGE 0xFFA48D62u
#define BROWN 0xFF8C2A10u /* the Pad's titles */
#define NAVY 0xFF1E3C78u
#define PHOTO 0xFFFBF7EEu /* the photo's border */
#define PHOTO_EDGE 16.0f

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
static float sRowWidth;           /* the page's, while a row's strip is drawn */
static int sDragging;
static float sDragY, sDragScroll, sLastY;
static unsigned sMoves; /* scrolling's changes */

static float W(void) { return sW / sScale; }

/* The pages: the left one over the right at the spine, as the Pad's. */
static Rect left_page(void) {
    Rect r = {0, 0, W() * 0.53f, REF_H};
    return r;
}

static Rect right_page(void) {
    Rect r = {W() * 0.47f, 0, W(), REF_H};
    return r;
}

/* A part of a page, in fractions of it. */
static Rect part(Rect page, float x0, float y0, float x1, float y1) {
    Rect r = {page.x0 + (page.x1 - page.x0) * x0, page.y0 + (page.y1 - page.y0) * y0,
              page.x0 + (page.x1 - page.x0) * x1, page.y0 + (page.y1 - page.y0) * y1};
    return r;
}

/* Where the rows scroll (the scroll bar at its right). */
static Rect list_rect(void) { return part(left_page(), 0.09f, 0.15f, 0.91f, 0.93f); }

/* The map, at its shape, as large as fits in its part of the page with
 * the photo's border around it. */
static Rect map_rect(void) {
    Rect in = part(right_page(), 0.11f, 0.15f, 0.77f, 0.92f), r;
    float aspect = (float)second_map_aspect(), w = in.x1 - in.x0 - 2 * PHOTO_EDGE, h = in.y1 - in.y0 - 2 * PHOTO_EDGE;
    if (w > h * aspect) w = h * aspect;
    else h = w / aspect;
    r.x0 = (in.x0 + in.x1 - w) / 2;
    r.y0 = (in.y0 + in.y1 - h) / 2;
    r.x1 = r.x0 + w;
    r.y1 = r.y0 + h;
    return r;
}

static float row_width(void) { return sRowWidth; }

static float max_scroll(int rows) {
    float m = rows * ROW_PITCH - (list_rect().y1 - list_rect().y0);
    return m > 0 ? m : 0;
}

static void picture(const char* name, const char* alpha, Rect src, Rect dst) {
    int w, h;
    const uint32_t* img = hle_pause_image(name, alpha, &w, &h);
    if (src.x1 <= 0) {
        src.x0 = src.y0 = 0;
        src.x1 = (float)w;
        src.y1 = (float)h;
    }
    draw_image(img, w, h, src, dst, 0xFFFFFF, 0);
}

/* How wide a number is in the game's digits. */
static float number_width(const char* s, float height) {
    float k = height / (DIGIT_Y1 - DIGIT_Y0), width = 0;
    for (; *s; s++) {
        int d = *s == '/' ? 0 : *s - '0' + 1;
        width += (kDigitX[d][1] - kDigitX[d][0] + 1) * k;
    }
    return width;
}

/* A number in the game's digits, its right end at x, its middle at y. */
static float draw_number(const char* s, float x, float y, float height) {
    int w, h, n = (int)strlen(s), i;
    const uint32_t* img = hle_friends_picture("PmFriendListNumMat", &w, &h);
    float k = height / (DIGIT_Y1 - DIGIT_Y0), width = number_width(s, height);
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

/* Row i at y (its top), from x 0 to row_width(). */
static void draw_row(int i, float y) {
    int w, h, status = hle_friends_status(i);
    const uint32_t* tag = hle_friends_picture("PmFriendsMat", &w, &h);
    const uint32_t* icon;
    float x0 = 0, x1 = row_width(), k = ROW_HEIGHT / (kTagLeft.y1 - kTagLeft.y0);
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
    draw_number(number, x0 + 100, cy, 30);
    icon = status ? hle_friends_icon(i, &w, &h) : NULL;
    if (!icon) icon = hle_friends_picture("IcQuestion01", &w, &h);
    if (icon) {
        Rect src = {0, 0, (float)w, (float)h}, dst = {x0 + 110, cy - 38, x0 + 186, cy + 38};
        draw_image(icon, w, h, src, dst, 0xFFFFFF, 0);
    }
    draw_text(status && hle_friends_name(i)[0] ? hle_friends_name(i) : "???", x0 + 198, cy, 40, ALIGN_LEFT, 0xFFFFFFFFu,
              NAVY, 3.0f, 0, rx - 64 - (x0 + 198));
    if (status) {
        const uint32_t* book = hle_friends_picture("FriendIcon", &w, &h);
        if (book) {
            Rect src = {status == 2 ? w / 2.0f : 0, 0, status == 2 ? (float)w : w / 2.0f, (float)h};
            Rect dst = {rx - 62, cy - 28, rx - 6, cy + 28};
            draw_image(book, w, h, src, dst, 0xFFFFFF, 0);
        }
    }
}

/* Rows drawn, each a strip as wide as a row, clear around its tag. */
#define STRIPS 40
#define AHEAD 16 /* rows drawn ahead, each way from the middle (fewer than STRIPS in all) */
static struct {
    int row, status, w;
    float scale;
    uint32_t* px;
} sStrips[STRIPS];
static unsigned sStripUse[STRIPS], sUses;

static int strip_width(void) { return (int)ceilf(row_width() * sScale); }

/* Row i's strip, if drawn; else -1, and the one to draw it in. */
static int strip_of(int i, int* oldest) {
    int k, status = hle_friends_status(i);
    *oldest = 0;
    for (k = 0; k < STRIPS; k++) {
        if (sStrips[k].px && sStrips[k].row == i && sStrips[k].status == status && sStrips[k].w == strip_width() &&
            sStrips[k].scale == sScale)
            return k;
        if (sStripUse[k] < sStripUse[*oldest]) *oldest = k;
    }
    return -1;
}

static const uint32_t* strip(int i, int rowPx) {
    int k, oldest, status = hle_friends_status(i), w = strip_width(), savedW = sW, savedH = sH;
    uint32_t* saved = sPx;
    float savedX = sOffX, savedY = sOffY;
    if ((k = strip_of(i, &oldest)) >= 0) {
        sStripUse[k] = ++sUses;
        return sStrips[k].px;
    }
    k = oldest;
    free(sStrips[k].px);
    sStrips[k].px = (uint32_t*)calloc((size_t)w * rowPx, 4);
    if (!sStrips[k].px) return NULL;
    sStrips[k].row = i;
    sStrips[k].status = status;
    sStrips[k].w = w;
    sStrips[k].scale = sScale;
    sStripUse[k] = ++sUses;
    sPx = sStrips[k].px;
    sW = w;
    sH = rowPx;
    sOffX = sOffY = 0;
    draw_row(i, (rowPx / sScale - ROW_HEIGHT) / 2);
    sPx = saved;
    sW = savedW;
    sH = savedH;
    sOffX = savedX;
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

/* Lays strip s (sw x sh) over the image at pixel (x, y), only on its rows
 * from y0 to y1. */
static void lay(const uint32_t* s, int sw, int sh, int x, int y, int y0, int y1) {
    int a, b;
    for (b = 0; b < sh; b++) {
        int py = y + b;
        uint32_t* d;
        if (py < y0 || py >= y1 || py < 0 || py >= sH) continue;
        d = sPx + (size_t)py * sW;
        for (a = 0; a < sw; a++) {
            uint32_t c = s[(size_t)b * sw + a], al = c >> 24, o = 0xFF000000u;
            int px = x + a, k;
            if (!al || px < 0 || px >= sW) continue;
            if (al == 255) {
                d[px] = c;
                continue;
            }
            for (k = 0; k < 24; k += 8) o |= (((c >> k & 255) * al + (d[px] >> k & 255) * (255 - al)) / 255) << k;
            d[px] = o;
        }
    }
}

/* The pages and their titles: what doesn't change. */
static void draw_pages(void) {
    static const Rect kWhole = {0, 0, 0, 0}, kLeaves = {1, 4, 63, 232};
    Rect l = left_page(), r = right_page(), m = map_rect();
    int k;
    for (k = 0; k < sW * sH; k++) sPx[k] = BEHIND;
    picture("PmPocketbookR", "PmPocketbookRA", kWhole, r);
    picture("PmReaf01", "PmReaf01A", kLeaves, part(r, 0.79f, 0.05f, 0.96f, 0.56f));
    picture("PmPocketbookL01", "PmPocketbookL01A", kWhole, l);
    draw_text("Map", (m.x0 + m.x1) / 2, r.y0 + 0.085f * REF_H, 54, ALIGN_CENTRE, BROWN, 0, 0, 1, 0);
    /* The photo: its shadow, its border, and dark where the map goes. */
    {
        Rect shadow = {m.x0 - PHOTO_EDGE + 6, m.y0 - PHOTO_EDGE + 8, m.x1 + PHOTO_EDGE + 6, m.y1 + PHOTO_EDGE + 8};
        Rect border = {m.x0 - PHOTO_EDGE, m.y0 - PHOTO_EDGE, m.x1 + PHOTO_EDGE, m.y1 + PHOTO_EDGE};
        draw_box(shadow, 6, 0x50000000u | (EDGE & 0xFFFFFF));
        draw_box(border, 4, PHOTO);
        draw_box(m, 1, BEHIND);
    }
}

void friends_touch(int action, float x, float y) {
    if (sScale <= 0) return;
    x /= sScale;
    y /= sScale;
    if (action == 0) {
        Rect l = left_page();
        if (x < l.x0 || x >= l.x1) return; /* only on the list's page */
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

const uint32_t* friends_draw(int w, int h, int* changed, int* map) {
    int all = 0;
    static uint32_t *px, *pages;
    static unsigned drawnChanges = ~0u, drawnMoves = ~0u;
    static int drawnW, drawnH;
    int rows, pw, rowPx, i, first, last, count = 0, fresh = 0;
    float top, limit;
    char text[16];
    Rect list, mr;
    *changed = 0;
    if (w <= 0 || h <= 0 || !hle_font_metrics(&sCellW, &sCellH, &sBaseline) || !(rows = hle_friends_rows()) ||
        !hle_pause_image("PmPocketbookL01", NULL, &pw, &pw))
        return NULL;
    sW = w;
    sH = h;
    sScale = h / REF_H;
    sOffX = sOffY = 0;
    mr = map_rect();
    map[0] = (int)X(mr.x0);
    map[1] = (int)Y(mr.y0);
    map[2] = (int)(X(mr.x1) - X(mr.x0));
    map[3] = (int)(Y(mr.y1) - Y(mr.y0));
    list = list_rect();
    sRowWidth = list.x1 - list.x0 - BAR - 8;
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
    rowPx = (int)ceilf(ROW_PITCH * sScale);
    if (px && w == drawnW && h == drawnH && drawnChanges == hle_friends_changes() && drawnMoves == sMoves) {
        /* Nothing changed: a row near those shown drawn ahead. */
        int oldest, middle = (int)(sScroll / ROW_PITCH) + 4, d;
        sPx = px;
        for (d = 0; d <= 2 * AHEAD; d++) {
            i = middle + (d % 2 ? -(d + 1) / 2 : d / 2);
            if (i >= 0 && i < rows && strip_of(i, &oldest) < 0) {
                strip(i, rowPx);
                break;
            }
        }
        return px;
    }
    if (w != drawnW || h != drawnH || !px) {
        free(px);
        free(pages);
        px = (uint32_t*)malloc((size_t)w * h * 4);
        pages = (uint32_t*)malloc((size_t)w * h * 4);
        if (!px || !pages) {
            free(px);
            free(pages);
            px = pages = NULL;
            return NULL;
        }
        sPx = pages;
        draw_pages();
        forget_strips();
        all = 1;
    }
    if (drawnChanges != hle_friends_changes()) {
        forget_strips();
        all = 1;
    }
    drawnW = w;
    drawnH = h;
    drawnChanges = hle_friends_changes();
    drawnMoves = sMoves;
    /* Only scrolled: only the list's part changed. */
    *changed = all ? 1 : 2;
    changed[1] = (int)X(list.x0);
    changed[2] = (int)Y(list.y0);
    changed[3] = (int)ceilf(X(list.x1)) - changed[1];
    changed[4] = (int)ceilf(Y(list.y1)) - changed[2];
    sPx = px;
    if (all) {
        memcpy(px, pages, (size_t)w * h * 4);
    } else {
        int y;
        for (y = changed[2]; y < changed[2] + changed[4] && y < h; y++)
            memcpy(px + (size_t)y * w + changed[1], pages + (size_t)y * w + changed[1], (size_t)changed[3] * 4);
    }
    /* The rows, in the list's part of the page. */
    top = list.y0 - sScroll;
    first = (int)(sScroll / ROW_PITCH);
    last = (int)((sScroll + list.y1 - list.y0) / ROW_PITCH) + 1;
    /* At most two rows drawn a frame: those past them, flung onto the
     * page, the next frame. */
    for (i = first < 0 ? 0 : first; i <= last && i < rows; i++) {
        int oldest;
        const uint32_t* s;
        if (strip_of(i, &oldest) < 0 && fresh++ >= 2) {
            sMoves++;
            continue;
        }
        s = strip(i, rowPx);
        if (s)
            lay(s, strip_width(), rowPx, (int)X(list.x0), (int)floorf(Y(top + i * ROW_PITCH)), (int)Y(list.y0),
                (int)Y(list.y1));
    }
    /* The title, and how many are friends beside it: together in the
     * middle of the page's label (its tape, measured on the page). */
    if (all) {
        Rect tape = part(left_page(), 0.09f, 0.068f, 0.795f, 0.149f);
        float y = (tape.y0 + tape.y1) / 2, title, number, x;
        for (i = 0; i < rows; i++) count += hle_friends_status(i) != 0;
        snprintf(text, sizeof(text), "%d/%d", count, rows);
        title = text_width("Friends", 54);
        number = number_width(text, 36);
        x = (tape.x0 + tape.x1) / 2 - (title + 28 + number) / 2;
        draw_text("Friends", x, y, 54, ALIGN_LEFT, BROWN, 0, 0, 1, 0);
        draw_number(text, x + title + 28 + number, y + 2, 36);
    }
    /* The scroll bar. */
    if (limit > 0) {
        int tw, th;
        const uint32_t* img = hle_friends_picture("PmFriendsMat", &tw, &th);
        float x1 = list.x1, x0 = x1 - BAR, y0 = list.y0, y1 = list.y1;
        float size = (y1 - y0) * (y1 - y0) / (rows * ROW_PITCH);
        float at;
        Rect track = {x0 + 7, y0, x1 - 7, y1};
        if (size < 60) size = 60;
        at = y0 + (y1 - y0 - size) * sScroll / limit;
        draw_box(track, 4, 0x40000000u | (EDGE & 0xFFFFFF));
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
