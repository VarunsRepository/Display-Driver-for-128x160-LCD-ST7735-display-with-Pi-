/*
 * st7735.cpp
 *
 * Copyright (c) 2026 Varun
 * SPDX-License-Identifier: MIT
 */

#include "st7735.h"

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#include "font5x7.h"

namespace {

// ---- ST7735 commands we actually use (datasheet section 10) ----------
constexpr std::uint8_t kSwReset = 0x01;   // software reset
constexpr std::uint8_t kSleepOut = 0x11;  // wake up
constexpr std::uint8_t kInvOff = 0x20;    // normal colours
constexpr std::uint8_t kNormalOn = 0x13;  // normal display mode
constexpr std::uint8_t kDisplayOn = 0x29; // turn the panel on
constexpr std::uint8_t kColumnSet = 0x2A; // CASET - x range
constexpr std::uint8_t kRowSet = 0x2B;    // RASET - y range
constexpr std::uint8_t kRamWrite = 0x2C;  // RAMWR - pixels follow
constexpr std::uint8_t kMadCtl = 0x36;    // orientation + RGB/BGR order
constexpr std::uint8_t kPixelFormat = 0x3A;

// spidev refuses transfers bigger than its buffer, which is 4096 bytes by
// default (see /sys/module/spidev/parameters/bufsiz), so long pixel runs
// are sent in slices of this size.
constexpr std::size_t kMaxChunk = 4096;

void sleepMs(int milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

} // namespace

ST7735::ST7735(const std::string& spiDevice,
               GPIOManager& gpioManager,
               unsigned int dc,
               unsigned int reset)
    : gpio(gpioManager), dcPin(dc), resetPin(reset), device(spiDevice)
{
}

ST7735::~ST7735()
{
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

void ST7735::setOffsets(int x, int y)
{
    xOffset = x;
    yOffset = y;
}

std::uint16_t ST7735::colour(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    // RGB565: keep the top 5 bits of red, 6 of green, 5 of blue.
    return static_cast<std::uint16_t>(((r & 0xF8) << 8) |
                                      ((g & 0xFC) << 3) |
                                       (b >> 3));
}

bool ST7735::begin(std::uint32_t speedHz)
{
    // ---- the two control pins ----
    gpio.configureOutputs({dcPin, resetPin});

    // ---- open SPI ----
    fd = ::open(device.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        std::cerr << "ST7735: cannot open " << device << ": "
                  << std::strerror(errno) << "\n"
                  << "        is SPI enabled? (raspi-config, or "
                     "dtparam=spi=on in config.txt)\n";
        return false;
    }

    std::uint8_t mode = SPI_MODE_0;     // ST7735 clocks data in on the rising edge
    std::uint8_t bits = 8;

    if (::ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ::ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ::ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speedHz) < 0) {
        std::cerr << "ST7735: cannot configure SPI: " << std::strerror(errno) << "\n";
        ::close(fd);
        fd = -1;
        return false;
    }

    // ---- wake the panel up ----
    hardReset();

    if (!writeCommand(kSwReset)) return false;
    sleepMs(150);

    if (!writeCommand(kSleepOut)) return false;
    sleepMs(500);

    // 16 bits per pixel
    if (!writeCommand(kPixelFormat) || !writeDataByte(0x05)) return false;

    // Orientation and colour order. 0x00 is portrait, RGB order.
    // If red and blue come out swapped, change this to 0x08 (BGR).
    if (!writeCommand(kMadCtl) || !writeDataByte(0x00)) return false;

    // Some panels ship inverted. If your display looks like a photo
    // negative, use 0x21 (INVON) here instead of kInvOff.
    if (!writeCommand(kInvOff)) return false;

    if (!writeCommand(kNormalOn)) return false;
    sleepMs(10);

    if (!writeCommand(kDisplayOn)) return false;
    sleepMs(100);

    fillScreen(kBlack);
    return true;
}

void ST7735::hardReset()
{
    // Datasheet wants reset held low for at least 10 us; being generous
    // costs nothing and some modules have a slow RC on the reset line.
    gpio.writePin(resetPin, true);
    sleepMs(5);
    gpio.writePin(resetPin, false);
    sleepMs(20);
    gpio.writePin(resetPin, true);
    sleepMs(150);
}

bool ST7735::writeCommand(std::uint8_t command)
{
    if (fd < 0) {
        return false;
    }
    gpio.writePin(dcPin, false);                 // DC low = this is a command
    return ::write(fd, &command, 1) == 1;
}

bool ST7735::writeData(const std::uint8_t* data, std::size_t length)
{
    if (fd < 0 || data == nullptr) {
        return false;
    }
    gpio.writePin(dcPin, true);                  // DC high = this is data

    std::size_t sent = 0;
    while (sent < length) {
        const std::size_t slice = std::min(kMaxChunk, length - sent);
        const ssize_t written = ::write(fd, data + sent, slice);
        if (written != static_cast<ssize_t>(slice)) {
            std::cerr << "ST7735: SPI write failed: " << std::strerror(errno) << "\n";
            return false;
        }
        sent += slice;
    }
    return true;
}

bool ST7735::writeDataByte(std::uint8_t value)
{
    return writeData(&value, 1);
}

bool ST7735::setWindow(int x0, int y0, int x1, int y1)
{
    x0 += xOffset;  x1 += xOffset;
    y0 += yOffset;  y1 += yOffset;

    // Column range: two 16-bit values, big-endian.
    const std::uint8_t columns[4] = {
        static_cast<std::uint8_t>(x0 >> 8), static_cast<std::uint8_t>(x0 & 0xFF),
        static_cast<std::uint8_t>(x1 >> 8), static_cast<std::uint8_t>(x1 & 0xFF)
    };
    const std::uint8_t rows[4] = {
        static_cast<std::uint8_t>(y0 >> 8), static_cast<std::uint8_t>(y0 & 0xFF),
        static_cast<std::uint8_t>(y1 >> 8), static_cast<std::uint8_t>(y1 & 0xFF)
    };

    if (!writeCommand(kColumnSet) || !writeData(columns, sizeof(columns))) return false;
    if (!writeCommand(kRowSet) || !writeData(rows, sizeof(rows))) return false;

    // After this, every data byte goes into the rectangle, left to right,
    // top to bottom, wrapping automatically.
    return writeCommand(kRamWrite);
}

void ST7735::fillRect(int x, int y, int w, int h, std::uint16_t c)
{
    // Clip to the screen so a caller can be sloppy without corrupting
    // anything.
    if (w <= 0 || h <= 0) return;
    if (x >= kWidth || y >= kHeight) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    w = std::min(w, kWidth - x);
    h = std::min(h, kHeight - y);
    if (w <= 0 || h <= 0) return;

    if (!setWindow(x, y, x + w - 1, y + h - 1)) {
        return;
    }

    // One chunk of the colour repeated, sent as many times as needed.
    const std::uint8_t high = static_cast<std::uint8_t>(c >> 8);
    const std::uint8_t low = static_cast<std::uint8_t>(c & 0xFF);

    std::size_t bytesLeft = static_cast<std::size_t>(w) * h * 2;
    std::vector<std::uint8_t> chunk(std::min(bytesLeft, kMaxChunk));
    for (std::size_t i = 0; i + 1 < chunk.size(); i += 2) {
        chunk[i] = high;
        chunk[i + 1] = low;
    }

    while (bytesLeft > 0) {
        const std::size_t slice = std::min(bytesLeft, chunk.size());
        if (!writeData(chunk.data(), slice)) {
            return;
        }
        bytesLeft -= slice;
    }
}

void ST7735::fillScreen(std::uint16_t c)
{
    fillRect(0, 0, kWidth, kHeight, c);
}

void ST7735::drawPixel(int x, int y, std::uint16_t c)
{
    fillRect(x, y, 1, 1, c);
}

void ST7735::drawText(int x, int y, const std::string& text,
                      std::uint16_t fg, std::uint16_t bg, int scale)
{
    if (scale < 1) scale = 1;

    // Each character is drawn as one rectangle blit: build its pixels in
    // memory, then send them in a single pass. Doing it pixel by pixel
    // would mean a separate SPI window for every dot, which is very slow.
    const int cellW = (kFontWidth + 1) * scale;      // +1 column of spacing
    const int cellH = kFontHeight * scale;

    std::vector<std::uint8_t> cell(static_cast<std::size_t>(cellW) * cellH * 2);
    int cursorX = x;

    for (char raw : text) {
        // The font has no lower case, so fold it.
        char ch = raw;
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 'a' + 'A');
        }
        if (ch < kFontFirstChar || ch > kFontLastChar) {
            ch = ' ';
        }

        const std::uint8_t* glyph = kFont5x7[ch - kFontFirstChar];

        for (int py = 0; py < cellH; ++py) {
            const int row = py / scale;
            for (int px = 0; px < cellW; ++px) {
                const int col = px / scale;

                // col 5 is the blank spacing column between characters.
                const bool on = (col < kFontWidth) &&
                                (((glyph[col] >> row) & 1) != 0);
                const std::uint16_t c = on ? fg : bg;

                const std::size_t offset =
                    (static_cast<std::size_t>(py) * cellW + px) * 2;
                cell[offset] = static_cast<std::uint8_t>(c >> 8);
                cell[offset + 1] = static_cast<std::uint8_t>(c & 0xFF);
            }
        }

        if (cursorX < kWidth && cursorX + cellW > 0 &&
            setWindow(cursorX, y, cursorX + cellW - 1, y + cellH - 1)) {
            writeData(cell.data(), cell.size());
        }

        cursorX += cellW;
        if (cursorX >= kWidth) {
            break;                                  // ran off the edge
        }
    }
}

void ST7735::drawNumber(int x, int y, double value, int decimals,
                        std::uint16_t fg, std::uint16_t bg, int scale)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    drawText(x, y, buffer, fg, bg, scale);
}
