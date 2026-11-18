// =====================================================================
//  Задача коммуникации (приоритет 1, ядро 0, период COMM_PERIOD_MS)
//  Каждый цикл: испытание связи → Wi-Fi → MQTT → телеметрия (последнее
//  значение под mutex) → опустошение очереди событий → дисплей → диагностика.
//  Дисплей (I2C) обслуживает только эта задача, поэтому шину не делят.
//  Пока связи нет, события остаются в очереди и ждут.
// =====================================================================
#include "tasks.h"
#include "config.h"
#include "ipc.h"
#include "net.h"
#include "message.h"
#include "display.h"

TaskHandle_t hComm = nullptr;

static char payload[MQTT_BUFFER];

// Телеметрия: копия общего состояния под mutex → JSON → topic telemetry
static bool publishTelemetry(uint32_t pubSeq)
{
    SharedState s;
    if (!sharedRead(s))
    {
        Serial.println("# COMM: mutex busy, telemetry skipped");
        return false;
    }
    if (buildTelemetry(payload, sizeof(payload), s, pubSeq,
                       uxQueueMessagesWaiting(qEvt), epochNow()) < 0)
        return false;
    return mqttPublish(TOPIC_TELEMETRY, payload);
}

// События: брать по одному; если публикация не удалась — вернуть в начало
// очереди, чтобы порядок сохранился и событие ушло при следующей попытке.
static int publishEvents()
{
    int sent = 0;
    Event e;
    while (sent < MAX_EVT_PER_CYCLE && xQueueReceive(qEvt, &e, 0) == pdTRUE)
    {
        bool ok = buildEvent(payload, sizeof(payload), e, millis()) > 0 &&
                  mqttPublish(TOPIC_EVENT, payload);
        if (!ok)
        {
            if (xQueueSendToFront(qEvt, &e, 0) != pdTRUE)
                sharedCount(DIAG_EVT_OVERWRITTEN);   // очередь успела заполниться
            break;
        }
        sent++;
    }
    return sent;
}

static void printDiag(bool up)
{
    SharedState s;
    if (!sharedRead(s))
        return;
    Serial.printf("# DIAG: link=%s%s tamper=%d q_meas=%u/%d q_evt=%u/%d "
                  "sem_to=%lu tick_miss=%lu meas_drop=%lu evt_over=%lu mtx_to=%lu echo_miss=%lu\r\n",
                  up ? "UP" : "DOWN", linkTestActive() ? "(test)" : "",
                  s.tamper ? 1 : 0,
                  (unsigned)uxQueueMessagesWaiting(qMeas), MEAS_QUEUE_LEN,
                  (unsigned)uxQueueMessagesWaiting(qEvt), EVT_QUEUE_LEN,
                  (unsigned long)s.semTimeouts, (unsigned long)s.ticksMissed,
                  (unsigned long)s.measDropped, (unsigned long)s.evtOverwritten,
                  (unsigned long)s.mutexTimeouts, (unsigned long)s.echoMissed);
    // Минимальный остаток стека за всё время (байт) — для обоснования размеров
    Serial.printf("# DIAG: stack free min: acq=%u app=%u comm=%u\r\n",
                  (unsigned)uxTaskGetStackHighWaterMark(hAcq),
                  (unsigned)uxTaskGetStackHighWaterMark(hApp),
                  (unsigned)uxTaskGetStackHighWaterMark(nullptr));
}

void commTask(void *arg)
{
    (void)arg;
    if (!displayBegin())
        Serial.println("# DISPLAY: SSD1306 not found, running without it");
    netBegin();

    const TickType_t period = pdMS_TO_TICKS(COMM_PERIOD_MS);
    TickType_t lastWake = xTaskGetTickCount();
    uint32_t pubSeq = 0;
    uint32_t cycle = 0;

    for (;;)
    {
        // Собственный период, не зависящий от задачи сбора
        vTaskDelayUntil(&lastWake, period);

        linkTestPoll();                                  // кнопка / команды w, m

        bool up = wifiEnsure() && mqttEnsure();          // попытка, без ожидания
        if (up)
        {
            mqttService();
            if (publishTelemetry(pubSeq))
                pubSeq++;
            else
                Serial.println("# MQTT: telemetry not sent");
            if (pubSeq > 0 && pubSeq % DIAG_EVERY_S == 0)
                Serial.printf("# MQTT: telemetry published: %lu\r\n", (unsigned long)pubSeq);
            int n = publishEvents();
            if (n > 0)
                Serial.printf("# MQTT: events published: %d, queued: %u\r\n",
                              n, (unsigned)uxQueueMessagesWaiting(qEvt));
        }

        SharedState s;
        if (sharedRead(s))
            displayShow(s, up, linkTestActive(), uxQueueMessagesWaiting(qEvt));

        if (++cycle % DIAG_EVERY_S == 0)
            printDiag(up);
    }
}
