#include "animations.h"
#include "chars.c"

#define PIN_LIGHT PB1
#define PIN_SERIAL PB0
#define PIN_CLOCK PB3
#define PIN_LATCH PB2
#define PIN_D44 PB4
#define PIN_BTN_D PB5 // PB5 == A0
#define PIN_BTN_A 0   // PB5 == A0

#define DEFAULT_BRIGHTNESS 50 // 0b11111110000000000000000000000000
#define TEXT_BRIGHTNESS_OR 0b01111110000000000000000000000000

#define BUTTON_THRESHOLD 950

static uint8_t _mode = 0;
static int8_t _state = 0;
static int _aniAddr = 0;
static int16_t _aniCount = 0;
static int16_t _aniState = -1;
static char* _aniText = { };
static bool _btnTriggered = false;
static bool _btnDone = false;


void setup() {
  pinMode(PIN_LIGHT, OUTPUT);
  pinMode(PIN_SERIAL, OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);
  pinMode(PIN_LATCH, OUTPUT);
  pinMode(PIN_D44, OUTPUT);
  pinMode(PIN_BTN_D, INPUT);

  applyMode();
}

uint32_t aniShift(uint32_t value, int8_t count) {
  uint32_t brightness = value & 0b11111110000000000000000000000000;
  switch (count) {
    case -4: value = (value << 4) & 0b00000001000010000100001000010000; break;
    case -3: value = (value << 3) & 0b00000001100011000110001100011000; break;
    case -2: value = (value << 2) & 0b00000001110011100111001110011100; break;
    case -1: value = (value << 1) & 0b00000001111011110111101111011110; break;
    case  0: return value;
    case  1: value = (value >> 1) & 0b00000000111101111011110111101111; break;
    case  2: value = (value >> 2) & 0b00000000011100111001110011100111; break;
    case  3: value = (value >> 3) & 0b00000000001100011000110001100011; break;
    case  4: value = (value >> 4) & 0b00000000000100001000010000100001; break;
    default: value = 0; break;
  }
  return brightness | value;
}

void setDisplay(uint32_t d) {
  digitalWrite(PIN_LATCH, LOW); // Latch off
  // Set first 24 pixels via shift registers
  shiftOut(PIN_SERIAL, PIN_CLOCK, MSBFIRST, (d >> 16) & 0xFF);
  shiftOut(PIN_SERIAL, PIN_CLOCK, MSBFIRST, (d >> 8) & 0xFF);
  shiftOut(PIN_SERIAL, PIN_CLOCK, MSBFIRST, d & 0xFF); 
  // Set last pixel on extra pin
  digitalWrite(PIN_D44, (d & 0x01000000) == 0 ? LOW : HIGH);
  digitalWrite(PIN_LATCH, HIGH); // Latch on
  analogWrite(PIN_LIGHT, ((d >> 24) & 0xFE) | 1); // Brightness from first 7 bits and 0x01
}

void initText(char* text, uint16_t len) {
  _aniText = text;
  _aniCount = len;
  _aniState = -5;
  _state = -1;
}

bool loopText() {
  _aniState++;
  if (_aniState > _aniCount * 6 + 6) {
    _aniState = -5;
    return false;
  }
  int8_t index = _aniState / 6;
  int8_t shift = _aniState % 6;
  uint32_t value = TEXT_BRIGHTNESS_OR;
  if (index > -1 && index     < _aniCount) value |= aniShift(getAniChar(_aniText[index    ]), 0 - shift);
  if (index > -2 && index + 1 < _aniCount) value |= aniShift(getAniChar(_aniText[index + 1]), 6 - shift);
  setDisplay(value);
  return true;
}

void initAni(int addr) {
  _aniAddr = addr;
  _aniCount = (int16_t)(pgm_read_dword(addr) & 0x7FFF);
  _aniState = 0;
  _state = -1;
}

bool loopAni(uint32_t brightness) {
  _aniState++;
  if (_aniState > _aniCount) {
    _aniState = 0;
    return false;
  }
  // Read frame from program memory
  uint32_t value = pgm_read_dword(_aniAddr + (_aniState * sizeof(uint32_t)));
  // Apply brightneess
  if (brightness != 0) value |= (brightness & 0x7F) << 25;
  // Apply frame to display
  setDisplay(value);
  return true;
}

void delayAni(uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    if (analogRead(PIN_BTN_A) < BUTTON_THRESHOLD) {
      if (!_btnDone) {
        _btnTriggered = true;
        _btnDone = true;
      }
    } else if (_btnDone) {
      _btnDone = false;
    }
    delay(10);
  }
}

void applyMode() {
  switch (_mode) {
    case 1:
      initAni(&ANI_JUMPING_DOTS);
      break;
    case 2:
      initText("ACHTUNG! SCHULWEG!", 20);
      break;
    case 3:
      initAni(&ANI_JUMPING_DOT2);
      break;
    case 4:
      initAni(&ANI_CAR);
      break;
    default:
      _mode = 0;
      initAni(&ANI_HEART);
      break;
  }
}

void loop() {
  if (_btnTriggered) {
    _mode++;
    applyMode();
    _btnTriggered = false;
  }

  switch (_mode) {
    case 0:
      if (loopAni(0)) delayAni(12);
      break;
    case 2:
      if (loopText()) delayAni(20);
      break;
    default:
      if (loopAni(DEFAULT_BRIGHTNESS)) delayAni(8);
      break;
  }
}
