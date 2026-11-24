/*
    Description: DM8BA10 の全文字(ASCII 0x20-0x7F)を、10文字ずつのページで表示して確認する。
      既定では、PAGE_MS ごとに自動で次のページへ進みます。
      ボタン A(M5Stamp の本体ボタン)を押すと、次のページへ進みます。
      シリアル(115200): n 次のページ / p 前のページ / s 自動送りの切り替え / a 全点灯 / 0〜9 そのページへ
      各ページの内容と文字コードは、シリアルにも表示します。
      最後のページの DEL(0x7F)は、全点灯です。
*/

#include <M5Unified.h>
#include "Unit_10d_16seg_LCD.h"

#define FIRST_CODE 0x20
#define LAST_CODE  0x7F
#define PAGE_MS    2500

Unit_10d_16seg_LCD lcd;
const int PAGES = (LAST_CODE - FIRST_CODE + 1 + DM8BA10_DIGITS - 1) / DM8BA10_DIGITS;  // 10
int page        = 0;
bool autoAdvance = true;
uint32_t lastMs = 0;

void showPage(int p) {
    page = (p % PAGES + PAGES) % PAGES;
    char text[DM8BA10_DIGITS + 1];
    char log[DM8BA10_DIGITS + 1];
    int first = FIRST_CODE + page * DM8BA10_DIGITS;
    int n     = 0;
    for (int i = 0; i < DM8BA10_DIGITS; i++) {
        int code = first + i;
        if (code <= LAST_CODE) {
            text[i] = (char)code;
            log[i]  = (code == 0x7F) ? '#' : (char)code;  // DEL は表示できないので # で代用
            n++;
        } else {
            text[i] = ' ';
            log[i]  = ' ';
        }
    }
    text[DM8BA10_DIGITS] = log[DM8BA10_DIGITS] = '\0';
    lcd.print(text);
    Serial.printf("page %d/%d  0x%02X-0x%02X  [%s]\r\n", page + 1, PAGES, first, first + n - 1, log);
    lastMs = millis();
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    // 外部 I2C(Port A)のピンは M5Unified から取得する
    int8_t sda = M5.Ex_I2C.getSDA();
    int8_t scl = M5.Ex_I2C.getSCL();
    Serial.printf("Ex_I2C SDA=%d SCL=%d\r\n", sda, scl);
    if (!lcd.begin(&Wire, DM8BA10_ADDR, sda, scl, 100000L)) {
        Serial.println("bridge not found. check SDA/SCL, pull-ups, power");
        while (true) delay(1000);
    }
    lcd.setBacklightActiveLow(true);
    lcd.setBacklight(true);

    // 全点灯で、セグメントの欠けがないかを先に確認する
    lcd.fill();
    delay(1500);
    lcd.clear();
    showPage(0);
}

void loop() {
    M5.update();
    if (M5.BtnA.wasClicked()) showPage(page + 1);

    while (Serial.available()) {
        char c = Serial.read();
        if (c == 'n') showPage(page + 1);
        else if (c == 'p') showPage(page - 1);
        else if (c == 's') {
            autoAdvance = !autoAdvance;
            Serial.printf("auto %s\r\n", autoAdvance ? "on" : "off");
        } else if (c == 'a') {
            lcd.fill();
        } else if (c >= '0' && c <= '9') {
            showPage(c - '0');
        }
    }

    if (autoAdvance && millis() - lastMs >= PAGE_MS) showPage(page + 1);
}
