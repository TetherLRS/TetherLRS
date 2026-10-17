// Host test of tRegulatory (mLRS/Common/regulatory.h, TetherLRS EU region). Run with test/run_tests.sh.
#include <cstdio>
#include <cstdint>
#include <vector>
#include "setup_types.h"

// what regulatory.h takes from the firmware
#define USE_REGION_EU // as on LR11xx, SX126x and SX128x devices
#define ANTENNA_1 0
#define ANTENNA_2 1
#define SX12XX_FREQ_MHZ_TO_REG(f) ((uint32_t)((f) * 1000.0 + 0.5)) // kHz, enough to order frequencies

static uint32_t now_us = 0;
uint32_t millis32(void) { return now_us / 1000; }
uint16_t micros16(void) { now_us += 5; return (uint16_t)now_us; } // each call takes 5 us

struct { uint8_t Region; struct { uint8_t FrequencyBand; bool is_lora; } Sx; } Config;

struct tSxStub {
    uint32_t toa_us = 13200;
    int8_t power = 14;
    int8_t rssi = -120;
    int rx_calls = 0, idle_calls = 0, rssi_reads = 0;
    uint32_t TimeOverAir_us(void) { return toa_us; }
    int8_t RfPower_dbm(void) { return power; }
    int8_t GetRssiInst_dbm(void) { rssi_reads++; return rssi; }
    void SetToRx(void) { rx_calls++; }
    void SetToIdle(void) { idle_calls++; }
};
tSxStub sx, sx2;

#include "regulatory.h"

