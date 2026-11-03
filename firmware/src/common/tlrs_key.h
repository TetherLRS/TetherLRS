//*******************************************************
// Copyright (c) TetherLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.html
//*******************************************************
// The link key: 32 bytes both radios hold and never send, all zero = not set
//*******************************************************
#ifndef TLRS_KEY_H
#define TLRS_KEY_H
#pragma once

#include <stdint.h>
#include <string.h>

#define TLRS_KEY_LEN  32


bool tlrs_key_is_set(const uint8_t* const key)
{
    for (uint8_t i = 0; i < TLRS_KEY_LEN; i++) if (key[i]) return true;
    return false;
}


int8_t _tlrs_hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}


// 64 hex digits, or "none" to clear; false and the key untouched for anything else
bool tlrs_key_from_text(const char* const s, uint8_t* const key)
{
    if (strcmp(s, "none") == 0) { memset(key, 0, TLRS_KEY_LEN); return true; }

    uint8_t k[TLRS_KEY_LEN];
    for (uint8_t i = 0; i < TLRS_KEY_LEN; i++) {
        int8_t hi = _tlrs_hex_digit(s[2 * i]);
        if (hi < 0) return false;
        int8_t lo = _tlrs_hex_digit(s[2 * i + 1]);
        if (lo < 0) return false;
        k[i] = (hi << 4) | lo;
    }
    if (s[2 * TLRS_KEY_LEN] != '\0') return false;
    memcpy(key, k, TLRS_KEY_LEN);
    return true;
}


#endif // TLRS_KEY_H
