// =====================================================================
//  Три задачи FreeRTOS
//    acqTask  — сбор и обработка (запуск семафором от таймера)
//    appTask  — логика use case: калибровка, два канала, события
//    commTask — Wi-Fi/MQTT, публикация (свой период, vTaskDelayUntil)
// =====================================================================
#pragma once
#include <Arduino.h>

extern TaskHandle_t hAcq;
extern TaskHandle_t hApp;
extern TaskHandle_t hComm;

void acqTask(void *arg);
void appTask(void *arg);
void commTask(void *arg);
