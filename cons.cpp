#include <Arduino.h>
#include <SdFat.h>
#include <stdio.h>

int printf(const char *format, ...);

#include "avr11.h"

namespace cons {

uint16_t TKS;
uint16_t TKB;
uint16_t TPS;
uint16_t TPB;

void clearterminal() {
  TKS = 0;
  TPS = 1 << 7;
  TKB = 0;
  TPB = 0;
}

static void addchar(char c) {
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
  if (TKS & (1 << 6)) {
    cpu::interrupt(INTTTYIN, 4);
  }
}

uint8_t count;

void poll() {
  if(Serial.available())
      addchar(Serial.read()); 
     
    if ((TPS & 0x80) == 0) {
    if (++count > 32) {
      Serial.write(TPB & 0x7f);
      TPS |= 0x80;
      if (TPS & (1 << 6)) {
        cpu::interrupt(INTTTYOUT, 4);
      }
    }
  }  
    
  #ifdef __ATMEGA2560__
  if (UCSR0A & _BV(RXC0)) {
    addchar(UDR0);
  }


  if ((TPS & 0x80) == 0) {
    if (++count > 32) {
      UDR0 = TPB & 0x7f;
      TPS |= 0x80;
      if (TPS & (1 << 6)) {
        cpu::interrupt(INTTTYOUT, 4);
      }
    }
  }  
  #endif
}

// TODO(dfc) this could be rewritten to translate to the native AVR UART registers
// http://www.appelsiini.net/2011/simple-usart-with-avr-libc

uint16_t read16(uint32_t a) {
  switch (a) {
    case 0777560:
      return TKS;
    case 0777562:
      if (TKS & 0x80) {
        TKS &= 0xff7e;
        return TKB;
      }
      return 0;
    case 0777564:
      return TPS;
    case 0777566:
      return 0;
    default:
      printf("consread16: read from invalid address: %06o\n", a);
      panic();
  }
}

void write16(uint32_t a, uint16_t v) {
  switch (a) {
    case 0777560:
      if (v & (1 << 6)) {
        TKS |= 1 << 6;
      }
      else {
        TKS &= ~(1 << 6);
      }
      break;
    case 0777564:
      if (v & (1 << 6)) {
        TPS |= 1 << 6;
      }
      else {
        TPS &= ~(1 << 6);
      }
      break;
    case 0777566:
      TPB = v & 0xff;
      TPS &= 0xff7f;
      count = 0;
      break;
    default:
      printf("conswrite16: write to invalid address: %06o\n", a); 
      panic();
  }
}

};
