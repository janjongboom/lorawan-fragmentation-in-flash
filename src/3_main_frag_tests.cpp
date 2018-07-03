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

#if PROGRAM == PROGRAM_TEST_FRAG

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

LoRaWANUpdateClient uc(&bd, fake_send_method);

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

void assert(uint8_t testIx, bool result, const char *msg) {
    printf("%d) %s - %s\n", testIx, result ? "OK" : "NOK", msg);
}

int main() {
    mbed_trace_init();

    LW_UC_STATUS status;

    uc.setEventCallback(&lorawan_uc_event_handler);

    {
        printf("1) Test creating frag session with invalid length\n");

        const uint8_t header[] = { 0x2, 0x0, 0x28 };
        status = uc.handleFragmentationCommand((uint8_t*)header, sizeof(header));

        printf("1) %s\n", status == LW_UC_INVALID_PACKET_LENGTH ? "OK" : "NOK");
    }

    {
        printf("2) Test creating frag session with invalid index\n");

        // frag index 4 is invalid, can only have one...
        const uint8_t header[] = { 0x2, 0b00110000, 0x28, 0x0, 0xcc, 0x0, 0xa7, 0x0, 0x0, 0x0, 0x0 };
        status = uc.handleFragmentationCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("2) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 201) {
                printf("2) NOK - last_message.port should be 201, but was %d\n", last_message.port);
            }
            else if (last_message.length != 2) {
                printf("2) NOK - last_message.length should be 2, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 0x2) {
                printf("2) NOK - last_message.data[0] should be 2, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 0b0100) {
                printf("2) NOK - last_message.data[1] should be %d, but was %d\n", 0b0100, last_message.data[1]);
            }
            else {
                printf("2) OK\n");
            }
        }
    }

    {
        printf("3) Create session\n");

        status = uc.handleFragmentationCommand((uint8_t*)FAKE_PACKETS_HEADER, sizeof(FAKE_PACKETS_HEADER));

        if (status != LW_UC_OK) {
            printf("2) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 201) {
                printf("3) NOK - last_message.port should be 201, but was %d\n", last_message.port);
            }
            else if (last_message.length != 2) {
                printf("3) NOK - last_message.length should be 2, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 0x2) {
                printf("3) NOK - last_message.data[0] should be 2, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 0b0000) {
                printf("3) NOK - last_message.data[1] should be %d, but was %d\n", 0b0000, last_message.data[1]);
            }
            else {
                printf("3) OK\n");
            }
        }
    }

    {
        printf("4) Get status\n");

        // alright now send 3 packets (2 missing, 0 and 3) and see what status we can read...
        status = uc.handleFragmentationCommand((uint8_t*)FAKE_PACKETS[1], sizeof(FAKE_PACKETS[0]));
        if (status != LW_UC_OK) printf("4) NOK - packet 1 status was not LW_UC_OK, but %d\n", status);
        uc.handleFragmentationCommand((uint8_t*)FAKE_PACKETS[2], sizeof(FAKE_PACKETS[0]));
        if (status != LW_UC_OK) printf("4) NOK - packet 2 status was not LW_UC_OK, but %d\n", status);
        uc.handleFragmentationCommand((uint8_t*)FAKE_PACKETS[4], sizeof(FAKE_PACKETS[0]));
        if (status != LW_UC_OK) printf("4) NOK - packet 4 status was not LW_UC_OK, but %d\n", status);

        // [7..3] = RFU, 00 = fragIx (which should be active), 1 = all participants
        const uint8_t header[] = { 0x1, 0b00000001 };
        status = uc.handleFragmentationCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("4) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 201) {
                printf("4) NOK - last_message.port should be 201, but was %d\n", last_message.port);
            }
            else if (last_message.length != 5) {
                printf("4) NOK - last_message.length should be 5, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 0x1) {
                printf("4) NOK - last_message.data[0] should be 1, but was %d\n", last_message.data[0]);
            }
            // 1&2 is 2 byte field, upper 2 bits should be the ix; then 3 received messages in total
            else if (last_message.data[1] != 0b00000000) {
                printf("4) NOK - last_message.data[1] should be 0, but was %d\n", last_message.data[1]);
            }
            else if (last_message.data[2] != 3) {
                printf("4) NOK - last_message.data[2] should be 3, but was %d\n", last_message.data[2]);
            }
            // missing frag field
            else if (last_message.data[3] != 2) {
                printf("4) NOK - last_message.data[3] should be 2, but was %d\n", last_message.data[3]);
            }
            // status field... 7..1 RFU, 0 should be 1 only if out of memory
            else if (last_message.data[4] != 0) {
                printf("4) NOK - last_message.data[4] should be %d, but was %d\n", 0, last_message.data[4]);
            }
            else {
                printf("4) OK\n");
            }
        }
    }

    {
        printf("5) Delete session\n");

        const uint8_t header[] = { 0x3, 0 };
        status = uc.handleFragmentationCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("5) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 201) {
                printf("5) NOK - last_message.port should be 201, but was %d\n", last_message.port);
            }
            else if (last_message.length != 2) {
                printf("5) NOK - last_message.length should be 2, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 3) {
                printf("5) NOK - last_message.data[0] should be 3, but was %d\n", last_message.data[0]);
            }
            // bit 2 = sessionNotExists flag, bit [1..0] is fragIx
            else if (last_message.data[1] != 0b00000000) {
                printf("5) NOK - last_message.data[1] should be 0, but was %d\n", last_message.data[1]);
            }
            else {
                printf("5) OK\n");
            }
        }
    }

    {
        printf("6) Delete invalid session\n");

        const uint8_t header[] = { 0x3, 0b10 };
        status = uc.handleFragmentationCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("6) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 201) {
                printf("6) NOK - last_message.port should be 201, but was %d\n", last_message.port);
            }
            else if (last_message.length != 2) {
                printf("6) NOK - last_message.length should be 2, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 3) {
                printf("6) NOK - last_message.data[0] should be 3, but was %d\n", last_message.data[0]);
            }
            // bit 2 = sessionNotExists flag, bit [1..0] is fragIx
            else if (last_message.data[1] != 0b00000110) {
                printf("6) NOK - last_message.data[1] should be 0b101, but was %d\n", last_message.data[1]);
            }
            else {
                printf("6) OK\n");
            }
        }
    }

    {
        printf("7) Get package version\n");

        const uint8_t header[] = { 0x0 };
        status = uc.handleFragmentationCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("7) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 201) {
                printf("7) NOK - last_message.port should be 201, but was %d\n", last_message.port);
            }
            else if (last_message.length != 3) {
                printf("7) NOK - last_message.length should be 3, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 0) {
                printf("7) NOK - last_message.data[0] should be 0, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 3) {
                printf("7) NOK - last_message.data[1] should be 3, but was %d\n", last_message.data[1]);
            }
            else if (last_message.data[2] != 1) {
                printf("7) NOK - last_message.data[2] should be 1, but was %d\n", last_message.data[2]);
            }
            else {
                printf("7) OK\n");
            }
        }
    }

    wait(osWaitForever);
}

#endif
