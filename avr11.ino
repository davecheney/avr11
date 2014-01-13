#include <stdint.h>
#include <SD.h>
#include "avr11.h"
#include "rk05.h"
#include "cons.h"
#include "mmu.h"

// signed integer registers
int32_t R[8] = { 
  0, 0, 0, 0, 0, 0, 0, 0 };

uint16_t	PS; // processor status
uint16_t	PC; // address of current instruction
uint16_t   KSP, USP; // kernel and user stack pointer
uint16_t SR0, SR2;
uint16_t LKS;
uint8_t curuser, prevuser;

uint32_t clkcounter;

void setup(void)
{
  // Start the UART
  Serial.begin(9600) ;
  Serial.println("setting up..."); 
  cpureset();
  Serial.println("setup done.");
  rkinit();
}

void loop() {
  cpustep();
  rkstep();
}

void printstate() {
  uint32_t ia;
  uint16_t inst;

  Serial.print("R0 "); 
  Serial.print(R[0], OCT);
  Serial.print(" R1 "); 
  Serial.print(R[1], OCT);
  Serial.print(" R2 "); 
  Serial.print(R[2], OCT);
  Serial.print(" R3 "); 
  Serial.print(R[3], OCT);
  Serial.print(" R4 "); 
  Serial.print(R[4], OCT);
  Serial.print(" R5 "); 
  Serial.print(R[5], OCT);
  Serial.print(" R6 "); 
  Serial.print(R[6], OCT);
  Serial.print(" R7 "); 
  Serial.print(R[7], OCT);
  Serial.print("\n [");

  if (prevuser) {
    Serial.print("u");
  } 
  else {
    Serial.print("k");
  }
  if (curuser) {
    Serial.print("U");
  } 
  else {
    Serial.print("K");
  }
  if (PS&FLAGN) {
    Serial.print("N");
  } 
  else {
    Serial.print(" ");
  }
  if (PS&FLAGZ) {
    Serial.print("Z");
  } 
  else {
    Serial.print(" ");
  }
  if (PS&FLAGV) {
    Serial.print("V");
  } 
  else {
    Serial.print(" ");
  }
  if (PS&FLAGC) {
    Serial.print("C");
  } 
  else {
    Serial.print(" ");
  }
  ia = decode(PC, false, curuser);
  inst = physread16(ia);
  Serial.print("]  instr ");
  Serial.print(PC, OCT);
  Serial.print(": ");
  Serial.print(inst, OCT);
  Serial.print("\n"); // + "   " + disasm(ia) + "\n")
}

void panic() {
  printstate();
  Serial.println("panic");
  while (true) delay(1);
}


static uint16_t bootrom[29] = {
  0042113,        /* "KD" */
  0012706, 02000, /* MOV #boot_start, SP */
  0012700, 0000000, /* MOV #unit, R0        ; unit number */
  0010003,          /* MOV R0, R3 */
  0000303,          /* SWAB R3 */
  0006303,          /* ASL R3 */
  0006303,          /* ASL R3 */
  0006303,          /* ASL R3 */
  0006303,          /* ASL R3 */
  0006303,          /* ASL R3 */
  0012701, 0177412, /* MOV #RKDA, R1        ; csr */
  0010311,          /* MOV R3, (R1)         ; load da */
  0005041,          /* CLR -(R1)            ; clear ba */
  0012741, 0177000, /* MOV #-256.*2, -(R1)  ; load wc */
  0012741, 0000005, /* MOV #READ+GO, -(R1)  ; read & go */
  0005002,        /* CLR R2 */
  0005003,        /* CLR R3 */
  0012704, 02020, /* MOV #START+20, R4 */
  0005005, /* CLR R5 */
  0105711, /* TSTB (R1) */
  0100376, /* BPL .-2 */
  0105011, /* CLRB (R1) */
  0005007, /* CLR PC */
};

uint16_t memory[MEMSIZE];

void cpureset(void) {
  uint16_t i;
  for (i = 0; i < 8; i++) {
    R[i] = 0;
  }
  PS = 0;
  PC = 0;
  KSP = 0;
  USP = 0;
  curuser = 0;
  prevuser = 0;
  SR0 = 0;
  LKS = 1 << 7;
  for (i = 0; i < MEMSIZE; i++) {
    memory[i] = 0;
  }
  for (i = 0; i < 29; i++) {
    memory[01000+i] = bootrom[i];
  }
  mmuinit();
  R[7] = 02002;
  clearterminal();
  rkreset();
  clkcounter = 0;
}

