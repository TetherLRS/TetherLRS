//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
// TetherLRS changes: Copyright (c) TetherLRS project
//*******************************************************
// CRYPTO
//*******************************************************


#include <stdlib.h>
#include <string.h>
#include "crypto.h"
#include "thirdparty/monocypher/src/monocypher.h"


#define NONCE_LEN       3
#define MAC_LEN         3
#define LVL3_NONCE_LEN  4
#define LVL3_MAC_LEN    8

#define STATIC_SOURCE_LEN  78


#if LVL3_NONCE_LEN + LVL3_MAC_LEN != CRYPTO_NONCE_MAX_LEN
#error CRYPTO_NONCE_MAX_LEN incompatible with nonce and MAC defines!
#endif


typedef struct {
    uint8_t nonce_len;
    uint8_t mac_len;
} crypto_level_t;


const crypto_level_t crypto_list[] = {
    { .nonce_len = 0,               .mac_len = 0            }, // nothing
    { .nonce_len = NONCE_LEN,       .mac_len = 0            }, // level 1: only encryption
    { .nonce_len = NONCE_LEN,       .mac_len = MAC_LEN      }, // level 2: encryption + authentication
    { .nonce_len = LVL3_NONCE_LEN,  .mac_len = LVL3_MAC_LEN }, // level 3: stronger encryption + authentication
};


#define PRIVACY_LEVEL_NUM  (sizeof(crypto_list)/sizeof(crypto_level_t))


//-------------------------------------------------------
// Crypto API
//-------------------------------------------------------

void tCrypto::Init(
    uint8_t role,
    char* const bind_phrase, uint8_t tx_uid[12], uint64_t tx_random,
    uint8_t privacy_level, const uint8_t* const link_key)
{
    _role = role;

    _privacy_level = 0;
    if (privacy_level < PRIVACY_LEVEL_NUM) _privacy_level = privacy_level;

    _nonce_len = crypto_list[_privacy_level].nonce_len;
    _mac_len = crypto_list[_privacy_level].mac_len;

    // static secrets

    bool link_key_set = false;
    for (uint8_t i = 0; i < 32; i++) { if (link_key[i]) link_key_set = true; }

    memset(_static_source, 0, sizeof(_static_source));
    memcpy(_static_source,                   "TLRS key",    8); //  8 bytes
    memcpy(_static_source + 8,               bind_phrase,   6); //  6 bytes
    if (!link_key_set) { // with a link key the air bind's material is not needed, so radios flashed alike pair up
        memcpy(_static_source + 8 + 6,       tx_uid,       12); // 12 bytes
        memcpy(_static_source + 8 + 6 + 12,  &tx_random,    8); //  8 bytes // sum 34 bytes
    }
    memcpy(_static_source + 46,              link_key,     32); // 32 bytes // sum 78 bytes, all zero when no key was set

    crypto_blake2b(_static_key, 32, _static_source, STATIC_SOURCE_LEN); // construct static key

    _static_valid = link_key_set || (tx_random != UINT64_MAX);

    _startup_nonce_u32 = 0;

    // session secrets: the static key stands in until the real ones are known

    _transmit_random = 0;
    _key_tx_set = false;
    memcpy(_key_tx, _static_key, 32);
    _nonce_u32 = 0;

    for (uint8_t slot = 0; slot < CRYPTO_SLOT_NUM; slot++) {
        _receive_random[slot] = 0;
        _key_rx_set[slot] = false;
        memcpy(_key_rx[slot], _static_key, 32);
        _nonce_u32_last_received[slot] = 0;
    }

    // statistics
    mac_errors = 0;
    replay_counts = 0;
}


// the Tx can connect a slot once it holds its own key and that slot's; the Rx gets both from the Tx's STARTUP
bool tCrypto::ValidKeys(uint8_t slot)
{
    if (!_privacy_level) return true;
    if (!_static_valid) return false;
    if (_role == TX) return _key_tx_set && (slot < CRYPTO_SLOT_NUM) && _key_rx_set[slot];
    return true;
}


