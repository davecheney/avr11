namespace pdp11 {

class cons {
  private:
    uint16_t TKS;
    uint16_t TKB;
    uint16_t TPS;
    uint16_t TPB;

    void addchar(char c);

  public:
    void write16(uint32_t a, uint16_t v);
    uint16_t read16(uint32_t a);
    void clearterminal();
    void poll();
};

};

extern pdp11::cons cons;
