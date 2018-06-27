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
SimulatorBlockDevice bd("lorawan-frag-in-flash", 256 * 528, 528);
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

    // CRC64 hash validation...
    if (port == 201 && length == 10 && data[0] == 0x5) {
        is_complete = true;

        printf("FragmentationSession is complete\n");

        // This hash needs to be sent to the network to verify that the packet originated from the network
        if (compare_buffers((uint8_t*)&FAKE_PACKETS_CRC64_HASH, data + 2, 8)) {
            printf("CRC64 Hash verification OK (%08llx)\n", FAKE_PACKETS_CRC64_HASH);
        }
        else {
            printf("CRC64 Hash verification NOK, hash was %08llx, expected %08llx\n", ((uint64_t*)data + 2)[0], FAKE_PACKETS_CRC64_HASH);
        }

        // @todo, inject the OK/NOK packet.
    }
}

int main() {

    LW_UC_STATUS status;

    status = uc.handleFragmentationCommand((uint8_t*)FAKE_PACKETS_HEADER, sizeof(FAKE_PACKETS_HEADER));
    if (status != LW_UC_OK) {
        printf("Could not parse header (%d)\n", status);
        return 1;
    }

    // Process the frames in the FAKE_PACKETS array
    for (size_t ix = 0; ix < sizeof(FAKE_PACKETS) / sizeof(FAKE_PACKETS[0]); ix++) {
        status = uc.handleFragmentationCommand((uint8_t*)FAKE_PACKETS[ix], sizeof(FAKE_PACKETS[0]));

        if (status != LW_UC_OK) {
            printf("FragmentationSession process_frame failed: %s\n", status);
            return 1;
        }

        if (is_complete) {
            break;
        }

        printf("Processed frame %d\n", ix);

        wait_ms(50); // @todo: this is really weird, writing these in quick succession leads to corrupt image... need to investigate.
    }

    // // The data is now in flash. Free the fragSession
    // delete fragSession;

    // // Calculate the CRC of the data in flash to see if the file was unpacked correctly
    // uint64_t crc_res;
    // // To calculate the CRC on desktop see 'calculate-crc64/main.cpp'
    // {
    //     uint8_t crc_buffer[128];

    //     FragmentationCrc64 crc64(&fbd, crc_buffer, sizeof(crc_buffer));
    //     crc_res = crc64.calculate(opts.FlashOffset, (opts.NumberOfFragments * opts.FragmentSize) - opts.Padding);

    //     // This hash needs to be sent to the network to verify that the packet originated from the network
    //     if (FAKE_PACKETS_CRC64_HASH == crc_res) {
    //         printf("CRC64 Hash verification OK (%08llx)\n", crc_res);
    //     }
    //     else {
    //         printf("CRC64 Hash verification NOK, hash was %08llx, expected %08llx\n", crc_res, FAKE_PACKETS_CRC64_HASH);
    //         return 1;
    //     }
    // }

    // wait_ms(1);

    // // the signature is the last FOTA_SIGNATURE_LENGTH bytes of the package
    // size_t signatureOffset = opts.FlashOffset + ((opts.NumberOfFragments * opts.FragmentSize) - opts.Padding) - FOTA_SIGNATURE_LENGTH;

    // // Read out the header of the package...
    // UpdateSignature_t* header = new UpdateSignature_t();
    // fbd.read(header, signatureOffset, FOTA_SIGNATURE_LENGTH);

    // if (!compare_buffers(header->manufacturer_uuid, UPDATE_CERT_MANUFACTURER_UUID, 16)) {
    //     printf("Manufacturer UUID does not match\n");
    //     return 1;
    // }

    // if (!compare_buffers(header->device_class_uuid, UPDATE_CERT_DEVICE_CLASS_UUID, 16)) {
    //     printf("Device Class UUID does not match\n");
    //     return 1;
    // }

    // printf("Manufacturer and Device Class UUID match\n");

    // wait_ms(1);

    // // Calculate the SHA256 hash of the file, and then verify whether the signature was signed with a trusted private key
    // unsigned char sha_out_buffer[32];
    // {
    //     uint8_t sha_buffer[128];

    //     // SHA256 requires a large buffer, alloc on heap instead of stack
    //     FragmentationSha256* sha256 = new FragmentationSha256(&fbd, sha_buffer, sizeof(sha_buffer));

    //     // // The last FOTA_SIGNATURE_LENGTH bytes are reserved for the sig, so don't use it for calculating the SHA256 hash
    //     sha256->calculate(
    //         opts.FlashOffset,
    //         (opts.NumberOfFragments * opts.FragmentSize) - opts.Padding - FOTA_SIGNATURE_LENGTH,
    //         sha_out_buffer);

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

    // wait_ms(1);

    // free(header);

    // // Hash is matching, now write the header so the bootloader can flash the update
    // arm_uc_firmware_details_t details;
    // details.version = static_cast<uint64_t>(MBED_BUILD_TIMESTAMP) + 10; // should be timestamp that the fw was built, this is to get around this
    // details.size = (opts.NumberOfFragments * opts.FragmentSize) - opts.Padding - FOTA_SIGNATURE_LENGTH;
    // memcpy(details.hash, sha_out_buffer, 32); // SHA256 hash of the firmware
    // memset(details.campaign, 0, ARM_UC_GUID_SIZE); // todo, add campaign info
    // details.signatureSize = 0; // not sure what this is used for

    // uint8_t *fw_header_buff = (uint8_t*)malloc(ARM_UC_EXTERNAL_HEADER_SIZE_V2);
    // if (!fw_header_buff) {
    //     printf("Could not allocate %d bytes for header\n", ARM_UC_EXTERNAL_HEADER_SIZE_V2);
    //     return 1;
    // }

    // arm_uc_buffer_t buff = { ARM_UC_EXTERNAL_HEADER_SIZE_V2, ARM_UC_EXTERNAL_HEADER_SIZE_V2, fw_header_buff };

    // arm_uc_error_t err = arm_uc_create_external_header_v2(&details, &buff);

    // if (err.error != ERR_NONE) {
    //     printf("Failed to create external header (%d)\n", err.error);
    //     return 1;
    // }

    // int r = fbd.program(buff.ptr, MBED_CONF_APP_FRAGMENTATION_BOOTLOADER_HEADER_OFFSET, buff.size);
    // if (r != BD_ERROR_OK) {
    //     printf("Failed to program firmware header: %d bytes at address 0x%x\n", buff.size, MBED_CONF_APP_FRAGMENTATION_STORAGE_OFFSET);
    //     return 1;
    // }

    // printf("Stored the update parameters in flash on 0x%x. Reset the board to apply update.\n", MBED_CONF_APP_FRAGMENTATION_STORAGE_OFFSET);

    // bd.deinit();

    wait(osWaitForever);
}
