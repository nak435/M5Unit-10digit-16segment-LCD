# M5Stack Unit 10digit-16segment-LCD

DM8BA10(HT1622 駆動の 10 桁 16 セグメント LCD モジュール)を、CH32V003 製の
I2C ブリッジ経由で制御するライブラリです。Arduino(ESP32)用ライブラリ、ブリッジのファームウェアを含みます。

## 構成

```
M5Unit-10digit-16segment-LCD/
├── src/                      Arduino ライブラリ (Unit_10d_16seg_LCD)
├── examples/
│   ├── unit_10d_16seg_lcd_hello/       基本表示のサンプル
│   ├── unit_10d_16seg_lcd_charset/     全文字(ASCII 0x20-0x7F)を10文字ずつ表示して確認するサンプル
│   ├── unit_10d_16seg_lcd_scroll/      全文字を1文字ずつ左へシフトして流すサンプル
│   ├── unit_10d_16seg_lcd_clock/       ESP32 内蔵 RTC の時刻を表示するサンプル
│   └── unit_10d_16seg_lcd_testing/     シリアルコマンドのテスター
├── firmware/ht1622_i2c_bridge/   ブリッジ(CH32V003J4M6, ch32fun)のソース
├── docs/DM8BA10_I2C_Protocol.md  接続・I2C プロトコル・RAM マップ・注意点
├── library.properties / library.json
└── LICENSE
```

## 文字の確認

`examples/unit_10d_16seg_lcd_charset` は、全文字を 10 ページで順に表示します(2.5 秒ごとの自動送り)。
`examples/unit_10d_16seg_lcd_scroll` は、全文字を 1 文字ずつ左へシフトしながら流します(ボタン A で一時停止)。
`examples/unit_10d_16seg_lcd_clock` は、ESP32 内蔵 RTC の時刻を `hh:mm:ss.t` で 100ms ごとに更新(`t` は数字でなく、1/10 秒ごとのビットパターン)し、ボタン A を押している間だけ `yyyy-mm-dd` を表示します。時刻はシリアルの `t yyyy-mm-dd hh:mm:ss` で合わせます(未設定ならビルド時刻)。
ボタン A、またはシリアル(115200)の `n` / `p` / `0`〜`9` でページを移動できます。各ページの内容は、
シリアルにも出力します。起動直後に、全点灯を 1.5 秒表示して、セグメントの欠けも確認できます。

## 接続

ESP32 ── I2C(アドレス 0x3C)── CH32V003 ブリッジ ── HT1622 ── DM8BA10

詳しい配線、I2C パケット、RAM マップ、バックライト回路は [docs/DM8BA10_I2C_Protocol.md](docs/DM8BA10_I2C_Protocol.md) を参照してください。

- I2C のプルアップは **3.3V** に接続します(ESP32 の GPIO は 5V 耐性がありません)。
- ブリッジは起動後 約 3 秒、I2C に応答しません。`begin()` が最大 10 秒待ちます。

## Arduino

`src/` と同じ階層ごと、Arduino の `libraries/` に置きます(または PlatformIO の `lib_deps`)。

```cpp
#include <M5Unified.h>
#include "Unit_10d_16seg_LCD.h"

Unit_10d_16seg_LCD lcd;

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);
    // 外部 I2C(Port A)のピンは M5Unified から取得する
    if (!lcd.begin(&Wire, DM8BA10_ADDR, M5.Ex_I2C.getSDA(), M5.Ex_I2C.getSCL(), 100000L)) {
        Serial.println("bridge not found");
    }
    lcd.setBacklightActiveLow(true);   // 回路に合わせる
    lcd.setBacklight(true);
    lcd.print("HELLO 1234");
    lcd.setPoint(3, true);
}

void loop() {}
```

サンプルは M5Unified を使い、`M5.Ex_I2C.getSDA()` / `getSCL()` でピンを取得します
(M5Stamp C3U は SDA=1、SCL=0、M5Stamp Pico は SDA=32、SCL=33)。
M5 以外のボードでは、ピン番号を直接渡してください。負の値を渡すと、`Wire` の既定ピンを使います。

### API

| 関数 | 内容 |
|---|---|
| `begin(wire, addr, sda, scl, speed, waitMs)` | ブリッジの応答を待ち、HT1622 を初期化して RAM を消去。応答がなければ `false`。sda/scl が負なら既定ピン |
| `isConnected()` / `getLastError()` | ブリッジの応答確認 / 直近の I2C エラー(0 で成功) |
| `print(text)` | 10 桁に左から表示(ASCII 0x20–0x7F)。小数点は保持 |
| `setChar(pos, c)` / `setDigit(pos, pattern)` | 1 桁だけ書込(pos: 0 左端 〜 9 右端)。`pattern` は 16bit(MSB が先頭アドレスの D0) |
| `setPoint(idx, on)` / `togglePoint(idx)` / `clearPoints()` | 小数点(idx: 0 左端 〜 8 右端) |
| `clear()` / `fill()` | 全消灯(小数点を含む) / 全点灯 |
| `setBacklight(on)` / `setBacklightActiveLow(b)` | バックライト。既定は PD1 が Low で点灯 |
| `setAutoUpdate(b)` / `update()` | 既定は描画のたびに送信。`false` にすると `update()` で 1 回だけ送信 |
| `sendCommand(cmd)` / `setRamNibble(addr, v)` / `getRamNibble(addr)` | 低レベルアクセス |


## ブリッジのファームウェア

`firmware/ht1622_i2c_bridge/ht1622_i2c_bridge.c` は CH32V003J4M6 用です(ch32fun を使用)。
ビルドと書き込み(WCH-LinkE の SWIO)の手順は、お使いの ch32fun 環境に合わせてください。

## 動作確認

確認した環境: M5Stamp C3U + I2C ブリッジ + DM8BA10、ESP32 Arduino Core 4.0.0。

| 内容 | 状態 |
|---|---|
| Arduino ライブラリ: `hello`、`charset`、`scroll`、`clock`、`testing` の各サンプル | 実機で動作を確認 |
| 字形の表(ASCII 0x20〜0x7F) | 実機の表示を見ながら調整 |

ビルド中に、M5GFX(M5Unified が使う描画ライブラリ)のコンパイルで、`NUM_DIGITAL_PINS` などの
マクロ再定義の警告が出ることがあります。本ライブラリのコードに関する警告ではありません。

## ライセンス

[MIT](LICENSE)。
文字の字形は[road-t/DM8BA10](https://github.com/road-t/DM8BA10)(MIT)と
フォント [Calculate 16](https://www.dafont.com/calculate-16.font)(Benjamin Blåholtz、dafont.com では「100% Free」)
を元に作者が調整した字形としています。
