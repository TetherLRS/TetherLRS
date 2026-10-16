//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// Regulatory
//********************************************************
// TetherLRS: the rules of the EU region, checked before each transmission, chosen at runtime (Setup.Common.Region)
// - 863-869.2 MHz, ETSI EN 300 220-2 V3.3.1, polite spectrum access (4.6):
//   listen >= 160 us below the CCA threshold, >= 100 ms off on the same frequency, <= 100 s per hour per channel
// - 869.4-869.65 MHz (band O, LR mode): <= 10 % duty cycle per hour, no listening needed
// - 2.4 GHz, ETSI EN 300 328: listen before talk above 10 mW, with ExpressLRS's thresholds (EU_CE_2400)
// The on-time limits are token buckets which start empty, so restarting cannot add time.
//********************************************************
#ifndef REGULATORY_H
#define REGULATORY_H
#pragma once


#include <inttypes.h>


#define REGULATORY_CCA_RSSI_VALID_US    50  // RSSI settles after entering RX
#define REGULATORY_CCA_WINDOW_US        200 // EN 300 220-2 asks for >= 160 us
#define REGULATORY_CCA_TIME_US          (REGULATORY_CCA_RSSI_VALID_US + REGULATORY_CCA_WINDOW_US + 50)
#define REGULATORY_TOFF_MS              100
#define REGULATORY_FREQ_NUM             24 // the 21 FSK channels plus the bind channel

#ifndef SX_LNA_GAIN_DB
#define SX_LNA_GAIN_DB                  0 // with an external LNA the chip reads the antenna level plus this
#endif


typedef enum {
    REGULATORY_RULE_NONE = 0,
    REGULATORY_RULE_PSA,     // polite spectrum access
    REGULATORY_RULE_DUTY10,  // 10 % duty cycle
    REGULATORY_RULE_LBT,     // listen before talk only
} REGULATORY_RULE_ENUM;


class tRegulatory
{
  public:
    void Init(void)
    {
        for (uint8_t i = 0; i < REGULATORY_FREQ_NUM; i++) freq[i] = 0;
        cca_busy_cnt = 0;
        blocked_cnt = 0;
    }

    bool CcaNeeded(uint8_t antenna, uint32_t freq_reg)
    {
        uint8_t r = rule(freq_reg);
        if (r == REGULATORY_RULE_PSA) return true;
        if (r == REGULATORY_RULE_LBT) return (power_dbm(antenna) > 10); // up to 10 dBm EN 300 328 needs no adaptivity
        return false;
    }

    // radio idle on freq_reg; may listen for REGULATORY_CCA_TIME_US, leaves the radio idle
    // true: send now, its time is booked
    bool TxGo(uint8_t antenna, uint32_t freq_reg)
    {
        uint8_t r = rule(freq_reg);
        if (r == REGULATORY_RULE_NONE) return true;

        uint32_t toa_us = (antenna == ANTENNA_1) ? sx.TimeOverAir_us() : sx2.TimeOverAir_us();
        uint32_t tnow_ms = millis32();

        int8_t slot = -1;
        if (r == REGULATORY_RULE_PSA || r == REGULATORY_RULE_DUTY10) {
            slot = find_slot(freq_reg, tnow_ms);
            if (slot < 0 || !time_ok(slot, r, toa_us, tnow_ms)) { blocked_cnt++; return false; }
        }

        if (CcaNeeded(antenna, freq_reg) && !cca_clear(antenna, cca_threshold_dbm(r, power_dbm(antenna)))) {
            cca_busy_cnt++;
            return false;
        }

        if (slot >= 0) {
            credit_us[slot] -= toa_us;
            tlast_tx_ms[slot] = tnow_ms;
            tx_seen[slot] = true;
        }
        return true;
    }

    uint32_t cca_busy_cnt; // frames not sent, channel busy
    uint32_t blocked_cnt; // frames not sent, on-time or off-time limit

  private:
    uint8_t rule(uint32_t freq_reg)
    {
        if (Config.Region != REGION_EU) return REGULATORY_RULE_NONE;
        if (Config.Sx.FrequencyBand == SX_FHSS_FREQUENCY_BAND_2P4_GHZ) return REGULATORY_RULE_LBT;
#ifdef SX12XX_FREQ_MHZ_TO_REG
        if (freq_reg >= SX12XX_FREQ_MHZ_TO_REG(869.4) && freq_reg <= SX12XX_FREQ_MHZ_TO_REG(869.65)) return REGULATORY_RULE_DUTY10;
#endif
        return REGULATORY_RULE_PSA;
    }

