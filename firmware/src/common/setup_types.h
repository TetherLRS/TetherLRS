//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
// TetherLRS changes: Copyright (c) TetherLRS project
//*******************************************************
// Setup and configuration types
//*******************************************************
#ifndef SETUP_TYPES_H
#define SETUP_TYPES_H
#pragma once


#include <stdint.h>


//-------------------------------------------------------
// Setup Enums
// ATTENTION: only extend/append, never change sequence
//-------------------------------------------------------

//-- common to Tx & Rx

// these are the frequency band options available to the user
typedef enum : uint8_t {
    SETUP_FREQUENCY_BAND_2P4_GHZ = 0,
    SETUP_FREQUENCY_BAND_915_MHZ_FCC,
    SETUP_FREQUENCY_BAND_868_MHZ,
    SETUP_FREQUENCY_BAND_433_MHZ,
    SETUP_FREQUENCY_BAND_70_CM_HAM,
    SETUP_FREQUENCY_BAND_866_MHZ_IN,
    SETUP_FREQUENCY_DUAL_BAND_915_MHZ_2P4_GHZ,
    SETUP_FREQUENCY_DUAL_BAND_868_MHZ_2P4_GHZ,
    SETUP_FREQUENCY_BAND_NUM,
} SETUP_FREQUENCY_BAND_ENUM;


// these are the frequency band options available to the hardware
// used in the SX12xx drivers and FHSS class
// should not be defined here, but we do here for convenience
typedef enum : uint8_t {
    SX_FHSS_FREQUENCY_BAND_2P4_GHZ = 0,
    SX_FHSS_FREQUENCY_BAND_915_MHZ_FCC,
    SX_FHSS_FREQUENCY_BAND_868_MHZ,
    SX_FHSS_FREQUENCY_BAND_866_MHZ_IN,
    SX_FHSS_FREQUENCY_BAND_433_MHZ,
    SX_FHSS_FREQUENCY_BAND_70_CM_HAM,
    SX_FHSS_FREQUENCY_BAND_NUM,
} SX_FHSS_FREQUENCY_BAND_ENUM;


// channel plans replacing the band's default list, see fhss.h
typedef enum : uint8_t {
    FHSS_PLAN_DEFAULT = 0,
    FHSS_PLAN_868_EU_LORA,
    FHSS_PLAN_868_EU_FSK,
    FHSS_PLAN_869_LR,
    FHSS_PLAN_NUM,
} FHSS_PLAN_ENUM;


// used by setup
SX_FHSS_FREQUENCY_BAND_ENUM cvt_to_sx_fhss_frequency_band(uint8_t setup_frequency_band);

// used by fhss, for bind only
SETUP_FREQUENCY_BAND_ENUM cvt_to_setup_frequency_band(uint8_t sx_fhss_frequency_band);


typedef enum {
    MODE_50HZ = 0,
    MODE_31HZ,
    MODE_19HZ,
    MODE_FLRC_111HZ,
    MODE_FSK_50HZ,
    MODE_19HZ_7X,
    MODE_18HZ, // 31 Hz radio settings at 55 ms, fits the EU 868 MHz on-time limits
    MODE_LR, // one 125 kHz channel at 869.525 MHz, slow, 500 mW allowed in the EU
    MODE_NUM,
} MODE_ENUM;


// regulatory rules applied at runtime, see regulatory.h
typedef enum {
    REGION_FREE = 0, // as mLRS, compliance left to the user
    REGION_EU,
    REGION_CUSTOM, // the user's own frequency plan, see Setup.Common.Custom*
    REGION_NUM,
} REGION_ENUM;


typedef enum {
    ORTHO_NONE = 0,
    ORTHO_1_3,
    ORTHO_2_3,
    ORTHO_3_3,
    ORTHO_NUM,
} ORTHO_ENUM;


typedef enum {
    PRIVACY_NONE = 0,
    PRIVACY_LEVEL1,
    PRIVACY_LEVEL2,
    PRIVACY_LEVEL3,
    PRIVACY_NUM,
} PRIVACY_ENUM;


typedef enum {
    EXCEPT_NONE = 0,
    EXCEPT_2P4_GHZ_WIFIBAND_1,
    EXCEPT_2P4_GHZ_WIFIBAND_6,
    EXCEPT_2P4_GHZ_WIFIBAND_11,
    EXCEPT_2P4_GHZ_WIFIBAND_13,
    EXCEPT_NUM,
} EXCEPT_ENUM;


