// =====================================================================
//  Типы данных, которыми обмениваются задачи
// =====================================================================
#pragma once
#include <Arduino.h>

// ---- Элемент очереди «сбор → приложение» ----
// Одно обработанное измерение расстояния.
struct Measurement
{
    uint32_t seq;      // номер отсчёта
    uint32_t tMs;      // время от запуска, мс
    float rawCm;       // как пришло с датчика; −1 — эха нет
    uint8_t ok;        // 1 — норма, 0 — нет эха / вне диапазона (заменён)
    uint16_t noEcho;   // сколько недостоверных отсчётов подряд
    float vCm;         // достоверный отсчёт ДО фильтров (для канала рывка)
    float fCm;         // после медианы и среднего
};

// ---- Элемент очереди «приложение → коммуникация» ----
enum EventType : uint8_t
{
    EVT_CALIBRATED = 0,  // расстояние покоя определено
    EVT_TAMPER_ON,       // начало вмешательства
    EVT_TAMPER_OFF,      // конец вмешательства
    EVT_JUMP,            // резкий скачок расстояния
    EVT_NO_ECHO,         // серия пропусков эха (датчик закрыли или повернули)
};

enum EventCause : uint8_t
{
    CAUSE_NONE = 0,
    CAUSE_SHIFT,         // расстояние изменилось (медленный канал)
    CAUSE_JUMP,          // резкий скачок (канал рывка)
    CAUSE_NO_ECHO,       // эхо пропало: датчик повернули или закрыли
};

struct Event
{
    uint32_t seq;
    uint32_t tMs;        // когда произошло (время от запуска), мс
    uint32_t epoch;      // время UTC по NTP (0, если ещё нет)
    EventType type;
    EventCause cause;
    float distCm;        // расстояние после фильтров
    float dev;           // |d_f − d_ref|, см
    float devPk;         // |d − d_f|, см
};

// ---- Общее состояние (защищено mutex) ----
// Пишут: задача сбора (последнее значение) и задача приложения (статус).
// Читает: задача коммуникации (телеметрия и дисплей).
struct SharedState
{
    // последнее значение (пишет задача сбора)
    uint32_t seq;
    uint32_t tMs;
    float rawCm;
    float fCm;
    uint8_t ok;
    uint16_t noEcho;

    // статус use case (пишет задача приложения)
    bool calibrated;
    bool tamper;
    float refCm;
    float dev;
    float devPk;

    // диагностика внутренних каналов
    uint32_t semTimeouts;
    uint32_t ticksMissed;
    uint32_t measDropped;
    uint32_t evtOverwritten;
    uint32_t mutexTimeouts;
    uint32_t echoMissed;     // всего отсчётов без эха
};
