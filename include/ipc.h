// =====================================================================
//  Объекты внутреннего взаимодействия FreeRTOS
//    semSample   — бинарный семафор: тик таймера → задача сбора
//    mtxShared   — mutex: общее состояние (сбор/приложение → коммуникация)
//    qMeas       — очередь измерений: сбор → приложение
//    qEvt        — очередь событий:   приложение → коммуникация
// =====================================================================
#pragma once
#include <Arduino.h>
#include "types.h"

extern SemaphoreHandle_t semSample;
extern SemaphoreHandle_t mtxShared;
extern QueueHandle_t qMeas;
extern QueueHandle_t qEvt;

// Создать все объекты. false — не хватило памяти.
bool ipcCreate();

// Таймер выборки: прерывание каждые SAMPLE_PERIOD_US выдаёт semSample.
bool sampleTimerStart();
uint32_t sampleTimerTicks();   // сколько раз сработало прерывание

// ---- Доступ к общему состоянию ТОЛЬКО через эти функции (под mutex) ----
// Каждая возвращает false, если mutex не захвачен за MUTEX_TIMEOUT_MS.
bool sharedWriteValue(const Measurement &m);
bool sharedWriteStatus(bool calibrated, bool tamper, float refCm, float dev, float devPk);
bool sharedRead(SharedState &out);           // копия всей структуры

// Счётчики диагностики (тоже под mutex)
enum DiagCounter : uint8_t
{
    DIAG_SEM_TIMEOUT,
    DIAG_TICK_MISSED,
    DIAG_MEAS_DROPPED,
    DIAG_EVT_OVERWRITTEN,
    DIAG_ECHO_MISSED,
};
void sharedCount(DiagCounter c, uint32_t n = 1);

// Положить событие в очередь событий с политикой «вытеснить самое старое».
bool eventPush(const Event &e);
