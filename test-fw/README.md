# Test firmware for FOTA testing

1. Compile an image for xDot with bootloader enabled.
1. Copy the `_application.bin` file into this folder.
1. Create `packets.h` via:

    ```
    $ node create-packets-h.js my-app_application.bin ../src/packets.h
    ```
