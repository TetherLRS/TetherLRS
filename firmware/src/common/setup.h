//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
// TetherLRS changes: Copyright (c) TetherLRS project
//*******************************************************
// Setup
//*******************************************************
#ifndef SETUP_H
#define SETUP_H
#pragma once


#include "setup_types.h"
#include "hal/hal.h"
#include "auto_ladder.h"
#include "thirdparty/monocypher/src/monocypher.h"


tSetupMetaData SetupMetaData;
tSetup Setup;
tGlobalConfig Config;
uint16_t mode_hw_mask; // the modes the hardware has, before a band filters them


// the EU region needs an instantaneous RSSI read for listen before talk, and one band at a time
#if (defined DEVICE_HAS_LR11xx || defined DEVICE_HAS_SX126x || defined DEVICE_HAS_SX128x) && \
    !defined DEVICE_HAS_DUAL_SX126x_SX128x && !defined DEVICE_HAS_DUAL_SX126x_SX126x
  #define USE_REGION_EU
#endif


//-------------------------------------------------------
// Configure Setup MetaData
//-------------------------------------------------------

void setup_configure_metadata(void)
{
    SetupMetaData = {};

    //-- FrequencyBand: "2.4,915 FCC,868,433,70,866 IN,915+2.4,868+2.4"
    // also define the default setting for use below in setup_sanitize_config()
//** dual band
#if defined DEVICE_HAS_DUAL_SX126x_SX128x
  // DUALBAND 2.4 GHz & 868/915 MHz
  #if defined FREQUENCY_BAND_2P4_GHZ && defined FREQUENCY_BAND_915_MHZ_FCC && defined FREQUENCY_BAND_868_MHZ
    SetupMetaData.FrequencyBand_allowed_mask = 0b11000111; // 2.4 GHz, 915 FCC, 868, 915+2.4, 868+2.4
    #define FREQUENCY_BAND_DEFAULT  SETUP_FREQUENCY_BAND_868_MHZ
  #else
    #error Unknown Frequencyband !
  #endif
#elif defined DEVICE_HAS_DUAL_SX126x_SX126x
  // DUALBAND 868/915 MHz & 433 MHz
  #if defined FREQUENCY_BAND_915_MHZ_FCC && defined FREQUENCY_BAND_868_MHZ && defined FREQUENCY_BAND_433_MHZ
    SetupMetaData.FrequencyBand_allowed_mask = 0b00000110; // 915 FCC, 868
    #define FREQUENCY_BAND_DEFAULT  SETUP_FREQUENCY_BAND_868_MHZ
  #else
    #error Unknown Frequencyband !
  #endif
//** multi band
#elif defined FREQUENCY_BAND_2P4_GHZ && defined FREQUENCY_BAND_915_MHZ_FCC && defined FREQUENCY_BAND_868_MHZ
    // MULTIBAND 2.4 GHz & 868/915 MHz
    SetupMetaData.FrequencyBand_allowed_mask = 0b00000111; // 2.4 GHz, 915 FCC, 868
    #define FREQUENCY_BAND_DEFAULT  SETUP_FREQUENCY_BAND_868_MHZ
#elif defined FREQUENCY_BAND_2P4_GHZ && defined FREQUENCY_BAND_433_MHZ && defined FREQUENCY_BAND_70_CM_HAM
    // MULTIBAND 2.4 GHz & 433 MHz/70 cm
    SetupMetaData.FrequencyBand_allowed_mask = 0b00011001; // 2.4 GHz, 433, 70
    #define FREQUENCY_BAND_DEFAULT  SETUP_FREQUENCY_BAND_433_MHZ
//** single band, multiple frequencies
#elif defined FREQUENCY_BAND_915_MHZ_FCC && defined FREQUENCY_BAND_868_MHZ && \
      defined FREQUENCY_BAND_433_MHZ && defined FREQUENCY_BAND_70_CM_HAM
    SetupMetaData.FrequencyBand_allowed_mask = 0b00011110; // 915 FCC, 868, 433, 70
    #define FREQUENCY_BAND_DEFAULT  SETUP_FREQUENCY_BAND_868_MHZ
#elif defined FREQUENCY_BAND_915_MHZ_FCC && defined FREQUENCY_BAND_868_MHZ && defined FREQUENCY_BAND_866_MHZ_IN
    SetupMetaData.FrequencyBand_allowed_mask = 0b00100110; // 915 FCC, 868, 866 IN
    #define FREQUENCY_BAND_DEFAULT  SETUP_FREQUENCY_BAND_868_MHZ
#elif defined FREQUENCY_BAND_915_MHZ_FCC && defined FREQUENCY_BAND_868_MHZ
    SetupMetaData.FrequencyBand_allowed_mask = 0b00000110; // 915 FCC, 868
    #define FREQUENCY_BAND_DEFAULT  SETUP_FREQUENCY_BAND_868_MHZ
#elif defined FREQUENCY_BAND_433_MHZ && defined FREQUENCY_BAND_70_CM_HAM
    SetupMetaData.FrequencyBand_allowed_mask = 0b00011000; // 433, 70
    #define FREQUENCY_BAND_DEFAULT  SETUP_FREQUENCY_BAND_433_MHZ
//** single band, single frequency
#elif defined FREQUENCY_BAND_2P4_GHZ
    SetupMetaData.FrequencyBand_allowed_mask = 0b00000001; // 2.4 GHz, not editable
    #define FREQUENCY_BAND_DEFAULT  SETUP_FREQUENCY_BAND_2P4_GHZ
#elif defined FREQUENCY_BAND_915_MHZ_FCC
    SetupMetaData.FrequencyBand_allowed_mask = 0b00000010; // 915 MHz FCC, not editable
    #define FREQUENCY_BAND_DEFAULT  SETUP_FREQUENCY_BAND_915_MHZ_FCC
#elif defined FREQUENCY_BAND_868_MHZ
    SetupMetaData.FrequencyBand_allowed_mask = 0b00000100; // 868 MHz, not editable
    #define FREQUENCY_BAND_DEFAULT  SETUP_FREQUENCY_BAND_868_MHZ
#elif defined FREQUENCY_BAND_433_MHZ
    SetupMetaData.FrequencyBand_allowed_mask = 0b00001000; // 433 MHz, not editable
    #define FREQUENCY_BAND_DEFAULT  SETUP_FREQUENCY_BAND_433_MHZ
#elif defined FREQUENCY_BAND_70_CM_HAM
    SetupMetaData.FrequencyBand_allowed_mask = 0b00010000; // 70 cm HAM, not editable
    #define FREQUENCY_BAND_DEFAULT  SETUP_FREQUENCY_BAND_70_CM_HAM
#elif defined FREQUENCY_BAND_866_MHZ_IN
    SetupMetaData.FrequencyBand_allowed_mask = 0b00100000; // 866 MHz IN, not editable
    #define FREQUENCY_BAND_DEFAULT  SETUP_FREQUENCY_BAND_866_MHZ_IN
#else
    #error Unknown Frequencyband !
#endif

    //-- Mode: "50 Hz,31 Hz,19 Hz,FLRC,FSK,19 Hz 7x"
    // also define the default setting for use below in setup_sanitize_config()
#if defined DEVICE_HAS_DUAL_SX126x_SX128x
    // DUALBAND 2.4 GHz & 868/915 MHz
    // we cannot work out all cases here, as it depends on actual FrequencyBand selection, so we here just do what we can do
    SetupMetaData.Mode_allowed_mask = 0b011111; // 50 Hz, 31 Hz, 19 Hz, FLRC, FSK - Note: FSK implies 50 Hz for SX128x
    #define MODE_DEFAULT  MODE_31HZ
#elif defined DEVICE_HAS_DUAL_SX126x_SX126x
    // DUALBAND 868/915 MHz & 433 MHz
    SetupMetaData.Mode_allowed_mask = 0b000110; // 31 Hz, 19 Hz
    #define MODE_DEFAULT  MODE_31HZ
#elif defined DEVICE_HAS_SX128x
    SetupMetaData.Mode_allowed_mask = 0b001111; // 50 Hz, 31 Hz, 19 Hz, FLRC
    #define MODE_DEFAULT  MODE_50HZ
#elif defined DEVICE_HAS_SX126x
    SetupMetaData.Mode_allowed_mask = 0b010110; // 31 Hz, 19 Hz, FSK
    #define MODE_DEFAULT  MODE_31HZ
#elif defined DEVICE_HAS_SX127x
    SetupMetaData.Mode_allowed_mask = 0b100000; // 19 Hz 7x, not editable
    #define MODE_DEFAULT  MODE_19HZ_7X
#elif defined DEVICE_HAS_LR11xx
    // MULTIBAND
    // we cannot work out all cases here, as it depends on actual FrequencyBand selection, so we here just do what we can do
    SetupMetaData.Mode_allowed_mask = 0b110111; // 50 Hz, 31 Hz, 19 Hz, 19 Hz 7x, FSK
    #define MODE_DEFAULT  MODE_31HZ
#elif defined DEVICE_HAS_LR20xx
    // MULTIBAND
    // we cannot work out all cases here, as it depends on actual FrequencyBand selection, so we here just do what we can do
    SetupMetaData.Mode_allowed_mask = 0b111111; // 50 Hz, 31 Hz, 19 Hz, FLRC, FSK, 19 Hz 7x
    #define MODE_DEFAULT  MODE_31HZ
#else
    #error Unknown Mode !
#endif
#if defined DEVICE_HAS_SX126x || defined DEVICE_HAS_LR11xx || defined DEVICE_HAS_LR20xx || defined DEVICE_HAS_DUAL_SX126x_SX128x
    SetupMetaData.Mode_allowed_mask |= (1 << MODE_17HZ) | (1 << MODE_8HZ); // narrow sub-GHz modes, the band filter keeps them off 2.4 GHz
#endif
    mode_hw_mask = SetupMetaData.Mode_allowed_mask;

    //-- Ortho: "off,1/3,2/3,3/3"
    // we cannot work out all cases here, as it depends on actual FrequencyBand selection, so we here just do what we can do
#if defined FREQUENCY_BAND_2P4_GHZ || defined FREQUENCY_BAND_915_MHZ_FCC || defined FREQUENCY_BAND_70_CM_HAM
    SetupMetaData.Ortho_allowed_mask = 0b1111; // all
#else
    SetupMetaData.Ortho_allowed_mask = 0; // not available, do not display
#endif

    //-- Privacy: "off,lvl1,lvl2,lvl3"
#if defined DEVICE_IS_TRANSMITTER && !(defined STM32G4 || defined ESP32) // || defined STM32WL
    SetupMetaData.Privacy_allowed_mask = 0; // not available, do not display
#else
    SetupMetaData.Privacy_allowed_mask = 0b1111; // all
#endif

    //-- Radio Scheme: "auto,single,dual redund,dual bonded,dual band": dual band only on that hardware, which has one
    //   radio per band, so never two on one band
#if defined DEVICE_HAS_DUAL_SX126x_SX128x || defined DEVICE_HAS_DUAL_SX126x_SX126x
    SetupMetaData.RadioScheme_allowed_mask = 0b10011;
#elif defined USE_SX2
    SetupMetaData.RadioScheme_allowed_mask = 0b01111;
#else
    SetupMetaData.RadioScheme_allowed_mask = 0b00011; // auto and single, one radio
#endif

    //-- Region: "free,EU,custom"
#ifdef USE_REGION_EU
    SetupMetaData.Region_allowed_mask = 0b111; // all
#else
    SetupMetaData.Region_allowed_mask = 0b101; // free and custom, custom cannot listen before talk here
#endif

    //-- Tx:

    power_optstr_from_rfpower_list(SetupMetaData.Tx_Power_optstr, rfpower_list, RFPOWER_LIST_NUM, 67);

    // Diversity: "enabled,antenna1,antenna2,r:e t:a1,r:e t:a2"
#if defined DEVICE_HAS_DUAL_SX126x_SX128x
    // we cannot work out all cases here, as it depends on actual FrequencyBand selection, so we here just do what we can do
    SetupMetaData.Tx_Diversity_allowed_mask = 0b00111; // will be adjusted below
#elif defined DEVICE_HAS_DUAL_SX126x_SX126x
    SetupMetaData.Tx_Diversity_allowed_mask = 0b00001; // only enabled, not editable
#elif defined DEVICE_HAS_DIVERSITY
    SetupMetaData.Tx_Diversity_allowed_mask = 0b11111; // all
#elif defined DEVICE_HAS_DIVERSITY_SINGLE_SPI
    SetupMetaData.Tx_Diversity_allowed_mask = 0b11011; // TODO: no antenna2 for the moment
#else
    SetupMetaData.Tx_Diversity_allowed_mask = 0b00010; // antenna1, not editable
#endif

    // Tx SerialPort: "serial,wbridge,serial2,com"
    // Tx SerialPort2: "none,serial,wbridge,serial2"
    SetupMetaData.Tx_SerialPort_allowed_mask = 0; // not available, do not display
    SetupMetaData.Tx_SerialPort2_allowed_mask = 0; // not available, do not display
#ifdef USE_SERIAL
    SetupMetaData.Tx_SerialPort_allowed_mask |= 0b00001; // add serial
    SetupMetaData.Tx_SerialPort2_allowed_mask |= 0b00011; // add none, serial
#endif
#ifdef USE_WIRELESS_BRIDGE
    SetupMetaData.Tx_SerialPort_allowed_mask |= 0b00010; // add wbridge
    SetupMetaData.Tx_SerialPort2_allowed_mask |= 0b00101; // add none, wbridge
#endif
#ifdef DEVICE_HAS_SERIAL2 // attention: USE_SERIAL2 implies either serial2 or wireless bridge
    SetupMetaData.Tx_SerialPort_allowed_mask |= 0b00100; // add serial2
    SetupMetaData.Tx_SerialPort2_allowed_mask |= 0b01001; // add none, serial2
#endif
#ifdef USE_COM
    SetupMetaData.Tx_SerialPort_allowed_mask |= 0b01000; // add com
#endif
#ifdef DEVICE_HAS_USB_CDC
    SetupMetaData.Tx_SerialPort_allowed_mask |= 0b10000; // add usb
#endif
#if !((defined STM32G4 || defined ESP32) && defined USE_SERIAL && defined USE_SERIAL2)
    SetupMetaData.Tx_SerialPort2_allowed_mask = 0; // only for devices with a fast processor and two serial ports
#endif

    // Tx SerialBaudrate2: "9600,19200,38400,57600,115200,230400"
    // display only when a second serial port is available
    SetupMetaData.Tx_SerialBaudrate2_allowed_mask = (SetupMetaData.Tx_SerialPort2_allowed_mask) ? UINT16_MAX : 0;

    // Tx Buzzer: ""off,LP,rxLQ"
#ifdef DEVICE_HAS_BUZZER
    SetupMetaData.Tx_Buzzer_allowed_mask = UINT16_MAX; // all
#else
    SetupMetaData.Tx_Buzzer_allowed_mask = 0; // not available, do not display
#endif

    // Tx Bridge WiFi Protocol : "TCP,UDP,BT,UDPSTA,BLE,ESPNOW"
#if defined DEVICE_HAS_ESP_WIFI_BRIDGE_ESP8266
    SetupMetaData.Tx_WiFiProt_allowed_mask = 0b101011; // TCP, UDP, UDPSTA, ESPNOW (no BT, no BLE)
#elif defined DEVICE_HAS_ESP_WIFI_BRIDGE_ESP32C3
    SetupMetaData.Tx_WiFiProt_allowed_mask = 0b111011; // all except BT (no classic BT on C3)
#else // ESP32
    SetupMetaData.Tx_WiFiProt_allowed_mask = 0b111111; // all protocols
#endif

    //-- Rx:

    power_optstr_from_rfpower_list(SetupMetaData.Rx_Power_optstr, rfpower_list, RFPOWER_LIST_NUM, 67);

    // Rx Diversity: "enabled,antenna1,antenna2,r:e t:a1,r:e t:a2"
#if defined DEVICE_HAS_DUAL_SX126x_SX128x
    // we cannot work out all cases here, as it depends on actual FrequencyBand selection, so we here just do what we can do
    SetupMetaData.Rx_Diversity_allowed_mask = 0b00111; // will be adjusted below
#elif defined DEVICE_HAS_DUAL_SX126x_SX126x
    SetupMetaData.Rx_Diversity_allowed_mask = 0b00001; // only enabled, not editable
#elif defined DEVICE_HAS_DIVERSITY
    SetupMetaData.Rx_Diversity_allowed_mask = 0b11111; // all
#elif defined DEVICE_HAS_DIVERSITY_SINGLE_SPI
    SetupMetaData.Rx_Diversity_allowed_mask = 0b11011; // TODO: no antenna2 for the moment
#else
    SetupMetaData.Rx_Diversity_allowed_mask = 0b00010; // antenna1, not editable
#endif

    // Rx SerialPort: "serial,can,canfd"
    SetupMetaData.Rx_SerialPort_allowed_mask = 0; // not available, do not display
#ifdef USE_SERIAL
    SetupMetaData.Rx_SerialPort_allowed_mask |= 0b001; // add serial
#endif
#ifdef DEVICE_HAS_DRONECAN
    SetupMetaData.Rx_SerialPort_allowed_mask |= 0b010; // add can
#elif defined DEVICE_HAS_DRONECAN_FD
    SetupMetaData.Rx_SerialPort_allowed_mask |= 0b110; // add can, canfd
#endif
#ifdef DEVICE_HAS_USB_CDC
    SetupMetaData.Rx_SerialPort_allowed_mask |= 0b1000; // add usb
#endif

    // Rx SerialOut2: "off,tx on rx pin,serial 1": any pin is a UART TX on the ESP32 family, a pad where the layout has one
    SetupMetaData.Rx_SerialOut2_allowed_mask = 0b001;
#if defined ESP32 && defined USE_SERIAL
    SetupMetaData.Rx_SerialOut2_allowed_mask |= 0b010;
#endif
#ifdef USE_SERIAL_OUT2
    SetupMetaData.Rx_SerialOut2_allowed_mask |= 0b100;
#endif
    if (SetupMetaData.Rx_SerialOut2_allowed_mask == 0b001) SetupMetaData.Rx_SerialOut2_allowed_mask = 0; // nothing to choose

    //-- Tx: Receiver setup meta data

    SetupMetaData.rx_available = false;

    SetupMetaData.rx_firmware_version = 0;
    SetupMetaData.rx_setup_layout = 0;
    SetupMetaData.rx_FrequencyBand_allowed_mask = 0;
    SetupMetaData.rx_Mode_allowed_mask = 0;
    strcpy(SetupMetaData.rx_device_name, "");
    SetupMetaData.rx_actual_power_dbm = INT8_MAX;
    SetupMetaData.rx_actual_diversity = DIVERSITY_NUM;
}


