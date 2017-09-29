# Test firmware for FOTA testing

The scripts in this folder can create a new package to test firmware updates with lorawan-fragmentation-in-flash. They take a firmware image, sign it with a private key, and create packets which can be injected into a `FragmentationSession`.

## Prerequisites

* node.js (8 or higher)
* GCC
* Python
* OpenSSL

## Keys

A firmware needs to be signed with a private key. You find the keys in the `certs` directory. The public key needs to be included in the device firmware (in the `update_certs.h` file), which uses the key to verify that the firmware was signed with the private key.

To create new keys, run:

```
$ openssl genrsa -out certs/update.key 2048
$ openssl rsa -pubout -in certs/update.key -out update.pub
```

Firmware is also tagged with a device manufacturer UUID and device class UUID, used to prevent flashing the wrong application.

## Generating an application package

1. Compile an image for Multi-Tech xDot with bootloader enabled (the same bootloader as in this project, so the offsets are correct).
1. Copy the `_application.bin` file into this folder.
1. Run:

    ```
    $ npm install
    $ node create-packets-h.js my-app_application.bin ../src/packets.h
    ```

1. This command creates the `packets.h` and the `update_certs.h` files.
1. Re-compile lorawan-fragmentation-in-flash and see the xDot update to your new application.
