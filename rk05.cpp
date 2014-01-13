#include <stdint.h>
#include "avr11.h"
#include "rk05.h"

int32_t RKBA, RKDS, RKER, RKCS, RKWC;
int32_t drive, sector, surface, cylinder;

bool running;

int32_t rkread16(int32_t a){
  switch (a) {
  case 0777400:
    return RKDS;
  case 0777402:
    return RKER;
  case 0777404:
    return RKCS | (RKBA&0x30000)>>12;
  case 0777406:
    return RKWC;
  case 0777410:
    return RKBA & 0xFFFF;
  case 0777412:
    return (sector) | (surface << 4) | (cylinder << 5) | (drive << 13);
  default:
    panic(); //panic("invalid read")
  }
}

void rknotready() {
  RKDS &= ~(1 << 6);
  RKCS &= ~(1 << 7);
}

void rkready() {
  RKDS |= 1 << 6;
  RKCS |= 1 << 7;
}

void rkgo() {
  switch ((RKCS & 017) >> 1) {
  case 0:
    rkreset(); 
    break;
  case 1:
  case 2:
    running = true;
    rknotready(); 
    break;
  default:
    panic(); // (fmt.Sprintf("unimplemented RK05 operation %#o", ((r.RKCS & 017) >> 1)))
  }
}

void rkwrite16(int32_t a, uint16_t v) {
  switch (a) {
  case 0777400:
    break;
  case 0777402:
    break;
  case 0777404:
    RKBA = (RKBA & 0xFFFF) | ((v & 060) << 12);
    v &= 017517; // writable bits
    RKCS &= ~017517;
    RKCS |= v & ~1; // don't set GO bit
    if (v&1) {
      rkgo();
    }
    break;
  case 0777406:
    RKWC = v; 
    break;
  case 0777410:
    RKBA = (RKBA & 0x30000) | (v); 
    break;
  case 0777412:
    drive = v >> 13;
    cylinder = (v >> 5) & 0377;
    surface = (v >> 4) & 1;
    sector = v & 15;
    break;
  default:
    panic(); // "invalid write")
  }
}

void rkreset() {
  RKDS = (1 << 11) | (1 << 7) | (1 << 6);
  RKER = 0;
  RKCS = 1 << 7;
  RKWC = 0;
  RKBA = 0;
}


