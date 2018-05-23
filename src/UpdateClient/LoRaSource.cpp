// ----------------------------------------------------------------------------
// Copyright 2016-2017 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------

#include "LoRaSource.h"

#include "mbed_trace.h"
#define TRACE_GROUP "LSRC"

#include <time.h>
#include <stdio.h>
#include <stdbool.h>

#define ARM_UCS_DEFAULT_COST (1000)
#define ARM_UCS_HASH_LENGTH  (40)

typedef struct _ARM_UCS_Configuration
{
    arm_uc_uri_t manifest;
    uint32_t interval;
    uint32_t currentCost;
    time_t lastPoll;
    int8_t hash[ARM_UCS_HASH_LENGTH];
    void (*eventHandler)(uint32_t event);
} ARM_UCS_Configuration_t;

/*static */ARM_UCS_Configuration_t default_config = {
    .manifest = {
        .size_max = 0,
        .size = 0,
        .ptr = NULL,
        .scheme = URI_SCHEME_NONE,
        .port = 0,
        .host = NULL,
        .path = NULL
    },
    .interval = 0,
    .currentCost = 0xFFFFFFFF,
    .lastPoll = 0,
    .hash = { 0 },
    .eventHandler = 0
};

typedef enum {
    STATE_UCS_LORA_IDLE,
    STATE_UCS_LORA_MANIFEST,
    STATE_UCS_LORA_FIRMWARE,
    STATE_UCS_LORA_KEYTABLE,
    STATE_UCS_LORA_HASH
} arm_ucs_lora_state_t;

#define MAX_RETRY 3
typedef struct {
    arm_ucs_lora_state_t state;
    arm_uc_buffer_t *buffer;
    uint32_t offset;
    uint8_t retryCount;
} arm_ucs_state_t;

static arm_ucs_state_t arm_ucs_state;

/* Helper function for resetting internal state */
static inline void uc_state_reset()
{
    arm_ucs_state.state  = STATE_UCS_LORA_IDLE;
    arm_ucs_state.buffer     = NULL;
    arm_ucs_state.offset     = 0;
    arm_ucs_state.retryCount = 0;
}

/* Helper function for checking if the stored hash is all zeros */
static inline bool hash_is_zero()
{
    bool result = true;

    for (uint32_t index = 0; index < ARM_UCS_HASH_LENGTH; index++)
    {
        if (default_config.hash[index] != 0)
        {
            result = false;
            break;
        }
    }

    return result;
}

/******************************************************************************/
/* ARM Update Client Source Extra                                             */
/******************************************************************************/

/**
 * @brief Set URI location for the default manifest.
 * @details The default manifest is polled regularly and generates a
 *          notification upon change. The URI struct and the content pointer to
 *          must be valid throughout the lifetime of the application.
 *
 * @param uri URI struct with manifest location.
 * @return Error code.
 */
arm_uc_error_t ARM_UCS_SetDefaultManifestURL(arm_uc_uri_t* uri)
{
    tr_debug("ARM_UCS_SetDefaultManifestURL");

    arm_uc_error_t result = (arm_uc_error_t){ SRCE_ERR_INVALID_PARAMETER };

    if (uri != 0)
    {
        /* copy pointers to local struct */
        default_config.manifest = *uri;

        result = (arm_uc_error_t){ SRCE_ERR_NONE };
    }

    return result;
}

/**
 * @brief Set polling interval for notification generation.
 * @details The default manifest location is polled with this interval.
 *
 * @param seconds Seconds between each poll.
 * @return Error code.
 */
arm_uc_error_t ARM_UCS_SetPollingInterval(uint32_t seconds)
{
    tr_debug("ARM_UCS_SetPollingInterval");

    default_config.interval = seconds;

    return (arm_uc_error_t){ SRCE_ERR_NONE };
}

/**
 * @brief Main function for the Source.
 * @details This function will query the default manifest location and generate
 *          a notification if it has changed since the last time it was checked.
 *          The number of queries generated is bound by the polling interval.
 *
 *          This function should be used on systems with timed callbacks.
 *
 * @return Seconds until the next polling interval.
 */
uint32_t ARM_UCS_CallMultipleTimes(arm_uc_buffer_t* hash_buffer)
{
    uint32_t result = default_config.interval;
    time_t unixtime = time(NULL);
    uint32_t elapsed = unixtime - default_config.lastPoll;

    // if ((default_config.eventHandler == NULL) ||
    //     (arm_ucs_state.state != STATE_UCS_LORA_IDLE) ||
    //     (hash_buffer == NULL))
    // {
    //     return default_config.interval;
    // }

    // if (elapsed >= default_config.interval)
    // {
    //     tr_debug("ARM_UCS_CallMultipleTimes");

    //     // poll default URI
    //     default_config.lastPoll = unixtime;

    //     // get resource hash
    //     arm_ucs_state.state = STATE_UCS_LORA_HASH;
    //     arm_ucs_state.buffer = hash_buffer;

    //     arm_uc_error_t retval = ARM_UCS_HttpSocket_GetHash(&default_config.manifest,
    //                                                        arm_ucs_state.buffer);
    //     if (retval.error != ERR_NONE)
    //     {
    //         uc_state_reset();
    //         return default_config.interval;
    //     }
    // }
    // else
    // {
    //     result = (elapsed > 0) ? default_config.interval - elapsed : default_config.interval;
    // }

    return result;
}


