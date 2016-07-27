/**
 * Copyright 2016 ICE9 Consulting
 *
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

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "bsp/bsp.h"
#include "console/console.h"
#include "host/ble_hs.h"
#include "quacker.h"

/**
 * Vendor-specific slide quacker service.
 *
 * Set your orientation and other fun tasks!
 */
/* 0332FF64-7433-499F-8F27-38F7A2638B6F */
const uint8_t gatt_svr_svc_quacker[16] = {
    0x6F, 0x8B, 0x63, 0xA2, 0xF7, 0x38, 0x27, 0x8F,
    0x9F, 0x49, 0x33, 0x74, 0x64, 0xFF, 0x32, 0x03,
};

/* 1BE106B6-B21E-49C5-B2C4-C639D94C2FE1 */
const uint8_t gatt_svr_chr_quacker_orientation[16] = {
    0xE1, 0x2F, 0x4C, 0xD9, 0x39, 0xC6, 0xC4, 0xB2,
    0xC5, 0x49, 0x1E, 0xB2, 0xB6, 0x06, 0xE1, 0x1B,
};

static int
gatt_svr_chr_access_gap(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
                        union ble_gatt_access_ctxt *ctxt, void *arg);
static int
gatt_svr_chr_access_gatt(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
                         union ble_gatt_access_ctxt *ctxt, void *arg);

static int
gatt_svr_chr_access_device_information(uint16_t conn_handle, uint16_t attr_handle,
                                       uint8_t op, union ble_gatt_access_ctxt *ctxt,
                                       void *arg);

static int
gatt_svr_chr_access_hid(uint16_t conn_handle, uint16_t attr_handle,
                                uint8_t op, union ble_gatt_access_ctxt *ctxt,
                                void *arg);

static int
gatt_svr_dsc_access_hid(uint16_t conn_handle, uint16_t attr_handle,
                                uint8_t op, union ble_gatt_access_ctxt *ctxt,
                                void *arg);

static int
gatt_svr_chr_access_quacker(uint16_t conn_handle, uint16_t attr_handle,
                                uint8_t op, union ble_gatt_access_ctxt *ctxt,
                                void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service: GAP. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = BLE_UUID16(BLE_GAP_SVC_UUID16),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic: Device Name. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_DEVICE_NAME),
            .access_cb = gatt_svr_chr_access_gap,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /*** Characteristic: Appearance. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_APPEARANCE),
            .access_cb = gatt_svr_chr_access_gap,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /*** Characteristic: Peripheral Privacy Flag. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_PERIPH_PRIV_FLAG),
            .access_cb = gatt_svr_chr_access_gap,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /*** Characteristic: Reconnection Address. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_RECONNECT_ADDR),
            .access_cb = gatt_svr_chr_access_gap,
            .flags = BLE_GATT_CHR_F_WRITE,
        }, {
            /*** Characteristic: Peripheral Preferred Connection Parameters. */
            .uuid128 = BLE_UUID16(BLE_GAP_CHR_UUID16_PERIPH_PREF_CONN_PARAMS),
            .access_cb = gatt_svr_chr_access_gap,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        /*** Service: GATT */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = BLE_UUID16(BLE_GATT_SVC_UUID16),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid128 = BLE_UUID16(BLE_GATT_CHR_SERVICE_CHANGED_UUID16),
            .access_cb = gatt_svr_chr_access_gatt,
            .flags = BLE_GATT_CHR_F_INDICATE,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        /*** Service: Device Information */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = BLE_UUID16(GATT_SVR_SVC_DEVICE_INFORMATION_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid128 = BLE_UUID16(GATT_SVR_CHR_MANUFACTURER_NAME_UUID),
            .access_cb = gatt_svr_chr_access_device_information,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            .uuid128 = BLE_UUID16(GATT_SVR_CHR_MODEL_NUMBER_UUID),
            .access_cb = gatt_svr_chr_access_device_information,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        /*** HID over GATT service. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = BLE_UUID16(GATT_SVR_SVC_HID_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid128 = BLE_UUID16(GATT_SVR_CHR_HID_INFORMATION),
            .access_cb = gatt_svr_chr_access_hid,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
        }, {
            .uuid128 = BLE_UUID16(GATT_SVR_CHR_REPORT_MAP),
            .access_cb = gatt_svr_chr_access_hid,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
        }, {
            .uuid128 = BLE_UUID16(GATT_SVR_CHR_BOOT_KEYBOARD_INPUT_MAP),
            .access_cb = gatt_svr_chr_access_hid,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
        }, {
            .uuid128 = BLE_UUID16(GATT_SVR_CHR_REPORT),
            .access_cb = gatt_svr_chr_access_hid,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
            .descriptors = (struct ble_gatt_dsc_def[]) { {
                .uuid128 = BLE_UUID16(GATT_SVR_DSC_REPORT_REFERENCE),
                .att_flags = BLE_ATT_F_READ | BLE_ATT_F_READ_ENC,
                .access_cb = gatt_svr_dsc_access_hid,
                .arg = (void *)0,
            }, {
                0, /* No more descriptors in this characteristic. */
            } },
        }, {
            .uuid128 = BLE_UUID16(GATT_SVR_CHR_REPORT),
            .access_cb = gatt_svr_chr_access_hid,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_NOTIFY,
            .descriptors = (struct ble_gatt_dsc_def[]) { {
                .uuid128 = BLE_UUID16(GATT_SVR_DSC_REPORT_REFERENCE),
                .att_flags = BLE_ATT_F_READ | BLE_ATT_F_READ_ENC,
                .access_cb = gatt_svr_dsc_access_hid,
                .arg = (void *)1,
            }, {
                0, /* No more descriptors in this characteristic. */
            } },
        }, {
            .uuid128 = BLE_UUID16(GATT_SVR_CHR_HID_CONTROL_POINT),
            .access_cb = gatt_svr_chr_access_hid,
            .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
        }, {
            .uuid128 = BLE_UUID16(GATT_SVR_CHR_PROTOCOL_MODE),
            .access_cb = gatt_svr_chr_access_hid,
            .flags = BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        /*** Service: quacker. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid128 = (void *)gatt_svr_svc_quacker,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic: Read/Write. */
            .uuid128 = (void *)gatt_svr_chr_quacker_orientation,
            .access_cb = gatt_svr_chr_access_quacker,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC |
                     BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },


    {
        0, /* No more services. */
    },
};

