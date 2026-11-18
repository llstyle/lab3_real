// =====================================================================
//  Объекты синхронизации, таймер выборки, общее состояние под mutex
// =====================================================================
#include "ipc.h"
#include "config.h"

SemaphoreHandle_t semSample = nullptr;
SemaphoreHandle_t mtxShared = nullptr;
QueueHandle_t qMeas = nullptr;
QueueHandle_t qEvt = nullptr;

static SharedState shared = {};               // доступ только под mtxShared
static hw_timer_t *sampleTimer = nullptr;
static volatile uint32_t isrTicks = 0;        // пишет только ISR

// Счётчик неудачных захватов mutex нельзя хранить под тем же mutex,
// поэтому он защищён критической секцией (spinlock).
static portMUX_TYPE diagMux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t mutexTimeouts = 0;

static void countMutexTimeout()
{
    portENTER_CRITICAL(&diagMux);
    mutexTimeouts++;
    portEXIT_CRITICAL(&diagMux);
}

bool ipcCreate()
{
    semSample = xSemaphoreCreateBinary();
    mtxShared = xSemaphoreCreateMutex();
    qMeas = xQueueCreate(MEAS_QUEUE_LEN, sizeof(Measurement));
    qEvt = xQueueCreate(EVT_QUEUE_LEN, sizeof(Event));
    return semSample && mtxShared && qMeas && qEvt;
}

// ---------------------------------------------------------------------
//  Прерывание таймера: только выдать семафор, никакой обработки.
//  В ISR разрешены лишь функции ...FromISR. Mutex выдать отсюда нельзя.
// ---------------------------------------------------------------------
static void IRAM_ATTR onSampleTimer()
{
    isrTicks++;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(semSample, &woken);   // если уже выдан — тик теряется
    if (woken == pdTRUE)
        portYIELD_FROM_ISR();                    // сразу переключиться на задачу сбора
}

bool sampleTimerStart()
{
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    sampleTimer = timerBegin(1000000);                    // 1 МГц: 1 тик = 1 мкс
    if (!sampleTimer)
        return false;
    timerAttachInterrupt(sampleTimer, &onSampleTimer);
    timerAlarm(sampleTimer, SAMPLE_PERIOD_US, true, 0);   // автоперезапуск
#else
    sampleTimer = timerBegin(0, 80, true);                // 80 МГц / 80 = 1 МГц
    if (!sampleTimer)
        return false;
    timerAttachInterrupt(sampleTimer, &onSampleTimer, true);
    timerAlarmWrite(sampleTimer, SAMPLE_PERIOD_US, true);
    timerAlarmEnable(sampleTimer);
#endif
    return true;
}

uint32_t sampleTimerTicks()
{
    return isrTicks;   // 32-битное чтение на ESP32 атомарно
}

// ---------------------------------------------------------------------
//  Общее состояние: захватить mutex → скопировать → сразу отпустить
// ---------------------------------------------------------------------
bool sharedWriteValue(const Measurement &m)
{
    if (xSemaphoreTake(mtxShared, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE)
    {
        countMutexTimeout();
        return false;
    }
    shared.seq = m.seq;
    shared.tMs = m.tMs;
    shared.rawCm = m.rawCm;
    shared.fCm = m.fCm;
    shared.ok = m.ok;
    shared.noEcho = m.noEcho;
    xSemaphoreGive(mtxShared);
    return true;
}

bool sharedWriteStatus(bool calibrated, bool tamper, float refCm, float dev, float devPk)
{
    if (xSemaphoreTake(mtxShared, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE)
    {
        countMutexTimeout();
        return false;
    }
    shared.calibrated = calibrated;
    shared.tamper = tamper;
    shared.refCm = refCm;
    shared.dev = dev;
    shared.devPk = devPk;
    xSemaphoreGive(mtxShared);
    return true;
}

bool sharedRead(SharedState &out)
{
    if (xSemaphoreTake(mtxShared, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE)
    {
        countMutexTimeout();
        return false;
    }
    out = shared;                      // копия, дальше работаем без mutex
    xSemaphoreGive(mtxShared);

    portENTER_CRITICAL(&diagMux);
    out.mutexTimeouts = mutexTimeouts;
    portEXIT_CRITICAL(&diagMux);
    return true;
}

void sharedCount(DiagCounter c, uint32_t n)
{
    if (xSemaphoreTake(mtxShared, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE)
    {
        countMutexTimeout();
        return;
    }
    switch (c)
    {
    case DIAG_SEM_TIMEOUT:     shared.semTimeouts += n; break;
    case DIAG_TICK_MISSED:     shared.ticksMissed += n; break;
    case DIAG_MEAS_DROPPED:    shared.measDropped += n; break;
    case DIAG_EVT_OVERWRITTEN: shared.evtOverwritten += n; break;
    case DIAG_ECHO_MISSED:     shared.echoMissed += n; break;
    }
    xSemaphoreGive(mtxShared);
}

// ---------------------------------------------------------------------
//  Очередь событий: политика «вытеснить самое старое».
//  Сначала ждём место EVT_TX_TIMEOUT_MS; если очередь всё ещё полна
//  (связи нет давно), удаляем самое старое событие и кладём новое —
//  сохраняются самые свежие события, приложение не блокируется надолго.
// ---------------------------------------------------------------------
bool eventPush(const Event &e)
{
    if (xQueueSend(qEvt, &e, pdMS_TO_TICKS(EVT_TX_TIMEOUT_MS)) == pdTRUE)
        return true;

    Event oldest;
    if (xQueueReceive(qEvt, &oldest, 0) == pdTRUE)
    {
        sharedCount(DIAG_EVT_OVERWRITTEN);
        Serial.printf("# event queue full: dropped event #%lu\r\n",
                      (unsigned long)oldest.seq);
    }
    return xQueueSend(qEvt, &e, 0) == pdTRUE;
}
