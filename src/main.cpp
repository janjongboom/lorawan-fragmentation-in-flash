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
#include "mbed_lorawan_frag_lib.h"
#include "packets.h"
#include "AT45BlockDevice.h"
#include "update_params.h"
#include "update_certs.h"
#include "mbed_debug.h"
#include "mbed_stats.h"

Serial pc(USBTX, USBRX);

// Print heap statistics
static void print_heap_stats(uint8_t prefix = 0) {
    mbed_stats_heap_t heap_stats;
    mbed_stats_heap_get(&heap_stats);

    if (prefix != 0) {
        debug("%d ", prefix);
    }
    debug("Heap stats: %d / %d (max=%d)\n", heap_stats.current_size, heap_stats.reserved_size, heap_stats.max_size);
}

static bool compare_buffers(uint8_t* buff1, const uint8_t* buff2, size_t size) {
    for (size_t ix = 0; ix < size; ix++) {
        if (buff1[ix] != buff2[ix]) return false;
    }
    return true;
}

static void print_buffer(void* buff, size_t size) {
    for (size_t ix = 0; ix < size; ix++) {
        debug("%02x", ((uint8_t*)buff)[ix]);
    }
}

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

    if ((result = fragSession->initialize()) != FRAG_OK) {
        debug("FragmentationSession initialize failed: %s\n", FragmentationSession::frag_result_string(result));
        return 1;
    }

    // Process the frames in the FAKE_PACKETS array
    for (size_t ix = 0; ix < sizeof(FAKE_PACKETS) / sizeof(FAKE_PACKETS[0]); ix++) {
        uint8_t* buffer = (uint8_t*)FAKE_PACKETS[ix];
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
    uint64_t crc_res;
    // To calculate the CRC on desktop see 'calculate-crc64/main.cpp'
    {
        uint8_t crc_buffer[128];

        FragmentationCrc64 crc64(&at45, crc_buffer, sizeof(crc_buffer));
        crc_res = crc64.calculate(opts.FlashOffset, (opts.NumberOfFragments * opts.FragmentSize) - opts.Padding);

        // This hash needs to be sent to the network to verify that the packet originated from the network
        if (FAKE_PACKETS_CRC64_HASH == crc_res) {
            debug("CRC64 Hash verification OK (%08llx)\n", crc_res);
        }
        else {
            debug("CRC64 Hash verification NOK, hash was %08llx, expected %08llx\n", crc_res, FAKE_PACKETS_CRC64_HASH);
            return 1;
        }
    }

    // Read out the header of the package...
    UpdateSignature_t* header = new UpdateSignature_t();
    at45.read(header, opts.FlashOffset, FOTA_SIGNATURE_LENGTH);

    if (!compare_buffers(header->manufacturer_uuid, UPDATE_CERT_MANUFACTURER_UUID, 16)) {
        debug("Manufacturer UUID does not match\n");
        return 1;
    }

    if (!compare_buffers(header->device_class_uuid, UPDATE_CERT_DEVICE_CLASS_UUID, 16)) {
        debug("Manufacturer UUID does not match\n");
        return 1;
    }

    // Calculate the SHA256 hash of the file, and then verify whether the signature was signed with a trusted private key
    unsigned char sha_out_buffer[32];
    {
        uint8_t sha_buffer[128];

        // SHA256 requires a large buffer, alloc on heap instead of stack
        FragmentationSha256* sha256 = new FragmentationSha256(&at45, sha_buffer, sizeof(sha_buffer));

        // // The first FOTA_SIGNATURE_LENGTH bytes are reserved for the sig, so don't use it for calculating the SHA256 hash
        sha256->calculate(
            opts.FlashOffset + FOTA_SIGNATURE_LENGTH,
            (opts.NumberOfFragments * opts.FragmentSize) - opts.Padding - FOTA_SIGNATURE_LENGTH,
            sha_out_buffer);

        debug("SHA256 hash is: ");
        for (size_t ix = 0; ix < 32; ix++) {
            debug("%02x", sha_out_buffer[ix]);
        }
        debug("\n");

        // now check that the signature is correct...
        {
            debug("ECDSA signature is: ");
            for (size_t ix = 0; ix < header->signature_length; ix++) {
                debug("%02x", header->signature[ix]);
            }
            debug("\n");
            debug("Verifying signature...\n");

            // ECDSA requires a large buffer, alloc on heap instead of stack
            FragmentationEcdsaVerify* ecdsa = new FragmentationEcdsaVerify(UPDATE_CERT_PUBKEY, UPDATE_CERT_LENGTH);
            bool valid = ecdsa->verify(sha_out_buffer, header->signature, header->signature_length);
            if (!valid) {
                debug("ECDSA verification of firmware failed\n");
                return 1;
            }
            else {
                debug("ECDSA verification OK\n");
            }
        }
    }

    free(header);

    // Hash is matching, now populate the FOTA_INFO_PAGE with information about the update, so the bootloader can flash the update
    UpdateParams_t update_params;
    update_params.update_pending = 1;
    update_params.size = (opts.NumberOfFragments * opts.FragmentSize) - opts.Padding - FOTA_SIGNATURE_LENGTH;
    update_params.offset = opts.FlashOffset + FOTA_SIGNATURE_LENGTH;
    update_params.signature = UpdateParams_t::MAGIC;
    memcpy(update_params.sha256_hash, sha_out_buffer, sizeof(sha_out_buffer));
    at45.program(&update_params, FOTA_INFO_PAGE * at45.get_read_size(), sizeof(UpdateParams_t));

    debug("Stored the update parameters in flash on page 0x%x. Reset the board to apply update.\n", FOTA_INFO_PAGE);

    wait(osWaitForever);
}
