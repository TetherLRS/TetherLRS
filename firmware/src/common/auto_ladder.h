//*******************************************************
// Copyright (c) TetherLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.html
//*******************************************************
// Mode = auto: the ladder of (band, mode) steps the Tx moves the link along
//*******************************************************
// A step is a mode on a band, with its frame rate and its link budget: the power the region allows there, less the
// mode's sensitivity and the band's extra path loss over the lower band. The ladder keeps, from the fastest down,
// only the steps that buy robustness for the rate they give up: each next one is slower and has a better budget.
#ifndef AUTO_LADDER_H
#define AUTO_LADDER_H
#pragma once


#include <inttypes.h>


typedef struct {
    uint8_t band; // SETUP_FREQUENCY_BAND_ENUM
    uint8_t mode; // MODE_ENUM
    uint8_t rate; // frames a second
    int16_t budget; // dB
} tAutoStep;


// 10 log10(x / 100) to the nearest dB, for x >= 100, without floats
int8_t db_from_x100(uint32_t x100)
{
    static const uint32_t half_db_up[40] = { // 100 * 10^((d + 0.5) / 10)
        112, 141, 178, 224, 282, 355, 447, 562, 708, 891, 1122, 1413, 1778, 2239, 2818, 3548, 4467, 5623, 7079, 8913,
        11220, 14125, 17783, 22387, 28184, 35481, 44668, 56234, 70795, 89125, 112202, 141254, 177828, 223872, 281838,
        354813, 446684, 562341, 707946, 891251 };
    int8_t db = 0;
    while (db < 40 && x100 >= half_db_up[db]) db++;
    return db;
}


uint16_t band_mhz(uint8_t band)
{
    switch (band) {
    case SETUP_FREQUENCY_BAND_915_MHZ_FCC: return 915;
    case SETUP_FREQUENCY_BAND_868_MHZ: return 868;
    case SETUP_FREQUENCY_BAND_433_MHZ: return 433;
    case SETUP_FREQUENCY_BAND_70_CM_HAM: return 435;
    case SETUP_FREQUENCY_BAND_866_MHZ_IN: return 866;
    }
    return 2440;
}


// what a band loses over a lower one on the same path, free space: 20 log10(f / f_low)
int8_t band_offset_db(uint8_t band, uint8_t band_low)
{
    uint32_t f = band_mhz(band);
    uint32_t f_low = band_mhz(band_low);
    if (f <= f_low) return 0;
    return db_from_x100((f * f * 100) / (f_low * f_low));
}


// frames a second, by mode
uint8_t mode_rate_hz(uint8_t mode)
{
    switch (mode) {
    case MODE_50HZ: return 50;
    case MODE_31HZ: return 31;
    case MODE_19HZ:
    case MODE_19HZ_7X: return 19;
    case MODE_FLRC_111HZ: return 111;
    case MODE_FSK_50HZ: return 50;
    case MODE_18HZ: return 18;
    case MODE_LR: return 2;
    case MODE_17HZ: return 17;
    case MODE_8HZ: return 8;
    }
    return 19;
}


// the drivers' receiver sensitivities, by mode
int8_t mode_sensitivity_dbm(uint8_t mode)
{
    switch (mode) {
    case MODE_50HZ: return -105;
    case MODE_31HZ: return -108;
    case MODE_19HZ:
    case MODE_19HZ_7X: return -112;
    case MODE_FLRC_111HZ: return -104;
    case MODE_FSK_50HZ: return -106;
    case MODE_18HZ: return -112;
    case MODE_17HZ: return -114;
    }
    return -117; // MODE_LR, MODE_8HZ
}


// a band's base mode, where Mode = auto starts and meets again: 19 Hz, or the one the band has closest to it
uint8_t mode_auto_base(uint16_t modes)
{
    const uint8_t order[] = { MODE_19HZ, MODE_19HZ_7X, MODE_18HZ, MODE_17HZ, MODE_31HZ, MODE_8HZ, MODE_FSK_50HZ, MODE_50HZ, MODE_LR, MODE_FLRC_111HZ };
    for (uint8_t i = 0; i < sizeof(order); i++) {
        if (modes & (1 << order[i])) return order[i];
    }
    return MODE_19HZ;
}


// adds a candidate step, keeping them in order: rate down, then budget down; among equals the first added stays first
void auto_ladder_add(tAutoStep* const all, uint8_t* const n, tAutoStep st)
{
    uint8_t i = *n;
    while (i > 0 && (all[i-1].rate < st.rate || (all[i-1].rate == st.rate && all[i-1].budget < st.budget))) {
        all[i] = all[i-1];
        i--;
    }
    all[i] = st;
    (*n)++;
}


// the ladder from the ordered candidates: each step kept has a better budget than the faster one before it
uint8_t auto_ladder_front(const tAutoStep* const all, uint8_t n, tAutoStep* const steps)
{
    uint8_t num = 0;
    for (uint8_t i = 0; i < n; i++) {
        if (num == 0 || all[i].budget > steps[num - 1].budget) steps[num++] = all[i];
    }
    return num;
}


// the next step up is the closest faster one; down, the closest one no faster with a better budget; -1 for none
void auto_ladder_neighbours(const tAutoStep* const steps, uint8_t num, uint8_t cur_rate, int16_t cur_budget, int8_t* const up, int8_t* const down)
{
    *up = -1;
    *down = -1;
    for (uint8_t i = 0; i < num; i++) {
        if (steps[i].rate > cur_rate) *up = i;
        if (*down < 0 && steps[i].rate <= cur_rate && steps[i].budget > cur_budget) *down = i;
    }
}


#endif // AUTO_LADDER_H
