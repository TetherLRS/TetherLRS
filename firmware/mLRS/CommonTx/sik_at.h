//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// SiK AT commands
//********************************************************
// TetherLRS: the AT commands of SiK radios (SiK 2.x at.c) in the serial escape's command mode, so that programs
// which configure SiK radios (Mission Planner's SiK page) also work here. AT = this Tx, RT = the receiver.
// The S-registers keep SiK's numbers. Those with a TetherLRS counterpart map onto its parameters, NETID onto the
// bind phrase ("sik025" for 25); the others read as fixed values and accept only these values.
// This part has no firmware dependencies, so it can be tested on a PC.
//********************************************************
#ifndef SIK_AT_H
#define SIK_AT_H
#pragma once


#include <inttypes.h>
#include <string.h>
#include <ctype.h>
#include <math.h>


typedef enum {
    SIK_AT_OK = 0,      // AT
    SIK_AT_INFO,        // ATIn
    SIK_AT_EXIT,        // ATO
    SIK_AT_RESET,       // ATZ
    SIK_AT_WRITE,       // AT&W
    SIK_AT_GET,         // ATSn?
    SIK_AT_SET,         // ATSn=v
    SIK_AT_ERROR,       // anything else starting with AT or RT
} SIK_AT_CMD_ENUM;

typedef enum {
    SIK_S_FORMAT = 0,
    SIK_S_SERIAL_SPEED,
    SIK_S_AIR_SPEED,
    SIK_S_NETID,
    SIK_S_TXPOWER,
    SIK_S_ECC,
    SIK_S_MAVLINK,
    SIK_S_OPPRESEND,
    SIK_S_MIN_FREQ,
    SIK_S_MAX_FREQ,
    SIK_S_NUM_CHANNELS,
    SIK_S_DUTY_CYCLE,
    SIK_S_LBT_RSSI,
    SIK_S_MANCHESTER,
    SIK_S_RTSCTS,
    SIK_S_MAX_WINDOW,
    SIK_S_NUM,
} SIK_S_ENUM;

const char* const sik_s_names[SIK_S_NUM] = {
    "FORMAT", "SERIAL_SPEED", "AIR_SPEED", "NETID", "TXPOWER", "ECC", "MAVLINK", "OPPRESEND",
    "MIN_FREQ", "MAX_FREQ", "NUM_CHANNELS", "DUTY_CYCLE", "LBT_RSSI", "MANCHESTER", "RTSCTS", "MAX_WINDOW",
};

#define SIK_NETID_NUM  500 // SiK's range 0-499


typedef struct {
    bool remote;
    uint8_t cmd;
    uint8_t n; // ATIn, ATSn
    uint32_t value; // ATSn=v
} tSikAtCmd;


// false if the line is not an AT or RT command at all
bool sik_at_parse(const char* line, tSikAtCmd* const c)
{
    char s[32];
    uint8_t len = 0;
    for (; *line && len < sizeof(s) - 1; line++) {
        if (*line == ' ') continue;
        s[len++] = toupper(*line);
    }
    s[len] = '\0';

    if (len < 2 || s[1] != 'T' || (s[0] != 'A' && s[0] != 'R')) return false;

    c->remote = (s[0] == 'R');
    c->n = 0;
    c->value = 0;
    const char* p = s + 2;

    if (*p == '\0') { c->cmd = SIK_AT_OK; return true; }
    if (!strcmp(p, "O")) { c->cmd = SIK_AT_EXIT; return true; }
    if (!strcmp(p, "Z")) { c->cmd = SIK_AT_RESET; return true; }
    if (!strcmp(p, "&W")) { c->cmd = SIK_AT_WRITE; return true; }
    if (p[0] == 'I' && (p[1] == '\0' || (isdigit(p[1]) && p[2] == '\0'))) {
        c->cmd = SIK_AT_INFO;
        c->n = (p[1]) ? p[1] - '0' : 0;
        return true;
    }

    c->cmd = SIK_AT_ERROR;
    if (p[0] != 'S' || !isdigit(p[1])) return true;
    p++;
    uint32_t n = 0;
    while (isdigit(*p)) { n = 10 * n + (*p - '0'); p++; if (n >= SIK_S_NUM) return true; }
    c->n = n;
    if (!strcmp(p, "?")) { c->cmd = SIK_AT_GET; return true; }
    if (*p != '=' || !isdigit(p[1])) return true;
    p++;
    uint32_t v = 0;
    while (isdigit(*p)) { if (v > 100000000) return true; v = 10 * v + (*p - '0'); p++; }
    if (*p != '\0') return true;
    c->value = v;
    c->cmd = SIK_AT_SET;
    return true;
}


