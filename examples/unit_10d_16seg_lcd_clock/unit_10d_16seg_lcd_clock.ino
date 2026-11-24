/*
    Description: ESP32 の内蔵 RTC(システム時計)の時刻を、DM8BA10 に表示する。
      通常:  hh:mm:ss.t  (t は 1/10 秒。100ms ごとに更新)
             t の桁は数字ではなく、TENTH_PATTERN の 16bit パターン(0〜9 の 10 通り)で表示します。
      ボタン A(M5Stamp の本体ボタン)が押されている間だけ、yyyy-mm-dd(日付)を表示します。
      どちらも、ちょうど 10 文字です。

      時刻の合わせ方:
        ・電源投入時に、時刻が未設定(2020 年より前)なら、ビルド時刻(__DATE__ / __TIME__)を設定します。
        ・シリアル(115200)で  t yyyy-mm-dd hh:mm:ss  を送ると、その時刻に合わせます。
          例) t 2026-11-23 22:00:00
      ESP32 内蔵の時計は、電源を切ると進みません(M5Stamp には RTC チップがありません)。
      電源を切っても保持したい場合は、外付けの RTC(PCF8563 など)か NTP を使ってください。
      時刻はタイムゾーン設定なし(入力した値をそのまま表示)です。
*/

#include <M5Unified.h>
#include <sys/time.h>
#include <time.h>
#include "Unit_10d_16seg_LCD.h"

Unit_10d_16seg_LCD lcd;

#define UPDATE_MS    100   // 表示の更新間隔(ms)。1/10 秒の桁が変わるたびに更新
// 1/10 秒の桁(右端)に出すパターン。添字が 0〜9(1/10 秒の値)。MSB が先頭アドレスの D0
static const uint16_t TENTH_PATTERN[10] = {
    0x0000, 0x0040, 0x0050, 0x0058, 0x005C,
    0x045C, 0x0C5C, 0x1C5C, 0x5C5C, 0xFFFF,
};


static void setTime(int y, int mo, int d, int h, int mi, int s) {
    struct tm t = {};
    t.tm_year = y - 1900;
    t.tm_mon  = mo - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min  = mi;
    t.tm_sec  = s;
    struct timeval tv = {mktime(&t), 0};
    settimeofday(&tv, nullptr);
}

// ビルド時刻(__DATE__ = "Nov 23 2026"、__TIME__ = "22:00:00")を設定する
static void setTimeFromBuild() {
    static const char* MON = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char m[4] = {0};
    int d = 0, y = 0, hh = 0, mm = 0, ss = 0;
    sscanf(__DATE__, "%3s %d %d", m, &d, &y);
    sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss);
    int mo = (strstr(MON, m) - MON) / 3 + 1;
    setTime(y, mo, d, hh, mm, ss);
}

static void handleLine(const char* line) {
    int y, mo, d, h, mi, s;
    if (sscanf(line, "t %d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6) {
        setTime(y, mo, d, h, mi, s);
        Serial.println("time set");
    } else {
        Serial.println("usage: t yyyy-mm-dd hh:mm:ss");
    }
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
    lcd.clear();
    lcd.setAutoUpdate(false);  // 文字と 1/10 秒の桁を、まとめて 1 回で送る(update() で送信)

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm t;
    localtime_r(&tv.tv_sec, &t);
    if (t.tm_year + 1900 < 2020) {  // 未設定なら、ビルド時刻にする
        setTimeFromBuild();
        Serial.println("time set from build time");
    }
}

void loop() {
    M5.update();  // ボタンの状態を更新(毎回呼ぶ)

    static char    line[48];
    static uint8_t n         = 0;
    static int32_t lastKey   = -1;
    static uint32_t lastMs   = 0;

    // シリアルから 1 行受け取る
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (n > 0) { line[n] = '\0'; handleLine(line); n = 0; }
        } else if (n < sizeof(line) - 1) {
            line[n++] = c;
        }
    }

    if (millis() - lastMs < 10) return;  // 10ms ごとに、1/10 秒の桁が変わったかを見る
    lastMs = millis();

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm t;
    localtime_r(&tv.tv_sec, &t);
    int tenth = tv.tv_usec / 100000;  // 0〜9

    // ボタン A が押されている間は日付。日付は、時刻と別のキーにして、押している間は 1 回だけ送る
    bool showDate = M5.BtnA.isPressed();
    int32_t key   = showDate ? -1000 - t.tm_mday : (int32_t)(t.tm_sec * 10 + tenth);
    if (key == lastKey) return;
    lastKey = key;

    char text[DM8BA10_DIGITS + 1];
    if (showDate) {
        snprintf(text, sizeof(text), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        lcd.print(text);
    } else {
        snprintf(text, sizeof(text), "%02d:%02d:%02d.", t.tm_hour, t.tm_min, t.tm_sec);  // 9 文字
        lcd.print(text);
        lcd.setDigit(DM8BA10_DIGITS - 1, TENTH_PATTERN[tenth]);  // 右端: 1/10 秒
    }
    lcd.update();
}
