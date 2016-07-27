/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef H_QUACKER_
#define H_QUACKER_

#include "log/log.h"

enum orientation_t {
    NONE = 0, FLAT, UPRIGHT, RUBBER,
};
extern enum orientation_t orientation;

extern struct log quacker_log;

extern const char *quacker_device_name;
extern const uint16_t quacker_appearance;
extern const uint8_t quacker_privacy_flag;
extern uint8_t quacker_reconnect_addr[6];
extern uint8_t quacker_pref_conn_params[8];
extern uint8_t quacker_gatt_service_changed[4];

/* quacker uses the first "peruser" log module. */
#define QUACKER_LOG_MODULE  (LOG_MODULE_PERUSER + 0)

/* Convenience macro for logging to the quacker module. */
#define QUACKER_LOG(lvl, ...) \
    LOG_ ## lvl(&quacker_log, QUACKER_LOG_MODULE, __VA_ARGS__)

/** GATT server. */
#define GATT_SVR_SVC_DEVICE_INFORMATION_UUID  0x180A
#define GATT_SVR_CHR_MANUFACTURER_NAME_UUID   0x2A29
#define GATT_SVR_CHR_MODEL_NUMBER_UUID        0x2A24

#define GATT_SVR_SVC_HID_UUID                 0x1812

#define GATT_SVR_CHR_HID_INFORMATION          0x2A4A
#define GATT_SVR_CHR_REPORT_MAP               0x2A4B
#define GATT_SVR_CHR_BOOT_KEYBOARD_INPUT_MAP  0x2A22
#define GATT_SVR_CHR_REPORT                   0x2A4D
#define GATT_SVR_CHR_HID_CONTROL_POINT        0x2A4C
#define GATT_SVR_CHR_PROTOCOL_MODE            0x2A4E

#define GATT_SVR_DSC_REPORT_REFERENCE         0x2908

void gatt_svr_init(void);

/** Keystore. */
int keystore_init(void);
int keystore_lookup(uint16_t ediv, uint64_t rand_num,
                    void *out_ltk, int *out_authenticated);
int keystore_add(uint16_t ediv, uint64_t rand_num, uint8_t *key,
                 int authenticated);

/** LEDs. */
void led_init(void);
void led_scroll(char *message);
void led_static(char *message);
void led_spinner(void);
void led_spinner_pairing(void);

#endif
