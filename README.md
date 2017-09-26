# Firmware update on Multi-Tech xDot with forward error correction

This is a demonstration application which uses the LoRaWAN data fragmentation proposal schema to apply a firmware update on a Multi-Tech xDot.

It:

* Demonstrates how to use [mbed-lorawan-frag-lib](https://github.com/janjongboom/mbed-lorawan-frag-lib) - a library for Low-Density Parity Encoding - to store an incoming firmware update in flash and use forward error correction to fix missing packets.
* Stores all firmware packets in external flash (AT45 SPI Flash).
* Integrates with a [bootloader](https://github.com/janjongboom/lorawan-at45-fota-bootloader), which will check the flash on startup, and uses the FlashIAP API to perform the firmware update.

You can run this application without access to a LoRaWAN network. [fota-lora-radio](https://github.com/armmbed/fota-lora-radio) uses the same libraries but also comes with a LoRa stack.

## How to get started

1. Install mbed CLI and the GNU ARM Embedded Toolchain (4.9.3).
1. Import this repository:

    ```
    $ mbed import https://github.com/janjongboom/lorawan-fragmentation-in-flash
    ```

1. Build the application:

    ```
    $ mbed compile -m xdot_l151cc -t GCC_ARM
    ```

1. Flash the application on your [L-TEK FF1705](https://os.mbed.com/platforms/L-TEK-FF1705/) development board (or other xDot with AT45 SPI Flash).
1. Attach a serial monitor at baud rate 9,600 to see the update happening.

You can fake packet loss by commenting lines in `src/packets.h`.

## How to add a flash driver

If you're using a different flash chip, you'll need to implement the [BlockDevice](https://docs.mbed.com/docs/mbed-os-api-reference/en/latest/APIs/storage/block_device/) interface. See `AT45BlockDevice.h` in the `at45-blockdevice` driver for more information.

The algorithm expects to be able to write to random addresses, which can span over multiple pages. The flash interface is supposed to hide this from the algorithm. For an example see the `AT45BlockDevice.h` driver.

## Updating the application

Currently the demo application is a simple blinky application, as it needs to be held in flash (which the xDot has little). If you want to update the application:

1. Compile the application with bootloader support enabled (point it at the same bootloader).
1. Copy the `_application.bin` file to the `test-fw` directory.
1. Run:

    ```
    $ node create-packets-h.js my-app_application.bin ../src/packets.h
    ```

## License

* crc.h is derived from work by Salvatore Sanfilippo.
* FragmentationMath.h is derived from work by Semtech Inc.

All other files are licensed under Apache 2.0 license. See LICENSE file for full license.