typedef enum {
    DIVERSITY_DEFAULT = 0, // diversity enabled, both receive and transmit
    DIVERSITY_ANTENNA1, // antenna 1
    DIVERSITY_ANTENNA2, // antenna 2
    DIVERSITY_R_ENABLED_T_ANTENNA1, // receive diversity enabled, transmit antenna 1
    DIVERSITY_R_ENABLED_T_ANTENNA2, // receive diversity enabled, transmit antenna 2
    DIVERSITY_NUM,
} DIVERSITY_ENUM;


typedef enum {
    SERIAL_BAUDRATE_57600 = 0,
    SERIAL_BAUDRATE_115200,
    SERIAL_BAUDRATE_230400,
    SERIAL_BAUDRATE_NUM,
} SERIAL_BAUDRATE_ENUM;

typedef enum {
    SERIAL_LINK_MODE_TRANSPARENT = 0,
    SERIAL_LINK_MODE_MAVLINK,
    SERIAL_LINK_MODE_MAVLINK_X,
    SERIAL_LINK_MODE_MSP_X,
    SERIAL_LINK_MODE_NUM,
} SERIAL_LINK_MODE_ENUM;


#define SERIAL_LINK_MODE_IS_MAVLINK(x)  ((x) == SERIAL_LINK_MODE_MAVLINK || (x) == SERIAL_LINK_MODE_MAVLINK_X)
#define SERIAL_LINK_MODE_IS_MSP(x)      ((x) == SERIAL_LINK_MODE_MSP_X)


typedef enum {
    TX_SEND_RADIO_STATUS_OFF = 0,
    TX_SEND_RADIO_STATUS_1HZ,
    TX_SEND_RADIO_STATUS_NUM,
} TX_SEND_RADIO_STATUS_ENUM;


typedef enum {
    TX_MAVLINK_COMPONENT_OFF = 0,
    TX_MAVLINK_COMPONENT_ENABLED,
    TX_MAVLINK_COMPONENT_NUM,
} TX_MAVLINK_COMPONENT_ENUM;


typedef enum {
    RX_SEND_RADIO_STATUS_OFF = 0,
    RX_SEND_RADIO_STATUS_METHOD_ARDUPILOT_1,
    RX_SEND_RADIO_STATUS_METHOD_PX4,
    RX_SEND_RADIO_STATUS_NUM,
} RX_SEND_RADIO_STATUS_ENUM;


//-- Tx only

typedef enum {
    TX_SERIAL_PORT_SERIAL = 0,
    TX_SERIAL_PORT_WIRELESS_BRIDGE,
    TX_SERIAL_PORT_SERIAL2,
    TX_SERIAL_PORT_COM,
    TX_SERIAL_PORT_USB, // the chip's own USB (CDC), where the board brings it out
    TX_SERIAL_PORT_NUM,
} TX_SERIAL_PORT_ENUM;


typedef enum {
    TX_SERIAL_PORT2_NONE = 0,
    TX_SERIAL_PORT2_SERIAL,
    TX_SERIAL_PORT2_WIRELESS_BRIDGE,
    TX_SERIAL_PORT2_SERIAL2,
    TX_SERIAL_PORT2_NUM,
} TX_SERIAL_PORT2_ENUM;


typedef enum {
    BUZZER_OFF = 0,
    BUZZER_LOST_PACKETS,
    BUZZER_RX_LQ,
    BUZZER_NUM,
} TX_BUZZER_END_ENUM;

typedef enum {
    TX_DOWNLINK_ON = 0,
    TX_DOWNLINK_OFF, // the receiver stays silent: data only to the aircraft, no telemetry, no link stats on the ground
    TX_DOWNLINK_NUM,
} TX_DOWNLINK_ENUM;


// how the two frames of a period share the 182 bytes of air time, see frame_types.h
typedef enum {
    LINK_SPLIT_BALANCED = 0, // 82 bytes of data each way
    LINK_SPLIT_DOWNLINK, // 64 up, 100 down: telemetry
    LINK_SPLIT_UPLINK, // 100 up, 64 down: corrections, commands
    LINK_SPLIT_NUM,
} LINK_SPLIT_ENUM;


