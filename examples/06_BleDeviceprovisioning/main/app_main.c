/**
 * \copyright Copyright (c) 2019-2024, Buildstorm Pvt Ltd
 *
 * \file app_main.c
 * \brief app_main.c file.
 *
 * The app_main.c is the main entry of the application.
 *
 *
 */

/* Includes ------------------------------------------------------------------*/

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lib_ble.h"
#include "lib_system.h"
#include "lib_gpio.h"
#include "app_config.h"

/* Macros ------------------------------------------------------------------*/

#define thisModule APP_MODULE_MAIN

#define STR_SHADOW_KEY_LED "LED"
#define STR_SHADOW_KEY_COLOR "COLOR"

#define DESIRED_LED_STATE 0
#define DESIRED_COLOR "RED"

/* Certificates ---------------------------------------------------------*/
extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");

/* Variables -----------------------------------------------------------------*/
int32_t gDesiredLedState_s32 = 0, gReportedLedState_s32 = 0;
char gDesiredColorStr[12] = DESIRED_COLOR, gReportedColorStr[12] = DESIRED_COLOR;

void classicShadowUpdateCallBack(uint8_t elementIndex_u8, const char *pKeyStr, const void *pValue);

#define CLASSIC_SHADOW 0
#define MAX_TYPES_OF_SHADOWS 1

const awsShadowElement_st classicShadowElements[] = {
    /* Shaodw Value type          Key         value                    needToPublish shadowUpdateType*/
    {SHADOW_VALUE_TYPE_STRING, "fw_ver", s_value : {pStr : APP_VERSION}, TRUE, SHADOW_UPDATE_TYPE_ALL},
    {SHADOW_VALUE_TYPE_INT, "LED", s_value : {val_i32 : 0}, TRUE, SHADOW_UPDATE_TYPE_ALL},
    {SHADOW_VALUE_TYPE_STRING, "COLOR", s_value : {pStr : DESIRED_COLOR}, TRUE, SHADOW_UPDATE_TYPE_ALL},
};

const shadowConfigTable_st shadowTable[MAX_TYPES_OF_SHADOWS] =
    {
        {
            // CLASSIC_SHADOW
            ptrShadowName : NULL,
            maxElementCount_u8 : (sizeof(classicShadowElements) / sizeof(classicShadowElements[0])),
            callbackHandler : classicShadowUpdateCallBack,
            pShadowElementsTable : classicShadowElements,
        },
};

void classicShadowUpdateCallBack(uint8_t elementIndex_u8, const char *pKeyStr, const void *pValue)
{
    printf("\r\nClassic shadow: %s", pKeyStr);
    if (elementIndex_u8 == 1) // 0-FV, 1-LED, 2-COLOR
    {
        gDesiredLedState_s32 = *(uint8_t *)pValue;
        printf("\r\nDelta update desired Led : %ld", gDesiredLedState_s32);
    }
    else if (elementIndex_u8 == 2) // 0-FV, 1-LED, 2-COLOR
    {
        if ((strcmp((char *)pValue, "WHITE") == 0) ||
            (strcmp((char *)pValue, "RED") == 0) ||
            (strcmp((char *)pValue, "GREEN") == 0) ||
            (strcmp((char *)pValue, "BLUE") == 0))
        {
            strcpy(gDesiredColorStr, (char *)pValue);
            printf("\r\ngDesiredColorStr:%s gReportedColorStr:%s", gDesiredColorStr, gReportedColorStr);
        }
    }
}

