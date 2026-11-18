// =====================================================================
//  Сообщения: самодостаточный JSON — устройство, время, значения, единицы
// =====================================================================
#include "message.h"
#include "config.h"

const char *eventName(EventType t)
{
    switch (t)
    {
    case EVT_CALIBRATED: return "CALIBRATED";
    case EVT_TAMPER_ON:  return "TAMPER_ON";
    case EVT_TAMPER_OFF: return "TAMPER_OFF";
    case EVT_JUMP:       return "JUMP";
    }
    return "UNKNOWN";
}

const char *causeName(EventCause c)
{
    switch (c)
    {
    case CAUSE_SHIFT:   return "shift";
    case CAUSE_JUMP:    return "jump";
    case CAUSE_NO_ECHO: return "no_echo";
    default:            return "none";
    }
}

static int checkLen(int n, size_t size)
{
    return (n > 0 && (size_t)n < size) ? n : -1;
}

// Пример:
// {"id":"si254-vereskul-tamper","type":"telemetry","seq":12,"t_ms":15020,
//  "ts":1762524000,"unit":"cm","d_raw":48.3,"d_f":48.1,"ok":1,"no_echo":0,
//  "cal":1,"ref":48.0,"tamper":0,"dev":0.1,"dev_pk":0.2,"q_evt":0,
//  "diag":{"sem_to":0,"tick_miss":0,"meas_drop":0,"evt_over":0,"mtx_to":0,"echo_miss":3}}
int buildTelemetry(char *buf, size_t size, const SharedState &s,
                   uint32_t pubSeq, uint32_t evtWaiting, uint32_t epoch)
{
    int n = snprintf(buf, size,
        "{\"id\":\"%s\",\"type\":\"telemetry\",\"seq\":%lu,\"t_ms\":%lu,\"ts\":%lu,"
        "\"unit\":\"cm\",\"d_raw\":%.1f,\"d_f\":%.1f,\"ok\":%u,\"no_echo\":%u,"
        "\"cal\":%d,\"ref\":%.1f,\"tamper\":%d,\"dev\":%.1f,\"dev_pk\":%.1f,\"q_evt\":%lu,"
        "\"diag\":{\"sem_to\":%lu,\"tick_miss\":%lu,\"meas_drop\":%lu,\"evt_over\":%lu,"
        "\"mtx_to\":%lu,\"echo_miss\":%lu}}",
        DEVICE_ID, (unsigned long)pubSeq, (unsigned long)s.tMs, (unsigned long)epoch,
        s.rawCm, s.fCm, (unsigned)s.ok, (unsigned)s.noEcho,
        s.calibrated ? 1 : 0, s.refCm, s.tamper ? 1 : 0, s.dev, s.devPk,
        (unsigned long)evtWaiting,
        (unsigned long)s.semTimeouts, (unsigned long)s.ticksMissed,
        (unsigned long)s.measDropped, (unsigned long)s.evtOverwritten,
        (unsigned long)s.mutexTimeouts, (unsigned long)s.echoMissed);
    return checkLen(n, size);
}

// Пример:
// {"id":"si254-vereskul-tamper","type":"event","seq":3,"event":"TAMPER_ON",
//  "cause":"shift","t_ms":20140,"ts":1762524005,"d_f":61.2,"dev":13.1,
//  "dev_pk":0.4,"delay_ms":35}
int buildEvent(char *buf, size_t size, const Event &e, uint32_t nowMs)
{
    int n = snprintf(buf, size,
        "{\"id\":\"%s\",\"type\":\"event\",\"seq\":%lu,\"event\":\"%s\",\"cause\":\"%s\","
        "\"t_ms\":%lu,\"ts\":%lu,\"d_f\":%.1f,\"dev\":%.1f,\"dev_pk\":%.1f,\"delay_ms\":%lu}",
        DEVICE_ID, (unsigned long)e.seq, eventName(e.type), causeName(e.cause),
        (unsigned long)e.tMs, (unsigned long)e.epoch, e.distCm, e.dev, e.devPk,
        (unsigned long)(nowMs - e.tMs));
    return checkLen(n, size);
}