// what a pair with two radios on each side does with them
typedef enum {
    RADIO_SCHEME_AUTO = 0, // the best the pair can do
    RADIO_SCHEME_SINGLE, // one link, the second radio for diversity
    RADIO_SCHEME_DUAL_REDUNDANT, // the same frame on two channels at once
    RADIO_SCHEME_DUAL_BONDED, // two links in lock-step, twice the data
    RADIO_SCHEME_DUAL_BAND, // the same frame on both bands at once
    RADIO_SCHEME_NUM,
} RADIO_SCHEME_ENUM;


//-- Rx only

typedef enum {
    RX_DOWNLINK_AS_TX = 0, // transmits unless the Tx says not to
    RX_DOWNLINK_OFF, // never transmits: a second receiver beside one that answers
    RX_DOWNLINK_NUM,
} RX_DOWNLINK_ENUM;


typedef enum {
    RX_SERIAL_OUT2_OFF = 0,
    RX_SERIAL_OUT2_TX_ON_RX_PIN, // the link's data also out of the serial port's RX pin, which then receives nothing
    RX_SERIAL_OUT2_SERIAL1, // the board's second serial port, where the layout has one
    RX_SERIAL_OUT2_NUM,
} RX_SERIAL_OUT2_ENUM;


typedef enum {
    RX_TEST_OFF = 0,
    RX_TEST_LOOPBACK, // the link's data goes back up instead of out of the serial port, for the tool's benchmark
    RX_TEST_NUM,
} RX_TEST_ENUM;


typedef enum {
    WIFI_PROTOCOL_TCP = 0,
    WIFI_PROTOCOL_UDP,
    WIFI_PROTOCOL_BT,
    WIFI_PROTOCOL_UDPSTA,
    WIFI_PROTOCOL_BLE,
    WIFI_PROTOCOL_ESPNOW,
    WIFI_PROTOCOL_NUM,
} TX_WIFI_PROTOCOL_ENUM;


typedef enum {
    WIFI_CHANNEL_1 = 0,
    WIFI_CHANNEL_6,
    WIFI_CHANNEL_11,
    WIFI_CHANNEL_13,
    WIFI_CHANNEL_NUM,
} TX_WIFI_CHANNEL_ENUM;


typedef enum {
    WIFI_POWER_LOW = 0,
    WIFI_POWER_MED,
    WIFI_POWER_MAX,
    WIFI_POWER_NUM,
} TX_WIFI_POWER_ENUM;


//-- Rx only

typedef enum {
    RX_SERIAL_PORT_SERIAL = 0,
    RX_SERIAL_PORT_CAN,
    RX_SERIAL_PORT_CANFD,
    RX_SERIAL_PORT_USB, // the chip's own USB (CDC), e.g. towards a companion computer
    RX_SERIAL_PORT_NUM,
} RX_SERIAL_PORT_ENUM;

#define RX_SERIAL_PORT_IS_CAN(x)  ((x) == RX_SERIAL_PORT_CAN || (x) == RX_SERIAL_PORT_CANFD)


//-------------------------------------------------------
// Config Enums
// ATTENTION: numbers must match config arrays in sx drivers !!
//-------------------------------------------------------

typedef enum {
    SX128x_LORA_CONFIG_BW800_SF5_CRLI4_5 = 0,
    SX128x_LORA_CONFIG_BW800_SF6_CRLI4_5,
    SX128x_LORA_CONFIG_BW800_SF7_CRLI4_5,
    SX128x_LORA_CONFIG_NUM,
} SX128x_LORA_CONFIG_ENUM;


typedef enum {
    SX127x_LORA_CONFIG_BW500_SF6_CR4_5 = 0,
    SX127x_LORA_CONFIG_NUM,
} SX127x_LORA_CONFIG_ENUM;


typedef enum {
    SX126x_LORA_CONFIG_BW500_SF5_CR4_5 = 0,
    SX126x_LORA_CONFIG_BW500_SF6_CR4_5,
    SX126x_LORA_CONFIG_NUM,
} SX126x_LORA_CONFIG_ENUM;


typedef enum {
    LR11xx_LORA_CONFIG_BW500_SF5_CR4_5 = 0, // 900 MHz
    LR11xx_LORA_CONFIG_BW500_SF6_CR4_5,
    LR11xx_LORA_CONFIG_BW800_SF5_CR4_5, // 2.4 GHz
    LR11xx_LORA_CONFIG_BW800_SF6_CR4_5,
    LR11xx_LORA_CONFIG_BW800_SF7_CR4_5,
    LR11xx_LORA_CONFIG_BW125_SF5_CR4_5, // 900 MHz LR
    LR11xx_LORA_CONFIG_NUM,
} LR11xx_LORA_CONFIG_ENUM;