void cpustep() {
  uint16_t max, maxp, msb, prev;
  uint8_t d, s, l;
  int32_t ia, sa, da, val, val1, val2, o, instr;
  PC = (uint16_t)R[7];
  ia = decode(PC, false, curuser);
  R[7] += 2;

  //printstate();

  instr = (int32_t)physread16(ia);

  d = instr & 077;
  s = (instr & 07700) >> 6;
  l = 2 - (instr >> 15);
  o = instr & 0xFF;
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
    // k.printstate()
    sa = aget(s, l);
    val = memread(sa, l);
    da = aget(d, l);
    PS &= 0xFFF1;
    if (val&msb) {
      PS |= FLAGN;
    }
    if (val == 0) {
      PS |= FLAGZ;
    }
    if ((da < 0) && (l == 1)) {
      l = 2;
      if (val&msb) {
        val |= 0xFF00;
      }
    }
    memwrite(da, l, val);
    return;
  case 0020000: // CMP
    sa = aget(s, l);
    val1 = memread(sa, l);
    da = aget(d, l);
    val2 = memread(da, l);
    val = (val1 - val2) & max;
    PS &= 0xFFF0;
    if(val == 0){
      PS |= FLAGZ;
    }
    if (val&msb) {
      PS |= FLAGN;
    }
    if (((val1^val2)&msb) && (!((val2^val)&msb))) {
      PS |= FLAGV;
    }
    if (val1 < val2) {
      PS |= FLAGC;
    }
    return;
  case 0030000: // BIT
    sa = aget(s, l);
    val1 = memread(sa, l);
    da = aget(d, l);
    val2 = memread(da, l);
    val = val1 & val2;
    PS &= 0xFFF1;
    if (val == 0) {
      PS |= FLAGZ;
    }
    if (val&msb) {
      PS |= FLAGN;
    }
    return;
  case 0040000: // BIC
    sa = aget(s, l);
    val1 = memread(sa, l);
    da = aget(d, l);
    val2 = memread(da, l);
    val = (max ^ val1) & val2;
    PS &= 0xFFF1;
    if (val == 0) {
      PS |= FLAGZ;
    }
    if (val&msb) {
      PS |= FLAGN;
    }
    memwrite(da, l, val);
    return;
  case 0050000: // BIS
    sa = aget(s, l);
    val1 = memread(sa, l);
    da = aget(d, l);
    val2 = memread(da, l);
    val = val1 | val2;
    PS &= 0xFFF1;
    if (val == 0) {
      PS |= FLAGZ;
    }
    if (val&msb) {
      PS |= FLAGN;
    }
    memwrite(da, l, val);
    return;
  }
  switch (instr & 0170000) {
  case 0060000: // ADD
    sa = aget(s, 2);
    val1 = memread(sa, 2);
    da = aget(d, 2);
    val2 = memread(da, 2);
    val = (val1 + val2) & 0xFFFF;
    PS &= 0xFFF0;
    if(val == 0){
      PS |= FLAGZ;
    }
    if (val&0x8000) {
      PS |= FLAGN;
    }
    if (!((val1^val2)&0x8000) && ((val2^val)&0x8000)) {
      PS |= FLAGV;
    }
    if ((val1+val2) >= 0xFFFF) {
      PS |= FLAGC;
    }
    memwrite(da, 2, val);
    return;
  case 0160000: // SUB
    sa = aget(s, 2);
    val1 = memread(sa, 2);
    da = aget(d, 2);
    val2 = memread(da, 2);
    val = (val2 - val1) & 0xFFFF;
    PS &= 0xFFF0;
    if(val == 0){
      PS |= FLAGZ;
    }
    if (val&0x8000) {
      PS |= FLAGN;
    }
    if (((val1^val2)&0x8000) && (!((val2^val)&0x8000))) {
      PS |= FLAGV;
    }
    if (val1 > val2) {
      PS |= FLAGC;
    }
    memwrite(da, 2, val);
    return;
  }
  switch (instr & 0177000) {
  case 0004000: // JSR
    val = aget(d, l);
    if (val < 0) {
      panic();
      break;
    }
    push((uint16_t)R[s&7]);
    R[s&7] = R[7];
    R[7] = val;
    return;
  case 0070000: // MUL
    val1 = R[s&7];
    if (val1&0x8000) {
      val1 = -((0xFFFF ^ val1) + 1);
    }
    da = aget(d, l);
    val2 = (int32_t)memread(da, 2);
    if (val2&0x8000) {
      val2 = -((0xFFFF ^ val2) + 1);
    }
    val = val1 * val2;
    R[s&7] = (val & 0xFFFF0000) >> 16;
    R[(s&7)|1] = val & 0xFFFF;
    PS &= 0xFFF0;
    if (val&0x80000000) {
      PS |= FLAGN;
    }
    if ((val&0xFFFFFFFF) == 0) {
      PS |= FLAGZ;
    }
    if ((val < (1<<15)) || (val >= ((1<<15)-1))) {
      PS |= FLAGC;
    }
    return;
  case 0071000: // DIV
    val1 = (R[s&7] << 16) | (R[(s&7)|1]);
    da = aget(d, l);
    val2 = (int32_t)memread(da, 2);
    PS &= 0xFFF0;
    if (val2 == 0) {
      PS |= FLAGC;
      return;
    }
    if ((val1/val2) >= 0x10000) {
      PS |= FLAGV;
      return;
    }
    R[s&7] = (val1 / val2) & 0xFFFF;
    R[(s&7)|1] = (val1 % val2) & 0xFFFF;
    if (R[s&7] == 0) {
      PS |= FLAGZ;
    }
    if (R[s&7]&0100000) {
      PS |= FLAGN;
    }
    if (val1 == 0) {
      PS |= FLAGV;
    }
    return;
  case 0072000: // ASH
    val1 = R[s&7];
    da = aget(d, 2);
    val2 = (uint32_t)memread(da, 2) & 077;
    PS &= 0xFFF0;
    if (val2&040) {
      val2 = (077 ^ val2) + 1;
      if (val1&0100000) {
        val = 0xFFFF ^ (0xFFFF >> val2);
        val |= val1 >> val2;
      } 
      else {
        val = val1 >> val2;
      }
      int32_t shift;
      shift = 1 << (val2 - 1);
      if (val1&shift) {
        PS |= FLAGC;
      }
    } 
    else {
      val = (val1 << val2) & 0xFFFF;
      int32_t shift;
      shift = 1 << (16 - val2);
      if (val1&shift) {
        PS |= FLAGC;
      }
    }
    R[s&7] = val;
    if(val == 0){
      PS |= FLAGZ;
    }
    if (val&0100000) {
      PS |= FLAGN;
    }
    if (xor32(val&0100000, val1&0100000)) {
      PS |= FLAGV;
    }
    return; 
  case 0073000: // ASHC
    val1 = R[s&7]<<16 | R[(s&7)|1];
    da = aget(d, 2);
    val2 = (uint32_t)memread(da, 2) & 077;
    PS &= 0xFFF0;

    if (val2&040) {
      val2 = (077 ^ val2) + 1;
      if (val1&0x80000000) {
        val = 0xFFFFFFFF ^ (0xFFFFFFFF >> val2);
        val |= val1 >> val2;
      } 
      else {
        val = val1 >> val2;
      }
      if (val1&(1<<(val2-1))) {
        PS |= FLAGC;
      }
    } 
    else {
      val = (val1 << val2) & 0xFFFFFFFF;
      if (val1&(1<<(32-val2))) {
        PS |= FLAGC;
      }
    }
    R[s&7] = (val >> 16) & 0xFFFF;
    R[(s&7)|1] = val & 0xFFFF;
    if(val == 0){
      PS |= FLAGZ;
    }
    if (val&0x80000000) {
      PS |= FLAGN;
    }
    if (xor32(val&0x80000000, val1&0x80000000)) {
      PS |= FLAGV;
    }
    return;
  case 0074000: // XOR
    val1 = R[s&7];
    da = aget(d, 2);
    val2 = memread(da, 2);
    val = val1 ^ val2;
    PS &= 0xFFF1;
    if(val == 0){
      PS |= FLAGZ;
    }
    if (val&0x8000) {
      PS |= FLAGZ;
    }
    memwrite(da, 2, val);
    return;
  case 0077000: // SOB
    R[s&7]--;
    if (R[s&7]) {
      o &= 077;
      o <<= 1;
      R[7] -= o;
    }
    return;
  }
  switch (instr & 0077700) {
  case 0005000: // CLR
    PS &= 0xFFF0;
    PS |= FLAGZ;
    da = aget(d, l);
    memwrite(da, l, 0);
    return;
  case 0005100: // COM
    da = aget(d, l);
    val = memread(da, l) ^ max;
    PS &= 0xFFF0;
    PS |= FLAGC;
    if (val&msb) {
      PS |= FLAGN;
    }
    if(val == 0){
      PS |= FLAGZ;
    }
    memwrite(da, l, val);
    return;
  case 0005200: // INC
    da = aget(d, l);
    val = (memread(da, l) + 1) & max;
    PS &= 0xFFF1;
    if (val&msb) {
      PS |= FLAGN | FLAGV;
    }
    if(val == 0){
      PS |= FLAGZ;
    }
    memwrite(da, l, val);
    return;
  case 0005300: // DEC
    da = aget(d, l);
    val = (memread(da, l) - 1) & max;
    PS &= 0xFFF1;
    if (val&msb) {
      PS |= FLAGN;
    }
    if (val == maxp) {
      PS |= FLAGV;
    }
    if(val == 0){
      PS |= FLAGZ;
    }
    memwrite(da, l, val);
    return;
  case 0005400: // NEG
    da = aget(d, l);
    val = (-memread(da, l)) & max;
    PS &= 0xFFF0;
    if (val&msb) {
      PS |= FLAGN;
    }
    if(val == 0){
      PS |= FLAGZ;
    } 
    else {
      PS |= FLAGC;
    }
    if (val == 0x8000) {
      PS |= FLAGV;
    }
    memwrite(da, l, val);
    return;
  case 0005500: // ADC
    da = aget(d, l);
    val = memread(da, l);
    if (PS&FLAGC) {
      PS &= 0xFFF0;
      if ((val+1)&msb) {
        PS |= FLAGN;
      }
      if (val == max) {
        PS |= FLAGZ;
      }
      if (val == 0077777) {
        PS |= FLAGV;
      }
      if (val == 0177777) {
        PS |= FLAGC;
      }
      memwrite(da, l, (val+1)&max);
    } 
    else {
      PS &= 0xFFF0;
      if (val&msb) {
        PS |= FLAGN;
      }
      if(val == 0){
        PS |= FLAGZ;
      }
    }
    return;
  case 0005600: // SBC
    da = aget(d, l);
    val = memread(da, l);
    if (PS&FLAGC) {
      PS &= 0xFFF0;
      if ((val-1)&msb) {
        PS |= FLAGN;
      }
      if (val == 1) {
        PS |= FLAGZ;
      }
      if (val) {
        PS |= FLAGC;
      }
      if (val == 0100000) {
        PS |= FLAGV;
      }
      memwrite(da, l, (val-1)&max);
    } 
    else {
      PS &= 0xFFF0;
      if (val&msb) {
        PS |= FLAGN;
      }
      if(val == 0){
        PS |= FLAGZ;
      }
      if (val == 0100000) {
        PS |= FLAGV;
      }
      PS |= FLAGC;
    }
    return;
  case 0005700: // TST
    da = aget(d, l);
    val = memread(da, l);
    PS &= 0xFFF0;
    if (val&msb) {
      PS |= FLAGN;
    }
    if(val == 0){
      PS |= FLAGZ;
    }
    return;
  case 0006000: // ROR
    da = aget(d, l);
    val = memread(da, l);
    if (PS&FLAGC) {
      val |= max + 1;
    }
    PS &= 0xFFF0;
    if (val&1) {
      PS |= FLAGC;
    }
    if (val&(max+1)) {
      PS |= FLAGN;
    }
    if (!(val&max)) {
      PS |= FLAGZ;
    }
    if (xor16(val&1, val&(max+1))) {
      PS |= FLAGV;
    }
    val >>= 1;
    memwrite(da, l, val);
    return;
  case 0006100: // ROL
    da = aget(d, l);
    val = memread(da, l) << 1;
    if (PS&FLAGC) {
      val |= 1;
    }
    PS &= 0xFFF0;
    if (val&(max+1)) {
      PS |= FLAGC;
    }
    if (val&msb) {
      PS |= FLAGN;
    }
    if (!(val&max)) {
      PS |= FLAGZ;
    }
    if ((val^(val>>1))&msb) {
      PS |= FLAGV;
    }
    val &= max;
    memwrite(da, l, val);
    return;
  case 0006200: // ASR
    da = aget(d, l);
    val = memread(da, l);
    PS &= 0xFFF0;
    if (val&1) {
      PS |= FLAGC;
    }
    if (val&msb) {
      PS |= FLAGN
        ;		
    }
    if (xor16(val&msb, val&1)) {
      PS |= FLAGV;
    }
    val = (val & msb) | (val >> 1);
    if(val == 0){
      PS |= FLAGZ;
    }
    memwrite(da, l, val);
    return;
  case 0006300: // ASL
    da = aget(d, l);
    val = memread(da, l);
    PS &= 0xFFF0;
    if (val&msb) {
      PS |= FLAGC;
    }
    if (val&(msb>>1)) {
      PS |= FLAGN;
    }
    if ((val^(val<<1))&msb) {
      PS |= FLAGV;
    }
    val = (val << 1) & max;
    if(val == 0){
      PS |= FLAGZ;
    }
    memwrite(da, l, val);
    return;
  case 0006700: // SXT
    da = aget(d, l);
    if (PS&FLAGN) {
      memwrite(da, l, max);
    } 
    else {
      PS |= FLAGZ;
      memwrite(da, l, 0);
    }
    return;
  }
  switch (instr & 0177700) {
  case 0000100: // JMP
    val = aget(d, 2);
    if (val < 0) {
      panic(); //panic("whoa!")
      break;
    }
    R[7] = val;
    return;
  case 0000300: // SWAB
    da = aget(d, l);
    val = memread(da, l);
    val = ((val >> 8) | (val << 8)) & 0xFFFF;
    PS &= 0xFFF0;
    if (val & 0xFF) {
      PS |= FLAGZ;
    }
    if (val&0x80) {
      PS |= FLAGN;
    }
    memwrite(da, l, val);
    return;
  case 0006400: // MARK
    R[6] = R[7] + ((instr&077)<<1);
    R[7] = R[5];
    R[5] = (int32_t)pop();
    break;
  case 0006500: // MFPI
    da = aget(d, 2);
    if (da == -7) {
      // val = (curuser == prevuser) ? R[6] : (prevuser ? k.USP : KSP);
      if (curuser == prevuser) {
        val = R[6];
      } 
      else {
        if (prevuser) {
          val = USP;
        } 
        else {
          val = KSP;
        }
      }
    } 
    else if (da < 0) {
      panic();		
      //panic("invalid MFPI instruction")
    } 
    else {
      val = physread16(decode((uint16_t)da, false, prevuser));
    }
    push(val);
    PS &= 0xFFF0;
    PS |= FLAGC;
    if(val == 0){
      PS |= FLAGZ;
    }
    if (val&0x8000) {
      PS |= FLAGN;
    }
    return;
  case 0006600: // MTPI
    da = aget(d, 2);
    val = pop();
    if (da == -7) {
      if (curuser == prevuser) {
        R[6] = val;
      } 
      else {
        if (prevuser) {
          USP = val;
        } 
        else {
          KSP = val;
        }
      }
    } 
    else if (da < 0) {
      panic();//	panic("invalid MTPI instrution")
    } 
    else {
      sa = decode((uint16_t)da, true, prevuser);
      physwrite16(sa, val);
    }
    PS &= 0xFFF0;
    PS |= FLAGC;
    if(val == 0){
      PS |= FLAGZ;
    }
    if (val&0x8000) {
      PS |= FLAGN;
    }
    return;
  }
  if ((instr & 0177770) == 0000200) { // RTS
    R[7] = R[d&7];
    R[d&7] = pop();
    return;
  }
  switch (instr & 0177400) {
  case 0000400:
    branch(o);
    return;
  case 0001000:
    if (!(PS&FLAGZ)) {
      branch(o);
    }
    return;
  case 0001400:
    if (PS&FLAGZ) {
      branch(o);
    }
    return;
  case 0002000:
    if (!(xor16(PS&FLAGN, PS&FLAGV))) {
      branch(o);
    }
    return;
  case 0002400:
    if (xor16(PS&FLAGN, PS&FLAGV)) {
      branch(o);
    }
    return;
  case 0003000:
    if ((!(xor16(PS&FLAGN, PS&FLAGV))) && (!(PS&FLAGZ))) {
      branch(o);
    }
    return;
  case 0003400:
    if ((xor16(PS&FLAGN, PS&FLAGV)) || (PS&FLAGZ)) {
      branch(o);
    }
    return;
  case 0100000:
    if ((PS&FLAGN) == 0) {
      branch(o);
    }
    return;
  case 0100400:
    if (PS&FLAGN) {
      branch(o);
    }
    return;
  case 0101000:
    if ((!(PS&FLAGC)) && (!(PS&FLAGZ))) {
      branch(o);
    }
    return;
  case 0101400:
    if ((PS&FLAGC) || (PS&FLAGZ)) {
      branch(o);
    }
    return;
  case 0102000:
    if (!(PS&FLAGV)) {
      branch(o);
    }
    return;
  case 0102400:
    if (PS&FLAGV) {
      branch(o);
    }
    return;
  case 0103000:
    if (!(PS&FLAGC)) {
      branch(o);
    }
    return;
  case 0103400:
    if (PS&FLAGC) {
      branch(o);
    }
    return;
  }
  if (((instr&0177000) == 0104000) || (instr == 3) || (instr == 4)) { // EMT TRAP IOT BPT
    uint16_t vec;

    if ((instr & 0177400) == 0104000) {
      vec = 030;
    } 
    else if ((instr & 0177400) == 0104400) {
      vec = 034;
    } 
    else if (instr == 3) {
      vec = 014;
    } 
    else {
      vec = 020;
    }
    prev = PS;
    switchmode(false);
    push(prev);
    push(R[7]);
    R[7] = memory[vec>>1];
    PS = memory[(vec>>1)+1];
    if (prevuser) {
      PS |= (1 << 13) | (1 << 12);
    }
    return;
  }
  if ((instr & 0177740) == 0240) { // CL?, SE?
    if (instr&020) {
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
    Serial.print("HALT\n"); panic();
    return;
  case 0000001: // WAIT
    if (curuser) {
      break;
    }
    //println("WAIT")
    //waiting = true
    return;
  case 0000002: // RTI

  case 0000006: // RTT
    R[7] = pop();
    val = pop();
    if (curuser) {
      val &= 047;
      val |= PS & 0177730;
    }
    physwrite16(0777776, val);
    return;
  case 0000005: // RESET
    if (curuser) {
      return;
    }
    //clearterminal()
    rkreset();
    return;
  case 0170011: // SETD ; not needed by UNIX, but used; therefore ignored
    return;
  }
  //fmt.Println(ia, disasm(ia))
  //panic(trap{INTINVAL, "invalid instruction"})
  panic();
}

