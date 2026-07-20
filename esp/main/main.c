#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "train.h"
#include "esp_log.h"

#include "util.h"
#include "network.h"

#define LOGI(fmt, ...) ESP_LOGI("main", fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) ESP_LOGE("main", fmt, ##__VA_ARGS__)

// Only used for the local demo. Can be commented out if not needed
// This is also true for aifes_train_demo and aifes_client_initialize_glorot_weights();
void training_task(void *pv) {
	aifes_train_demo();
	vTaskDelete(NULL);
}

void app_main(void) {
	init_wifi();

	// USE ONLY ONE OF THESE!

	// This is for federated learning
	xTaskCreate(network_task, "network_comm", 65536, NULL, 5, NULL);
	// This is the local demo training the AI fully locally 
	// xTaskCreate(training_task, "training", 65536, NULL, 1, NULL);
}

#undef LOGI
#undef LOGE

// TODO PLAN
// FINAL dynamic IP handling here and in the flower client
