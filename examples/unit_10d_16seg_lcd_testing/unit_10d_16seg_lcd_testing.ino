/*
    Description: DM8BA10 テスト用シリアルコマンド(115200, 改行で確定)
      文字列        : 10桁に左から表示(ASCII 0x20-0x7F)
      P n           : 小数点 n(0-8) をトグル
      a / c         : 全点灯 / 全消灯
      n / p         : ウォーカー(1ビットずつ点灯 次/前)  位置=addr*4+bit (0..255)
      g <位置>      : ウォーカーをその位置へ
      w <hex4> <桁> : 16bitパターンを桁(0-9)に直接書込 例: w 8001 0
      l 1 / l 0     : バックライト ON / OFF
      i             : 再初期化(SYS EN, LCD ON)
*/

#include <M5Unified.h>
#include "Unit_10d_16seg_LCD.h"

Unit_10d_16seg_LCD lcd;
int cur = 0;  // ウォーカー位置

void walk() {
    lcd.clear();
    int addr = cur / 4, bit = cur % 4;
    lcd.setRamNibble(addr, 1 << bit);
    lcd.update();
    Serial.printf("pos %d: addr 0x%02X bit D%d\r\n", cur, addr, bit);
}

String line;
void handle(String l) {
    l.trim();
    if (!l.length()) return;
    char c = l[0];
    if (l == "n") {
        cur = (cur + 1) % 256;
        walk();
    } else if (l == "p") {
        cur = (cur + 255) % 256;
        walk();
    } else if (l == "a") {
        lcd.fill();
        lcd.update();
    } else if (l == "c") {
        lcd.clear();
        lcd.update();
    } else if (l == "i") {
        lcd.sendCommand(DM8BA10_CMD_SYS_EN);
        lcd.sendCommand(DM8BA10_CMD_LCD_ON);
        Serial.print("init\r\n");
    } else if (l == "l 1") {
        lcd.setBacklight(true);
    } else if (l == "l 0") {
        lcd.setBacklight(false);
    } else if (c == 'g' && l.length() > 2) {
        cur = constrain(l.substring(2).toInt(), 0, 255);
        walk();
    } else if (c == 'P' && l.length() > 2) {
        int n = l.substring(2).toInt();
        if (n >= 0 && n <= 8) {
            lcd.togglePoint(n);
            lcd.update();
        }
    } else if (c == 'w' && l.length() > 2) {
        int sp = l.indexOf(' ', 2);
        if (sp > 0) {
            uint16_t pat = strtoul(l.substring(2, sp).c_str(), nullptr, 16);
            int pos      = constrain(l.substring(sp + 1).toInt(), 0, 9);
            lcd.setDigit(pos, pat);
            lcd.update();
        }
    } else {
        lcd.print(l.c_str());
        lcd.update();
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    // 外部 I2C(Port A)のピンは M5Unified から取得する
    int8_t sda = M5.Ex_I2C.getSDA();
    int8_t scl = M5.Ex_I2C.getSCL();
    Serial.printf("Ex_I2C SDA=%d SCL=%d\r\n", sda, scl);

    Serial.print("waiting for I2C bridge (0x3C)...\r\n");
    if (!lcd.begin(&Wire, DM8BA10_ADDR, sda, scl, 100000L)) {
        Serial.print("bridge not found. check SDA/SCL, pull-ups, power\r\n");
    }
    lcd.setAutoUpdate(false);  // 描画後に update() で1回だけ送信する
    lcd.setBacklightActiveLow(true);
    lcd.setBacklight(true);
    lcd.clear();
    lcd.update();
    Serial.print(
        "DM8BA10 I2C test. text:show P n:point n/p/g:walk a:all c:clear w hex pos "
        "i:init l 1/l 0:backlight\r\n");
    lcd.print("HELLO 1234");
    lcd.update();
}

void loop() {
    while (Serial.available()) {
        char ch = Serial.read();
        if (ch == '\n' || ch == '\r') {
            handle(line);
            line = "";
        } else
            line += ch;
    }
}
