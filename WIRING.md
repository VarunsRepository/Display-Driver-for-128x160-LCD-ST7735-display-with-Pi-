# ST7735 on a Raspberry Pi 4 — wiring and setup

## Wiring

Most ST7735 breakouts label the pins like this. Yours may use slightly
different names — the alternatives are in brackets.

| Module pin | Pi 4 pin | BCM | Notes |
|---|---|---|---|
| VCC | 1 or 17 | — | **3.3 V**, not 5 V, unless your board has a regulator |
| GND | 6 | — | |
| SCL (SCK, CLK) | 23 | GPIO 11 | SPI clock |
| SDA (MOSI, DIN) | 19 | GPIO 10 | data to the display |
| CS (CE) | 24 | GPIO 8 | chip select — spidev drives this, not your code |
| DC (A0, RS) | 22 | GPIO 25 | data/command select |
| RES (RST) | 18 | GPIO 24 | reset |
| BLK (LED, BL) | 17 | — | backlight → 3.3 V to stay always on |

The display is write-only in this driver, so MISO is not connected.

If `BLK` is left floating you get a black screen with everything else
working — that catches people out.

## Enable SPI

```bash
sudo raspi-config        # Interface Options -> SPI -> Enable
sudo reboot
ls -l /dev/spidev0.0     # should exist
```

Or add `dtparam=spi=on` to `/boot/firmware/config.txt` yourself.

## Build and run

```bash
./build_st7735
./st7735_demo
```

If you get a permissions error, add yourself to the right groups once:

```bash
sudo usermod -aG spi,gpio $USER      # then log out and back in
```

## If something looks wrong

| Symptom | Fix |
|---|---|
| Black screen, no errors | Backlight pin not powered — connect BLK to 3.3 V |
| Nothing at all, errors on open | SPI not enabled, or wrong `/dev/spidev` number |
| Red and blue swapped | Change `MADCTL` from `0x00` to `0x08` in `begin()` |
| Looks like a photo negative | Use `0x21` (INVON) instead of `kInvOff` in `begin()` |
| Stripe of noise down one edge | `tft.setOffsets(2, 1);` before `begin()` |
| Image shifted a pixel or two | Same — try `(2, 1)` or `(1, 2)` |
| Garbage, or works then glitches | Drop the speed: `tft.begin(8000000)` |
| Upside down / wrong rotation | `MADCTL`: `0x00` portrait, `0xC0` portrait flipped, `0x60` and `0xA0` landscape |

The `MADCTL` and inversion settings vary by module batch, which is why
every ST7735 library has a "tab colour" option. Mine defaults to the
black-tab behaviour; the table above covers the rest.

## Speed, honestly

SPI at 16 MHz gives about 2 MB/s of real throughput. A full 128×160 frame
is 40 KB, so a complete redraw is roughly 20 ms — around 50 fps if you
redraw everything, which is plenty. The demo only redraws the parts that
change, so it is far faster than that.

Raise the clock with `tft.begin(32000000)` if your wiring is short and
tidy. Long jumper leads start producing glitches well before that.
