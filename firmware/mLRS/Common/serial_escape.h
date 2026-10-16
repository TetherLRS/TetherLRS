//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// Serial Escape
//********************************************************
// One UART shared by serial (data) and com (CLI), switched SiK-style: after 1 s of silence, "+++" followed by 1 s
// of silence puts the port in command mode and answers "OK"; the CLI's "exit" (or "ato") gives it back to the data.
// No button needed, which matters on boards whose only button is a boot strapping pin (ESP32-C3 IO9).
// "+" bytes are held back only after a silence, until the sequence is complete or broken; all other data passes
// through unchanged.
//********************************************************
#ifndef SERIAL_ESCAPE_H
#define SERIAL_ESCAPE_H
#pragma once


#include <inttypes.h>
#include "libs/fifo.h"


#define SERIAL_ESCAPE_GUARD_MS    1000
#define SERIAL_ESCAPE_FIFO_SIZE   512 // power of 2


// The escape logic, without any port: fed bytes and time, it fills the data fifo or enters command mode
class tSerialEscapeCore
{
  public:
    void Init(void)
    {
        fifo.Init();
        command = false;
        plus_cnt = 0;
        last_rx_ms = (uint32_t)0 - SERIAL_ESCAPE_GUARD_MS; // the line counts as silent since power up
    }

    // a byte received in data mode
    void Feed(char c, uint32_t tnow_ms)
    {
        if (c == '+' && plus_cnt < 3 && (plus_cnt > 0 || (tnow_ms - last_rx_ms) >= SERIAL_ESCAPE_GUARD_MS)) {
            plus_cnt++;
        } else {
            release_plus();
            fifo.Put(c);
        }
        last_rx_ms = tnow_ms;
    }

    // every main loop pass: complete the escape after the trailing silence, or give back an incomplete one
    // returns true when command mode was just entered
    bool Tick(uint32_t tnow_ms)
    {
        if (!plus_cnt || (tnow_ms - last_rx_ms) < SERIAL_ESCAPE_GUARD_MS) return false;
        if (plus_cnt == 3) {
            plus_cnt = 0;
            command = true;
            return true;
        }
        release_plus();
        return false;
    }

    void ExitCommand(uint32_t tnow_ms) { command = false; last_rx_ms = tnow_ms; }

    bool IsCommand(void) { return command; }
    bool HasSpace(void) { return fifo.HasSpace(4); } // room for the held "+" bytes and the next one
    bool DataAvailable(void) { return (fifo.Available() > 0); }
    uint16_t DataBytesAvailable(void) { return fifo.Available(); }
    char DataGetc(void) { return fifo.Get(); }
    void DataFlush(void) { fifo.Flush(); }

  private:
    void release_plus(void)
    {
        for (; plus_cnt > 0; plus_cnt--) fifo.Put('+');
    }

    tFifo<char, SERIAL_ESCAPE_FIFO_SIZE> fifo;
    bool command;
    uint8_t plus_cnt;
    uint32_t last_rx_ms;
};


#ifndef SERIAL_ESCAPE_CORE_ONLY

// The two faces the rest of the code sees: serial (data to the radio) and com (CLI), both on the same uart
class tSerialEscape
{
  public:
    void Init(tSerialBase* const _uart) { uart = _uart; core.Init(); }

    void Do(void)
    {
        uint32_t tnow_ms = millis32();
        if (!core.IsCommand()) {
            while (uart->available() && core.HasSpace()) core.Feed(uart->getc(), tnow_ms);
        }
        if (core.Tick(tnow_ms)) uart->puts("\r\nOK\r\n");
    }

    void ExitCommand(void) { core.ExitCommand(millis32()); }
    bool IsCommand(void) { return core.IsCommand(); }

    tSerialBase* uart;
    tSerialEscapeCore core;
};

extern tSerialEscape serial_escape;


class tSerialEscapeDataPort : public tSerialBase
{
  public:
    void SetBaudRate(uint32_t baud) override { serial_escape.uart->SetBaudRate(baud); }
    bool full(void) override { return serial_escape.uart->full(); }
    // while the CLI has the port, data from the link is dropped, as SiK does in AT mode
    void putbuf(uint8_t* const buf, uint16_t len) override { if (!serial_escape.IsCommand()) serial_escape.uart->putbuf(buf, len); }
    bool available(void) override { return serial_escape.core.DataAvailable(); }
    char getc(void) override { return serial_escape.core.DataGetc(); }
    void flush(void) override { serial_escape.core.DataFlush(); serial_escape.uart->flush(); }
    uint16_t bytes_available(void) override { return serial_escape.core.DataBytesAvailable(); }
};


class tSerialEscapeComPort : public tSerialBase
{
  public:
    void SetBaudRate(uint32_t baud) override {} // the CLI runs at the data baud rate, as SiK's AT mode
    bool full(void) override { return serial_escape.uart->full(); }
    void putbuf(uint8_t* const buf, uint16_t len) override { if (serial_escape.IsCommand()) serial_escape.uart->putbuf(buf, len); }
    bool available(void) override { return serial_escape.IsCommand() && serial_escape.uart->available(); }
    char getc(void) override { return serial_escape.IsCommand() ? serial_escape.uart->getc() : '\0'; }
    void flush(void) override {}
    uint16_t bytes_available(void) override { return serial_escape.IsCommand() ? serial_escape.uart->bytes_available() : 0; }
};

#endif // SERIAL_ESCAPE_CORE_ONLY

#endif // SERIAL_ESCAPE_H
