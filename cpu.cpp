#include <SdFat.h>
#include <setjmp.h>
#include <stdio.h>

#include "avr11.h"
#include "bootrom.h"

extern jmp_buf trapbuf;

pdp11::intr itab[ITABN];

namespace cpu {

uint16_t R[8];

uint16_t PS;       // processor status
uint16_t PC;       // address of current instruction
uint16_t KSP, USP; // kernel and user stack pointer
uint16_t LKS;
bool curuser, prevuser;

void reset(void) {
  LKS = 1 << 7;
  uint16_t i;
  for (i = 0; i < 29; i++) {
    unibus::write16(02000 + (i * 2), bootrom[i]);
  }
  R[7] = 002002;
  cons::clearterminal();
  rk11::reset();
}

#define N (PS & FLAGN)
#define Z (PS & FLAGZ)
#define V (PS & FLAGV)
#define C (PS & FLAGC)

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

static bool isReg(const uint16_t a) {
  return (a & 0177770) == 0170000;
}

static uint16_t memread16(const uint16_t a) {
  if (isReg(a)) {
    return R[a & 7];
  }
  return read16(a);
}

static uint16_t memread(uint16_t a, uint8_t l) {
  if (isReg(a)) {
    const uint8_t r = a & 7;
    if (l == 2) {
      return R[r];
    } else {
      return R[r] & 0xFF;
    }
  }
  if (l == 2) {
    return read16(a);
  }
  return read8(a);
}

static void memwrite16(const uint16_t a, const uint16_t v) {
  if (isReg(a)) {
    R[a & 7] = v;
  } else {
    write16(a, v);
  }
}

static void memwrite(const uint16_t a, const uint8_t l, const uint16_t v) {
  if (isReg(a)) {
    const uint8_t r = a & 7;
    if (l == 2) {
      R[r] = v;
    } else {
      R[r] &= 0xFF00;
      R[r] |= v;
    }
    return;
  }
  if (l == 2) {
    write16(a, v);
  } else {
    write8(a, v);
  }
}

static uint16_t fetch16() {
  const uint16_t val = read16(R[7]);
  R[7] += 2;
  return val;
}

static void push(const uint16_t v) {
  R[6] -= 2;
  write16(R[6], v);
}

static uint16_t pop() {
  const uint16_t val = read16(R[6]);
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

void switchmode(const bool newm) {
  prevuser = curuser;
  curuser = newm;
  if (prevuser) {
    USP = R[6];
  } else {
    KSP = R[6];
  }
  if (curuser) {
    R[6] = USP;
  } else {
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

static void setZ(const bool b) {
  if (b)
    PS |= FLAGZ;
}

#define D(x) (x & 077)
#define S(x) ((x & 07700) >> 6)
#define L(x) (2 - (x >> 15))
#define SA(x) (aget(S(x), L(x)))
#define DA(x) (aget(D(x), L(x)))

static void MOV(const uint16_t instr) {
  uint8_t l = 2 - (instr >> 15);
  const uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t uval = memread(aget(S(instr), l), l);
  const uint16_t da = DA(instr);
  PS &= 0xFFF1;
  if (uval & msb) {
    PS |= FLAGN;
  }
  setZ(uval == 0);
  if ((isReg(da)) && (l == 1)) {
    l = 2;
    if (uval & msb) {
      uval |= 0xFF00;
    }
  }
  memwrite(da, l, uval);
}

static void CMP(const uint16_t instr) {
  const uint8_t l = 2 - (instr >> 15);
  const uint16_t msb = l == 2 ? 0x8000 : 0x80;
  const uint16_t max = l == 2 ? 0xFFFF : 0xff;
  const uint16_t val1 = memread(aget(S(instr), l), l);
  const uint16_t da = DA(instr);
  const uint16_t val2 = memread(da, l);
  const int32_t sval = (val1 - val2) & max;
  PS &= 0xFFF0;
  setZ(sval == 0);
  if (sval & msb) {
    PS |= FLAGN;
  }
  if (((val1 ^ val2) & msb) && (!((val2 ^ sval) & msb))) {
    PS |= FLAGV;
  }
  if (val1 < val2) {
    PS |= FLAGC;
  }
}

static void BIT(const uint16_t instr) {
  const uint8_t l = 2 - (instr >> 15);
  const uint16_t msb = l == 2 ? 0x8000 : 0x80;
  const uint16_t val1 = memread(SA(instr), l);
  const uint16_t da = DA(instr);
  const uint16_t val2 = memread(da, l);
  const uint16_t uval = val1 & val2;
  PS &= 0xFFF1;
  setZ(uval == 0);
  if (uval & msb) {
    PS |= FLAGN;
  }
}

static void BIC(const uint16_t instr) {
  const uint8_t l = 2 - (instr >> 15);
  const uint16_t msb = l == 2 ? 0x8000 : 0x80;
  const uint16_t max = l == 2 ? 0xFFFF : 0xff;
  const uint16_t val1 = memread(SA(instr), l);
  const uint16_t da = DA(instr);
  const uint16_t val2 = memread(da, l);
  const uint16_t uval = (max ^ val1) & val2;
  PS &= 0xFFF1;
  setZ(uval == 0);
  if (uval & msb) {
    PS |= FLAGN;
  }
  memwrite(da, l, uval);
}

static void BIS(const uint16_t instr) {
  const uint8_t l = 2 - (instr >> 15);
  const uint16_t msb = l == 2 ? 0x8000 : 0x80;
  const uint16_t val1 = memread(SA(instr), l);
  const uint16_t da = DA(instr);
  const uint16_t val2 = memread(da, l);
  const uint16_t uval = val1 | val2;
  PS &= 0xFFF1;
  setZ(uval == 0);
  if (uval & msb) {
    PS |= FLAGN;
  }
  memwrite(da, l, uval);
}

static void ADD(const uint16_t instr) {
  const uint16_t val1 = memread16(aget(S(instr), 2));
  const uint16_t da = aget(D(instr), 2);
  const uint16_t val2 = memread16(da);
  const uint16_t uval = (val1 + val2) & 0xFFFF;
  PS &= 0xFFF0;
  setZ(uval == 0);
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

static void SUB(const uint16_t instr) {
  const uint16_t val1 = memread16(aget(S(instr), 2));
  const uint16_t da = aget(D(instr), 2);
  const uint16_t val2 = memread16(da);
  const uint16_t uval = (val2 - val1) & 0xFFFF;
  PS &= 0xFFF0;
  setZ(uval == 0);
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

static void JSR(const uint16_t instr) {
  const uint16_t uval = DA(instr);
  if (isReg(uval)) {
    printf("JSR called on registeri\n");
    panic();
  }
  push(R[S(instr) & 7]);
  R[S(instr) & 7] = R[7];
  R[7] = uval;
}

static void MUL(const uint16_t instr) {
  int32_t val1 = R[S(instr) & 7];
  if (val1 & 0x8000) {
    val1 = -((0xFFFF ^ val1) + 1);
  }
  uint16_t da = DA(instr);
  int32_t val2 = memread16(da);
  if (val2 & 0x8000) {
    val2 = -((0xFFFF ^ val2) + 1);
  }
  const int32_t sval = val1 * val2;
  R[S(instr) & 7] = sval >> 16;
  R[(S(instr) & 7) | 1] = sval & 0xFFFF;
  PS &= 0xFFF0;
  if (sval & 0x80000000) {
    PS |= FLAGN;
  }
  setZ((sval & 0xFFFFFFFF) == 0);
  if ((sval < (1 << 15)) || (sval >= ((1L << 15) - 1))) {
    PS |= FLAGC;
  }
}

static void DIV(const uint16_t instr) {
  const int32_t val1 = (R[S(instr) & 7] << 16) | (R[(S(instr) & 7) | 1]);
  const uint16_t da = DA(instr);
  const int32_t val2 = memread16(da);
  PS &= 0xFFF0;
  if (val2 == 0) {
    PS |= FLAGC;
    return;
  }
  if ((val1 / val2) >= 0x10000) {
    PS |= FLAGV;
    return;
  }
  R[S(instr) & 7] = (val1 / val2) & 0xFFFF;
  R[(S(instr) & 7) | 1] = (val1 % val2) & 0xFFFF;
  setZ(R[S(instr) & 7] == 0);
  if (R[S(instr) & 7] & 0100000) {
    PS |= FLAGN;
  }
  if (val1 == 0) {
    PS |= FLAGV;
  }
}

static void ASH(const uint16_t instr) {
  const uint16_t val1 = R[S(instr) & 7];
  const uint16_t da = aget(D(instr), 2);
  uint16_t val2 = memread16(da) & 077;
  PS &= 0xFFF0;
  int32_t sval;
  if (val2 & 040) {
    val2 = (077 ^ val2) + 1;
    if (val1 & 0100000) {
      sval = 0xFFFF ^ (0xFFFF >> val2);
      sval |= val1 >> val2;
    } else {
      sval = val1 >> val2;
    }
    if (val1 & (1 << (val2 - 1))) {
      PS |= FLAGC;
    }
  } else {
    sval = (val1 << val2) & 0xFFFF;
    if (val1 & (1 << (16 - val2))) {
      PS |= FLAGC;
    }
  }
  R[S(instr) & 7] = sval;
  setZ(sval == 0);
  if (sval & 0100000) {
    PS |= FLAGN;
  }
  if ((sval & 0100000)xor(val1 & 0100000)) {
    PS |= FLAGV;
  }
}

static void ASHC(const uint16_t instr) {
  const uint32_t val1 = R[S(instr) & 7] << 16 | R[(S(instr) & 7) | 1];
  const uint16_t da = aget(D(instr), 2);
  uint16_t val2 = memread16(da) & 077;
  PS &= 0xFFF0;
  int32_t sval;
  if (val2 & 040) {
    val2 = (077 ^ val2) + 1;
    if (val1 & 0x80000000) {
      sval = 0xFFFFFFFF ^ (0xFFFFFFFF >> val2);
      sval |= val1 >> val2;
    } else {
      sval = val1 >> val2;
    }
    if (val1 & (1 << (val2 - 1))) {
      PS |= FLAGC;
    }
  } else {
    sval = (val1 << val2) & 0xFFFFFFFF;
    if (val1 & (1 << (32 - val2))) {
      PS |= FLAGC;
    }
  }
  R[S(instr) & 7] = (sval >> 16) & 0xFFFF;
  R[(S(instr) & 7) | 1] = sval & 0xFFFF;
  setZ(sval == 0);
  if (sval & 0x80000000) {
    PS |= FLAGN;
  }
  if ((sval & 0x80000000)xor(val1 & 0x80000000)) {
    PS |= FLAGV;
  }
}

static void XOR(const uint16_t instr) {
  const uint16_t val1 = R[S(instr) & 7];
  const uint16_t da = aget(D(instr), 2);
  const uint16_t val2 = memread16(da);
  const uint16_t uval = val1 ^ val2;
  PS &= 0xFFF1;
  setZ(uval == 0);
  if (uval & 0x8000) {
    PS |= FLAGN;
  }
  memwrite16(da, uval);
}

static void SOB(const uint16_t instr) {
  uint8_t o = instr & 0xFF;
  R[S(instr) & 7]--;
  if (R[S(instr) & 7]) {
    o &= 077;
    o <<= 1;
    R[7] -= o;
  }
}

static void CLR(const uint16_t instr) {
  const uint8_t l = 2 - (instr >> 15);
  PS &= 0xFFF0;
  PS |= FLAGZ;
  const uint16_t da = DA(instr);
  memwrite(da, l, 0);
}

static void COM(const uint16_t instr) {
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = DA(instr);
  uint16_t uval = memread(da, l) ^ max;
  PS &= 0xFFF0;
  PS |= FLAGC;
  if (uval & msb) {
    PS |= FLAGN;
  }
  setZ(uval == 0);
  memwrite(da, l, uval);
}

static void INC(const uint16_t instr) {
  const uint8_t l = 2 - (instr >> 15);
  const uint16_t msb = l == 2 ? 0x8000 : 0x80;
  const uint16_t max = l == 2 ? 0xFFFF : 0xff;
  const uint16_t da = DA(instr);
  const uint16_t uval = (memread(da, l) + 1) & max;
  PS &= 0xFFF1;
  if (uval & msb) {
    PS |= FLAGN | FLAGV;
  }
  setZ(uval == 0);
  memwrite(da, l, uval);
}

static void _DEC(const uint16_t instr) {
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t maxp = l == 2 ? 0x7FFF : 0x7f;
  uint16_t da = DA(instr);
  uint16_t uval = (memread(da, l) - 1) & max;
  PS &= 0xFFF1;
  if (uval & msb) {
    PS |= FLAGN;
  }
  if (uval == maxp) {
    PS |= FLAGV;
  }
  setZ(uval == 0);
  memwrite(da, l, uval);
}

static void NEG(const uint16_t instr) {
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = DA(instr);
  int32_t sval = (-memread(da, l)) & max;
  PS &= 0xFFF0;
  if (sval & msb) {
    PS |= FLAGN;
  }
  if (sval == 0) {
    PS |= FLAGZ;
  } else {
    PS |= FLAGC;
  }
  if (sval == 0x8000) {
    PS |= FLAGV;
  }
  memwrite(da, l, sval);
}

static void _ADC(const uint16_t instr) {
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = DA(instr);
  uint16_t uval = memread(da, l);
  if (PS & FLAGC) {
    PS &= 0xFFF0;
    if ((uval + 1) & msb) {
      PS |= FLAGN;
    }
    setZ(uval == max);
    if (uval == 0077777) {
      PS |= FLAGV;
    }
    if (uval == 0177777) {
      PS |= FLAGC;
    }
    memwrite(da, l, (uval + 1) & max);
  } else {
    PS &= 0xFFF0;
    if (uval & msb) {
      PS |= FLAGN;
    }
    setZ(uval == 0);
  }
}

static void SBC(const uint16_t instr) {
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = DA(instr);
  int32_t sval = memread(da, l);
  if (PS & FLAGC) {
    PS &= 0xFFF0;
    if ((sval - 1) & msb) {
      PS |= FLAGN;
    }
    setZ(sval == 1);
    if (sval) {
      PS |= FLAGC;
    }
    if (sval == 0100000) {
      PS |= FLAGV;
    }
    memwrite(da, l, (sval - 1) & max);
  } else {
    PS &= 0xFFF0;
    if (sval & msb) {
      PS |= FLAGN;
    }
    setZ(sval == 0);
    if (sval == 0100000) {
      PS |= FLAGV;
    }
    PS |= FLAGC;
  }
}

static void TST(const uint16_t instr) {
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t uval = memread(aget(D(instr), l), l);
  PS &= 0xFFF0;
  if (uval & msb) {
    PS |= FLAGN;
  }
  setZ(uval == 0);
}

static void ROR(const uint16_t instr) {
  uint8_t l = 2 - (instr >> 15);
  int32_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = DA(instr);
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
  setZ(!(sval & max));
  if ((sval & 1)xor(sval & (max + 1))) {
    PS |= FLAGV;
  }
  sval >>= 1;
  memwrite(da, l, sval);
}

static void ROL(const uint16_t instr) {
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  int32_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = DA(instr);
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
  setZ(!(sval & max));
  if ((sval ^ (sval >> 1)) & msb) {
    PS |= FLAGV;
  }
  sval &= max;
  memwrite(da, l, sval);
}

static void ASR(const uint16_t instr) {
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t da = DA(instr);
  uint16_t uval = memread(da, l);
  PS &= 0xFFF0;
  if (uval & 1) {
    PS |= FLAGC;
  }
  if (uval & msb) {
    PS |= FLAGN;
  }
  if ((uval & msb)xor(uval & 1)) {
    PS |= FLAGV;
  }
  uval = (uval & msb) | (uval >> 1);
  setZ(uval == 0);
  memwrite(da, l, uval);
}

static void ASL(const uint16_t instr) {
  uint8_t l = 2 - (instr >> 15);
  uint16_t msb = l == 2 ? 0x8000 : 0x80;
  uint16_t max = l == 2 ? 0xFFFF : 0xff;
  uint16_t da = DA(instr);
  // TODO(dfc) doesn't need to be an sval
  int32_t sval = memread(da, l);
  PS &= 0xFFF0;
  if (sval & msb) {
    PS |= FLAGC;
  }
  if (sval & (msb >> 1)) {
    PS |= FLAGN;
  }
  if ((sval ^ (sval << 1)) & msb) {
    PS |= FLAGV;
  }
  sval = (sval << 1) & max;
  setZ(sval == 0);
  memwrite(da, l, sval);
}

static void SXT(const uint16_t instr) {
  const uint8_t l = 2 - (instr >> 15);
  const uint16_t max = l == 2 ? 0xFFFF : 0xff;
  const uint16_t da = DA(instr);
  if (PS & FLAGN) {
    memwrite(da, l, max);
  } else {
    PS |= FLAGZ;
    memwrite(da, l, 0);
  }
}

static void JMP(const uint16_t instr) {
  const uint16_t uval = aget(D(instr), 2);
  if (isReg(uval)) {
    printf("JMP called with register dest\n");
    panic();
  }
  R[7] = uval;
}

static void SWAB(const uint16_t instr) {
  uint8_t l = 2 - (instr >> 15);
  uint16_t da = DA(instr);
  uint16_t uval = memread(da, l);
  uval = ((uval >> 8) | (uval << 8)) & 0xFFFF;
  PS &= 0xFFF0;
  setZ(uval & 0xFF);
  if (uval & 0x80) {
    PS |= FLAGN;
  }
  memwrite(da, l, uval);
}

static void MARK(const uint16_t instr) {
  R[6] = R[7] + ((instr & 077) << 1);
  R[7] = R[5];
  R[5] = pop();
}

static void MFPI(const uint16_t instr) {
  uint16_t da = aget(D(instr), 2);
  uint16_t uval;
  if (da == 0170006) {
    // val = (curuser == prevuser) ? R[6] : (prevuser ? k.USP : KSP);
    if (curuser == prevuser) {
      uval = R[6];
    } else {
      if (prevuser) {
        uval = USP;
      } else {
        uval = KSP;
      }
    }
  } else if (isReg(da)) {
    printf("invalid MFPI instruction\n");
    panic();
    return; // unreached
  } else {
    uval = unibus::read16(mmu::decode((uint16_t)da, false, prevuser));
  }
  push(uval);
  PS &= 0xFFF0;
  PS |= FLAGC;
  setZ(uval == 0);
  if (uval & 0x8000) {
    PS |= FLAGN;
  }
}

static void MTPI(const uint16_t instr) {
  uint16_t da = aget(D(instr), 2);
  uint16_t uval = pop();
  if (da == 0170006) {
    if (curuser == prevuser) {
      R[6] = uval;
    } else {
      if (prevuser) {
        USP = uval;
      } else {
        KSP = uval;
      }
    }
  } else if (isReg(da)) {
    printf("invalid MTPI instrution\n");
    panic();
  } else {
    unibus::write16(mmu::decode((uint16_t)da, true, prevuser), uval);
  }
  PS &= 0xFFF0;
  PS |= FLAGC;
  setZ(uval == 0);
  if (uval & 0x8000) {
    PS |= FLAGN;
  }
}

static void RTS(const uint16_t instr) {
  R[7] = R[D(instr) & 7];
  R[D(instr) & 7] = pop();
}

static void EMTX(const uint16_t instr) {
  uint16_t uval;
  if ((instr & 0177400) == 0104000) {
    uval = 030;
  } else if ((instr & 0177400) == 0104400) {
    uval = 034;
  } else if (instr == 3) {
    uval = 014;
  } else {
    uval = 020;
  }
  uint16_t prev = PS;
  switchmode(false);
  push(prev);
  push(R[7]);
  R[7] = unibus::read16(uval);
  PS = unibus::read16(uval + 2);
  if (prevuser) {
    PS |= (1 << 13) | (1 << 12);
  }
}

static void _RTT() {
  R[7] = pop();
  uint16_t uval = pop();
  if (curuser) {
    uval &= 047;
    uval |= PS & 0177730;
  }
  unibus::write16(0777776, uval);
}

static void RESET() {
  if (curuser) {
    return;
  }
  cons::clearterminal();
  rk11::reset();
}

void step() {
  PC = R[7];
  uint16_t instr = unibus::read16(mmu::decode(PC, false, curuser));
  R[7] += 2;

  if (PRINTSTATE)
    printstate();

  switch ((instr >> 12) & 007) {
    case 001: // MOV
      MOV(instr);
      return;
    case 002: // CMP
      CMP(instr);
      return;
    case 003: // BIT
      BIT(instr);
      return;
    case 004: // BIC
      BIC(instr);
      return;
    case 005: // BIS
      BIS(instr);
      return;
  }
  switch ((instr >> 12) & 017) {
    case 006: // ADD
      ADD(instr);
      return;
    case 016: // SUB
      SUB(instr);
      return;
  }
  switch ((instr >> 9) & 0177) {
    case 0004: // JSR
      JSR(instr);
      return;
    case 0070: // MUL
      MUL(instr);
      return;
    case 0071: // DIV
      DIV(instr);
      return;
    case 0072: // ASH
      ASH(instr);
      return;
    case 0073: // ASHC
      ASHC(instr);
      return;
    case 0074: // XOR
      XOR(instr);
      return;
    case 0077: // SOB
      SOB(instr);
      return;
  }
  switch ((instr >> 6) & 00777) {
    case 00050: // CLR
      CLR(instr);
      return;
    case 00051: // COM
      COM(instr);
      return;
    case 00052: // INC
      INC(instr);
      return;
    case 00053: // DEC
      _DEC(instr);
      return;
    case 00054: // NEG
      NEG(instr);
      return;
    case 00055: // ADC
      _ADC(instr);
      return;
    case 00056: // SBC
      SBC(instr);
      return;
    case 00057: // TST
      TST(instr);
      return;
    case 00060: // ROR
      ROR(instr);
      return;
    case 00061: // ROL
      ROL(instr);
      return;
    case 00062: // ASR
      ASR(instr);
      return;
    case 00063: // ASL
      ASL(instr);
      return;
    case 00067: // SXT
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
      MARK(instr);
      break;
    case 0006500: // MFPI
      MFPI(instr);
      return;
    case 0006600: // MTPI
      MTPI(instr);
      return;
  }
  if ((instr & 0177770) == 0000200) { // RTS
    RTS(instr);
    return;
  }

  switch (instr & 0177400) {
    case 0000400:
      branch(instr & 0xFF);
      return;
    case 0001000:
      if (!Z) {
        branch(instr & 0xFF);
      }
      return;
    case 0001400:
      if (Z) {
        branch(instr & 0xFF);
      }
      return;
    case 0002000:
      if (!(N xor V)) {
        branch(instr & 0xFF);
      }
      return;
    case 0002400:
      if (N xor V) {
        branch(instr & 0xFF);
      }
      return;
    case 0003000:
      if ((!(N xor V)) && (!Z)) {
        branch(instr & 0xFF);
      }
      return;
    case 0003400:
      if ((N xor V) || Z) {
        branch(instr & 0xFF);
      }
      return;
    case 0100000:
      if (!N) {
        branch(instr & 0xFF);
      }
      return;
    case 0100400:
      if (N) {
        branch(instr & 0xFF);
      }
      return;
    case 0101000:
      if ((!C) && (!Z)) {
        branch(instr & 0xFF);
      }
      return;
    case 0101400:
      if (C || Z) {
        branch(instr & 0xFF);
      }
      return;
    case 0102000:
      if (!V) {
        branch(instr & 0xFF);
      }
      return;
    case 0102400:
      if (V) {
        branch(instr & 0xFF);
      }
      return;
    case 0103000:
      if (!C) {
        branch(instr & 0xFF);
      }
      return;
    case 0103400:
      if (C) {
        branch(instr & 0xFF);
      }
      return;
  }
  if (((instr & 0177000) == 0104000) || (instr == 3) ||
      (instr == 4)) { // EMT TRAP IOT BPT
    EMTX(instr);
    return;
  }
  if ((instr & 0177740) == 0240) { // CL?, SE?
    if (instr & 020) {
      PS |= instr & 017;
    } else {
      PS &= ~instr & 017;
    }
    return;
  }
  switch (instr & 7) {
    case 00: // HALT
      if (curuser) {
        break;
      }
      printf("HALT\n");
      panic();
    case 01: // WAIT
      if (curuser) {
        break;
      }
      return;
    case 02: // RTI

    case 06: // RTT
      _RTT();
      return;
    case 05: // RESET
      RESET();
      return;
  }
  if (instr == 0170011) {
    // SETD ; not needed by UNIX, but used; therefore ignored
    return;
  }
  printf("invalid instruction\n");
  trap(INTINVAL);
}

void trapat(uint16_t vec) { // , msg string) {
  if (vec & 1) {
    printf("Thou darst calling trapat() with an odd vector number?\n");
    panic();
  }
  printf("trap: %x\n", vec);

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
    printf("Thou darst calling interrupt() with an odd vector number?\n");
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
    printf("interrupt table full\n");
    panic();
  }
  uint8_t j;
  for (j = i + 1; j < ITABN; j++) {
    itab[j] = itab[j - 1];
  }
  itab[i].vec = vec;
  itab[i].pri = pri;
}

// pop the top interrupt off the itab.
static void popirq() {
  uint8_t i;
  for (i = 0; i < ITABN - 1; i++) {
    itab[i] = itab[i + 1];
  }
  itab[ITABN - 1].vec = 0;
  itab[ITABN - 1].pri = 0;
}

void handleinterrupt() {
  uint8_t vec = itab[0].vec;
  if (DEBUG_INTER) {
    printf("IRQ: %x\n", vec);
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
  popirq();
}
};