//-------------------------------------------------------
// Setup
//-------------------------------------------------------


void inc_bindphrase_char(char* const s, uint8_t pos)
{
    char* cptr = strchr(bindphrase_chars, s[pos]);
    uint8_t n = (cptr) ? cptr - bindphrase_chars + 1 : 0; // must not happen that c is not found, but play it safe
    if (n >= strlen(bindphrase_chars)) n = 0;
    s[pos] = bindphrase_chars[n];
}


// make the default bind phrases all different for all config id's
// this provides a sort of model match per default
void setup_default_bindphrase(char* const s, uint8_t config_id, const char* bindphrase_default)
{
    strcpy(s, bindphrase_default);
    if (config_id == 0) return;

#ifdef FREQUENCY_BAND_2P4_GHZ // a bit more to do because of the "except" option
    switch (config_id) {
        case 1: s[5] = '5'; break;
        case 2: s[5] = 'a'; break;
        case 3: s[5] = 'f'; break;
        case 4: s[5] = 'k'; break;
        case 5: s[5] = 'p'; break;
        case 6: s[5] = 'u'; break;
        case 7: s[5] = 'z'; break;
        case 8: s[5] = '1'; break;
        case 9: s[5] = '2'; break;
    }
    if (s[5] == bindphrase_default[5]) inc_bindphrase_char(s, 5);
#else
    for (uint8_t cnt = 0; cnt < config_id; cnt++) { // inc s[5] config_id many times
        inc_bindphrase_char(s, 5);
    }
#endif
}


void setup_default(uint8_t config_id)
{
    char bind_phrase[6+1];
    setup_default_bindphrase(bind_phrase, config_id, BIND_PHRASE);
    strcpy(Setup.Common[config_id].BindPhrase, bind_phrase);

    Setup.Common[config_id].FrequencyBand = SETUP_RF_BAND;
    Setup.Common[config_id].Mode = SETUP_MODE;
    Setup.Common[config_id].Ortho = SETUP_RF_ORTHO;
    Setup.Common[config_id].Privacy = SETUP_PRIVACY;
    Setup.Common[config_id].Region = SETUP_REGION;
    Setup.Common[config_id].DynPower = SETUP_DYN_POWER;
    Setup.Common[config_id].LinkSplit = LINK_SPLIT_BALANCED;
    Setup.Common[config_id].RadioScheme = RADIO_SCHEME_AUTO;
    Setup.Common[config_id].CustomFreqKHz = 869525; // the EU 500 mW channel, as the LR mode
    Setup.Common[config_id].CustomSpanKHz = 250;
    Setup.Common[config_id].CustomLbt = 1;
    Setup.Common[config_id].CustomPowerMaxDbm = 27;
    Setup.Common[config_id].PeerRadioCount = 1;
    Setup.Common[config_id].PeerRadioBands = 0;
    Setup.Common[config_id].RxCount = 0; // one receiver
    Setup.Common[config_id].Band2 = 0; // off

    Setup.Tx[config_id].Power = SETUP_TX_POWER;
    Setup.Tx[config_id].Diversity = SETUP_TX_DIVERSITY;
    Setup.Tx[config_id].SerialPort = SETUP_TX_SERIAL_PORT;
    Setup.Tx[config_id].SerialPort2 = SETUP_TX_SERIAL_PORT2;
    Setup.Tx[config_id].SerialBaudrate = SETUP_TX_SERIAL_BAUDRATE;
    Setup.Tx[config_id].SerialBaudrate2 = SERIAL_BAUDRATE_115200;
    Setup.Tx[config_id].SendRadioStatus = SETUP_TX_SEND_RADIO_STATUS;
    Setup.Tx[config_id].Buzzer = SETUP_TX_BUZZER;
    Setup.Tx[config_id].MavlinkComponent = SETUP_TX_MAV_COMPONENT;
    Setup.Tx[config_id].Downlink = TX_DOWNLINK_ON;

    Setup.Tx[config_id].WifiProtocol = WIFI_PROTOCOL_UDP;
    Setup.Tx[config_id].WifiChannel = WIFI_CHANNEL_6;
    Setup.Tx[config_id].WifiPower = WIFI_POWER_MED;

    Setup.Rx.Power = SETUP_RX_POWER;
    Setup.Rx.Diversity = SETUP_RX_DIVERSITY;
    Setup.Rx.SerialPort = SETUP_RX_SERIAL_PORT;
    Setup.Rx.SerialBaudrate = SETUP_RX_SERIAL_BAUDRATE;
    Setup.Rx.SerialLinkMode = SETUP_RX_SERIAL_LINK_MODE;
    Setup.Rx.SendRadioStatus = SETUP_RX_SEND_RADIO_STATUS;
    Setup.Rx.MavCoalesce = 0;
    Setup.Rx.Downlink = RX_DOWNLINK_AS_TX;
    Setup.Rx.SerialOut2 = RX_SERIAL_OUT2_OFF;
    Setup.Rx.SerialBaudrate2 = SERIAL_BAUDRATE_115200;
    Setup.Rx.Test = RX_TEST_OFF;
    Setup.Rx.Slot = 0;
}


