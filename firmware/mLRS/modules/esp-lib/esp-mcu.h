//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// ESP MCU
//********************************************************
#ifndef ESPLIB_MCU_H
#define ESPLIB_MCU_H


#ifdef ESP8266
#include "user_interface.h"
#endif


//-------------------------------------------------------
// ESP Device Information
//-------------------------------------------------------

#define ESP_MCU_UID_LEN  12 // must be same as that for STM32


void mcu_uid(uint8_t uid[ESP_MCU_UID_LEN])
{
uint8_t mac[6];

#ifdef ESP8266
    wifi_get_macaddr(STATION_IF, mac);
#else
    esp_efuse_mac_get_default(mac);
#endif

    memcpy(uid, mac, 6);
    memcpy(uid + 6, mac, 6);
}


//-------------------------------------------------------
// BootLoaderInit
//-------------------------------------------------------

#if defined CONFIG_IDF_TARGET_ESP32C3 || defined CONFIG_IDF_TARGET_ESP32S3
#include "soc/rtc_cntl_reg.h"
#endif

void BootLoaderInit(void)
{
#if defined CONFIG_IDF_TARGET_ESP32C3 || defined CONFIG_IDF_TARGET_ESP32S3
    // ROM download mode on the next reset, as holding the boot pin at power up does; flashed through UART0
    REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    esp_restart();
#endif
    // the classic ESP32 and the ESP8266 have no such register: only a hard reset with the boot pin held
};


#endif // ESPLIB_MCU_H