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

#ifndef MBED_CLOUD_CLIENT_UPDATE_CLIENT_H
#define MBED_CLOUD_CLIENT_UPDATE_CLIENT_H

#ifdef MBED_CLOUD_CLIENT_USER_CONFIG_FILE
#include MBED_CLOUD_CLIENT_USER_CONFIG_FILE
#endif

#include "update-client-hub/update_client_public.h"

namespace UpdateClient
{
    enum UpdateClientEventType {
        UPDATE_CLIENT_EVENT_INITIALIZE,
        UPDATE_CLIENT_EVENT_PROCESS_QUEUE
    };

    /**
     * Error codes used by the Update Client.
     *
     * Warning: a recoverable error occured, no user action required.
     * Error  : a recoverable error occured, action required. E.g. the
     *          application has to free some space and let the Update
     *          Service try again.
     * Fatal  : a non-recoverable error occured, application should safe
     *          ongoing work and reboot the device.
     */
    enum {
        WarningBase                     = 0x0400, // Range reserved for Update Error from 0x0400 - 0x04FF
        WarningCertificateNotFound      = WarningBase + ARM_UC_WARNING_CERTIFICATE_NOT_FOUND,
        WarningIdentityNotFound         = WarningBase + ARM_UC_WARNING_IDENTITY_NOT_FOUND,
        WarningVendorMismatch           = WarningBase + ARM_UC_WARNING_VENDOR_MISMATCH,
        WarningClassMismatch            = WarningBase + ARM_UC_WARNING_CLASS_MISMATCH,
        WarningDeviceMismatch           = WarningBase + ARM_UC_WARNING_DEVICE_MISMATCH,
        WarningCertificateInvalid       = WarningBase + ARM_UC_WARNING_CERTIFICATE_INVALID,
        WarningSignatureInvalid         = WarningBase + ARM_UC_WARNING_SIGNATURE_INVALID,
        WarningBadKeyTable              = WarningBase + ARM_UC_WARNING_BAD_KEYTABLE,
        WarningURINotFound              = WarningBase + ARM_UC_WARNING_URI_NOT_FOUND,
        WarningRollbackProtection       = WarningBase + ARM_UC_WARNING_ROLLBACK_PROTECTION,
        WarningUnknown                  = WarningBase + ARM_UC_WARNING_UNKNOWN,
        WarningCertificateInsertion,
        ErrorBase,
        ErrorWriteToStorage             = ErrorBase + ARM_UC_ERROR_WRITE_TO_STORAGE,
        ErrorInvalidHash                = ErrorBase + ARM_UC_ERROR_INVALID_HASH,
        FatalBase
    };

    enum {
        RequestInvalid                  = ARM_UCCC_REQUEST_INVALID,
        RequestDownload                 = ARM_UCCC_REQUEST_DOWNLOAD,
        RequestInstall                  = ARM_UCCC_REQUEST_INSTALL
    };

    /**
     * \brief Initialization function for the Update Client.
     * \param Callback to error handler.
     * \param m2mInterface Pointer to M2MInterface instance
     */
    void UpdateClient(Callback<void(int32_t)> callback);

    /**
     * \brief Registers a callback function for authorizing firmware downloads and reboots.
     * \param handler Callback function.
     */
    void set_update_authorize_handler(void (*handler)(int32_t request));

    /**
     * \brief Authorize request passed to authorization handler.
     * \param request Request being authorized.
     */
    void update_authorize(int32_t request);

    /**
     * \brief Registers a callback function for monitoring download progress.
     * \param handler Callback function.
     */
    void set_update_progress_handler(void (*handler)(uint32_t progress, uint32_t total));

    void event_handler(UpdateClientEventType event);
}

#endif // MBED_CLOUD_CLIENT_UPDATE_CLIENT_H
