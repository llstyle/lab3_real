// =====================================================================
//  Задача приложения (приоритет 2, ядро 1)
//  Измерения из очереди → калибровка расстояния покоя → три признака
//  вмешательства → светодиод, статус (mutex), события (очередь).
//
//    сдвиг   : |d_f − d_ref| ≥ SHIFT_THRESHOLD_CM   (медленный канал)
//    рывок   : |d − d_f|     ≥ JUMP_THRESHOLD_CM    (быстрый канал, удержание 1 с)
//    нет эха : NO_ECHO_ALARM пропусков подряд     (датчик повернули/закрыли),
//              событие NO_ECHO при каждом начале серии пропусков
// =====================================================================
#include "tasks.h"
#include "config.h"
#include "ipc.h"
#include "net.h"
#include "message.h"

TaskHandle_t hApp = nullptr;

static uint32_t evtSeq = 0;

static void emit(EventType type, EventCause cause, const Measurement &m, float dev, float devPk)
{
    Event e;
    e.seq = evtSeq++;
    e.tMs = m.tMs;
    e.epoch = epochNow();
    e.type = type;
    e.cause = cause;
    e.distCm = m.fCm;
    e.dev = dev;
    e.devPk = devPk;
    if (!eventPush(e))
        Serial.println("# APP: event not queued");
    Serial.printf("# EVENT #%lu %s (%s) d=%.1f cm dev=%.1f dev_pk=%.1f\r\n",
                  (unsigned long)e.seq, eventName(type), causeName(cause),
                  m.fCm, dev, devPk);
}

void appTask(void *arg)
{
    (void)arg;

    uint32_t received = 0;
    uint32_t calibN = 0;
    float refSum = 0, ref = 0;
    bool calibrated = false;

    uint32_t lastJumpMs = 0;
    bool jumpSeen = false;
    bool tamperPrev = false;
    bool lostPrev = false;

    for (;;)
    {
        Measurement m;
        if (xQueueReceive(qMeas, &m, pdMS_TO_TICKS(MEAS_RX_TIMEOUT_MS)) != pdTRUE)
        {
            Serial.println("# APP: no measurements");
            continue;
        }
        received++;

        // ---- Калибровка: среднее расстояние покоя по достоверным отсчётам ----
        if (!calibrated)
        {
            if (received > WARMUP_SAMPLES && m.ok)
            {
                refSum += m.fCm;
                if (++calibN == CALIB_SAMPLES)
                {
                    ref = refSum / CALIB_SAMPLES;
                    calibrated = true;
                    Serial.printf("# calibrated rest distance: %.1f cm\r\n", ref);
                    emit(EVT_CALIBRATED, CAUSE_NONE, m, 0, 0);
                    sharedWriteStatus(true, false, ref, 0, 0);
                }
            }
            continue;
        }

        // ---- Три признака вмешательства ----
        float dev = fabsf(m.fCm - ref);
        float devPk = m.ok ? fabsf(m.vCm - m.fCm) : 0.0f;
        bool shift = dev >= SHIFT_THRESHOLD_CM;
        bool jump = m.ok && devPk >= JUMP_THRESHOLD_CM;
        bool lost = m.noEcho >= NO_ECHO_ALARM;

        bool holdBefore = jumpSeen && (m.tMs - lastJumpMs < JUMP_HOLD_MS);
        if (jump)
        {
            if (!holdBefore)
                emit(EVT_JUMP, CAUSE_JUMP, m, dev, devPk);
            lastJumpMs = m.tMs;
            jumpSeen = true;
        }
        bool jumpHold = jumpSeen && (m.tMs - lastJumpMs < JUMP_HOLD_MS);

        // Пропажа эха — отдельное событие, даже если тревога уже включена
        // другим признаком (например, рукой: сначала срабатывает рывок)
        if (lost && !lostPrev)
            emit(EVT_NO_ECHO, CAUSE_NO_ECHO, m, dev, devPk);
        lostPrev = lost;

        // ---- Решение ----
        bool tamper = shift || jumpHold || lost;
        digitalWrite(LED_PIN, tamper ? HIGH : LOW);

        if (tamper && !tamperPrev)
        {
            EventCause cause = lost ? CAUSE_NO_ECHO : (shift ? CAUSE_SHIFT : CAUSE_JUMP);
            emit(EVT_TAMPER_ON, cause, m, dev, devPk);
        }
        else if (!tamper && tamperPrev)
        {
            emit(EVT_TAMPER_OFF, CAUSE_NONE, m, dev, devPk);
        }
        tamperPrev = tamper;

        sharedWriteStatus(true, tamper, ref, dev, devPk);

#if PRINT_CSV
        Serial.printf("%lu,%.1f,%u,%.1f,%.1f,%.1f,%u,%d\r\n",
                      (unsigned long)m.tMs, m.rawCm, (unsigned)m.ok, m.fCm, dev, devPk,
                      (unsigned)m.noEcho, tamper ? 1 : 0);
#endif
    }
}
