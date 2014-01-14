
class page {
public:
  uint16_t par, pdr;

  uint16_t addr();
  uint16_t len();
  bool read();
  bool write();
  bool ed();
};

extern page pages[16];

page createpage(uint16_t par, uint16_t pdr);
uint16_t mmuread16(int32_t a);
void mmuwrite16(int32_t a, uint16_t v);
void mmuinit();

