//*******************************************************
// Copyright (c) TetherLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.html
//*******************************************************
// Host test of the link key text form (src/common/tlrs_key.h) and the Link Split frame lengths. Run with test/run_tests.sh.
#include <cstdio>
#include <cstring>
#include "tlrs_key.h"

#define FRAME_TX_RX_LEN 91
#define FRAME_LEN_MIN 73
#define FRAME_LEN_MAX 109
#define FRAME_TX_RX_HEADER_LEN 7
#define FRAME_PAYLOAD_LEN(frame_len)  ((frame_len) - FRAME_TX_RX_HEADER_LEN - 2)
#define FRAME_CMD_PAYLOAD_LEN 64

static int fails = 0;
static void check(const char* name, bool ok)
{
    printf("%-76s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

int main(void)
{
    uint8_t key[32];
    memset(key, 0, 32);
    check("all zero is not set", !tlrs_key_is_set(key));
    key[31] = 1;
    check("one byte makes it set", tlrs_key_is_set(key));

    const char* hex = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1F";
    check("64 hex digits, both cases, are taken", tlrs_key_from_text(hex, key));
    bool bytes_ok = true;
    for (uint8_t i = 0; i < 32; i++) if (key[i] != i) bytes_ok = false;
    check("  and give the bytes in order", bytes_ok);

    uint8_t before[32];
    memcpy(before, key, 32);
    check("63 digits are refused", !tlrs_key_from_text("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1", key));
    check("65 digits are refused", !tlrs_key_from_text("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f0", key));
    check("a stray character is refused", !tlrs_key_from_text("0001020304050607g8090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", key));
    check("an empty text is refused", !tlrs_key_from_text("", key));
    check("  and the key is untouched by a refusal", memcmp(before, key, 32) == 0);
    check("none clears it", tlrs_key_from_text("none", key) && !tlrs_key_is_set(key));

    check("the split frames add up to two balanced ones", FRAME_LEN_MIN + FRAME_LEN_MAX == 2 * FRAME_TX_RX_LEN);
    check("the short frame still carries a command frame", FRAME_PAYLOAD_LEN(FRAME_LEN_MIN) == FRAME_CMD_PAYLOAD_LEN);
    check("the long frame carries 100 bytes", FRAME_PAYLOAD_LEN(FRAME_LEN_MAX) == 100);

    printf("%s\n", fails ? "FAILED" : "all ok");
    return fails ? 1 : 0;
}
