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

#if PROGRAM == PROGRAM_TEST_MC_SWITCH

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

static bool compare_buffers(uint8_t* buff1, const uint8_t* buff2, size_t size) {
    for (size_t ix = 0; ix < size; ix++) {
        if (buff1[ix] != buff2[ix]) return false;
    }
    return true;
}

static void print_buffer(void* buff, size_t size, bool withSpace = true) {
    for (size_t ix = 0; ix < size; ix++) {
        printf("%02x", ((uint8_t*)buff)[ix]);
        if (withSpace) {
            printf(" ");
        }
    }
}

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
static LoRaWANUpdateClientClassCSession_t class_c;
static bool in_class_c = false;

static void fake_send_method(LoRaWANUpdateClientSendParams_t &params) {
    last_message.port = params.port;
    memcpy(last_message.data, params.data, params.length);
    last_message.length = params.length;
}

static void switch_to_class_a() {
    in_class_c = false;
}

static void switch_to_class_c(LoRaWANUpdateClientClassCSession_t &session) {
    class_c = session;

    in_class_c = true;
}

int main() {
    mbed_trace_init();

    LW_UC_STATUS status;

    // difference between UTC epoch and GPS epoch is 315964800 seconds
    uint64_t gpsTime = 1214658125; // Tue Jul 03 2018 21:02:35 GMT+0800 - yes, it's 9PM in Bali and I'm writing code

    uc.outOfBandClockSync(gpsTime);

    // !!! THESE FUNCTIONS RUN IN AN ISR !!!
    // !!! DO NOT DO BLOCKING THINGS IN THEM !!!
    uc.callbacks.switchToClassA = switch_to_class_a;
    uc.callbacks.switchToClassC = switch_to_class_c;

    // create new group and start a MC request
    const uint8_t setup_header[] = { 0x2, 0b00,
        0x3e, 0xaa, 0x24, 0x18, /* mcaddr */
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* mcKey_Encrypted */
        0x3, 0x0, 0x0, 0x0, /* minFcCount */
        0x2, 0x10, 0x0, 0x0 /* maxFcCount */
    };
    uc.handleMulticastControlCommand((uint8_t*)setup_header, sizeof(setup_header));

    {
        printf("1) Start active class C session request\n");

        uint32_t timeToStart = static_cast<uint32_t>(gpsTime + 2);
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
            printf("1) NOK - should not be in Class C yet\n");
            return 1;
        }

        // after 2 seconds we should be switched to class C...
        wait_ms(2100);

        // @todo: i just hard coded these based on the output so it doesn't actually test anything
        uint8_t expectedNwkSKey[] = { 0xe2, 0xde, 0x64, 0xfe, 0x51, 0x28, 0x06, 0x4f, 0x54, 0x49, 0xd1, 0xb2, 0xc1, 0x17, 0xad, 0xf5 };
        uint8_t expectedAppSKey[] = { 0x23, 0x30, 0x1e, 0xd1, 0xad, 0xde, 0x65, 0x4d, 0xa1, 0x53, 0xdc, 0xf1, 0x41, 0xd4, 0xf5, 0x68 };

        if (!in_class_c) {
            printf("1) NOK - did not switch to Class C after 2 seconds\n");
            return 1;
        }
        else if (class_c.deviceAddr != 0x1824aa3e) {
            printf("1) NOK - did not have right dev address, expected 0x%08x but was 0x%08x\n",
                0x1824aa3e, class_c.deviceAddr);
            return 1;
        }
        else if (class_c.downlinkFreq != freq * 100) {
            printf("1) NOK - did not have right downlink frequency, expected %u but was %u\n", freq * 100, class_c.downlinkFreq);
            return 1;
        }
        else if (class_c.datarate != 3) {
            printf("1) NOK - did not have right datarate, expected %u but was %u\n", 3, class_c.datarate);
            return 1;
        }
        else if (!compare_buffers(class_c.nwkSKey, expectedNwkSKey, 16)) {
            printf("1) NOK - did not have right nwkSKey, expected ");
            print_buffer(expectedNwkSKey, 16, false);
            printf(" but was ");
            print_buffer(class_c.nwkSKey, 16, false);
            printf("\n");
            return 1;
        }
        else if (!compare_buffers(class_c.appSKey, expectedAppSKey, 16)) {
            printf("1) NOK - did not have right appSKey, expected ");
            print_buffer(expectedAppSKey, 16, false);
            printf(" but was ");
            print_buffer(class_c.appSKey, 16, false);
            printf("\n");
            return 1;
        }
        else {
            printf("1) OK - switched to Class C after 2 seconds\n");
        }

        // wait another 2 seconds, and it should not have timed out
        wait_ms(2000);
        if (!in_class_c) {
            printf("1) NOK - should not have switched back to Class A\n");
            return 1;
        }

        // wait another 2 seconds, now it should have timed out
        wait_ms(2100);
        if (in_class_c) {
            printf("1) NOK - should have switched back to Class A\n");
            return 1;
        }
        else {
            printf("1) OK - switched back to Class A\n");
        }
    }

    // now we're 6 seconds into the future
    gpsTime += 6;

    {
        printf("2) Getting a data fragment should reset timeout\n");

        uint32_t timeToStart = static_cast<uint32_t>(gpsTime + 2);
        uint32_t freq = 869525000 / 100;

        // start mc session
        const uint8_t header[] = { 0x4, 0b00,
            timeToStart & 0xff, (timeToStart >> 8) & 0xff, (timeToStart >> 16) & 0xff, (timeToStart >> 24) & 0xff,
            2 /* timeOut (2^8) */,
            freq & 0xff, (freq >> 8) & 0xff, (freq >> 16) & 0xff,
            3 /* data rate */
        };
        uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

        // after 2 seconds we should be switched to class C...
        wait_ms(2100);

        if (!in_class_c) {
            printf("2) NOK - did not switch to Class C after 2 seconds\n");
            return 1;
        }
        else {
            printf("2) OK - switched to Class C after 2 seconds\n");
        }

        // now handle a data fragment (the actual value can be bogus)
        uint8_t dataFragPacket[] = { 0x8, 0x3, 0x3, 0x3 };
        status = uc.handleFragmentationCommand(0x1824aa3e, dataFragPacket, sizeof(dataFragPacket));

        if (status != LW_UC_FRAG_SESSION_NOT_ACTIVE) {
            printf("2) NOK - status should be LW_UC_FRAG_SESSION_NOT_ACTIVE but was %d\n", status);
            return 1;
        }

        // wait another 3.5 seconds, and it should not have timed out
        wait_ms(3500);
        if (!in_class_c) {
            printf("2) NOK - should not have switched back to Class A\n");
            return 1;
        }
        else {
            printf("2) OK - still in Class C after 3.5 seconds\n");
        }

        // wait another second, now it should have timed out
        wait_ms(1000);
        if (in_class_c) {
            printf("2) NOK - should have switched back to Class A\n");
            return 1;
        }
        else {
            printf("2) OK - switched back to Class A\n");
        }
    }

    wait(osWaitForever);
}

#endif
