/* The friends list a second screen shows (friends.c): the PokéPark Pad's
 * Friend List, from the game's own data and pictures.
 *
 * FriendNoteData (at 0x803A2228, set up as the park loads) lists the
 * Pokémon, Pikachu first: 16 bytes each, the character at +0 and its
 * number in the Pad in the low 15 bits of +6. A character's internal name is in a table of char*
 * (0x80317850); its icon is the texture "Ic" + that name + "01" (64x64),
 * and its English name the message 10000 + character in the resident
 * messages (a BMG whose MID1 has ids 0x01000000 | n). Whether the player
 * has befriended one, or made it a best friend, the game answers
 * (fn_8017E120, fn_8017E3A0, on the story's friends record: +0x18 of
 * FriendNoteSingleModeSaveDataOne is is_best_friend), asked from the
 * minimap's drawing (hle_gx.c), which is on the game's thread in the park.
 *
 * The icons, the list's own pictures (Pause.brres: the blue tags, the
 * digits, the friend books) and the messages are all resident, found by a
 * search of memory once the list is first wanted, and decoded when first
 * drawn. */
#include <stdlib.h>
#include <string.h>

#include "hle.h"

#define FRIEND_NOTE 0x803A2228u /* +8 the entries, +0xC how many */
#define CHAR_NAMES 0x80317850u
#define MAX_ROWS 256
#define TEX0 0x54455830u
#define SINGLE_MODE_GET 0x801273C0u /* FriendNoteSingleModeSaveData::virt0x8 */
#define NAME_ID(chr) (0x01000000u | (10000u + (chr)))

uint32_t* hle_pause_decode(uint32_t tex, int* w, int* h); /* hle_pause.c */

typedef struct {
    uint32_t tex; /* its TEX0, 0 if not found */
    char name[40];
    int w, h;
    uint32_t* argb; /* decoded, once drawn */
} Pic;

static struct {
    uint32_t chr;
    char name[32]; /* in English, "" until the messages are found */
    int status;    /* 0 not a friend, 1 a friend, 2 a best friend */
    Pic icon;
} sRows[MAX_ROWS];
static int sCount;       /* rows, Pikachu left out */
static uint32_t sTable;  /* FriendNoteData's entries the rows are of */
static uint32_t sBmg;    /* the messages with the names */
static uint64_t sSearched, sWanted;
static int sSearches; /* since the rows were set up */
static unsigned sChanges;

static const char* const kPics[] = {"IcQuestion01", "FriendIcon", "PmFriendListNumMat", "PmFriendsMat", "PmFriendsMatA"};
#define PICS ((int)(sizeof(kPics) / sizeof(kPics[0])))
static Pic sPics[PICS];

static int phys_ok(uint32_t a) { return (a >= 0x80000000u && a < 0x81800000u) || (a >= 0x90000000u && a < 0x94000000u); }

static void read_name(uint32_t at, char* out, int size) {
    int n;
    for (n = 0; n < size - 1 && phys_ok(at + n) && LD8(at + n) >= 32 && LD8(at + n) < 127; n++) out[n] = (char)LD8(at + n);
    out[n] = 0;
}

static void drop(Pic* p) {
    free(p->argb);
    p->argb = NULL;
    p->tex = 0;
}

/* A message's text, in a BMG; 0 if it has none of that id. */
static uint32_t message(uint32_t bmg, uint32_t id) {
    uint32_t sec = bmg + 0x20, end = bmg + LD32(bmg + 8), inf = 0, dat = 0, mid = 0, k, n;
    for (k = 0; k < LD32(bmg + 0xC) && sec + 8 <= end && LD32(sec + 4); k++, sec += LD32(sec + 4)) {
        if (LD32(sec) == 0x494E4631u) inf = sec; /* INF1 */
        if (LD32(sec) == 0x44415431u) dat = sec; /* DAT1 */
        if (LD32(sec) == 0x4D494431u) mid = sec; /* MID1 */
    }
    if (!inf || !dat || !mid) return 0;
    n = LD16(inf + 8);
    for (k = 0; k < n && k < LD16(mid + 8); k++) {
        if (LD32(mid + 0x10 + 4 * k) == id) return dat + 8 + LD32(inf + 0x10 + k * LD16(inf + 0xA));
    }
    return 0;
}

