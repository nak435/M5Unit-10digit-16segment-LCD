// HT1622_I2C_bridge.ino for ch32fun
/*

LCDドライバ HT1621(HT1622) I2Cブリッジ - CH32V003J4M6をI2Cスレーブとして動作
【I2C通信プロトコル】
  0x00 + [CMD]                     : 制御コマンド送信
  0x01 + [ADDR] + [D0] + [D1]...   : 1バイト＝1ニブル（下位4bit）連続書き込み
  0x02 + [ADDR] + [P0] + [P1]...   : 1バイト＝2ニブル連続書き込み（上位4bit→ADDR, 下位4bit→ADDR+1）

• ホスト側からデータを送りやすくするため、I2Cの書き込みデータ（先頭1バイト目）をモード切替ヘッダとして定義する
	1. 0x00 + [コマンド1バイト] : HT1621へ制御コマンド（LCD ON、SYS EN、バイアス設定など）を送信
	2. 0x01 + [開始アドレス(0〜63)] + [データ1バイト目] + [データ2バイト目]... : 指定したRAMアドレスから連続書き込み（バースト転送）
  3. 0x02 + [開始アドレス(0〜63)] + [データ0, データ1, ...] ※注記

• 初期化シーケンスの送信（I2Cアドレス 0x3C 宛）
	1. [0x00, 0x01] を送信 → SYS EN（システム発振器ON）
	2. [0x00, 0x18] を送信 → RC 32K（内蔵RCオシレータ使用）
	3. [0x00, 0x03] を送信 → LCD ON（LCD表示ON）

• 表示データの書き込み
	1. [0x01, 0x00, 0x0F, 0x0A, ...] を送信 → アドレス 0x00 から順にセグメントデータを一括反映

• 1回の送信を 32バイト以下（ヘッダ2バイト＋データ30バイトまで） にすること
  1回目: [0x01, 0x00, (アドレス0〜29の30バイト)] → 計32バイト
  2回目: [0x01, 0x1E, (アドレス30〜59の30バイト)] → 計32バイト（0x1E = 30）
  3回目: [0x01, 0x3C, (アドレス60〜63の4バイト)] → 計6バイト（0x3C = 60）

• モード 0x02 のパケット仕様
  [0x02, 開始アドレス(0〜63), データ0, データ1, ...]
  • データ1バイトの内訳:
    • 上位4bit (data[i] >> 4) → 先のアドレス（偶数側：startAddr + 0, +2, ...）へ書き込み
    • 下位4bit (data[i] & 0x0F) → 次のアドレス（奇数側：startAddr + 1, +3, ...）へ書き込み

*/

#include "ch32fun.h"
#include <stdint.h>
#include <stdbool.h>

// ピン定義 (CH32V003J4M6)
#define PIN_CS   PD6
#define PIN_WR   PA2
#define PIN_DATA PC4
#define PIN_BL   PD1 // backlight on/off control

#define I2C_SLAVE_ADDR 0x3C  // I2Cアドレス
#define RX_BUF_SIZE    70

volatile uint8_t rxBuf[RX_BUF_SIZE];
volatile uint8_t rxIdx = 0;
volatile uint8_t rxLen = 0;
volatile bool dataReady = false;
volatile bool rxOverflow = false;

// --- MSBファースト送信（ID・アドレス・コマンド用） ---
void HT1622_sendBitsMSB(uint16_t data, uint8_t bits) {
    for (int8_t i = bits - 1; i >= 0; i--) {
        funDigitalWrite(PIN_WR, FUN_LOW);
        funDigitalWrite(PIN_DATA, (data >> i) & 0x01 ? FUN_HIGH : FUN_LOW);
        Delay_Us(2);
        funDigitalWrite(PIN_WR, FUN_HIGH);
        Delay_Us(2);
    }
}

