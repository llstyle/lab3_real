// =====================================================================
//  L3. Передача информации во внешнюю среду
//  Многозадачная архитектура FreeRTOS и публикация данных по MQTT
//
//  Use case: домашнее Wi-Fi устройство обнаружения вмешательства.
//  Реальное железо: Heltec WiFi Kit 32 (ESP32 + OLED), ультразвуковой
//  дальномер HC-SR04 (Trig 13, Echo 14 через делитель), светодиод GPIO27,
//  кнопка PRG (GPIO0) — испытание потери связи.
//
//  Архитектура:
//    Таймер (60 мс) --ISR--> semSample --> [acqTask]  измерение + обработка
//         acqTask --mtxShared--> общее состояние --> [commTask] телеметрия
//         acqTask --qMeas--> [appTask] калибровка, 2 канала, события
//         appTask --qEvt--> [commTask] --MQTT--> Wi-Fi --> broker
//
//  Topic:  utm/si254/vereskul/tamper/{telemetry, event, status}
// =====================================================================
#include <Arduino.h>
#include "config.h"
#include "ipc.h"
#include "net.h"
#include "tasks.h"

static void halt(const char *msg)
{
    Serial.println(msg);
    for (;;)
        delay(1000);
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("# L3: HC-SR04 tamper detection, FreeRTOS (3 tasks) + MQTT");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    pinMode(LINK_TEST_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(LINK_TEST_PIN), linkTestButtonIsr, FALLING);

    if (!ipcCreate())
        halt("# ERROR: failed to create semaphore/mutex/queues");

    Serial.printf("# topics: %s/{telemetry,event,status}, broker %s:%d\r\n",
                  TOPIC_PREFIX, MQTT_HOST, MQTT_PORT);
    Serial.printf("# queues: meas %d, evt %d; comm period %d ms; link test: PRG or 'w'/'m'\r\n",
                  MEAS_QUEUE_LEN, EVT_QUEUE_LEN, COMM_PERIOD_MS);

    bool ok =
        xTaskCreatePinnedToCore(acqTask, "acq", ACQ_STACK, nullptr, ACQ_PRIO, &hAcq, ACQ_CORE) == pdPASS &&
        xTaskCreatePinnedToCore(appTask, "app", APP_STACK, nullptr, APP_PRIO, &hApp, APP_CORE) == pdPASS &&
        xTaskCreatePinnedToCore(commTask, "comm", COMM_STACK, nullptr, COMM_PRIO, &hComm, COMM_CORE) == pdPASS;
    if (!ok)
        halt("# ERROR: failed to create tasks");

    // Таймер запускается последним, когда задача сбора уже ждёт семафор
    if (!sampleTimerStart())
        halt("# ERROR: failed to start timer");
}

void loop()
{
    vTaskDelete(nullptr);   // вся работа — в задачах FreeRTOS
}