void app_eventsCallBackHandler(systemEvents_et event_e)
{
    switch (event_e)
    {
    case EVENT_WIFI_CONNECTED:
        printf("\r\nEVENT_WIFI_CONNECTED");
        break;
    case EVENT_WIFI_DISCONNECTED:
        printf("\r\nEVENT_WIFI_DISCONNECTED");
        break;

    case EVENT_MQTT_CONNECTED:
        printf("\r\nEVENT_MQTT_CONNECTED");
        break;
    case EVENT_MQTT_DISCONNECTED:
        printf("\r\nEVENT_MQTT_DISCONNECTED");
        break;

    case EVENT_BLE_CONNECTED:
        printf("\r\nEVENT_BLE_CONNECTED");
        break;
    case EVENT_BLE_AUTHENTICATED:
        printf("\r\nEVENT_BLE_AUTHENTICATED");
        break;
    case EVENT_BLE_DISCONNECTED:
        printf("\r\nEVENT_BLE_DISCONNECTED");
        break;

    default:
        break;
    }
}

void app_task(void *param)
{
    uint32_t nextMsgTime_u32 = 0;

    while (1)
    {
        switch (SYSTEM_getMode())
        {
        case SYSTEM_MODE_DEVICE_CONFIG:
            if (millis() > nextMsgTime_u32)
            {
                nextMsgTime_u32 = millis() + 2000;

                if (FLASH_isDeviceRegistered())
                {
                    printf("\r\nDevice successfully provisioned");
                }
                else
                {
                    printf("\r\nDevice is not provisioned");
                }
            }
            break;

        case SYSTEM_MODE_NORMAL:
            if (gDesiredLedState_s32 != gReportedLedState_s32)
            {
                gReportedLedState_s32 = gDesiredLedState_s32;
                GPIO_pinWrite(LED0_PIN, gDesiredLedState_s32);
                printf("\r\ngDesiredLedState_s32:%ld gReportedLedState_s32:%ld", gDesiredLedState_s32, gReportedLedState_s32);
                SHADOW_update(CLASSIC_SHADOW, STR_SHADOW_KEY_LED, &gReportedLedState_s32, SHADOW_UPDATE_TYPE_REPORTED);
            }
            else if (strcmp(gDesiredColorStr, gReportedColorStr) != 0)
            {
                strcpy(gReportedColorStr, gDesiredColorStr);
                printf("\r\ngDesiredColorStr:%s gReportedColorStr:%s", gDesiredColorStr, gReportedColorStr);
                SHADOW_update(CLASSIC_SHADOW, STR_SHADOW_KEY_COLOR, gReportedColorStr, SHADOW_UPDATE_TYPE_REPORTED);
            }
            break;

        case SYSTEM_MODE_OTA:
            break;

        default:
            break;
        }
        TASK_DELAY_MS(200);
    }
}

/**
 * @brief    entry point of the project
 * @param    None
 * @return   None
 */
void app_main()
{
    systemInitConfig_st sysConfig = {
        .systemEventCallbackHandler = app_eventsCallBackHandler,
        .pAppVersionStr = APP_VERSION,

        .s_mqttClientConfig = {
            .maxPubMsgToStore_u8 = 6,
            .maxSubMsgToStore_u8 = 4,
            .maxSubscribeTopics_u8 = 6,
            .maxJobs_u8 = 2,
            .pRootCaStr = (char *)aws_root_ca_pem_start,
        }};

    GPIO_pinMode(LED0_PIN, GPIO_MODE_OUTPUT, GPIO_INTR_DISABLE, NULL);

    if (SYSTEM_init(&sysConfig) == TRUE)
    {
        BLE_init();
        SYSTEM_start();

        if (SYSTEM_getMode() == SYSTEM_MODE_NORMAL)
        {

            SHADOW_register(shadowTable, MAX_TYPES_OF_SHADOWS);
        }

        BaseType_t err = xTaskCreate(&app_task, "app_task", TASK_APP_STACK_SIZE, NULL, TASK_APP_PRIORITY, NULL);
        if (pdPASS != err)
        {
            printf("\r\nError 0x%X in creating app_task \r\n restarting system\r\n\r\r\n\r", err);
            fflush(stdout);
            esp_restart();
        }
    }
    else
    {
        while (1)
        {
            printf("\r\nSystem Init failed, verify the init config ....");
            TASK_DELAY_MS(5000);
        }
    }
}
