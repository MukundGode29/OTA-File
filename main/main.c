#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "protocol_examples_common.h"
#include "esp_crt_bundle.h"

#define PIN_SWITCH 15

#define TAG "OTA"
SemaphoreHandle_t ota_semaphore;

const int software_version = 101;
//extern const uint8_t server_cert_pem_start[] asm("_binary_github_cer_start");

esp_err_t client_event_handler(esp_http_client_event_t *evt)
{
  return ESP_OK;
}

void on_button_pushed(void *params)
{
//  printf("Button pushed\n");
//  ESP_LOGE(TAG, "Button pushed, starting OTA");
    xSemaphoreGiveFromISR(ota_semaphore, pdFALSE);
}

void run_ota(void *params)
{
  int count = 0;

  while (true)
  {
    xSemaphoreTake(ota_semaphore, portMAX_DELAY);
    ESP_LOGI(TAG, "Invoking OTA");

    // disable the interrupt
    gpio_isr_handler_remove(GPIO_NUM_15);

    // wait some time while we check for the button to be released
    do
    {
        vTaskDelay(30 / portTICK_PERIOD_MS);
    } while (gpio_get_level(GPIO_NUM_15) == 1);

    // do some work
    printf("GPIO %d was pressed %d times. The state is %d\n", GPIO_NUM_15, count++, gpio_get_level(GPIO_NUM_15));

    // re-enable the interrupt
    gpio_isr_handler_add(GPIO_NUM_15, on_button_pushed, (void *)GPIO_NUM_15);

    esp_http_client_config_t clientConfig = {
        .url = "https://github.com/MukundGode29/OTA-File/releases/download/v1.0.0/ExampleConnect.bin", // our ota location
        .event_handler = client_event_handler,
        //.cert_pem = (char *)server_cert_pem_start};
        .crt_bundle_attach = esp_crt_bundle_attach,   // was: .cert_pem = (char *)server_cert_pem_start
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
        .disable_auto_redirect = false,
        .max_redirection_count = 5,
};

    // IDF V5
    // esp_https_ota_config_t ota_config = {
    //     .http_config = &clientConfig};

    // IDF V4.
     if (esp_https_ota(&clientConfig) == ESP_OK)
    // IDF V5
    //if (esp_https_ota(&ota_config) == ESP_OK)
    {
      ESP_LOGI(TAG, "OTA flash succsessfull for version %d.", software_version);
      printf("restarting in 5 seconds\n");
      vTaskDelay(pdMS_TO_TICKS(5000));
      esp_restart();
    }
    ESP_LOGE(TAG, "Failed to update firmware");
   }
}



void app_main(void)
{
 // printf("HAY!!! This is a new feature\n");
  ESP_LOGI("SOFTWARE VERSION", "we are running %d", software_version);
   gpio_config_t gpioConfig = {
      .pin_bit_mask = 1ULL << GPIO_NUM_15,
      .mode = GPIO_MODE_DEF_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLUP_DISABLE,
      .intr_type = GPIO_INTR_NEGEDGE};
  gpio_config(&gpioConfig); 

  ESP_ERROR_CHECK(nvs_flash_init());

  // IDF V4  
    tcpip_adapter_init();

  // IDF V5
  //esp_netif_init();

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(example_connect());


  gpio_install_isr_service(0);
  gpio_isr_handler_add(GPIO_NUM_15, on_button_pushed, NULL);

  ota_semaphore = xSemaphoreCreateBinary();
  xTaskCreate(run_ota, "run_ota", 1024 * 8, NULL, 2, NULL);
}   
 