// decimal, no padding; s needs 11 chars
char* sik_utoa(uint32_t v, char* const s)
{
    char t[10];
    uint8_t n = 0;
    do { t[n++] = '0' + v % 10; v /= 10; } while (v);
    for (uint8_t i = 0; i < n; i++) s[i] = t[n - 1 - i];
    s[n] = '\0';
    return s;
}


// NETID n <-> bind phrase "sikNNN"; any other phrase reads as a number derived from it
uint16_t sik_netid_from_bindphrase(const char* const bp)
{
    if (bp[0] == 's' && bp[1] == 'i' && bp[2] == 'k' && isdigit(bp[3]) && isdigit(bp[4]) && isdigit(bp[5])) {
        uint16_t n = 100 * (bp[3] - '0') + 10 * (bp[4] - '0') + (bp[5] - '0');
        if (n < SIK_NETID_NUM) return n;
    }
    uint32_t h = 2166136261u; // FNV-1a
    for (uint8_t i = 0; i < 6; i++) { h ^= (uint8_t)bp[i]; h *= 16777619u; }
    return h % SIK_NETID_NUM;
}

void sik_bindphrase_from_netid(char* const bp, uint16_t netid) // bp gets 6 chars and '\0'
{
    bp[0] = 's'; bp[1] = 'i'; bp[2] = 'k';
    bp[3] = '0' + (netid / 100) % 10;
    bp[4] = '0' + (netid / 10) % 10;
    bp[5] = '0' + netid % 10;
    bp[6] = '\0';
}


// SERIAL_SPEED is the baud rate in thousands, as SiK
const uint16_t sik_serial_speeds[] = { 57, 115, 230 }; // in the order of SERIAL_BAUDRATE_ENUM

int8_t sik_serial_speed_to_idx(uint32_t v)
{
    for (uint8_t i = 0; i < sizeof(sik_serial_speeds)/sizeof(sik_serial_speeds[0]); i++) {
        if (sik_serial_speeds[i] == v) return i;
    }
    return -1;
}


// AIR_SPEED in kbps, from the downlink data rate in bytes/s, rounded to the nearest SiK value on a log scale
const uint16_t sik_air_speeds[] = { 4, 8, 16, 19, 24, 32, 48, 64, 96, 128, 192, 250 };

uint16_t sik_air_speed_from_rate(uint16_t bytes_per_sec)
{
    float kbps = 8.0f * bytes_per_sec / 1000.0f;
    uint16_t best = sik_air_speeds[0];
    float best_d = 1.0e9f;
    for (uint8_t i = 0; i < sizeof(sik_air_speeds)/sizeof(sik_air_speeds[0]); i++) {
        float d = fabsf(logf(kbps / sik_air_speeds[i]));
        if (d < best_d) { best_d = d; best = sik_air_speeds[i]; }
    }
    return best;
}

// among those set in allowed_mask: the first index that reads back as kbps, else the closest on a log scale; -1 if none
int8_t sik_pick_rate(uint32_t kbps, const uint16_t* const rates_bytes_per_sec, uint8_t num, uint32_t allowed_mask)
{
    if (kbps == 0) return -1;
    for (uint8_t i = 0; i < num; i++) {
        if (!(allowed_mask & (1UL << i)) || !rates_bytes_per_sec[i]) continue;
        if (sik_air_speed_from_rate(rates_bytes_per_sec[i]) == kbps) return i;
    }
    int8_t best = -1;
    float best_d = 1.0e9f;
    for (uint8_t i = 0; i < num; i++) {
        if (!(allowed_mask & (1UL << i)) || !rates_bytes_per_sec[i]) continue;
        float d = fabsf(logf(8.0f * rates_bytes_per_sec[i] / 1000.0f / kbps));
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}


#endif // SIK_AT_H
