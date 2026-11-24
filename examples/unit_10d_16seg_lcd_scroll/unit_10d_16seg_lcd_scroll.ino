/*
    Description: DM8BA10 に、全文字(ASCII 0x20-0x7F)を、1 文字ずつ左へシフトしながら流して表示する。
      右端から新しい文字が入り、左端から出ていきます。最後まで流れたら、先頭に戻ります。
      ボタン A(M5Stamp の本体ボタン)で、一時停止/再開。
      シリアル(115200): s 一時停止/再開 / + 速く / - 遅く / r 先頭に戻る / a 全点灯
      最後の DEL(0x7F)は、全点灯です。
*/

#include <M5Unified.h>
#include "Unit_10d_16seg_LCD.h"

#define FIRST_CODE 0x20
#define LAST_CODE  0x7F
#define STEP_MS    400   // 1 文字ずつ動かす間隔(ms)

Unit_10d_16seg_LCD lcd;
const int N_CODES = LAST_CODE - FIRST_CODE + 1;  // 96
// 流れる列: 空白 10 桁 + 全文字 + 空白 10 桁(画面が空から始まり、空で終わる)
const int TOTAL   = DM8BA10_DIGITS + N_CODES + DM8BA10_DIGITS;
int  pos          = 0;       // 表示窓の先頭位置(0〜TOTAL-DM8BA10_DIGITS)
bool running      = true;
uint32_t stepMs   = STEP_MS;
uint32_t lastMs   = 0;

char charAt(int i) {  // 流れる列の i 番目の文字
    int k = i - DM8BA10_DIGITS;
    if (k < 0 || k >= N_CODES) return ' ';
    return (char)(FIRST_CODE + k);
}

void show() {
    char text[DM8BA10_DIGITS + 1];
    char log[DM8BA10_DIGITS + 1];
    for (int i = 0; i < DM8BA10_DIGITS; i++) {
        text[i] = charAt(pos + i);
        log[i]  = (text[i] == 0x7F) ? '#' : text[i];  // DEL は # で代用して表示
    }
    text[DM8BA10_DIGITS] = log[DM8BA10_DIGITS] = '\0';
    lcd.print(text);
    Serial.printf("%3d [%s]\r\n", pos, log);
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    int8_t sda = M5.Ex_I2C.getSDA();
    int8_t scl = M5.Ex_I2C.getSCL();
    Serial.printf("Ex_I2C SDA=%d SCL=%d\r\n", sda, scl);
    if (!lcd.begin(&Wire, DM8BA10_ADDR, sda, scl, 100000L)) {
        Serial.println("bridge not found. check SDA/SCL, pull-ups, power");
        while (true) delay(1000);
    }
    lcd.setBacklightActiveLow(true);
    lcd.setBacklight(true);

    lcd.fill();
    delay(1000);
    lcd.clear();
    show();
    lastMs = millis();
}

void loop() {
    M5.update();
    if (M5.BtnA.wasClicked()) running = !running;

    while (Serial.available()) {
        char c = Serial.read();
        if (c == 's') {
            running = !running;
            Serial.printf("%s\r\n", running ? "run" : "pause");
        } else if (c == '+') {
            if (stepMs > 100) stepMs -= 100;
            Serial.printf("step %lu ms\r\n", (unsigned long)stepMs);
        } else if (c == '-') {
            stepMs += 100;
            Serial.printf("step %lu ms\r\n", (unsigned long)stepMs);
        } else if (c == 'r') {
            pos = 0;
            show();
        } else if (c == 'a') {
            lcd.fill();
        }
    }

    if (running && millis() - lastMs >= stepMs) {
        lastMs = millis();
        pos = (pos + 1) % (TOTAL - DM8BA10_DIGITS + 1);
        show();
    }
}
