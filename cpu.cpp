#include <SdFat.h>
#include "avr11.h"
#include "mmu.h"
#include "cons.h"
#include "unibus.h"
#include "cpu.h"

#include "bootrom.h"
#include "rk05.h"

pdp11::intr itab[ITABN];

namespace cpu {

// signed integer registers
int32_t R[8];

uint16_t	PS; // processor status
uint16_t	PC; // address of current instruction
uint16_t   KSP, USP; // kernel and user stack pointer
uint16_t LKS;
bool curuser, prevuser;

void reset(void) {
  LKS = 1 << 7;
  uint16_t i;
  for (i = 0; i < 29; i++) {
    unibus::write16(02000 + (i * 2), bootrom[i]);
  }
  R[7] = 02002;
  cons::clearterminal();
  rk11::reset();
}

static uint16_t read8(const uint16_t a) {
  return unibus::read8(mmu::decode(a, false, curuser));
}

static uint16_t read16(const uint16_t a) {
  return unibus::read16(mmu::decode(a, false, curuser));
}

static void write8(const uint16_t a, const uint16_t v) {
  unibus::write8(mmu::decode(a, true, curuser), v);
}

static void write16(const uint16_t a, const uint16_t v) {
  unibus::write16(mmu::decode(a, true, curuser), v);
}

static bool isReg(uint16_t a) {
  return (a & 0177770) == 0170000;
}

static uint16_t memread16(uint16_t a) {
  if (isReg(a)) {
    return R[a & 7];
  }
  return read16(a);
}

uint16_t memread(uint16_t a, uint8_t l) {
  if (isReg(a)) {
    uint8_t r = a & 7;
    if (l == 2) {
      return R[r];
    }
    else {
      return R[r] & 0xFF;
    }
  }
  if (l == 2) {
    return read16(a);
  }
  return read8(a);
}

static void memwrite16(uint16_t a, uint16_t v) {
  if (isReg(a)) {
    R[a & 7] = v;
  } else {
    write16(a, v);
  }
}

void memwrite(uint16_t a, uint8_t l, uint16_t v) {
  if (isReg(a)) {
    uint8_t r = a & 7;
    if (l == 2) {
      R[r] = v;
    }
    else {
      R[r] &= 0xFF00;
      R[r] |= v;
    }
    return;
  }
  if (l == 2) {
    write16(a, v);
  }
  else {
    write8(a, v);
  }
}

static uint16_t fetch16() {
  uint16_t val = read16(R[7]);
  R[7] += 2;
  return val;
}

static void push(uint16_t v) {
  R[6] -= 2;
  write16(R[6], v);
}

static uint16_t pop() {
  uint16_t val = read16(R[6]);
  R[6] += 2;
  return val;
}

// aget resolves the operand to a vaddress.
// if the operand is a register, an address in
// the range [0170000,0170007). This address range is
// technically a valid IO page, but unibus doesn't map
// any addresses here, so we can safely do this.
static uint16_t aget(uint8_t v, uint8_t l) {
  if ((v & 070) == 000) {
    return 0170000 | (v & 7);
  }
  if (((v & 7) >= 6) || (v & 010)) {
    l = 2;
  }
  uint16_t addr = 0;
  switch (v & 060) {
    case 000:
      v &= 7;
      addr = R[v & 7];
      break;
    case 020:
      addr = R[v & 7];
      R[v & 7] += l;
      break;
    case 040:
      R[v & 7] -= l;
      addr = R[v & 7];
      break;
    case 060:
      addr = fetch16();
      addr += R[v & 7];
      break;
  }
  if (v & 010) {
    addr = read16(addr);
  }
  return addr;
}

static void branch(int16_t o) {
  if (o & 0x80) {
    o = -(((~o) + 1) & 0xFF);
  }
  o <<= 1;
  R[7] += o;
}

void switchmode(bool newm) {
  prevuser = curuser;
  curuser = newm;
  if (prevuser) {
    USP = R[6];
  }
  else {
    KSP = R[6];
  }
  if (curuser) {
    R[6] = USP;
  }
  else {
    R[6] = KSP;
  }
  PS &= 0007777;
  if (curuser) {
    PS |= (1 << 15) | (1 << 14);
  }
  if (prevuser) {
    PS |= (1 << 13) | (1 << 12);
  }
}

int32_t xor32(int32_t x, int32_t y) {
  int32_t a, b, z;
  a = x & y;
  b = ~x & ~y;
  z = ~a & ~b;
  return z;
}

uint16_t xor16(uint16_t x, uint16_t y) {
  uint16_t a, b, z;
  a = x & y;
  b = ~x & ~y;
  z = ~a & ~b;
  return z;
}

static void MOV(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t uval = memread(aget(s, l), l);
  uint16_t da = aget(d, l);
  PS &= 0xFFF1;
  if (uval & msb) {
    PS |= FLAGN;
  }
  if (uval == 0) {
    PS |= FLAGZ;
  }
  if ((isReg(da)) && (l == 1)) {
    l = 2;
    if (uval & msb) {
      uval |= 0xFF00;
    }
  }
  memwrite(da, l, uval);
}

static void CMP(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t val1 = memread(aget(s, l), l);
  uint16_t da = aget(d, l);
  uint16_t val2 = memread(da, l);
  int32_t sval = (val1 - val2) & max;
  PS &= 0xFFF0;
  if (sval == 0) {
    PS |= FLAGZ;
  }
  if (sval & msb) {
    PS |= FLAGN;
  }
  if (((val1 ^ val2)&msb) && (!((val2 ^ sval)&msb))) {
    PS |= FLAGV;
  }
  if (val1 < val2) {
    PS |= FLAGC;
  }
}

static void BIT(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t val1 = memread(aget(s, l), l);
  uint16_t da = aget(d, l);
  uint16_t val2 = memread(da, l);
  uint16_t uval = val1 & val2;
  PS &= 0xFFF1;
  if (uval == 0) {
    PS |= FLAGZ;
  }
  if (uval & msb) {
    PS |= FLAGN;
  }
}

static void BIC(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t val1 = memread(aget(s, l), l);
  uint16_t da = aget(d, l);
  uint16_t val2 = memread(da, l);
  uint16_t uval = (max ^ val1) & val2;
  PS &= 0xFFF1;
  if (uval == 0) {
    PS |= FLAGZ;
  }
  if (uval & msb) {
    PS |= FLAGN;
  }
  memwrite(da, l, uval);
}

static void BIS(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t val1 = memread(aget(s, l), l);
  uint16_t da = aget(d, l);
  uint16_t val2 = memread(da, l);
  uint16_t uval = val1 | val2;
  PS &= 0xFFF1;
  if (uval == 0) {
    PS |= FLAGZ;
  }
  if (uval & msb) {
    PS |= FLAGN;
  }
  memwrite(da, l, uval);
}

static void ADD(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  uint8_t l = 2 - (instr >> 15);
  uint16_t val1 = memread16(aget(s, 2));
  uint16_t da = aget(d, 2);
  uint16_t val2 = memread16(da);
  uint16_t uval = (val1 + val2) & 0xFFFF;
  PS &= 0xFFF0;
  if (uval == 0) {
    PS |= FLAGZ;
  }
  if (uval & 0x8000) {
    PS |= FLAGN;
  }
  if (!((val1 ^ val2) & 0x8000) && ((val2 ^ uval) & 0x8000)) {
    PS |= FLAGV;
  }
  if ((val1 + val2) >= 0xFFFF) {
    PS |= FLAGC;
  }
  memwrite16(da, uval);
}

static void SUB(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  uint8_t l = 2 - (instr >> 15);
  uint16_t val1 = memread16(aget(s, 2));
  uint16_t da = aget(d, 2);
  uint16_t val2 = memread16(da);
  uint16_t uval = (val2 - val1) & 0xFFFF;
  PS &= 0xFFF0;
  if (uval == 0) {
    PS |= FLAGZ;
  }
  if (uval & 0x8000) {
    PS |= FLAGN;
  }
  if (((val1 ^ val2) & 0x8000) && (!((val2 ^ uval) & 0x8000))) {
    PS |= FLAGV;
  }
  if (val1 > val2) {
    PS |= FLAGC;
  }
  memwrite16(da, uval);
}

static void JSR(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  uint8_t l = 2 - (instr >> 15);
  uint16_t uval = aget(d, l);
  if (isReg(uval)) {
    Serial.println(F("JSR called on register"));
    panic();
  }
  push(R[s & 7]);
  R[s & 7] = R[7];
  R[7] = uval;
}

static void MUL(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  int32_t val1 = R[s & 7];
  if (val1 & 0x8000) {
    val1 = -((0xFFFF ^ val1) + 1);
  }
  uint8_t l = 2 - (instr >> 15);
  uint16_t da = aget(d, l);
  int32_t val2 = (int32_t)memread16(da);
  if (val2 & 0x8000) {
    val2 = -((0xFFFF ^ val2) + 1);
  }
  int32_t sval = val1 * val2;
  R[s & 7] = (sval & 0xFFFF0000) >> 16;
  R[(s & 7) | 1] = sval & 0xFFFF;
  PS &= 0xFFF0;
  if (sval & 0x80000000) {
    PS |= FLAGN;
  }
  if ((sval & 0xFFFFFFFF) == 0) {
    PS |= FLAGZ;
  }
  if ((sval < (1 << 15)) || (sval >= ((1 << 15) - 1))) {
    PS |= FLAGC;
  }
}

static void DIV(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  int32_t val1 = (R[s & 7] << 16) | (R[(s & 7) | 1]);
  uint8_t l = 2 - (instr >> 15);
  uint16_t da = aget(d, l);
  int32_t val2 = (int32_t)memread16(da);
  PS &= 0xFFF0;
  if (val2 == 0) {
    PS |= FLAGC;
    return;
  }
  if ((val1 / val2) >= 0x10000) {
    PS |= FLAGV;
    return;
  }
  R[s & 7] = (val1 / val2) & 0xFFFF;
  R[(s & 7) | 1] = (val1 % val2) & 0xFFFF;
  if (R[s & 7] == 0) {
    PS |= FLAGZ;
  }
  if (R[s & 7] & 0100000) {
    PS |= FLAGN;
  }
  if (val1 == 0) {
    PS |= FLAGV;
  }
}

static void ASH(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  uint16_t val1 = R[s & 7];
  uint16_t da = aget(d, 2);
  uint16_t val2 = memread16(da) & 077;
  PS &= 0xFFF0;
  int32_t sval;
  if (val2 & 040) {
    val2 = (077 ^ val2) + 1;
    if (val1 & 0100000) {
      sval = 0xFFFF ^ (0xFFFF >> val2);
      sval |= val1 >> val2;
    }
    else {
      sval = val1 >> val2;
    }
    int32_t shift;
    shift = 1 << (val2 - 1);
    if (val1 & shift) {
      PS |= FLAGC;
    }
  }
  else {
    sval = (val1 << val2) & 0xFFFF;
    int32_t shift;
    shift = 1 << (16 - val2);
    if (val1 & shift) {
      PS |= FLAGC;
    }
  }
  R[s & 7] = sval;
  if (sval == 0) {
    PS |= FLAGZ;
  }
  if (sval & 0100000) {
    PS |= FLAGN;
  }
  if (xor32(sval & 0100000, val1 & 0100000)) {
    PS |= FLAGV;
  }
}

static void ASHC(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  uint16_t val1 = R[s & 7] << 16 | R[(s & 7) | 1];
  uint16_t da = aget(d, 2);
  uint16_t val2 = memread16(da) & 077;
  PS &= 0xFFF0;
  int32_t sval;
  if (val2 & 040) {
    val2 = (077 ^ val2) + 1;
    if (val1 & 0x80000000) {
      sval = 0xFFFFFFFF ^ (0xFFFFFFFF >> val2);
      sval |= val1 >> val2;
    }
    else {
      sval = val1 >> val2;
    }
    if (val1 & (1 << (val2 - 1))) {
      PS |= FLAGC;
    }
  }
  else {
    sval = (val1 << val2) & 0xFFFFFFFF;
    if (val1 & (1 << (32 - val2))) {
      PS |= FLAGC;
    }
  }
  R[s & 7] = (sval >> 16) & 0xFFFF;
  R[(s & 7) | 1] = sval & 0xFFFF;
  if (sval == 0) {
    PS |= FLAGZ;
  }
  if (sval & 0x80000000) {
    PS |= FLAGN;
  }
  if (xor32(sval & 0x80000000, val1 & 0x80000000)) {
    PS |= FLAGV;
  }
}

static void XOR(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  uint16_t val1 = R[s & 7];
  uint16_t da = aget(d, 2);
  uint16_t val2 = memread16(da);
  uint16_t uval = val1 ^ val2;
  PS &= 0xFFF1;
  if (uval == 0) {
    PS |= FLAGZ;
  }
  if (uval & 0x8000) {
    PS |= FLAGZ;
  }
  memwrite16(da, uval);
}

static void SOB(uint16_t instr) {
  uint8_t s = (instr & 07700) >> 6;
  uint8_t o = instr & 0xFF;
  R[s & 7]--;
  if (R[s & 7]) {
    o &= 077;
    o <<= 1;
    R[7] -= o;
  }
}

static void CLR(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t l = 2 - (instr >> 15);
  PS &= 0xFFF0;
  PS |= FLAGZ;
  memwrite(aget(d, l), l, 0);
}

static void COM(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = aget(d, l);
  uint16_t uval = memread(da, l) ^ max;
  PS &= 0xFFF0;
  PS |= FLAGC;
  if (uval & msb) {
    PS |= FLAGN;
  }
  if (uval == 0) {
    PS |= FLAGZ;
  }
  memwrite(da, l, uval);
}

static void INC(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = aget(d, l);
  uint16_t uval = (memread(da, l) + 1) & max;
  PS &= 0xFFF1;
  if (uval & msb) {
    PS |= FLAGN | FLAGV;
  }
  if (uval == 0) {
    PS |= FLAGZ;
  }
  memwrite(da, l, uval);
}

static void _DEC(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t maxp = l == 2 ? 0x7FFF : 0x7f;
  uint16_t da = aget(d, l);
  uint16_t uval = (memread(da, l) - 1) & max;
  PS &= 0xFFF1;
  if (uval & msb) {
    PS |= FLAGN;
  }
  if (uval == maxp) {
    PS |= FLAGV;
  }
  if (uval == 0) {
    PS |= FLAGZ;
  }
  memwrite(da, l, uval);
}

static void NEG(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = aget(d, l);
  int32_t sval = (-memread(da, l)) & max;
  PS &= 0xFFF0;
  if (sval & msb) {
    PS |= FLAGN;
  }
  if (sval == 0) {
    PS |= FLAGZ;
  }
  else {
    PS |= FLAGC;
  }
  if (sval == 0x8000) {
    PS |= FLAGV;
  }
  memwrite(da, l, sval);
}

static void _ADC(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = aget(d, l);
  uint16_t uval = memread(da, l);
  if (PS & FLAGC) {
    PS &= 0xFFF0;
    if ((uval + 1)&msb) {
      PS |= FLAGN;
    }
    if (uval == max) {
      PS |= FLAGZ;
    }
    if (uval == 0077777) {
      PS |= FLAGV;
    }
    if (uval == 0177777) {
      PS |= FLAGC;
    }
    memwrite(da, l, (uval + 1)&max);
  }
  else {
    PS &= 0xFFF0;
    if (uval & msb) {
      PS |= FLAGN;
    }
    if (uval == 0) {
      PS |= FLAGZ;
    }
  }
}

static void SBC(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = aget(d, l);
  int32_t sval = memread(da, l);
  if (PS & FLAGC) {
    PS &= 0xFFF0;
    if ((sval - 1)&msb) {
      PS |= FLAGN;
    }
    if (sval == 1) {
      PS |= FLAGZ;
    }
    if (sval) {
      PS |= FLAGC;
    }
    if (sval == 0100000) {
      PS |= FLAGV;
    }
    memwrite(da, l, (sval - 1)&max);
  }
  else {
    PS &= 0xFFF0;
    if (sval & msb) {
      PS |= FLAGN;
    }
    if (sval == 0) {
      PS |= FLAGZ;
    }
    if (sval == 0100000) {
      PS |= FLAGV;
    }
    PS |= FLAGC;
  }
}

static void TST(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t uval = memread(aget(d, l), l);
  PS &= 0xFFF0;
  if (uval & msb) {
    PS |= FLAGN;
  }
  if (uval == 0) {
    PS |= FLAGZ;
  }
}

static void ROR(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t l = 2 - (instr >> 15);
  int32_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = aget(d, l);
  int32_t sval = memread(da, l);
  if (PS & FLAGC) {
    sval |= max + 1;
  }
  PS &= 0xFFF0;
  if (sval & 1) {
    PS |= FLAGC;
  }
  // watch out for integer wrap around
  if (sval & (max + 1)) {
    PS |= FLAGN;
  }
  if (!(sval & max)) {
    PS |= FLAGZ;
  }
  if (xor16(sval & 1, sval & (max + 1))) {
    PS |= FLAGV;
  }
  sval >>= 1;
  memwrite(da, l, sval);
}

static void ROL(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  int32_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = aget(d, l);
  int32_t sval = memread(da, l) << 1;
  if (PS & FLAGC) {
    sval |= 1;
  }
  PS &= 0xFFF0;
  if (sval & (max + 1)) {
    PS |= FLAGC;
  }
  if (sval & msb) {
    PS |= FLAGN;
  }
  if (!(sval & max)) {
    PS |= FLAGZ;
  }
  if ((sval ^ (sval >> 1))&msb) {
    PS |= FLAGV;
  }
  sval &= max;
  memwrite(da, l, sval);
}

static void ASR(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t da = aget(d, l);
  uint16_t uval = memread(da, l);
  PS &= 0xFFF0;
  if (uval & 1) {
    PS |= FLAGC;
  }
  if (uval & msb) {
    PS |= FLAGN;
  }
  if (xor16(uval & msb, uval & 1)) {
    PS |= FLAGV;
  }
  uval = (uval & msb) | (uval >> 1);
  if (uval == 0) {
    PS |= FLAGZ;
  }
  memwrite(da, l, uval);
}

static void ASL(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = aget(d, l);
  // TODO(dfc) doesn't need to be an sval
  int32_t sval = memread(da, l);
  PS &= 0xFFF0;
  if (sval & msb) {
    PS |= FLAGC;
  }
  if (sval & (msb >> 1)) {
    PS |= FLAGN;
  }
  if ((sval ^ (sval << 1))&msb) {
    PS |= FLAGV;
  }
  sval = (sval << 1) & max;
  if (sval == 0) {
    PS |= FLAGZ;
  }
  memwrite(da, l, sval);
}

static void SXT(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t l = 2 - (instr >> 15);
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = aget(d, l);
  if (PS & FLAGN) {
    memwrite(da, l, max);
  }
  else {
    PS |= FLAGZ;
    memwrite(da, l, 0);
  }
}

static void JMP(uint16_t instr) {
  uint8_t d = instr & 077;
  uint16_t uval = aget(d, 2);
  if (isReg(uval)) {
    Serial.println(F("JMP called with register dest"));
    panic();
  }
  R[7] = uval;
}

static void SWAB(uint16_t instr) {
  uint8_t d = instr & 077;
  uint8_t l = 2 - (instr >> 15);
  uint16_t da = aget(d, l);
  uint16_t uval = memread(da, l);
  uval = ((uval >> 8) | (uval << 8)) & 0xFFFF;
  PS &= 0xFFF0;
  if (uval & 0xFF) {
    PS |= FLAGZ;
  }
  if (uval & 0x80) {
    PS |= FLAGN;
  }
  memwrite(da, l, uval);
}

void step() {
  uint16_t uval;
  int32_t sval;
  uint16_t max, maxp, msb, prev, da;
  int32_t val1, val2;
  PC = R[7];
  uint16_t instr = unibus::read16(mmu::decode(PC, false, curuser));
  R[7] += 2;

  if (PRINTSTATE) printstate();

  uint8_t d = instr & 077;
  uint8_t s = (instr & 07700) >> 6;
  uint8_t l = 2 - (instr >> 15);
  uint8_t o = instr & 0xFF;
  if (l == 2) {
    max = 0xFFFF;
    maxp = 0x7FFF;
    msb = 0x8000;
  }
  else {
    max = 0xFF;
    maxp = 0x7F;
    msb = 0x80;
  }
  switch (instr & 0070000) {
    case 0010000: // MOV
      MOV(instr);
      return;
    case 0020000: // CMP
      CMP(instr);
      return;
    case 0030000: // BIT
      BIT(instr);
      return;
    case 0040000: // BIC
      BIC(instr);
      return;
    case 0050000: // BIS
      BIS(instr);
      return;
  }
  switch (instr & 0170000) {
    case 0060000: // ADD
      ADD(instr);
      return;
    case 0160000: // SUB
      SUB(instr);
      return;
  }
  switch (instr & 0177000) {
    case 0004000: // JSR
      JSR(instr);
      return;
    case 0070000: // MUL
      MUL(instr);
      return;
    case 0071000: // DIV
      DIV(instr);
      return;
    case 0072000: // ASH
      ASH(instr);
      return;
    case 0073000: // ASHC
      ASHC(instr);
      return;
    case 0074000: // XOR
      XOR(instr);
      return;
    case 0077000: // SOB
      SOB(instr);
      return;
  }
  switch (instr & 0077700) {
    case 0005000: // CLR
      CLR(instr);
      return;
    case 0005100: // COM
      COM(instr);
      return;
    case 0005200: // INC
      INC(instr);
      return;
    case 0005300: // DEC
      _DEC(instr);
      return;
    case 0005400: // NEG
      NEG(instr);
      return;
    case 0005500: // ADC
      _ADC(instr);
      return;
    case 0005600: // SBC
      SBC(instr);
      return;
    case 0005700: // TST
      TST(instr);
      return;
    case 0006000: // ROR
      ROR(instr);
      return;
    case 0006100: // ROL
      ROL(instr);
      return;
    case 0006200: // ASR
      ASR(instr);
      return;
    case 0006300: // ASL
      ASL(instr);
      return;
    case 0006700: // SXT
      SXT(instr);
      return;
  }
  switch (instr & 0177700) {
    case 0000100: // JMP
      JMP(instr);
      return;
    case 0000300: // SWAB
      SWAB(instr);
      return;
    case 0006400: // MARK
      R[6] = R[7] + ((instr & 077) << 1);
      R[7] = R[5];
      R[5] = pop();
      break;
    case 0006500: // MFPI
      da = aget(d, 2);
      if (da == 0170006) {
        // val = (curuser == prevuser) ? R[6] : (prevuser ? k.USP : KSP);
        if (curuser == prevuser) {
          uval = R[6];
        }
        else {
          if (prevuser) {
            uval = USP;
          }
          else {
            uval = KSP;
          }
        }
      }
      else if (isReg(da)) {
        Serial.println(F("invalid MFPI instruction"));
        panic();
      }
      else {
        uval = unibus::read16(mmu::decode((uint16_t)da, false, prevuser));
      }
      push(uval);
      PS &= 0xFFF0;
      PS |= FLAGC;
      if (uval == 0) {
        PS |= FLAGZ;
      }
      if (uval & 0x8000) {
        PS |= FLAGN;
      }
      return;
    case 0006600: // MTPI
      da = aget(d, 2);
      uval = pop();
      if (da == 0170006) {
        if (curuser == prevuser) {
          R[6] = uval;
        }
        else {
          if (prevuser) {
            USP = uval;
          }
          else {
            KSP = uval;
          }
        }
      }
      else if (isReg(da)) {
        Serial.println(F("invalid MTPI instrution")); panic();
      }
      else {
        unibus::write16(mmu::decode((uint16_t)da, true, prevuser), uval);
      }
      PS &= 0xFFF0;
      PS |= FLAGC;
      if (uval == 0) {
        PS |= FLAGZ;
      }
      if (uval & 0x8000) {
        PS |= FLAGN;
      }
      return;
  }
  if ((instr & 0177770) == 0000200) { // RTS
    R[7] = R[d & 7];
    R[d & 7] = pop();
    return;
  }
  switch (instr & 0177400) {
    case 0000400:
      branch(o);
      return;
    case 0001000:
      if (!(PS & FLAGZ)) {
        branch(o);
      }
      return;
    case 0001400:
      if (PS & FLAGZ) {
        branch(o);
      }
      return;
    case 0002000:
      if (!(xor16(PS & FLAGN, PS & FLAGV))) {
        branch(o);
      }
      return;
    case 0002400:
      if (xor16(PS & FLAGN, PS & FLAGV)) {
        branch(o);
      }
      return;
    case 0003000:
      if ((!(xor16(PS & FLAGN, PS & FLAGV))) && (!(PS & FLAGZ))) {
        branch(o);
      }
      return;
    case 0003400:
      if ((xor16(PS & FLAGN, PS & FLAGV)) || (PS & FLAGZ)) {
        branch(o);
      }
      return;
    case 0100000:
      if ((PS & FLAGN) == 0) {
        branch(o);
      }
      return;
    case 0100400:
      if (PS & FLAGN) {
        branch(o);
      }
      return;
    case 0101000:
      if ((!(PS & FLAGC)) && (!(PS & FLAGZ))) {
        branch(o);
      }
      return;
    case 0101400:
      if ((PS & FLAGC) || (PS & FLAGZ)) {
        branch(o);
      }
      return;
    case 0102000:
      if (!(PS & FLAGV)) {
        branch(o);
      }
      return;
    case 0102400:
      if (PS & FLAGV) {
        branch(o);
      }
      return;
    case 0103000:
      if (!(PS & FLAGC)) {
        branch(o);
      }
      return;
    case 0103400:
      if (PS & FLAGC) {
        branch(o);
      }
      return;
  }
  if (((instr & 0177000) == 0104000) || (instr == 3) || (instr == 4)) { // EMT TRAP IOT BPT
    if ((instr & 0177400) == 0104000) {
      uval = 030;
    }
    else if ((instr & 0177400) == 0104400) {
      uval = 034;
    }
    else if (instr == 3) {
      uval = 014;
    }
    else {
      uval = 020;
    }
    prev = PS;
    switchmode(false);
    push(prev);
    push(R[7]);
    R[7] = unibus::read16(uval);
    PS = unibus::read16(uval + 2);
    if (prevuser) {
      PS |= (1 << 13) | (1 << 12);
    }
    return;
  }
  if ((instr & 0177740) == 0240) { // CL?, SE?
    if (instr & 020) {
      PS |= instr & 017;
    }
    else {
      PS &= ~instr & 017;
    }
    return;
  }
  switch (instr) {
    case 0000000: // HALT
      if (curuser) {
        break;
      }
      Serial.println(F("HALT"));
      panic();
    case 0000001: // WAIT
      if (curuser) {
        break;
      }
      return;
    case 0000002: // RTI

    case 0000006: // RTT
      R[7] = pop();
      uval = pop();
      if (curuser) {
        uval &= 047;
        uval |= PS & 0177730;
      }
      unibus::write16(0777776, uval);
      return;
    case 0000005: // RESET
      if (curuser) {
        return;
      }
      cons::clearterminal();
      rk11::reset();
      return;
    case 0170011: // SETD ; not needed by UNIX, but used; therefore ignored
      return;
  }
  //fmt.Println(ia, disasm(ia))
  Serial.println("invalid instruction");
  longjmp(trapbuf, INTINVAL);
}

void trapat(uint16_t vec) { // , msg string) {
  if (vec & 1) {
    Serial.println(F("Thou darst calling trapat() with an odd vector number?"));
    panic();
  }
  Serial.print(F("trap: ")); Serial.println(vec, OCT);
  //printstate();

  /*var prev uint16
   	defer func() {
   		t = recover()
   		switch t = t.(type) {
   		case trap:
   			writedebug("red stack trap!\n")
   			memory[0] = uint16(k.R[7])
   			memory[1] = prev
   			vec = 4
   			panic("fatal")
   		case nil:
   			break
   		default:
   			panic(t)
   		}
   */
  uint16_t prev = PS;
  switchmode(false);
  push(prev);
  push(R[7]);

  R[7] = unibus::read16(vec);
  PS = unibus::read16(vec + 2);
  if (prevuser) {
    PS |= (1 << 13) | (1 << 12);
  }
}

void interrupt(uint8_t vec, uint8_t pri) {
  if (vec & 1) {
    Serial.println(F("Thou darst calling interrupt() with an odd vector number?"));
    panic();
  }
  // fast path
  if (itab[0].vec == 0) {
    itab[0].vec = vec;
    itab[0].pri = pri;
    return;
  }
  uint8_t i;
  for (i = 0; i < ITABN; i++) {
    if ((itab[i].vec == 0) || (itab[i].pri < pri)) {
      break;
    }
  }
  for (; i < ITABN; i++) {
    if ((itab[i].vec == 0) || (itab[i].vec >= vec)) {
      break;
    }
  }
  if (i >= ITABN) {
    Serial.println(F("interrupt table full")); panic();
  }
  uint8_t j;
  for (j = i + 1; j < ITABN; j++) {
    itab[j] = itab[j - 1];
  }
  itab[i].vec = vec;
  itab[i].pri = pri;
}

void handleinterrupt(uint8_t vec) {
  if (DEBUG_INTER) {
    Serial.print("IRQ: "); Serial.println(vec, OCT);
  }
  uint16_t vv = setjmp(trapbuf);
  if (vv == 0) {
    uint16_t prev = PS;
    switchmode(false);
    push(prev);
    push(R[7]);
  } else {
    trapat(vv);
  }

  R[7] = unibus::read16(vec);
  PS = unibus::read16(vec + 2);
  if (prevuser) {
    PS |= (1 << 13) | (1 << 12);
  }
}

};
