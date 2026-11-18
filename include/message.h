// =====================================================================
//  Формирование сообщений (JSON) для внешней системы
// =====================================================================
#pragma once
#include <Arduino.h>
#include "types.h"

// Возвращают длину строки или -1, если буфер мал.
int buildTelemetry(char *buf, size_t size, const SharedState &s,
                   uint32_t pubSeq, uint32_t evtWaiting, uint32_t epoch);
int buildEvent(char *buf, size_t size, const Event &e, uint32_t nowMs);

const char *eventName(EventType t);
const char *causeName(EventCause c);
