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

#if PROGRAM == PROGRAM_TEST_CLOCK_SYNC

#include "mbed.h"
#include "packets.h"
#include "UpdateCerts.h"
#include "LoRaWANUpdateClient.h"
#include "storage_helper.h"

// fwd declaration
static void fake_send_method(LoRaWANUpdateClientSendParams_t&);

const uint8_t APP_KEY[16] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf };

LoRaWANUpdateClient uc(&bd, APP_KEY, fake_send_method);

typedef struct {
    uint8_t port;
    uint8_t data[255];
    size_t length;
} send_message_t;

static send_message_t last_message;
static bool in_class_c = false;

void switch_to_class_a() {
    in_class_c = false;
}

void switch_to_class_c(LoRaWANUpdateClientClassCSession_t*) {
    in_class_c = true;
}

static void fake_send_method(LoRaWANUpdateClientSendParams_t &params) {
    last_message.port = params.port;
    memcpy(last_message.data, params.data, params.length);
    last_message.length = params.length;
}

int main() {
    mbed_trace_init();
    mbed_trace_exclude_filters_set("QSPIF");

    uint64_t gpsTime = 1214658125; // Tue Jul 03 2018 21:02:35 GMT+0800
    uc.outOfBandClockSync(gpsTime);
    // !!! THESE FUNCTIONS RUN IN AN ISR !!!
    // !!! DO NOT DO BLOCKING THINGS IN THEM !!!
    uc.callbacks.switchToClassA = switch_to_class_a;
    uc.callbacks.switchToClassC = switch_to_class_c;

    wait_ms(2000); // 2 seconds delay to make sure the clock is forward

    LW_UC_STATUS status;

    {
        printf("1) Manual clock sync\n");

        status = uc.requestClockSync(true);
        if (status != LW_UC_OK) {
            printf("1) NOK - expected LW_UC_OK but got %d\n", status);
            return 1;
        }
        else if (last_message.port != 202) {
            printf("1) NOK - last_message.port should be 202, but was %d\n", last_message.port);
            return 1;
        }
        else if (last_message.length != 6) {
            printf("1) NOK - last_message.length should be 6, but was %u\n", last_message.length);
            return 1;
        }
        else if (last_message.data[0] != 1) {
            printf("1) NOK - last_message.data[0] should be 1, but was %d\n", last_message.data[0]);
            return 1;
        }
        else if (last_message.data[5] != 0b10000) {
            printf("1) NOK - last_message.data[5] should be 0b10000, but was %d\n", last_message.data[5]);
        }
        else {
            uint32_t curr_time = (last_message.data[4] << 24) + (last_message.data[3] << 16) + (last_message.data[2] << 8) + last_message.data[1];

            if (curr_time - gpsTime > 4) {
                printf("1) NOK - curr_time drifts too much from OOB clock sync (difference is %llu)\n", curr_time - gpsTime);
                return 1;
            }
        }

        printf("1) OK\n");
    }

    {
        printf("2) Clock sync response should adjust time\n");

        // device clock is 2400 seconds too fast
        int32_t adjust = -2400;

        uint8_t header[] = { 1, adjust & 0xff, (adjust >> 8) & 0xff, (adjust >> 16) & 0xff, (adjust >> 24) & 0xff, 0b0000 /* tokenAns */ };
        status = uc.handleClockSyncCommand(header, sizeof(header));
        if (status != LW_UC_OK) {
            printf("2) NOK - expected LW_UC_OK but got %d\n", status);
            return 1;
        }

        uint64_t currTime = uc.getCurrentTime_s();
        if (currTime > gpsTime) {
            printf("2) NOK - currTime should be before gpsTime (currTime=%llu, gpsTime=%llu)\n", currTime, gpsTime);
            return 1;
        }

        if (gpsTime - currTime > 2405 || gpsTime - currTime < 2395) {
            printf("2) NOK - currTime and gpsTime difference should be about 2400 (currTime=%llu, gpsTime=%llu)\n", currTime, gpsTime);
            return 1;
        }

        printf("2) OK\n");
    }

    {
        printf("3) Clock sync response should've upped tokenAns\n");

        status = uc.requestClockSync(false);
        if (status != LW_UC_OK) {
            printf("3) NOK - expected LW_UC_OK but got %d\n", status);
            return 1;
        }
        else if (last_message.data[5] != 0b00001) {
            printf("3) NOK - last_message.data[5] should be 0b00001, but was %d\n", last_message.data[5]);
        }

        printf("3) OK\n");
    }

    {
        printf("4) Should handle forceDeviceSyncReq\n");

        uint8_t header[] = { 3, 0b001 /* nbTrans, not implemented yet */ };
        status = uc.handleClockSyncCommand(header, sizeof(header));
        if (status != LW_UC_OK) {
            printf("4) NOK - expected LW_UC_OK but got %d\n", status);
            return 1;
        }
        else if (last_message.port != 202) {
            printf("4) NOK - last_message.port should be 202, but was %d\n", last_message.port);
            return 1;
        }
        else if (last_message.length != 6) {
            printf("4) NOK - last_message.length should be 6, but was %u\n", last_message.length);
            return 1;
        }
        else if (last_message.data[0] != 1) {
            printf("4) NOK - last_message.data[0] should be 1, but was %d\n", last_message.data[0]);
            return 1;
        }
        else if (last_message.data[5] != 0b00001) { // still the same as no reply was had yet
            printf("4) NOK - last_message.data[5] should be 0b00001, but was %d\n", last_message.data[5]);
        }
        else {
            uint32_t curr_time = (last_message.data[4] << 24) + (last_message.data[3] << 16) + (last_message.data[2] << 8) + last_message.data[1];

            if (curr_time - (gpsTime - 2400) > 5) {
                printf("4) NOK - curr_time drifts too much from OOB clock sync, curr_time=%u, gpsTime=%llu (difference is %llu)\n", curr_time, gpsTime, curr_time - (gpsTime - 2400));
                return 1;
            }
        }

        printf("4) OK\n");
    }

    {
        printf("5) Should update MC sessions when clock sync comes in\n");

        uc.outOfBandClockSync(gpsTime);

        // create new group and start a MC request
        const uint8_t setup_header[] = { 0x2, 0b00,
            0x3e, 0xaa, 0x24, 0x18, /* mcaddr */
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* mcKey_Encrypted */
            0x3, 0x0, 0x0, 0x0, /* minFcCount */
            0x2, 0x10, 0x0, 0x0 /* maxFcCount */
        };
        uc.handleMulticastControlCommand((uint8_t*)setup_header, sizeof(setup_header));

        uint32_t timeToStart = static_cast<uint32_t>(gpsTime + 100);
        uint32_t freq = 869525000 / 100;

        // start mc session
        const uint8_t header[] = { 0x4, 0b00,
            timeToStart & 0xff, (timeToStart >> 8) & 0xff, (timeToStart >> 16) & 0xff, (timeToStart >> 24) & 0xff,
            2 /* timeOut (2^8) */,
            freq & 0xff, (freq >> 8) & 0xff, (freq >> 16) & 0xff,
            3 /* data rate */
        };
        uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        if (in_class_c) {
            printf("5) NOK - should not be in Class C yet\n");
            return 1;
        }

        // after 2 seconds we should not be switched to class C...
        wait_ms(2100);

        if (in_class_c) {
            printf("5) NOK - should not be in Class C yet\n");
            return 1;
        }

        // so now, send an adjustment of +96 seconds and the MC group should start 2 seconds later...
        int32_t adjust = 96;

        // device needs to trigger this to make sure the request is valid
        uc.requestClockSync(false);

        uint8_t adjustHeader[] = { 1, adjust & 0xff, (adjust >> 8) & 0xff, (adjust >> 16) & 0xff, (adjust >> 24) & 0xff, 0b0001 /* tokenAns */ };
        status = uc.handleClockSyncCommand(adjustHeader, sizeof(adjustHeader));

        wait_ms(2100);

        if (!in_class_c) {
            printf("5) NOK - should have switched to Class C\n");
            return 1;
        }

        printf("5) OK\n");
    }

    wait(osWaitForever);
}

#endif
