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

#include "mbed.h"
#include "packets.h"
#include "update_params.h"
#include "UpdateCerts.h"
#include "mbed_stats.h"
#include "arm_uc_metadata_header_v2.h"
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

// Print heap statistics
static void print_heap_stats(uint8_t prefix = 0) {
    mbed_stats_heap_t heap_stats;
    mbed_stats_heap_get(&heap_stats);

    if (prefix != 0) {
        printf("%d ", prefix);
    }
    printf("Heap stats: %d / %d (max=%d)\n", heap_stats.current_size, heap_stats.reserved_size, heap_stats.max_size);
}

static bool compare_buffers(uint8_t* buff1, const uint8_t* buff2, size_t size) {
    for (size_t ix = 0; ix < size; ix++) {
        if (buff1[ix] != buff2[ix]) return false;
    }
    return true;
}

static void print_buffer(void* buff, size_t size) {
    for (size_t ix = 0; ix < size; ix++) {
        printf("%02x ", ((uint8_t*)buff)[ix]);
    }
}

static bool is_complete = false;

static void fake_send_method(uint8_t port, uint8_t *data, size_t length) {
    printf("Sending %u bytes on port %u: ", length, port);
    for (size_t ix = 0; ix < length; ix++) {
        printf("%02x ", data[ix]);
    }
    printf("\n");
}

static void lorawan_uc_event_handler(LW_UC_EVENT event) {
    switch (event) {
        case LW_UC_EVENT_FRAGSESSION_COMPLETE:
            is_complete = true;
            break;

        case LW_UC_EVENT_FIRMWARE_READY:
            printf("Firmware is ready - resetting device to flash new firmware...\n");

#ifndef TARGET_SIMULATOR
            NVIC_SystemReset();
#endif
            break;
    }
}

int main() {

    LW_UC_STATUS status;

    uc.setEventCallback(&lorawan_uc_event_handler);

    status = uc.handleFragmentationCommand((uint8_t*)FAKE_PACKETS_HEADER, sizeof(FAKE_PACKETS_HEADER));
    if (status != LW_UC_OK) {
        printf("Could not parse header (%d)\n", status);
        return 1;
    }

    // Process the frames in the FAKE_PACKETS array
    for (size_t ix = 0; ix < sizeof(FAKE_PACKETS) / sizeof(FAKE_PACKETS[0]); ix++) {
        if (is_complete) break;

        status = uc.handleFragmentationCommand((uint8_t*)FAKE_PACKETS[ix], sizeof(FAKE_PACKETS[0]));

        if (status != LW_UC_OK) {
            printf("FragmentationSession process_frame failed: %u\n", status);
            return 1;
        }

        if (is_complete) {
            break;
        }

        printf("Processed frame %d\n", ix);

        wait_ms(50); // @todo: this is really weird, writing these in quick succession leads to corrupt image... need to investigate.
    }

    wait(osWaitForever);
}
