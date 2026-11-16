# Display-Driver-for-128x160-LCD-ST7735-display-with-Pi-SPI

A small 128×160 colour screen, driven by the Pi over SPI. You get a blank screen and functions to draw on it.

Wire it up — follow the table in WIRING.md. Don't forget the backlight pin, or the screen stays black.
Turn SPI on — sudo raspi-config → Interface Options -> SPI -> Enable, then reboot.
Build it — run ./build_st7735.
Run it — ./st7735_demo. You should see red, green, blue, then a number counting up and down.
Draw your own — tft.fillScreen(), tft.fillRect(), tft.drawText(), tft.drawNumber().

If the colours look wrong or the picture is shifted, WIRING.md has a what-to-change table at the bottom.

ReadME is build using Gen AI because Documentation and Actual-Developmental hobbies do not intersect anywhere in their Ven Diagram.\
This is similar to how John Mulaney clearly explains in [CIA Cold Open in SNL](https://www.youtube.com/watch?v=R_8UPhJNrZ4) at 02:59   
