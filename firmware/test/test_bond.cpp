//*******************************************************
// Copyright (c) TetherLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.html
//*******************************************************
// Host test of the bonded lanes: the lockstep fetch of tTransmitArq (src/common/arq.h) and the order the receiving
// side puts the two lanes out in (src/common/bond.h). Run with test/run_tests.sh.
#include <cstdio>
#include <cstring>
#include <string>
#include "setup_types.h"
#define DECc(x, n) x = ((x) + 1) % (n)
#include "arq.h"
#include "bond.h"

static int fails = 0;
static void check(const char* name, bool ok)
{
    printf("%-76s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

static std::string out;
static void emit(uint8_t* const data, uint8_t len) { out.append((const char*)data, len); }

static void accepted(tBondReorder& r, uint8_t lane, uint8_t seq, const char* text)
{
    r.Accepted(lane, seq, (uint8_t*)text, strlen(text));
}

int main(void)
{
    tBondReorder r;
    r.Init(emit, 100);

    out.clear();
    accepted(r, BOND_LANE_A, 1, "A1");
    accepted(r, BOND_LANE_B, 1, "B1");
    accepted(r, BOND_LANE_A, 2, "A2");
    accepted(r, BOND_LANE_B, 2, "B2");
    check("both lanes in: A then B each cycle", out == "A1B1A2B2");

    out.clear();
    accepted(r, BOND_LANE_B, 3, "B3");
    check("B before its A waits", out == "");
    accepted(r, BOND_LANE_A, 3, "A3");
    check("  and goes out after it", out == "A3B3");

    out.clear();
    accepted(r, BOND_LANE_A, 4, "A4");
    check("A alone goes out at once", out == "A4");
    accepted(r, BOND_LANE_B, 4, "B4");
    check("  its B follows", out == "A4B4");

    out.clear();
    accepted(r, BOND_LANE_B, 5, "B5"); // A5 never comes: the retries ran out
    accepted(r, BOND_LANE_A, 6, "A6");
    check("a B whose A is lost goes out before the next A", out == "B5A6");
    accepted(r, BOND_LANE_B, 6, "B6");
    check("  and the next B follows its A", out == "B5A6B6");

    out.clear();
    accepted(r, BOND_LANE_B, 7, "B7");
    accepted(r, BOND_LANE_B, 0, "B8"); // seq nos are 3 bits
    check("two Bs waiting keep their order", out == "B7");
    accepted(r, BOND_LANE_A, 0, "A8");
    check("  the second after its A", out == "B7A8B8");

    // the lockstep fetch: new data for both lanes only when both are free
    tTransmitArq a, b;
    a.Init(); b.Init();
    a.SetRetryCnt(2); b.SetRetryCnt(2);
    bool fresh = a.WouldAdvance() && b.WouldAdvance();
    a.Settle(fresh); b.Settle(fresh);
    check("idle lanes take new data", fresh && a.SeqNo() == 1 && b.SeqNo() == 1);
    a.AckReceived(1); b.FrameMissed();
    fresh = a.WouldAdvance() && b.WouldAdvance();
    a.Settle(fresh); b.Settle(fresh);
    check("A acked, B missed: both send again", !fresh && a.SeqNo() == 1 && b.SeqNo() == 1);
    a.AckReceived(1); b.AckReceived(1);
    fresh = a.WouldAdvance() && b.WouldAdvance();
    a.Settle(fresh); b.Settle(fresh);
    check("both acked: both move on together", fresh && a.SeqNo() == 2 && b.SeqNo() == 2);
    a.AckReceived(0); b.FrameMissed();
    for (uint8_t i = 0; i < 2; i++) { fresh = a.WouldAdvance() && b.WouldAdvance(); a.Settle(fresh); b.Settle(fresh); }
    check("  B missing twice: still the same payload", !fresh && b.SeqNo() == 2);
    b.FrameMissed();
    fresh = a.WouldAdvance() && b.WouldAdvance();
    a.Settle(fresh); b.Settle(fresh);
    check("  a third miss: the retries ran out, both move on", fresh && a.SeqNo() == 3 && b.SeqNo() == 3);

    tTransmitArq c;
    c.Init(); c.SetRetryCnt(1);
    c.FrameMissed();
    bool w = c.WouldAdvance();
    check("WouldAdvance changes nothing", w == c.WouldAdvance() && c.SeqNo() == 0);
    check("GetFreshPayload is WouldAdvance and Settle", c.GetFreshPayload() == w && c.SeqNo() == (w ? 1 : 0));

    printf("%s\n", fails ? "FAILED" : "all ok");
    return fails ? 1 : 0;
}