static int fails = 0;
static void check(const char* name, bool ok)
{
    printf("%-72s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

static const uint32_t F1 = SX12XX_FREQ_MHZ_TO_REG(864.000);
static const uint32_t F2 = SX12XX_FREQ_MHZ_TO_REG(866.500);
static const uint32_t FLR = SX12XX_FREQ_MHZ_TO_REG(869.525);

static void eu868(bool lora = true)
{
    Config.Region = REGION_EU;
    Config.Sx.FrequencyBand = SX_FHSS_FREQUENCY_BAND_868_MHZ;
    Config.Sx.is_lora = lora;
    sx = tSxStub();
    now_us = 0;
}

// tries to send on f every period_ms for duration_ms, returns the start times (ms) of what went out
static std::vector<uint32_t> run(tRegulatory& r, uint32_t f, uint32_t period_ms, uint32_t duration_ms)
{
    std::vector<uint32_t> sent;
    uint32_t t0 = millis32();
    for (uint32_t t = 0; t < duration_ms; t += period_ms) {
        now_us = (t0 + t) * 1000;
        if (r.TxGo(ANTENNA_1, f)) sent.push_back(t0 + t);
    }
    return sent;
}

// largest on-time in any window of window_ms, in us
static uint64_t max_window_us(const std::vector<uint32_t>& sent, uint32_t toa_us, uint32_t window_ms)
{
    uint64_t best = 0;
    size_t j = 0;
    for (size_t i = 0; i < sent.size(); i++) {
        while (sent[i] - sent[j] >= window_ms) j++;
        uint64_t on = (uint64_t)(i - j + 1) * toa_us;
        if (on > best) best = on;
    }
    return best;
}

int main()
{
    {   // free region: nothing checked, no listening
        tRegulatory r; eu868(); Config.Region = REGION_FREE; r.Init();
        bool all = true;
        for (int i = 0; i < 100; i++) all &= r.TxGo(ANTENNA_1, F1);
        check("free: every frame goes, no CCA", all && sx.rx_calls == 0 && !r.CcaNeeded(ANTENNA_1, F1));
    }
    {   // starts empty, so a restart cannot add on-time
        tRegulatory r; eu868(); r.Init();
        bool first = r.TxGo(ANTENNA_1, F1);
        now_us = 484 * 1000; // 484 ms * 27.2 us/ms = 13.18 ms < 13.2 ms
        bool early = r.TxGo(ANTENNA_1, F1);
        now_us = 486 * 1000;
        bool later = r.TxGo(ANTENNA_1, F1);
        check("PSA: nothing at start, a 13.2 ms frame after 485 ms of credit", !first && !early && later);
    }
    {   // 100 ms off on the same frequency after each transmission
        tRegulatory r; eu868(); r.Init();
        r.TxGo(ANTENNA_1, F1); // first use: its bucket starts empty here
        now_us = 200000000u; // 200 s later: full
        bool a = r.TxGo(ANTENNA_1, F1);
        now_us += 114 * 1000;
        bool b = r.TxGo(ANTENNA_1, F1); // 13.2 ms frame + 100 ms off = 113.2, rounded up and one more
        now_us += 1 * 1000;
        bool c = r.TxGo(ANTENNA_1, F1);
        check("PSA: same frequency blocked until frame + 100 ms (+ rounding)", a && !b && c);
    }
    {   // another frequency is not affected by the first
        tRegulatory r; eu868(); r.Init();
        r.TxGo(ANTENNA_1, F1); r.TxGo(ANTENNA_1, F2); // first use: the buckets start empty here
        now_us = 200000000u;
        bool a = r.TxGo(ANTENNA_1, F1);
        now_us += 20 * 1000;
        bool b = r.TxGo(ANTENNA_1, F2);
        check("PSA: each frequency has its own off-time and bucket", a && b);
    }
    {   // the hourly cap holds in every window, with a sender that tries as often as it can
        tRegulatory r; eu868(false); r.Init();
        sx.toa_us = 7600; // FSK frame
        std::vector<uint32_t> sent = run(r, F1, 20, 3 * 3600 * 1000u);
        uint64_t worst = max_window_us(sent, 7600, 3600 * 1000u);
        printf("    FSK on one channel every 20 ms: worst hour %.1f s\n", worst / 1e6);
        check("PSA: at most 100 s in any hour on one channel", worst <= 100000000ull && worst > 95000000ull);
    }
    {   // the EU LoRa plan as designed: 9 channels at 55 ms, nothing is ever blocked after the start
        tRegulatory r; eu868(); r.Init();
        uint32_t f[9]; for (int k = 0; k < 9; k++) f[k] = SX12XX_FREQ_MHZ_TO_REG(863.375 + 0.6 * k);
        uint32_t blocked_late = 0, sent = 0;
        for (uint32_t n = 0; n < 2 * 3600 * 1000u / 55; n++) {
            now_us = n * 55 * 1000;
            bool ok = r.TxGo(ANTENNA_1, f[n % 9]);
            if (ok) sent++;
            if (!ok && now_us > 2000000) blocked_late++; // the first 2 s fill the buckets
        }
        check("LoRa 18 Hz plan: 9 channels, nothing blocked once running", blocked_late == 0 && sent > 130000);
    }
    {   // the FSK plan: 21 channels at 20 ms
        tRegulatory r; eu868(false); r.Init();
        sx.toa_us = 7600;
        uint32_t blocked_late = 0;
        for (uint32_t n = 0; n < 2 * 3600 * 1000u / 20; n++) {
            now_us = n * 20 * 1000;
            bool ok = r.TxGo(ANTENNA_1, SX12XX_FREQ_MHZ_TO_REG(863.25 + 0.25 * (n % 21)));
            if (!ok && now_us > 2000000) blocked_late++;
        }
        check("FSK 50 Hz plan: 21 channels, nothing blocked once running", blocked_late == 0);
    }
    {   // LR on 869.525: 10 % duty, no listening
        tRegulatory r; eu868(); r.Init();
        sx.toa_us = 52800;
        std::vector<uint32_t> sent = run(r, FLR, 60, 3 * 3600 * 1000u);
        uint64_t worst = max_window_us(sent, 52800, 3600 * 1000u);
        printf("    LR every 60 ms: worst hour %.1f s\n", worst / 1e6);
        check("LR: at most 360 s in any hour, no CCA", worst <= 360000000ull && worst > 340000000ull && sx.rx_calls == 0);
    }
    {   // LR as designed, every 560 ms: never blocked once running
        tRegulatory r; eu868(); r.Init();
        sx.toa_us = 52800;
        std::vector<uint32_t> sent = run(r, FLR, 560, 3600 * 1000u);
        check("LR mode at 560 ms: all frames but the first go", sent.size() >= 3600 * 1000u / 560 - 1);
    }
    {   // listening: threshold for LoRa 500 kHz below 100 mW is -75 dBm at the antenna
        tRegulatory r; eu868(); r.Init();
        r.TxGo(ANTENNA_1, F1); // first use: its bucket starts empty here
        now_us = 200000000u;
        sx.rssi = -76;
        bool clear = r.TxGo(ANTENNA_1, F1);
        now_us += 200 * 1000;
        sx.rssi = -75;
        bool busy = !r.TxGo(ANTENNA_1, F1);
        check("PSA LoRa: -76 dBm clear, -75 dBm busy", clear && busy && r.cca_busy_cnt == 1);
        check("CCA listens in RX for >= 160 us and leaves the radio idle", sx.rssi_reads >= 160 / 5 / 2 && sx.idle_calls == sx.rx_calls);
    }
    {   // a busy channel books no time, the next try on it can go
        tRegulatory r; eu868(); r.Init();
        r.TxGo(ANTENNA_1, F1); // first use: its bucket starts empty here
        now_us = 200000000u;
        sx.rssi = -50;
        bool a = r.TxGo(ANTENNA_1, F1);
        sx.rssi = -120;
        now_us += 1000;
        bool b = r.TxGo(ANTENNA_1, F1);
        check("PSA: a frame not sent for a busy channel starts no off-time", !a && b);
    }
    {   // threshold moves up by 4 dB from 100 mW e.r.p., and by the LNA gain
        tRegulatory r; eu868(false); r.Init();
        r.TxGo(ANTENNA_1, F1); // first use: its bucket starts empty here
        now_us = 200000000u;
        sx.power = 20; // FSK, OCW 250 kHz: 24 - 117 + 11 = -82
        sx.rssi = -83;
        bool clear = r.TxGo(ANTENNA_1, F1);
        now_us += 200 * 1000;
        sx.rssi = -82;
        bool busy = !r.TxGo(ANTENNA_1, F1);
        check("PSA FSK at 100 mW: -83 dBm clear, -82 dBm busy", clear && busy);
    }
    {   // 2.4 GHz: listen only above 10 dBm, ExpressLRS thresholds, no time limits
        tRegulatory r; eu868(); Config.Sx.FrequencyBand = SX_FHSS_FREQUENCY_BAND_2P4_GHZ; r.Init();
        sx.power = 10;
        bool low = r.TxGo(ANTENNA_1, 2440000) && r.TxGo(ANTENNA_1, 2440000) && sx.rx_calls == 0;
        sx.power = 20; sx.rssi = -72;
        bool clear = r.TxGo(ANTENNA_1, 2440000);
        sx.rssi = -71;
        bool busy = !r.TxGo(ANTENNA_1, 2440000);
        check("2.4 GHz: no CCA at 10 dBm, back to back ok, -71 dBm busy at 100 mW", low && clear && busy);
    }
    {   // more frequencies than slots: refuses rather than forgetting one
        tRegulatory r; eu868(); r.Init();
        for (int k = 0; k < REGULATORY_FREQ_NUM; k++) r.TxGo(ANTENNA_1, F1 + 1000 * k); // takes all slots
        now_us += 200000000u;
        int ok = 0;
        for (int k = 0; k < REGULATORY_FREQ_NUM; k++) ok += r.TxGo(ANTENNA_1, F1 + 1000 * k) ? 1 : 0;
        uint32_t blocked = r.blocked_cnt;
        bool extra = r.TxGo(ANTENNA_1, F1 + 1000 * REGULATORY_FREQ_NUM);
        check("PSA: 24 frequencies go, the 25th is refused and counted", ok == REGULATORY_FREQ_NUM && !extra && r.blocked_cnt == blocked + 1);
    }
    printf("%s\n", fails ? "FAILED" : "all passed");
    return fails ? 1 : 0;
}