// --- LSBファースト送信（RAMデータ D0->D3 用） ---
void HT1622_sendBitsLSB(uint8_t data, uint8_t bits) {
    for (uint8_t i = 0; i < bits; i++) {
        funDigitalWrite(PIN_WR, FUN_LOW);
        funDigitalWrite(PIN_DATA, (data >> i) & 0x01 ? FUN_HIGH : FUN_LOW);
        Delay_Us(2);
        funDigitalWrite(PIN_WR, FUN_HIGH);
        Delay_Us(2);
    }
}

// --- コマンド送信 (モード 0x00) ---
void HT1622_writeCommand(uint8_t cmd) {
    funDigitalWrite(PIN_CS, FUN_LOW);
    Delay_Us(2);
    HT1622_sendBitsMSB(0b100, 3);
    HT1622_sendBitsMSB(cmd, 8);
    HT1622_sendBitsMSB(0, 1);
    funDigitalWrite(PIN_CS, FUN_HIGH);
    Delay_Us(2);
}

// --- データ連続書き込み：1バイト＝1ニブル (モード 0x01) ---
void HT1622_writeDataBurst(uint8_t startAddr, const uint8_t* data, uint8_t len) {
    funDigitalWrite(PIN_CS, FUN_LOW);
    Delay_Us(2);
    HT1622_sendBitsMSB(0b101, 3);
    HT1622_sendBitsMSB(startAddr & 0x3F, 6);

    for (uint8_t i = 0; i < len; i++) {
        HT1622_sendBitsLSB(data[i] & 0x0F, 4);
    }
    funDigitalWrite(PIN_CS, FUN_HIGH);
    Delay_Us(2);
}

// --- データ連続書き込み：1バイト＝2ニブル (モード 0x02) ---
void HT1622_writeDataPacked(uint8_t startAddr, const uint8_t* data, uint8_t len) {
    funDigitalWrite(PIN_CS, FUN_LOW);
    Delay_Us(2);
    HT1622_sendBitsMSB(0b101, 3);
    HT1622_sendBitsMSB(startAddr & 0x3F, 6);

    for (uint8_t i = 0; i < len; i++) {
        // 1. 上位4bitを送信（startAddr + 2*i 番地へ）
        HT1622_sendBitsLSB((data[i] >> 4) & 0x0F, 4);
        // 2. 下位4bitを送信（startAddr + 2*i + 1 番地へ）
        HT1622_sendBitsLSB(data[i] & 0x0F, 4);
    }
    funDigitalWrite(PIN_CS, FUN_HIGH);
    Delay_Us(2);
}

// --- I2C1 スレーブ初期化 (PC1: SDA, PC2: SCL) ---
void I2C1_Slave_Init(uint8_t addr) {
    // 必要なペリフェラルクロックを有効化 (GPIOC, AFIO, I2C1)
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;
    RCC->APB1PCENR |= RCC_APB1Periph_I2C1;

    // PC1 (SDA), PC2 (SCL) をオルタネート機能オープンドレイン出力 (50MHz) に設定
    GPIOC->CFGLR &= ~(0xF << (1 * 4));
    GPIOC->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_OD_AF) << (1 * 4);
    GPIOC->CFGLR &= ~(0xF << (2 * 4));
    GPIOC->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_OD_AF) << (2 * 4);

    // I2C1 ソフトウェアリセット
    RCC->APB1PRSTR |= RCC_APB1Periph_I2C1;
    RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;

    // ペリフェラル入力クロック設定 (48MHz)
    I2C1->CTLR2 = 48;

    // 7ビットスレーブアドレス設定 (ビット[7:1]に配置)
    I2C1->OADDR1 = (addr << 1);

    // 割り込み有効化 (イベント割り込み、バッファ割り込み、エラー割り込み)
    I2C1->CTLR2 |= I2C_CTLR2_ITEVTEN | I2C_CTLR2_ITBUFEN | I2C_CTLR2_ITERREN;

    // NVIC 割り込み有効化
    NVIC_EnableIRQ(I2C1_EV_IRQn);
    NVIC_EnableIRQ(I2C1_ER_IRQn);

    // I2C1 有効化 & ACK応答有効化
    I2C1->CTLR1 |= I2C_CTLR1_PE | I2C_CTLR1_ACK;
}

