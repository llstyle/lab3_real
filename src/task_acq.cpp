// =====================================================================
//  Задача сбора и обработки (приоритет 3, ядро 1)
//  Ждёт семафор от таймера → измеряет расстояние HC-SR04 → проверка
//  достоверности → медиана → среднее → общее состояние (mutex) → очередь.
// =====================================================================
#include "tasks.h"
#include "config.h"
#include "ipc.h"
#include "sensor.h"
#include "dsp.h"

TaskHandle_t hAcq = nullptr;

void acqTask(void *arg)
{
    (void)arg;
    sensorBegin(xTaskGetCurrentTaskHandle());   // прерывание Echo будит эту задачу

    MedianFilter med;
    MovingAverage avg;
    medianInit(med);
    avgInit(avg);

    float lastCm = -1.0f;        // последний достоверный отсчёт
    uint16_t noEcho = 0;         // недостоверных отсчётов подряд
    uint32_t seq = 0;
    uint32_t lastTicks = 0;

    for (;;)
    {
        // ---- 0. Ожидание момента выборки (семафор от прерывания таймера) ----
        if (xSemaphoreTake(semSample, pdMS_TO_TICKS(SEM_TIMEOUT_MS)) != pdTRUE)
        {
            sharedCount(DIAG_SEM_TIMEOUT);
            Serial.println("# ACQ: no timer tick");
            continue;
        }
        uint32_t ticks = sampleTimerTicks();
        if (lastTicks != 0 && ticks - lastTicks > 1)
            sharedCount(DIAG_TICK_MISSED, ticks - lastTicks - 1);
        lastTicks = ticks;

        uint32_t tMs = millis();

        // ---- 1. Сбор: одно измерение HC-SR04 ----
        float raw = sensorMeasureCm();             // −1 — эха нет

        // ---- 2. Проверка достоверности ----
        bool ok = checkSample(raw);
        if (ok)
        {
            lastCm = raw;
            noEcho = 0;
        }
        else
        {
            if (noEcho < 0xFFFF)
                noEcho++;
            sharedCount(DIAG_ECHO_MISSED);
        }
        if (lastCm < 0)
            continue;                              // ещё ни одного достоверного отсчёта
        float v = ok ? raw : lastCm;               // сбой → прошлый хороший

        // ---- 3. Медиана и скользящее среднее (в миллиметрах) ----
        int32_t vMm = (int32_t)(v * 10.0f + 0.5f);
        float fMm = movingAverage(avg, medianFilter(med, vMm));

        // ---- 4. Измерение ----
        Measurement m;
        m.seq = seq++;
        m.tMs = tMs;
        m.rawCm = raw;
        m.ok = ok ? 1 : 0;
        m.noEcho = noEcho;
        m.vCm = v;
        m.fCm = fMm / 10.0f;

        // ---- 5. Последнее значение — в общее состояние под mutex ----
        sharedWriteValue(m);

        // ---- 6. В очередь задаче приложения, без ожидания ----
        if (xQueueSend(qMeas, &m, 0) != pdTRUE)
            sharedCount(DIAG_MEAS_DROPPED);
    }
}
