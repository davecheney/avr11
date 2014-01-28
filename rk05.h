namespace rk11 {

  extern SdFile rkdata;
  
void reset();
void write16(uint32_t a, uint16_t v);
uint16_t read16(uint32_t a);
void step();
};

enum {
  RKOVR = (1 << 14),
  RKNXD = (1 << 7),
  RKNXC = (1 << 6),
  RKNXS = (1 << 5)
  };


