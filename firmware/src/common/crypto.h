//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
// TetherLRS changes: Copyright (c) TetherLRS project
//*******************************************************
// CRYPTO
//*******************************************************
// Based on Monocypher, https://github.com/LoupVaillant/monocypher
/*
Key handling:
- the static key comes from the bind phrase and the link key when one is set, else from the bind phrase, the Tx uid
  and the 8-byte random the Tx sent at bind. Without a link key that material went over the air in plain text at
  bind, so bind in a safe place and bind again if in doubt. With a link key no air bind is needed: radios flashed
  with the same phrase and key share the static key.
- the keys of a session: the Tx draws a random at start for the frames it sends (one key for every receiver of
  the group) and a random for each receiver when it (re)connects it, for the frames that receiver sends. Both go to
  the receiver in GET_RX_SETUPDATA_STARTUP, encrypted and authenticated with the static key (12-byte random nonce,
  8-byte MAC). The receiver takes them whenever they change, so a restarted Tx re-keys it and a restarted receiver
  gets a fresh key of its own: no nonce is ever used twice under one key.
- depending on the privacy level, the nonce is 3 or 4 bytes, and a mac for authentication is 0, 3, or 8 bytes
- replay attacks can be prevented by requiring the nonce to monotonously increase (not yet implemented)
- privacy levels
    off: nothing
    level 1: only encryption                       only payload  (3 bytes nonce, no authentication, no replay attack prevention)
    level 2: encryption + authentication           payload       (3 bytes nonce, 3 bytes mac)
    level 3: stronger encryption + authentication  payload       (4 bytes nonce, 8 bytes mac)
*/
//*******************************************************
#ifndef CRYPTO_H
#define CRYPTO_H
#pragma once


#include <inttypes.h>


#define CRYPTO_STARTUP_RANDOM_BUF_LEN  36 // two 8-byte randoms, the 12-byte nonce and the 8-byte mac
#define CRYPTO_NONCE_MAX_LEN  12 // maximum length of nonce and mac
#define CRYPTO_SLOT_NUM  4 // receivers a Tx holds a receive key for; the same on both roles, this header is one TU's view

typedef enum {
    CRYPTO_STARTUP_REFUSED = 0, // its MAC did not hold: not from the Tx, or for another slot
    CRYPTO_STARTUP_SAME, // the keys held already, the counter goes on
    CRYPTO_STARTUP_NEW_KEY, // a new transmit key
} CRYPTO_STARTUP_ENUM;


class tCrypto
{
  public:
    typedef enum {
        TX = 0,
        RX,
    } ROLE_ENUM;

    void Init(
        uint8_t role,
        char* const bind_phrase, uint8_t tx_uid[12], uint64_t tx_random,
        uint8_t privacy_level, const uint8_t* const link_key);

    void SetTransmitKey(uint64_t random); // the key of the frames this side sends
    void SetReceiveKey(uint8_t slot, uint64_t random); // the key of the frames received from that slot, the Rx uses slot 0
    void EncryptSessionRandoms(uint8_t* const buf36, uint8_t slot, uint64_t startup_random, uint64_t bind_random); // Tx only
    uint8_t SetSessionKeysFromEncryptedBuf(uint8_t* const buf36, uint8_t slot); // Rx only, CRYPTO_STARTUP_ENUM

    bool ValidKeys(uint8_t slot = 0);
    bool SessionKeys(void) { return !_privacy_level || (_key_tx_set && _key_rx_set[0]); } // Rx: both, from a STARTUP

    uint8_t PrivacyLevel(void) { return _privacy_level; }
    bool IsAuthenticated(void) { return (_privacy_level >= 2); }
    uint16_t NonceLen(void);
    bool NonceRunsOut(void); // a 3 byte nonce nears its wrap under this transmit key, which has to change
    void Encrypt(void* const header, uint8_t header_len, void* const data, uint8_t len);
    void RecalculateMac(void* const header, uint8_t header_len, void* const data, uint8_t len);
    bool Decrypt(uint8_t slot, void* const header, uint8_t header_len, void* const data, uint8_t len);

    uint64_t SessionRandom(void) { return (_key_rx_set[0]) ? _receive_random[0] : 0; } // Rx only, only for reporting, no function

    uint32_t mac_errors;
    uint32_t replay_counts;

  private:
    uint8_t _role;
    uint8_t _privacy_level;

    uint8_t _nonce_len;
    uint8_t _mac_len;

    bool _static_valid;
    uint8_t _static_source[80]; // 78 used
    uint8_t _static_key[32];

    uint32_t _startup_nonce_u32;

    uint64_t _transmit_random;
    uint8_t _key_tx[32];
    bool _key_tx_set;
    uint32_t _nonce_u32;

    uint64_t _receive_random[CRYPTO_SLOT_NUM];
    uint8_t _key_rx[CRYPTO_SLOT_NUM][32];
    bool _key_rx_set[CRYPTO_SLOT_NUM];
    uint32_t _nonce_u32_last_received[CRYPTO_SLOT_NUM];

    void _derive_key(uint8_t key[32], uint64_t random);

    void _encrypt_it(uint8_t* const header, uint8_t header_len, uint8_t* const data, uint8_t len);
    void _remac_it(uint8_t* const header, uint8_t header_len, uint8_t* const data, uint8_t len);
    bool _decrypt_it(uint8_t slot, uint8_t* const header, uint8_t header_len, uint8_t* const data, uint8_t len);

    void _crypt_it(const uint8_t key[32], uint8_t* data, uint16_t len, uint8_t nonce[12]);
    void _mac_it(const uint8_t key[32], uint8_t mac[16], uint8_t* const header, uint8_t header_len, uint8_t* const data, uint16_t len, uint8_t nonce[12], uint8_t nonce_len);
};


#endif // CRYPTO_H
