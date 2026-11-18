// =====================================================================
//  Wi-Fi / MQTT: работают только в задаче коммуникации.
//  Ни одна функция не ждёт подключения в цикле: если связи нет,
//  запускается попытка и управление сразу возвращается.
// =====================================================================
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include "net.h"
#include "config.h"

static WiFiClient tcp;
static PubSubClient mqtt(tcp);
static char clientId[40];

static bool wifiWasUp = false;
static bool ntpStarted = false;
static uint32_t wifiLastTryMs = 0;
static bool wifiTried = false;

static uint32_t mqttLastTryMs = 0;
static uint32_t mqttRetryMs = MQTT_RETRY_MIN_MS;
static bool mqttTried = false;
static bool mqttWasUp = false;

// испытание потери связи
static char testMode = 0;            // 0 — нет, 'w' — Wi-Fi, 'm' — MQTT
static uint32_t testUntilMs = 0;
static volatile bool buttonPressed = false;

// ---------------------------------------------------------------------
void netBegin()
{
    uint64_t mac = ESP.getEfuseMac();
    // Уникальный client ID: часть MAC + случайное число при каждом запуске.
    // В Wokwi у всех ESP32 одинаковый MAC; если два клиента подключаются к
    // брокеру с одним ID, брокер отключает предыдущего.
    snprintf(clientId, sizeof(clientId), "%s-%04lX%04X", DEVICE_ID,
             (unsigned long)((mac >> 32) & 0xFFFFUL), (unsigned)(esp_random() & 0xFFFF));

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);    // переподключением управляем сами
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setBufferSize(MQTT_BUFFER);
    mqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
    mqtt.setKeepAlive(15);
}

const char *mqttClientId() { return clientId; }

// ---------------------------------------------------------------------
//  Wi-Fi
// ---------------------------------------------------------------------
bool wifiEnsure()
{
    if (testMode == 'w')
        return false;                // идёт испытание: не переподключаться

    if (WiFi.status() == WL_CONNECTED)
    {
        if (!wifiWasUp)
        {
            wifiWasUp = true;
            Serial.printf("# Wi-Fi: connected, IP %s\r\n", WiFi.localIP().toString().c_str());
            if (!ntpStarted)
            {
                configTime(0, 0, NTP_SERVER);   // синхронизация времени в фоне
                ntpStarted = true;
            }
        }
        return true;
    }

    if (wifiWasUp)
    {
        wifiWasUp = false;
        tcp.stop();                  // старый TCP-сокет MQTT больше не годен
        Serial.println("# Wi-Fi: connection lost");
    }
    uint32_t now = millis();
    if (!wifiTried || now - wifiLastTryMs >= WIFI_RETRY_MS)
    {
        wifiTried = true;
        wifiLastTryMs = now;
        Serial.printf("# Wi-Fi: connecting to \"%s\"... (status=%d)\r\n", WIFI_SSID, (int)WiFi.status());
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);  // не блокирует
    }
    return false;
}

// ---------------------------------------------------------------------
//  MQTT
// ---------------------------------------------------------------------
bool mqttEnsure()
{
    if (testMode == 'm')
        return false;
    if (mqtt.connected())
        return true;

    if (mqttWasUp)
    {
        // -4 тайм-аут keep-alive, -3 соединение разорвано, -2 сеть недоступна
        mqttWasUp = false;
        Serial.printf("# MQTT: connection lost, state=%d\r\n", mqtt.state());
    }

    uint32_t now = millis();
    if (mqttTried && now - mqttLastTryMs < mqttRetryMs)
        return false;                // ещё рано для новой попытки
    mqttTried = true;
    mqttLastTryMs = now;

    Serial.printf("# MQTT: connecting to %s:%d as %s...\r\n", MQTT_HOST, MQTT_PORT, clientId);
    // Last Will: если устройство пропадёт, брокер сам опубликует "offline"
#ifdef MQTT_USER
    bool ok = mqtt.connect(clientId, MQTT_USER, MQTT_PASS, TOPIC_STATUS, 0, true, "offline");
#else
    bool ok = mqtt.connect(clientId, TOPIC_STATUS, 0, true, "offline");
#endif
    if (ok)
    {
        mqttRetryMs = MQTT_RETRY_MIN_MS;
        mqttWasUp = true;
        mqtt.publish(TOPIC_STATUS, "online", true);
        Serial.println("# MQTT: connected");
        return true;
    }
    Serial.printf("# MQTT: failed, state=%d, next attempt in %lu ms\r\n",
                  mqtt.state(), (unsigned long)mqttRetryMs);
    mqttRetryMs = (mqttRetryMs * 2 > MQTT_RETRY_MAX_MS) ? MQTT_RETRY_MAX_MS : mqttRetryMs * 2;
    return false;
}

void mqttService()
{
    mqtt.loop();
}

bool mqttPublish(const char *topic, const char *payload, bool retain)
{
    if (!mqtt.connected())
        return false;
    return mqtt.publish(topic, payload, retain);
}

uint32_t epochNow()
{
    time_t t = time(nullptr);
    return (t > 1700000000) ? (uint32_t)t : 0;   // до синхронизации time() ≈ 0
}

// ---------------------------------------------------------------------
//  Испытание потери связи
// ---------------------------------------------------------------------
void linkTestStart(char mode, uint32_t durationMs)
{
    testMode = mode;
    testUntilMs = millis() + durationMs;
    if (mode == 'w')
    {
        // Wi-Fi пропадает «аварийно», без MQTT DISCONNECT: брокер сам
        // заметит пропажу по keep-alive и опубликует Last Will "offline".
        tcp.stop();                  // закрыть сокет локально, пакет DISCONNECT не уходит
        WiFi.disconnect();
        wifiWasUp = false;
        Serial.printf("# TEST: Wi-Fi disabled for %lu s\r\n", (unsigned long)(durationMs / 1000));
    }
    else
    {
        mqtt.disconnect();
        Serial.printf("# TEST: MQTT disabled for %lu s\r\n", (unsigned long)(durationMs / 1000));
    }
}

bool linkTestActive() { return testMode != 0; }

void IRAM_ATTR linkTestButtonIsr()
{
    buttonPressed = true;            // в ISR только флаг
}

void linkTestPoll()
{
    if (testMode && (int32_t)(millis() - testUntilMs) >= 0)
    {
        testMode = 0;
        mqttTried = false;           // сразу пробовать снова
        Serial.println("# TEST: finished, restoring link");
    }
    if (buttonPressed)
    {
        buttonPressed = false;
        if (!testMode)
            linkTestStart('w', LINK_TEST_MS);
    }
    while (Serial.available())
    {
        char c = Serial.read();
        if ((c == 'w' || c == 'm') && !testMode)
            linkTestStart(c, LINK_TEST_MS);
    }
}
