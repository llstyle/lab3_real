// =====================================================================
//  OLED SSD1306 128x64 на шине I2C (выводы 4/15, сброс 16).
//  Шрифт библиотеки не содержит кириллицы, поэтому надписи латиницей.
// =====================================================================
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "display.h"
#include "config.h"

static Adafruit_SSD1306 oled(128, 64, &Wire, OLED_RST);
static bool oledOk = false;

bool displayBegin()
{
    pinMode(VEXT_PIN, OUTPUT);
    digitalWrite(VEXT_PIN, LOW);         // питание дисплея (Heltec V2)
    delay(50);
    Wire.begin(OLED_SDA, OLED_SCL);
    oledOk = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    if (oledOk)
    {
        oled.setTextColor(SSD1306_WHITE);
        oled.clearDisplay();
        oled.setCursor(0, 0);
        oled.print("L3 tamper: starting");
        oled.display();
    }
    return oledOk;
}

void displayShow(const SharedState &s, bool linkUp, bool linkTest, uint32_t evtWaiting)
{
    if (!oledOk)
        return;
    oled.clearDisplay();

    // строка 1: статус связи
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print(linkTest ? "LINK TEST" : (linkUp ? "MQTT: online" : "MQTT: offline"));
    oled.setCursor(92, 0);
    oled.printf("q:%lu", (unsigned long)evtWaiting);

    // строка 2: расстояние крупно
    oled.setTextSize(2);
    oled.setCursor(0, 14);
    if (s.noEcho > 0 && !s.ok)
        oled.print("no echo");
    else
        oled.printf("%.1f cm", s.fCm);

    // строка 3: опорное расстояние и отклонение
    oled.setTextSize(1);
    oled.setCursor(0, 38);
    if (s.calibrated)
        oled.printf("ref %.1f  dev %.1f", s.refCm, s.dev);
    else
        oled.print("calibrating...");

    // строка 4: тревога
    oled.setCursor(0, 54);
    oled.print(s.tamper ? ">>> TAMPER <<<" : "OK");

    oled.display();
}
