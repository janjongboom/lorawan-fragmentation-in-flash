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
#include "AT45BlockDevice.h"
#include "FragmentationSession.h"
#include "FragmentationCrc64.h"
#include "UpdateParameters.h"
#include "mbed_debug.h"

// These values need to be the same between target application and bootloader!
#define     FOTA_INFO_PAGE         0x1800    // The information page for the firmware update
#define     FOTA_UPDATE_PAGE       0x1801    // The update starts at this page (and then continues)

Serial pc(USBTX, USBRX);

int main() {
    pc.baud(9600);

    // Flash interface on the L-TEK xDot shield
    AT45BlockDevice at45;
    int at45_init;
    if ((at45_init = at45.init()) != BD_ERROR_OK) {
        debug("Failed to initialize AT45BlockDevice (%d)\n", at45_init);
        return 1;
    }

    // This data is normally obtained from the FragSessionSetupReq
    // comment out fragments in packets.h to simulate packet loss
    FragmentationSessionOpts_t opts;
    opts.NumberOfFragments = (FAKE_PACKETS_HEADER[3] << 8) + FAKE_PACKETS_HEADER[2];
    opts.FragmentSize = FAKE_PACKETS_HEADER[4];
    opts.Padding = FAKE_PACKETS_HEADER[6];
    opts.RedundancyPackets = (sizeof(FAKE_PACKETS) / sizeof(FAKE_PACKETS[0])) - opts.NumberOfFragments;
    opts.FlashOffset = FOTA_UPDATE_PAGE * at45.get_read_size();

    FragResult result;

    // Declare the fragSession on the heap so we can free() it when CRC'ing the result in flash
    FragmentationSession* fragSession = new FragmentationSession(&at45, opts);

    // with 26 packets, 204 size, 25 padding, 10 redundancy, we use 322 bytes of heap space
    if ((result = fragSession->initialize()) != FRAG_OK) {
        debug("FragmentationSession initialize failed: %s\n", FragmentationSession::frag_result_string(result));
        return 1;
    }

    // Process the frames in the FAKE_PACKETS array
    for (size_t ix = 0; ix < sizeof(FAKE_PACKETS) / sizeof(FAKE_PACKETS[0]); ix++) {
        uint8_t* buffer = FAKE_PACKETS[ix];
        uint16_t frameCounter = (buffer[2] << 8) + buffer[1];

        // Skip the first 3 bytes, as they contain metadata
        if ((result = fragSession->process_frame(frameCounter, buffer + 3, sizeof(FAKE_PACKETS[0]) - 3)) != FRAG_OK) {
            if (result == FRAG_COMPLETE) {
                debug("FragmentationSession is complete at frame %d\n", frameCounter);
                break;
            }
            else {
                debug("FragmentationSession process_frame %d failed: %s\n",
                    frameCounter, FragmentationSession::frag_result_string(result));
                return 1;
            }
        }

        debug("Processed frame with frame counter %d\n", frameCounter);
    }

    // The data is now in flash. Free the fragSession
    delete fragSession;

    // Calculate the CRC of the data in flash to see if the file was unpacked correctly
    // To calculate the CRC on desktop see 'calculate-crc64/main.cpp'
    uint8_t crc_buffer[128];

    FragmentationCrc64 crc64(&at45, crc_buffer, sizeof(crc_buffer));
    uint64_t crc_res = crc64.calculate(opts.FlashOffset, (opts.NumberOfFragments * opts.FragmentSize) - opts.Padding);

    if (FAKE_PACKETS_HASH == crc_res) {
        debug("Hash verification OK (%08llx)\n", crc_res);
    }
    else {
        debug("Hash verification NOK, hash was %08llx, expected %08llx\n", crc_res, FAKE_PACKETS_HASH);
        return 1;
    }

    // Hash is matching, now populate the FOTA_INFO_PAGE with information about the update, so the bootloader can flash the update
    UpdateParams_t update_params;
    update_params.update_pending = 1;
    update_params.size = (opts.NumberOfFragments * opts.FragmentSize) - opts.Padding;
    update_params.signature = UpdateParams_t::MAGIC;
    update_params.hash = crc_res;
    at45.program(&update_params, FOTA_INFO_PAGE * at45.get_read_size(), sizeof(UpdateParams_t));

    debug("Stored the update parameters in flash on page 0x%x. Reset the board to apply update.\n", FOTA_INFO_PAGE);

    wait(osWaitForever);
}
