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

#if PROGRAM == PROGRAM_TEST_UPDATE_OVER_MC

#include "mbed.h"
#include "mbed_mem_trace.h"
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

typedef struct {
    uint8_t port;
    uint8_t data[255];
    size_t length;
} send_message_t;

static send_message_t last_message;
static LoRaWANUpdateClientClassCSession_t class_c;
static bool in_class_c = false;
static bool is_complete = false;
static bool is_fw_ready = false;

static void fake_send_method(LoRaWANUpdateClientSendParams_t &params) {
    last_message.port = params.port;
    memcpy(last_message.data, params.data, params.length);
    last_message.length = params.length;
}

static void switch_to_class_a() {
    in_class_c = false;
}

static void switch_to_class_c(LoRaWANUpdateClientClassCSession_t *session) {
    memcpy(&class_c, session, sizeof(LoRaWANUpdateClientClassCSession_t));

    in_class_c = true;
}

static void lorawan_uc_fragsession_complete() {
    is_complete = true;
}

static void lorawan_uc_firmware_ready() {
    is_fw_ready = true;

    printf("Firmware is ready\n");
}

static void clear_mem_trace() {
    mbed_mem_trace_set_callback(NULL);
}

static void setup_mem_trace() {
    mbed_mem_trace_set_callback(mbed_mem_trace_default_callback);
}

int main() {
    mbed_trace_init();
    mbed_mem_trace_set_callback(mbed_mem_trace_default_callback);

    LoRaWANUpdateClient *uc = new LoRaWANUpdateClient(&bd, APP_KEY, fake_send_method);
    uc->printHeapStats("BEGIN ");

    LW_UC_STATUS status;

#ifdef TARGET_SIMULATOR
    uint32_t curr_time = EM_ASM_INT({
        return Date.now();
    });
    srand(curr_time);
#endif

    // difference between UTC epoch and GPS epoch is 315964800 seconds
    uint64_t gpsTime = 1214658125; // Tue Jul 03 2018 21:02:35 GMT+0800

    uc->outOfBandClockSync(gpsTime);

    uc->printHeapStats("OOB CLOCK SYNC ");

    // !!! THESE FUNCTIONS RUN IN AN ISR !!!
    // !!! DO NOT DO BLOCKING THINGS IN THEM !!!
    uc->callbacks.switchToClassA = switch_to_class_a;
    uc->callbacks.switchToClassC = switch_to_class_c;

    // These run in the context that calls the update client
    uc->callbacks.fragSessionComplete = lorawan_uc_fragsession_complete;
    uc->callbacks.firmwareReady = lorawan_uc_firmware_ready;
    uc->callbacks.verificationStarting = clear_mem_trace;
    uc->callbacks.verificationFinished = setup_mem_trace;

    uc->printHeapStats("CALLBACKS ");

    // create new group and start a MC request
    const uint8_t mc_setup_header[] = { 0x2, 0b00,
        0x3e, 0xaa, 0x24, 0x18, /* mcaddr */
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* mcKey_Encrypted */
        0x3, 0x0, 0x0, 0x0, /* minFcCount */
        0x2, 0x10, 0x0, 0x0 /* maxFcCount */
    };
    status = uc->handleMulticastControlCommand((uint8_t*)mc_setup_header, sizeof(mc_setup_header));
    if (status != LW_UC_OK) {
        printf("1) NOK - Create MC group failed, expected LW_UC_OK, but got %d\n", status);
        return 1;
    }

    uc->printHeapStats("HANDLE MC ");

    // create fragmentation group
    status = uc->handleFragmentationCommand(0x0, (uint8_t*)FAKE_PACKETS_HEADER, sizeof(FAKE_PACKETS_HEADER));
    if (status != LW_UC_OK) {
        printf("1) NOK - Create fragmentation group failed, expected LW_UC_OK, but got %d\n", status);
        return 1;
    }

    uc->printHeapStats("HANDLE FRAG ");

    // start MC session
    uint32_t timeToStart = static_cast<uint32_t>(gpsTime + 2);
    uint32_t freq = 869525000 / 100;

    const uint8_t mc_start_header[] = { 0x4, 0b00,
        timeToStart & 0xff, (timeToStart >> 8) & 0xff, (timeToStart >> 16) & 0xff, (timeToStart >> 24) & 0xff,
        2 /* timeOut (2^8) */,
        freq & 0xff, (freq >> 8) & 0xff, (freq >> 16) & 0xff,
        3 /* data rate */
    };
    status = uc->handleMulticastControlCommand((uint8_t*)mc_start_header, sizeof(mc_start_header));
    if (status != LW_UC_OK) {
        printf("1) NOK - Start MC session failed, expected LW_UC_OK, but got %d\n", status);
        return 1;
    }

    uc->printHeapStats("HANDLE MC CONTROL ");

    printf("1) Instructed device with fragmentation session and Class C session - should start in 2 seconds\n");

    wait_ms(2100);

    if (!in_class_c) {
        printf("2) NOK - Did not switch to Class C after 2 seconds\n");
        return 1;
    }

    uc->printHeapStats("BEFORE PKTS ");

    // OK, start sending packets with 12.5% packet loss
    // Process the frames in the FAKE_PACKETS array
    for (size_t ix = 0; ix < sizeof(FAKE_PACKETS) / sizeof(FAKE_PACKETS[0]); ix++) {
        if (is_complete) break;

        if (!in_class_c) {
            printf("2) NOK - session is no longer in class C\n");
            return 1;
        }

        bool lose_packet = (rand() % 8) == 4;
        if (lose_packet) {
            printf("Lost frame %d\n", ix);
            continue;
        }

        status = uc->handleFragmentationCommand(0x1824aa3e, (uint8_t*)FAKE_PACKETS[ix], sizeof(FAKE_PACKETS[0]));

        if (status != LW_UC_OK) {
            printf("2) NOK - handleFragmentationCommand did not return LW_UC_OK, but %u\n", status);
            return 1;
        }

        printf("Processed frame %d\n", ix);
        uc->printHeapStats("PROCESS FRM ");

        if (is_complete) {
            break;
        }

        wait_ms(100); // @todo: this is really weird, writing these in quick succession leads to corrupt image... need to investigate.
    }

    if (!is_complete) {
        printf("2) NOK - frag session is not complete\n");
        return 1;
    }

    if (!is_fw_ready) {
        printf("2) NOK - firmware is not ready (validation failed?)\n");
        return 1;
    }

    if (in_class_c) {
        printf("2) NOK - device is still in Class C\n");
        return 1;
    }

    printf("2) OK\n");

    uc->printHeapStats("DONE ");

    delete uc;

    uc->printHeapStats("DELETED UC ");

    wait(osWaitForever);
}

#endif
