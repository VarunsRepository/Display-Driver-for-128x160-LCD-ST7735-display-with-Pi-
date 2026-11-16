/*
 * st7735.h
 *
 * Driver for an ST7735 SPI TFT (128x160) on a Raspberry Pi.
 *
 * No kernel driver and no device tree overlay. This talks to the panel
 * through /dev/spidev0.0 and two ordinary GPIO pins, so everything that
 * happens is visible in this one file.
 *
 * How the panel works, in three sentences:
 *   1. Every byte you send is either a COMMAND or DATA, and the DC pin is
 *      what says which - low for command, high for data.
 *   2. You tell it a rectangle to draw into (CASET + RASET), then say
 *      "here comes pixel data" (RAMWR), then stream two bytes per pixel.
 *   3. Colours are 16-bit RGB565: 5 bits red, 6 green, 5 blue.
 *
 * Copyright (c) 2026 Varun
 * SPDX-License-Identifier: MIT
 */

#ifndef ST7735_H
#define ST7735_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "../GPIOManager/GPIOManager.h"

class ST7735 {
public:
    static constexpr int kWidth = 128;
    static constexpr int kHeight = 160;

    // A few ready-made colours.
    static constexpr std::uint16_t kBlack   = 0x0000;
    static constexpr std::uint16_t kWhite   = 0xFFFF;
    static constexpr std::uint16_t kRed     = 0xF800;
    static constexpr std::uint16_t kGreen   = 0x07E0;
    static constexpr std::uint16_t kBlue    = 0x001F;
    static constexpr std::uint16_t kYellow  = 0xFFE0;
    static constexpr std::uint16_t kCyan    = 0x07FF;
    static constexpr std::uint16_t kMagenta = 0xF81F;
    static constexpr std::uint16_t kGrey    = 0x8410;

    // gpio must already exist; this class asks it to configure dcPin and
    // resetPin as outputs. The SPI chip-select is handled by spidev, so
    // it is not a pin you manage here.
    ST7735(const std::string& spiDevice,
           GPIOManager& gpio,
           unsigned int dcPin,
           unsigned int resetPin);
    ~ST7735();

    ST7735(const ST7735&) = delete;
    ST7735& operator=(const ST7735&) = delete;

    // Opens SPI, resets the panel, runs the init sequence, clears to black.
    // Returns false if /dev/spidev* will not open or a transfer fails.
    bool begin(std::uint32_t speedHz = 16000000);

    // Pack 8-bit RGB into the panel's 16-bit format.
    static std::uint16_t colour(std::uint8_t r, std::uint8_t g, std::uint8_t b);

    void fillScreen(std::uint16_t c);
    void fillRect(int x, int y, int w, int h, std::uint16_t c);
    void drawPixel(int x, int y, std::uint16_t c);

    // Text uses the 5x7 font, so only ASCII 32..90 (space to 'Z').
    // Lower case is folded to upper case. scale 1 = 5x7 px, scale 3 = 15x21.
    void drawText(int x, int y, const std::string& text,
                  std::uint16_t fg, std::uint16_t bg, int scale = 1);

    // Formats value and draws it, e.g. drawNumber(4, 40, -12.5, 1, ...)
    void drawNumber(int x, int y, double value, int decimals,
                    std::uint16_t fg, std::uint16_t bg, int scale = 1);

    // Some modules have the visible area offset inside the controller's
    // memory. If you see a stripe of noise down one edge, or everything
    // is shifted by a pixel or two, set these. Common values:
    //   black tab: 0, 0   |   green tab: 2, 1   |   red tab: 2, 1
    void setOffsets(int xOffset, int yOffset);

private:
    bool writeCommand(std::uint8_t command);
    bool writeData(const std::uint8_t* data, std::size_t length);
    bool writeDataByte(std::uint8_t value);

    // Sets the rectangle that the next pixel stream will fill.
    bool setWindow(int x0, int y0, int x1, int y1);

    void hardReset();

    int fd = -1;
    GPIOManager& gpio;
    unsigned int dcPin;
    unsigned int resetPin;
    std::string device;

    int xOffset = 0;
    int yOffset = 0;
};

#endif // ST7735_H
