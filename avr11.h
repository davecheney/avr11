// interrupts
enum {
  INTBUS    = 0004,
  INTINVAL  = 0010,
  INTDEBUG  = 0014,
  INTIOT    = 0020,
  INTTTYIN  = 0060,
  INTTTYOUT = 0064,
  INTFAULT  = 0250,
  INTCLOCK  = 0100,
  INTRK     = 0220
};

enum {
  PRINTSTATE = false,
  INSTR_TIMING = true,
  DEBUG_INTER = false,
  DEBUG_RK05 = false,
  DEBUG_MMU = false,
  ENABLE_LKS = false,
};

void printstate();
void panic();
void disasm(uint32_t ia);

uint16_t trap(uint16_t num);

namespace unibus {

// operations on uint32_t types are insanely expensive
union addr {
  uint8_t  bytes[4];
  uint32_t value;
};
void init();

uint16_t read8(uint32_t addr);
uint16_t read16(uint32_t addr);
void write8(uint32_t a, uint16_t v);
void write16(uint32_t a, uint16_t v);
};

namespace cons {

void write16(uint32_t a, uint16_t v);
uint16_t read16(uint32_t a);
void clearterminal();
void poll();

};

namespace pdp11 {
struct intr {
  uint8_t vec;
  uint8_t pri;
};
};

#define ITABN 8

extern pdp11::intr itab[ITABN];

enum {
  FLAGN = 8,
  FLAGZ = 4,
  FLAGV = 2,
  FLAGC = 1
};

namespace cpu {

extern uint16_t PC;
extern uint16_t PS;
extern uint16_t USP;
extern uint16_t KSP;
extern uint16_t LKS;
extern bool curuser;
extern bool prevuser;

void step();
void reset(void);
void switchmode(bool newm);

void trapat(uint16_t vec);
void interrupt(uint8_t vec, uint8_t pri);
void handleinterrupt();

};

namespace mmu {

extern uint16_t SR0;
extern uint16_t SR2;

uint32_t decode(uint16_t a, uint8_t w, uint8_t user);
uint16_t read16(uint32_t a);
void write16(uint32_t a, uint16_t v);

};

namespace rk11 {

extern SdFile rkdata;

void reset();
void write16(uint32_t a, uint16_t v);
uint16_t read16(uint32_t a);
};

enum {
  RKOVR = (1 << 14),
  RKNXD = (1 << 7),
  RKNXC = (1 << 6),
  RKNXS = (1 << 5)
};

#ifdef __ATEMEGA2560__
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit)) 
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit)) 
#endif

enum {
  MEMSIZE = 1<<16
};
