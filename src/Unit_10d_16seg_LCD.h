#ifndef __UNIT_10D_16SEG_LCD_H
#define __UNIT_10D_16SEG_LCD_H

#include "Arduino.h"
#include "Wire.h"

// HT1622 I2C ブリッジ(CH32V003, firmware/ht1622_i2c_bridge)の I2C アドレス
#define DM8BA10_ADDR   0x3C
#define DM8BA10_DIGITS 10   // 10桁 16セグメント
#define DM8BA10_POINTS 9    // 小数点(桁と桁の間)
#define DM8BA10_RAM    64   // HT1622 RAM: 4bit x 64 アドレス
#define DM8BA10_GAP_MS 3    // 送信後の待ち(ブリッジが HT1622 へ書き終わるまで)

// ブリッジの I2C プロトコル
#define DM8BA10_MODE_CMD    0x00  // [0x00, CMD]
#define DM8BA10_MODE_NIBBLE 0x01  // [0x01, ADDR, N0, N1, ...]
#define DM8BA10_MODE_PACKED 0x02  // [0x02, ADDR, (N0<<4)|N1, ...]
#define DM8BA10_MODE_BL     0x03  // [0x03, 0/1]

// HT1622 コマンド
#define DM8BA10_CMD_SYS_EN 0x01
#define DM8BA10_CMD_LCD_ON 0x03

class Unit_10d_16seg_LCD {
   private:
    uint8_t _addr         = DM8BA10_ADDR;
    TwoWire* _wire        = nullptr;
    int8_t _sda           = 21;
    int8_t _scl           = 22;
    uint32_t _speed       = 100000L;
    uint8_t _ram[DM8BA10_RAM] = {0};  // bit i = D_i (D0 が先に送られる)
    bool _autoUpdate      = true;
    bool _blActiveLow     = true;
    uint8_t _lastError    = 0;

    bool send(const uint8_t* data, size_t len);
    bool waitBridge(uint32_t timeoutMs);
    void apply();

   public:
    // ブリッジは起動後に約3秒 I2C に応答しないため、waitMs まで応答を待つ。
    // sda/scl が負のときは、Wire の既定ピンを使う。
    // 応答があれば、HT1622 を初期化(SYS EN, LCD ON)して RAM を消去し true を返す。
    bool begin(TwoWire* wire = &Wire, uint8_t addr = DM8BA10_ADDR,
               int8_t sda = 21, int8_t scl = 22, uint32_t speed = 100000L,
               uint32_t waitMs = 10000);
    bool isConnected();
    uint8_t getLastError();  // 直近の Wire.endTransmission() の戻り値(0 = 成功)

    // ---- 更新 ----
    // true(既定): 描画関数のたびに RAM 全体を送信。false: update() を呼んだときだけ送信。
    void setAutoUpdate(bool on);
    bool update();

    // ---- 文字表示 ----
    void clear();                            // 文字も小数点も全消灯
    void fill();                             // 全点灯
    void clearPoints();                      // 小数点だけ消灯
    void print(const char* text);            // 10桁に左から表示(ASCII 0x20-0x7F)。小数点は保持
    void print(const String& text);
    void setChar(uint8_t pos, char c);       // pos: 0(左端)〜9(右端)
    void setDigit(uint8_t pos, uint16_t pattern);  // 16bit パターン(MSB が先頭アドレスの D0)

    // ---- 小数点 ----
    void setPoint(uint8_t idx, bool on);     // idx: 0(左端)〜8(右端)
    void togglePoint(uint8_t idx);

    // ---- バックライト ----
    void setBacklightActiveLow(bool activeLow);  // 既定 true(ブリッジ PD1 が Low で点灯)
    void setBacklight(bool on);

    // ---- 低レベル ----
    void sendCommand(uint8_t cmd);
    void setRamNibble(uint8_t addr, uint8_t value);  // addr 0..63, value 4bit
    uint8_t getRamNibble(uint8_t addr);
};

#endif
