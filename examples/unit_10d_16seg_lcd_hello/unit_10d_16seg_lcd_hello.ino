/*
    Description: DM8BA10 (10桁16セグメントLCD) の基本的な表示。
    HELLO 1234 を表示し、小数点を順に点滅、その後カウンタを表示します。
*/

#include <M5Unified.h>
#include "Unit_10d_16seg_LCD.h"

Unit_10d_16seg_LCD lcd;

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    // 外部 I2C(Port A)のピンは M5Unified から取得する
    int8_t sda = M5.Ex_I2C.getSDA();
    int8_t scl = M5.Ex_I2C.getSCL();
    Serial.printf("Ex_I2C SDA=%d SCL=%d\r\n", sda, scl);
    if (sda < 0 || scl < 0) {
        Serial.println("this board has no external I2C pins");
        while (true) delay(1000);
    }

    // ブリッジは起動後に約3秒応答しないため、begin() が最大10秒待つ
    if (!lcd.begin(&Wire, DM8BA10_ADDR, sda, scl, 100000L)) {
        Serial.println("bridge not found. check SDA/SCL, pull-ups, power");
        while (true) delay(1000);
    }
    lcd.setBacklightActiveLow(true);  // ブリッジ PD1 の Low でバックライト点灯(回路に合わせる)
    lcd.setBacklight(true);

    lcd.print("HELLO 1234");
    delay(1500);

    for (uint8_t i = 0; i < DM8BA10_POINTS; i++) {  // 小数点を左から順に点灯
        lcd.setPoint(i, true);
        delay(150);
    }
    delay(500);
    lcd.clearPoints();
}

void loop() {
    static uint32_t n = 0;
    char buf[16];
    snprintf(buf, sizeof buf, "COUNT %04lu", (unsigned long)(n++ % 10000));
    lcd.print(buf);
    delay(200);
}