uint32_t decode(uint16_t a, uint8_t w, uint8_t user) {
  page p;
  uint32_t aa, block, disp;
  if (!(SR0&1)) {
    aa = (uint32_t)a;
    if (aa >= 0170000) {
      aa += 0600000;
    } 
    return aa;
  }
  if (user) {
    p = pages[(a>>13)+8];
  } 
  else {
    p = pages[(a >> 13)];
  }

  if (w && !p.write) {
    SR0 = (1 << 13) | 1;
    SR0 |= a >> 12 & ~1;
    if (user) {
      SR0 |= (1 << 5) | (1 << 6);
    }
    SR2 = PC;
    panic(); //panic(trap{INTFAULT, "write to read-only page " + ostr(a, 6)})
  }
  if (!p.read) {
    SR0 = (1 << 15) | 1;
    SR0 |= (a >> 12) & ~1;
    if (user) {
      SR0 |= (1 << 5) | (1 << 6);
    }
    SR2 = PC;
    panic(); //panic(trap{INTFAULT, "read from no-access page " + ostr(a, 6)})
  }
  block = a >> 6 & 0177;
  disp = a & 077;
  if (((p.ed && (block < p.len)) || !(p.ed && (block > p.len)))) {
    //if(p.ed ? (block < p.len) : (block > p.len)) {
    SR0 = (1 << 14) | 1;
    SR0 |= (a >> 12) & ~1;
    if (user) {
      SR0 |= (1 << 5) | (1 << 6);
    }
    SR2 = PC;
    panic(); // panic(trap{INTFAULT, "page length exceeded, address " + ostr(a, 6) + " (block " + ostr(block, 3) + ") is beyond length " + ostr(p.len, 3)})
  }
  if (w) {
    p.pdr |= 1 << 6;
  }
  return ((block+p.addr) << 6) + disp;
}

