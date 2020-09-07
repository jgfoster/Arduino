
#define BUFFER_SIZE 100
// https://github.com/Locoduino/RingBuffer
#include <RingBuf.h>
RingBuf<byte, BUFFER_SIZE> data;
unsigned long overflowCount = 0;
unsigned long priorOverflowCount = 0;
enum { FOUR_BIT = 0, EIGHT_BIT = 1 } sizeMode = EIGHT_BIT;
enum { CONTROL = 0, DATA = 1 } dataMode = CONTROL;

void setup() {
  delay(200);  // https://forum.arduino.cc/index.php?topic=600452.0
  Serial.begin(2000000);
  Serial.println("Read pins 2-7 as RS, E, and 4-7 of 1602 LCD device");
//  Uncommenting these lines causes subsequent Serial.print() calls to truncate output
//  This indicates a significant limit to this approach
//  Serial.println("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz");
//  Serial.println("01234567890123456789012345678901234567890123456789012345678901234567890123456789");
  for (int i = 2; i < 8; ++i) {
    pinMode(i, INPUT_PULLUP);
  }
  // https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/
  attachInterrupt(digitalPinToInterrupt(3), pin3falling, FALLING);
}

byte getPins() {
  byte pins = 0;
  for (int i = 7; i >= 2; --i) {
    pins = pins * 2 + digitalRead(i);
  }
  return pins;
}

void pin3falling() {
  if (data.isFull()) {
    ++overflowCount;
  } else {
    data.lockedPush(getPins());
  }
}

void loop() {
  unsigned long newOverflowCount = overflowCount;
  if (priorOverflowCount != newOverflowCount) {
    priorOverflowCount = newOverflowCount;
    Serial.print("Overflow: ");
    Serial.println(newOverflowCount);
  }
  if ((sizeMode == EIGHT_BIT && !data.isEmpty()) || (data.size() >= 2)) {
    bool isData;
    byte value;
    if (sizeMode == EIGHT_BIT) {
      data.lockedPop(value);
      isData = value & 0x1;
      value = value >> 2 << 4;
    } else {
      byte temp1, temp2;
      data.lockedPop(temp1);
      data.lockedPop(temp2);
      isData = temp1 & 0x1;
      value = (temp1 >> 2 << 4) + (temp2 >> 2);
    }
    if (isData) {
      if (dataMode == CONTROL) {
        Serial.print('"');
        dataMode = DATA;
      }
      char character = value;
      Serial.print(character);
    } else {
      if (dataMode == DATA) {
        Serial.println('"');
        dataMode = CONTROL;
      }
      if (value & 0x80)        { // set DDRAM address
        Serial.print("DDRAM: 0x");
        Serial.println(value & 0x7F, HEX);
      } else if (value & 0x40) { // set CGRAM address
        Serial.print("CGRAM: 0x");
        Serial.println(value & 0x3F, HEX);
      } else if (value & 0x20) { // function set
        if (value & 0x10) {
          sizeMode = EIGHT_BIT;
          Serial.print('8');
        } else {
          if (sizeMode == EIGHT_BIT) {
            while (data.isEmpty()) {
              delay(1);
            }
            byte temp;
            if (!data.lockedPop(temp)) {
              Serial.println("MISSING DATA");
            }
            value = value + (temp >> 2);
          }
          sizeMode = FOUR_BIT;
          Serial.print('4');
        }
        Serial.print("-bit; ");
        if (value & 0x08) {
          Serial.print("2 lines; ");
        } else {
          Serial.print("1 line; ");
        }
        if (value & 0x04) {
          Serial.println("5x10");
        } else {
          Serial.println("5x8");
        }
      } else if (value & 0x10) { // cursor/display shift
        if (value & 0x08) {
          Serial.print("shift display; ");
        } else {
          Serial.print("move cursor; ");
        }
        Serial.print("shift ");
        if (value & 0x04) {
          Serial.println("right");
        } else {
          Serial.println("left");
        }
      } else if (value & 0x08) { // display on/off control
        Serial.print("display ");
        if (value & 0x04) {
          Serial.print("on");
        } else {
          Serial.print("off");
        }
        Serial.print("; cursor ");
        if (value & 0x02) {
          Serial.print("on");
        } else {
          Serial.print("off");
        }
        Serial.print("; blink ");
        if (value & 0x01) {
          Serial.println("on");
        } else {
          Serial.println("off");
        }
      } else if (value & 0x04) { // entry mode set
        if (value & 0x02) {
          Serial.print("increment");
        } else {
          Serial.print("decrement");
        }
        Serial.print(" cursor position; ");
        if (!(value & 0x01)) {
          Serial.print("no ");
        }
        Serial.println("display shift");
      } else if (value & 0x02) { // cursor home
        Serial.println("cursor home");
      } else if (value & 0x01) { // clear display
        Serial.println("clear display");
      } else {
        Serial.println("invalid command (0x00)");
      }
    }
  }
}
