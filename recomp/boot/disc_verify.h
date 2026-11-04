/* SparkPark: checking the player's disc image is R8AE01 Rev 0, byte for
 * byte, before it is set up (import_win32.c). The checks read what
 * decomp-toolkit prints ("dtk disc info" and "dtk disc verify"), which reads
 * every image format the set-up accepts; this file has no OS calls, so it is
 * tested on its own (recomp/tests/test_disc_verify.cpp). */
#ifndef DISC_VERIFY_H
#define DISC_VERIFY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DISC_OK = 0,
    DISC_UNREADABLE,      /* not a disc image dtk can read */
    DISC_WRONG_GAME,      /* another game */
    DISC_WRONG_REGION,    /* PokePark Wii, but not the USA release */
    DISC_WRONG_REVISION,  /* USA, but not Rev 0 */
    DISC_NKIT_SCRUBBED,   /* NKit, scrubbed or trimmed: not a 1:1 dump */
    DISC_MODIFIED         /* the right header, but not the Redump dump */
} disc_result;

/* The Redump dump of PokePark Wii - Pikachu's Adventure (USA), R8AE01 Rev 0. */
#define DISC_REDUMP_SIZE 4699979776LL
#define DISC_REDUMP_CRC32 "26e533d5"
#define DISC_REDUMP_SHA1 "c9a6f7601fd54b5e21a0909084839d30874a9a37"

/* What the image file itself shows: the four bytes at 0x200 ("NKIT" in an
 * NKit image) and its size. Only meaningful for a plain ISO. */
typedef struct {
    char magic200[4];
    long long size;
} disc_file;

/* From "dtk disc info": the game ID, revision and whether the image is a
 * full copy. Writes a reason for the player into msg on failure. */
disc_result disc_check_info(const char* info, const disc_file* file, char* msg, size_t n);

/* From "dtk disc verify": the whole disc's hashes against Redump's. info is
 * the "dtk disc info" output again (for the format). */
disc_result disc_check_verify(const char* info, const char* verify, const disc_file* file, char* msg, size_t n);

#ifdef __cplusplus
}
#endif

#endif
