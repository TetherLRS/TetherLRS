//*******************************************************
// Copyright (c) TetherLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.html
//*******************************************************
// Host test of tCrypto (src/common/crypto.cpp): a key per direction, a key per receiver slot, the session randoms
// exchange, the link key without an air bind. Run with test/run_tests.sh.
#include <cstdio>
#include <cstring>
#include "crypto.h"
#include "crypto.cpp"
extern "C" {
#include "thirdparty/monocypher/src/monocypher.c"
}

static int fails = 0;
static void check(const char* name, bool ok)
{
    printf("%-76s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

static char phrase[7] = "tlrs.0";
static uint8_t tx_uid[12] = {1,2,3,4,5,6,7,8,9,10,11,12};
static uint8_t no_key[32] = {};
static uint8_t key[32] = {9,8,7,6,5,4,3,2,1,0,9,8,7,6,5,4,3,2,1,0,9,8,7,6,5,4,3,2,1,0,9,8};

// a frame: 5 status bytes the MAC covers, then the payload the way frames.h hands it over
struct tFrame { uint8_t status[5]; uint8_t payload[100]; uint8_t len; };

static void send(tCrypto& from, tFrame& f, const char* text)
{
    memset(f.status, 0x5A, 5);
    f.len = strlen(text);
    memcpy(f.payload, text, f.len);
    from.Encrypt(f.status, 5, f.payload, f.len);
    f.len += from.NonceLen();
}

static bool receive(tCrypto& to, uint8_t slot, tFrame f, const char* text)
{
    if (!to.Decrypt(slot, f.status, 5, f.payload, f.len)) return false;
    f.len -= to.NonceLen();
    return f.len == strlen(text) && memcmp(f.payload, text, f.len) == 0;
}

int main(void)
{
    tCrypto tx, rx, rx2;
    uint8_t buf[CRYPTO_STARTUP_RANDOM_BUF_LEN];
    tFrame f;

    for (uint8_t level = 1; level <= 3; level++) {
        printf("privacy level %u\n", level);
        tx.Init(tCrypto::TX, phrase, tx_uid, 0x1122334455667788ULL, level, no_key);
        rx.Init(tCrypto::RX, phrase, tx_uid, 0x1122334455667788ULL, level, no_key);
        rx2.Init(tCrypto::RX, phrase, tx_uid, 0x1122334455667788ULL, level, no_key);

        check("the Tx cannot connect before it holds the slot's key", !tx.ValidKeys(0));
        tx.SetTransmitKey(0xAAAA000011112222ULL);
        tx.SetReceiveKey(0, 0xBBBB000011112222ULL);
        check("  and can once it does", tx.ValidKeys(0) && !tx.ValidKeys(1));

        send(tx, f, "down before startup");
        check("before the STARTUP the receiver cannot read the Tx", !receive(rx, 0, f, "down before startup") || level == 1);

        tx.EncryptSessionRandoms(buf, 0, 0x0102030405060708ULL, 0x1112131415161718ULL);
        uint8_t bad[CRYPTO_STARTUP_RANDOM_BUF_LEN];
        memcpy(bad, buf, sizeof(bad));
        bad[3] ^= 0x01;
        rx.SetSessionKeysFromEncryptedBuf(bad);
        check("a tampered STARTUP is refused", rx.SessionRandom() == 0);
        rx.SetSessionKeysFromEncryptedBuf(buf);
        check("the STARTUP gives the receiver the group random", rx.SessionRandom() == 0xAAAA000011112222ULL);

        send(tx, f, "down");
        check("the receiver reads the Tx with the group key", receive(rx, 0, f, "down"));
        send(rx, f, "up from slot 0");
        check("the Tx reads slot 0 with its key", receive(tx, 0, f, "up from slot 0"));
        check("  and not as another slot", !receive(tx, 1, f, "up from slot 0") || level == 1);

        // a second receiver on slot 1
        tx.SetReceiveKey(1, 0xCCCC000011112222ULL);
        tx.EncryptSessionRandoms(buf, 1, 0x0102030405060708ULL, 0x1112131415161718ULL);
        rx2.SetSessionKeysFromEncryptedBuf(buf);
        send(rx2, f, "up from slot 1");
        check("slot 1 has its own key", receive(tx, 1, f, "up from slot 1") && (!receive(tx, 0, f, "up from slot 1") || level == 1));
        send(tx, f, "down to all");
        check("both receivers read the same Tx frame", receive(rx, 0, f, "down to all") && receive(rx2, 0, f, "down to all"));

        // the receiver of slot 0 restarts: the Tx reconnects it with a fresh slot random
        rx.Init(tCrypto::RX, phrase, tx_uid, 0x1122334455667788ULL, level, no_key);
        tx.SetReceiveKey(0, 0xDDDD000011112222ULL);
        tx.EncryptSessionRandoms(buf, 0, 0x0102030405060708ULL, 0x1112131415161718ULL);
        rx.SetSessionKeysFromEncryptedBuf(buf);
        send(rx, f, "up after restart");
        check("a restarted receiver comes back under a new key", receive(tx, 0, f, "up after restart"));
        tFrame same = f;
        rx.SetSessionKeysFromEncryptedBuf(buf); // the Tx repeats the command until answered
        send(rx, f, "up after restart");
        check("  the repeated STARTUP changes nothing", receive(tx, 0, f, "up after restart") && memcmp(same.payload, f.payload, f.len) != 0);

        // the Tx restarts: a new group random, the receiver takes it
        tx.Init(tCrypto::TX, phrase, tx_uid, 0x1122334455667788ULL, level, no_key);
        tx.SetTransmitKey(0xEEEE000011112222ULL);
        tx.SetReceiveKey(0, 0xFFF0000011112222ULL);
        tx.EncryptSessionRandoms(buf, 0, 0x0102030405060709ULL, 0x1112131415161718ULL);
        rx.SetSessionKeysFromEncryptedBuf(buf);
        send(tx, f, "down after Tx restart");
        check("a restarted Tx re-keys the receiver", receive(rx, 0, f, "down after Tx restart") && rx.SessionRandom() == 0xEEEE000011112222ULL);

        // a resend: the stats change, the ciphertext stays, the MAC follows the header
        send(tx, f, "resent");
        f.status[0] = 0x11;
        tFrame stale = f;
        tx.RecalculateMac(f.status, 5, f.payload, f.len);
        check("a resend with a new header passes after the MAC is redone", receive(rx, 0, f, "resent"));
        check("  and fails without it", !receive(rx, 0, stale, "resent") || level == 1);
    }

    // the link key: no air bind needed, uid and bind random differ between the two
    uint8_t other_uid[12];
    memset(other_uid, 0xFF, 12);
    tx.Init(tCrypto::TX, phrase, tx_uid, 0x1122334455667788ULL, 2, key);
    rx.Init(tCrypto::RX, phrase, other_uid, UINT64_MAX, 2, key);
    check("with a link key the Rx is valid without an air bind", rx.ValidKeys());
    tx.SetTransmitKey(0x1111000011112222ULL);
    tx.SetReceiveKey(0, 0x2222000011112222ULL);
    tx.EncryptSessionRandoms(buf, 0, 0x0102030405060708ULL, UINT64_MAX);
    rx.SetSessionKeysFromEncryptedBuf(buf);
    send(tx, f, "fleet");
    check("  and reads the Tx it never bound with", receive(rx, 0, f, "fleet"));
    rx.Init(tCrypto::RX, phrase, other_uid, UINT64_MAX, 2, no_key);
    check("without a link key the air bind's random is needed", !rx.ValidKeys());

    // privacy off
    tx.Init(tCrypto::TX, phrase, tx_uid, UINT64_MAX, 0, no_key);
    rx.Init(tCrypto::RX, phrase, tx_uid, UINT64_MAX, 0, no_key);
    check("privacy off: valid, no nonce, plain text", tx.ValidKeys() && tx.NonceLen() == 0);
    send(tx, f, "plain");
    check("  and the frame goes as it is", memcmp(f.payload, "plain", 5) == 0 && receive(rx, 0, f, "plain"));

    printf("%s\n", fails ? "FAILED" : "all ok");
    return fails ? 1 : 0;
}
