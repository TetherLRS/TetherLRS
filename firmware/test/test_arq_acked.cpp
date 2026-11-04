//*******************************************************
// Copyright (c) TetherLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.html
//*******************************************************
// Host test of tTransmitArq::Acked() (src/common/arq.h): an advance because the retries ran out is not an ack, so a
// receiver that sent the answer naming a new mode restarts only once the Tx really has it. Run with test/run_tests.sh.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "setup_types.h"
#include "arq.h"

static int fails = 0;
static void check(const char* name, bool ok)
{
    printf("%-76s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

int main(void)
{
    tTransmitArq arq;
    arq.Init();
    arq.SetRetryCnt(2);
    arq.GetFreshPayload(); // the answer goes out with seq s

    // nacked twice, then once more: the retries run out and the next payload goes, but nobody acked
    uint8_t nack = (arq.payload_seq_no + 1) & 0x01;
    bool acked = true, advance = false;
    for (uint8_t i = 0; i < 4 && !advance; i++) {
        arq.AckReceived(nack);
        acked = arq.Acked();
        advance = arq.GetFreshPayload();
    }
    check("retries run out: the ARQ moves on to the next payload", advance);
    check("  and it does not count as an ack", !acked);

    // a missed frame is not an ack either
    arq.FrameMissed();
    check("a missed frame is not an ack", !arq.Acked());

    // the Tx acks the payload in flight
    arq.AckReceived(arq.payload_seq_no & 0x01);
    check("the Tx acks the payload in flight: acked", arq.Acked());
    check("  and the next payload goes", arq.GetFreshPayload());

    printf("%s\n", fails ? "FAILED" : "all ok");
    return fails ? 1 : 0;
}
