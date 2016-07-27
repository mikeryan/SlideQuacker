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

/**
 * This file implements a simple in-RAM key database for long-term keys.  A key
 * is inserted into the database immediately after a successful pairing
 * procedure.  A key is retrieved from the database when the central performs
 * the encryption procedure (bonding).
 *
 * This has been modified from the original version from 0.9.0 to write data to
 * a file in NFFS.
 */

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include <bsp/bsp.h>
#include <fs/fsutil.h>

#include "host/ble_hs.h"

#define KEYSTORE_FILE "/keystore.bin"

#define KEYSTORE_MAX_ENTRIES   4

struct keystore_entry {
    uint64_t rand_num;
    uint16_t ediv;
    uint8_t ltk[16];

    unsigned authenticated:1;

    /* XXX: authreq. */
};

static struct keystore_entry keystore_entries[KEYSTORE_MAX_ENTRIES];
static int keystore_num_entries;

static int keystore_load(void);
static int keystore_save(void);

/**
 * Searches the database for a long-term key matching the specified criteria.
 *
 * @return                      0 if a key was found; else BLE_HS_ENOENT.
 */
int
keystore_lookup(uint16_t ediv, uint64_t rand_num,
                void *out_ltk, int *out_authenticated)
{
    struct keystore_entry *entry;
    int i;

    for (i = 0; i < keystore_num_entries; i++) {
        entry = keystore_entries + i;

        if (entry->ediv == ediv && entry->rand_num == rand_num) {
            memcpy(out_ltk, entry->ltk, sizeof entry->ltk);
            *out_authenticated = entry->authenticated;

            return 0;
        }
    }

    return BLE_HS_ENOENT;
}

/**
 * Adds the specified key to the database and saves the database to NFFS.
 *
 * @return                      0 on success; FS error on failure
 */
int
keystore_add(uint16_t ediv, uint64_t rand_num, uint8_t *ltk, int authenticated)
{
    struct keystore_entry *entry;

    if (keystore_num_entries >= KEYSTORE_MAX_ENTRIES) {
        memmove(keystore_entries, keystore_entries + 1,
                sizeof(struct keystore_entry) * (KEYSTORE_MAX_ENTRIES - 1));
        keystore_num_entries = KEYSTORE_MAX_ENTRIES - 1;
    }

    entry = keystore_entries + keystore_num_entries;
    keystore_num_entries++;

    entry->ediv = ediv;
    entry->rand_num = rand_num;
    memcpy(entry->ltk, ltk, sizeof entry->ltk);
    entry->authenticated = authenticated;

    return keystore_save();
}

/**
 * Load the keystore from NFFS. Create a new keystore if needed.
 *
 * @return                      0 on success; fs error on failure
 */
int
keystore_init(void)
{
    int rc;

    // load keystore
    rc = keystore_load();
    if (rc != 0) {
        // create a new blank keystore if one doesn't exist
        keystore_num_entries = 0;
        memset(keystore_entries, 0, sizeof(keystore_entries));
        rc = keystore_save();
    }

    return rc;
}

/**
 * Load the keystore from NFFS.
 *
 * @return                      0 on success; -1 or fs error on failure
 */
static int
keystore_load(void) {
    int rc;
    unsigned char file[sizeof(keystore_num_entries) + sizeof(keystore_entries)];

    rc = fsutil_read_file(KEYSTORE_FILE, 0, sizeof(file), file, NULL);
    if (rc != 0) {
        return rc;
    }

    memcpy(&keystore_num_entries, file, sizeof(keystore_num_entries));
    memcpy(keystore_entries, file + sizeof(keystore_num_entries),
            sizeof(keystore_entries));

    if (keystore_num_entries > KEYSTORE_MAX_ENTRIES)
    {
        return -1;
    }

    return 0;
}

/**
 * Save the keystore to NFFS.
 *
 * @return                      0 on success; fs error on failure
 */
static int
keystore_save(void) {
    int rc;
    unsigned char file[sizeof(keystore_num_entries) + sizeof(keystore_entries)];

    memcpy(file, &keystore_num_entries, sizeof(keystore_num_entries));
    memcpy(file + sizeof(keystore_num_entries),
            keystore_entries, sizeof(keystore_entries));

    // save keys
    rc = fsutil_write_file(KEYSTORE_FILE, file, sizeof(file));
    return rc;
}