static int
gatt_svr_chr_write(uint8_t op, union ble_gatt_access_ctxt *ctxt,
                   uint16_t min_len, uint16_t max_len, void *dst,
                   uint16_t *len)
{
    assert(op == BLE_GATT_ACCESS_OP_WRITE_CHR);
    if (ctxt->chr_access.len < min_len ||
        ctxt->chr_access.len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    memcpy(dst, ctxt->chr_access.data, ctxt->chr_access.len);
    if (len != NULL) {
        *len = ctxt->chr_access.len;
    }

    return 0;
}

static int
gatt_svr_chr_access_gap(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
                        union ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid16;

    uuid16 = ble_uuid_128_to_16(ctxt->chr_access.chr->uuid128);
    assert(uuid16 != 0);

    switch (uuid16) {
    case BLE_GAP_CHR_UUID16_DEVICE_NAME:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)quacker_device_name;
        ctxt->chr_access.len = strlen(quacker_device_name);
        break;

    case BLE_GAP_CHR_UUID16_APPEARANCE:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)&quacker_appearance;
        ctxt->chr_access.len = sizeof quacker_appearance;
        break;

    case BLE_GAP_CHR_UUID16_PERIPH_PRIV_FLAG:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)&quacker_privacy_flag;
        ctxt->chr_access.len = sizeof quacker_privacy_flag;
        break;

    case BLE_GAP_CHR_UUID16_RECONNECT_ADDR:
        assert(op == BLE_GATT_ACCESS_OP_WRITE_CHR);
        if (ctxt->chr_access.len != sizeof quacker_reconnect_addr) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        memcpy(quacker_reconnect_addr, ctxt->chr_access.data,
               sizeof quacker_reconnect_addr);
        break;

    case BLE_GAP_CHR_UUID16_PERIPH_PREF_CONN_PARAMS:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)&quacker_pref_conn_params;
        ctxt->chr_access.len = sizeof quacker_pref_conn_params;
        break;

    default:
        assert(0);
        break;
    }

    return 0;
}

