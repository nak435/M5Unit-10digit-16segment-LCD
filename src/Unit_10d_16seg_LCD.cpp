#include "Unit_10d_16seg_LCD.h"

// 桁アドレス(左から)
static const uint8_t DIG_ADDR[DM8BA10_DIGITS] = {0x24, 0x20, 0x1C, 0x18, 0x14,
                                                 0x10, 0x0C, 0x08, 0x04, 0x00};

// フォント表 (0x20-0x7F): 作者が調整した字形表。
// Calculate 16 (Grid) by Benjamin Blåholtz (https://www.dafont.com/calculate-16.font) と
// road-t/DM8BA10 (MIT) の字形を元に、実機の見え方に合わせて直したもの。
// 16bit の MSB が先頭アドレスの D0。
static const uint16_t FONT[96] = {
    0x0000,0x0540,0x0060,0x156B,0xB5CB,0xBC5B,0xD345,0x0040,0x0014,0x4800,0x5C5C,0x1448,0x0800,0x1008,0x0100,0x0810,
    0xABB3,0x0022,0x93A9,0x91AB,0x302A,0xB18B,0xB38B,0x80A2,0xB3AB,0xB1AB,0x1100,0x8800,0x0018,0x9088,0x5000,0xA4A8,
    0x9DA3,0x083A,0x85EB,0xA381,0x85E3,0xB381,0xB280,0xA38B,0x322A,0x85C1,0x0323,0x3214,0x2301,0x6232,0x6226,0xA3A3,
    0xB2A8,0xA3A7,0xB2AC,0xC18B,0x84C0,0x2323,0x2A10,0x2A26,0x4814,0x4410,0x8991,0x04C1,0x4004,0x8540,0x0804,0x0101,
    0x4000,0x1D00,0x3700,0x1300,0x1740,0x1B00,0x14C8,0x000F,0x044A,0x0480,0x0580,0x044C,0x0840,0x160A,0x1600,0x1700,
    0x7200,0x003A,0x0408,0x000D,0x0449,0x0700,0x0A00,0x0A06,0x180C,0x0007,0x1900,0x14C1,0x0440,0x8548,0x6010,0xFFFF };
bool Unit_10d_16seg_LCD::send(const uint8_t* data, size_t len) {
    _wire->beginTransmission(_addr);
    _wire->write(data, len);
    _lastError = _wire->endTransmission();
    delay(DM8BA10_GAP_MS);
    return _lastError == 0;
}

bool Unit_10d_16seg_LCD::isConnected() {
    _wire->beginTransmission(_addr);
    return _wire->endTransmission() == 0;
}

bool Unit_10d_16seg_LCD::waitBridge(uint32_t timeoutMs) {
    uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
        if (isConnected()) return true;
        delay(100);
    }
    return false;
}

bool Unit_10d_16seg_LCD::begin(TwoWire* wire, uint8_t addr, int8_t sda, int8_t scl,
                         uint32_t speed, uint32_t waitMs) {
    _wire  = wire;
    _addr  = addr;
    _sda   = sda;
    _scl   = scl;
    _speed = speed;
    if (_sda >= 0 && _scl >= 0)
        _wire->begin(_sda, _scl);
    else
        _wire->begin();
    _wire->setClock(_speed);
    if (!waitBridge(waitMs)) return false;
    sendCommand(DM8BA10_CMD_SYS_EN);
    sendCommand(DM8BA10_CMD_LCD_ON);
    memset(_ram, 0, sizeof _ram);
    return update();
}

uint8_t Unit_10d_16seg_LCD::getLastError() {
    return _lastError;
}

void Unit_10d_16seg_LCD::setAutoUpdate(bool on) {
    _autoUpdate = on;
}

void Unit_10d_16seg_LCD::apply() {
    if (_autoUpdate) update();
}

// RAM 全体(アドレス 0..63)を 0x02 モードで1回送信(2+32=34バイト)
bool Unit_10d_16seg_LCD::update() {
    uint8_t buf[2 + DM8BA10_RAM / 2];
    buf[0] = DM8BA10_MODE_PACKED;
    buf[1] = 0x00;
    for (int i = 0; i < DM8BA10_RAM / 2; i++) {
        buf[2 + i] = ((_ram[2 * i] & 0x0F) << 4) | (_ram[2 * i + 1] & 0x0F);
    }
    return send(buf, sizeof buf);
}