uint16_t read8(uint16_t a) {
  return physread8(decode(a, false, curuser));
}

uint16_t read16(uint16_t a) {
  return physread16(decode(a, false, curuser));
}

void write8(uint16_t a, uint16_t v) {
  physwrite8(decode(a, true, curuser), v);
}

void write16(uint16_t a, uint16_t v) {
  physwrite16(decode(a, true, curuser), v);
}

uint16_t fetch16() {
  uint16_t val;
  val = read16((uint16_t)R[7]);
  R[7] += 2;
  return val;
}

void push(uint16_t v) {
  R[6] -= 2;
  write16((uint16_t)R[6], v);
}

uint16_t pop() {
  uint16_t val;
  val = read16((uint16_t)R[6]);
  R[6] += 2;
  return val;
}

uint16_t physread16(uint32_t a) {
  if (a & 1) {
    panic(); // panic(trap{INTBUS, "read from odd address " + ostr(a, 6)})
  } 
  else if (a < 0760000 ) {
    return memory[a>>1];
  } 
  else if (a == 0777546) {
    return LKS;
  } 
  else if (a == 0777570) {
    return 0173030;
  } 
  else if (a == 0777572) {
    return SR0;
  } 
  else if (a == 0777576) {
    return SR2;
  } 
  else if (a == 0777776) {
    return PS;
  } 
  else if ((a&0777770) == 0777560) {
    return consread16(a);
  } 
  else if ((a&0777760) == 0777400) {
    return rkread16(a);
  } 
  else if (((a&0777600) == 0772200) || ((a&0777600) == 0777600)) {
    mmuread16(a);
  } 
  panic(); 
  //panic(trap{INTBUS, "read from invalid address " + ostr(a, 6)})
}

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

