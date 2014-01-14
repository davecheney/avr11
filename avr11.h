

struct intr { 
  int32_t vec; 
  int32_t pri;  
};

// traps
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

void printstate();
void panic();

