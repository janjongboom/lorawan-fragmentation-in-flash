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

#include "mbed_memory_status.h"
#include "packets.h"
#include "AT45Flash.h"
#include "FragmentationSession.h"
#include "FragmentationCrc64.h"

Serial pc(USBTX, USBRX);

/**
 * To see memory stats, add:
 *    print_all_thread_info();
 *    print_heap_and_isr_stack_info();
 */

int main() {
    pc.baud(115200);

    // Flash interface on the L-TEK xDot shield
    AT45Flash at45;

    // This data is normally obtained from the FragSessionSetupReq
    FragmentationSessionOpts_t opts;
    opts.NumberOfFragments = 26;
    opts.FragmentSize = 204;
    opts.Padding = 184;
    opts.RedundancyPackets = 10;
    // in total there are 26 packets + max. 10 error correction packets, packets 5, 6 and 12 are missing...

    FragResult result;

    // Declare the fragSession on the heap so we can free() it when CRC'ing the result in flash
    FragmentationSession* fragSession = new FragmentationSession(&at45, opts);

    // with these vars (26 packets, 204 size, 25 padding, 10 redundancy) we use 322 bytes of heap space
    if ((result = fragSession->initialize()) != FRAG_OK) {
        printf("FragmentationSession initialize failed: %s\n", FragmentationSession::frag_result_string(result));
        return 1;
    }

    // Process the frames in the FAKE_PACKETS array
    for (size_t ix = 0; ix < sizeof(FAKE_PACKETS) / sizeof(FAKE_PACKETS[0]); ix++) {
        uint8_t* buffer = FAKE_PACKETS[ix];
        // I think this is still the old format, so needs to be fixed (1 and 2 are swapped around)
        uint16_t frameCounter = (buffer[1] << 8) + buffer[2];

        // Skip the first 3 bytes, as they contain metadata
        if ((result = fragSession->process_frame(frameCounter, buffer + 3, sizeof(FAKE_PACKETS[0]) - 3)) != FRAG_OK) {
            if (result == FRAG_COMPLETE) {
                printf("FragmentationSession is complete at frame %d\n", ix);
                break;
            }
            else {
                printf("FragmentationSession process_frame %d failed: %s\n",
                    frameCounter, FragmentationSession::frag_result_string(result));
                return 1;
            }
        }

        printf("Processed frame with frame counter %d\n", frameCounter);
    }

    // The data is now in flash. Free the fragSession
    delete fragSession;

    // Calculate the CRC of the data in flash to see if the file was unpacked correctly
    // CRC64 of the original file is 150eff2bcd891e18 (see fake-fw/test-crc64/main.cpp)
    uint8_t crc_buffer[128];

    FragmentationCrc64 crc64(&at45, crc_buffer, sizeof(crc_buffer));
    uint64_t crc_res = crc64.calculate(0, (opts.NumberOfFragments * opts.FragmentSize) - opts.Padding);

    printf("Expected %08llx, hash was %08llx, success=%d\n", 0x150eff2bcd891e18, crc_res, 0x150eff2bcd891e18 == crc_res);

    return 0;
}
