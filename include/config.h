// =====================================================================
//  L3. Передача информации во внешнюю среду (реальная плата)
//  Heltec WiFi Kit 32 + ультразвуковой дальномер HC-SR04 + MQTT
//  Конфигурация проекта: все настраиваемые параметры в одном месте
// =====================================================================
#pragma once
#include "secrets.h" // WIFI_SSID, WIFI_PASS (файл не публикуется в git)

// ---------------------------------------------------------------------
//  Аппаратное подключение (нижний ряд выводов Heltec)
// ---------------------------------------------------------------------
#define TRIG_PIN 13     // HC-SR04 Trig
#define ECHO_PIN 14     // HC-SR04 Echo (через делитель, если датчик от 5 В)
#define LED_PIN 27      // светодиод «вмешательство» (через резистор 1 кОм)
#define LINK_TEST_PIN 0 // кнопка PRG на плате: испытание потери связи

// Встроенный OLED SSD1306 128x64 (I2C)
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16
#define VEXT_PIN 21 // Heltec V2: LOW включает питание дисплея
#define OLED_ADDR 0x3C

// ---------------------------------------------------------------------
//  Датчик и обработка сигнала
// ---------------------------------------------------------------------
#define SAMPLE_PERIOD_US 60000 // период выборки 60 мс (рекомендация HC-SR04: не менее 60 мс)
#define ECHO_TIMEOUT_MS 30     // ждать эхо не дольше 30 мс (~5 м)
#define US_PER_CM 58.0f        // звук туда и обратно: 58 мкс на 1 см
#define VALID_MIN_CM 2.0f      // рабочий диапазон HC-SR04
#define VALID_MAX_CM 400.0f
#define MEDIAN_WINDOW 5
#define AVG_WINDOW 4 // 4 × 60 мс = 240 мс

#if (MEDIAN_WINDOW % 2) == 0
#error "MEDIAN_WINDOW должно быть нечётным"
#endif

// ---------------------------------------------------------------------
//  Логика use case (задача приложения)
// ---------------------------------------------------------------------
#define WARMUP_SAMPLES 8        // пропустить, пока заполняются фильтры
#define CALIB_SAMPLES 20        // достоверных отсчётов на калибровку (~1,2 с)
#define SHIFT_THRESHOLD_CM 5.0f // медленный канал: |d_f − d_ref| ≥ 5 см
#define JUMP_THRESHOLD_CM 15.0f // канал рывка: |d − d_f| ≥ 15 см
#define JUMP_HOLD_MS 1000       // удержание тревоги после рывка
#define NO_ECHO_ALARM 10        // столько пропусков эха подряд (0,6 с) = тревога

// ---------------------------------------------------------------------
//  FreeRTOS: задачи, очереди, тайм-ауты
// ---------------------------------------------------------------------
//                        стек, байт   приоритет   ядро
#define ACQ_STACK 4096
#define ACQ_PRIO 3
#define ACQ_CORE 1
#define APP_STACK 4096
#define APP_PRIO 2
#define APP_CORE 1
#define COMM_STACK 8192
#define COMM_PRIO 1
#define COMM_CORE 0 // ядро 0: там же работает стек Wi-Fi

#define COMM_PERIOD_MS 1000 // период публикации телеметрии и обновления дисплея

#define MEAS_QUEUE_LEN 10 // сбор → приложение: 10 × 60 мс = 600 мс запаса
#define EVT_QUEUE_LEN 32  // приложение → коммуникация

#define SEM_TIMEOUT_MS 300     // ожидание тика таймера (5 периодов)
#define MEAS_RX_TIMEOUT_MS 300 // приложение ждёт измерение
#define MUTEX_TIMEOUT_MS 5
#define EVT_TX_TIMEOUT_MS 10
#define MAX_EVT_PER_CYCLE 16

// ---------------------------------------------------------------------
//  Wi-Fi (имя и пароль — в secrets.h)
// ---------------------------------------------------------------------
#define WIFI_CHANNEL 0 // 0 = любой канал
#define WIFI_RETRY_MS 5000

// ---------------------------------------------------------------------
//  MQTT
// ---------------------------------------------------------------------
// Брокер можно переопределить в secrets.h (например, свой Mosquitto)
#ifndef MQTT_HOST
#define MQTT_HOST "broker.hivemq.com"
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#define MQTT_BUFFER 512
#define MQTT_SOCKET_TIMEOUT_S 2
#define MQTT_RETRY_MIN_MS 2000
#define MQTT_RETRY_MAX_MS 30000

#define DEVICE_ID "si254-vereskul-tamper"
#define TOPIC_PREFIX "utm/si254/vereskul/tamper"
#define TOPIC_TELEMETRY TOPIC_PREFIX "/telemetry"
#define TOPIC_EVENT TOPIC_PREFIX "/event"
#define TOPIC_STATUS TOPIC_PREFIX "/status"

#define NTP_SERVER "pool.ntp.org"

// ---------------------------------------------------------------------
//  Испытания и вывод
// ---------------------------------------------------------------------
#define LINK_TEST_MS 30000 // длительность имитации потери связи
#define PRINT_CSV 0        // 1 — строка на каждое измерение
#define DIAG_EVERY_S 10
