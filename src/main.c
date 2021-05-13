#include <stdio.h>
#include <string.h>
#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "dht11.h"
#include "firestore.h"
#include "wifi_utils.h"

static void _send_data(struct dht11_reading *);
static void _firestore_task(void *);

#define TAG                                      "APP_MAIN"
#define DHT11_PIN                                GPIO_NUM_4
#define DOC_MAX_SIZE                             128
#define DOC_EXAMPLE                              "{"                          \
                                                   "\"fields\": {"            \
                                                     "\"humidity\": {"        \
                                                       "\"integerValue\": 55" \
                                                     "},"                     \
                                                     "\"temperature\": {"     \
                                                       "\"integerValue\": 23" \
                                                     "},"                     \
                                                   "}"                        \
                                                 "}"

static uint32_t u32DocLength;
static char tcDoc[DOC_MAX_SIZE];
static struct dht11_reading stDht11Reading;

void app_main(void)
{
  wifi_initialise();
  wifi_wait_connected();

  xTaskCreate(_firestore_task, "firestore", 10240, NULL, 5, NULL);
}

static void _firestore_task(void *pvParameter)
{
  DHT11_init(DHT11_PIN);
  firestore_init();

  while(1)
  {
    stDht11Reading = DHT11_read();
    if((DHT11_OK == stDht11Reading.status) &&
       (0 != stDht11Reading.temperature) &&
       (0 != stDht11Reading.humidity))
    {
      ESP_LOGI(TAG,
               "Temperature: %d °C    Humidity: %d %%",
               stDht11Reading.temperature,
               stDht11Reading.humidity);
      _send_data(&stDht11Reading);
    } 
    else
    {
      ESP_LOGW(TAG,
               "Cannot read from sensor: %s",
               (DHT11_TIMEOUT_ERROR == stDht11Reading.status)
               ?"Timeout"
               :"Bad CRC");
    }
    vTaskDelay(2500 / portTICK_PERIOD_MS);
  }
}

static void _send_data(struct dht11_reading *pstReading)
{
  /* Format document with newly fetched data */
  u32DocLength = snprintf(tcDoc,
                          sizeof(tcDoc),
                          "{\"fields\":{\"humidity\":{\"integerValue\":%d},\"temperature\":{\"integerValue\":%d}}}",
                          pstReading->humidity,
                          pstReading->temperature);
  ESP_LOGD(TAG, "Document length after formatting: %d", u32DocLength);
  if(u32DocLength > 0)
  {
    /* Update document in firestore or create it if it doesn't already exists */
    if(FIRESTORE_OK == firestore_update_document("devices",
                                                 "dht11-node",
                                                 tcDoc,
                                                 &u32DocLength))
    {
      ESP_LOGI(TAG, "Document updated successfully");
      ESP_LOGD(TAG, "Updated document length: %d", u32DocLength);
      ESP_LOGD(TAG, "Updated document content:\r\n%.*s", u32DocLength, tcDoc);
    }
    else
    {
      ESP_LOGE(TAG, "Couldn't update document");
    }
  }
  else
  {
    ESP_LOGE(TAG, "Couldn't format document");
  }
}