void switchmode(uint8_t newm) {
  prevuser = curuser;
  curuser = newm;
  if (prevuser) {
    USP = (uint16_t)R[6];
  } 
  else {
    KSP = (uint16_t)R[6];
  }
  if (curuser) {
    R[6] = (int32_t)USP;
  } 
  else {
    R[6] = (int32_t)KSP;
  }
  PS &= 0007777;
  if (curuser) {
    PS |= (1 << 15) | (1 << 14);
  }
  if (prevuser) {
    PS |= (1 << 13) | (1 << 12);
  }
}

int32_t aget(uint8_t v, uint8_t l) {
  if (((v&7) >= 6) || (v&010)) {
    l = 2;
  }
  if ((v & 070) == 000) {
    return -((int32_t)v + 1);
  }
  int32_t addr = 0;
  switch (v & 060) {
  case 000:
    v &= 7;
    addr = R[v&7];
    break;
  case 020:
    addr = R[v&7];
    R[v&7] += l;
    break;
  case 040:
    R[v&7] -= l;
    addr = R[v&7];
    break;
  case 060:
    addr = fetch16();
    addr += R[v&7];
    break;
  }
  addr &= 0xFFFF;
  if (v&010) {
    addr = read16(addr);
  }
  return addr;
}

uint16_t memread(int32_t a, uint8_t l) {
  if (a < 0) {
    a = -(a + 1);
    if (l == 2) {
      return (uint16_t)R[a&7];
    } 
    else {
      return (uint16_t)R[a&7] & 0xFF;
    }
  }
  if (l == 2) {
    return read16((uint16_t)a);
  }
  return read8((uint16_t)a);
}

