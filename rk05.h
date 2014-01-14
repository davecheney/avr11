void rkreset();
void rkwrite16(int32_t a, uint16_t v);
int32_t rkread16(int32_t a);
void rknotready();
void rkstep();
void rkinit();

enum {
  	RKOVR = (1 << 14),
  	RKNXD = (1 << 7),
  	RKNXC = (1 << 6),
  	RKNXS = (1 << 5)
};