typedef enum {
    LR20xx_LORA_CONFIG_BW500_SF5_CR4_5 = 0, // 900 MHz
    LR20xx_LORA_CONFIG_BW500_SF6_CR4_5,
    LR20xx_LORA_CONFIG_BW800_SF5_CR4_5, // 2.4 GHz
    LR20xx_LORA_CONFIG_BW800_SF6_CR4_5,
    LR20xx_LORA_CONFIG_BW800_SF7_CR4_5,
    LR20xx_LORA_CONFIG_NUM,
} LR20xx_LORA_CONFIG_ENUM;


//-------------------------------------------------------
// Setup Types
// ATTENTION: only extend/append, never change sequence
//-------------------------------------------------------

typedef struct
{
    char BindPhrase[6+1];
    SETUP_FREQUENCY_BAND_ENUM FrequencyBand;
    uint8_t Mode;
    uint8_t Ortho;
    uint8_t Privacy;
    uint8_t Region;
    uint8_t DynPower;
    uint8_t LinkSplit;
    uint8_t RadioScheme;
    uint8_t CustomBw; // index among the chip's LoRa bandwidths
    uint32_t CustomFreqKHz; // centre of the custom plan
    uint16_t CustomSpanKHz; // its width, the channels come from span / bandwidth
    uint8_t CustomLbt;
    uint8_t CustomPowerMaxDbm;

    uint8_t spare[4];
} tCommonSetup; // 28 bytes


typedef struct
{
    uint8_t Power;
    uint8_t Diversity;
    uint8_t SerialPort;
    uint8_t SerialBaudrate;
    uint8_t SerialPort2;
    uint8_t SendRadioStatus;
    uint8_t Buzzer;
    uint8_t SerialBaudrate2;
    uint8_t MavlinkComponent;
    uint8_t WifiProtocol;
    uint8_t WifiChannel;
    uint8_t WifiPower;
    uint8_t Downlink;

    uint8_t spare[3];
} tTxSetup; // 16 bytes


typedef struct
{
    uint8_t Power;
    uint8_t Diversity;
    uint8_t SerialBaudrate;
    uint8_t SerialLinkMode;
    uint8_t SendRadioStatus;
    uint8_t SerialPort;
    uint8_t MavlinkSystemID;
    uint8_t MavCoalesce; // drop stale copies of state messages waiting for the link
    uint8_t Downlink; // RX_DOWNLINK_ENUM
    uint8_t SerialOut2; // RX_SERIAL_OUT2_ENUM
    uint8_t SerialBaudrate2;
    uint8_t Test; // RX_TEST_ENUM

    uint8_t spare[4];
} tRxSetup; // 16 bytes


#define SETUP_MARKER_RX_STR   "SetupStartStx4Rx"
#define SETUP_MARKER_TX_STR   "SetupStartStx4Tx"
#define SETUP_MARKER_OLD_STR  "SetupStartMarker"
#define SETUP_MARKEREND_STR   "!end!"

#define SETUP_CONFIG_NUM      10 // not more, so it's only one char '0'...'9'


// user setable parameter values, stored in EEPROM
typedef struct
{
    char MarkerStr[16];
    uint32_t Version;
    uint16_t Layout;

    // auxilliary
    uint8_t spare[5];
    uint8_t BindStamp[4]; // TetherLRS: the stamp of the image whose bind phrase was taken, see tlrs_bind.h

    uint8_t _ConfigId; // strange name to avoid mistake

    uint8_t spare1[8]; // 8 keeps Common, with its uint32_t, 4-byte aligned without hidden padding

    // parameters specific to Rx, can be changed on the fly
    // for transmitters this is populated upon first connection, see SetupMetaData.rx_available mechanism
    tRxSetup Rx;

    // parameters specific to Tx, can be changed on the fly
    // not used by receivers
    tTxSetup Tx[SETUP_CONFIG_NUM];

    // parameters common to both Tx and Rx
    // cannot be changed on the fly, loss of connection will happen, needs restart/reconnect
    tCommonSetup Common[SETUP_CONFIG_NUM];

    // crypto
    uint8_t peer_uid[SETUP_CONFIG_NUM][12]; // 12 bytes = 96 bits
    uint64_t tx_random[SETUP_CONFIG_NUM];   //  8 bytes = 64 bits
    uint8_t LinkKey[32]; // from the image's marker or the local CLI, never over the air; all zero = not set

    char MarkerEnd[8];
} tSetup;