// --- I2C1 イベント割り込みハンドラ ---
void I2C1_EV_IRQHandler(void) __attribute__((interrupt));
void I2C1_EV_IRQHandler(void) {
    uint16_t star1 = I2C1->STAR1;

    // 1. アドレス一致 (ADDRフラグ)
    if (star1 & I2C_STAR1_ADDR) {
        // STAR1に続いてSTAR2を読むことでADDRフラグをクリア
        (void)I2C1->STAR2;
        rxIdx = 0;
        // メインループがまだ前のデータを処理中なら今回は破棄フラグを立てる
        rxOverflow = dataReady;
    }

    // 2. データ受信 (RXNEフラグ)
    if (star1 & I2C_STAR1_RXNE) {
        uint8_t d = I2C1->DATAR;
        if (!rxOverflow && rxIdx < RX_BUF_SIZE) {
            rxBuf[rxIdx++] = d;
        }
    }

    // 3. ストップコンディション検出 (STOPFフラグ)
    if (star1 & I2C_STAR1_STOPF) {
        // STAR1を読んだ後、CTLR1に書き込むことでSTOPFフラグをクリア
        I2C1->CTLR1 |= I2C_CTLR1_PE;
        if (!rxOverflow && rxIdx > 0) {
            rxLen = rxIdx;
            dataReady = true;
        }
    }
}

// --- I2C1 エラー割り込みハンドラ ---
void I2C1_ER_IRQHandler(void) __attribute__((interrupt));
void I2C1_ER_IRQHandler(void) {
    // バスエラーやNACK等のフラグをクリアして復帰させる
    I2C1->STAR1 = 0;
}

int main(void) {
    SystemInit();

    // GPIOクロック有効化 (Port A, C, D)
    funGpioInitAll();

#ifdef PIN_BL
    // SWIO有効モード (3秒)
    Delay_Ms(3000);
    // SWIO無効モード（PD1 GPIOモード）へ切り替え
    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOD;
    AFIO->PCFR1 |= AFIO_PCFR1_SWCFG_DISABLE; // SWIO無効化
    funPinMode(PD1, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
#endif

    // HT1622 制御ピン設定
    funPinMode(PIN_CS, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
    funPinMode(PIN_WR, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
    funPinMode(PIN_DATA, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);

    funDigitalWrite(PIN_CS, FUN_HIGH);
    funDigitalWrite(PIN_WR, FUN_HIGH);

    // I2Cスレーブ開始
    I2C1_Slave_Init(I2C_SLAVE_ADDR);

    while (1) {
        if (dataReady) {
            uint8_t localBuf[RX_BUF_SIZE];
            uint8_t len;

            // 割り込みを一時停止して安全にローカルバッファへコピー
            __disable_irq();
            len = rxLen;
            for (uint8_t i = 0; i < len; i++) {
                localBuf[i] = rxBuf[i];
            }
            dataReady = false;
            __enable_irq();

            uint8_t mode = localBuf[0];
            if (mode == 0x00 && len >= 2) {
                // コマンドモード: [0x00, CMD]
                HT1622_writeCommand(localBuf[1]);
            }
            else if (mode == 0x01 && len >= 3) {
                // 1ニブル/バイトモード: [0x01, ADDR, D0, D1, ...]
                HT1622_writeDataBurst(localBuf[1], &localBuf[2], len - 2);
            }
            else if (mode == 0x02 && len >= 3) {
                // 2ニブル/バイトモード: [0x02, ADDR, (D0<<4)|D1, (D2<<4)|D3, ...]
                HT1622_writeDataPacked(localBuf[1], &localBuf[2], len - 2);
#ifdef PIN_BL
            } else if (mode == 0x03 && len == 2) {
                funDigitalWrite(PIN_BL, localBuf[1]);
#endif
            }
        }
    }
}
