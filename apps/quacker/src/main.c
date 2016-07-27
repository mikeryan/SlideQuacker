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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "bsp/bsp.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_cputime.h"
#include "hal/hal_i2c.h"
#include "hal/hal_flash.h"
#include "hal/flash_map.h"
#include "console/console.h"
#include "fs/fs.h"
#include "nffs/nffs.h"

/* BLE */
#include "nimble/ble.h"
#include "host/host_hci.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_uuid.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_l2cap.h"
#include "controller/ble_ll.h"

#include "quacker.h"

#define BSWAP16(x)  ((uint16_t)(((x) << 8) | (((x) & 0xff00) >> 8)))

/** Mbuf settings. */
#define MBUF_NUM_MBUFS      (12)
#define MBUF_BUF_SIZE       OS_ALIGN(BLE_MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + BLE_MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

static os_membuf_t quacker_mbuf_mpool_data[MBUF_MEMPOOL_SIZE];
struct os_mbuf_pool quacker_mbuf_pool;
struct os_mempool quacker_mbuf_mpool;

/** Log data. */
static struct log_handler quacker_log_console_handler;
struct log quacker_log;

/** Priority of the nimble host and controller tasks. */
#define BLE_LL_TASK_PRI             (OS_TASK_PRI_HIGHEST)

/** quacker task settings. */
#define QUACKER_TASK_PRIO           1
#define QUACKER_STACK_SIZE          (OS_STACK_ALIGN(336))

#define BUTTON_TASK_PRIO            2
#define BUTTON_STACK_SIZE           (OS_STACK_ALIGN(256))

#define LED_TASK_PRIO               3
#define LED_STACK_SIZE              (OS_STACK_ALIGN(128))

#define POWER_LED_TASK_PRIO         3
#define POWER_LED_STACK_SIZE        (OS_STACK_ALIGN(64))

#define ACCEL_TASK_PRIO             4
#define ACCEL_STACK_SIZE            (OS_STACK_ALIGN(128))

struct os_eventq quacker_evq;
struct os_task quacker_task;
bssnz_t os_stack_t quacker_stack[QUACKER_STACK_SIZE];

struct os_task button_task;
bssnz_t os_stack_t button_stack[BUTTON_STACK_SIZE];

struct os_eventq led_evq;
struct os_task led_task;
bssnz_t os_stack_t led_stack[LED_STACK_SIZE];

struct os_task power_led_task;
bssnz_t os_stack_t power_led_stack[POWER_LED_STACK_SIZE];

struct os_task accel_task;
bssnz_t os_stack_t accel_stack[ACCEL_STACK_SIZE];

/** Our global device address (public) */
uint8_t g_dev_addr[BLE_DEV_ADDR_LEN] = {0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a};

/** Our random address (in case we need it) */
uint8_t g_random_addr[BLE_DEV_ADDR_LEN];

/** Device name - included in advertisements and exposed by GAP service. */
const char *quacker_device_name = "Slide Quacker";

/** Device properties - exposed by GAP service. */
const uint16_t quacker_appearance = 961; // BSWAP16(961); // HID keyboard
const uint8_t quacker_privacy_flag = 0;
uint8_t quacker_reconnect_addr[6];
uint8_t quacker_pref_conn_params[8];
uint8_t quacker_gatt_service_changed[4];

static int _nffs_init(void);

static int quacker_gap_event(int event, int status,
                             struct ble_gap_conn_ctxt *ctxt, void *arg);

/**
 * Utility function to log an array of bytes.
 */
