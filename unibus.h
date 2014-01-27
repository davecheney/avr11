namespace unibus {

    void init();

    uint16_t read8(uint32_t addr);
    uint16_t read16(uint32_t addr);
    void write8(uint32_t a, uint16_t v);
    void write16(uint32_t a, uint16_t v);
};