/* A message's text as Latin-1, its tags (0x1A, their length at +2) left
 * out. */
static void message_text(uint32_t s, char* out, int size) {
    int n = 0, i;
    for (i = 0; i < 64 && n < size - 1; i++) {
        uint16_t c = LD16(s);
        if (!c) break;
        if (c == 0x1A) {
            s += LD8(s + 2) > 2 ? LD8(s + 2) : 2;
            continue;
        }
        out[n++] = (char)(c < 256 ? c : '?');
        s += 2;
    }
    out[n] = 0;
}

/* Searches memory for the icons, the pictures and the messages. */
static void search(void) {
    static const uint32_t kRange[2][2] = {{0x80000000u, 0x81800000u}, {0x90000000u, 0x94000000u}};
    int r, i;
    for (r = 0; r < 2; r++) {
        uint32_t a;
        for (a = kRange[r][0]; a + 0x40 < kRange[r][1]; a += 4) {
            uint32_t v = LD32(a), file, name;
            char buf[40];
            if (v == 0x4D455347u && LD32(a + 4) == 0x626D6731u) { /* MESGbmg1 */
                if (!sBmg && a + LD32(a + 8) <= kRange[r][1] && message(a, NAME_ID(0))) sBmg = a;
                continue;
            }
            if (v != TEX0) continue;
            file = a + LD32(a + 0xC);
            name = a + LD32(a + 0x14);
            if (file < kRange[r][0] || file >= a || name <= a || name >= kRange[r][1] - 40 || LD32(file) != 0x62726573)
                continue;
            read_name(name, buf, sizeof(buf));
            for (i = 0; i < PICS; i++) {
                if (!sPics[i].tex && !strcmp(buf, kPics[i])) {
                    sPics[i].tex = a;
                    strcpy(sPics[i].name, buf);
                }
            }
            if (buf[0] != 'I' || buf[1] != 'c') continue;
            for (i = 0; i < sCount; i++) {
                if (!sRows[i].icon.tex && !strcmp(buf, sRows[i].icon.name)) sRows[i].icon.tex = a;
            }
        }
    }
}

/* The rows, set up again if the game's list changed. */
static void set_up(void) {
    uint32_t table = LD32(FRIEND_NOTE + 8), n = LD32(FRIEND_NOTE + 0xC), i;
    if (n < 2 || !phys_ok(table)) {
        sCount = 0;
        return;
    }
    if (table != sTable || (int)n - 1 != sCount) {
        for (i = 0; i < MAX_ROWS; i++) drop(&sRows[i].icon);
        sTable = table;
        sCount = n - 1 > MAX_ROWS ? MAX_ROWS : (int)n - 1;
        /* In the Pad's order, by number; in the table's if the numbers
         * aren't each row's once. */
        for (i = 0; i < (uint32_t)sCount; i++) sRows[i].chr = ~0u;
        for (i = 0; i < (uint32_t)sCount; i++) {
            uint32_t number = LD16(table + 16 * (i + 1) + 6) & 0x7FFF;
            if (number < 1 || number > (uint32_t)sCount || sRows[number - 1].chr != ~0u) break;
            sRows[number - 1].chr = LD32(table + 16 * (i + 1));
        }
        for (i = 0; i < (uint32_t)sCount; i++) {
            uint32_t chr = sRows[i].chr, at;
            char name[32] = "";
            if (chr == ~0u) { /* (the numbers were no good) */
                for (i = 0; i < (uint32_t)sCount; i++) sRows[i].chr = LD32(table + 16 * (i + 1));
                i = 0;
                chr = sRows[0].chr;
            }
            at = LD32(CHAR_NAMES + 4 * chr);
            if (chr < 1000 && phys_ok(at)) read_name(at, name, sizeof(name));
            sRows[i].name[0] = 0;
            sRows[i].status = 0;
            strcpy(sRows[i].icon.name, "Ic");
            strcat(sRows[i].icon.name, name);
            strcat(sRows[i].icon.name, "01");
        }
        sSearched = 0;
        sSearches = 0;
        sChanges++;
    }
    /* Searched once, and again (every 5 seconds, a few times at most) if
     * the messages weren't found or what was found went. */
    if (!sSearched || (!sBmg && sSearches < 4 && ppc_timebase() - sSearched > 5 * PPC_TB_HZ)) {
        sSearched = ppc_timebase();
        sSearches++;
        search();
        for (i = 0; i < (uint32_t)sCount; i++) {
            uint32_t s = sBmg ? message(sBmg, NAME_ID(sRows[i].chr)) : 0;
            if (s) message_text(s, sRows[i].name, sizeof(sRows[i].name));
        }
        sChanges++;
    }
}