//-- session keys

void tCrypto::_derive_key(uint8_t key[32], uint64_t random)
{
uint8_t key_source[96]; // 78 + 8 = 86

    memcpy(key_source,                     _static_source, STATIC_SOURCE_LEN);
    memcpy(key_source + STATIC_SOURCE_LEN, &random,        8);

    crypto_blake2b(key, 32, key_source, STATIC_SOURCE_LEN + 8);
}


void tCrypto::SetTransmitKey(uint64_t random)
{
    _transmit_random = random;
    _derive_key(_key_tx, random);
    _key_tx_set = (random != UINT64_MAX);
    _nonce_u32 = 0; // a new key, the counter may start over
}


void tCrypto::SetReceiveKey(uint8_t slot, uint64_t random)
{
    if (slot >= CRYPTO_SLOT_NUM) return;
    _receive_random[slot] = random;
    _derive_key(_key_rx[slot], random);
    _key_rx_set[slot] = (random != UINT64_MAX);
    _nonce_u32_last_received[slot] = 0;
}


// The session randoms go encrypted, in this format:
//  0 ..  7:  8 bytes the Tx's transmit random, the group's
//  8 .. 15:  8 bytes the receiver's own random
// 16 .. 27: 12 bytes startup-random | bind-random | startup-nonce
// 28 .. 35:  8 bytes mac

// only Tx: sent along with a FRAME_CMD_GET_RX_SETUPDATA_STARTUP frame, to one slot
void tCrypto::EncryptSessionRandoms(uint8_t* const buf36, uint8_t slot, uint64_t startup_random, uint64_t bind_random)
{
uint8_t nonce[12];
uint8_t poly1305_key[32];
uint8_t mac[16];
uint8_t randoms[16];

    if (slot >= CRYPTO_SLOT_NUM) slot = 0;

    bind_random += _startup_nonce_u32;
    _startup_nonce_u32++; // ready it for next use

    memset(nonce, 0, 12);
    memcpy(nonce, &startup_random, 8);
    memcpy(nonce + 8, &bind_random, 4);

    memcpy(randoms, &_transmit_random, 8);
    memcpy(randoms + 8, &_receive_random[slot], 8);

    crypto_chacha20_ietf(buf36, randoms, 16, _static_key, nonce, 1); // buf36[0] ... buf36[16 - 1]

    memcpy(buf36 + 16, nonce, 12); // buf36[16] ... buf36[28 - 1]

    crypto_chacha20_ietf(poly1305_key, NULL, 32, _static_key, nonce, 0);
    crypto_poly1305(mac, buf36, 28, poly1305_key); // mac over the randoms & nonce

    memcpy(buf36 + 28, mac, 8); // buf36[28] ... buf36[36 - 1]
}


// only Rx: called upon receive of a FRAME_CMD_GET_RX_SETUPDATA_STARTUP frame; the randoms are taken when they
// changed, so a restarted Tx re-keys the receiver and a reconnect brings the receiver a fresh key of its own
bool tCrypto::SetSessionKeysFromEncryptedBuf(uint8_t* const buf36)
{
uint8_t nonce[12];
uint8_t poly1305_key[32];
uint8_t mac[16];
uint8_t randoms[16];
uint64_t group_random, own_random;

    memset(nonce, 0, 12);
    memcpy(nonce, buf36 + 16, 12);

    crypto_chacha20_ietf(poly1305_key, NULL, 32, _static_key, nonce, 0);
    crypto_poly1305(mac, buf36, 28, poly1305_key); // mac over the randoms & nonce
    for (uint8_t i = 0; i < 8; i++) { if (buf36[28 + i] != mac[i]) return false; } // authentication failed

    crypto_chacha20_ietf(randoms, buf36, 16, _static_key, nonce, 1);
    memcpy(&group_random, randoms, 8);
    memcpy(&own_random, randoms + 8, 8);

    if (!_key_rx_set[0] || group_random != _receive_random[0]) SetReceiveKey(0, group_random);
    if (_key_tx_set && own_random == _transmit_random) return false; // a repeat: same key, the counter goes on
    SetTransmitKey(own_random);
    return true;
}