void Unit_10d_16seg_LCD::clear() {
    memset(_ram, 0, sizeof _ram);
    apply();
}

void Unit_10d_16seg_LCD::fill() {
    memset(_ram, 0x0F, sizeof _ram);
    apply();
}

void Unit_10d_16seg_LCD::clearPoints() {
    _ram[0x29] = _ram[0x2B] = _ram[0x2D] = 0;
    apply();
}

// 16bit 語は MSB から送る: 先頭4bit が addr の D0..D3 (bit15 が D0)
static void drawDigit(uint8_t* ram, uint8_t pos, uint16_t pat) {
    for (int k = 0; k < 4; k++) {
        uint8_t v = 0;
        for (int i = 0; i < 4; i++) {
            if ((pat >> (15 - 4 * k - i)) & 1) v |= (1 << i);
        }
        ram[DIG_ADDR[pos] + k] = v;
    }
}

void Unit_10d_16seg_LCD::setDigit(uint8_t pos, uint16_t pattern) {
    if (pos >= DM8BA10_DIGITS) return;
    drawDigit(_ram, pos, pattern);
    apply();
}

void Unit_10d_16seg_LCD::setChar(uint8_t pos, char c) {
    if (pos >= DM8BA10_DIGITS) return;
    uint16_t pat = (c >= 0x20 && c < 0x80) ? FONT[c - 0x20] : 0;
    drawDigit(_ram, pos, pat);
    apply();
}

void Unit_10d_16seg_LCD::print(const char* text) {
    uint8_t pts[3] = {_ram[0x29], _ram[0x2B], _ram[0x2D]};  // 小数点は保持
    memset(_ram, 0, sizeof _ram);
    _ram[0x29] = pts[0];
    _ram[0x2B] = pts[1];
    _ram[0x2D] = pts[2];
    for (int i = 0; i < DM8BA10_DIGITS; i++) {
        char c       = *text ? *text++ : ' ';
        uint16_t pat = (c >= 0x20 && c < 0x80) ? FONT[c - 0x20] : 0;
        drawDigit(_ram, i, pat);
    }
    apply();
}

void Unit_10d_16seg_LCD::print(const String& text) {
    print(text.c_str());
}

// 小数点: cluster=idx/3 -> アドレス 0x29+2*cluster, idx%3 -> D1,D2,D3
void Unit_10d_16seg_LCD::setPoint(uint8_t idx, bool on) {
    if (idx >= DM8BA10_POINTS) return;
    uint8_t addr = (idx / 3) * 2 + 0x29;
    uint8_t mask = 2 << (idx % 3);
    if (on)
        _ram[addr] |= mask;
    else
        _ram[addr] &= ~mask;
    apply();
}

void Unit_10d_16seg_LCD::togglePoint(uint8_t idx) {
    if (idx >= DM8BA10_POINTS) return;
    _ram[(idx / 3) * 2 + 0x29] ^= (2 << (idx % 3));
    apply();
}

void Unit_10d_16seg_LCD::setBacklightActiveLow(bool activeLow) {
    _blActiveLow = activeLow;
}

void Unit_10d_16seg_LCD::setBacklight(bool on) {
    uint8_t b[2] = {DM8BA10_MODE_BL, (uint8_t)((on != _blActiveLow) ? 1 : 0)};
    send(b, 2);
}

void Unit_10d_16seg_LCD::sendCommand(uint8_t cmd) {
    uint8_t b[2] = {DM8BA10_MODE_CMD, cmd};
    send(b, 2);
}

void Unit_10d_16seg_LCD::setRamNibble(uint8_t addr, uint8_t value) {
    if (addr >= DM8BA10_RAM) return;
    _ram[addr] = value & 0x0F;
    apply();
}

uint8_t Unit_10d_16seg_LCD::getRamNibble(uint8_t addr) {
    return addr < DM8BA10_RAM ? _ram[addr] : 0;
}