// the modes the hardware can run on a band, in a region
uint16_t mode_mask_for_band(uint8_t band, uint8_t region)
{
    uint16_t mask = mode_hw_mask;

#if defined DEVICE_HAS_DUAL_SX126x_SX128x
    switch (band) {
    case SETUP_FREQUENCY_BAND_2P4_GHZ: mask &= 0b001111; break; // 50 Hz, 31 Hz, 19 Hz, FLRC
    case SETUP_FREQUENCY_BAND_915_MHZ_FCC:
    case SETUP_FREQUENCY_BAND_868_MHZ: mask &= 0b1100010110; break; // 31 Hz, 19 Hz, FSK, 17 Hz, 8 Hz
    case SETUP_FREQUENCY_DUAL_BAND_915_MHZ_2P4_GHZ:
    case SETUP_FREQUENCY_DUAL_BAND_868_MHZ_2P4_GHZ: mask &= 0b010110; break; // 31 Hz, 19 Hz, FSK
    }
#elif defined DEVICE_HAS_LR11xx
    mask &= (band == SETUP_FREQUENCY_BAND_2P4_GHZ) ? 0b000111 : 0b1100110110; // 50/31/19 Hz; 31/19 Hz, FSK, 19 Hz 7x, 17/8 Hz
#elif defined DEVICE_HAS_LR20xx
    mask &= (band == SETUP_FREQUENCY_BAND_2P4_GHZ) ? 0b001111 : 0b1100110110; // 50/31/19 Hz, FLRC; 31/19 Hz, FSK, 19 Hz 7x, 17/8 Hz
#endif

    // 18 Hz and LR are 868 MHz channel plans; in the EU the 868 MHz on-time limits leave only these and FSK
    if (band == SETUP_FREQUENCY_BAND_868_MHZ) {
#if defined DEVICE_HAS_LR11xx
        mask |= (1 << MODE_18HZ) | (1 << MODE_LR);
#elif defined DEVICE_HAS_SX126x && !defined DEVICE_HAS_DUAL_SX126x_SX126x
        mask |= (1 << MODE_18HZ);
#endif
        if (region == REGION_EU) mask &= (1 << MODE_FSK_50HZ) | (1 << MODE_18HZ) | (1 << MODE_LR);
    }
    return mask;
}


