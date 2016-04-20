/* Copyright Statement:
 *
 * (C) 2005-2016  MediaTek Inc. All rights reserved.
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. ("MediaTek") and/or its licensors.
 * Without the prior written permission of MediaTek and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 * You may only use, reproduce, modify, or distribute (as applicable) MediaTek Software
 * if you have agreed to and been bound by the applicable license agreement with
 * MediaTek ("License Agreement") and been granted explicit permission to do so within
 * the License Agreement ("Permitted User").  If you are not a Permitted User,
 * please cease any access or use of MediaTek Software immediately.
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT MEDIATEK SOFTWARE RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES
 * ARE PROVIDED TO RECEIVER ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 */

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "task_def.h"
#include "syslog.h"

#if defined(MTK_MINICLI_ENABLE)
#include "cli_def.h"
#endif

#ifdef RELAY
#include "relay.h"
#endif

#include "queue.h"
#include "semphr.h"
#ifdef MTK_NVRAM_ENABLE
#include "nvram_handler.h"
#endif
#include "nvdm.h"

#if defined(MTK_HAL_LOWPOWER_ENABLE)
#include "hal_lp.h"
#endif

#ifdef MTK_MINISUPP_ENABLE
#include "wifi_api.h"
#include "wifi_inband.h"
extern int wpa_supplicant_entry(unsigned char);
#if 0
extern int hostapd_entry(void);
#endif
#endif


/****************************************************************************
 * Types.
 ****************************************************************************/

typedef struct tasks_list_ {
    TaskFunction_t      pvTaskCode;
    char                *pcName;
    uint16_t            usStackDepth;
    void                *pvParameters;
    UBaseType_t         uxPriority;
} tasks_list_t;

/****************************************************************************
 * Forward Declarations.
 ****************************************************************************/

#ifdef MTK_MINISUPP_ENABLE
static void wpa_supplicant_task(void *pvParameters);
#endif

/****************************************************************************
 * Local Data
 ****************************************************************************/

static const tasks_list_t _tasks_list[] = {
#ifdef MTK_MINISUPP_ENABLE
    { wpa_supplicant_task,    WPA_SUPPLICANT_TASK_NAME,      WPA_SUPPLICANT_TASK_STACKSIZE,     NULL,   WPA_SUPPLICANT_TASK_PRIO | portPRIVILEGE_BIT  },
#endif

#if defined(MTK_MINICLI_ENABLE)
    { cli_def_task,    CLI_TASK_NAME,      CLI_TASK_STACKSIZE,     NULL,   CLI_TASK_PRIO   },
#endif

#if defined(MTK_NVRAM_ENABLE)
    { prvNvramTask,    NVRAM_TASK_NAME,      NVRAM_TASK_STACKSIZE,     NULL,   NVRAM_TASK_PRIO   },
#endif

#ifdef RELAY
    { relay_uart_task, "rly",      1024,     NULL,   2   },
#endif
};

#define tasks_list_count  (sizeof(_tasks_list) / sizeof(tasks_list_t))

static TaskHandle_t     _task_handles[tasks_list_count];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifdef MTK_MINISUPP_ENABLE
static void wpa_supplicant_task(void *pvParameters)
{
    uint8_t op_mode = 0;

    char buff[2] = {0};
    int sz = 1;
    nvdm_read_data_item("common", "OpMode", (uint8_t *)buff, (uint32_t *)&sz);
    op_mode = buff[0] - 0x30;
    if ((op_mode == 0) || (op_mode > WIFI_MODE_MONITOR)) {
        op_mode = WIFI_MODE_STA_ONLY;
    }

#if defined(MTK_HAL_LOWPOWER_ENABLE)
    if (0 == hal_lp_get_wic_wakeup())
#endif
    {
        LOG_I(common, "%s: Set OpMode(=%d)", __FUNCTION__, op_mode);
        wifi_inband_opmode(1, &op_mode);
    }
    vTaskDelay(1000);

    wpa_supplicant_entry((unsigned char)op_mode);
    vTaskDelete(NULL);
}
#endif

/****************************************************************************
 * Public API
 ****************************************************************************/
extern size_t xPortGetFreeHeapSize(void);

void task_def_create(void)
{
    uint16_t            i;
    BaseType_t          x;
    const tasks_list_t  *t;

    for (i = 0; i < tasks_list_count; i++) {
        t = &_tasks_list[i];

        LOG_I(common, "xCreate task %s, pri %d", t->pcName, (int)t->uxPriority);

        x = xTaskCreate(t->pvTaskCode,
                        t->pcName,
                        t->usStackDepth,
                        t->pvParameters,
                        t->uxPriority,
                        &_task_handles[i]);

        if (x != pdPASS) {
            LOG_E(common, ": failed");
        } else {
            LOG_I(common, ": succeeded");
        }
    }
    LOG_I(common, "Free Heap size:%d bytes", xPortGetFreeHeapSize());
}
