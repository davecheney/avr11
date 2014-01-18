class page {
public:
  uint16_t par, pdr;

  uint16_t addr();
  uint16_t len();
  bool read();
  bool write();
  bool ed();
};

namespace pdp11 {

  class mmu {
    page pages[16];

  public:
    uint32_t decode(uint16_t a, uint8_t w, uint8_t user);
    uint16_t read16(int32_t a);
    void write16(int32_t a, uint16_t v);
  };

};
extern pdp11::mmu mmu;