void _setup_sanitize_config(uint8_t config_id, bool only_rx)
{
// note: if allowed_mask = 0, this also triggers
// we may have to distinguish between not editable and not displayed, but currently
// not displayed applies only to settings for which the default is ok

#define TST_NOTALLOWED_TYPED(amask,pfield,val,ptype) \
    if ((SetupMetaData.amask & (1 << Setup.pfield)) == 0) { Setup.pfield = (ptype)val; } \
    if (SetupMetaData.amask != 0 && (SetupMetaData.amask & (1 << Setup.pfield)) == 0) { \
      for (uint8_t i = 0; i < 16; i++) { if (SetupMetaData.amask & (1 << i)) { Setup.pfield = (ptype)i; break; } } \
    }

#define TST_NOTALLOWED(amask,pfield,val) \
    if ((SetupMetaData.amask & (1 << Setup.pfield)) == 0) { Setup.pfield = val; } \
    if (SetupMetaData.amask != 0 && (SetupMetaData.amask & (1 << Setup.pfield)) == 0) { \
      for (uint8_t i = 0; i < 16; i++) { if (SetupMetaData.amask & (1 << i)) { Setup.pfield = i; break; } } \
    }

#define SANITIZE(pfield,max,setupval,defaultval) \
    if (Setup.pfield >= max) { Setup.pfield = setupval; } \
    if (Setup.pfield >= max) { Setup.pfield = defaultval; }

    //-- BindPhrase, FrequencyBand, Mode, Ortho

    char bind_phrase[6+1];
    setup_default_bindphrase(bind_phrase, config_id, BIND_PHRASE);
    sanitize_bindphrase(Setup.Common[config_id].BindPhrase, bind_phrase);

    // the region limits the bands, so it comes first
    SANITIZE(Common[config_id].Region, REGION_NUM, SETUP_REGION, REGION_FREE);
    SANITIZE(Common[config_id].DynPower, 2, SETUP_DYN_POWER, 1);
    SANITIZE(Common[config_id].LinkSplit, LINK_SPLIT_NUM, LINK_SPLIT_BALANCED, LINK_SPLIT_BALANCED);
    SANITIZE(Common[config_id].RadioScheme, RADIO_SCHEME_NUM, RADIO_SCHEME_AUTO, RADIO_SCHEME_AUTO);
    TST_NOTALLOWED(RadioScheme_allowed_mask, Common[config_id].RadioScheme, RADIO_SCHEME_AUTO);
    if (Setup.Common[config_id].PeerRadioCount < 1 || Setup.Common[config_id].PeerRadioCount > 2) Setup.Common[config_id].PeerRadioCount = 1;
    SANITIZE(Common[config_id].RxCount, RX_SLOT_NUM, 0, 0);
    SANITIZE(Common[config_id].CustomLbt, 2, 1, 1);
    if (Setup.Common[config_id].CustomFreqKHz < 100000 || Setup.Common[config_id].CustomFreqKHz > 2500000) Setup.Common[config_id].CustomFreqKHz = 869525;
    if (Setup.Common[config_id].CustomSpanKHz < 25) Setup.Common[config_id].CustomSpanKHz = 250;
    if (Setup.Common[config_id].CustomPowerMaxDbm > 33) Setup.Common[config_id].CustomPowerMaxDbm = 27;
    TST_NOTALLOWED(Region_allowed_mask, Common[config_id].Region, REGION_FREE);
    if (Setup.Common[config_id].Region == REGION_CUSTOM && Setup.Common[config_id].FrequencyBand >= SETUP_FREQUENCY_DUAL_BAND_915_MHZ_2P4_GHZ) {
        Setup.Common[config_id].Region = REGION_FREE; // one plan, one band
    }
    if (Setup.Common[config_id].Region == REGION_EU) {
        SetupMetaData.FrequencyBand_allowed_mask &= (1 << SETUP_FREQUENCY_BAND_2P4_GHZ) | (1 << SETUP_FREQUENCY_BAND_868_MHZ);
    }

    if (Setup.Common[config_id].FrequencyBand >= SETUP_FREQUENCY_BAND_NUM) {
        Setup.Common[config_id].FrequencyBand = FREQUENCY_BAND_DEFAULT;
    }
    TST_NOTALLOWED_TYPED(FrequencyBand_allowed_mask, Common[config_id].FrequencyBand, FREQUENCY_BAND_DEFAULT, SETUP_FREQUENCY_BAND_ENUM);

    // the modes of the band, and the antennas a dual band board has on it
    SetupMetaData.Mode_allowed_mask = mode_mask_for_band(Setup.Common[config_id].FrequencyBand, Setup.Common[config_id].Region);
#if defined DEVICE_HAS_DUAL_SX126x_SX128x
    switch (Setup.Common[config_id].FrequencyBand) {
    case SETUP_FREQUENCY_BAND_2P4_GHZ:
        SetupMetaData.Tx_Diversity_allowed_mask = 0b00100; // antenna2
        SetupMetaData.Rx_Diversity_allowed_mask = 0b00100; // antenna2
        break;
    case SETUP_FREQUENCY_BAND_915_MHZ_FCC:
    case SETUP_FREQUENCY_BAND_868_MHZ:
        SetupMetaData.Tx_Diversity_allowed_mask = 0b00010; // antenna1
        SetupMetaData.Rx_Diversity_allowed_mask = 0b00010; // antenna1
        break;
    case SETUP_FREQUENCY_DUAL_BAND_915_MHZ_2P4_GHZ:
    case SETUP_FREQUENCY_DUAL_BAND_868_MHZ_2P4_GHZ:
        SetupMetaData.Tx_Diversity_allowed_mask = 0b00001; // diversity / both antenna
        SetupMetaData.Rx_Diversity_allowed_mask = 0b00001; // diversity / both antenna
        break;
    default:
        while(1){} // must not happen, should have been resolved in setup_sanitize()
    }
#endif

    // auto picks among the modes above: it needs two to choose from, and one receiver (the change restarts the link)
    {
    uint16_t modes = SetupMetaData.Mode_allowed_mask & ~(1 << MODE_AUTO);
    uint8_t n = 0;
    for (uint8_t i = 0; i < MODE_AUTO; i++) if (modes & (1 << i)) n++;
    if (n >= 2 && Setup.Common[config_id].RxCount == 0) {
        SetupMetaData.Mode_allowed_mask |= (1 << MODE_AUTO);
    } else {
        SetupMetaData.Mode_allowed_mask &= ~(1 << MODE_AUTO);
    }
    }

    SANITIZE(Common[config_id].Mode, MODE_NUM, SETUP_MODE, MODE_DEFAULT);
    // a Mode = auto that cannot run (several receivers, from a bind or an image) becomes the mode auto starts from,
    // which both ends pick alike, and not this chip's default
    if (Setup.Common[config_id].Mode == MODE_AUTO && !(SetupMetaData.Mode_allowed_mask & (1 << MODE_AUTO))) {
        uint16_t modes = mode_mask_for_band(Setup.Common[config_id].FrequencyBand, Setup.Common[config_id].Region) & ~(1 << MODE_AUTO);
        Setup.Common[config_id].Mode = mode_auto_base(modes);
    }
    TST_NOTALLOWED(Mode_allowed_mask, Common[config_id].Mode, MODE_DEFAULT);

    SANITIZE(Common[config_id].Ortho, ORTHO_NUM, SETUP_RF_ORTHO, ORTHO_NONE);
    // allowed Ortho depends on FrequencyBand, so we adjust here!
    switch (Setup.Common[config_id].FrequencyBand) {
    case SETUP_FREQUENCY_BAND_2P4_GHZ: // restrict ortho to 2.4GHz, 915FCC, 70CM
    case SETUP_FREQUENCY_BAND_915_MHZ_FCC:
    case SETUP_FREQUENCY_BAND_70_CM_HAM:
//TODO    case SETUP_FREQUENCY_DUAL_BAND_915_MHZ_2P4_GHZ: // for the moment don't be concerned about ortho
//TODO    case SETUP_FREQUENCY_DUAL_BAND_868_MHZ_2P4_GHZ:
        break;
    default:
        SetupMetaData.Ortho_allowed_mask = 0; // not available, do not display
    }
    TST_NOTALLOWED(Ortho_allowed_mask, Common[config_id].Ortho, ORTHO_NONE);

    SANITIZE(Common[config_id].Privacy, PRIVACY_NUM, SETUP_PRIVACY, PRIVACY_NONE);
    TST_NOTALLOWED(Privacy_allowed_mask, Common[config_id].Privacy, PRIVACY_NONE);

    // RF Band 2: Mode = auto may move the link to a second band, on radios that run both, with one receiver, a plan
    // of the region's own and no ortho (the plans and the ortho split are per band); only a band above RF Band, so the
    // link starts and meets again on RF Band, which a receiver that cannot run RF Band 2 is on too
    SetupMetaData.Band2_allowed_mask = 0;
#if defined DEVICE_HAS_LR11xx || defined DEVICE_HAS_LR20xx
    SetupMetaData.Band2_allowed_mask = 0x01; // off
    if (Setup.Common[config_id].FrequencyBand < SETUP_FREQUENCY_DUAL_BAND_915_MHZ_2P4_GHZ &&
        Setup.Common[config_id].Mode == MODE_AUTO && Setup.Common[config_id].RxCount == 0 &&
        Setup.Common[config_id].Region != REGION_CUSTOM && Setup.Common[config_id].Ortho == ORTHO_NONE) {
        for (uint8_t b = 0; b < SETUP_FREQUENCY_DUAL_BAND_915_MHZ_2P4_GHZ; b++) {
            if (band_mhz(b) <= band_mhz(Setup.Common[config_id].FrequencyBand)) continue;
            if (SetupMetaData.FrequencyBand_allowed_mask & (1 << b)) SetupMetaData.Band2_allowed_mask |= (1 << (b + 1));
        }
    }
#endif
    SANITIZE(Common[config_id].Band2, SETUP_FREQUENCY_DUAL_BAND_915_MHZ_2P4_GHZ + 1, 0, 0);
    TST_NOTALLOWED(Band2_allowed_mask, Common[config_id].Band2, 0);

    //-- Tx:
if (!only_rx) {

    SANITIZE(Tx[config_id].Power, RFPOWER_LIST_NUM, SETUP_TX_POWER, RFPOWER_LIST_NUM - 1);

    SANITIZE(Tx[config_id].Diversity, DIVERSITY_NUM, SETUP_TX_DIVERSITY, DIVERSITY_DEFAULT);
    TST_NOTALLOWED(Tx_Diversity_allowed_mask, Tx[config_id].Diversity, DIVERSITY_ANTENNA1);

    SANITIZE(Tx[config_id].SerialPort, TX_SERIAL_PORT_NUM, SETUP_TX_SERIAL_PORT, TX_SERIAL_PORT_SERIAL);
    TST_NOTALLOWED(Tx_SerialPort_allowed_mask, Tx[config_id].SerialPort, TX_SERIAL_PORT_SERIAL);

    SANITIZE(Tx[config_id].SerialPort2, TX_SERIAL_PORT2_NUM, SETUP_TX_SERIAL_PORT2, TX_SERIAL_PORT2_SERIAL);
    TST_NOTALLOWED(Tx_SerialPort2_allowed_mask, Tx[config_id].SerialPort2, TX_SERIAL_PORT2_NONE);

    SANITIZE(Tx[config_id].SerialBaudrate, SERIAL_BAUDRATE_NUM, SETUP_TX_SERIAL_BAUDRATE, SERIAL_BAUDRATE_115200);
    SANITIZE(Tx[config_id].SerialBaudrate2, SERIAL_BAUDRATE_NUM, SERIAL_BAUDRATE_115200, SERIAL_BAUDRATE_115200);

    SANITIZE(Tx[config_id].SendRadioStatus, TX_SEND_RADIO_STATUS_NUM, SETUP_TX_SEND_RADIO_STATUS, TX_SEND_RADIO_STATUS_OFF);
    SANITIZE(Tx[config_id].MavlinkComponent, TX_MAVLINK_COMPONENT_NUM, SETUP_TX_MAV_COMPONENT, TX_MAVLINK_COMPONENT_OFF);

    SANITIZE(Tx[config_id].Buzzer, BUZZER_NUM, SETUP_TX_BUZZER, BUZZER_OFF);
    TST_NOTALLOWED(Tx_Buzzer_allowed_mask, Tx[config_id].Buzzer, BUZZER_OFF);
    SANITIZE(Tx[config_id].Downlink, TX_DOWNLINK_NUM, TX_DOWNLINK_ON, TX_DOWNLINK_ON); // a former spare byte, 0xFF

    // device cannot use same serial for both SerialPort and SerialPort2 !
    // also exclude "com" & "serial"
    if ( ((Setup.Tx[config_id].SerialPort == TX_SERIAL_PORT_SERIAL) &&
          (Setup.Tx[config_id].SerialPort2 == TX_SERIAL_PORT2_SERIAL)) ||
         ((Setup.Tx[config_id].SerialPort == TX_SERIAL_PORT_WIRELESS_BRIDGE) &&
          (Setup.Tx[config_id].SerialPort2 == TX_SERIAL_PORT2_WIRELESS_BRIDGE)) ||
         ((Setup.Tx[config_id].SerialPort == TX_SERIAL_PORT_SERIAL2) &&
          (Setup.Tx[config_id].SerialPort2 == TX_SERIAL_PORT2_SERIAL2)) ||
         ((Setup.Tx[config_id].SerialPort == TX_SERIAL_PORT_COM) &&
          (Setup.Tx[config_id].SerialPort2 == TX_SERIAL_PORT2_SERIAL)) ){
        Setup.Tx[config_id].SerialPort2 = TX_SERIAL_PORT2_NONE;
    }

#ifdef USE_ESP_WIFI_BRIDGE_CONFIGURE
    SANITIZE(Tx[config_id].WifiProtocol, WIFI_PROTOCOL_NUM, WIFI_PROTOCOL_UDP, WIFI_PROTOCOL_UDP);
    TST_NOTALLOWED(Tx_WiFiProt_allowed_mask, Tx[config_id].WifiProtocol, WIFI_PROTOCOL_UDP);
    SANITIZE(Tx[config_id].WifiChannel, WIFI_CHANNEL_NUM, WIFI_CHANNEL_6, WIFI_CHANNEL_6);
    SANITIZE(Tx[config_id].WifiPower, WIFI_POWER_NUM, WIFI_POWER_MED, WIFI_POWER_MED);
#else
    Setup.Tx[config_id].WifiProtocol = WIFI_PROTOCOL_UDP; // force them to default
    Setup.Tx[config_id].WifiChannel = WIFI_CHANNEL_6;
    Setup.Tx[config_id].WifiPower = WIFI_POWER_MED;
#endif
}

    //-- Rx:

    SANITIZE(Rx.Power, RFPOWER_LIST_NUM, SETUP_RX_POWER, RFPOWER_LIST_NUM - 1);

    SANITIZE(Rx.Diversity, DIVERSITY_NUM, SETUP_RX_DIVERSITY, DIVERSITY_DEFAULT);
    TST_NOTALLOWED(Rx_Diversity_allowed_mask, Rx.Diversity,  DIVERSITY_ANTENNA1);

    SANITIZE(Rx.SerialPort, RX_SERIAL_PORT_NUM, SETUP_RX_SERIAL_PORT, RX_SERIAL_PORT_SERIAL);
    TST_NOTALLOWED(Rx_SerialPort_allowed_mask, Rx.SerialPort, RX_SERIAL_PORT_SERIAL);

    SANITIZE(Rx.SerialBaudrate, SERIAL_BAUDRATE_NUM, SETUP_RX_SERIAL_BAUDRATE, SERIAL_BAUDRATE_57600);

    SANITIZE(Rx.SerialLinkMode, SERIAL_LINK_MODE_NUM, SETUP_RX_SERIAL_LINK_MODE, SERIAL_LINK_MODE_TRANSPARENT);
    if (SERIAL_LINK_MODE_IS_MSP(Setup.Rx.SerialLinkMode)) Setup.Rx.SerialPort = RX_SERIAL_PORT_SERIAL; // don't do CAN when MSPX

    SANITIZE(Rx.SendRadioStatus, RX_SEND_RADIO_STATUS_NUM, SETUP_RX_SEND_RADIO_STATUS, RX_SEND_RADIO_STATUS_OFF);

    SANITIZE(Rx.MavlinkSystemID, 5, SETUP_RX_MAVLINK_SYSTEM_ID, 0);

    SANITIZE(Rx.MavCoalesce, 2, 0, 0);

    SANITIZE(Rx.Downlink, RX_DOWNLINK_NUM, RX_DOWNLINK_AS_TX, RX_DOWNLINK_AS_TX);

    SANITIZE(Rx.SerialOut2, RX_SERIAL_OUT2_NUM, RX_SERIAL_OUT2_OFF, RX_SERIAL_OUT2_OFF);
    TST_NOTALLOWED(Rx_SerialOut2_allowed_mask, Rx.SerialOut2, RX_SERIAL_OUT2_OFF);
    // on the RX pin only with a transparent link: MAVLink and MSP need to hear the autopilot
    if (Setup.Rx.SerialOut2 == RX_SERIAL_OUT2_TX_ON_RX_PIN && Setup.Rx.SerialLinkMode != SERIAL_LINK_MODE_TRANSPARENT) {
        Setup.Rx.SerialOut2 = RX_SERIAL_OUT2_OFF;
    }
    SANITIZE(Rx.SerialBaudrate2, SERIAL_BAUDRATE_NUM, SERIAL_BAUDRATE_115200, SERIAL_BAUDRATE_115200);

    SANITIZE(Rx.Test, RX_TEST_NUM, RX_TEST_OFF, RX_TEST_OFF);
    SANITIZE(Rx.Slot, RX_SLOT_NUM, 0, 0);


    //-- Spares and deprecated options:
    // should be 0xFF'ed

    for (uint8_t n = 0; n < sizeof(Setup.spare)/sizeof(Setup.spare[0]); n++) Setup.spare[n] = 0xFF;
    for (uint8_t n = 0; n < sizeof(Setup.spare1)/sizeof(Setup.spare1[0]); n++) Setup.spare1[n] = 0xFF;
    for (uint8_t n = 0; n < sizeof(Setup.Common[config_id].spare)/sizeof(Setup.Common[config_id].spare[0]); n++) Setup.Common[config_id].spare[n] = 0xFF;
    for (uint8_t n = 0; n < sizeof(Setup.Tx[config_id].spare)/sizeof(Setup.Tx[config_id].spare[0]); n++) Setup.Tx[config_id].spare[n] = 0xFF;
    for (uint8_t n = 0; n < sizeof(Setup.Rx.spare)/sizeof(Setup.Rx.spare[0]); n++) Setup.Rx.spare[n] = 0xFF;
}


void setup_sanitize_config(uint8_t config_id) { _setup_sanitize_config(config_id, false); }
void setup_sanitize_rx_config(void) { _setup_sanitize_config(0, true); }


//-------------------------------------------------------
// Configure
//-------------------------------------------------------

