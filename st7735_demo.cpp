/*
 * st7735_demo.cpp
 *
 * Proves the panel works, then shows a pitch readout that would be fed by
 * the accelerometer. Run it with no arguments.
 *
 * Copyright (c) 2026 Varun
 * SPDX-License-Identifier: MIT
 */

#include <cmath>
#include <iostream>
#include <thread>
#include <chrono>

#include "st7735.h"
#include "../GPIOManager/GPIOManager.h"

int main()
{
    // Pins (BCM numbering). SPI itself uses GPIO 9/10/11 and CE0 = GPIO 8;
    // those are handled by the spidev driver, not by us.
    const unsigned int dcPin = 25;     // physical pin 22
    const unsigned int resetPin = 24;  // physical pin 18

    try {
        GPIOManager gpio("/dev/gpiochip0", "st7735");
        ST7735 tft("/dev/spidev0.0", gpio, dcPin, resetPin);

        // If you see a band of noise down an edge, try tft.setOffsets(2, 1);
        if (!tft.begin()) {
            std::cerr << "Display did not start\n";
            return 1;
        }

        std::cout << "Display up. Ctrl-C to stop.\n";

        // ---- 1. is it alive? three colour flashes ----
        tft.fillScreen(ST7735::kRed);    std::this_thread::sleep_for(std::chrono::milliseconds(400));
        tft.fillScreen(ST7735::kGreen);  std::this_thread::sleep_for(std::chrono::milliseconds(400));
        tft.fillScreen(ST7735::kBlue);   std::this_thread::sleep_for(std::chrono::milliseconds(400));
        tft.fillScreen(ST7735::kBlack);

        // ---- 2. static layout, drawn once ----
        tft.fillRect(0, 0, ST7735::kWidth, 18, ST7735::kBlue);
        tft.drawText(6, 6, "EEN1071", ST7735::kWhite, ST7735::kBlue, 1);
        tft.drawText(6, 30, "PITCH", ST7735::kGrey, ST7735::kBlack, 1);
        tft.drawText(6, 96, "DEG", ST7735::kGrey, ST7735::kBlack, 1);

        // ---- 3. the bit that changes ----
        // Replace the fake value with sensor.readOrientation() to wire this
        // up to the real accelerometer.
        double t = 0.0;
        while (true) {
            const double pitch = 45.0 * std::sin(t);

            // Big number. Drawing with a background colour overwrites the
            // previous value, so there is no need to clear first.
            tft.drawNumber(6, 50, pitch, 1, ST7735::kYellow, ST7735::kBlack, 3);

            // A bar from the middle: right for positive, left for negative.
            const int centre = ST7735::kWidth / 2;
            const int length = static_cast<int>(pitch * (centre - 4) / 90.0);

            tft.fillRect(0, 120, ST7735::kWidth, 20, ST7735::kBlack);
            if (length >= 0) {
                tft.fillRect(centre, 120, length, 20, ST7735::kGreen);
            } else {
                tft.fillRect(centre + length, 120, -length, 20, ST7735::kRed);
            }
            tft.fillRect(centre, 118, 1, 24, ST7735::kWhite);   // centre mark

            t += 0.08;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
