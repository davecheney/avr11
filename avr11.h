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
  ENABLE_LKS = true,
};

#include <stdarg.h>
static void p(char *fmt, ... ){
        char tmp[128]; // resulting string limited to 128 chars
        va_list args;
        va_start (args, fmt );
        vsnprintf(tmp, 128, fmt, args);
        va_end (args);
        Serial.print(tmp);
}

void printstate();
void panic();
void disasm(uint32_t ia);

void trap(uint16_t num);