//-- API miscellaneous

bool tCrypto::NonceRunsOut(void)
{
    if (!_nonce_len || _nonce_len >= 4) return false; // 4 bytes last years
    return (_nonce_u32 >= (1UL << (8 * _nonce_len)) - (1UL << 20)); // over an hour ahead, at any frame rate
}


uint16_t tCrypto::NonceLen(void)
{
    if (!_privacy_level) return 0; // no encryption

    return _nonce_len + _mac_len;
}


void tCrypto::Encrypt(void* const header, uint8_t header_len, void* const data, uint8_t len)
{
    if (!_privacy_level) return; // no encryption

    _encrypt_it((uint8_t*)header, header_len, (uint8_t*)data, len);
}


void tCrypto::RecalculateMac(void* const header, uint8_t header_len, void* const data, uint8_t len)
{
    if (!_privacy_level) return; // no encryption

    _remac_it((uint8_t*)header, header_len, (uint8_t*)data, len);
}


bool tCrypto::Decrypt(uint8_t slot, void* const header, uint8_t header_len, void* const data, uint8_t len)
{
    if (!_privacy_level) return true; // no encryption
    if (slot >= CRYPTO_SLOT_NUM) return false;

    return _decrypt_it(slot, (uint8_t*)header, header_len, (uint8_t*)data, len);
}


//-------------------------------------------------------
// Encryption handlers
//-------------------------------------------------------

// The data is transmitted encrypted, in the following format:
//   0/3/8 bytes mac
//   3/4 bytes nonce
//   data

// Note: The outside code must ensure that payload_len is adjusted correct,
// so that payload_len + nonce_len + mac_len never exceeds the size of the payload buffer.
void tCrypto::_encrypt_it(uint8_t* const header, uint8_t header_len, uint8_t* const data, uint8_t len)
{
uint8_t nonce[12];
uint8_t mac[16];

    // update nonce
    _nonce_u32++;

    // create 12-byte nonce
    memset(nonce, 0, 12);
    memcpy(nonce, &_nonce_u32, _nonce_len); // _nonce[0] ... _nonce[nonce_len - 1] = _nonce_u32

    // fake the nonce for role
    nonce[11] = (_role == RX) ? 0xAA : 0x55;

    // encrypt data at data[0]
    _crypt_it(_key_tx, data, len, nonce);

    if (_mac_len) {
        // MAC = poly1305(header || nonce || ciphertext)
        _mac_it(_key_tx, mac, header, header_len, data, len, nonce, _nonce_len);
    }

    // move data to data + mac_len + nonce_len
    memmove(data + _mac_len + _nonce_len, data, len); // NOT memcpy(), needs to copy from end towards beginning !!

    // copy mac into data
    memcpy(data, mac, _mac_len); // data[0] ... data[mac_len - 1]

    // copy nonce into data
    memcpy(data + _mac_len, nonce, _nonce_len); // data[mac_len] ... data[mac_len + nonce_len - 1]
}


void tCrypto::_remac_it(uint8_t* const header, uint8_t header_len, uint8_t* const data, uint8_t len)
{
uint8_t nonce[12];
uint8_t mac[16];

    if (!_mac_len) return; // nothing to do

    if (len < _mac_len + _nonce_len) return; // should not happen, note: len is here NOT the payload len!

    // get nonce from data
    memset(nonce, 0, 12);
    memcpy(nonce, data + _mac_len, _nonce_len);

    // fake the nonce for role
    nonce[11] = (_role == RX) ? 0xAA : 0x55;

    // MAC the existing encrypted data, not plain palyoad
    _mac_it(_key_tx, mac, header, header_len, data + _mac_len + _nonce_len, len - _mac_len - _nonce_len, nonce, _nonce_len);

    // copy new mac into data; leave nonce and ciphertext untouched
    memcpy(data, mac, _mac_len); // data[0] ... data[mac_len - 1]
}


