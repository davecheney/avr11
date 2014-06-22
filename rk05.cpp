#include <stdint.h>
#include <Arduino.h>
#include <SdFat.h>
#include "avr11.h"
#include "unibus.h"
#include "rk05.h"
#include "cpu.h"

namespace rk11 {

uint32_t RKBA, RKDS, RKER, RKCS, RKWC;
uint32_t drive, sector, surface, cylinder;

SdFile rkdata;

uint16_t read16(uint32_t a) {
  switch (a) {
    case 0777400:
      return RKDS;
    case 0777402:
      return RKER;
    case 0777404:
      return RKCS | (RKBA & 0x30000) >> 12;
    case 0777406:
      return RKWC;
    case 0777410:
      return RKBA & 0xFFFF;
    case 0777412:
      return (sector) | (surface << 4) | (cylinder << 5) | (drive << 13);
    default:
      Serial.println(F("rk11::read16 invalid read"));
      panic();
  }
}

static void rknotready() {
  digitalWrite(13, 1);
  RKDS &= ~(1 << 6);
  RKCS &= ~(1 << 7);
}

static void rkready() {
  RKDS |= 1 << 6;
  RKCS |= 1 << 7;
  digitalWrite(13, 0);
}

void rkerror(uint16_t e) {
}

static void step() {
  again:
  bool w;
  switch ((RKCS & 017) >> 1) {
    case 0:
      return;
    case 1:
      w = true;
      break;
    case 2:
      w = false;
      break;
    default:
      Serial.println(F("unimplemented RK05 operation")); //  %#o", ((r.RKCS & 017) >> 1)))
      panic();
  }

  if (DEBUG_RK05) {
    Serial.print("rkstep: RKBA: ");
    Serial.print(RKBA, DEC);
    Serial.print(" RKWC: ");
    Serial.print(RKWC, DEC);
    Serial.print(" cylinder: ");
    Serial.print(cylinder, DEC);
    Serial.print(" sector: ");
    Serial.print(sector, DEC);
    Serial.print(" write: ");
    Serial.println(w ? "true" : "false");
  }

  if (drive != 0) {
    rkerror(RKNXD);
  }
  if (cylinder > 0312) {
    rkerror(RKNXC);
  }
  if (sector > 013) {
    rkerror(RKNXS);
  }

  int32_t pos = (cylinder * 24 + surface * 12 + sector) * 512;
  if (!rkdata.seekSet(pos)) {
    Serial.println(F("rkstep: failed to seek"));
    panic();
  }

  uint16_t i;
  uint16_t val;
  for (i = 0; i < 256 && RKWC != 0; i++) {
    if (w) {
      val = unibus::read16(RKBA);
      rkdata.write(val & 0xFF);
      rkdata.write((val >> 8) & 0xFF);
    } else {
      unibus::write16(RKBA, rkdata.read() | (rkdata.read() << 8));
    }
    RKBA += 2;
    RKWC = (RKWC + 1) & 0xFFFF;
  }
  sector++;
  if (sector > 013) {
    sector = 0;
    surface++;
    if (surface > 1) {
      surface = 0;
      cylinder++;
      if (cylinder > 0312) {
        rkerror(RKOVR);
      }
    }
  }
  if (RKWC == 0) {
    rkready();
    if (RKCS & (1 << 6)) {
      cpu::interrupt(INTRK, 5);
    }
  } else {
    goto again;
  }
}

void write16(uint32_t a, uint16_t v) {
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
      if (v & 1) {
        switch ((RKCS & 017) >> 1) {
          case 0:
            reset();
            break;
          case 1:
          case 2:
            rknotready();
            step();
            break;
          default:
            Serial.println(F("unimplemented RK05 operation")); // %#o", ((r.RKCS & 017) >> 1)))
            panic();
        }
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
      Serial.println(F("rkwrite16: invalid write"));
      panic();
  }
}

void reset() {
  RKDS = (1 << 11) | (1 << 7) | (1 << 6);
  RKER = 0;
  RKCS = 1 << 7;
  RKWC = 0;
  RKBA = 0;
}

};