// also called by bind
void configure_mode(uint8_t mode, uint8_t frequencyband)
{
    Config.Mode = mode;

    switch (Config.Mode) {
    case MODE_50HZ:
        Config.frame_rate_ms = 20; // 20 ms = 50 Hz
        Config.frame_rate_hz = 50;
        Config.send_frame_tmo_ms = MODE_50HZ_SEND_FRAME_TMO_MS; // 10;
        break;
    case MODE_31HZ:
        Config.frame_rate_ms = 32; // 32 ms = 31.25 Hz
        Config.frame_rate_hz = 31;
        Config.send_frame_tmo_ms = MODE_31HZ_SEND_FRAME_TMO_MS; // 15
        break;
    case MODE_19HZ:
    case MODE_19HZ_7X:
        Config.frame_rate_ms = 53; // 53 ms = 18.9 Hz
        Config.frame_rate_hz = 19;
        Config.send_frame_tmo_ms = MODE_19HZ_SEND_FRAME_TMO_MS; // 25;
        break;
    case MODE_FLRC_111HZ:
        Config.frame_rate_ms = 9; // 9 ms = 111 Hz
        Config.frame_rate_hz = 111,
        Config.send_frame_tmo_ms = MODE_FLRC_111HZ_SEND_FRAME_TMO_MS; // 7;
        break;
    case MODE_FSK_50HZ:
        Config.frame_rate_ms = 20; // 20 ms = 50 Hz
        Config.frame_rate_hz = 50;
        Config.send_frame_tmo_ms = MODE_FSK_50HZ_SEND_FRAME_TMO_MS; // 10;
        break;
    case MODE_18HZ:
        Config.frame_rate_ms = 55; // 55 ms = 18.2 Hz
        Config.frame_rate_hz = 18;
        Config.send_frame_tmo_ms = MODE_18HZ_SEND_FRAME_TMO_MS; // 15
        break;
    case MODE_LR:
        Config.frame_rate_ms = 560; // 560 ms = 1.8 Hz, 52.8 ms in air each side is 9.4 %
        Config.frame_rate_hz = 2;
        Config.send_frame_tmo_ms = MODE_LR_SEND_FRAME_TMO_MS; // 60
        break;
    case MODE_17HZ:
        Config.frame_rate_ms = 60; // 60 ms = 16.7 Hz, 26.4 ms in air each side
        Config.frame_rate_hz = 17;
        Config.send_frame_tmo_ms = MODE_17HZ_SEND_FRAME_TMO_MS; // 30
        break;
    case MODE_8HZ:
        Config.frame_rate_ms = 120; // 120 ms = 8.3 Hz, 52.8 ms in air each side
        Config.frame_rate_hz = 8;
        Config.send_frame_tmo_ms = MODE_8HZ_SEND_FRAME_TMO_MS; // 60
        break;
    default:
        while(1){} // must not happen, should have been resolved in setup_sanitize_config()
    }

    // Sx/Sx2 LoraConfigIndex, is_lora helper for sx drivers
    Config.Sx.is_lora = Config.Sx2.is_lora = true;

    switch (Config.Mode) {
    case MODE_50HZ:
#if defined DEVICE_HAS_DUAL_SX126x_SX128x
        // has hopefully been correctly sanitized in setup_sanitize_config(), only for 2.4 GHz frequency band
        Config.Sx.LoraConfigIndex = 0; // 50Hz Lora not possible on sx126x, has no effect
        Config.Sx2.LoraConfigIndex = SX128x_LORA_CONFIG_BW800_SF5_CRLI4_5;
#elif defined DEVICE_HAS_DUAL_SX126x_SX126x
        while(1){} // not possible
#elif defined DEVICE_HAS_SX128x
        Config.Sx.LoraConfigIndex = SX128x_LORA_CONFIG_BW800_SF5_CRLI4_5;
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
#elif defined DEVICE_HAS_SX126x
        while(1){} // not possible
#elif defined DEVICE_HAS_SX127x
        while(1){} // not possible
#elif defined DEVICE_HAS_LR11xx
        if (frequencyband == SETUP_FREQUENCY_BAND_2P4_GHZ) {
            Config.Sx.LoraConfigIndex = LR11xx_LORA_CONFIG_BW800_SF5_CR4_5;
        } else {
            while(1){} // not possible
        }
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
#elif defined DEVICE_HAS_LR20xx
        if (frequencyband == SETUP_FREQUENCY_BAND_2P4_GHZ) {
            Config.Sx.LoraConfigIndex = LR20xx_LORA_CONFIG_BW800_SF5_CR4_5;
        } else {
            while(1){} // not possible
        }
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
#else
  #error Unknown Device !
#endif
        break;

    case MODE_31HZ:
#if defined DEVICE_HAS_DUAL_SX126x_SX128x
        Config.Sx.LoraConfigIndex = SX126x_LORA_CONFIG_BW500_SF5_CR4_5;
        Config.Sx2.LoraConfigIndex = SX128x_LORA_CONFIG_BW800_SF6_CRLI4_5;
#elif defined DEVICE_HAS_DUAL_SX126x_SX126x
        Config.Sx.LoraConfigIndex = SX126x_LORA_CONFIG_BW500_SF5_CR4_5;
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
#elif defined DEVICE_HAS_SX128x
        Config.Sx.LoraConfigIndex = SX128x_LORA_CONFIG_BW800_SF6_CRLI4_5;
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
#elif defined DEVICE_HAS_SX126x
        Config.Sx.LoraConfigIndex = SX126x_LORA_CONFIG_BW500_SF5_CR4_5;
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
#elif defined DEVICE_HAS_SX127x
        while(1){} // not possible
#elif defined DEVICE_HAS_LR11xx
        if (frequencyband == SETUP_FREQUENCY_BAND_2P4_GHZ) {
            Config.Sx.LoraConfigIndex = LR11xx_LORA_CONFIG_BW800_SF6_CR4_5;
        } else {
            Config.Sx.LoraConfigIndex = LR11xx_LORA_CONFIG_BW500_SF5_CR4_5;
        }
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
#elif defined DEVICE_HAS_LR20xx
        if (frequencyband == SETUP_FREQUENCY_BAND_2P4_GHZ) {
            Config.Sx.LoraConfigIndex = LR20xx_LORA_CONFIG_BW800_SF6_CR4_5;
        } else {
            Config.Sx.LoraConfigIndex = LR20xx_LORA_CONFIG_BW500_SF5_CR4_5;
        }
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
#else
  #error Unknown Device !
#endif
        break;

    case MODE_19HZ:
    case MODE_19HZ_7X:
#if defined DEVICE_HAS_DUAL_SX126x_SX128x
        Config.Sx.LoraConfigIndex = SX126x_LORA_CONFIG_BW500_SF6_CR4_5;
        Config.Sx2.LoraConfigIndex = SX128x_LORA_CONFIG_BW800_SF7_CRLI4_5;
#elif defined DEVICE_HAS_DUAL_SX126x_SX126x
        Config.Sx.LoraConfigIndex = SX126x_LORA_CONFIG_BW500_SF6_CR4_5;
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
#elif defined DEVICE_HAS_SX128x
        Config.Sx.LoraConfigIndex = SX128x_LORA_CONFIG_BW800_SF7_CRLI4_5;
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
#elif defined DEVICE_HAS_SX126x
        Config.Sx.LoraConfigIndex = SX126x_LORA_CONFIG_BW500_SF6_CR4_5;
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
#elif defined DEVICE_HAS_SX127x
        Config.Sx.LoraConfigIndex = SX127x_LORA_CONFIG_BW500_SF6_CR4_5;
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
#elif defined DEVICE_HAS_LR11xx
        if (frequencyband == SETUP_FREQUENCY_BAND_2P4_GHZ) {
            Config.Sx.LoraConfigIndex = LR11xx_LORA_CONFIG_BW800_SF7_CR4_5;
        } else {
            Config.Sx.LoraConfigIndex = LR11xx_LORA_CONFIG_BW500_SF6_CR4_5;
        }
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
#elif defined DEVICE_HAS_LR20xx
        if (frequencyband == SETUP_FREQUENCY_BAND_2P4_GHZ) {
            Config.Sx.LoraConfigIndex = LR20xx_LORA_CONFIG_BW800_SF7_CR4_5;
        } else {
            Config.Sx.LoraConfigIndex = LR20xx_LORA_CONFIG_BW500_SF6_CR4_5;
        }
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
#else
  #error Unknown Device !
#endif
        break;

    case MODE_FLRC_111HZ:
        Config.Sx.LoraConfigIndex = 0;
        Config.Sx2.LoraConfigIndex = 0;
        Config.Sx.is_lora = Config.Sx2.is_lora = false;
        break;

    case MODE_18HZ: // has hopefully been correctly sanitized in setup_sanitize_config(), only 868 MHz
#if defined DEVICE_HAS_DUAL_SX126x_SX128x || defined DEVICE_HAS_DUAL_SX126x_SX126x
        while(1){} // not possible
#elif defined DEVICE_HAS_SX126x
        Config.Sx.LoraConfigIndex = SX126x_LORA_CONFIG_BW500_SF5_CR4_5;
#elif defined DEVICE_HAS_LR11xx
        Config.Sx.LoraConfigIndex = LR11xx_LORA_CONFIG_BW500_SF5_CR4_5;
#else
        while(1){} // not possible
#endif
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
        break;

    case MODE_LR: // has hopefully been correctly sanitized in setup_sanitize_config(), only 868 MHz
#if defined DEVICE_HAS_LR11xx
        Config.Sx.LoraConfigIndex = LR11xx_LORA_CONFIG_BW125_SF5_CR4_5;
#else
        while(1){} // not possible
#endif
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
        break;
    case MODE_17HZ: // sub-GHz only, the band filter in setup_sanitize_config() keeps it there
#if defined DEVICE_HAS_SX126x || defined DEVICE_HAS_DUAL_SX126x_SX128x
        Config.Sx.LoraConfigIndex = SX126x_LORA_CONFIG_BW250_SF5_CR4_5;
#elif defined DEVICE_HAS_LR11xx
        Config.Sx.LoraConfigIndex = LR11xx_LORA_CONFIG_BW250_SF5_CR4_5;
#elif defined DEVICE_HAS_LR20xx
        Config.Sx.LoraConfigIndex = LR20xx_LORA_CONFIG_BW250_SF5_CR4_5;
#else
        while(1){} // not possible
#endif
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
        break;
    case MODE_8HZ:
#if defined DEVICE_HAS_SX126x || defined DEVICE_HAS_DUAL_SX126x_SX128x
        Config.Sx.LoraConfigIndex = SX126x_LORA_CONFIG_BW125_SF5_CR4_5;
#elif defined DEVICE_HAS_LR11xx
        Config.Sx.LoraConfigIndex = LR11xx_LORA_CONFIG_BW125_SF5_CR4_5;
#elif defined DEVICE_HAS_LR20xx
        Config.Sx.LoraConfigIndex = LR20xx_LORA_CONFIG_BW125_SF5_CR4_5;
#else
        while(1){} // not possible
#endif
        Config.Sx2.LoraConfigIndex = Config.Sx.LoraConfigIndex;
        break;

    case MODE_FSK_50HZ:
#if defined DEVICE_HAS_DUAL_SX126x_SX128x
        // has hopefully been correctly sanitized in setup_sanitize_config(), not for 2.4 GHz frequency band
        Config.Sx.LoraConfigIndex = 0;
        Config.Sx.is_lora = false;
        Config.Sx2.LoraConfigIndex = SX128x_LORA_CONFIG_BW800_SF5_CRLI4_5;
        Config.Sx2.is_lora = true;
#else
        Config.Sx.LoraConfigIndex = 0;
        Config.Sx2.LoraConfigIndex = 0;
        Config.Sx.is_lora = Config.Sx2.is_lora = false;
#endif
        break;
    }

    // Sx/Sx2 FrequencyBand
    Config.Sx.FrequencyBand = cvt_to_sx_fhss_frequency_band(frequencyband); // asserts if not a valid SETUP_FREQUENCY_BAND_ENUM
    Config.Sx2.FrequencyBand = Config.Sx.FrequencyBand;

#if defined DEVICE_HAS_DUAL_SX126x_SX128x
    if (Config.Sx.FrequencyBand == SX_FHSS_FREQUENCY_BAND_2P4_GHZ) {
        Config.Sx.FrequencyBand = SX_FHSS_FREQUENCY_BAND_868_MHZ; // set a valid dummy
    }
    Config.Sx2.FrequencyBand = SX_FHSS_FREQUENCY_BAND_2P4_GHZ;
#elif defined DEVICE_HAS_DUAL_SX126x_SX126x
    Config.Sx2.FrequencyBand = SX_FHSS_FREQUENCY_BAND_433_MHZ;
#endif
}


