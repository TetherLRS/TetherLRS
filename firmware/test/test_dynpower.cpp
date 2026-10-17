// Host test of the dynamic RF power (mLRS/Common/dynamic_power.h, TetherLRS). Run with test/run_tests.sh.
#include <cstdio>
#include "dynamic_power.h"

static int fails = 0;
static void check(const char* name, bool ok)
{
    printf("%-76s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

// the Nano Pro's table, 10 to 500 mW, and the generic ESP8285 2400 one (min, 1 mW, 13 mW) with its 18 dB step
static const int8_t nanopro[] = { 10, 14, 17, 20, 24, 27 };
static const int8_t esp8285[] = { -18, 0, 12 };
static const int8_t SENS = -105;

// the other end hears us at path_db below our power, plus a fading of +-fade dB from frame to frame; 50 Hz frames
struct Link {
    tDynPower dp;
    const int8_t* tab;
    uint8_t num;
    uint32_t t = 0;
    uint8_t idx;
    int path_db;
    int fade = 0;
    uint32_t seed = 12345;
    uint8_t lq = 100;
    bool connected = true;
    int changes = 0;
    Link(int path, const int8_t* t_ = nanopro, uint8_t n = 6) : tab(t_), num(n), idx(n - 1), path_db(path) { dp.Init(tab, num); }
    int noise() {
        if (!fade) return 0;
        seed = seed * 1103515245u + 12345u;
        return (int)((seed >> 16) % (2 * fade + 1)) - fade;
    }
    int8_t rssi() { return (int8_t)(tab[idx] - path_db + noise()); }
    void run(uint32_t ms, uint8_t ceil = 255) {
        if (ceil == 255) ceil = num - 1;
        for (uint32_t n = 0; n < ms / 20; n++, t += 20) {
            uint8_t before = idx;
            idx = dp.Step(t, connected, rssi(), lq, SENS, ceil);
            if (idx != before) changes++;
        }
    }
    int margin() const { return tab[idx] - path_db - SENS; }
};

int main()
{
    const uint8_t CEIL = 5;
    {   // close: 65 dB of margin at full power: down one step per 2 s, to the lowest
        Link l(67);
        l.run(1900);
        check("strong link: no step down before 2 s", l.idx == CEIL);
        l.run(200);
        check("strong link: one step down after 2 s", l.idx == CEIL - 1);
        l.run(1900);
        check("strong link: the next step waits another 2 s", l.idx == CEIL - 1);
        l.run(20000);
        check("strong link: settles at the lowest power", l.idx == 0);
    }
    {   // the same with the 30 dB frame-to-frame swings measured on the bench
        Link l(67);
        l.fade = 15;
        l.run(30000);
        check("fading +-15 dB: still comes down to the lowest", l.idx == 0);
        // a 41 dB mean: single frames dip under 35 dB a third of the time, the average does not
        Link m(27 - (SENS + 41));
        m.fade = 15;
        m.run(10000);
        check("fading +-15 dB around 41 dB: the average still steps down", m.idx < CEIL);
    }
    {   // middle: 37 dB at full power; one step down leaves 34, inside the window: stays there, no hunting
        Link l(27 - (SENS + 37));
        l.run(30000);
        check("settles inside the 20..35 dB window", l.idx == CEIL - 1);
        int c = l.changes;
        l.run(60000);
        check("no hunting: no change in the next 60 s", l.changes == c);
    }
    {   // an 18 dB step from 0 to -18 dBm: from 36 dB it would land at 18, under the step-up threshold
        Link l(0 - (SENS + 36), esp8285, 3);
        l.run(60000);
        check("18 dB step not taken when it would leave < 25 dB", l.idx == 1 && l.changes == 1);
        Link m(0 - (SENS + 45), esp8285, 3);
        m.run(60000);
        check("18 dB step taken when it leaves 27 dB, and it stays", m.idx == 0 && m.changes == 2);
    }
    {   // far: below 20 dB at full power: stays at the ceiling
        Link l(27 - (SENS + 15));
        l.run(10000);
        check("weak link stays at the ceiling", l.idx == CEIL);
    }
    {   // a fade to 15 dB of margin: climbs once the average gets under 20 dB (about 15 frames), then one step per 250 ms
        Link l(67);
        l.run(30000);
        uint8_t low = l.idx;
        l.path_db += 33; // at the lowest power the margin was 48, now 15; one step up gives 19, two give 22
        uint32_t t0 = l.t;
        while (l.idx == low && l.t - t0 < 2000) l.run(20);
        uint32_t t_up = l.t - t0;
        check("margin 10..20 dB: a step up within 400 ms", l.idx == low + 1 && t_up <= 400);
        l.run(200);
        check("margin 10..20 dB: no second step within 250 ms", l.idx == low + 1);
        l.run(100);
        check("margin 10..20 dB: second step after 250 ms", l.idx == low + 2);
    }
    {   // a deep fade: one frame under 10 dB of margin jumps straight to the ceiling
        Link l(67);
        l.run(30000);
        l.path_db += 60;
        l.run(20);
        check("one frame under 10 dB: straight to the ceiling", l.idx == CEIL);
    }
    {   // LQ: below 70 % to the ceiling, 70..90 % one step up, below 95 % no step down
        Link l(67);
        l.run(30000);
        uint8_t low = l.idx;
        l.lq = 85;
        l.run(20);
        check("LQ 85 %: one step up", l.idx == low + 1);
        l.lq = 60;
        l.run(20);
        check("LQ 60 %: straight to the ceiling", l.idx == CEIL);
        l.lq = 94;
        l.run(5000);
        check("LQ 94 %: does not come down (needs 95 %)", l.idx == CEIL);
    }
    {   // the LQ is counted over about a second: a step up for LQ waits that long before the next one
        Link l(67);
        l.run(30000);
        uint8_t low = l.idx;
        l.lq = 85;
        l.run(20);
        bool one = l.idx == low + 1;
        l.run(900);
        bool still = l.idx == low + 1;
        l.run(200);
        check("LQ 85 %: one step, the next one only after 1 s", one && still && l.idx == low + 2);
    }
    {   // the bench case: at the lowest power fades take the LQ to 88 %, one step up it is 100 %
        Link l(0 - (SENS + 45), esp8285, 3); // 45 dB at 0 dBm: the 18 dB step down is allowed
        int changes = 0;
        for (int i = 0; i < 6000; i++, l.t += 20) { // 120 s
            l.lq = (l.idx == 0) ? 88 : 100;
            uint8_t before = l.idx;
            l.idx = l.dp.Step(l.t, true, l.rssi(), l.lq, SENS, 2);
            if (l.idx != before) changes++;
        }
        check("LQ 88 % only at the lowest power: few changes in 120 s (wait doubles)", changes <= 16); // 2 down, then down-up pairs 4, 8, 16, 32, 32 s apart; about 80 without
        printf("    (%d changes)\n", changes);
        // the wait is now 32 s; a calm minute brings it back to 2 s: a later fade lifts the power, and once it is
        // over the step down comes 2 s later, not 32
        l.lq = 100;
        l.run(70000, 2);
        bool settled = l.idx == 0;
        l.path_db += 12; // margin at the lowest power from 27 to 15 dB: one step up
        l.run(400, 2);
        bool up = l.idx == 1;
        l.path_db -= 12;
        uint32_t t0 = l.t;
        while (l.idx == 1 && l.t - t0 < 40000) l.run(20, 2);
        check("after a calm minute the next step down waits 2 s again", settled && up && l.t - t0 <= 2500);
    }
    {   // lost link and a report that the other end did not hear us
        Link l(67);
        l.run(30000);
        l.connected = false;
        l.run(20);
        check("disconnected: ceiling", l.idx == CEIL);
        Link m(67);
        m.run(30000);
        uint8_t low = m.idx;
        m.idx = m.dp.Step(m.t, true, 127, 98, SENS, CEIL);
        check("RSSI 127 on one frame (a missed frame): the power stays", m.idx == low);
        // every other frame missed, real margin 30 dB inside the window: 127 must not count as a strong signal
        Link w(27 - (SENS + 30));
        for (int i = 0; i < 1500; i++, w.t += 20) {
            int8_t r = (i & 1) ? 127 : w.rssi();
            w.idx = w.dp.Step(w.t, true, r, 96, SENS, CEIL);
        }
        check("half the frames missed: 127 not taken as a strong signal", w.idx == CEIL);
    }
    {   // the ceiling: never above it, and a lower one clamps at once (region cap or a new setting)
        Link l(67);
        l.run(1000, 3);
        check("starts at the ceiling it is given", l.idx == 3);
        l.lq = 60;
        l.run(20, 2);
        check("lower ceiling clamps", l.idx == 2);
        l.run(20, 4);
        check("weak link goes to the new ceiling", l.idx == 4);
        Link m(27 - (SENS + 37));
        m.run(30000);
        bool settled = m.idx == CEIL - 1;
        m.run(20, 2);
        check("lower ceiling in the window clamps too", settled && m.idx == 2);
        Link n(67);
        n.run(20, 9);
        check("a ceiling past the table end is held to the table", n.idx == CEIL);
    }
    {   // millis wrap: a step down is not skipped or doubled across 2^32
        Link l(67);
        l.t = 0xFFFFFFFFu - 1000;
        l.run(1900);
        bool none = l.idx == CEIL;
        l.run(200);
        check("2 s hold across the millis wrap", none && l.idx == CEIL - 1);
    }
    printf("%s\n", fails ? "FAILED" : "all ok");
    return fails ? 1 : 0;
}
