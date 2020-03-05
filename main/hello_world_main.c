#include <stdio.h>
#include <string.h>

#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "Smartconfig.h"
#include "Http.h"
#include "Mqtt.h"
#include "Bluetooth.h"
#include "Json_parse.h"
#include "esp_log.h"

#include "Uart0.h"
#include "RS485_Read.h"
#include "CSE7759B.h"
#include "Led.h"
#include "E2prom.h"
#include "RtcUsr.h"
#include "Switch.h"
#include "ota.h"
#include "ds18b20.h"
#include "user_app.h"
#include "my_spi_flash.h"
#include "Cache_data.h"

void app_main(void)
{
    Switch_Init();
    E2prom_Init();
    Read_Metadate_E2p();
    Read_Product_E2p();
    Read_Fields_E2p();
    SPI_FLASH_Init();

    Uart_Init();
    Led_Init();
    user_app_key_init();

    /* 判断是否有序列号和product id */
    if ((strlen(SerialNum) == 0) || (strlen(ProductId) == 0) || (strlen(WEB_SERVER) == 0)) //未获取到序列号或productid，未烧写序列号
    {
        while (1)
        {
            ESP_LOGE("Init", "no SerialNum or product id!");
            Led_Status = LED_STA_NOSER; //故障灯
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    }

    RS485_Init();
    CSE7759B_Init();
    start_ds18b20();
    Start_Cache();

    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    init_wifi();
    ble_app_init();
    initialise_http(); //须放在 采集任务建立之后
    initialise_mqtt();
}