// also called by bind
void configure_diversity(uint8_t diversity)
{
#if defined DEVICE_HAS_DUAL_SX126x_SX128x || defined DEVICE_HAS_DUAL_SX126x_SX126x || \
    defined DEVICE_HAS_DIVERSITY || defined DEVICE_HAS_DIVERSITY_SINGLE_SPI
    switch (diversity) {
    case DIVERSITY_DEFAULT:
        Config.Diversity = DIVERSITY_DEFAULT;
        Config.ReceiveUseAntenna1 = Config.TransmitUseAntenna1 = true;
        Config.ReceiveUseAntenna2 = Config.TransmitUseAntenna2 = true;
        break;
    case DIVERSITY_ANTENNA1:
        Config.Diversity = DIVERSITY_ANTENNA1;
        Config.ReceiveUseAntenna1 = Config.TransmitUseAntenna1 = true;
        Config.ReceiveUseAntenna2 = Config.TransmitUseAntenna2 = false;
        break;
    case DIVERSITY_ANTENNA2:
        Config.Diversity = DIVERSITY_ANTENNA2;
        Config.ReceiveUseAntenna1 = Config.TransmitUseAntenna1 = false;
        Config.ReceiveUseAntenna2 = Config.TransmitUseAntenna2 = true;
        break;
#if defined DEVICE_HAS_DIVERSITY || defined DEVICE_HAS_DIVERSITY_SINGLE_SPI
    // must not happen for dual band, should have been resolved in setup_sanitize_config()
    case DIVERSITY_R_ENABLED_T_ANTENNA1:
        Config.Diversity = DIVERSITY_R_ENABLED_T_ANTENNA1;
        Config.ReceiveUseAntenna1 = Config.TransmitUseAntenna1 = true;
        Config.ReceiveUseAntenna2 = true;
        Config.TransmitUseAntenna2 = false;
        break;
    case DIVERSITY_R_ENABLED_T_ANTENNA2:
        Config.Diversity = DIVERSITY_R_ENABLED_T_ANTENNA2;
        Config.ReceiveUseAntenna1 = true;
        Config.TransmitUseAntenna1 = false;
        Config.ReceiveUseAntenna2 = Config.TransmitUseAntenna2 = true;
        break;
#endif
    default:
        while(1){} // must not happen, should have been resolved in setup_sanitize()
    }
#else
    Config.Diversity = DIVERSITY_ANTENNA1;
    Config.ReceiveUseAntenna1 = Config.TransmitUseAntenna1 = true;
    Config.ReceiveUseAntenna2 = Config.TransmitUseAntenna2 = false;
#endif
}


// highest power the region allows; if the list has nothing that low, its lowest entry
uint8_t region_power_idx_max(uint8_t region, uint8_t frequency_band, uint8_t mode, int8_t custom_max_dbm)
{
    if (region == REGION_FREE) return RFPOWER_LIST_NUM - 1;

    int8_t limit_dbm = custom_max_dbm; // REGION_CUSTOM: the plan's own cap
    if (region == REGION_EU) {
    limit_dbm = 14; // 863-868.6 MHz: 25 mW e.r.p., with a dipole
    if (frequency_band == SETUP_FREQUENCY_BAND_2P4_GHZ) limit_dbm = 18; // 100 mW e.i.r.p. with a 2 dBi antenna
    if (mode == MODE_LR) limit_dbm = 27; // 869.4-869.65 MHz: 500 mW e.r.p.
    }

    uint8_t idx = 0;
    for (uint8_t i = 0; i < RFPOWER_LIST_NUM; i++) {
        if (rfpower_list[i].dbm <= limit_dbm) idx = i;
    }
    return idx;
}


uint8_t region_fhss_plan(uint8_t region, uint8_t sx_frequency_band, uint8_t mode)
{
    if (region == REGION_CUSTOM) return FHSS_PLAN_CUSTOM;
    if (sx_frequency_band != SX_FHSS_FREQUENCY_BAND_868_MHZ) return FHSS_PLAN_DEFAULT;
    if (mode == MODE_LR) return FHSS_PLAN_869_LR;
    if (region != REGION_EU) return FHSS_PLAN_DEFAULT;
    return (mode == MODE_FSK_50HZ) ? FHSS_PLAN_868_EU_FSK : FHSS_PLAN_868_EU_LORA;
}


// the channel spacing of a custom plan, and the occupied bandwidth the listening threshold is for
uint16_t mode_bandwidth_khz(uint8_t mode, bool is_2p4)
{
    switch (mode) {
    case MODE_FLRC_111HZ: return 1000;
    case MODE_FSK_50HZ: return 250;
    case MODE_LR:
    case MODE_8HZ: return 125;
    case MODE_17HZ: return 250;
    default: return (is_2p4) ? 800 : 500;
    }
}

uint8_t fhss_custom_plan_set(uint32_t centre_khz, uint16_t span_khz, uint16_t bw_khz, bool is_2p4); // fhss.h


//-- Mode = auto: the mode in use lives here, not in the setup, so a change never wears the EEPROM

uint8_t mode_auto_current = MODE_AUTO; // MODE_AUTO: not chosen yet, the base
uint8_t mode_auto_pending = MODE_AUTO; // the mode the link is about to restart with, MODE_AUTO = none



//-- RF Band 2: the band in use lives here as well

uint8_t band_auto_current = SETUP_FREQUENCY_BAND_NUM; // none chosen yet: the meeting band
uint8_t band_auto_pending = SETUP_FREQUENCY_BAND_NUM;

// where Mode = auto starts and meets again: the lower of the two bands, which reaches further, at its base mode
uint8_t band_auto_meeting(void)
{
    if (Config.Band2 >= SETUP_FREQUENCY_BAND_NUM) return Config.Band1;
    return (band_mhz(Config.Band2) < band_mhz(Config.Band1)) ? Config.Band2 : Config.Band1;
}

uint8_t mode_auto_meeting(void)
{
    return mode_auto_base(mode_mask_for_band(band_auto_meeting(), Config.Region) & ~(1 << MODE_AUTO));
}

bool mode_auto_at_meeting(void)
{
    return (Config.FrequencyBand == band_auto_meeting() && Config.Mode == mode_auto_meeting());
}

void mode_auto_to_meeting(void)
{
    band_auto_current = band_auto_meeting();
    mode_auto_current = mode_auto_meeting();
}

// without a link the meeting point and the mode the link went in take turns: ends left in different modes meet at
// the base, and a link lost at range comes back in the mode that reached there, which the base may not
uint8_t mode_auto_lost_mode = MODE_AUTO; // MODE_AUTO: linked, no turns
uint8_t mode_auto_lost_band;

// a turn's length: at least MODE_AUTO_REVERT_FRAMES frames
uint8_t mode_auto_revert_s(void)
{
    uint32_t s = ((uint32_t)MODE_AUTO_REVERT_FRAMES * Config.frame_rate_ms + 999) / 1000;
    return (s > MODE_AUTO_REVERT_S) ? s : MODE_AUTO_REVERT_S;
}

// the turn is over: true when the link restarts in the other place
bool mode_auto_lost_next(void)
{
    if (mode_auto_lost_mode >= MODE_AUTO) { mode_auto_lost_mode = Config.Mode; mode_auto_lost_band = Config.FrequencyBand; }
    if (!mode_auto_at_meeting()) {
        mode_auto_to_meeting();
        return true;
    }
    if (mode_auto_lost_mode == Config.Mode && mode_auto_lost_band == Config.FrequencyBand) return false; // went at the base
    // back only to a mode that reaches further than the base: after a fade at a fast one the base keeps the turns
    int16_t lost_dbm = mode_sensitivity_dbm(mode_auto_lost_mode) + band_offset_db(mode_auto_lost_band, Config.FrequencyBand);
    if (lost_dbm >= mode_sensitivity_dbm(Config.Mode)) return false;
    mode_auto_current = mode_auto_lost_mode;
    band_auto_current = mode_auto_lost_band;
    return true;
}

// a 91-byte frame's time in the air, the drivers' tables; where the chips of a band differ, the slower one, so both
// ends of a link compute the same reply slot
uint32_t mode_toa_us(uint8_t mode, bool is_2p4)
{
    switch (mode) {
    case MODE_50HZ: return 7892;
    case MODE_31HZ: return 13418;
    case MODE_19HZ:
    case MODE_19HZ_7X: return (is_2p4) ? 23527 : 22560;
    case MODE_FLRC_111HZ: return 2383;
    case MODE_FSK_50HZ: return 7600;
    case MODE_18HZ: return 22300;
    case MODE_17HZ: return 26400;
    }
    return 52800; // MODE_LR, MODE_8HZ
}


