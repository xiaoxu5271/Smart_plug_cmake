#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

#include "esp_http_client.h"
#include "esp_https_ota.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "Smartconfig.h"
#include "Json_parse.h"
#include "Http.h"
#include "Mqtt.h"
#include "E2prom.h"

#include "ota.h"

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

//数据包长度
#define BUFFSIZE 2048

static const char *TAG = "ota";
//OTA数据
static char ota_write_data[BUFFSIZE + 1] = {0};

uint32_t content_len = 0;

uint8_t ota_dns_host_ip[4];

TaskHandle_t ota_handle = NULL;
// extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
// extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

static void __attribute__((noreturn)) task_fatal_error()
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    esp_restart();
    (void)vTaskDelete(NULL);

    // while (1)
    // {
    //     ;
    // }
}

static void http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

static void wifi_ota_task(void *pvParameter)
{
    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG, "Starting OTA example...");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running)
    {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);

    /* Wait for the callback to set the CONNECTED_BIT in the
       event group.
    */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connect to Wifi ! Start to Connect to Server....");

    // E2prom_page_Read(ota_url_add, (uint8_t *)mqtt_json_s.mqtt_ota_url, 128);
    ESP_LOGI(TAG, "OTA-URL=[%s]\r\n", mqtt_json_s.mqtt_ota_url);

    esp_http_client_config_t config = {
        .url = mqtt_json_s.mqtt_ota_url,
        // .cert_pem = (char *)server_cert_pem_start,   //https 证书
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
        task_fatal_error();
    }
    err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        task_fatal_error();
    }
    esp_http_client_fetch_headers(client);

    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);

    int binary_file_length = 0;
    /*deal with all receive packet*/
    bool image_header_was_checked = false;
    while (1)
    {
        int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
        // printf("\n ota_write_data=%2x \n", (unsigned int)ota_write_data);
        if (data_read < 0)
        {
            ESP_LOGE(TAG, "Error: SSL data read error");
            http_cleanup(client);
            task_fatal_error();
        }
        else if (data_read > 0)
        {
            if (image_header_was_checked == false)
            {
                // esp_app_desc_t new_app_info;
                if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
                {
                    // check current version with downloading
                    // memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                    // ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

                    // esp_app_desc_t running_app_info;
                    // if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
                    // {
                    //     ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
                    // }

                    // const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
                    // esp_app_desc_t invalid_app_info;
                    // if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK)
                    // {
                    //     ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);
                    // }

                    // check current version with last invalid partition
                    // if (last_invalid_app != NULL)
                    // {
                    //     if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0)
                    //     {
                    //         ESP_LOGW(TAG, "New version is the same as invalid version.");
                    //         ESP_LOGW(TAG, "Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
                    //         ESP_LOGW(TAG, "The firmware has been rolled back to the previous version.");
                    //         http_cleanup(client);
                    //         infinite_loop();
                    //     }
                    // }

                    // if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0)
                    // {
                    //     ESP_LOGW(TAG, "Current running version is the same as a new. We will not continue the update.");
                    //     http_cleanup(client);
                    //     infinite_loop();
                    // }

                    image_header_was_checked = true;

                    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                        http_cleanup(client);
                        task_fatal_error();
                    }
                    ESP_LOGI(TAG, "esp_ota_begin succeeded");
                }
                else
                {
                    ESP_LOGE(TAG, "received package is not fit len");
                    http_cleanup(client);
                    task_fatal_error();
                }
            }
            err = esp_ota_write(update_handle, (const void *)ota_write_data, data_read);
            if (err != ESP_OK)
            {
                http_cleanup(client);
                task_fatal_error();
            }
            binary_file_length += data_read;
            ESP_LOGI(TAG, "Written image length %d", binary_file_length);
        }
        else if (data_read == 0)
        {
            ESP_LOGI(TAG, "Connection closed,all data received");
            break;
        }
    }
    ESP_LOGI(TAG, "Total Write binary data length : %d", binary_file_length);

    if (esp_ota_end(update_handle) != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end failed!");
        http_cleanup(client);
        task_fatal_error();
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        http_cleanup(client);
        task_fatal_error();
    }
    ESP_LOGI(TAG, "Prepare to restart system!");
    esp_restart();
    return;
}

void ota_start(void) //建立OTA升级任务，目的是为了让此函数被调用后尽快执行完毕
{
    xTaskCreate(wifi_ota_task, "wifi_ota_task", 8192, NULL, 2, ota_handle);
}