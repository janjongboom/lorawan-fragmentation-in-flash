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

#if PROGRAM == PROGRAM_DIFF_UPDATE

#include "mbed.h"
#include "packets_diff.h"
#include "UpdateCerts.h"
#include "LoRaWANUpdateClient.h"
#include "storage_helper.h"

// fwd declaration
static void fake_send_method(LoRaWANUpdateClientSendParams_t &params);

const uint8_t APP_KEY[16] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf };

LoRaWANUpdateClient uc(&bd, APP_KEY, fake_send_method);

// Print heap statistics
static void print_heap_stats(uint8_t prefix = 0) {
    mbed_stats_heap_t heap_stats;
    mbed_stats_heap_get(&heap_stats);

    if (prefix != 0) {
        printf("%d ", prefix);
    }
    printf("Heap stats: %d / %d (max=%d)\n", heap_stats.current_size, heap_stats.reserved_size, heap_stats.max_size);
}

static bool is_complete = false;

static void fake_send_method(LoRaWANUpdateClientSendParams_t &params) {
    printf("Sending %u bytes on port %u: ", params.length, params.port);
    for (size_t ix = 0; ix < params.length; ix++) {
        printf("%02x ", params.data[ix]);
    }
    printf("\n");
}

static void lorawan_uc_fragsession_complete() {
    is_complete = true;
}

static void lorawan_uc_firmware_ready() {
    printf("Firmware is ready - reset the device to flash new firmware...\n");
}

int main() {
    mbed_trace_init();
    mbed_trace_exclude_filters_set("QSPIF");

    // Copy data into slot 2
    {
        FragmentationBlockDeviceWrapper fbd(&bd);
        fbd.init();

        int r = fbd.program(SLOT2_DATA, MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT2_FW_ADDRESS, SLOT2_DATA_LENGTH);
        if (r != BD_ERROR_OK) {
            printf("Failed to put firmware in slot 2 (%d)\n", r);
            return 0;
        }

        printf("Putting old firmware in slot 2...\n");

        arm_uc_firmware_details_t details;

        // @todo: replace by real version?
        details.version = static_cast<uint64_t>(MBED_BUILD_TIMESTAMP); // should be timestamp that the fw was built, this is to get around this
        details.size = SLOT2_DATA_LENGTH;
        memcpy(details.hash, SLOT2_SHA256_HASH, 32); // SHA256 hash of the firmware
        memset(details.campaign, 0, ARM_UC_GUID_SIZE); // todo, add campaign info
        details.signatureSize = 0; // not sure what this is used for

        r = fbd.program(&details, MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT2_HEADER_ADDRESS, sizeof(arm_uc_firmware_details_t));
        if (r != BD_ERROR_OK) {
            printf("Failed to write slot 2 header (%d)\n", r);
            return 1;
        }
        printf("Programmed slot 2 header page\n");
        wait_ms(1);
    }


    LW_UC_STATUS status;

    uc.callbacks.fragSessionComplete = lorawan_uc_fragsession_complete;
    uc.callbacks.firmwareReady = lorawan_uc_firmware_ready;

    status = uc.handleFragmentationCommand(0x0, (uint8_t*)FAKE_PACKETS_HEADER, sizeof(FAKE_PACKETS_HEADER));
    if (status != LW_UC_OK) {
        printf("Could not parse header (%d)\n", status);
        return 1;
    }

    // Process the frames in the FAKE_PACKETS array
    for (size_t ix = 0; ix < sizeof(FAKE_PACKETS) / sizeof(FAKE_PACKETS[0]); ix++) {
        if (is_complete) break;

        status = uc.handleFragmentationCommand(0x0, (uint8_t*)FAKE_PACKETS[ix], sizeof(FAKE_PACKETS[0]));

        if (status != LW_UC_OK) {
            printf("FragmentationSession process_frame failed: %u\n", status);
            return 1;
        }

        if (is_complete) {
            break;
        }

        printf("Processed frame %d\n", ix);

#ifdef TARGET_FF1705_L151CC
        wait_ms(50); // @todo: this is really weird, writing these in quick succession leads to corrupt image... need to investigate.
#endif
    }

    wait(osWaitForever);
}

#endif
