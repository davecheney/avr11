#include <Arduino.h>
#include "avr11.h"
#include "mmu.h"
#include "cpu.h"
#include "unibus.h"
#include "cons.h"
#include "rk05.h"

uint16_t physread8(uint32_t a) {
  uint16_t val;
  val = physread16(a & ~1);
  if (a&1) {
    return val >> 8;
  }
  return val & 0xFF;
}

void physwrite8(uint32_t a, uint16_t v) {
  if (a < 0760000) {
    if (a&1) {
      memory[a>>1] &= 0xFF;
      memory[a>>1] |= v & 0xFF << 8;
    } 
    else {
      memory[a>>1] &= 0xFF00;
      memory[a>>1] |= v & 0xFF;
    }
  } 
  else {
    if (a&1) {
      physwrite16(a&~1, (physread16(a)&0xFF)|(v&0xFF)<<8);
    } 
    else {
      physwrite16(a&~1, (physread16(a)&0xFF00)|(v&0xFF));
    }
  }
}

void physwrite16(uint32_t a, uint16_t v) {
  if (a%1) {
    panic(); //panic(trap{INTBUS, "write to odd address " + ostr(a, 6)})
  }
  if (a < 0760000) {
    memory[a>>1] = v;
  } 
  else if (a == 0777776) {
    switch (v >> 14) {
    case 0:
      switchmode(false);
      break;
    case 3:
      switchmode(true);
      break;
    default:
      panic(); //panic("invalid mode")
    }
    switch ((v >> 12) & 3) {
    case 0:
      prevuser = false;
      break;
    case 3:
      prevuser = true;
      break;
    default:
      panic(); //panic("invalid mode")
    }
    PS = v;
  } 
  else if (a == 0777546) {
    LKS = v;
  } 
  else if (a == 0777572) {
    SR0 = v;
  } 
  else if ((a & 0777770) == 0777560) {
    conswrite16(a, v);
  } 
  else if ((a & 0777700) == 0777400) {
    rkwrite16(a, v);
  } 
  else if (((a&0777600) == 0772200) || ((a&0777600) == 0777600)) {
    mmuwrite16(a, v);
  } 
  else {
    panic(); //panic(trap{INTBUS, "write to invalid address " + ostr(a, 6)})
  }
}
