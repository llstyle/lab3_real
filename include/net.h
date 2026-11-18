// =====================================================================
//  Wi-Fi и MQTT: подключение, переподключение без блокировки, публикация
// =====================================================================
#pragma once
#include <Arduino.h>

void netBegin();                    // настроить Wi-Fi (STA) и MQTT-клиент
bool wifiEnsure();                  // true — Wi-Fi подключён; иначе запускает попытку и сразу возвращается
bool mqttEnsure();                  // true — MQTT подключён; попытки с растущей паузой
void mqttService();                 // обслуживание клиента (keep-alive), вызывать каждый цикл
bool mqttPublish(const char *topic, const char *payload, bool retain = false);
const char *mqttClientId();
uint32_t epochNow();                // время UTC по NTP; 0 — ещё не синхронизировано

// ---- Испытание потери связи ----
// mode: 'w' — разорвать Wi-Fi, 'm' — разорвать только MQTT
void linkTestStart(char mode, uint32_t durationMs);
bool linkTestActive();
void IRAM_ATTR linkTestButtonIsr(); // кнопка LINK_TEST_PIN
void linkTestPoll();                // проверить кнопку и команды из Serial
