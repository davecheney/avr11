#include <Arduino.h>
#include "avr11.h"
#include "cons.h"

int32_t TKS, TKB, TPS, TPB;
bool ready;

void clearterminal() {
  TKS = 0;
  TPS = 1 << 7;
  TKB = 0;
  TPB = 0;
  ready = true;
}

void writeterminal(char c) {
  Serial.print(c);
}

void addchar(char c) {
  switch (c) {
  case 42:
    TKB = 4;
    break;
  case 19:
    TKB = 034; 
    break;
    //case 46:
    //	TKB = 127;
  default:
    TKB = c; 
    break;
  }
  TKS |= 0x80;
  ready = false;
  if (TKS&(1<<6)) {
    // interrupt(INTTTYIN, 4);
  }
}

int32_t consgetchar() {
  if (TKS&0x80) {
    TKS &= 0xff7e;
    ready = true;
    return TKB;
  }
  return 0;
}

void stepcons() {
  if ((TPS&0x80) == 0) {
    writeterminal(TPB & 0x7f);
    TPS |= 0x80;
    if (TPS&(1<<6)) {
      //interrupt(INTTTYOUT, 4);
    }
  }
}

uint16_t consread16(int32_t a) {
  switch (a) {
  case 0777560:
    return TKS;
  case 0777562:
    return consgetchar();
  case 0777564:
    return TPS;
  case 0777566:
    return 0;
  default:
    panic("consread16: read from invalid address"); // " + ostr(a, 6))
  }
}

void conswrite16(int32_t a, uint16_t v) {
  switch (a) {
  case 0777560:
    if (v&(1<<6)) {
      TKS |= 1 << 6;
    } 
    else {
      TKS &= ~(1 << 6);
    }
    break;
  case 0777564:
    if (v&(1<<6)) {
      TPS |= 1 << 6;
    } 
    else {
      TPS &= ~(1 << 6);
    }
    break;
  case 0777566:
    TPB = v & 0xff;
    TPS &= 0xff7f;
  default:
    panic("conswrite16: write to invalid address"); // " + ostr(a, 6))
  }
}

