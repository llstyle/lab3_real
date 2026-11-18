// =====================================================================
//  HC-SR04: Trig — импульс 10 мкс; Echo — импульс, длительность которого
//  равна времени полёта звука туда и обратно.
//
//  Прерывание по обоим фронтам Echo запоминает время подъёма и спада
//  (micros) и уведомляет задачу сбора. Задача не крутится в цикле
//  ожидания (как pulseIn), а спит до уведомления или тайм-аута.
// =====================================================================
#include "sensor.h"
#include "config.h"

static TaskHandle_t ownerTask = nullptr;
static volatile uint32_t riseUs = 0;
static volatile uint32_t widthUs = 0;
static volatile bool armed = false;      // ждём эхо только после своего Trig

static void IRAM_ATTR onEcho()
{
    uint32_t now = micros();
    if (!armed)
        return;
    if (digitalRead(ECHO_PIN))
    {
        riseUs = now;                    // фронт: эхо-импульс начался
    }
    else
    {
        widthUs = now - riseUs;          // спад: импульс закончился
        armed = false;
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR(ownerTask, &woken);
        if (woken == pdTRUE)
            portYIELD_FROM_ISR();
    }
}

void sensorBegin(TaskHandle_t owner)
{
    ownerTask = owner;
    pinMode(TRIG_PIN, OUTPUT);
    digitalWrite(TRIG_PIN, LOW);
    pinMode(ECHO_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(ECHO_PIN), onEcho, CHANGE);
}

float sensorMeasureCm()
{
    ulTaskNotifyTake(pdTRUE, 0);         // сбросить старое уведомление
    widthUs = 0;
    armed = true;

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Ждём уведомление из прерывания, но не дольше тайм-аута
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ECHO_TIMEOUT_MS)) == 0)
    {
        armed = false;
        return -1.0f;                    // эхо не пришло
    }
    return widthUs / US_PER_CM;
}