bool tCrypto::_decrypt_it(uint8_t slot, uint8_t* const header, uint8_t header_len, uint8_t* const data, uint8_t len)
{
uint8_t received_mac[LVL3_MAC_LEN];
uint8_t nonce[12];
uint32_t nonce_u32;
uint8_t mac[16];

    if (len < _mac_len + _nonce_len) { // higher level code should prevent this, play it safe
        return false;
    }

    // get mac from data
    memcpy(received_mac, data, _mac_len); // data[0] ... data[mac_len - 1]

    // get nonce from data
    memset(nonce, 0, 12);
    nonce_u32 = 0;
    memcpy(nonce, data + _mac_len, _nonce_len); // data[mac_len] ... data[mac_len + nonce_len - 1]
    memcpy(&nonce_u32, nonce, _nonce_len);      // _nonce_u32 = _nonce[0] ... _nonce[nonce_len - 1]

    // correct len for the mac and nonce
    len -= _mac_len + _nonce_len;

    // move data to data[0]
    memmove(data, data + _mac_len + _nonce_len, len); // NOT memcpy(), needs to copy from beginning towards end !!

    // fake the nonce for role
    nonce[11] = (_role == TX) ? 0xAA : 0x55;

    if (_mac_len) {
        // calculate MAC over header + nonce + payload
        _mac_it(_key_rx[slot], mac, header, header_len, data, len, nonce, _nonce_len);

        // comparison of mac_len byte mac
        bool ok = true;
        for (uint8_t i = 0; i < _mac_len; i++) { if (mac[i] != received_mac[i]) ok = false; }

        if (!ok) { // authentication failed
            mac_errors++;
            return false;
        }
    }

    // check nonce, don't accept previously seen nonces, to prevent replay attacks
    // do only for privacy levels > 1
    // TODO: what needs to be done upon connection loss? does it play well with ARQ?
    if (_privacy_level >= 2 && nonce_u32 <= _nonce_u32_last_received[slot]) {
        replay_counts++;
        //return false;
    }
    _nonce_u32_last_received[slot] = nonce_u32;

    // decrypt data at data[0]
    _crypt_it(_key_rx[slot], data, len, nonce);

    return true;
}


//-------------------------------------------------------
// Monocypher interface
//-------------------------------------------------------

void tCrypto::_crypt_it(const uint8_t key[32], uint8_t* data, uint16_t len, uint8_t nonce[12])
{
// Note: the counter does not have to start at 0, one just needs to use
// different counter for each block, so always starting with 1 is fine

    crypto_chacha20_ietf(
        data,         // cipher_text,
        data,         // plain_text, same as cipher = in-place encoding
        len,          // text_size,
        key,          // key[32],
        nonce,        // nonce[12],
        1);           // ctr
}


void tCrypto::_mac_it(const uint8_t key[32], uint8_t mac[16], uint8_t* const header, uint8_t header_len, uint8_t* const data, uint16_t len, uint8_t nonce[12], uint8_t nonce_len)
{
uint8_t poly1305_key[32];
crypto_poly1305_ctx ctx;

// Note: the ChaCha20 keystream of the first block is used as key for poly1305 (which wants 32 bytes key)
// so, we use counter = 0
// it is important that for the data then a different counter is used, so use counter = 1 there

    crypto_chacha20_ietf(
        poly1305_key, // cipher_text,
        NULL,         // plain_text, NULL = returns ChaCha20 keystream
        32,           // text_size,
        key,          // key[32],
        nonce,        // nonce[12],
        0);           // ctr

    crypto_poly1305_init(&ctx, poly1305_key);
    crypto_poly1305_update(&ctx, header, header_len);
    crypto_poly1305_update(&ctx, nonce, nonce_len);
    crypto_poly1305_update(&ctx, data, len);
    crypto_poly1305_final(&ctx, mac);
}