//-------------------------------------------------------
// MetaData and Config Types
//-------------------------------------------------------

typedef struct
{
    uint16_t FrequencyBand_allowed_mask;
    uint16_t Mode_allowed_mask;
    uint16_t Ortho_allowed_mask;
    uint16_t Privacy_allowed_mask;
    uint16_t Region_allowed_mask;

    char Tx_Power_optstr[67+1];
    uint16_t Tx_Diversity_allowed_mask;
    uint16_t Tx_SerialPort_allowed_mask;
    uint16_t Tx_SerialPort2_allowed_mask;
    uint16_t Tx_SerialBaudrate2_allowed_mask;
    uint16_t Tx_Buzzer_allowed_mask;
    uint16_t Tx_WiFiProt_allowed_mask;

    char Rx_Power_optstr[67+1];
    uint16_t Rx_Diversity_allowed_mask;
    uint16_t Rx_SerialPort_allowed_mask;
    uint16_t Rx_SerialOut2_allowed_mask;

    bool rx_available;

    uint32_t rx_firmware_version; // 6.99.99 + enough head room
    uint32_t rx_setup_layout; // 6.99.99 + enough head room
    char rx_device_name[20+1];
    int8_t rx_actual_power_dbm;
    uint8_t rx_actual_diversity;
    uint16_t rx_FrequencyBand_allowed_mask; // what the connected receiver can take
    uint16_t rx_Mode_allowed_mask;
    uint8_t rx_radio_count; // radios the connected receiver has
    uint8_t rx_radio_bands; // bit per SX_FHSS_FREQUENCY_BAND_ENUM it can run
} tSetupMetaData;


// global configuration values, not stored in EEPROM
// can be/are derived from setup parameters, from defines, or otherwise
typedef struct
{
    uint8_t LoraConfigIndex;
    uint16_t FskSyncWord;
    uint32_t FlrcSyncWord;
    int8_t Power_dbm;
    SX_FHSS_FREQUENCY_BAND_ENUM FrequencyBand;
    // helper
    bool is_lora;
    bool modeIsLora(void) { return is_lora; }
} tSxGlobalConfig;


typedef struct 
{
    uint8_t Num;
    uint32_t Seed;
    SX_FHSS_FREQUENCY_BAND_ENUM FrequencyBand;
    uint8_t Ortho;
    uint8_t Except;
    uint16_t BindScan_mask;
    uint8_t Plan; // FHSS_PLAN_ENUM, regional channel plans
} tFhssGlobalConfig;


typedef struct
{
    uint8_t ConfigId; // we take a copy at startup to avoid confusion

    uint8_t FrequencyBand;
    uint8_t Mode;
    uint8_t Region;
    uint8_t PowerIdxMax; // highest rfpower_list index the region allows

    tSxGlobalConfig Sx;
    tSxGlobalConfig Sx2;
    uint8_t send_frame_tmo_ms;
    uint8_t FrameLenTx; // the Tx->Rx frame, from Link Split
    uint8_t FrameLenRx; // the Rx->Tx frame

    uint16_t FrameSyncWord;

    tFhssGlobalConfig Fhss;
    tFhssGlobalConfig Fhss2;

    uint16_t LQAveragingPeriod;

    uint32_t SerialBaudrate;
    uint32_t SerialBaudrate2;

    uint16_t frame_rate_ms;
    uint16_t frame_rate_hz;
    uint16_t connect_tmo_systicks;
    uint16_t connect_listen_hop_cnt;
    uint8_t connect_sync_cnt_max;

    uint8_t Diversity; // snapshot of Setup's Diversity at startup
    bool ReceiveUseAntenna1;
    bool ReceiveUseAntenna2;
    bool TransmitUseAntenna1;
    bool TransmitUseAntenna2;
    bool IsDualBand;


    // crypto
    uint8_t Uid[12];
    uint64_t BindRandom;
    uint64_t SessionRandom;
    uint64_t StartupRandom;
} tGlobalConfig;


//-------------------------------------------------------
// Defines
//-------------------------------------------------------

#define SEND_FRAME_TMO_MS         Config.send_frame_tmo_ms

#define CONNECT_TMO_SYSTICKS      Config.connect_tmo_systicks
#define CONNECT_LISTEN_HOP_CNT    Config.connect_listen_hop_cnt


#endif // SETUP_TYPES_H