int hle_friends_rows(void) {
    sWanted = ppc_timebase();
    set_up();
    return sCount;
}

unsigned hle_friends_changes(void) { return sChanges; }

/* A message of the resident ones, as Latin-1; "" if there's none. */
const char* hle_friends_message(uint32_t id, char* out, int size) {
    uint32_t s = sBmg && LD32(sBmg) == 0x4D455347u ? message(sBmg, id) : 0;
    out[0] = 0;
    if (s) message_text(s, out, size);
    return out;
}

const char* hle_friends_name(int i) { return i >= 0 && i < sCount ? sRows[i].name : ""; }

int hle_friends_status(int i) { return i >= 0 && i < sCount ? sRows[i].status : 0; }

static const uint32_t* decoded(Pic* p, int* w, int* h) {
    char buf[40];
    if (!p->tex) return NULL;
    read_name(p->tex + LD32(p->tex + 0x14), buf, sizeof(buf));
    if (LD32(p->tex) != TEX0 || strcmp(buf, p->name)) { /* gone */
        drop(p);
        sBmg = 0; /* searched again */
        sSearches = 0;
        return NULL;
    }
    if (!p->argb) p->argb = hle_pause_decode(p->tex, &p->w, &p->h);
    *w = p->w;
    *h = p->h;
    return p->argb;
}

const uint32_t* hle_friends_icon(int i, int* w, int* h) {
    return i >= 0 && i < sCount ? decoded(&sRows[i].icon, w, h) : NULL;
}

/* One of the list's pictures (kPics); PmFriendsMat with PmFriendsMatA's
 * grey as its alpha. */
const uint32_t* hle_friends_picture(const char* name, int* w, int* h) {
    static uint32_t* merged;
    const uint32_t *c, *a;
    int i, aw, ah, k;
    for (i = 0; i < PICS && strcmp(kPics[i], name); i++) {}
    if (i == PICS || !(c = decoded(&sPics[i], w, h))) return NULL;
    if (strcmp(name, "PmFriendsMat")) return c;
    if (!(a = decoded(&sPics[i + 1], &aw, &ah)) || aw != *w || ah != *h) return NULL;
    if (!merged) {
        merged = (uint32_t*)malloc((size_t)aw * ah * 4);
        if (!merged) return NULL;
        for (k = 0; k < aw * ah; k++) merged[k] = (c[k] & 0xFFFFFFu) | (a[k] & 0xFFu) << 24;
    }
    return merged;
}

static int is_story(uint32_t record) {
    uint32_t list = LD32(record + 4);
    return phys_ok(list) && phys_ok(LD32(list)) && LD32(LD32(list) + 8) == SINGLE_MODE_GET;
}

/* Asks the game who are friends: from the minimap's drawing, a few times
 * a second while the list is drawn. */
void hle_friends_update(ppc_t* c) {
    static unsigned frames;
    uint32_t lr = c->lr, g, record;
    int i, changed = 0;
    if (!sCount || !sWanted || ppc_timebase() - sWanted > PPC_TB_HZ || frames++ % 15) return;
    f_8002E840(c);
    g = c->r[3];
    /* The story's record (the attraction mode's has no best friends):
     * the one whose list is a FriendNoteSingleModeSaveData. */
    record = g + 0x1D4;
    if (!is_story(record)) record = g + 0x1180;
    if (!is_story(record)) return;
    for (i = 0; i < sCount; i++) {
        int status = 0;
        c->r[3] = record;
        c->r[4] = sRows[i].chr;
        f_8017E120(c);
        if (c->r[3] & 0xFF) {
            c->r[3] = record;
            c->r[4] = sRows[i].chr;
            f_8017E3A0(c);
            status = c->r[3] & 0xFF ? 2 : 1;
        }
        if (status != sRows[i].status) {
            sRows[i].status = status;
            changed = 1;
        }
    }
    c->lr = lr;
    if (changed) sChanges++;
}
