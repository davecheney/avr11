struct page {
	uint16_t par, pdr;
	uint16_t addr, len;
	bool read, write, ed;
};

page createpage(uint16_t par, uint16_t pdr);

uint32_t decode(uint16_t a, bool w, bool user);

uint16_t physread8(uint32_t addr);
uint16_t physread16(uint32_t addr);
void physwrite16(uint32_t a, uint16_t v);
void physwrite8(uint32_t a, uint16_t v);
void switchmode(bool newm);
void cpustep();

void printstate();

uint16_t read8(uint16_t a);

uint16_t read16(uint16_t a);

void write8(uint16_t a, uint16_t v);

void write16(uint16_t a, uint16_t v);
uint16_t fetch16();
void push(uint16_t v);
uint16_t pop();
