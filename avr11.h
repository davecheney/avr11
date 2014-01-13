enum {
	FLAGN = 8,
	FLAGZ = 4,
	FLAGV = 2,
	FLAGC = 1
};

enum {
  	RKOVR = (1 << 14),
  	RKNXD = (1 << 7),
  	RKNXC = (1 << 6),
  	RKNXS = (1 << 5)
};

enum {
  MEMSIZE = 2048,
};

extern uint16_t memory[MEMSIZE];

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

uint32_t decode(uint16_t a, uint8_t w, uint8_t user);

uint16_t physread8(uint32_t addr);
uint16_t physread16(uint32_t addr);
void physwrite16(uint32_t a, uint16_t v);
void physwrite8(uint32_t a, uint16_t v);
void switchmode(uint8_t newm);
int32_t aget(uint8_t v, uint8_t l);
uint16_t memread(int32_t a, uint8_t l);
void memwrite(int32_t a, uint8_t l, uint16_t v);

void cpustep();
void cpureset(void);

void branch(int32_t o);
void printstate();

int32_t xor32(int32_t x, int32_t y);
uint16_t xor16(uint16_t x, uint16_t y);

void printstate();

void panic();

uint16_t read8(uint16_t a);

uint16_t read16(uint16_t a);

void write8(uint16_t a, uint16_t v);

void write16(uint16_t a, uint16_t v);
uint16_t fetch16();
void push(uint16_t v);
uint16_t pop();
