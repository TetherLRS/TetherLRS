//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// TetherLRS: dynamic RF power
//********************************************************
// The configured power is the ceiling (the region caps it further). Each end steps its own power from what the other
// end reports about our frames: its RSSI over the receiver sensitivity of the mode, and its LQ. Both ends transmit
// data, so both do it. Up fast, down slow: a weak or lost link goes back to the ceiling at once, a weak-ish one climbs
// a step every 250 ms, a strong one comes down a step after 2 s of staying strong.
// The decisions use an average of the margin over about 8 frames: on the bench the RSSI of one frame swings by 30 dB
// at a fixed spot, and one dip must not undo the 2 s of strong link a step down waits for. The fast fades are caught
// by the rules on single frames: under 10 dB of margin or 70 % LQ the power goes back to the ceiling at once.
// A step down is taken only if the margin it leaves is still 5 dB over the step-up threshold, so the large steps of
// some tables (18 dB on ESP8285 receivers) do not make it hunt. The LQ the other end reports is counted over about a
// second, so a step up for LQ waits that long before the next one. And when a step down has to be undone within 10 s,
// the wait before the next step down doubles (up to 32 s), back to 2 s after a minute without going up: on the bench
// at the lowest power fades took the LQ to 88 %, and without it the power went down and up every few seconds.
#ifndef DYNAMIC_POWER_H
#define DYNAMIC_POWER_H
#pragma once

#include <stdint.h>


#define DYNPOWER_MARGIN_LOW_DB      10 // one frame below: straight to the ceiling
#define DYNPOWER_MARGIN_UP_DB       20 // average below: one step up
#define DYNPOWER_MARGIN_DOWN_DB     35 // average above, held: one step down
#define DYNPOWER_MARGIN_KEEP_DB     5  // a step down must leave the average this far over the step-up threshold
#define DYNPOWER_LQ_LOW             70
#define DYNPOWER_LQ_UP              90
#define DYNPOWER_LQ_DOWN            95
#define DYNPOWER_UP_MS              250
#define DYNPOWER_UP_LQ_MS           1000 // the LQ window
#define DYNPOWER_DOWN_HOLD_MS       2000
#define DYNPOWER_DOWN_HOLD_MAX_MS   32000
#define DYNPOWER_UNDONE_MS          10000 // a step up this soon after a step down undoes it
#define DYNPOWER_CALM_MS            60000 // no step up for this long: the wait goes back to 2 s
#define DYNPOWER_AVG_SHIFT          3  // average over about 8 frames


class tDynPower
{
  public:
    // the power table in dBm, the same indices as rfpower_list
    void Init(const int8_t* _table_dbm, uint8_t _num)
    {
        table_dbm = _table_dbm;
        num = _num;
        idx = UINT8_MAX;
        t_last_up_ms = 0;
        t_last_down_ms = 0;
        t_strong_ms = 0;
        down_hold_ms = DYNPOWER_DOWN_HOLD_MS;
        strong = false;
        went_down = false;
        avg_valid = false;
    }

    // once per frame cycle; peer_rssi_dbm >= 0 means the other end missed our last frame (mLRS reports 127 then)
    uint8_t Step(uint32_t tnow_ms, bool connected, int8_t peer_rssi_dbm, uint8_t peer_lq, int8_t sensitivity_dbm,
                 uint8_t ceiling)
    {
        if (ceiling >= num) ceiling = num - 1;
        if (idx > ceiling) idx = ceiling;

        if (!connected || peer_lq < DYNPOWER_LQ_LOW) {
            go_up(tnow_ms, ceiling);
            return idx;
        }

        // one missed frame says nothing about the margin, the LQ counts it; on the bench this is a few % of frames
        if (peer_rssi_dbm >= 0) return idx;

        int16_t margin_db = (int16_t)peer_rssi_dbm - sensitivity_dbm;

        if (margin_db < DYNPOWER_MARGIN_LOW_DB) {
            go_up(tnow_ms, ceiling);
            return idx;
        }

        // the average in 1/16 dB
        if (!avg_valid) {
            avg_x16 = margin_db * 16;
            avg_valid = true;
        } else {
            avg_x16 += (margin_db * 16 - avg_x16) / (1 << DYNPOWER_AVG_SHIFT);
        }
        int16_t avg_db = avg_x16 / 16;

        if (avg_db < DYNPOWER_MARGIN_UP_DB || peer_lq < DYNPOWER_LQ_UP) {
            strong = false;
            uint32_t wait_ms = (avg_db < DYNPOWER_MARGIN_UP_DB) ? DYNPOWER_UP_MS : DYNPOWER_UP_LQ_MS;
            if (idx < ceiling && (uint32_t)(tnow_ms - t_last_up_ms) >= wait_ms) go_up(tnow_ms, idx + 1);
            return idx;
        }

        if ((uint32_t)(tnow_ms - t_last_up_ms) >= DYNPOWER_CALM_MS) down_hold_ms = DYNPOWER_DOWN_HOLD_MS;

        bool can_go_down = idx > 0 &&
                           avg_db - (table_dbm[idx] - table_dbm[idx - 1]) >= DYNPOWER_MARGIN_UP_DB + DYNPOWER_MARGIN_KEEP_DB;

        if (avg_db > DYNPOWER_MARGIN_DOWN_DB && peer_lq >= DYNPOWER_LQ_DOWN && can_go_down) {
            if (!strong) {
                strong = true;
                t_strong_ms = tnow_ms;
            } else if ((uint32_t)(tnow_ms - t_strong_ms) >= down_hold_ms) {
                idx--;
                t_strong_ms = tnow_ms; // the next step down waits for the other end to report the lower power
                t_last_down_ms = tnow_ms;
                went_down = true;
            }
        } else {
            strong = false;
        }

        return idx;
    }

  private:
    const int8_t* table_dbm;
    uint8_t num;
    uint8_t idx;
    uint32_t t_last_up_ms;
    uint32_t t_last_down_ms;
    uint32_t t_strong_ms;
    uint32_t down_hold_ms;
    bool strong;
    bool went_down;
    bool avg_valid;
    int16_t avg_x16;

    void go_up(uint32_t tnow_ms, uint8_t new_idx)
    {
        strong = false;
        if (new_idx <= idx) return;
        if (went_down && (uint32_t)(tnow_ms - t_last_down_ms) < DYNPOWER_UNDONE_MS) {
            down_hold_ms = (down_hold_ms * 2 > DYNPOWER_DOWN_HOLD_MAX_MS) ? DYNPOWER_DOWN_HOLD_MAX_MS : down_hold_ms * 2;
        }
        idx = new_idx;
        t_last_up_ms = tnow_ms;
    }
};


#endif // DYNAMIC_POWER_H