// the link's part of the configuration, from Config and the mode and band in use: at a controller restart, and
// again at a restart of the link alone (Mode = auto's steps, a worn transmit key), which leaves the setup in RAM as
// it is, so changes not stored yet and the sessions on the serial ports survive it
void setup_configure_link(void)
{
    //-- Diversity: two radios transmitting override it below

    configure_diversity(Config.Diversity);

    //-- FrequencyBand

    // with Mode = auto and RF Band 2 the band in use, which starts and meets again on the lower of the two
    Config.FrequencyBand = Config.Band1;
    bool auto_reset = false;
    if (Config.ModeAuto && Config.Band2 < SETUP_FREQUENCY_BAND_NUM) {
        if (band_auto_current != Config.Band1 && band_auto_current != Config.Band2) {
            band_auto_current = band_auto_meeting();
            auto_reset = true;
        }
        Config.FrequencyBand = band_auto_current;
    }

    Config.IsDualBand = (Config.FrequencyBand == SETUP_FREQUENCY_DUAL_BAND_915_MHZ_2P4_GHZ ||
                         Config.FrequencyBand == SETUP_FREQUENCY_DUAL_BAND_868_MHZ_2P4_GHZ);

    //-- Mode, Mode dependent settings

    // sets Mode and related, also sets Sx/Sx2.LoraConfigIndex, Sx/Sx2.is_lora, Sx/Sx2.FrequencyBand
    // Mode = auto: the mode in use, the base until the link picked one
    uint8_t mode = Config.Mode;
    if (Config.ModeAuto) {
        uint16_t modes = mode_mask_for_band(Config.FrequencyBand, Config.Region) & ~(1 << MODE_AUTO);
        if (auto_reset || mode_auto_current >= MODE_AUTO || !(modes & (1 << mode_auto_current))) mode_auto_current = mode_auto_base(modes);
        mode = mode_auto_current;
    }
    configure_mode(mode, Config.FrequencyBand);

    //-- Multipoint: the cycle grows by one slot for each receiver beyond the first

    Config.LinkHeader = (Config.RxCount > 1);
    {
    bool is_2p4 = Config.IsDualBand || (Config.Sx.FrequencyBand == SX_FHSS_FREQUENCY_BAND_2P4_GHZ);
    uint32_t toa_us = mode_toa_us(Config.Mode, is_2p4);
    uint32_t toa_tx_us = (toa_us * Config.FrameLenTx) / FRAME_TX_RX_LEN;
    uint32_t toa_rx_us = (toa_us * Config.FrameLenRx) / FRAME_TX_RX_LEN;
    Config.SlotPeriod_ms = (toa_rx_us + RX_SLOT_GUARD_US + 999) / 1000;
    Config.FirstReplyEnd_ms = (1000 + toa_tx_us + 1000 + toa_rx_us + 999) / 1000; // the Tx sends 0.75 ms after its tick, the Rx answers 1 ms after the frame
    if (Config.RxCount > 1) {
        Config.frame_rate_ms += (Config.RxCount - 1) * Config.SlotPeriod_ms;
        Config.frame_rate_hz = (1000 + Config.frame_rate_ms / 2) / Config.frame_rate_ms;
        if (Config.frame_rate_hz < 1) Config.frame_rate_hz = 1;
    }
    }

    //-- Sx12xx power: the region's cap for the band and mode in use

    Config.PowerIdxMax = region_power_idx_max(Config.Region, Config.FrequencyBand, Config.Mode, Config.CustomPowerMaxDbm);
    uint8_t power_idx = Config.PowerIdx;
    if (power_idx > Config.PowerIdxMax) power_idx = Config.PowerIdxMax;
    Config.Sx.Power_dbm = rfpower_list[power_idx].dbm;
    Config.Sx2.Power_dbm = Config.Sx.Power_dbm;

    //-- Fhss

//TODO: sort out for dual band
    Config.Fhss.Except = EXCEPT_NONE;
    if (Config.Sx.FrequencyBand == SX_FHSS_FREQUENCY_BAND_2P4_GHZ) {
        Config.Fhss.Except = Config.FhssExcept2p4;
    }

    Config.Fhss.FrequencyBand = cvt_to_sx_fhss_frequency_band(Config.FrequencyBand); // asserts if not a valid SETUP_FREQUENCY_BAND_ENUM

    uint8_t fhss_num_list[SX_FHSS_FREQUENCY_BAND_NUM][MODE_NUM] = {
// MODE_50HZ,         MODE_31HZ,              MODE_19HZ,                MODE_FLRC_111HZ,   MODE_FSK_50HZ,         MODE_19HZ_7X,            MODE_18HZ,         MODE_LR,         MODE_17HZ,           MODE_8HZ
// SX_FHSS_FREQUENCY_BAND_2P4_GHZ
{  FHSS_NUM_2P4_GHZ,  FHSS_NUM_2P4_GHZ_31HZ,  FHSS_NUM_2P4_GHZ_19HZ,    FHSS_NUM_2P4_GHZ,  1,                     1,                       1,                 1,               1,                   1},
// SX_FHSS_FREQUENCY_BAND_915_MHZ_FCC
{  1,                 FHSS_NUM_915_MHZ_FCC,   FHSS_NUM_915_MHZ_FCC,     1,                 FHSS_NUM_915_MHZ_FCC,  FHSS_NUM_915_MHZ_FCC,    1,                 1,               FHSS_NUM_915_MHZ_FCC, FHSS_NUM_915_MHZ_FCC},
// SX_FHSS_FREQUENCY_BAND_868_MHZ
{  1,                 FHSS_NUM_868_MHZ,       FHSS_NUM_868_MHZ,         1,                 FHSS_NUM_868_MHZ,      FHSS_NUM_868_MHZ,        FHSS_NUM_868_MHZ,  FHSS_NUM_869_LR, FHSS_NUM_868_MHZ,    FHSS_NUM_868_MHZ},
// SX_FHSS_FREQUENCY_BAND_866_MHZ_IN
{  1,                 FHSS_NUM_866_MHZ_IN,    FHSS_NUM_866_MHZ_IN,      1,                 FHSS_NUM_866_MHZ_IN,   FHSS_NUM_866_MHZ_IN,     1,                 1,               FHSS_NUM_866_MHZ_IN, FHSS_NUM_866_MHZ_IN},
// SX_FHSS_FREQUENCY_BAND_433_MHZ
{  1,                 FHSS_NUM_433_MHZ,       FHSS_NUM_433_MHZ,         1,                 FHSS_NUM_433_MHZ,      FHSS_NUM_433_MHZ,        1,                 1,               FHSS_NUM_433_MHZ,    FHSS_NUM_433_MHZ},
// SX_FHSS_FREQUENCY_BAND_70_CM_HAM
{  1,                 FHSS_NUM_70_CM_HAM,     FHSS_NUM_70_CM_HAM_19HZ,  1,                 FHSS_NUM_70_CM_HAM,    FHSS_NUM_70_CM_HAM_19HZ, 1,                 1,               FHSS_NUM_70_CM_HAM,  FHSS_NUM_70_CM_HAM}
    };
    Config.Fhss.Num = fhss_num_list[Config.Fhss.FrequencyBand][Config.Mode];

    // regional and LR channel plans replace the band's list, and use all of their channels
    Config.Fhss.Plan = region_fhss_plan(Config.Region, Config.Fhss.FrequencyBand, Config.Mode);
    switch (Config.Fhss.Plan) {
    case FHSS_PLAN_868_EU_LORA: Config.Fhss.Num = FHSS_NUM_868_EU_LORA; break;
    case FHSS_PLAN_868_EU_FSK: Config.Fhss.Num = FHSS_NUM_868_EU_FSK; break;
    case FHSS_PLAN_869_LR: Config.Fhss.Num = FHSS_NUM_869_LR; break;
    case FHSS_PLAN_CUSTOM:
        Config.Fhss.Num = fhss_custom_plan_set(Config.CustomFreqKHz, Config.CustomSpanKHz,
                                               mode_bandwidth_khz(Config.Mode, Config.Fhss.FrequencyBand == SX_FHSS_FREQUENCY_BAND_2P4_GHZ),
                                               Config.Fhss.FrequencyBand == SX_FHSS_FREQUENCY_BAND_2P4_GHZ);
        break;
    }

    // ensure equal fhss1, fhss2, is required in single band mode
    Config.Fhss.Offset = 0;
    Config.Fhss2 = Config.Fhss;

    //-- Radio Scheme: what this pair runs here

    Config.RadioScheme = RADIO_SCHEME_SINGLE;
    Config.Bonded = false;
    // in the EU on 868 MHz the modes there fill the on-time limits with one radio sending (FSK 1.8 % per channel of
    // PSA's 2.7 %, LR 9.4 % of 10 %): a second one would have frames held back every cycle, so one sends
    bool eu_868 = (Config.Region == REGION_EU && Config.FrequencyBand == SETUP_FREQUENCY_BAND_868_MHZ);
    if (Config.IsDualBand) {
        Config.RadioScheme = RADIO_SCHEME_DUAL_BAND;
    } else if (Config.RadioSchemeSet != RADIO_SCHEME_SINGLE && Config.Fhss.Num >= 2 && !eu_868) {
        // auto is bonded: twice the data while both channels are good, one frame on both when one is poor; one
        // receiver only, the lanes and the reply slots both need the link header's bits
        bool bonded = (Config.RadioSchemeSet != RADIO_SCHEME_DUAL_REDUNDANT) && (Config.RxCount == 1);
        Config.RadioScheme = (bonded) ? RADIO_SCHEME_DUAL_BONDED : RADIO_SCHEME_DUAL_REDUNDANT;
        Config.Bonded = bonded;
        Config.Fhss2.Offset = 1; // trails by half the hops in use: never the same frequency in the same slot
        Config.ReceiveUseAntenna1 = Config.ReceiveUseAntenna2 = true; // both radios, whatever Diversity says
        Config.TransmitUseAntenna1 = Config.TransmitUseAntenna2 = true;
    }
    Config.TransmitBoth = (Config.RadioScheme != RADIO_SCHEME_SINGLE);
    if (Config.Bonded) Config.LinkHeader = true;

#if defined DEVICE_HAS_DUAL_SX126x_SX128x
    if (Config.IsDualBand) { // only modify if in dual band mode, otherwise keep fhss1 = fhss2
        Config.Fhss2.FrequencyBand = SX_FHSS_FREQUENCY_BAND_2P4_GHZ;
        Config.Fhss2.Num = fhss_num_list[Config.Fhss2.FrequencyBand][Config.Mode];
    }
#elif defined DEVICE_HAS_DUAL_SX126x_SX126x
    Config.IsDualBand = true; // currently only dual band mode supported
    Config.Fhss2.FrequencyBand = SX_FHSS_FREQUENCY_BAND_433_MHZ;
    Config.Fhss2.Num = FHSS_NUM_433_MHZ;
#endif

    //-- More Config, may depend on above config settings

    uint16_t connect_tmo_ms = CONNECT_TMO_MS;
    if (connect_tmo_ms < 5 * Config.frame_rate_ms) connect_tmo_ms = 5 * Config.frame_rate_ms; // LR needs a few frames
    Config.connect_tmo_systicks = SYSTICK_DELAY_MS(connect_tmo_ms);
    if (Config.Fhss.Num >= Config.Fhss2.Num) {
        Config.connect_sync_cnt_max = Config.Fhss.Num / 2;
        Config.connect_listen_hop_cnt = (uint8_t)(1.5f * Config.Fhss.Num);
    } else {
        Config.connect_sync_cnt_max = Config.Fhss2.Num / 2;
        Config.connect_listen_hop_cnt = (uint8_t)(1.5f * Config.Fhss2.Num);
    }
    if (Config.connect_sync_cnt_max < CONNECT_SYNC_CNT) Config.connect_sync_cnt_max = CONNECT_SYNC_CNT;

    Config.LQAveragingPeriod = (LQ_AVERAGING_MS/Config.frame_rate_ms);

}