static int
gatt_svr_chr_access_gatt(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
                         union ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid16;

    uuid16 = ble_uuid_128_to_16(ctxt->chr_access.chr->uuid128);
    assert(uuid16 != 0);

    switch (uuid16) {
    case BLE_GATT_CHR_SERVICE_CHANGED_UUID16:
        if (op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            if (ctxt->chr_access.len != sizeof quacker_gatt_service_changed) {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
            memcpy(quacker_gatt_service_changed, ctxt->chr_access.data,
                   sizeof quacker_gatt_service_changed);
        } else if (op == BLE_GATT_ACCESS_OP_READ_CHR) {
            ctxt->chr_access.data = (void *)&quacker_gatt_service_changed;
            ctxt->chr_access.len = sizeof quacker_gatt_service_changed;
        }
        break;

    default:
        assert(0);
        break;
    }

    return 0;
}


static int
gatt_svr_chr_access_device_information(uint16_t conn_handle, uint16_t attr_handle,
                                       uint8_t op, union ble_gatt_access_ctxt *ctxt,
                                       void *arg)
{
    uint16_t uuid16;

    static const char manufacturer[] = "ICE9 Consulting";
    static const char model_number[] = "Wrong Island Con Slide Quacker";

    uuid16 = ble_uuid_128_to_16(ctxt->chr_access.chr->uuid128);
    assert(uuid16 != 0);

    switch (uuid16) {
    case GATT_SVR_CHR_MANUFACTURER_NAME_UUID:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)manufacturer;
        ctxt->chr_access.len = sizeof manufacturer;
        return 0;

    case GATT_SVR_CHR_MODEL_NUMBER_UUID:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)model_number;
        ctxt->chr_access.len = sizeof model_number;
        return 0;

    default:
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

/* both of these shamelessly stolen from BLE keyboard */
static const uint8_t gatt_svr_hid_information[] = { 0x01, 0x01, 0x00, 0x02, };
static const uint8_t gatt_svr_report_map[] = {
  0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x85, 0x01, 0x05, 0x07, 0x19, 0xe0, 0x29,
  0xe7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01,
  0x75, 0x08, 0x81, 0x03, 0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29,
  0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x03, 0x95, 0x06, 0x75, 0x08,
  0x15, 0x00, 0x26, 0xff, 0x00, 0x05, 0x07, 0x19, 0x00, 0x2a, 0xff, 0x00, 0x81,
  0x00, 0xc0, 0x06, 0x00, 0xff, 0x09, 0x02, 0xa1, 0x01, 0x85, 0x02, 0x75, 0x08,
  0x95, 0x01, 0x15, 0x01, 0x25, 0x64, 0x09, 0x20, 0x81, 0x00, 0xc0, 0x05, 0x0c,
  0x09, 0x01, 0xa1, 0x01, 0x85, 0x03, 0x75, 0x10, 0x95, 0x01, 0x15, 0x01, 0x26,
  0xff, 0x02, 0x19, 0x01, 0x2a, 0xff, 0x02, 0x81, 0x60, 0xc0 };

static const uint8_t gatt_svr_boot_keyboard_input_map[8] = { 0x00, };
uint8_t gatt_svr_hid_report[8] = { 0x00, };
static uint8_t gatt_svr_hid_control_point = 0x00;
static uint8_t gatt_svr_protocol_mode = 0x01;

static int
gatt_svr_chr_access_hid(uint16_t conn_handle, uint16_t attr_handle,
                            uint8_t op, union ble_gatt_access_ctxt *ctxt,
                            void *arg)
{
    uint16_t uuid16;
    int rc;

    uuid16 = ble_uuid_128_to_16(ctxt->chr_access.chr->uuid128);
    assert(uuid16 != 0);

    switch (uuid16) {
    case GATT_SVR_CHR_HID_INFORMATION:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)gatt_svr_hid_information;
        ctxt->chr_access.len = sizeof gatt_svr_hid_information;
        return 0;

    case GATT_SVR_CHR_REPORT_MAP:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)gatt_svr_report_map;
        ctxt->chr_access.len = sizeof gatt_svr_report_map;
        return 0;

    case GATT_SVR_CHR_BOOT_KEYBOARD_INPUT_MAP:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)gatt_svr_boot_keyboard_input_map;
        ctxt->chr_access.len = sizeof gatt_svr_boot_keyboard_input_map;
        return 0;

    case GATT_SVR_CHR_REPORT:
        assert(op == BLE_GATT_ACCESS_OP_READ_CHR);
        ctxt->chr_access.data = (void *)gatt_svr_hid_report;
        ctxt->chr_access.len = sizeof gatt_svr_hid_report;
        return 0;

    case GATT_SVR_CHR_HID_CONTROL_POINT:
        if (op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            rc = gatt_svr_chr_write(op, ctxt, 1, 1,
                                    &gatt_svr_hid_control_point, NULL);
        } else {
            rc = BLE_ATT_ERR_UNLIKELY;
        }
        return rc;

    case GATT_SVR_CHR_PROTOCOL_MODE:
        if (op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            rc = gatt_svr_chr_write(op, ctxt, 1, 1,
                                  &gatt_svr_protocol_mode, NULL);
            return rc;
        } else if (op == BLE_GATT_ACCESS_OP_READ_CHR) {
            ctxt->chr_access.data = (void *)&gatt_svr_protocol_mode;
            ctxt->chr_access.len = sizeof gatt_svr_protocol_mode;
            return 0;
        }

    default:
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }


    return BLE_ATT_ERR_UNLIKELY;
}

static const uint8_t gatt_svr_hid_report_ref[][2] = {
    { 0x00, 0x01, },
    { 0x01, 0x01, },
};

