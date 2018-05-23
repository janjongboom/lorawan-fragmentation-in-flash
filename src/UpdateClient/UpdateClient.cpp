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

#include <stdio.h>
#include "mbed.h"

#include "LoRaSource.h"

#include "update-client-hub/update_client_hub.h"
#include "UpdateClient.h"

#include "arm_uc_pal_flashiap.h"

#include "mbed-trace/mbed_trace.h"
#define TRACE_GROUP "uccc"


extern const uint8_t arm_uc_vendor_id[];
extern const uint16_t arm_uc_vendor_id_size;
extern const uint8_t arm_uc_class_id[];
extern const uint8_t arm_uc_default_fingerprint[];
extern const uint16_t arm_uc_default_fingerprint_size;
extern const uint8_t arm_uc_default_subject_key_identifier[];
extern const uint16_t arm_uc_default_subject_key_identifier_size;
extern const uint8_t arm_uc_default_certificate[];
extern const uint16_t arm_uc_default_certificate_size;

namespace UpdateClient
{
    static int8_t update_client_tasklet_id = -1;
    // static Callback<void(int32_t)> error_callback;

    static void certificate_done(arm_uc_error_t error,
                                 const arm_uc_buffer_t* fingerprint);
    static void initialization(void);
    static void initialization_done(int32_t);
    static void queue_handler(void);
    static void schedule_event(void);
    static void error_handler(int32_t error);
}

void UpdateClient::UpdateClient(Callback<void(int32_t)> callback)
{
    tr_info("Update Client External Initialization");

    /* store callback handler */
    // error_callback = callback;
}

void UpdateClient::set_update_authorize_handler(void (*handler)(int32_t request))
{
    ARM_UC_SetAuthorizeHandler(handler);
}

void UpdateClient::update_authorize(int32_t request)
{
    switch (request)
    {
        case RequestDownload:
            ARM_UC_Authorize(ARM_UCCC_REQUEST_DOWNLOAD);
            break;
        case RequestInstall:
            ARM_UC_Authorize(ARM_UCCC_REQUEST_INSTALL);
            break;
        case RequestInvalid:
        default:
            break;
    }
}

void UpdateClient::set_update_progress_handler(void (*handler)(uint32_t progress, uint32_t total))
{
    ARM_UC_SetProgressHandler(handler);
}


static void UpdateClient::initialization(void)
{
    tr_info("internal initialization");

    /* Register sources */
    static const ARM_UPDATE_SOURCE* sources[] = {
        &ARM_UCS_LoRaSource
    };

    ARM_UC_HUB_SetSources(sources, sizeof(sources)/sizeof(ARM_UPDATE_SOURCE*));

    /* Register local error handler */
    ARM_UC_HUB_AddErrorCallback(UpdateClient::error_handler);

    /* Link internal queue with external scheduler.
       The callback handler is called whenever a task is posted to
       an empty queue. This will trigger the queue to be processed.
    */
    ARM_UC_HUB_AddNotificationHandler(UpdateClient::queue_handler);

    ARM_UC_HUB_SetStorage(&MBED_CLOUD_CLIENT_UPDATE_STORAGE);


#ifdef MBED_CLOUD_DEV_UPDATE_PSK
    /* Add pre shared key */
    ARM_UC_AddPreSharedKey(arm_uc_default_psk, arm_uc_default_psk_bits);
#endif

    /* Add verification certificate */
    arm_uc_error_t result = ARM_UC_AddCertificate(arm_uc_default_certificate,
                                                  arm_uc_default_certificate_size,
                                                  arm_uc_default_fingerprint,
                                                  arm_uc_default_fingerprint_size,
                                                  UpdateClient::certificate_done);

    /* Certificate insertion failed, most likely because the certificate
       has already been inserted once before.

       Continue initialization regardlessly, since the Update Client can still
       work if verification certificates are inserted through the Factory
       Client or by other means.
    */
    if (result.code != ARM_UC_CM_ERR_NONE)
    {
        tr_info("ARM_UC_AddCertificate failed");

        ARM_UC_HUB_Initialize(UpdateClient::initialization_done);
    }
}

static void UpdateClient::certificate_done(arm_uc_error_t error,
                                           const arm_uc_buffer_t* fingerprint)
{
    (void) fingerprint;

    /* Certificate insertion failure is not necessarily fatal.
       If verification certificates have been injected by other means
       it is still possible to perform updates, which is why the
       Update client initializes anyway.
    */
    if (error.code != ARM_UC_CM_ERR_NONE)
    {
        // error_callback(WarningCertificateInsertion);
    }

    ARM_UC_HUB_Initialize(UpdateClient::initialization_done);
}

static void UpdateClient::initialization_done(int32_t result)
{
    tr_info("internal initialization done: %" PRIu32, result);
}

void UpdateClient::event_handler(UpdateClientEventType event)
{
    switch (event)
    {
        case UPDATE_CLIENT_EVENT_INITIALIZE:
            UpdateClient::initialization();
            break;

        case UPDATE_CLIENT_EVENT_PROCESS_QUEUE:
            {
                /* process a single callback, for better cooperability */
                bool queue_not_empty = ARM_UC_ProcessSingleCallback();

                if (queue_not_empty)
                {
                    /* reschedule event handler, if queue is not empty */
                    UpdateClient::schedule_event();
                }
            }
            break;

        default:
            break;
    }
}

static void UpdateClient::queue_handler(void)
{
    /* warning: queue_handler can be called from interrupt context.
    */
    UpdateClient::schedule_event();
}

static void UpdateClient::schedule_event()
{
    tr_info("schedule-event");
    UpdateClient::event_handler(UPDATE_CLIENT_EVENT_PROCESS_QUEUE);
}

static void UpdateClient::error_handler(int32_t error)
{
    tr_info("error reported: %d", error);

    /* add warning base if less severe than error */
    if (error < ARM_UC_ERROR)
    {
        // error_callback(WarningBase + error);
    }
    /* add error base if less severe than fatal */
    else if (error < ARM_UC_FATAL)
    {
        // error_callback(ErrorBase + error);
    }
    /* add fatal base */
    else
    {
        // error_callback(FatalBase + error);
    }
}
