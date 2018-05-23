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
#include "update_params.h"
#include "CopyPartOfFile.h"
#include "mbed_debug.h"
#include "mbed_stats.h"
#include "UpdateClient.h"

extern void ARM_UCS_FwDone(uint8_t*, size_t);

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
        printf("%02x", ((uint8_t*)buff)[ix]);
    }
}


void uc_error(int32_t err) {
    printf("uc_error %d\n", err);
}

void uc_authorize(int32_t request) {
    printf("uc_authorize %d\n", request);
    UpdateClient::update_authorize(request);
}

int main() {
    // Flash interface on the L-TEK xDot shield
    // AT45BlockDevice at45;
    // int at45_init;
    // if ((at45_init = at45.init()) != BD_ERROR_OK) {
    //     printf("Failed to initialize AT45BlockDevice (%d)\n", at45_init);
    //     return 1;
    // }

    mbed_trace_init();

#if 0
    FILE *file = fopen("/fragsession.raw", "wb+");
    if (!file) {
        printf("Could not open fragsession.raw\n");
        return -1;
    }

    // This data is normally obtained from the FragSessionSetupReq
    // comment out fragments in packets.h to simulate packet loss
    FragmentationSessionOpts_t opts;
    opts.NumberOfFragments = (FAKE_PACKETS_HEADER[3] << 8) + FAKE_PACKETS_HEADER[2];
    opts.FragmentSize = FAKE_PACKETS_HEADER[4];
    opts.Padding = FAKE_PACKETS_HEADER[6];
    opts.RedundancyPackets = (sizeof(FAKE_PACKETS) / sizeof(FAKE_PACKETS[0])) - opts.NumberOfFragments;

    size_t totalSize = (opts.NumberOfFragments * opts.FragmentSize) - opts.Padding;

    FragResult result;

    // Declare the fragSession on the heap so we can free() it when CRC'ing the result in flash
    FragmentationSession* fragSession = new FragmentationSession(file, opts);

    if ((result = fragSession->initialize()) != FRAG_OK) {
        printf("FragmentationSession initialize failed: %s\n", FragmentationSession::frag_result_string(result));
        return 1;
    }

    // Process the frames in the FAKE_PACKETS array
    for (size_t ix = 0; ix < sizeof(FAKE_PACKETS) / sizeof(FAKE_PACKETS[0]); ix++) {
        uint8_t* buffer = (uint8_t*)FAKE_PACKETS[ix];
        uint16_t frameCounter = (buffer[2] << 8) + buffer[1];

        // Skip the first 3 bytes, as they contain metadata
        if ((result = fragSession->process_frame(frameCounter, buffer + 3, sizeof(FAKE_PACKETS[0]) - 3)) != FRAG_OK) {
            if (result == FRAG_COMPLETE) {
                printf("FragmentationSession is complete at frame %d\n", frameCounter);
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
    uint64_t crc_res;
    // To calculate the CRC on desktop see 'calculate-crc64/main.cpp'
    {
        uint8_t crc_buffer[128];

        FragmentationCrc64 crc64(file, crc_buffer, sizeof(crc_buffer));
        crc_res = crc64.calculate(0, totalSize);

        // This hash needs to be sent to the network to verify that the packet originated from the network
        if (FAKE_PACKETS_CRC64_HASH == crc_res) {
            printf("CRC64 Hash verification OK (%08llx)\n", crc_res);
        }
        else {
            printf("CRC64 Hash verification NOK, hash was %08llx, expected %08llx\n", crc_res, FAKE_PACKETS_CRC64_HASH);
            return 1;
        }
    }

    // Read out the header of the package...
    UpdateSignature_t header;
    fseek(file, 0, SEEK_SET);
    fread(&header, 1, FOTA_SIGNATURE_LENGTH, file);

    uint8_t *manifest = (uint8_t*)malloc(header.manifest_size);
    if (!manifest) {
        printf("Could not alloc manifest size\n");
        return 1;
    }
    fseek(file, FOTA_SIGNATURE_LENGTH, SEEK_SET);
    fread(manifest, 1, header.manifest_size, file);

    // And copy over the manifest and the binary to separate locations...
    printf("Manifest size is %u\n", header.manifest_size);
    {
        uint8_t copy_buffer[128];

        int r = copy_part_of_file(file, "/fragsession.firmware", FOTA_SIGNATURE_LENGTH + header.manifest_size,
                            totalSize - header.manifest_size - FOTA_SIGNATURE_LENGTH, copy_buffer, sizeof(copy_buffer));
        if (r != 0) {
            printf("Could not copy into fragsession.firmware\n");
            return -1;
        }
    }

    // // Calculate the SHA256 hash of the file, and then verify whether the signature was signed with a trusted private key
    // unsigned char sha_out_buffer[32];
    // {
    //     uint8_t sha_buffer[128];

    //     // SHA256 requires a large buffer, alloc on heap instead of stack
    //     FragmentationSha256* sha256 = new FragmentationSha256(file, sha_buffer, sizeof(sha_buffer));

    //     // // The first FOTA_SIGNATURE_LENGTH + manifest_size bytes are reserved for the sig, so don't use it for calculating the SHA256 hash
    //     sha256->calculate(FOTA_SIGNATURE_LENGTH + header->manifest_size, totalSize - FOTA_SIGNATURE_LENGTH, sha_out_buffer);

    //     printf("SHA256 hash is: ");
    //     for (size_t ix = 0; ix < 32; ix++) {
    //         printf("%02x", sha_out_buffer[ix]);
    //     }
    //     printf("\n");

    //     // now check that the signature is correct...
    //     {
    //         printf("ECDSA signature is: ");
    //         for (size_t ix = 0; ix < header->signature_length; ix++) {
    //             printf("%02x", header->signature[ix]);
    //         }
    //         printf("\n");
    //         printf("Verifying signature...\n");

    //         // ECDSA requires a large buffer, alloc on heap instead of stack
    //         FragmentationEcdsaVerify* ecdsa = new FragmentationEcdsaVerify(UPDATE_CERT_PUBKEY, UPDATE_CERT_LENGTH);
    //         bool valid = ecdsa->verify(sha_out_buffer, header->signature, header->signature_length);
    //         if (!valid) {
    //             printf("ECDSA verification of firmware failed\n");
    //             return 1;
    //         }
    //         else {
    //             printf("ECDSA verification OK\n");
    //         }
    //     }
    // }

    // Hash is matching, now populate the FOTA_INFO_PAGE with information about the update, so the bootloader can flash the update
    // UpdateParams_t update_params;
    // update_params.update_pending = 1;
    // update_params.size = totalSize - FOTA_SIGNATURE_LENGTH;
    // update_params.offset = opts.FlashOffset + FOTA_SIGNATURE_LENGTH;
    // update_params.signature = UpdateParams_t::MAGIC;
    // memcpy(update_params.sha256_hash, sha_out_buffer, sizeof(sha_out_buffer));
    // at45.program(&update_params, FOTA_INFO_PAGE * at45.get_read_size(), sizeof(UpdateParams_t));

    printf("Stored the update parameters in flash on page 0x%x. Reset the board to apply update.\n", FOTA_INFO_PAGE);
#endif

    UpdateClient::set_update_authorize_handler(&uc_authorize);

    UpdateClient::event_handler(UpdateClient::UPDATE_CLIENT_EVENT_INITIALIZE);

    uint8_t manifest[] = {
        0x30 ,0x82 ,0x01 ,0x26 ,0x30 ,0x81 ,0x89 ,0x0A ,0x01 ,0x00 ,0x30 ,0x81 ,0x83 ,0x0A ,0x01 ,0x01 ,0x02 ,0x04 ,0x5B ,0x05 ,0x3E ,0x9B ,0x04 ,0x10 ,0xFA ,0x6B ,0x4A ,0x53 ,0xD5 ,0xAD ,0x5F ,0xDF ,0xBE ,0x9D ,0xE6 ,0x63 ,0xE4 ,0xD4 ,0x1F ,0xFE ,0x04 ,0x10 ,0x71 ,0x9C ,0xBD ,0xC8 ,0x1C ,0xEC ,0x5A ,0x37 ,0xA6 ,0x55 ,0x3D ,0x7B ,0x72 ,0x8B ,0x8A ,0x9C ,0x04 ,0x00 ,0x04 ,0x10 ,0x32 ,0xB4 ,0x01 ,0xF6 ,0x0D ,0x5B ,0x68 ,0x16 ,0xBC ,0x91 ,0xFA ,0x4A ,0x71 ,0x60 ,0x71 ,0x18 ,0x04 ,0x00 ,0x01 ,0x01 ,0xFF ,0x0A ,0x01 ,0x02 ,0x30 ,0x00 ,0x30 ,0x00 ,0x30 ,0x34 ,0x0A ,0x01 ,0x01 ,0x0C ,0x07 ,0x64 ,0x65 ,0x66 ,0x61 ,0x75 ,0x6C ,0x74 ,0x30 ,0x26 ,0x04 ,0x20 ,0x98 ,0x88 ,0x4C ,0x52 ,0x0D ,0xCF ,0x8B ,0x5C ,0xF0 ,0x30 ,0xFE ,0x63 ,0xF9 ,0xFA ,0x71 ,0xC5 ,0x94 ,0x8B ,0x12 ,0xE0 ,0x26 ,0x5E ,0x6F ,0x47 ,0xB1 ,0x86 ,0xCC ,0xF4 ,0xC4 ,0xAD ,0x44 ,0x0E ,0x02 ,0x02 ,0x31 ,0xC8 ,0x30 ,0x81 ,0x97 ,0x04 ,0x20 ,0x33 ,0x40 ,0xA1 ,0x5F ,0x65 ,0x2C ,0xC7 ,0x06 ,0xCF ,0x40 ,0xD9 ,0xE0 ,0x06 ,0x0E ,0x99 ,0x75 ,0x8B ,0x73 ,0xA9 ,0x25 ,0x15 ,0x28 ,0xC8 ,0x92 ,0xF8 ,0xB9 ,0x95 ,0x35 ,0x4B ,0xFE ,0x79 ,0xC4 ,0x30 ,0x73 ,0x30 ,0x71 ,0x04 ,0x47 ,0x30 ,0x45 ,0x02 ,0x20 ,0x1F ,0x95 ,0xBE ,0x18 ,0xE0 ,0x6D ,0xF6 ,0xED ,0xAB ,0xDB ,0xF8 ,0xB9 ,0x3E ,0x24 ,0x61 ,0x4C ,0xA7 ,0x59 ,0x9F ,0x89 ,0xCE ,0x30 ,0xEA ,0x79 ,0x9B ,0xF4 ,0xE4 ,0xAE ,0xF4 ,0xC0 ,0x3C ,0x6C ,0x02 ,0x21 ,0x00 ,0xE0 ,0x28 ,0x68 ,0xE7 ,0x57 ,0x4C ,0x2B ,0xE6 ,0xFE ,0xA0 ,0x72 ,0x9D ,0x2C ,0x2A ,0x9E ,0xAB ,0x1B ,0xE9 ,0x2A ,0x03 ,0xFB ,0xA6 ,0xA5 ,0xCD ,0x93 ,0xAC ,0x98 ,0xE5 ,0x2F ,0x8B ,0x19 ,0x93 ,0x30 ,0x26 ,0x30 ,0x24 ,0x04 ,0x20 ,0xD2 ,0x3B ,0xB5 ,0xE4 ,0x1D ,0xDA ,0xBB ,0x6A ,0x50 ,0xB9 ,0x70 ,0x9B ,0xB4 ,0x7A ,0x51 ,0x53 ,0xB0 ,0x7A ,0x4A ,0xBF ,0x01 ,0xB0 ,0x7A ,0x68 ,0xF5 ,0xD3 ,0xA8 ,0x81 ,0x61 ,0x5D ,0x94 ,0x07 ,0x0C ,0x00
    };
    size_t manifest_size = sizeof(manifest);

    wait_ms(1000);

    ARM_UCS_FwDone(manifest, /*header.*/manifest_size);

    printf("Done processhash\n");

    wait(0xffffffff);
}