static int
gatt_svr_dsc_access_hid(uint16_t conn_handle, uint16_t attr_handle,
                            uint8_t op, union ble_gatt_access_ctxt *ctxt,
                            void *arg)
{
    uint16_t uuid16;
    int which;

    uuid16 = ble_uuid_128_to_16(ctxt->chr_access.chr->uuid128);
    assert(uuid16 != 0);

    which = (int)arg;
    assert(which == 0 || which == 1);

    switch (uuid16) {
    case GATT_SVR_DSC_REPORT_REFERENCE:
        assert(op == BLE_GATT_ACCESS_OP_READ_DSC);
        ctxt->chr_access.data = (void *)gatt_svr_hid_report_ref[which];
        ctxt->chr_access.len = 2;
        return 0;

    default:
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

char quacker_orientation[sizeof("UPRIGHT")] = { 'n', 'o', 'n', 'e', 0 };
static int
gatt_svr_chr_access_quacker(uint16_t conn_handle, uint16_t attr_handle,
                            uint8_t op, union ble_gatt_access_ctxt *ctxt,
                            void *arg)
{
    void *uuid128;
    int rc, len, i;
    char scratch[sizeof("UPRIGHT")];

    static const char* orientations[] = {
        "none", "flat", "upright", "rubber",
    };

    uuid128 = ctxt->chr_access.chr->uuid128;

    /* Determine which characteristic is being accessed by examining its
     * 128-bit UUID.
     */

    if (memcmp(uuid128, gatt_svr_chr_quacker_orientation, 16) == 0) {
        if (op == BLE_GATT_ACCESS_OP_READ_CHR) {
            ctxt->chr_access.data = (void *)quacker_orientation;
            ctxt->chr_access.len = strlen(quacker_orientation);
            return 0;
        } else if (op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            if ((len = ctxt->chr_access.len) < sizeof(scratch)) {
                // tolower() the entire write
                for (i = 0; i < len; ++i)
                    scratch[i] = tolower(((char *)ctxt->chr_access.data)[i]);
                scratch[len] = 0;
                // see if it matches any known orientations
                for (i = 0; i < sizeof(orientations)/sizeof(char *); ++i) {
                    if (strcmp(scratch, orientations[i]) == 0) {
                        // write lowercase version into memory
                        memcpy(ctxt->chr_access.data, scratch, len);
                        rc = gatt_svr_chr_write(op, ctxt, 1,
                                                sizeof quacker_orientation,
                                                (void *)quacker_orientation,
                                                NULL);
                        // TODO fire event
                        orientation = i; // set global enum
                        return rc;
                    }
                }
            }
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static char *
gatt_svr_uuid128_to_s(void *uuid128, char *dst)
{
    uint16_t uuid16;
    uint8_t *u8p;

    uuid16 = ble_uuid_128_to_16(uuid128);
    if (uuid16 != 0) {
        sprintf(dst, "0x%04x", uuid16);
        return dst;
    }

    u8p = uuid128;

    sprintf(dst,      "%02x%02x%02x%02x-", u8p[15], u8p[14], u8p[13], u8p[12]);
    sprintf(dst + 9,  "%02x%02x-%02x%02x-", u8p[11], u8p[10], u8p[9], u8p[8]);
    sprintf(dst + 19, "%02x%02x%02x%02x%02x%02x%02x%02x",
            u8p[7], u8p[6], u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);

    return dst;
}

static void
gatt_svr_register_cb(uint8_t op, union ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[40];

    switch (op) {
    case BLE_GATT_REGISTER_OP_SVC:
        QUACKER_LOG(DEBUG, "registered service %s with handle=%d\n",
                    gatt_svr_uuid128_to_s(ctxt->svc_reg.svc->uuid128, buf),
                    ctxt->svc_reg.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        QUACKER_LOG(DEBUG, "registering characteristic %s with "
                           "def_handle=%d val_handle=%d\n",
                    gatt_svr_uuid128_to_s(ctxt->chr_reg.chr->uuid128, buf),
                    ctxt->chr_reg.def_handle,
                    ctxt->chr_reg.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        QUACKER_LOG(DEBUG, "registering descriptor %s with handle=%d "
                           "chr_handle=%d\n",
                    gatt_svr_uuid128_to_s(ctxt->dsc_reg.dsc->uuid128, buf),
                    ctxt->dsc_reg.dsc_handle,
                    ctxt->dsc_reg.chr_def_handle);
        break;

    default:
        assert(0);
        break;
    }
}

void
gatt_svr_init(void)
{
    int rc;

    rc = ble_gatts_register_svcs(gatt_svr_svcs, gatt_svr_register_cb, NULL);
    if (rc != 0) { while (1) ; }
    assert(rc == 0);
}