/******************************************************************************/
/* ARM Update Client Source                                                   */
/******************************************************************************/

static uint8_t *state_manifest_ptr;
static size_t   state_manifest_size;

void ARM_UCS_FwDone(uint8_t *manifest_ptr, size_t manifest_size)
{
    tr_debug("ARM_UCS_FwDone");

    state_manifest_ptr = manifest_ptr;
    state_manifest_size = manifest_size;

    if (default_config.eventHandler)
    {
        default_config.eventHandler(EVENT_NOTIFICATION);
    }
}


/**
 * @brief Get driver version.
 * @return Driver version.
 */
uint32_t ARM_UCS_GetVersion(void)
{
    tr_debug("ARM_UCS_GetVersion");

    return 0;
}

/**
 * @brief Get Source capabilities.
 * @return Struct containing capabilites. See definition above.
 */
ARM_SOURCE_CAPABILITIES ARM_UCS_GetCapabilities(void)
{
    tr_debug("ARM_UCS_GetCapabilities");

    ARM_SOURCE_CAPABILITIES result = {
        .notify = 0,
        .manifest_default = 1,
        .manifest_url = 0,
        .firmware = 1,
        .keytable = 0
    };

    return result;
}

/**
 * @brief Initialize Source.
 * @details Function pointer to event handler is passed as argument.
 *
 * @param cb_event Function pointer to event handler. See events above.
 * @return Error code.
 */
arm_uc_error_t ARM_UCS_Initialize(ARM_SOURCE_SignalEvent_t cb_event)
{
    tr_debug("ARM_UCS_Initialize");

    arm_uc_error_t result = (arm_uc_error_t){ SRCE_ERR_INVALID_PARAMETER };

    if (cb_event != 0)
    {
        default_config.currentCost = ARM_UCS_DEFAULT_COST;
        default_config.eventHandler = cb_event;
        result = (arm_uc_error_t){ SRCE_ERR_NONE };
    }

    uc_state_reset();
    return result;
}

/**
 * @brief Uninitialized Source.
 * @return Error code.
 */
arm_uc_error_t ARM_UCS_Uninitialize(void)
{
    tr_debug("ARM_UCS_Uninitialize");

    return (arm_uc_error_t){ SRCE_ERR_INVALID_PARAMETER };
}

/**
 * @brief Cost estimation for retrieving manifest from the default location.
 * @details The estimation can vary over time and should not be cached too long.
 *          0x00000000 - The manifest is already downloaded.
 *          0xFFFFFFFF - Cannot retrieve manifest from this Source.
 *
 * @param cost Pointer to variable for the return value.
 * @return Error code.
 */
arm_uc_error_t ARM_UCS_GetManifestDefaultCost(uint32_t* cost)
{
    tr_debug("ARM_UCS_GetManifestDefaultCost");

    arm_uc_error_t result = (arm_uc_error_t){ SRCE_ERR_INVALID_PARAMETER };

    if (cost != 0)
    {
        *cost = default_config.currentCost;
        result = (arm_uc_error_t){ SRCE_ERR_NONE };
    }

    return result;
}

/**
 * @brief Cost estimation for retrieving manifest from URL.
 * @details The estimation can vary over time and should not be cached too long.
 *          0x00000000 - The manifest is already downloaded.
 *          0xFFFFFFFF - Cannot retrieve manifest from this Source.
 *
 * @param uri URI struct with manifest location.
 * @param cost Pointer to variable for the return value.
 * @return Error code.
 */
arm_uc_error_t ARM_UCS_GetManifestURLCost(arm_uc_uri_t* uri, uint32_t* cost)
{
    tr_debug("ARM_UCS_GetManifestURLCost");

    *cost = 0xFFFFFFFF;
    arm_uc_error_t result = (arm_uc_error_t){ SRCE_ERR_INVALID_PARAMETER };

    return result;
}

/**
 * @brief Cost estimation for retrieving firmware from URL.
 * @details The estimation can vary over time and should not be cached too long.
 *          0x00000000 - The firmware is already downloaded.
 *          0xFFFFFFFF - Cannot retrieve firmware from this Source.
 *
 * @param uri URI struct with firmware location.
 * @param cost Pointer to variable for the return value.
 * @return Error code.
 */
