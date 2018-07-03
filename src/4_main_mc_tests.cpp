/*
 * PackageLicenseDeclared: Apache-2.0
 * Copyright (c) 2017 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "select_program.h"

#if PROGRAM == PROGRAM_TEST_MC

#include "mbed.h"
#include "packets.h"
#include "UpdateCerts.h"
#include "LoRaWANUpdateClient.h"

#ifdef TARGET_SIMULATOR
// Initialize a persistent block device with 528 bytes block size, and 256 blocks (mimicks the at45, which also has 528 size blocks)
#include "SimulatorBlockDevice.h"
SimulatorBlockDevice bd("lorawan-frag-in-flash", 256 * 528, static_cast<uint64_t>(528));
#else
// Flash interface on the L-TEK xDot shield
#include "AT45BlockDevice.h"
AT45BlockDevice bd(SPI_MOSI, SPI_MISO, SPI_SCK, SPI_NSS);
#endif

// fwd declaration
static void fake_send_method(uint8_t, uint8_t*, size_t);

const uint8_t APP_KEY[16] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf };

LoRaWANUpdateClient uc(&bd, APP_KEY, fake_send_method);

typedef struct {
    uint8_t port;
    uint8_t data[255];
    size_t length;
} send_message_t;

static send_message_t last_message;

static void fake_send_method(uint8_t port, uint8_t *data, size_t length) {
    last_message.port = port;
    memcpy(last_message.data, data, length);
    last_message.length = length;
}

static void lorawan_uc_event_handler(LW_UC_EVENT event) {
    /* noop */
}

int main() {
    mbed_trace_init();

    LW_UC_STATUS status;

    uc.setEventCallback(&lorawan_uc_event_handler);

    {
        printf("1) Get package version\n");

        const uint8_t header[] = { 0x0 };
        status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("1) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 200) {
                printf("1) NOK - last_message.port should be 200, but was %d\n", last_message.port);
            }
            else if (last_message.length != 3) {
                printf("1) NOK - last_message.length should be 3, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 0) {
                printf("1) NOK - last_message.data[0] should be 0, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 2) {
                printf("1) NOK - last_message.data[1] should be 2, but was %d\n", last_message.data[1]);
            }
            else if (last_message.data[2] != 1) {
                printf("1) NOK - last_message.data[2] should be 1, but was %d\n", last_message.data[2]);
            }
            else {
                printf("1) OK\n");
            }
        }
    }

    {
        printf("2) Create multicast group with invalid length\n");

        const uint8_t header[] = { 0x2, 0b00,
            0x3e, 0xaa, 0x24, 0x18, /* mcaddr */
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* mcKey_Encrypted */
        };
        status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_INVALID_PACKET_LENGTH) {
            printf("2) NOK - status was not LW_UC_INVALID_PACKET_LENGTH, but %d\n", status);
        }
        else {
            printf("2) OK\n");
        }
    }

    {
        printf("3) Create multicast group with invalid index\n");

        const uint8_t header[] = { 0x2, 0b01,
            0x3e, 0xaa, 0x24, 0x18, /* mcaddr */
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* mcKey_Encrypted */
            0x3, 0x0, 0x0, 0x0, /* minFcCount */
            0x2, 0x10, 0x0, 0x0 /* maxFcCount */
        };
        status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("3) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 200) {
                printf("3) NOK - last_message.port should be 200, but was %d\n", last_message.port);
            }
            else if (last_message.length != 2) {
                printf("3) NOK - last_message.length should be 2, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 2) {
                printf("3) NOK - last_message.data[0] should be 2, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 0b101) { // first bit is error status, last is group
                printf("3) NOK - last_message.data[1] should be 0b101, but was %d\n", last_message.data[1]);
            }
            else {
                printf("3) OK\n");
            }
        }
    }

    {
        printf("4) Create multicast group\n");

        const uint8_t header[] = { 0x2, 0b00,
            0x3e, 0xaa, 0x24, 0x18, /* mcaddr */
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* mcKey_Encrypted */
            0x3, 0x0, 0x0, 0x0, /* minFcCount */
            0x2, 0x10, 0x0, 0x0 /* maxFcCount */
        };
        status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("4) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 200) {
                printf("4) NOK - last_message.port should be 200, but was %d\n", last_message.port);
            }
            else if (last_message.length != 2) {
                printf("4) NOK - last_message.length should be 2, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 2) {
                printf("4) NOK - last_message.data[0] should be 2, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 0) { // status should be 0b000 (first bit is error status, last is group)
                printf("4) NOK - last_message.data[1] should be 0, but was %d\n", last_message.data[1]);
            }
            else {
                printf("4) OK\n");
            }
        }
    }

    {
        printf("5) Delete invalid multicast group\n");

        const uint8_t header[] = { 0x3, 0b10 };
        status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("5) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 200) {
                printf("5) NOK - last_message.port should be 200, but was %d\n", last_message.port);
            }
            else if (last_message.length != 2) {
                printf("5) NOK - last_message.length should be 2, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 3) {
                printf("5) NOK - last_message.data[0] should be 3, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 0b110) {
                printf("5) NOK - last_message.data[1] should be 0b110, but was %d\n", last_message.data[1]);
            }
            else {
                printf("5) OK\n");
            }
        }
    }

    {
        printf("5) Delete valid multicast group\n");

        const uint8_t header[] = { 0x3, 0b00 };
        status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("5) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 200) {
                printf("5) NOK - last_message.port should be 200, but was %d\n", last_message.port);
            }
            else if (last_message.length != 2) {
                printf("5) NOK - last_message.length should be 2, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 3) {
                printf("5) NOK - last_message.data[0] should be 3, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 0) {
                printf("5) NOK - last_message.data[1] should be 0, but was %d\n", last_message.data[1]);
            }
            else {
                printf("5) OK\n");
            }
        }
    }

    {
        printf("6) Delete already deleted multicast group\n");

        const uint8_t header[] = { 0x3, 0b00 };
        status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("6) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 200) {
                printf("6) NOK - last_message.port should be 200, but was %d\n", last_message.port);
            }
            else if (last_message.length != 2) {
                printf("6) NOK - last_message.length should be 2, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 3) {
                printf("6) NOK - last_message.data[0] should be 3, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 0b100) {
                printf("6) NOK - last_message.data[1] should be 0b100, but was %d\n", last_message.data[1]);
            }
            else {
                printf("6) OK\n");
            }
        }
    }


    wait(osWaitForever);
}

#endif
