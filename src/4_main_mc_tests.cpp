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
#include "storage_helper.h"

// fwd declaration
static void fake_send_method(LoRaWANUpdateClientSendParams_t &params);

const uint8_t APP_KEY[16] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf };

LoRaWANUpdateClient uc(&bd, APP_KEY, fake_send_method);

typedef struct {
    uint8_t port;
    uint8_t data[255];
    size_t length;
} send_message_t;

static send_message_t last_message;

static void fake_send_method(LoRaWANUpdateClientSendParams_t &params) {
    last_message.port = params.port;
    memcpy(last_message.data, params.data, params.length);
    last_message.length = params.length;
}

int main() {
    mbed_trace_init();
    mbed_trace_exclude_filters_set("QSPIF");

    LW_UC_STATUS status;

    // difference between UTC epoch and GPS epoch is 315964800 seconds
    uint64_t gpsTime = 1214658125; // Tue Jul 03 2018 21:02:35 GMT+0800 - yes, it's 9PM in Bali and I'm writing code

    uc.outOfBandClockSync(gpsTime);

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
        printf("6) Delete valid multicast group\n");

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
            else if (last_message.data[1] != 0) {
                printf("6) NOK - last_message.data[1] should be 0, but was %d\n", last_message.data[1]);
            }
            else {
                printf("6) OK\n");
            }
        }
    }

    {
        printf("7) Delete already deleted multicast group\n");

        const uint8_t header[] = { 0x3, 0b00 };
        status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("7) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 200) {
                printf("7) NOK - last_message.port should be 200, but was %d\n", last_message.port);
            }
            else if (last_message.length != 2) {
                printf("7) NOK - last_message.length should be 2, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 3) {
                printf("7) NOK - last_message.data[0] should be 3, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 0b100) {
                printf("7) NOK - last_message.data[1] should be 0b100, but was %d\n", last_message.data[1]);
            }
            else {
                printf("7) OK\n");
            }
        }
    }

    {
        printf("8) Get status when no multicast groups are active\n");

        // get the status for all mc groups
        const uint8_t header[] = { 0x1, 0b1111 };
        status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("8) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 200) {
                printf("8) NOK - last_message.port should be 200, but was %d\n", last_message.port);
            }
            else if (last_message.length != 2) {
                printf("8) NOK - last_message.length should be 2, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 1) {
                printf("8) NOK - last_message.data[0] should be 1, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 0) {
                printf("8) NOK - last_message.data[1] should be 0, but was %d\n", last_message.data[1]);
            }
            else {
                printf("8) OK\n");
            }
        }
    }

    {
        printf("9) Get status of active multicast group\n");

        // create new group
        const uint8_t setup_header[] = { 0x2, 0b00,
            0x3e, 0xaa, 0x24, 0x18, /* mcaddr */
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* mcKey_Encrypted */
            0x3, 0x0, 0x0, 0x0, /* minFcCount */
            0x2, 0x10, 0x0, 0x0 /* maxFcCount */
        };
        uc.handleMulticastControlCommand((uint8_t*)setup_header, sizeof(setup_header));

        // get the status for all mc groups
        const uint8_t header[] = { 0x1, 0b1111 };
        status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("9) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 200) {
                printf("9) NOK - last_message.port should be 200, but was %d\n", last_message.port);
            }
            else if (last_message.length != 7) {
                printf("9) NOK - last_message.length should be 7, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 1) {
                printf("9) NOK - last_message.data[0] should be 1, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 0b010001) { // 01 = number of groups, 0001 is the group mask
                printf("9) NOK - last_message.data[1] should be 0b10001, but was %d\n", last_message.data[1]);
            }
            else if (last_message.data[2] != 0) {
                printf("9) NOK - last_message.data[2] should be 0 (mc group index), but was %d\n", last_message.data[2]);
            }
            else if (last_message.data[3] != 0x3e || last_message.data[4] != 0xaa || last_message.data[5] != 0x24 || last_message.data[6] != 0x18) {
                printf("9) NOK - last_message.data[3..7] should be %02x%02x%02x%02x, but was %02x%02x%02x%02x\n",
                    0x3e, 0xaa, 0x24, 0x18,
                    last_message.data[3], last_message.data[4], last_message.data[5], last_message.data[6]);
            }
            else {
                printf("9) OK\n");
            }
        }
    }

    {
        printf("10) Start inactive class C session request\n");

        // start in ~5 seconds (actually a bit less because we ran the other tests before)
        uint32_t timeToStart = static_cast<uint32_t>((gpsTime + 5) % static_cast<uint64_t>(pow(2.0f, 32.0f)));
        uint32_t freq = 869525000 / 100;

        // start mc session
        const uint8_t header[] = { 0x4, 0b10,
            timeToStart & 0xff, (timeToStart >> 8) & 0xff, (timeToStart >> 16) & 0xff, (timeToStart >> 24) & 0xff,
            8 /* timeOut (2^8) */,
            freq & 0xff, (freq >> 8) & 0xff, (freq >> 16) & 0xff, 3 };
        status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("10) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 200) {
                printf("10) NOK - last_message.port should be 200, but was %d\n", last_message.port);
            }
            else if (last_message.length != 2) {
                printf("10) NOK - last_message.length should be 2, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 4) {
                printf("10) NOK - last_message.data[0] should be 4, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 0b10010) { // mc session undefined for 0b10
                printf("10) NOK - last_message.data[1] should be 0b10010, but was %d\n", last_message.data[1]);
            }
            else {
                printf("10) OK\n");
            }
        }
    }

    {
        printf("11) Start active class C session request\n");

        // start in ~5 seconds (actually a bit less because we ran the other tests before)
        uint32_t timeToStart = static_cast<uint32_t>(gpsTime + 5);
        uint32_t freq = 869525000 / 100;

        // start mc session
        const uint8_t header[] = { 0x4, 0b00,
            timeToStart & 0xff, (timeToStart >> 8) & 0xff, (timeToStart >> 16) & 0xff, (timeToStart >> 24) & 0xff,
            8 /* timeOut (2^8) */,
            freq & 0xff, (freq >> 8) & 0xff, (freq >> 16) & 0xff,
            3 /* data rate */
        };
        status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("11) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 200) {
                printf("11) NOK - last_message.port should be 200, but was %d\n", last_message.port);
            }
            else if (last_message.length != 5) {
                printf("11) NOK - last_message.length should be 5, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 4) {
                printf("11) NOK - last_message.data[0] should be 4, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 0) { // no error
                printf("11) NOK - last_message.data[1] should be 0, but was %d\n", last_message.data[1]);
            }
            else {
                // we set our tts to 5 seconds after program start, so this should always be under 5
                uint32_t timeToStart = (last_message.data[4] << 16) + (last_message.data[3] << 8) + (last_message.data[2]);
                if (timeToStart > 5 || timeToStart == 0) {
                    printf("11) NOK - timeToStart should be between 0 and 5, but was %u\n", timeToStart);
                }
                else {
                    printf("11) OK\n");
                }
            }
        }
    }

    {
        printf("12) Class C session in the past should start directly\n");

        uint32_t timeToStart = static_cast<uint32_t>(gpsTime - 10);
        uint32_t freq = 869525000 / 100;

        // start mc session
        const uint8_t header[] = { 0x4, 0b00,
            timeToStart & 0xff, (timeToStart >> 8) & 0xff, (timeToStart >> 16) & 0xff, (timeToStart >> 24) & 0xff,
            8 /* timeOut (2^8) */,
            freq & 0xff, (freq >> 8) & 0xff, (freq >> 16) & 0xff,
            3 /* data rate */
        };
        status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        if (status != LW_UC_OK) {
            printf("12) NOK - status was not LW_UC_OK, but %d\n", status);
        }
        else {
            if (last_message.port != 200) {
                printf("12) NOK - last_message.port should be 200, but was %d\n", last_message.port);
            }
            else if (last_message.length != 5) {
                printf("12) NOK - last_message.length should be 5, but was %d\n", last_message.length);
            }
            else if (last_message.data[0] != 4) {
                printf("12) NOK - last_message.data[0] should be 4, but was %d\n", last_message.data[0]);
            }
            else if (last_message.data[1] != 0) { // no error
                printf("12) NOK - last_message.data[1] should be 0, but was %d\n", last_message.data[1]);
            }
            else {
                // we set our tts to 5 seconds after program start, so this should always be under 5
                uint32_t timeToStart = (last_message.data[4] << 16) + (last_message.data[3] << 8) + (last_message.data[2]);
                if (timeToStart != 0) {
                    printf("12) NOK - timeToStart should be 0, but was %u\n", timeToStart);
                }
                else {
                    printf("12) OK\n");
                }
            }
        }
    }

    wait(osWaitForever);
}

#endif
