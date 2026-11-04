/* SparkPark: the disc checks behind the set-up (see disc_verify.h). */
#include "disc_verify.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The text after the first "key" in out, its leading spaces skipped. */
static const char* after(const char* out, const char* key) {
    const char* p = out ? strstr(out, key) : NULL;
    if (!p) return NULL;
    p += strlen(key);
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* Copies the run of hex digits at p, lowercased. */
static void hex_at(const char* p, char* out, size_t n) {
    size_t i = 0;
    while (p && i + 1 < n && isxdigit((unsigned char)p[i])) {
        out[i] = (char)tolower((unsigned char)p[i]);
        i++;
    }
    out[i] = '\0';
}

static const char* region_name(char c) {
    switch (c) {
    case 'E': return "the USA release";
    case 'P': return "the European release";
    case 'J': return "the Japanese release";
    case 'K': return "the Korean release";
    case 'W': return "the Taiwanese release";
    default: return "another region's release";
    }
}

static int is_iso(const char* info) {
    const char* f = after(info, "Format:");
    return f && !strncmp(f, "ISO", 3);
}

disc_result disc_check_info(const char* info, const disc_file* file, char* msg, size_t n) {
    const char* id = after(info, "Game ID:");
    const char* rev = after(info, "Revision");
    const char* lossless = after(info, "Lossless:");
    if (!id || strlen(id) < 6) {
        snprintf(msg, n, "That file couldn't be read as a disc image. Choose a disc image of PokePark Wii (USA): "
                         "an ISO, RVZ, WBFS or the like.");
        return DISC_UNREADABLE;
    }
    if (strncmp(id, "R8A", 3) != 0 || strncmp(id + 4, "01", 2) != 0) {
        snprintf(msg, n, "That disc is another game (%.6s), not PokePark Wii: Pikachu's Adventure (USA, R8AE01).", id);
        return DISC_WRONG_GAME;
    }
    if (id[3] != 'E') {
        snprintf(msg, n, "That's %s of PokePark Wii (%.6s). SparkPark needs the USA release, R8AE01.",
                 region_name(id[3]), id);
        return DISC_WRONG_REGION;
    }
    if (!rev || atoi(rev) != 0) {
        snprintf(msg, n, "That's revision %d of PokePark Wii (USA). SparkPark needs the first release, Rev 0.",
                 rev ? atoi(rev) : -1);
        return DISC_WRONG_REVISION;
    }
    if ((file && is_iso(info) && !memcmp(file->magic200, "NKIT", 4)) || (lossless && !strncmp(lossless, "false", 5))) {
        snprintf(msg, n, "That's an NKit or scrubbed image, not a full copy of the disc. Convert it back to a full "
                         "ISO (NKit's \"Recover to ISO\"), or use an unmodified dump.");
        return DISC_NKIT_SCRUBBED;
    }
    return DISC_OK;
}

disc_result disc_check_verify(const char* info, const char* verify, const disc_file* file, char* msg, size_t n) {
    char sha1[48], crc[16];
    hex_at(after(verify, "SHA-1 :"), sha1, sizeof(sha1));
    hex_at(after(verify, "CRC32 :"), crc, sizeof(crc));
    if (!sha1[0]) {
        snprintf(msg, n, "The disc image couldn't be read through to the end. Is the file complete?");
        return DISC_UNREADABLE;
    }
    if (!strcmp(sha1, DISC_REDUMP_SHA1) && (!crc[0] || !strcmp(crc, DISC_REDUMP_CRC32))) return DISC_OK;
    if (file && is_iso(info) && file->size != DISC_REDUMP_SIZE) {
        snprintf(msg, n, "That image is %lld bytes; a full dump of the disc is %lld. It looks scrubbed, trimmed or "
                         "NKit-compressed. Use an unmodified 1:1 dump.", file->size, DISC_REDUMP_SIZE);
        return DISC_NKIT_SCRUBBED;
    }
    snprintf(msg, n, "That disc image is PokePark Wii (USA, Rev 0) but doesn't match the original disc "
                     "(Redump): it has been modified, patched or had its padding scrubbed. Use an unmodified "
                     "dump of your disc.");
    return DISC_MODIFIED;
}