void memwrite(int32_t a, uint8_t l, uint16_t v) {
  if (a < 0) {
    a = -(a + 1);
    if (l == 2) {
      R[a&7] = (int32_t)v;
    } 
    else {
      R[a&7] &= 0xFF00;
      R[a&7] |= (int32_t)v;
    }
  } 
  else if (l == 2) {
    write16((uint16_t)a, v);
  } 
  else {
    write8((uint16_t)a, v);
  }
}

void branch(int32_t o) {
  //printstate()
  if (o&0x80) {
    o = -(((~o) + 1) & 0xFF);
  }
  o <<= 1;
  R[7] += o;
}

int32_t xor32(int32_t x, int32_t y) {
  int32_t a, b, z;
  a = x & y;
  b = ~x & ~y;
  z = ~a & ~b;
  return z;
}

uint16_t xor16(uint16_t x, uint16_t y) {
  uint16_t a,b, z;
  a = x & y;
  b = ~x & ~y;
  z = ~a & ~b;
  return z;
}



/**
 * 
 * var writedebug = fmt.Print
 * 
 * 
 * 
 * type trap struct {
 * 	num int
 * 	msg string
 * }
 * 
 * func (t trap) String() string {
 * 	return fmt.Sprintf("trap %06o occured: %s", t.num, t.msg)
 * }
 * 
 * func interrupt(vec, pri int) {
 * 	var i int
 * 	if vec&1 == 1 {
 * 		panic("Thou darst calling interrupt() with an odd vector number?")
 * 	}
 * 	for ; i < len(interrupts); i++ {
 * 		if interrupts[i].pri < pri {
 * 			break
 * 		}
 * 	}
 * 	for ; i < len(interrupts); i++ {
 * 		if interrupts[i].vec >= vec {
 * 			break
 * 		}
 * 	}
 * 	// interrupts.splice(i, 0, {vec: vec, pri: pri});
 * 	interrupts = append(interrupts[:i], append([]intr{{vec, pri}}, interrupts[i:]...)...)
 * }
 * 
 * func (k *KB11) handleinterrupt(vec int) {
 * 	defer func() {
 * 		trap = recover()
 * 		switch trap = trap.(type) {
 * 		case struct {
 * 			num int
 * 			msg string
 * 		}:
 * 			k.trapat(trap.num, trap.msg)
 * 		case nil:
 * 			break
 * 		default:
 * 			panic(trap)
 * 		}
 * 		k.R[7] = int(memory[vec>>1])
 * 		PS = memory[(vec>>1)+1]
 * 		if prevuser {
 * 			PS |= (1 << 13) | (1 << 12)
 * 		}
 * 		waiting = false
 * 	}()
 * 	prev = PS
 * 	k.switchmode(false)
 * 	k.push(prev)
 * 	k.push(uint16(k.R[7]))
 * }
 * 
 * func (k *KB11) trapat(vec int, msg string) {
 * 	var prev uint16
 * 	defer func() {
 * 		t = recover()
 * 		switch t = t.(type) {
 * 		case trap:
 * 			writedebug("red stack trap!\n")
 * 			memory[0] = uint16(k.R[7])
 * 			memory[1] = prev
 * 			vec = 4
 * 			panic("fatal")
 * 		case nil:
 * 			break
 * 		default:
 * 			panic(t)
 * 		}
 * 		k.R[7] = int(memory[vec>>1])
 * 		PS = memory[(vec>>1)+1]
 * 		if prevuser {
 * 			PS |= (1 << 13) | (1 << 12)
 * 		}
 * 		waiting = false
 * 	}()
 * 	if vec&1 == 1 {
 * 		panic("Thou darst calling trapat() with an odd vector number?")
 * 	}
 * 	writedebug("trap " + ostr(vec, 6) + " occured: " + msg + "\n")
 * 	k.printstate()
 * 
 * 	prev = PS
 * 	k.switchmode(false)
 * 	k.push(prev)
 * 	k.push(uint16(k.R[7]))
 * }
 * 
 * 
 * 
 * 
 * 
 * 	k.unibus.rk.Step()
 * 	k.unibus.cons.Step(k)
 * }
 * 
 * func (k *KB11) onestep() {
 * 	defer func() {
 * 		t = recover()
 * 		switch t = t.(type) {
 * 		case trap:
 * 			k.trapat(t.num, t.msg)
 * 		case nil:
 * 			// ignore
 * 		default:
 * 			panic(t)
 * 		}
 * 	}()
 * 
 * 	k.step()
 * 	if len(interrupts) > 0 && interrupts[0].pri >= ((int(PS)>>5)&7) {
 * 		//fmt.Printf("IRQ: %06o\n", interrupts[0].vec)
 * 		k.handleinterrupt(interrupts[0].vec)
 * 		interrupts = interrupts[1:]
 * 	}
 * 	clkcounter++
 * 	if clkcounter >= 40000 {
 * 		clkcounter = 0
 * 		k.unibus.LKS |= (1 << 7)
 * 		if k.unibus.LKS&(1<<6) != 0 {
 * 			interrupt(INTCLOCK, 6)
 * 		}
 * 	}
 * }
 * 
 

 * package pdp11
 * 
 * }
 * 
 */
 

