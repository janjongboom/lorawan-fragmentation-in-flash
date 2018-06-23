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
#include "UpdateCerts.h"
#include "mbed_debug.h"
#include "mbed_stats.h"
#include "arm_uc_metadata_header_v2.h"
#include "swap_int.h"

#ifdef TARGET_SIMULATOR
// Initialize a persistent block device with 512 bytes block size, and 256 blocks (128K of storage)
#include "SimulatorBlockDevice.h"
SimulatorBlockDevice bd("lorawan-frag-in-flash", 256 * 512, 512);
#else
// Flash interface on the L-TEK xDot shield
#include "AT45BlockDevice.h"
AT45BlockDevice bd(SPI_MOSI, SPI_MISO, SPI_SCK, SPI_NSS);
#endif

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
    // Wrap the block device to allow for unaligned reads/writes
    FragmentationBlockDeviceWrapper fbd(&bd);

    int bd_init;
    if ((bd_init = fbd.init()) != BD_ERROR_OK) {
        debug("Failed to initialize BlockDevice (%d)\n", bd_init);
        return 1;
    }

    // This data is normally obtained from the FragSessionSetupReq
    // comment out fragments in packets.h to simulate packet loss
    FragmentationSessionOpts_t opts;
    opts.NumberOfFragments = (FAKE_PACKETS_HEADER[3] << 8) + FAKE_PACKETS_HEADER[2];
    opts.FragmentSize = FAKE_PACKETS_HEADER[4];
    opts.Padding = FAKE_PACKETS_HEADER[6];
    opts.RedundancyPackets = (sizeof(FAKE_PACKETS) / sizeof(FAKE_PACKETS[0])) - opts.NumberOfFragments;
    // reserve page with offset 0x0 for the header
    opts.FlashOffset = MBED_CONF_APP_FRAGMENTATION_STORAGE_OFFSET + sizeof(arm_uc_external_header_t);

    FragResult result;

    // Declare the fragSession on the heap so we can free() it when CRC'ing the result in flash
    FragmentationSession* fragSession = new FragmentationSession(&fbd, opts);

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
        wait_ms(1);
    }

    // The data is now in flash. Free the fragSession
    delete fragSession;

    // Calculate the CRC of the data in flash to see if the file was unpacked correctly
    uint64_t crc_res;
    // To calculate the CRC on desktop see 'calculate-crc64/main.cpp'
    {
        uint8_t crc_buffer[128];

        FragmentationCrc64 crc64(&fbd, crc_buffer, sizeof(crc_buffer));
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

    wait_ms(1);

    // Read out the header of the package...
    UpdateSignature_t* header = new UpdateSignature_t();
    fbd.read(header, opts.FlashOffset, FOTA_SIGNATURE_LENGTH);

    if (!compare_buffers(header->manufacturer_uuid, UPDATE_CERT_MANUFACTURER_UUID, 16)) {
        debug("Manufacturer UUID does not match\n");
        return 1;
    }

    if (!compare_buffers(header->device_class_uuid, UPDATE_CERT_DEVICE_CLASS_UUID, 16)) {
        debug("Device Class UUID does not match\n");
        return 1;
    }

    debug("Manufacturer and Device Class UUID match\n");

    wait_ms(1);

    // Calculate the SHA256 hash of the file, and then verify whether the signature was signed with a trusted private key
    unsigned char sha_out_buffer[32];
    {
        uint8_t sha_buffer[128];

        // SHA256 requires a large buffer, alloc on heap instead of stack
        FragmentationSha256* sha256 = new FragmentationSha256(&fbd, sha_buffer, sizeof(sha_buffer));

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
            // FragmentationEcdsaVerify* ecdsa = new FragmentationEcdsaVerify(UPDATE_CERT_PUBKEY, UPDATE_CERT_LENGTH);
            // bool valid = ecdsa->verify(sha_out_buffer, header->signature, header->signature_length);
            // if (!valid) {
            //     debug("ECDSA verification of firmware failed\n");
            //     return 1;
            // }
            // else {
            //     debug("ECDSA verification OK\n");
            // }
        }
    }

    wait_ms(1);

    free(header);

    uint8_t sha512hash[] = { 0x9d, 0xb3, 0xe7, 0x3b, 0x06, 0x25, 0x8b, 0x84, 0xd6, 0x4f, 0x0b, 0x4b,
                             0x95, 0x90, 0x45, 0x37, 0xa2, 0x9f, 0x53, 0xa9, 0x5b, 0x33, 0x08, 0xcb,
                             0xce, 0xc4, 0xa6, 0x6d, 0x6a, 0x34, 0xc4, 0x81, 0x7f, 0x50, 0xec, 0xfb,
                             0xee, 0xff, 0x4c, 0x49, 0x9d, 0x56, 0x32, 0xe5, 0x26, 0x98, 0x7e, 0x00,
                             0x0a, 0x65, 0xe1, 0x2d, 0x9a, 0xf1, 0xa1, 0xde, 0x66, 0x94, 0x90, 0xda,
                             0xbd, 0x68, 0x03, 0x2a };

    // Hash is matching, now populate the FOTA_INFO_PAGE with information about the update, so the bootloader can flash the update
    arm_uc_external_header_t uc_header;
    uc_header.headerMagic = swap_uint32(ARM_UC_EXTERNAL_HEADER_MAGIC_V2);
    uc_header.headerVersion = swap_uint32(ARM_UC_EXTERNAL_HEADER_VERSION_V2);
    uc_header.firmwareVersion = swap_uint64(0xffffffffffffffff); // should be timestamp but OK, we're just testing
    uc_header.firmwareSize = swap_uint64((opts.NumberOfFragments * opts.FragmentSize) - opts.Padding);
    memcpy(uc_header.firmwareHash, sha512hash, sizeof(sha512hash));
    uc_header.firmwareTransformationMode = 0;
    uc_header.firmwareSignatureSize = 0;
    uc_header.payloadSize = 0;

    printf("sizeof thingy is %d\n", sizeof(arm_uc_external_header_t));

    fbd.program(&uc_header, MBED_CONF_APP_FRAGMENTATION_STORAGE_OFFSET, sizeof(arm_uc_external_header_t));

    uint8_t *poep = (uint8_t*)malloc(528);
    bd.read(poep, 0x0, 528);
    print_buffer(poep, 528);

    debug("Stored the update parameters in flash on 0x%x. Reset the board to apply update.\n", MBED_CONF_APP_FRAGMENTATION_STORAGE_OFFSET);

    bd.deinit();


    /*
// typedef struct _arm_uc_external_header_t
// {
//     /* Metadata-header specific magic code */
//     uint32_t headerMagic;

