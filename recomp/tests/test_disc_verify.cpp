// SparkPark: checks for the disc verification (recomp/boot/disc_verify.c),
// on text shaped like decomp-toolkit's output. No game data. Runs before
// test_recomp.cpp's main through a static initializer and exits 1 on failure.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../boot/disc_verify.h"

namespace {

const char* kInfoGood = "Format: ISO\nLossless: true\n\nTitle: Pokepark Wii\nGame ID: R8AE01\nDisc 1, Revision 0\n";
const char* kVerifyGood = "Redump: PokePark Wii (USA) \xe2\x9c\x85\nCRC32 : 26e533d5 \xe2\x9c\x85\n"
                          "SHA-1 : c9a6f7601fd54b5e21a0909084839d30874a9a37 \xe2\x9c\x85\n";
const char* kVerifyOther = "CRC32 : 11111111 \xe2\x9d\x8c\nSHA-1 : 0123456789abcdef0123456789abcdef01234567\n";

int g_fail = 0;

void expect(const char* what, disc_result got, disc_result want, const char* msg) {
    if (got != want) {
        std::printf("disc_verify: %s: got %d, expected %d (%s)\n", what, got, want, msg);
        g_fail = 1;
    }
}

struct Run {
    Run() {
        char msg[512];
        disc_file iso = {{'\0', '\0', '\0', '\0'}, DISC_REDUMP_SIZE};
        disc_file nkit = {{'N', 'K', 'I', 'T'}, 1000000000LL};
        disc_file trimmed = {{'\0', '\0', '\0', '\0'}, 2000000000LL};

        expect("good info", disc_check_info(kInfoGood, &iso, msg, sizeof msg), DISC_OK, msg);
        expect("good verify", disc_check_verify(kInfoGood, kVerifyGood, &iso, msg, sizeof msg), DISC_OK, msg);
        expect("not a disc", disc_check_info("Failed: unknown format", &iso, msg, sizeof msg), DISC_UNREADABLE, msg);
        expect("other game", disc_check_info("Format: ISO\nGame ID: RSPE01\nDisc 1, Revision 0", &iso, msg, sizeof msg),
               DISC_WRONG_GAME, msg);
        expect("PAL", disc_check_info("Format: ISO\nGame ID: R8AP01\nDisc 1, Revision 0", &iso, msg, sizeof msg),
               DISC_WRONG_REGION, msg);
        expect("Japan", disc_check_info("Format: RVZ\nGame ID: R8AJ01\nDisc 1, Revision 0", &iso, msg, sizeof msg),
               DISC_WRONG_REGION, msg);
        expect("Rev 1", disc_check_info("Format: ISO\nGame ID: R8AE01\nDisc 1, Revision 1", &iso, msg, sizeof msg),
               DISC_WRONG_REVISION, msg);
        expect("NKit ISO", disc_check_info(kInfoGood, &nkit, msg, sizeof msg), DISC_NKIT_SCRUBBED, msg);
        expect("lossy RVZ", disc_check_info("Format: RVZ\nLossless: false\nGame ID: R8AE01\nDisc 1, Revision 0",
                                            &iso, msg, sizeof msg), DISC_NKIT_SCRUBBED, msg);
        expect("trimmed ISO", disc_check_verify(kInfoGood, kVerifyOther, &trimmed, msg, sizeof msg),
               DISC_NKIT_SCRUBBED, msg);
        expect("modified ISO", disc_check_verify(kInfoGood, kVerifyOther, &iso, msg, sizeof msg), DISC_MODIFIED, msg);
        expect("modified RVZ", disc_check_verify("Format: RVZ\nLossless: true", kVerifyOther, &trimmed, msg, sizeof msg),
               DISC_MODIFIED, msg);
        expect("verify failed", disc_check_verify(kInfoGood, "Failed: I/O error", &iso, msg, sizeof msg),
               DISC_UNREADABLE, msg);
        if (g_fail) std::exit(1);
        std::printf("disc_verify: 13 checks, 0 failures\n");
    }
} g_run;

}  // namespace
