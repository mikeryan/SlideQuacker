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

#ifndef H_BSP_H
#define H_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Define special stackos sections */
#define sec_data_core   __attribute__((section(".data.core")))
#define sec_bss_core    __attribute__((section(".bss.core")))

/* More convenient section placement macros. */
#define bssnz_t

/* LED pins */
#define LED_BLINK_PIN   (14)

#define LED_0           (0)
#define LED_1           (1)
#define LED_2           (2)
#define LED_3           (3)
#define LED_4           (4)
#define LED_5           (5)
#define LED_6           (6)
#define LED_7           (7)
#define LED_8           (8)
#define LED_9           (9)
#define LED_A           (10)
#define LED_B           (11)
#define LED_C           (12)
#define LED_D           (13)

/* EYE1 is next to the buttons and EYE2 is next to the bill */
#define LED_EYE1        (14)
#define LED_EYE2        (15)

/* BUTTON1 is the back button, BUTTON2 is forward */
#define BUTTON1         (29)
#define BUTTON2         (28)

/* UART info */
#define CONSOLE_UART    0

int bsp_imgr_current_slot(void);

#define NFFS_AREA_MAX    (8)


#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