// from the setup: what a restart of the controller reads, the link's part follows in setup_configure_link()
void setup_configure_config(uint8_t config_id)
{
    //-- SyncWord
    // note: FrameSyncWord will be modified by Ortho
    // note: FrameSyncWord will be modified also for dual band
    //       Use distinct sync word for dual band to prevent single band receivers from connecting
    //       could be done nice after Config.IsDualBand is set up, but clearer to do here.

    uint32_t bind_dblword = u32_from_bindphrase(Setup.Common[config_id].BindPhrase);

    Config.FrameSyncWord = fmav_crc_calculate((uint8_t*)&bind_dblword, 4); // condense it into a u16

    if (Setup.Common[config_id].Ortho > ORTHO_NONE) { // has hopefully been correctly sanitized in setup_sanitize_config()
        Config.FrameSyncWord += 0x1111 * Setup.Common[config_id].Ortho;
    }

    if (Setup.Common[config_id].FrequencyBand == SETUP_FREQUENCY_DUAL_BAND_915_MHZ_2P4_GHZ) {
        Config.FrameSyncWord += 0x5AA5;
    }
    if (Setup.Common[config_id].FrequencyBand == SETUP_FREQUENCY_DUAL_BAND_868_MHZ_2P4_GHZ) {
        Config.FrameSyncWord += 0xA55A;
    }

    if (Setup.Common[config_id].Privacy > PRIVACY_NONE) {
        Config.FrameSyncWord += 0x1212 * Setup.Common[config_id].Privacy;
    }

    // EU and free radios must not connect to each other, free stays as mLRS
    if (Setup.Common[config_id].Region == REGION_EU) {
        Config.FrameSyncWord += 0x3C3C;
    }

    //-- Diversity
    // Config.Diversity is not actually used for anything besides reporting to disp, cli, etc.

#ifdef DEVICE_IS_TRANSMITTER
    configure_diversity(Setup.Tx[config_id].Diversity); // has hopefully been correctly sanitized in setup_sanitize_config()
#else // DEVICE_IS_RECEIVER
    configure_diversity(Setup.Rx.Diversity); // has hopefully been correctly sanitized in setup_sanitize_config()
#endif

    //-- FrequencyBand, Mode: the bands and the mode set; which are in use is the link's part

    Config.Band1 = Setup.Common[config_id].FrequencyBand; // has hopefully been correctly sanitized in setup_sanitize_config()
    Config.Band2 = (Setup.Common[config_id].Band2) ? Setup.Common[config_id].Band2 - 1 : SETUP_FREQUENCY_BAND_NUM;
    Config.Region = Setup.Common[config_id].Region;
    Config.Mode = Setup.Common[config_id].Mode;
    Config.ModeAuto = (Config.Mode == MODE_AUTO);

    //-- Link Split: the frame length of each direction, their sum stays the same so a cycle's air time does too

    Config.FrameLenTx = Config.FrameLenRx = FRAME_TX_RX_LEN;
    switch (Setup.Common[config_id].LinkSplit) {
    case LINK_SPLIT_DOWNLINK: Config.FrameLenTx = FRAME_LEN_MIN; Config.FrameLenRx = FRAME_LEN_MAX; break;
    case LINK_SPLIT_UPLINK: Config.FrameLenTx = FRAME_LEN_MAX; Config.FrameLenRx = FRAME_LEN_MIN; break;
    }

    //-- Multipoint: a reply slot per receiver

    Config.RxCount = Setup.Common[config_id].RxCount + 1;
#ifdef DEVICE_IS_RECEIVER
    Config.RxSlot = Setup.Rx.Slot;
#else
    Config.RxSlot = 0;
#endif

    //-- Sx12xx
    //     LoraConfigIndex; <- set by configure_mode()
    //     FskSyncWord
    //     FlrcSyncWord;
    //     Power_dbm;       <- set by setup_configure_link(), the region's cap is the band's and mode's
    //     FrequencyBand;   <- set by configure_mode()
    //     is_lora;         <- set by configure_mode()

    Config.Sx.FskSyncWord = Config.FrameSyncWord;
    Config.Sx2.FskSyncWord = Config.Sx.FskSyncWord;

    Config.Sx.FlrcSyncWord = bind_dblword;
    Config.Sx2.FlrcSyncWord = Config.Sx.FlrcSyncWord;

//TODO: sort out for dual band
    if (Setup.Common[config_id].Ortho > ORTHO_NONE) { // has hopefully been correctly sanitized in setup_sanitize_config()
        Config.Sx.FlrcSyncWord += 0x11111111 * Setup.Common[config_id].Ortho;
        Config.Sx2.FlrcSyncWord = Config.Sx.FlrcSyncWord;
    }

    // note: the actually used power will be determined later when the SX are set up
    Config.CustomLbt = Setup.Common[config_id].CustomLbt;
    Config.CustomPowerMaxDbm = Setup.Common[config_id].CustomPowerMaxDbm;
    Config.CustomFreqKHz = Setup.Common[config_id].CustomFreqKHz;
    Config.CustomSpanKHz = Setup.Common[config_id].CustomSpanKHz;
#ifdef DEVICE_IS_TRANSMITTER
    Config.PowerIdx = Setup.Tx[config_id].Power;
#else // DEVICE_IS_RECEIVER
    Config.PowerIdx = Setup.Rx.Power;
#endif

    //-- Fhss
    //     Num;             <- set by setup_configure_link()
    //     Seed;
    //     FrequencyBand;   <- set by setup_configure_link()
    //     Ortho;
    //     Except;          <- set by setup_configure_link(), on 2.4 GHz
    //     BindScan_mask;

    //Config.FhssSeed = bind_dblword;
    // this is much better for narrow bands, like 868 MHz
    // we could make it dependable with a #ifdef, but what's the point
    Config.Fhss.Seed = Config.FrameSyncWord;

    Config.Fhss.Ortho = Setup.Common[config_id].Ortho; // has hopefully been correctly sanitized in setup_sanitize_config()

    Config.FhssExcept2p4 = except_from_bindphrase(Setup.Common[config_id].BindPhrase);

    // this mask is only used in bind, to determine the frequencies to cycle through
    // since both fhss should cycle in sync, we always set them to be equal and have all frequencies
    Config.Fhss.BindScan_mask = 0;
    if (SetupMetaData.FrequencyBand_allowed_mask & (1 << SETUP_FREQUENCY_BAND_2P4_GHZ)) {
        Config.Fhss.BindScan_mask |= (1 << SX_FHSS_FREQUENCY_BAND_2P4_GHZ);
    }
    if (SetupMetaData.FrequencyBand_allowed_mask & (1 << SETUP_FREQUENCY_BAND_915_MHZ_FCC)) {
        Config.Fhss.BindScan_mask |= (1 << SX_FHSS_FREQUENCY_BAND_915_MHZ_FCC);
    }
    if (SetupMetaData.FrequencyBand_allowed_mask & (1 << SETUP_FREQUENCY_BAND_868_MHZ)) {
        Config.Fhss.BindScan_mask |= (1 << SX_FHSS_FREQUENCY_BAND_868_MHZ);
    }
    if (SetupMetaData.FrequencyBand_allowed_mask & (1 << SETUP_FREQUENCY_BAND_433_MHZ)) {
        Config.Fhss.BindScan_mask |= (1 << SX_FHSS_FREQUENCY_BAND_433_MHZ);
    }
    if (SetupMetaData.FrequencyBand_allowed_mask & (1 << SETUP_FREQUENCY_BAND_70_CM_HAM)) {
        Config.Fhss.BindScan_mask |= (1 << SX_FHSS_FREQUENCY_BAND_70_CM_HAM);
    }
    if (SetupMetaData.FrequencyBand_allowed_mask & (1 << SETUP_FREQUENCY_BAND_866_MHZ_IN)) {
        Config.Fhss.BindScan_mask |= (1 << SX_FHSS_FREQUENCY_BAND_866_MHZ_IN);
    }

    //-- Radio Scheme: what this pair may run, from what both ends have; the peer's radios came with the bind
#if defined USE_SX2 && !defined DEVICE_HAS_DUAL_SX126x_SX128x && !defined DEVICE_HAS_DUAL_SX126x_SX126x // two radios on one band here
    bool pair_two_radios = (Setup.Common[config_id].PeerRadioCount == 2);
#else
    bool pair_two_radios = false;
#endif
    Config.RadioSchemeSet = (pair_two_radios) ? Setup.Common[config_id].RadioScheme : RADIO_SCHEME_SINGLE;

    //-- Serial
#ifdef DEVICE_IS_TRANSMITTER
    switch (Setup.Tx[config_id].SerialBaudrate) {
#else // DEVICE_IS_RECEIVER
    switch (Setup.Rx.SerialBaudrate) {
#endif
    case SERIAL_BAUDRATE_57600: Config.SerialBaudrate = 57600; break;
    case SERIAL_BAUDRATE_115200: Config.SerialBaudrate = 115200; break;
    case SERIAL_BAUDRATE_230400: Config.SerialBaudrate = 230400; break;
    default:
#ifdef DEVICE_IS_TRANSMITTER
        Config.SerialBaudrate = 115200;
#else // DEVICE_IS_RECEIVER
        Config.SerialBaudrate = 57600;
#endif
    }

#ifdef DEVICE_IS_TRANSMITTER
    switch (Setup.Tx[config_id].SerialBaudrate2) {
#else // DEVICE_IS_RECEIVER, Ser Out2
    switch (Setup.Rx.SerialBaudrate2) {
#endif
    case SERIAL_BAUDRATE_57600: Config.SerialBaudrate2 = 57600; break;
    case SERIAL_BAUDRATE_115200: Config.SerialBaudrate2 = 115200; break;
    case SERIAL_BAUDRATE_230400: Config.SerialBaudrate2 = 230400; break;
    default:
        Config.SerialBaudrate2 = 115200;
    }

    setup_configure_link();
}


//-------------------------------------------------------
// Crypto
//-------------------------------------------------------

// a fresh 64-bit random, UINT64_MAX when the Tx has no true random source: with Privacy > 0 it then must not connect
uint64_t setup_new_random(void)
{
uint64_t random = UINT64_MAX;

#if defined DEVICE_IS_TRANSMITTER && \
    (defined STM32G4 || defined STM32WL || defined ESP32) // trng is available, note, must match Privacy_allowed_mask
    // the TRNG gives a seed once and BLAKE2b(seed, counter) the randoms: the ESP32 harvests only a small pool at
    // power up, and every connect of a receiver slot draws one. No seed (no TRNG): UINT64_MAX, as before
    static uint8_t drbg[32 + 8]; // seed, counter
    static bool seeded = false;
    if (!seeded) {
        seeded = true;
        for (uint8_t i = 0; i < 8; i++) {
            uint32_t r = UINT32_MAX;
            for (uint8_t k = 0; k < 4 && r == UINT32_MAX; k++) r = trng_get32(); // it can return UINT32_MAX
            if (r == UINT32_MAX) seeded = false;
            memcpy(&drbg[4 * i], &r, 4);
        }
        if (!seeded) memset(drbg, 0, sizeof(drbg)); // and it is tried again at the next draw
    }
    for (uint8_t i = 0; seeded && i < 4 && random == UINT64_MAX; i++) {
        uint64_t counter;
        memcpy(&counter, &drbg[32], 8);
        counter++;
        memcpy(&drbg[32], &counter, 8);
        crypto_blake2b((uint8_t*)&random, 8, drbg, sizeof(drbg));
    }
#endif
    return random;
}


void setup_configure_config_crypto(void)
{
    memset(Config.Uid, 0xFF, 12);
    mcu_uid(Config.Uid);

    Config.BindRandom = setup_new_random();
    Config.SessionRandom = setup_new_random();
    Config.StartupRandom = setup_new_random();
}


//-------------------------------------------------------
// helper
//-------------------------------------------------------

void setup_clear(void)
{
    memset((uint8_t*)&Setup, 0xff, sizeof(tSetup));
}


EE_STATUS_ENUM setup_store_to_EEPROM(void)
{
    return ee_writedata(&Setup, sizeof(tSetup));
}


EE_STATUS_ENUM setup_retrieve_from_EEPROM(void)
{
    return ee_readdata(&Setup, sizeof(tSetup));
}


void setup_reload(void)
{
    setup_retrieve_from_EEPROM();
    setup_sanitize_config(Config.ConfigId);
}


//-------------------------------------------------------
// Init
//-------------------------------------------------------

#include "tlrs_bind.h"

#define SETUPLAYOUT_L0_0_02   2 // layout version up to v0.3.28
#define SETUPLAYOUT_L0_3_29  329 // layout version from v0.3.29 - v0.3.34
#define SETUPLAYOUT_L0_3_35  335 // layout version from v0.3.35 onwards

#ifdef DEVICE_IS_TRANSMITTER
#define SETUP_MARKER_NEW_STR  SETUP_MARKER_TX_STR
#else // DEVICE_IS_RECEIVER
#define SETUP_MARKER_NEW_STR  SETUP_MARKER_RX_STR
#endif


void setup_init(void)
{
EE_STATUS_ENUM ee_status;
bool doEEPROMwrite;

    setup_clear();
    ee_status = ee_init();
    if (ee_status == EE_STATUS_OK) { ee_status = setup_retrieve_from_EEPROM(); }
    if (ee_status != EE_STATUS_OK) { // try it a 2nd time
        setup_clear();
        ee_status = ee_init();
        if (ee_status == EE_STATUS_OK) { ee_status = setup_retrieve_from_EEPROM(); }
    }

    if (ee_status != EE_STATUS_OK) setup_clear(); // force default

    if ((strncmp(Setup.MarkerStr, SETUP_MARKER_NEW_STR, 16) != 0) &&
        (strncmp(Setup.MarkerStr, SETUP_MARKER_OLD_STR, 16) != 0)) {
        setup_clear(); // force default
    }

#ifdef SETUP_FORCE_COMMON_CONF
    setup_clear(); // force default
#endif

    doEEPROMwrite = false;
    if (Setup.Layout != SETUPLAYOUT) {
        // the 2.0 layout shares nothing with the 1.0 one: start from the defaults; the bind phrase and
        // the link key come from the image again (tlrs_bind_apply())
        for (uint8_t id = 0; id < SETUP_CONFIG_NUM; id++) setup_default(id);
        Setup._ConfigId = 0;
        memset(Setup.BindStamp, 0xFF, 4);
        memset(Setup.LinkKey, 0, sizeof(Setup.LinkKey));
        Setup.Layout = SETUPLAYOUT;
        doEEPROMwrite = true;
    }
    if (Setup.Version != VERSION) { // do after Layout, ensures that these flags are correct irrespective of Layout handling
        Setup.Version = VERSION;
        doEEPROMwrite = true;
    }
    if ((strncmp(Setup.MarkerStr, SETUP_MARKER_NEW_STR, 16) != 0)) {
        strbufstrcpy((char*)Setup.MarkerStr, SETUP_MARKER_NEW_STR, 16);
        strbufstrcpy((char*)Setup.MarkerEnd, SETUP_MARKEREND_STR, 8);
        doEEPROMwrite = true;
    }
    if (tlrs_bind_apply()) doEEPROMwrite = true;
    if (doEEPROMwrite) {
        setup_store_to_EEPROM();
    }

#ifdef DEVICE_IS_TRANSMITTER
    if (Setup._ConfigId >= SETUP_CONFIG_NUM) Setup._ConfigId = 0;

#else // DEVICE_IS_RECEIVER
    Setup._ConfigId = 0;
#endif

    Config.ConfigId = Setup._ConfigId;

    setup_configure_metadata();

    setup_sanitize_config(Config.ConfigId);

    setup_configure_config(Config.ConfigId);

    setup_configure_config_crypto();
}


#endif // SETUP_H


/*
setup_configure_metadata();
  -> allowed_mask
  -> optst
  -> SetupMetaData.rx_xxxx

setup_sanitize_config(Config.ConfigId):
  SANITIZE():   ensure param value is in possible range
  TST_NOTALLOWED(): ensure param value is one of the allowed ones

  adjust allowed_mask where it depends on setting

setup_configure_config(Config.ConfigId):



*/
