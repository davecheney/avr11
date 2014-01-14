// this is all kinds of wrong
#include <setjmp.h>

extern int32_t R[8];

extern uint16_t PC;
extern uint16_t PS;
extern uint16_t SR0;
extern uint16_t SR2;
extern uint16_t USP;
extern uint16_t KSP;
extern uint16_t LKS;
extern uint8_t curuser;
extern uint8_t prevuser;

extern uint32_t clkcounter;

extern jmp_buf trapbuf;

enum {
  MEMSIZE = 2048,
};

extern uint16_t memory[MEMSIZE];

enum {
  FLAGN = 8,
  FLAGZ = 4,
  FLAGV = 2,
  FLAGC = 1
};

void cpustep();
void cpureset(void);
void switchmode(bool newm);
void trapat(uint16_t vec);