static void
quacker_print_bytes(uint8_t *bytes, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        QUACKER_LOG(INFO, "%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
}

/**
 * Logs information about a connection to the console.
 */
static void
quacker_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    QUACKER_LOG(INFO, "handle=%d peer_addr_type=%d peer_addr=",
                desc->conn_handle,
                desc->peer_addr_type);
    quacker_print_bytes(desc->peer_addr, 6);
    QUACKER_LOG(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                      "encrypted=%d authenticated=%d",
                desc->conn_itvl,
                desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.enc_enabled,
                desc->sec_state.authenticated);
}

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
static void
quacker_advertise(void)
{
    struct ble_hs_adv_fields fields;
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     *     o Appearance.
     */

    memset(&fields, 0, sizeof fields);

    // fields.tx_pwr_lvl_is_present = 1;

    fields.name = (uint8_t *)quacker_device_name;
    fields.name_len = strlen(quacker_device_name);
    fields.name_is_complete = 1;

    fields.uuids16 = (uint16_t[]){ GATT_SVR_SVC_HID_UUID };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    fields.appearance = 961;
    fields.appearance_is_present = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        QUACKER_LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    rc = ble_gap_adv_start(BLE_GAP_DISC_MODE_GEN, BLE_GAP_CONN_MODE_UND,
                           NULL, 0, NULL, quacker_gap_event, NULL);
    if (rc != 0) {
        QUACKER_LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * quacker uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param status                The error code associated with the event
 *                                  (0 = success).
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unuesd by
 *                                  quacker.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
quacker_gap_event(int event, int status, struct ble_gap_conn_ctxt *ctxt,
                  void *arg)
{
    int authenticated;
    int rc;

    switch (event) {
    case BLE_GAP_EVENT_CONN:
        /* A new connection has been established or an existing one has been
         * terminated.
         */
        QUACKER_LOG(INFO, "connection %s; status=%d ",
                    status == 0 ? "up" : "down", status);
        quacker_print_conn_desc(ctxt->desc);
        QUACKER_LOG(INFO, "\n");

        if (status != 0) {
            /* Connection terminated; resume advertising. */
            quacker_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATED:
        /* The central has updated the connection parameters. */
        QUACKER_LOG(INFO, "connection updated; status=%d ", status);
        quacker_print_conn_desc(ctxt->desc);
        QUACKER_LOG(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_LTK_REQUEST:
        /* An encryption procedure (bonding) is being attempted.  The nimble
         * stack is asking us to look in our key database for a long-term key
         * corresponding to the specified ediv and random number.
         */
        QUACKER_LOG(INFO, "looking up ltk with ediv=0x%02x rand=0x%llx\n",
                    ctxt->ltk_params->ediv, ctxt->ltk_params->rand_num);

        /* Perform a key lookup and populate the context object with the
         * result.  The nimble stack will use this key if this function returns
         * success.
         */
        rc = keystore_lookup(ctxt->ltk_params->ediv,
                             ctxt->ltk_params->rand_num, ctxt->ltk_params->ltk,
                             &authenticated);
        if (rc == 0) {
            ctxt->ltk_params->authenticated = authenticated;
            QUACKER_LOG(INFO, "ltk=");
            quacker_print_bytes(ctxt->ltk_params->ltk,
                                sizeof ctxt->ltk_params->ltk);
            QUACKER_LOG(INFO, " authenticated=%d\n", authenticated);
        } else {
            QUACKER_LOG(INFO, "no matching ltk\n");
        }

        /* Indicate whether we were able to find an appropriate key. */
        return rc;

    case BLE_GAP_EVENT_KEY_EXCHANGE:
        /* The central is sending us key information or vice-versa.  If the
         * central is doing the sending, save the long-term key in the in-RAM
         * database.  This permits bonding to occur on subsequent connections
         * with this peer (as long as quacker isn't restarted!).
         */
        if (ctxt->key_params->is_ours   &&
            ctxt->key_params->ltk_valid &&
            ctxt->key_params->ediv_rand_valid) {

            rc = keystore_add(ctxt->key_params->ediv,
                              ctxt->key_params->rand_val,
                              ctxt->key_params->ltk,
                              ctxt->desc->sec_state.authenticated);
            if (rc != 0) {
                QUACKER_LOG(INFO, "error persisting LTK; status=%d\n", rc);
            }
        }
        return 0;

    case BLE_GAP_EVENT_SECURITY:
        /* Encryption has been enabled or disabled for this connection. */
        QUACKER_LOG(INFO, "security event; status=%d ", status);
        quacker_print_conn_desc(ctxt->desc);
        QUACKER_LOG(INFO, "\n");
        return 0;
    }

    return 0;
}

/**
 * Event loop for the main quacker task.
 */
static void
quacker_task_handler(void *unused)
{
    struct os_event *ev;
    struct os_callout_func *cf;
    int rc;

    rc = ble_hs_start();
    assert(rc == 0);

    /* Begin advertising. */
    quacker_advertise();

    while (1) {
        ev = os_eventq_get(&quacker_evq);
        switch (ev->ev_type) {
        case OS_EVENT_T_TIMER:
            cf = (struct os_callout_func *)ev;
            assert(cf->cf_func);
            cf->cf_func(CF_ARG(cf));
            break;
        default:
            assert(0);
            break;
        }
    }
}

// FIXME - find a better way to inject HID reports
extern uint8_t gatt_svr_hid_report[8];

static void
button_task_handler(void *unused)
{
    static const uint8_t report_char[] = { 0x50, 0x4F }; // left and right
    static const int button[] = { BUTTON1, BUTTON2 };
    int state[] = { 1, 1 };
    int i;

    hal_gpio_init_out(LED_EYE1, 0);

    hal_gpio_init_in(BUTTON1, GPIO_PULL_DOWN);
    hal_gpio_init_in(BUTTON2, GPIO_PULL_DOWN);

    while (1) {
        for (i = 0; i < 2; ++i) {
            int val = hal_gpio_read(button[i]);
            if (state[i] == 0 && val > 0) {
                gatt_svr_hid_report[2] = 0x00;
                ble_gatts_chr_updated(0x21);
                hal_gpio_clear(LED_EYE1);
                state[i] = 1;
            } else if (state[i] == 1 && val == 0) {
                gatt_svr_hid_report[2] = report_char[i];
                ble_gatts_chr_updated(0x21);
                hal_gpio_set(LED_EYE1);
                state[i] = 0;
            }
        }
        os_time_delay(OS_TICKS_PER_SEC / 100);
    }
}

static void
led_task_handler(void *unused)
{
    struct os_event *ev = NULL;

    while (1) {
        led_scroll("flat");
        led_scroll("upright");
        led_scroll("dspill is odious");
        led_static("FL");
        os_time_delay(3000);
        led_static("UP");
        os_time_delay(3000);
        led_static("r*");
        os_time_delay(3000);
    }

    while (1) {
        ev = os_eventq_get(&led_evq);
        os_time_delay(1000);
        if (ev != NULL)
            console_printf("ok\n");
    }
}

static void
power_led_task_handler(void *unused)
{
    hal_gpio_init_out(LED_EYE1, 0);
    while (1) {
        hal_gpio_set(LED_EYE2);
        os_time_delay(OS_TICKS_PER_SEC / 30);
        hal_gpio_clear(LED_EYE2);
        os_time_delay(3 * OS_TICKS_PER_SEC);
    }
}

static void
accel_task_handler(void *unused)
{
    // hal_i2c_init(0);
    while (1) {
        os_time_delay(OS_TICKS_PER_SEC);
    }
}

/**
 * main
 *
 * The main function for the project. This function initializes the os, calls
 * init_tasks to initialize tasks (and possibly other objects), then starts the
 * OS. We should not return from os start.
 *
 * @return int NOTE: this function should never return!
 */
int
main(void)
{
    struct ble_hs_cfg cfg;
    uint32_t seed;
    int rc;
    int i;

    g_dev_addr[0] = NRF_FICR->DEVICEADDRTYPE;
    memcpy(g_dev_addr, (void *)NRF_FICR->DEVICEADDR + 2, 6);

    /* Initialize OS */
    os_init();

    /* Set cputime to count at 1 usec increments */
    rc = cputime_init(1000000);
    assert(rc == 0);

    /* Seed random number generator with least significant bytes of device
     * address.
     */
    seed = 0;
    for (i = 0; i < 4; ++i) {
        seed |= g_dev_addr[i];
        seed <<= 8;
    }
    srand(seed);

    /* Initialize msys mbufs. */
    rc = os_mempool_init(&quacker_mbuf_mpool, MBUF_NUM_MBUFS,
                         MBUF_MEMBLOCK_SIZE, quacker_mbuf_mpool_data,
                         "quacker_mbuf_data");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&quacker_mbuf_pool, &quacker_mbuf_mpool,
                           MBUF_MEMBLOCK_SIZE, MBUF_NUM_MBUFS);
    assert(rc == 0);

    rc = os_msys_register(&quacker_mbuf_pool);
    assert(rc == 0);

    /* NFFS */
    _nffs_init();

    /* LEDs */
    led_init();

    /* Initialize the logging system. */
    log_init();
    log_console_handler_init(&quacker_log_console_handler);
    log_register("quacker", &quacker_log, &quacker_log_console_handler);

    os_task_init(&quacker_task, "quacker", quacker_task_handler,
                 NULL, QUACKER_TASK_PRIO, OS_WAIT_FOREVER,
                 quacker_stack, QUACKER_STACK_SIZE);

    os_task_init(&button_task, "button", button_task_handler,
                 NULL, BUTTON_TASK_PRIO, OS_WAIT_FOREVER,
                 button_stack, BUTTON_STACK_SIZE);

    os_task_init(&led_task, "led", led_task_handler,
                 NULL, LED_TASK_PRIO, OS_WAIT_FOREVER,
                 led_stack, LED_STACK_SIZE);

    os_task_init(&power_led_task, "power_led", power_led_task_handler,
                 NULL, POWER_LED_TASK_PRIO, OS_WAIT_FOREVER,
                 power_led_stack, POWER_LED_STACK_SIZE);

    os_task_init(&accel_task, "accel", accel_task_handler,
                 NULL, ACCEL_TASK_PRIO, OS_WAIT_FOREVER,
                 accel_stack, ACCEL_STACK_SIZE);

    /* Initialize the keystore */
    rc = keystore_init();
    assert(rc == 0);

    /* Initialize the BLE LL */
    rc = ble_ll_init(BLE_LL_TASK_PRI, MBUF_NUM_MBUFS, BLE_MBUF_PAYLOAD_SIZE);
    assert(rc == 0);

    /* Initialize the BLE host. */
    cfg = ble_hs_cfg_dflt;
    cfg.max_hci_bufs = 3;
    cfg.max_connections = 1;
    cfg.max_attrs = 42;
    cfg.max_services = 5;
    cfg.max_client_configs = 10;
    cfg.max_gattc_procs = 2;
    cfg.max_l2cap_chans = 3;
    cfg.max_l2cap_sig_procs = 1;
    cfg.sm_bonding = 1;
    cfg.sm_our_key_dist = BLE_L2CAP_SM_PAIR_KEY_DIST_ENC;
    cfg.sm_their_key_dist = BLE_L2CAP_SM_PAIR_KEY_DIST_ENC;

    /* Initialize eventq */
    os_eventq_init(&quacker_evq);

    rc = ble_hs_init(&quacker_evq, &cfg);
    assert(rc == 0);

    /* Initialize LED eventq */
    os_eventq_init(&led_evq);

    /* Initialize the console (for log output). */
    rc = console_init(NULL);
    assert(rc == 0);

    /* Register GATT attributes (services, characteristics, and
     * descriptors).
     */
    gatt_svr_init();

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return 0;
}

static int
_nffs_init(void)
{
    struct nffs_area_desc descs[NFFS_AREA_MAX];
    int rc;
    int cnt;

    rc = hal_flash_init();
    assert(rc == 0);

    rc = nffs_init();
    assert(rc == 0);

    cnt = NFFS_AREA_MAX;
    rc = flash_area_to_nffs_desc(FLASH_AREA_NFFS, &cnt, descs);
    assert(rc == 0);
    if (nffs_detect(descs) == FS_ECORRUPT) {
        rc = nffs_format(descs);
        assert(rc == 0);
    }

    return rc;
}
