namespace mmu {
  
    extern uint16_t SR0;
    extern uint16_t SR2;

    uint32_t decode(uint16_t a, bool w, bool user);
    uint16_t read16(uint32_t a);
    void write16(uint32_t a, uint16_t v);

};
