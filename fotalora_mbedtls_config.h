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

#ifndef FOTALORA_MBEDTLS_CONFIG_H

#include "platform/inc/platform_mbed.h"

#define FOTALORA_MBEDTLS_CONFIG_H

#define MBEDTLS_HAVE_ASM
#define MBEDTLS_HAVE_TIME

#define MBEDTLS_REMOVE_ARC4_CIPHERSUITES
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECDSA_DETERMINISTIC
#define MBEDTLS_NO_PLATFORM_ENTROPY

#define MBEDTLS_X509_USE_C
#define MBEDTLS_CIPHER_MODE_CTR
#define MBEDTLS_X509_CRT_PARSE_C

#define MBEDTLS_AES_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CCM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ERROR_C
#define MBEDTLS_HMAC_DRBG_C
#define MBEDTLS_MD_C
#define MBEDTLS_OID_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_RSA_C
#define MBEDTLS_SHA256_C

#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21

/* Client-only configuration */
#undef MBEDTLS_CERTS_C
#undef MBEDTLS_SSL_CACHE_C
#undef MBEDTLS_SSL_SRV_C
// needed for Base64 encoding Opaque data for
// registration payload, adds 500 bytes to flash.
#define MBEDTLS_BASE64_C

#define MBEDTLS_SSL_MAX_CONTENT_LEN 2048
#define MBEDTLS_ENTROPY_MAX_SOURCES 2

#define MBEDTLS_CIPHER_MODE_CTR

/* Support only PSK with AES 128 in CCM-8 mode */
#define MBEDTLS_SSL_CIPHERSUITES MBEDTLS_TLS_PSK_WITH_AES_128_CCM_8

#include "check_config.h"

#endif /* FOTALORA_MBEDTLS_CONFIG_H */