arm_uc_error_t ARM_UCS_GetFirmwareURLCost(arm_uc_uri_t* uri, uint32_t* cost)
{
    tr_debug("ARM_UCS_GetFirmwareURLCost");

    *cost = 0xFFFFFFFF;
    arm_uc_error_t result = (arm_uc_error_t){ SRCE_ERR_INVALID_PARAMETER };


    return result;
}

/**
 * @brief Cost estimation for retrieving key table from URL.
 * @details The estimation can vary over time and should not be cached too long.
 *          0x00000000 - The firmware is already downloaded.
 *          0xFFFFFFFF - Cannot retrieve firmware from this Source.
 *
 * @param uri URI struct with keytable location.
 * @param cost Pointer to variable for the return value.
 * @return Error code.
 */
arm_uc_error_t ARM_UCS_GetKeytableURLCost(arm_uc_uri_t* uri, uint32_t* cost)
{
    tr_debug("ARM_UCS_GetKeytableURLCost");

    *cost = 0xFFFFFFFF;
    arm_uc_error_t result = (arm_uc_error_t){ SRCE_ERR_INVALID_PARAMETER };

    return result;
}


/**
 * @brief Retrieve manifest from the default location.
 * @details Manifest is stored in supplied buffer.
 *          Event is generated once manifest is in buffer.
 *
 * @param buffer Struct containing byte array, maximum size, and actual size.
 * @return Error code.
 */
arm_uc_error_t ARM_UCS_GetManifestDefault(arm_uc_buffer_t* buffer, uint32_t offset)
{
    tr_debug("ARM_UCS_GetManifestDefault");
    printf("offset %u\n", offset);

    buffer->size_max = state_manifest_size;
    buffer->size = state_manifest_size;
    buffer->ptr = state_manifest_ptr;

    for (size_t ix = 0; ix < state_manifest_size; ix++) {
        printf("%02x ", state_manifest_ptr[ix]);
    }
    printf("\n");

    if (default_config.eventHandler)
    {
        default_config.eventHandler(EVENT_MANIFEST);
    }

    arm_uc_error_t result = (arm_uc_error_t){ SRCE_ERR_NONE };
    return result;
}

/**
 * @brief Retrieve manifest from URL.
 * @details Manifest is stored in supplied buffer.
 *          Event is generated once manifest is in buffer.
 *
 * @param uri URI struct with manifest location.
 * @param buffer Struct containing byte array, maximum size, and actual size.
 *
 * @return Error code.
 */
arm_uc_error_t ARM_UCS_GetManifestURL(arm_uc_uri_t* uri, arm_uc_buffer_t* buffer, uint32_t offset)
{
    tr_debug("ARM_UCS_GetManifestURL");

    return (arm_uc_error_t){ SRCE_ERR_INVALID_PARAMETER };;
}

/**
 * @brief Retrieve firmware fragment.
 * @details Firmware fragment is stored in supplied buffer.
 *          Event is generated once fragment is in buffer.
 *
 * @param uri URI struct with firmware location.
 * @param buffer Struct containing byte array, maximum size, and actual size.
 * @param offset Firmware offset to retrieve fragment from.
 * @return Error code.
 */
arm_uc_error_t ARM_UCS_GetFirmwareFragment(arm_uc_uri_t* uri, arm_uc_buffer_t* buffer, uint32_t offset)
{
    tr_debug("ARM_UCS_GetFirmwareFragment");

    return (arm_uc_error_t){ SRCE_ERR_INVALID_PARAMETER };
}

/**
 * @brief Retrieve a key table from a URL.
 * @details Key table is stored in supplied buffer.
 *          Event is generated once fragment is in buffer.
 *
 * @param uri URI struct with keytable location.
 * @param buffer Struct containing byte array, maximum size, and actual size.
 * @return Error code.
 */
arm_uc_error_t ARM_UCS_GetKeytableURL(arm_uc_uri_t* uri, arm_uc_buffer_t* buffer)
{
    tr_debug("ARM_UCS_GetKeytableURL");

    return (arm_uc_error_t){ SRCE_ERR_INVALID_PARAMETER };
}

ARM_UPDATE_SOURCE ARM_UCS_LoRaSource =
{
    .GetVersion             = ARM_UCS_GetVersion,
    .GetCapabilities        = ARM_UCS_GetCapabilities,
    .Initialize             = ARM_UCS_Initialize,
    .Uninitialize           = ARM_UCS_Uninitialize,
    .GetManifestDefaultCost = ARM_UCS_GetManifestDefaultCost,
    .GetManifestURLCost     = ARM_UCS_GetManifestURLCost,
    .GetFirmwareURLCost     = ARM_UCS_GetFirmwareURLCost,
    .GetKeytableURLCost     = ARM_UCS_GetKeytableURLCost,
    .GetManifestDefault     = ARM_UCS_GetManifestDefault,
    .GetManifestURL         = ARM_UCS_GetManifestURL,
    .GetFirmwareFragment    = ARM_UCS_GetFirmwareFragment,
    .GetKeytableURL         = ARM_UCS_GetKeytableURL
};