//     /* Revision number for metadata header. */
//     uint32_t headerVersion;

//     /* Version number accompanying the firmware. Larger numbers imply more
//        recent and preferred versions. This is used for determining the
//        selection order when multiple versions are available. For downloaded
//        firmware the manifest timestamp is used as the firmware version.
//     */
//     uint64_t firmwareVersion;

//     /* Total space (in bytes) occupied by the firmware BLOB. */
//     uint64_t firmwareSize;

//     /* Firmware hash calculated over the firmware size. Should match the hash
//        generated by standard command line tools, e.g., shasum on Linux/Mac.
//     */
//     uint8_t firmwareHash[ARM_UC_SHA512_SIZE];

//     /* Total space (in bytes) occupied by the payload BLOB.
//        The payload is the firmware after some form of transformation like
//        encryption and/or compression.
//     */
//     uint64_t payloadSize;

//     /* Payload hash calculated over the payload size. Should match the hash
//        generated by standard command line tools, e.g., shasum on Linux/Mac.
//        The payload is the firmware after some form of transformation like
//        encryption and/or compression.
//     */
//     uint8_t payloadHash[ARM_UC_SHA512_SIZE];

//     /* The ID for the update campaign that resulted in the firmware update.
//     */
//     uint8_t campaign[ARM_UC_GUID_SIZE];

//     /* Type of transformation used to turn the payload into the firmware image.
//        Possible values are:
//      * * NONE
//      * * AES128_CTR
//      * * AES128_CBC
//      * * AES256_CTR
//      * * AES256_CBC
//      */
//     uint32_t firmwareTransformationMode;

//     /* Encrypted firmware encryption key.
//      * To decrypt the firmware, the bootloader combines the bootloader secret
//      * and the firmwareKeyDerivationFunctionSeed to create an AES key. It uses
//      * This AES key to decrypt the firmwareCipherKey. The decrypted
//      * firmwareCipherKey is the FirmwareKey, which is used with the
//      * firmwareInitVector to decrypt the firmware.
//      */
//     uint8_t firmwareCipherKey[ARM_UC_AES256_KEY_SIZE];

//     /* AES Initialization vector. This is a random number used to protect the
//        encryption algorithm from attack. It must be unique for every firmware.
//      */
//     uint8_t firmwareInitVector[ARM_UC_AES_BLOCK_SIZE];

//     /* Size of the firmware signature. Must be 0 if no signature is supplied. */
//     uint32_t firmwareSignatureSize;

//     /* Hash based message authentication code for the metadata header. Uses per
//        device secret as key. Should use same hash algorithm as firmware hash.
//        The headerHMAC field and firmwareSignature field are not part of the hash.
//     */
//     uint8_t headerHMAC[ARM_UC_SHA512_SIZE];

//     /* Optional firmware signature. Hashing algorithm should be the same as the
//        one used for the firmware hash. The firmwareSignatureSize must be set.
//     */
//     uint8_t firmwareSignature[0];
// } arm_uc_external_header_t;



    // UpdateParams_t update_params;
    // update_params.update_pending = 1;
    // update_params.size = (opts.NumberOfFragments * opts.FragmentSize) - opts.Padding - FOTA_SIGNATURE_LENGTH;
    // update_params.offset = opts.FlashOffset + FOTA_SIGNATURE_LENGTH;
    // update_params.signature = UpdateParams_t::MAGIC;
    // memcpy(update_params.sha256_hash, sha_out_buffer, sizeof(sha_out_buffer));
    // // bd.program(&update_params, FOTA_INFO_PAGE * bd.get_read_size(), sizeof(UpdateParams_t));

    wait(osWaitForever);
}