    int8_t power_dbm(uint8_t antenna)
    {
        return (antenna == ANTENNA_1) ? sx.RfPower_dbm() : sx2.RfPower_dbm();
    }

    // at the chip, i.e. including the LNA gain
    int8_t cca_threshold_dbm(uint8_t r, int8_t pwr_dbm)
    {
        int16_t thr;
        if (r == REGULATORY_RULE_LBT) { // ExpressLRS EU_CE_2400 for 25/50/100 mW
            bool lora = Config.Sx.is_lora;
            if (pwr_dbm <= 14) {
                thr = (lora) ? -65 : -67;
            } else if (pwr_dbm <= 17) {
                thr = (lora) ? -68 : -70;
            } else {
                thr = (lora) ? -71 : -73;
            }
        } else { // EN 300 220-2 tables 11 and 17: 10 log(OCW in kHz) - 117 dBm, +15 dB below 100 mW e.r.p., +11 dB up to 500 mW
            thr = ((Config.Sx.is_lora) ? 27 : 24) - 117 + ((pwr_dbm < 20) ? 15 : 11); // OCW 500 kHz LoRa, 250 kHz FSK
        }
        return thr + SX_LNA_GAIN_DB;
    }

    bool cca_clear(uint8_t antenna, int8_t threshold_dbm)
    {
#ifndef USE_REGION_EU
        return true; // the region stays free on chips without an instantaneous RSSI read
#else
        if (antenna == ANTENNA_1) { sx.SetToRx(); } else { sx2.SetToRx(); }

        uint16_t t0_us = micros16();
        while ((uint16_t)(micros16() - t0_us) < REGULATORY_CCA_RSSI_VALID_US) {}

        bool clear = true;
        t0_us = micros16();
        do {
            int8_t rssi = (antenna == ANTENNA_1) ? sx.GetRssiInst_dbm() : sx2.GetRssiInst_dbm();
            if (rssi >= threshold_dbm) { clear = false; break; }
        } while ((uint16_t)(micros16() - t0_us) < REGULATORY_CCA_WINDOW_US);

        if (antenna == ANTENNA_1) { sx.SetToIdle(); } else { sx2.SetToIdle(); }
        return clear;
#endif
    }

    int8_t find_slot(uint32_t freq_reg, uint32_t tnow_ms)
    {
        for (uint8_t i = 0; i < REGULATORY_FREQ_NUM; i++) {
            if (freq[i] == freq_reg) return i;
        }
        for (uint8_t i = 0; i < REGULATORY_FREQ_NUM; i++) {
            if (freq[i] != 0) continue;
            freq[i] = freq_reg;
            credit_us[i] = 0;
            tupdate_ms[i] = tnow_ms;
            tx_seen[i] = false;
            return i;
        }
        return -1; // more frequencies than any plan has, so better not send
    }

    // token bucket: in any hour at most capacity + rate * 1 h, i.e. 2 + 98 = 100 s (PSA) or 10 + 350 = 360 s (10 %)
    bool time_ok(uint8_t i, uint8_t r, uint32_t toa_us, uint32_t tnow_ms)
    {
        uint32_t dt_ms = tnow_ms - tupdate_ms[i];
        if (dt_ms > 600000) dt_ms = 600000; // fills any bucket, and no overflow below
        tupdate_ms[i] = tnow_ms;

        int32_t capacity_us;
        if (r == REGULATORY_RULE_DUTY10) {
            credit_us[i] += (dt_ms * 875) / 9; // 97.2 us per ms
            capacity_us = 10000000;
        } else {
            credit_us[i] += (dt_ms * 245) / 9; // 27.2 us per ms
            capacity_us = 2000000;
        }
        if (credit_us[i] > capacity_us) credit_us[i] = capacity_us;

        if (r == REGULATORY_RULE_PSA && tx_seen[i]) {
            uint32_t toff_ms = (toa_us + 999) / 1000 + REGULATORY_TOFF_MS + 1;
            if ((tnow_ms - tlast_tx_ms[i]) < toff_ms) return false;
        }

        return (credit_us[i] >= (int32_t)toa_us);
    }

    uint32_t freq[REGULATORY_FREQ_NUM]; // 0 = slot free
    int32_t credit_us[REGULATORY_FREQ_NUM];
    uint32_t tupdate_ms[REGULATORY_FREQ_NUM];
    uint32_t tlast_tx_ms[REGULATORY_FREQ_NUM];
    bool tx_seen[REGULATORY_FREQ_NUM];
};


#endif // REGULATORY_H
