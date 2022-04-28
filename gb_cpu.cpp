#include "gb_cpu.h"
#include <cmath>

namespace TKPEmu::Gameboy::Devices {
    CPU::CPU(Bus& bus, PPU& ppu, APU& apu, Timer& timer) :
        bus_(bus),
        ppu_(ppu),
        apu_(apu),
        timer_(timer),
        IF(bus_.GetReference(0xFF0F)),
        IE(bus_.GetReference(0xFFFF)),
        LY(bus_.GetReference(0xFF44)),
        STAT(bus_.GetReference(addr_sta)) {
        A = 0;
        B = 0;
        C = 0;
        D = 0;
        E = 0;
        H = 0;
        L = 0;
        F = 0;
        SP = 0;
        PC = 0;
        ime_ = true;
        TClock = 0;
        halt_ = false;
        stop_ = false;
        bus_.Write(0xFF00, 0b11011111);
    }
    void CPU::reg_dec(RegisterType& reg) {
        auto temp = reg - 1;
        auto flag = FLAG_NEG_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= (((reg & 0xF) - (1 & 0xF)) < 0) << FLAG_HCARRY_SHIFT;
        F &= FLAG_CARRY_MASK;
        F |= flag;
        reg = temp & 0xFF;
        tTemp = 4;
    }
    void CPU::reg_inc(RegisterType& reg) {
        auto temp = reg + 1;
        auto flag = FLAG_EMPTY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= (((reg & 0xF) + (1 & 0xF)) > 0xF) << FLAG_HCARRY_SHIFT;
        F &= FLAG_CARRY_MASK;
        F |= flag;
        temp &= 0xFF;
        reg = temp;
        tTemp = 4;
    }
    void CPU::reg_sub(RegisterType& reg) {
        auto temp = A - reg;
        auto flag = FLAG_NEG_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= (((A & 0xF) - (reg & 0xF)) < 0) << FLAG_HCARRY_SHIFT;
        flag |= (temp < 0) << FLAG_CARRY_SHIFT;
        F = flag;
        A = temp & 0xFF;
        tTemp = 4;
    }
    void CPU::reg_sbc(RegisterType& reg) {
        bool carry = F & FLAG_CARRY_MASK;
        auto temp = A - reg - carry;
        auto flag = FLAG_NEG_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= (((A & 0xF) - (reg & 0xF) - carry) < 0) << FLAG_HCARRY_SHIFT;
        flag |= (temp < 0) << FLAG_CARRY_SHIFT;
        F = flag;
        A = temp & 0xFF;
        tTemp = 4;
    }
    void CPU::reg_and(RegisterType& reg) {
        auto temp = A & reg;
        auto flag = FLAG_HCARRY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        F = flag;
        A = temp & 0xFF;
        tTemp = 4;
    }
    void CPU::reg_add(RegisterType& reg) {
        auto temp = A + reg;
        auto flag = FLAG_EMPTY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= (((A & 0xF) + (reg & 0xF)) > 0xF) << FLAG_HCARRY_SHIFT;
        flag |= (temp > 0xFF) << FLAG_CARRY_SHIFT;
        F = flag;
        A = temp & 0xFF;
        tTemp = 4;
    }
    void CPU::reg_adc(RegisterType& reg) {
        bool carry = F & FLAG_CARRY_MASK;
        auto temp = A + reg + carry;
        auto flag = FLAG_EMPTY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= (((A & 0xF) + (reg & 0xF) + carry) > 0xF) << FLAG_HCARRY_SHIFT;
        flag |= (temp > 0xFF) << FLAG_CARRY_SHIFT;
        F = flag;
        A = temp & 0xFF;
        tTemp = 4;
    }
    void CPU::reg_cmp(RegisterType& reg) {
        auto temp = A - reg;
        auto flag = FLAG_NEG_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= (((A & 0xF) - (reg & 0xF)) < 0) << FLAG_HCARRY_SHIFT;
        flag |= (temp < 0) << FLAG_CARRY_SHIFT;
        F = flag;
        tTemp = 4;
    }
    void CPU::reg_or(RegisterType& reg) {
        auto temp = A | reg;
        auto flag = FLAG_EMPTY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        F = flag;
        A = temp & 0xFF;
        tTemp = 4;
    }
    void CPU::reg_xor(RegisterType& reg) {
        auto temp = A ^ reg;
        auto flag = FLAG_EMPTY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        F = flag;
        A = temp & 0xFF;
        tTemp = 4;
    }
    void CPU::big_reg_inc(RegisterType& big_reg, RegisterType& small_reg) {
        ++small_reg;
        if (small_reg == 0) {
            ++big_reg;
        }
        tTemp = 8;
    }
    void CPU::big_reg_dec(RegisterType& big_reg, RegisterType& small_reg) {
        --small_reg;
        if (small_reg == 0xFF) {
            --big_reg;
        }
        tTemp = 8;
    }
    void CPU::hl_add(BigRegisterType& big_reg) {
        uint16_t t = (H << 8) | L;
        auto temp = t + big_reg;
        auto flag = FLAG_EMPTY_MASK;
        flag |= (((t & 0xFFF) +
            (big_reg & 0xFFF)) > 0xFFF) <<
            FLAG_HCARRY_SHIFT;
        flag |= (temp > 0xFFFF) << FLAG_CARRY_SHIFT;
        F &= FLAG_ZERO_MASK;
        F |= flag;
        temp &= 0xFFFF;
        H = temp >> 8;
        L = temp & 0xFF;
        tTemp = 8;
    }
    void CPU::bit_ch(RegisterType reg, unsigned shift) {
        auto temp = reg & (1 << shift);
        auto flag = FLAG_HCARRY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        F &= FLAG_CARRY_MASK;
        F |= flag;
        tTemp = 8;
    }
    void CPU::bit_res(RegisterType& reg, unsigned shift) {
        reg &= ~(1 << shift);
        tTemp = 8;
    }
    void CPU::bit_swap(RegisterType& reg) {
        auto temp = ((reg & 0xF0) >> 4) | ((reg & 0x0F) << 4);
        auto flag = FLAG_EMPTY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        F = flag;
        reg = temp & 0xFF;
        tTemp = 8;
    }
    void CPU::bit_rrc(RegisterType& reg) {
        auto temp = (reg >> 1) + ((reg & 0x1) << 7) + ((reg & 0x1) << 8);
        auto flag = FLAG_EMPTY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= (temp > 0xFF) << FLAG_CARRY_SHIFT;
        F = flag;
        reg = temp & 0xFF;
        tTemp = 8;
    }
    void CPU::bit_rl(RegisterType& reg) {
        bool carry = F & FLAG_CARRY_MASK;
        auto temp = (reg << 1) + carry;
        auto flag = FLAG_EMPTY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= (temp > 0xFF) << FLAG_CARRY_SHIFT;
        F = flag;
        reg = temp & 0xFF;
        tTemp = 8;
    }
    void CPU::bit_rr(RegisterType& reg) {
        bool carry = F & FLAG_CARRY_MASK;
        auto temp = (reg >> 1) + (carry << 7) + ((reg & 0x1) << 8);
        auto flag = FLAG_EMPTY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= (temp > 0xFF) << FLAG_CARRY_SHIFT;
        F = flag;
        reg = temp & 0xFF;
        tTemp = 8;
    }
    void CPU::bit_sl(RegisterType& reg) {
        auto temp = (reg << 1);
        auto flag = FLAG_EMPTY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= (temp > 0xFF) << FLAG_CARRY_SHIFT;
        F = flag;
        reg = temp & 0xFF;
        tTemp = 8;
    }
    void CPU::bit_sr(RegisterType& reg) {
        auto temp = ((reg >> 1) | (reg & 0x80)) + ((reg & 0x1) << 8);
        auto flag = FLAG_EMPTY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= (temp > 0xFF) << FLAG_CARRY_SHIFT;
        F = flag;
        reg = temp & 0xFF;
        tTemp = 8;
    }
    void CPU::bit_srl(RegisterType& reg) {
        auto temp = (reg >> 1) + ((reg & 0x1) << 8);
        auto flag = FLAG_EMPTY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= (temp > 0xFF) << FLAG_CARRY_SHIFT;
        F = flag;
        reg = temp & 0xFF;
        tTemp = 8;
    }
    void CPU::conditional_jump_rel(bool condition) {
        if (condition) {
            auto temp = bus_.Read(PC);
            PC += 1;
            PC += ((temp ^ 0x80) - 0x80);
            tTemp = 12;
        } else {
            PC += 1;
            tTemp = 8;
        }
    }
    void CPU::conditional_jump(bool condition) {
        if (condition) {
            delay();
            PC = bus_.ReadL(PC);
            tTemp = 16;
        } else {
            PC += 2;
            tTemp = 12;
        }
    }
    void CPU::conditional_call(bool condition) {
        if (condition) {
            SP -= 2;
            auto temp2 = bus_.Read(PC);
            auto temp = read(PC + 1) << 8;
            delay();
            write(SP + 1, (PC + 2) >> 8);
            write(SP, (PC + 2) & 0xFF);
            PC = temp + temp2;
            tTemp = 24;
        } else {
            PC += 2;
            tTemp = 12;
        }
    }
    void CPU::rst(RegisterType addr) {
        SP -= 2;
        write(SP + 1, PC >> 8);
        write(SP, PC & 0xFF);
        PC = addr;
        tTemp = 16;
    }
    void CPU::bit_set(RegisterType& reg, unsigned shift) {
        reg |= 1 << shift;
        tTemp = 8;
    }
    void CPU::ADDAA() {
        reg_add(A);
    }
    void CPU::ADDAB() {
        reg_add(B);
    }
    void CPU::ADDAC() {
        reg_add(C);
    }
    void CPU::ADDAD() {
        reg_add(D);
    }
    void CPU::ADDAE() {
        reg_add(E);
    }
    void CPU::ADDAH() {
        reg_add(H);
    }
    void CPU::ADDAL() {
        reg_add(L);
    }
    void CPU::ADDAHL() {
        uint8_t t = bus_.Read((H << 8) | L);
        reg_add(t);
        tTemp = 8;
    }
    void CPU::ADDA8() {
        uint8_t t = bus_.Read(PC++);
        reg_add(t);
        tTemp = 8;
    }
    void CPU::ADDHLBC() {
        uint16_t t = (B << 8) | C;
        hl_add(t);
    }
    void CPU::ADDHLDE() {
        uint16_t t = (D << 8) | E;
        hl_add(t);
    }
    void CPU::ADDHLHL() {
        uint16_t t = (H << 8) | L;
        hl_add(t);
    }
    void CPU::ADDHLSP() {
        hl_add(SP);
    }
    void CPU::ADDSPD() {
        int val = bus_.Read(PC);
        auto temp = SP + ((val ^ 0x80) - 0x80);
        auto flag = FLAG_EMPTY_MASK;
        flag |= (((SP & 0xF) + (val & 0xF)) > 0xF) << FLAG_HCARRY_SHIFT;
        flag |= (((SP & 0xFF) + (val & 0xFF)) > 0xFF) << FLAG_CARRY_SHIFT;
        F = flag;
        temp &= 0xFFFF;
        SP = temp;
        PC++;
        tTemp = 16;
    }
    void CPU::ADCAA() {
        reg_adc(A);
    }
    void CPU::ADCAB() {
        reg_adc(B);
    }
    void CPU::ADCAC() {
        reg_adc(C);
    }
    void CPU::ADCAD() {
        reg_adc(D);
    }
    void CPU::ADCAE() {
        reg_adc(E);
    }
    void CPU::ADCAH() {
        reg_adc(H);
    }
    void CPU::ADCAL() {
        reg_adc(L);
    }
    void CPU::ADCAHL() {
        uint8_t t = bus_.Read((H << 8) | L);
        reg_adc(t);
        tTemp = 8;
    }
    void CPU::ADCA8() {
        uint8_t t = bus_.Read(PC++);
        reg_adc(t);
        tTemp = 8;
    }
    void CPU::SUBAA() {
        reg_sub(A);
    }
    void CPU::SUBAB() {
        reg_sub(B);
    }
    void CPU::SUBAC() {
        reg_sub(C);
    }
    void CPU::SUBAD() {
        reg_sub(D);
    }
    void CPU::SUBAE() {
        reg_sub(E);
    }
    void CPU::SUBAH() {
        reg_sub(H);
    }
    void CPU::SUBAL() {
        reg_sub(L);
    }
    void CPU::SUBAHL() {
        uint8_t t = bus_.Read((H << 8) | L);
        reg_sub(t);
        tTemp = 8;
    }
    void CPU::SUBA8() {
        uint8_t t = bus_.Read(PC++);
        reg_sub(t);
        tTemp = 8;
    }
    void CPU::SBCAA() {
        reg_sbc(A);
    }
    void CPU::SBCAB() {
        reg_sbc(B);
    }
    void CPU::SBCAC() {
        reg_sbc(C);
    }
    void CPU::SBCAD() {
        reg_sbc(D);
    }
    void CPU::SBCAE() {
        reg_sbc(E);
    }
    void CPU::SBCAH() {
        reg_sbc(H);
    }
    void CPU::SBCAL() {
        reg_sbc(L);
    }
    void CPU::SBCAHL() {
        uint8_t t = bus_.Read(H << 8 | L);
        reg_sbc(t);
        tTemp = 8;
    }
    void CPU::SBCA8() {
        uint8_t t = bus_.Read(PC++);
        reg_sbc(t);
        tTemp = 8;
    }
    void CPU::CPAA() {
        reg_cmp(A);
    }
    void CPU::CPAB() {
        reg_cmp(B);
    }
    void CPU::CPAC() {
        reg_cmp(C);
    }
    void CPU::CPAD() {
        reg_cmp(D);
    }
    void CPU::CPAE() {
        reg_cmp(E);
    }
    void CPU::CPAH() {
        reg_cmp(H);
    }
    void CPU::CPAL() {
        reg_cmp(L);
    }
    void CPU::PUSHBC() {
        SP -= 2;
        write(SP + 1, B);
        write(SP, C);
        tTemp = 16;
    }
    void CPU::PUSHAF() {
        SP -= 2;
        write(SP + 1, A);
        write(SP, F);
        tTemp = 16;
    }
    void CPU::PUSHDE() {
        SP -= 2;
        write(SP + 1, D);
        write(SP, E);
        tTemp = 16;
    }
    void CPU::PUSHHL() {
        SP -= 2;
        write(SP + 1, H);
        write(SP, L);
        tTemp = 16;
    }
    void CPU::POPBC() {
        C = bus_.Read(SP);
        B = read(SP + 1);
        delay();
        SP += 2;
        tTemp = 12;
    }
    void CPU::POPAF() {
        auto temp = bus_.ReadL(SP);
        A = (temp >> 8) & 0xFF;
        F = temp & 0xF0;
        SP += 2;
        tTemp = 12;
    }
    void CPU::POPDE() {
        E = bus_.Read(SP);
        D = read(SP + 1);
        delay();
        SP += 2;
        tTemp = 12;
    }
    void CPU::POPHL() {
        L = bus_.Read(SP);
        H = read(SP + 1);
        delay();
        SP += 2;
        tTemp = 12;
    }
    void CPU::LDABC() {
        int addr = C | (B << 8);
        A = bus_.Read(addr);
        tTemp = 8;
    }
    void CPU::LDADE() {
        int addr = E | (D << 8);
        A = bus_.Read(addr);
        tTemp = 8;
    }
    void CPU::LDAA() {
        A = A;
        tTemp = 4;
    }
    void CPU::LDAB() {
        A = B;
        tTemp = 4;
    }
    void CPU::LDAC() {
        A = C;
        tTemp = 4;
    }
    void CPU::LDAD() {
        A = D;
        tTemp = 4;
    }
    void CPU::LDAE() {
        A = E;
        tTemp = 4;
    }
    void CPU::LDAH() {
        A = H;
        tTemp = 4;
    }
    void CPU::LDAL() {
        A = L;
        tTemp = 4;
    }
    void CPU::LDBA() {
        B = A;
        tTemp = 4;
    }
    void CPU::LDBB() {
        B = B;
        tTemp = 4;
    }
    void CPU::LDBC() {
        B = C;
        tTemp = 4;
    }
    void CPU::LDBD() {
        B = D;
        tTemp = 4;
    }
    void CPU::LDBE() {
        B = E;
        tTemp = 4;
    }
    void CPU::LDBH() {
        B = H;
        tTemp = 4;
    }
    void CPU::LDBL() {
        B = L;
        tTemp = 4;
    }
    void CPU::LDCA() {
        C = A;
        tTemp = 4;
    }
    void CPU::LDCB() {
        C = B;
        tTemp = 4;
    }
    void CPU::LDCC() {
        C = C;
        tTemp = 4;
    }
    void CPU::LDCD() {
        C = D;
        tTemp = 4;
    }
    void CPU::LDCE() {
        C = E;
        tTemp = 4;
    }
    void CPU::LDCH() {
        C = H;
        tTemp = 4;
    }
    void CPU::LDCL() {
        C = L;
        tTemp = 4;
    }
    void CPU::LDDA() {
        D = A;
        tTemp = 4;
    }
    void CPU::LDDB() {
        D = B;
        tTemp = 4;
    }
    void CPU::LDDC() {
        D = C;
        tTemp = 4;
    }
    void CPU::LDDD() {
        D = D;
        tTemp = 4;
    }
    void CPU::LDDE() {
        D = E;
        tTemp = 4;
    }
    void CPU::LDDH() {
        D = H;
        tTemp = 4;
    }
    void CPU::LDDL() {
        D = L;
        tTemp = 4;
    }
    void CPU::LDEA() {
        E = A;
        tTemp = 4;
    }
    void CPU::LDEB() {
        E = B;
        tTemp = 4;
    }
    void CPU::LDEC() {
        E = C;
        tTemp = 4;
    }
    void CPU::LDED() {
        E = D;
        tTemp = 4;
    }
    void CPU::LDEE() {
        E = E;
        tTemp = 4;
    }
    void CPU::LDEH() {
        E = H;
        tTemp = 4;
    }
    void CPU::LDEL() {
        E = L;
        tTemp = 4;
    }
    void CPU::LDHA() {
        H = A;
        tTemp = 4;
    }
    void CPU::LDHB() {
        H = B;
        tTemp = 4;
    }
    void CPU::LDHC() {
        H = C;
        tTemp = 4;
    }
    void CPU::LDHD() {
        H = D;
        tTemp = 4;
    }
    void CPU::LDHE() {
        H = E;
        tTemp = 4;
    }
    void CPU::LDHH() {
        H = H;
        tTemp = 4;
    }
    void CPU::LDHL() {
        H = L;
        tTemp = 4;
    }
    void CPU::LDLA() {
        L = A;
        tTemp = 4;
    }
    void CPU::LDLB() {
        L = B;
        tTemp = 4;
    }
    void CPU::LDLC() {
        L = C;
        tTemp = 4;
    }
    void CPU::LDLD() {
        L = D;
        tTemp = 4;
    }
    void CPU::LDLE() {
        L = E;
        tTemp = 4;
    }
    void CPU::LDLH() {
        L = H;
        tTemp = 4;
    }
    void CPU::LDLL() {
        L = L;
        tTemp = 4;
    }
    void CPU::LDAHL() {
        A = bus_.Read((H << 8) | L);
        tTemp = 8;
    }
    void CPU::LDBHL() {
        B = bus_.Read((H << 8) | L);
        tTemp = 8;
    }
    void CPU::LDCHL() {
        C = bus_.Read((H << 8) | L);
        tTemp = 8;
    }
    void CPU::LDDHL() {
        D = bus_.Read((H << 8) | L);
        tTemp = 8;
    }
    void CPU::LDEHL() {
        E = bus_.Read((H << 8) | L);
        tTemp = 8;
    }
    void CPU::LDHHL() {
        H = bus_.Read((H << 8) | L);
        tTemp = 8;
    }
    void CPU::LDLHL() {
        L = bus_.Read((H << 8) | L);
        tTemp = 8;
    }
    void CPU::LDHLA() {
        bus_.Write((H << 8) | L, A);
        tTemp = 8;
    }
    void CPU::LDHLB() {
        bus_.Write((H << 8) | L, B);
        tTemp = 8;
    }
    void CPU::LDHLC() {
        bus_.Write((H << 8) | L, C);
        tTemp = 8;
    }
    void CPU::LDHLD() {
        bus_.Write((H << 8) | L, D);
        tTemp = 8;
    }
    void CPU::LDHLE() {
        bus_.Write((H << 8) | L, E);
        tTemp = 8;
    }
    void CPU::LDHLH() {
        bus_.Write((H << 8) | L, H);
        tTemp = 8;
    }
    void CPU::LDHLL() {
        bus_.Write((H << 8) | L, L);
        tTemp = 8;
    }
    void CPU::LDA8() {
        A = bus_.Read(PC);
        PC++;
        tTemp = 8;
    }
    void CPU::LDB8() {
        B = bus_.Read(PC);
        PC++;
        tTemp = 8;
    }
    void CPU::LDC8() {
        C = bus_.Read(PC);
        PC++;
        tTemp = 8;
    }
    void CPU::LDD8() {
        D = bus_.Read(PC);
        PC++;
        tTemp = 8;
    }
    void CPU::LDE8() {
        E = bus_.Read(PC);
        PC++;
        tTemp = 8;
    }
    void CPU::LDH8() {
        H = bus_.Read(PC);
        PC++;
        tTemp = 8;
    }
    void CPU::LDL8() {
        L = bus_.Read(PC);
        PC++;
        tTemp = 8;
    }
    void CPU::LDHL8() {
        write((H << 8) | L, bus_.Read(PC));
        PC++;
        tTemp = 12;
    }
    void CPU::LDBCA() {
        bus_.Write((B << 8) | C, A);
        tTemp = 8;
    }
    void CPU::LDDEA() {
        bus_.Write((D << 8) | E, A);
        tTemp = 8;
    }
    void CPU::LD16A() {
        delay();
        write(bus_.ReadL(PC), A);
        PC += 2;
        tTemp = 16;
    }
    void CPU::LDA16() {
        delay();
        A = read(bus_.ReadL(PC));
        PC += 2;
        tTemp = 16;
    }
    void CPU::LDBC16() {
        C = bus_.Read(PC);
        B = bus_.Read(PC + 1);
        PC += 2;
        tTemp = 12;
    }
    void CPU::LDDE16() {
        E = bus_.Read(PC);
        D = bus_.Read(PC + 1);
        PC += 2;
        tTemp = 12;
    }
    void CPU::LDHL16() {
        L = bus_.Read(PC);
        H = bus_.Read(PC + 1);
        PC += 2;
        tTemp = 12;
    }
    void CPU::LD16SP() {
        bus_.Write(bus_.ReadL(PC), SP & 0xFF);
        bus_.Write(bus_.ReadL(PC) + 1, (SP >> 8) & 0xFF);
        PC += 2;
        tTemp = 20;
    }
    void CPU::INCA() {
        reg_inc(A);
    }
    void CPU::INCB() {
        reg_inc(B);
    }
    void CPU::INCC() {
        reg_inc(C);
    }
    void CPU::INCD() {
        reg_inc(D);
    }
    void CPU::INCE() {
        reg_inc(E);
    }
    void CPU::INCH() {
        reg_inc(H);
    }
    void CPU::INCL() {
        reg_inc(L);
    }
    void CPU::INCHLR() {
        uint8_t t = bus_.Read((H << 8) | L);
        reg_inc(t);
        write((H << 8) | L, t);
        tTemp = 12;
    }
    void CPU::DECA() {
        reg_dec(A);
    }
    void CPU::DECB() {
        reg_dec(B);
    }
    void CPU::DECC() {
        reg_dec(C);
    }
    void CPU::DECD() {
        reg_dec(D);
    }
    void CPU::DECE() {
        reg_dec(E);
    }
    void CPU::DECH() {
        reg_dec(H);
    }
    void CPU::DECL() {
        reg_dec(L);
    }
    void CPU::DECHLR() {
        uint8_t t = bus_.Read((H << 8) | L);
        reg_dec(t);
        write((H << 8) | L, t);
        tTemp = 12;
    }
    void CPU::INCBC() {
        big_reg_inc(B, C);
    }
    void CPU::INCDE() {
        big_reg_inc(D, E);
    }
    void CPU::INCHL() {
        big_reg_inc(H, L);
    }
    void CPU::INCSP() {
        ++SP;
        tTemp = 8;
    }
    void CPU::DECBC() {
        big_reg_dec(B, C);
    }
    void CPU::DECDE() {
        big_reg_dec(D, E);
    }
    void CPU::DECHL() {
        big_reg_dec(H, L);
    }
    void CPU::DECSP() {
        --SP;
        tTemp = 8;
    }
    void CPU::JPHL() {
        PC = (H << 8) | L;
        tTemp = 4;
    }
    void CPU::JP16() {
        conditional_jump(true);
    }
    void CPU::JPNZ16() {
        conditional_jump(!(F & FLAG_ZERO_MASK));
    }
    void CPU::JPZ16() {
        conditional_jump(F & FLAG_ZERO_MASK);
    }
    void CPU::JPNC16() {
        conditional_jump(!(F & FLAG_CARRY_MASK));
    }
    void CPU::JPC16() {
        conditional_jump(F & FLAG_CARRY_MASK);
    }
    void CPU::JR8() {
        conditional_jump_rel(true);
    }
    void CPU::JRNZ8() {
        conditional_jump_rel(!(F & FLAG_ZERO_MASK));
    }
    void CPU::JRZ8() {
        conditional_jump_rel(F & FLAG_ZERO_MASK);
    }
    void CPU::JRNC8() {
        conditional_jump_rel(!(F & FLAG_CARRY_MASK));
    }
    void CPU::JRC8() {
        conditional_jump_rel(F & FLAG_CARRY_MASK);
    }
    void CPU::ANDA() {
        reg_and(A);
    }
    void CPU::ANDB() {
        reg_and(B);
    }
    void CPU::ANDC() {
        reg_and(C);
    }
    void CPU::ANDD() {
        reg_and(D);
    }
    void CPU::ANDE() {
        reg_and(E);
    }
    void CPU::ANDH() {
        reg_and(H);
    }
    void CPU::ANDL() {
        reg_and(L);
    }
    void CPU::ANDHL() {
        uint8_t t = bus_.Read((H << 8) | L);
        reg_and(t);
        tTemp = 8;
    }
    void CPU::AND8() {
        uint8_t t = bus_.Read(PC++);
        reg_and(t);
        tTemp = 8;
    }
    void CPU::ORA() {
        reg_or(A);
    }
    void CPU::ORB() {
        reg_or(B);
    }
    void CPU::ORC() {
        reg_or(C);
    }
    void CPU::ORD() {
        reg_or(D);
    }
    void CPU::ORE() {
        reg_or(E);
    }
    void CPU::ORH() {
        reg_or(H);
    }
    void CPU::ORL() {
        reg_or(L);
    }
    void CPU::ORHL() {
        uint8_t t = bus_.Read((H << 8) | L);
        reg_or(t);
        tTemp = 8;
    }
    void CPU::OR8() {
        uint8_t t = bus_.Read(PC++);
        reg_or(t);
        tTemp = 8;
    }
    void CPU::XORA() {
        reg_xor(A);
    }
    void CPU::XORB() {
        reg_xor(B);
    }
    void CPU::XORC() {
        reg_xor(C);
    }
    void CPU::XORD() {
        reg_xor(D);
    }
    void CPU::XORE() {
        reg_xor(E);
    }
    void CPU::XORH() {
        reg_xor(H);
    }
    void CPU::XORL() {
        reg_xor(L);
    }
    void CPU::XORHL() {
        uint8_t t = bus_.Read((H << 8) | L);
        reg_xor(t);
        tTemp = 8;
    }
    void CPU::XOR8() {
        uint8_t t = bus_.Read(PC++);
        reg_xor(t);
        tTemp = 8;
    }
    void CPU::NOP() {
        tTemp = 4;
    }
    void CPU::STOP() {
        PC++;
    }
    void CPU::RET() {
        delay();
        PC = bus_.ReadL(SP);
        SP += 2;
        tTemp = 16;
    }
    void CPU::RETI() {
        ime_ = true;
        delay();
        PC = bus_.ReadL(SP);
        SP += 2;
        tTemp = 16;
    }
    void CPU::RETNZ() {
        tTemp = 8;
        if ((F & 0x80) == 0x00) {
            delay();
            delay();
            PC = bus_.ReadL(SP);
            SP += 2;
            tTemp += 12;
        }
    }
    void CPU::RETZ() {
        tTemp = 8;
        if ((F & 0x80) == 0x80) {
            delay();
            delay();
            PC = bus_.ReadL(SP);
            SP += 2;
            tTemp += 12;
        }
    }
    void CPU::RETNC() {
        tTemp = 8;
        if ((F & 0x10) == 0x00) {
            delay();
            delay();
            PC = bus_.ReadL(SP);
            SP += 2;
            tTemp += 12;
        }
    }
    void CPU::RETC() {
        tTemp = 8;
        if ((F & 0x10) == 0x10) {
            delay();
            delay();
            PC = bus_.ReadL(SP);
            SP += 2;
            tTemp += 12;
        }
    }
    void CPU::RST0() {
        rst(0x00);
    }
    void CPU::RST8() {
        rst(0x08);
    }
    void CPU::RST10() {
        rst(0x10);
    }
    void CPU::RST18() {
        rst(0x18);
    }
    void CPU::RST20() {
        rst(0x20);
    }
    void CPU::RST28() {
        rst(0x28);
    }
    void CPU::RST30() {
        rst(0x30);
    }
    void CPU::RST38() {
        rst(0x38);
    }
    void CPU::LDSPHL() {
        SP = (H << 8) | L;
        tTemp = 8;
    }
    void CPU::LDAMC() {
        A = bus_.Read(0xFF00 + C);
        tTemp = 8;
    }
    void CPU::DI() {
        ime_ = false;
        ime_scheduled_ = false;
        tTemp = 4;
    }
    void CPU::EI() {
        ime_scheduled_ = true;
        tTemp = 4;
    }
    void CPU::RLA() {
        bool carry = F & FLAG_CARRY_MASK;
        auto temp = (A << 1) + carry;
        auto flag = FLAG_EMPTY_MASK;
        flag |= (temp > 0xFF) << FLAG_CARRY_SHIFT;
        F = flag;
        A = temp & 0xFF;
        tTemp = 4;
    }
    void CPU::RLCA() {
        auto temp = (A << 1) + (A >> 7);
        auto flag = FLAG_EMPTY_MASK;
        flag |= (temp > 0xFF) << FLAG_CARRY_SHIFT;
        F = flag;
        A = temp & 0xFF;
        tTemp = 4;
    }
    void CPU::RRA() {
        bool carry = F & FLAG_CARRY_MASK;
        auto temp = (A >> 1) + ((carry) << 7) + ((A & 1) << 8);
        auto flag = FLAG_EMPTY_MASK;
        flag |= (temp > 0xFF) << FLAG_CARRY_SHIFT;
        F = flag;
        temp &= 0xFF;
        A = temp;
        tTemp = 4;
    }
    void CPU::RRCA() {
        auto temp = (A >> 1) + ((A & 1) << 7) + ((A & 1) << 8);
        auto flag = FLAG_EMPTY_MASK;
        flag += (temp > 0xFF) << FLAG_CARRY_SHIFT;
        F = flag;
        temp &= 0xFF;
        A = temp;
        tTemp = 4;
    }
    void CPU::CALL16() {
        SP -= 2;
        auto temp2 = bus_.Read(PC);
        auto temp = read(PC + 1) << 8;
        delay();
        write(SP + 1, (PC + 2) >> 8);
        write(SP, (PC + 2) & 0xFF);
        PC = temp + temp2;
        tTemp = 24;
    }
    void CPU::CALLNZ16() {
        conditional_call(!(F & FLAG_ZERO_MASK));
    }
    void CPU::CALLZ16() {
        conditional_call(F & FLAG_ZERO_MASK);
    }
    void CPU::CALLNC16() {
        conditional_call(!(F & FLAG_CARRY_MASK));
    }
    void CPU::CALLC16() {
        conditional_call(F & FLAG_CARRY_MASK);
    }
    void CPU::LDSP16() {
        SP = bus_.ReadL(PC);
        PC += 2;
        tTemp = 12;
    }
    void CPU::LDDHLA() {
        bus_.Write((H << 8) | L, A);
        L = (L - 1) & 0xFF;
        if (L == 0xFF) {
            H = (H - 1) & 0xFF;
        }
        tTemp = 8;
    }
    void CPU::LDDAHL() {
        A = bus_.Read((H << 8) | L);
        L = (L - 1) & 0xFF;
        if (L == 0xFF) {
            H = (H - 1) & 0xFF;
        }
        tTemp = 8;
    }
    void CPU::LDIHLA() {
        bus_.Write((H << 8) | L, A);
        L = (L + 1) & 0xFF;
        if (!L) {
            H = (H + 1) & 0xFF;
        }
        tTemp = 8;
    }
    void CPU::LDIAHL() {
        A = bus_.Read((H << 8) | L);
        L = (L + 1) & 0xFF;
        if (!L) {
            H = (H + 1) & 0xFF;
        }
        tTemp = 8;
    }
    void CPU::DAA() {
        int temp = A;
        uint16_t corr = 0;
        corr |= ((F & FLAG_HCARRY_MASK) ? 0x06 : 0x00);
        corr |= ((F & FLAG_CARRY_MASK) ? 0x60 : 0x00);
        if (F & FLAG_NEG_MASK) {
            temp -= corr;
        } else {
            corr |= ((temp & 0x0F) > 0x09) ? 0x06 : 0;
            corr |= (temp > 0x99) ? 0x60 : 0;
            temp += corr;
        }
        uint16_t flag = FLAG_EMPTY_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= ((corr & 0x60) != 0) << FLAG_CARRY_SHIFT;
        F &= FLAG_NEG_MASK;
        F |= flag;
        temp &= 0xFF;
        A = temp;
        tTemp = 4;
    }
    void CPU::CPL() {
        A = (~A) & 0xFF;
        F &= 0b1001'0000;
        F |= 0b0110'0000;
        tTemp = 4;
    }
    void CPU::SCF() {
        F &= 0b1000'0000;
        F |= 0b0001'0000;
        tTemp = 4;
    }
    void CPU::CCF() {
        auto flag = (F & 0b0001'0000) ^ 0b0001'0000;
        F &= 0b1000'0000;
        F |= flag;
        tTemp = 4;
    }
    void CPU::LDHLSPD() {
        auto val = bus_.Read(PC);
        auto HL = SP + ((val ^ 0x80) - 0x80);
        H = (HL >> 8) & 0xFF;
        L = HL & 0xFF;
        auto flag = FLAG_EMPTY_MASK;
        flag |=    (((SP & 0xF) + (val & 0xF)) > 0xF) << FLAG_HCARRY_SHIFT;
        flag |= (((SP & 0xFF) + (val & 0xFF)) > 0xFF) << FLAG_CARRY_SHIFT;
        F = flag;
        PC++;
        tTemp = 12;
    }
    void CPU::LDHA8() {
        A = read(0xFF00 + bus_.Read(PC));
        PC++;
        tTemp = 12;
    }
    void CPU::LDH8A() {
        write(0xFF00 + bus_.Read(PC), A);
        delay();
        PC++;
        tTemp = 12;
    }
    void CPU::LDHCA() {
        bus_.Write(0xFF00 + C, A);
        tTemp = 8;
    }
    void CPU::EXT() {
        int i = bus_.Read(PC);
        PC++;
        PC &= 0xFFFF;
        if (i <= 0xFF) {
            (this->*CBInstructions[i].op)();
        }
    }
    void CPU::CPAHL() {
        uint8_t t = bus_.Read((H << 8) | L);
        reg_cmp(t);
        tTemp = 8;
    }
    void CPU::CP8() {
        uint16_t m = bus_.Read(PC);
        int temp = A - m;
        auto flag = FLAG_NEG_MASK;
        flag |= ((temp & 0xFF) == 0) << FLAG_ZERO_SHIFT;
        flag |= (((A & 0x0F) - (m & 0x0F)) < 0) << FLAG_HCARRY_SHIFT;
        flag |= (temp < 0) << FLAG_CARRY_SHIFT;
        F = flag;
        PC++;
        tTemp = 8;
    }
    void CPU::HALT() {
        halt_ = true;
        if ((IE & IF & 0x1F) && ime_ == 0) {
            halt_bug_ = true;
            halt_ = false;
        }
        tTemp = 4;
    }
    void CPU::XXX() {
        stop_ = true;
    }
    void CPU::RLCB() {
        int i = B & 0x80 ? 1 : 0;
        int o = B & 0x80 ? 0x10 : 0;
        B = (B << 1) + i;
        B &= 0xFF;
        F = B ? 0 : 0x80;
        F = (F & 0xEF) + o;
        tTemp = 8;
    }
    void CPU::RLCC() {
        int i = C & 0x80 ? 1 : 0;
        int o = C & 0x80 ? 0x10 : 0;
        C = (C << 1) + i;
        C &= 0xFF;
        F = C ? 0 : 0x80;
        F = (F & 0xEF) + o;
        tTemp = 8;
    }
    void CPU::RLCD() {
        int i = D & 0x80 ? 1 : 0;
        int o = D & 0x80 ? 0x10 : 0;
        D = (D << 1) + i;
        D &= 0xFF;
        F = D ? 0 : 0x80;
        F = (F & 0xEF) + o;
        tTemp = 8;
    }
    void CPU::RLCE() {
        int i = E & 0x80 ? 1 : 0;
        int o = E & 0x80 ? 0x10 : 0;
        E = (E << 1) + i;
        E &= 0xFF;
        F = E ? 0 : 0x80;
        F = (F & 0xEF) + o;
        tTemp = 8;
    }
    void CPU::RLCH() {
        int i = H & 0x80 ? 1 : 0;
        int o = H & 0x80 ? 0x10 : 0;
        H = (H << 1) + i;
        H &= 0xFF;
        F = H ? 0 : 0x80;
        F = (F & 0xEF) + o;
        tTemp = 8;
    }
    void CPU::RLCL() {
        int i = L & 0x80 ? 1 : 0;
        int o = L & 0x80 ? 0x10 : 0;
        L = (L << 1) + i;
        L &= 0xFF;
        F = L ? 0 : 0x80;
        F = (F & 0xEF) + o;
        tTemp = 8;
    }
    void CPU::RLCAr() {
        int i = A & 0x80 ? 1 : 0;
        int o = A & 0x80 ? 0x10 : 0;
        A = (A << 1) + i;
        A &= 0xFF;
        F = A ? 0 : 0x80;
        F = (F & 0xEF) + o;
        tTemp = 8;
    }
    void CPU::RLCHL() {
        int i = read((H << 8) | L);
        int ci = (i & 0x80) ? 1 : 0;
        int co = (i & 0x80) ? 0x10 : 0;
        i = (i << 1) + ci;
        i &= 0xFF;
        F = (i) ? 0 : 0x80;
        write((H << 8) | L, i);
        F = (F & 0xEF) + co;
        tTemp = 16;
    }
    void CPU::RRCB() {
        bit_rrc(B);
    }
    void CPU::RRCC() {
        bit_rrc(C);
    }
    void CPU::RRCD() {
        bit_rrc(D);
    }
    void CPU::RRCE() {
        bit_rrc(E);
    }
    void CPU::RRCH() {
        bit_rrc(H);
    }
    void CPU::RRCL() {
        bit_rrc(L);
    }
    void CPU::RRCAr() {
        bit_rrc(A);
    }
    void CPU::RRCHL() {
        uint8_t t = read((H << 8) | L);
        bit_rrc(t);
        write((H << 8) | L, t);
        tTemp = 16;
    }
    void CPU::RLB() {
        bit_rl(B);
    }
    void CPU::RLC() {
        bit_rl(C);
    }
    void CPU::RLD() {
        bit_rl(D);
    }
    void CPU::RLE() {
        bit_rl(E);
    }
    void CPU::RLH() {
        bit_rl(H);
    }
    void CPU::RLL() {
        bit_rl(L);
    }
    void CPU::RLAr() {
        bit_rl(A);
    }
    void CPU::RLHL() {
        uint8_t t = read((H << 8) | L);
        bit_rl(t);
        write((H << 8) | L, t);
        tTemp = 16;
    }
    void CPU::RRB() {
        bit_rr(B);
    }
    void CPU::RRC() {
        bit_rr(C);
    }
    void CPU::RRD() {
        bit_rr(D);
    }
    void CPU::RRE() {
        bit_rr(E);
    }
    void CPU::RRH() {
        bit_rr(H);
    }
    void CPU::RRL() {
        bit_rr(L);
    }
    void CPU::RRAr() {
        bit_rr(A);
    }
    void CPU::RRHL() {
        uint8_t t = read((H << 8) | L);
        bit_rr(t);
        write((H << 8) | L, t);
        tTemp = 16;
    }
    void CPU::SLAB() {
        bit_sl(B);
    }
    void CPU::SLAC() {
        bit_sl(C);
    }
    void CPU::SLAD() {
        bit_sl(D);
    }
    void CPU::SLAE() {
        bit_sl(E);
    }
    void CPU::SLAH() {
        bit_sl(H);
    }
    void CPU::SLAL() {
        bit_sl(L);
    }
    void CPU::SLAHL() {
        uint8_t t = read((H << 8) | L);
        bit_sl(t);
        write((H << 8) | L, t);
        tTemp = 16;
    }
    void CPU::SLAA() {
        bit_sl(A);
    }
    void CPU::SRAB() {
        bit_sr(B);
    }
    void CPU::SRAC() {
        bit_sr(C);
    }
    void CPU::SRAD() {
        bit_sr(D);
    }
    void CPU::SRAE() {
        bit_sr(E);
    }
    void CPU::SRAH() {
        bit_sr(H);
    }
    void CPU::SRAL() {
        bit_sr(L);
    }
    void CPU::SRAHL() {
        uint8_t t = read((H << 8) | L);
        bit_sr(t);
        write((H << 8) | L, t);
        tTemp = 16;
    }
    void CPU::SRAA() {
        bit_sr(A);
    }
    void CPU::SWAPB() {
        bit_swap(B);
    }
    void CPU::SWAPC() {
        bit_swap(C);
    }
    void CPU::SWAPD() {
        bit_swap(D);
    }
    void CPU::SWAPE() {
        bit_swap(E);
    }
    void CPU::SWAPH() {
        bit_swap(H);
    }
    void CPU::SWAPL() {
        bit_swap(L);
    }
    void CPU::SWAPHL() {
        uint8_t t = read((H << 8) | L);
        bit_swap(t);
        write((H << 8) | L, t);
        tTemp = 16;
    }
    void CPU::SWAPA() {
        bit_swap(A);
    }
    void CPU::SRLB() {
        bit_srl(B);
    }
    void CPU::SRLC() {
        bit_srl(C);
    }
    void CPU::SRLD() {
        bit_srl(D);
    }
    void CPU::SRLE() {
        bit_srl(E);
    }
    void CPU::SRLH() {
        bit_srl(H);
    }
    void CPU::SRLL() {
        bit_srl(L);
    }
    void CPU::SRLHL() {
        uint8_t t = read((H << 8) | L);
        bit_srl(t);
        write((H << 8) | L, t);
        tTemp = 16;
    }
    void CPU::SRLA() {
        bit_srl(A);
    }
    void CPU::BIT0B() {
        bit_ch(B, 0);
    }
    void CPU::BIT0C() {
        bit_ch(C, 0);
    }
    void CPU::BIT0D() {
        bit_ch(D, 0);
    }
    void CPU::BIT0E() {
        bit_ch(E, 0);
    }
    void CPU::BIT0H() {
        bit_ch(H, 0);
    }
    void CPU::BIT0L() {
        bit_ch(L, 0);
    }
    void CPU::BIT0A() {
        bit_ch(A, 0);
    }
    void CPU::BIT0M() {
        bit_ch(read((H << 8) | L), 0);
        tTemp = 12;
    }
    void CPU::BIT1B() {
        bit_ch(B, 1);
    }
    void CPU::BIT1C() {
        bit_ch(C, 1);
    }
    void CPU::BIT1D() {
        bit_ch(D, 1);
    }
    void CPU::BIT1E() {
        bit_ch(E, 1);
    }
    void CPU::BIT1H() {
        bit_ch(H, 1);
    }
    void CPU::BIT1L() {
        bit_ch(L, 1);
    }
    void CPU::BIT1A() {
        bit_ch(A, 1);
    }
    void CPU::BIT1M() {
        bit_ch(read((H << 8) | L), 1);
        tTemp = 12;
    }
    void CPU::BIT2B() {
        bit_ch(B, 2);
    }
    void CPU::BIT2C() {
        bit_ch(C, 2);
    }
    void CPU::BIT2D() {
        bit_ch(D, 2);
    }
    void CPU::BIT2E() {
        bit_ch(E, 2);
    }
    void CPU::BIT2H() {
        bit_ch(H, 2);
    }
    void CPU::BIT2L() {
        bit_ch(L, 2);
    }
    void CPU::BIT2A() {
        bit_ch(A, 2);
    }
    void CPU::BIT2M() {
        bit_ch(read((H << 8) | L), 2);
        tTemp = 12;
    }
    void CPU::BIT3B() {
        bit_ch(B, 3);
    }
    void CPU::BIT3C() {
        bit_ch(C, 3);
    }
    void CPU::BIT3D() {
        bit_ch(D, 3);
    }
    void CPU::BIT3E() {
        bit_ch(E, 3);
    }
    void CPU::BIT3H() {
        bit_ch(H, 3);
    }
    void CPU::BIT3L() {
        bit_ch(L, 3);
    }
    void CPU::BIT3A() {
        bit_ch(A, 3);
    }
    void CPU::BIT3M() {
        bit_ch(read((H << 8) | L), 3);
        tTemp = 12;
    }
    void CPU::BIT4B() {
        bit_ch(B, 4);
    }
    void CPU::BIT4C() {
        bit_ch(C, 4);
    }
    void CPU::BIT4D() {
        bit_ch(D, 4);
    }
    void CPU::BIT4E() {
        bit_ch(E, 4);
    }
    void CPU::BIT4H() {
        bit_ch(H, 4);
    }
    void CPU::BIT4L() {
        bit_ch(L, 4);
    }
    void CPU::BIT4A() {
        bit_ch(A, 4);
    }
    void CPU::BIT4M() {
        bit_ch(read((H << 8) | L), 4);
        tTemp = 12;
    }
    void CPU::BIT5B() {
        bit_ch(B, 5);
    }
    void CPU::BIT5C() {
        bit_ch(C, 5);
    }
    void CPU::BIT5D() {
        bit_ch(D, 5);
    }
    void CPU::BIT5E() {
        bit_ch(E, 5);
    }
    void CPU::BIT5H() {
        bit_ch(H, 5);
    }
    void CPU::BIT5L() {
        bit_ch(L, 5);
    }
    void CPU::BIT5A() {
        bit_ch(A, 5);
    }
    void CPU::BIT5M() {
        bit_ch(read((H << 8) | L), 5);
        tTemp = 12;
    }
    void CPU::BIT6B() {
        bit_ch(B, 6);
    }
    void CPU::BIT6C() {
        bit_ch(C, 6);
    }
    void CPU::BIT6D() {
        bit_ch(D, 6);
    }
    void CPU::BIT6E() {
        bit_ch(E, 6);
    }
    void CPU::BIT6H() {
        bit_ch(H, 6);
    }
    void CPU::BIT6L() {
        bit_ch(L, 6);
    }
    void CPU::BIT6A() {
        bit_ch(A, 6);
    }
    void CPU::BIT6M() {
        bit_ch(read((H << 8) | L), 6);
        tTemp = 12;
    }
    void CPU::BIT7B() {
        bit_ch(B, 7);
    }
    void CPU::BIT7C() {
        bit_ch(C, 7);
    }
    void CPU::BIT7D() {
        bit_ch(D, 7);
    }
    void CPU::BIT7E() {
        bit_ch(E, 7);
    }
    void CPU::BIT7H() {
        bit_ch(H, 7);
    }
    void CPU::BIT7L() {
        bit_ch(L, 7);
    }
    void CPU::BIT7A() {
        bit_ch(A, 7);
    }
    void CPU::BIT7M() {
        bit_ch(read((H << 8) | L), 7);
        tTemp = 12;
    }
    void CPU::RES0B() {
        bit_res(B, 0);
    }
    void CPU::RES0C() {
        bit_res(C, 0);
    }
    void CPU::RES0D() {
        bit_res(D, 0);
    }
    void CPU::RES0E() {
        bit_res(E, 0);
    }
    void CPU::RES0H() {
        bit_res(H, 0);
    }
    void CPU::RES0L() {
        bit_res(L, 0);
    }
    void CPU::RES0HL() {
        auto t = read((H << 8) | L);
        bit_res(t, 0);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::RES0A() {
        bit_res(A, 0);
    }
    void CPU::RES1B() {
        bit_res(B, 1);
    }
    void CPU::RES1C() {
        bit_res(C, 1);
    }
    void CPU::RES1D() {
        bit_res(D, 1);
    }
    void CPU::RES1E() {
        bit_res(E, 1);
    }
    void CPU::RES1H() {
        bit_res(H, 1);
    }
    void CPU::RES1L() {
        bit_res(L, 1);
    }
    void CPU::RES1HL() {
        auto t = read((H << 8) | L);
        bit_res(t, 1);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::RES1A() {
        bit_res(A, 1);
    }
    void CPU::RES2B() {
        bit_res(B, 2);
    }
    void CPU::RES2C() {
        bit_res(C, 2);
    }
    void CPU::RES2D() {
        bit_res(D, 2);
    }
    void CPU::RES2E() {
        bit_res(E, 2);
    }
    void CPU::RES2H() {
        bit_res(H, 2);
    }
    void CPU::RES2L() {
        bit_res(L, 2);
    }
    void CPU::RES2HL() {
        auto t = read((H << 8) | L);
        bit_res(t, 2);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::RES2A() {
        bit_res(A, 2);
    }
    void CPU::RES3B() {
        bit_res(B, 3);
    }
    void CPU::RES3C() {
        bit_res(C, 3);
    }
    void CPU::RES3D() {
        bit_res(D, 3);
    }
    void CPU::RES3E() {
        bit_res(E, 3);
    }
    void CPU::RES3H() {
        bit_res(H, 3);
    }
    void CPU::RES3L() {
        bit_res(L, 3);
    }
    void CPU::RES3HL() {
        auto t = read((H << 8) | L);
        bit_res(t, 3);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::RES3A() {
        bit_res(A, 3);
    }
    void CPU::RES4B() {
        bit_res(B, 4);
    }
    void CPU::RES4C() {
        bit_res(C, 4);
    }
    void CPU::RES4D() {
        bit_res(D, 4);
    }
    void CPU::RES4E() {
        bit_res(E, 4);
    }
    void CPU::RES4H() {
        bit_res(H, 4);
    }
    void CPU::RES4L() {
        bit_res(L, 4);
    }
    void CPU::RES4HL() {
        auto t = read((H << 8) | L);
        bit_res(t, 4);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::RES4A() {
        bit_res(A, 4);
    }
    void CPU::RES5B() {
        bit_res(B, 5);
    }
    void CPU::RES5C() {
        bit_res(C, 5);
    }
    void CPU::RES5D() {
        bit_res(D, 5);
    }
    void CPU::RES5E() {
        bit_res(E, 5);
    }
    void CPU::RES5H() {
        bit_res(H, 5);
    }
    void CPU::RES5L() {
        bit_res(L, 5);
    }
    void CPU::RES5HL() {
        auto t = read((H << 8) | L);
        bit_res(t, 5);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::RES5A() {
        bit_res(A, 5);
    }
    void CPU::RES6B() {
        bit_res(B, 6);
    }
    void CPU::RES6C() {
        bit_res(C, 6);
    }
    void CPU::RES6D() {
        bit_res(D, 6);
    }
    void CPU::RES6E() {
        bit_res(E, 6);
    }
    void CPU::RES6H() {
        bit_res(H, 6);
    }
    void CPU::RES6L() {
        bit_res(L, 6);
    }
    void CPU::RES6HL() {
        auto t = read((H << 8) | L);
        bit_res(t, 6);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::RES6A() {
        bit_res(A, 6);
    }
    void CPU::RES7B() {
        bit_res(B, 7);
    }
    void CPU::RES7C() {
        bit_res(C, 7);
    }
    void CPU::RES7D() {
        bit_res(D, 7);
    }
    void CPU::RES7E() {
        bit_res(E, 7);
    }
    void CPU::RES7H() {
        bit_res(H, 7);
    }
    void CPU::RES7L() {
        bit_res(L, 7);
    }
    void CPU::RES7HL() {
        auto t = read((H << 8) | L);
        bit_res(t, 7);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::RES7A() {
        bit_res(A, 7);
    }
    void CPU::SET0B() {
        bit_set(B, 0);
    }
    void CPU::SET0C() {
        bit_set(C, 0);
    }
    void CPU::SET0D() {
        bit_set(D, 0);
    }
    void CPU::SET0E() {
        bit_set(E, 0);
    }
    void CPU::SET0H() {
        bit_set(H, 0);
    }
    void CPU::SET0L() {
        bit_set(L, 0);
    }
    void CPU::SET0HL() {
        auto t = read((H << 8) | L);
        bit_set(t, 0);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::SET0A() {
        bit_set(A, 0);
    }
    void CPU::SET1B() {
        bit_set(B, 1);
    }
    void CPU::SET1C() {
        bit_set(C, 1);
    }
    void CPU::SET1D() {
        bit_set(D, 1);
    }
    void CPU::SET1E() {
        bit_set(E, 1);
    }
    void CPU::SET1H() {
        bit_set(H, 1);
    }
    void CPU::SET1L() {
        bit_set(L, 1);
    }
    void CPU::SET1HL() {
        auto t = read((H << 8) | L);
        bit_set(t, 1);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::SET1A() {
        bit_set(A, 1);
    }
    void CPU::SET2B() {
        bit_set(B, 2);
    }
    void CPU::SET2C() {
        bit_set(C, 2);
    }
    void CPU::SET2D() {
        bit_set(D, 2);
    }
    void CPU::SET2E() {
        bit_set(E, 2);
    }
    void CPU::SET2H() {
        bit_set(H, 2);
    }
    void CPU::SET2L() {
        bit_set(L, 2);
    }
    void CPU::SET2HL() {
        auto t = read((H << 8) | L);
        bit_set(t, 2);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::SET2A() {
        bit_set(A, 2);
    }
    void CPU::SET3B() {
        bit_set(B, 3);
    }
    void CPU::SET3C() {
        bit_set(C, 3);
    }
    void CPU::SET3D() {
        bit_set(D, 3);
    }
    void CPU::SET3E() {
        bit_set(E, 3);
    }
    void CPU::SET3H() {
        bit_set(H, 3);
    }
    void CPU::SET3L() {
        bit_set(L, 3);
    }
    void CPU::SET3HL() {
        auto t = read((H << 8) | L);
        bit_set(t, 3);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::SET3A() {
        bit_set(A, 3);
    }
    void CPU::SET4B() {
        bit_set(B, 4);
    }
    void CPU::SET4C() {
        bit_set(C, 4);
    }
    void CPU::SET4D() {
        bit_set(D, 4);
    }
    void CPU::SET4E() {
        bit_set(E, 4);
    }
    void CPU::SET4H() {
        bit_set(H, 4);
    }
    void CPU::SET4L() {
        bit_set(L, 4);
    }
    void CPU::SET4HL() {
        auto t = read((H << 8) | L);
        bit_set(t, 4);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::SET4A() {
        bit_set(A, 4);
    }
    void CPU::SET5B() {
        bit_set(B, 5);
    }
    void CPU::SET5C() {
        bit_set(C, 5);
    }
    void CPU::SET5D() {
        bit_set(D, 5);
    }
    void CPU::SET5E() {
        bit_set(E, 5);
    }
    void CPU::SET5H() {
        bit_set(H, 5);
    }
    void CPU::SET5L() {
        bit_set(L, 5);
    }
    void CPU::SET5HL() {
        auto t = read((H << 8) | L);
        bit_set(t, 5);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::SET5A() {
        bit_set(A, 5);
    }
    void CPU::SET6B() {
        bit_set(B, 6);
    }
    void CPU::SET6C() {
        bit_set(C, 6);
    }
    void CPU::SET6D() {
        bit_set(D, 6);
    }
    void CPU::SET6E() {
        bit_set(E, 6);
    }
    void CPU::SET6H() {
        bit_set(H, 6);
    }
    void CPU::SET6L() {
        bit_set(L, 6);
    }
    void CPU::SET6HL() {
        auto t = read((H << 8) | L);
        bit_set(t, 6);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::SET6A() {
        bit_set(A, 6);
    }
    void CPU::SET7B() {
        bit_set(B, 7);
    }
    void CPU::SET7C() {
        bit_set(C, 7);
    }
    void CPU::SET7D() {
        bit_set(D, 7);
    }
    void CPU::SET7E() {
        bit_set(E, 7);
    }
    void CPU::SET7H() {
        bit_set(H, 7);
    }
    void CPU::SET7L() {
        bit_set(L, 7);
    }
    void CPU::SET7HL() {
        auto t = read((H << 8) | L);
        bit_set(t, 7);
        write((H << 8) | L, t);
        tTemp += 8;
    }
    void CPU::SET7A() {
        bit_set(A, 7);
    }
    void CPU::Reset(bool skip) {
        if (!skip) {
            A = 0;
            F = 0;
            B = 0;
            C = 0;
            D = 0;
            E = 0;
            H = 0;
            L = 0;
            SP = 0;
            PC = 0;
        } else {
            if (bus_.UseCGB) {
                A = 0x11;
                F = 0x80;
                B = 0x00;
                C = 0x00;
                D = 0xFF;
                E = 0x56;
                H = 0x00;
                L = 0x0D;
                SP = 0xFFFE;
                PC = 0x100;
            } else {
                A = 0x01;
                F = 0xB0;
                B = 0x00;
                C = 0x13;
                D = 0x00;
                E = 0xD8;
                H = 0x01;
                L = 0x4D;
                SP = 0xFFFE;
                PC = 0x100;
            }
        }
        TClock = 0;
        TotalClocks = 0;
        halt_ = false;
        stop_ = false;
        ime_ = false;
        ime_scheduled_ = false;
    }
    int CPU::Update() {
        tTemp = 0;
        tRemove = 0;
        handle_interrupts();
        if (ime_scheduled_) {
            ime_ = true;
            ime_scheduled_ = false;
        }
        if (halt_) {
            TClock += 4;
            return 4;
        }
        auto old_pc = PC++;
        PC -= halt_bug_;
        halt_bug_ = false;
        last_instr_ = bus_.Read(old_pc);
        (this->*Instructions[last_instr_].op)();
        TClock += tTemp;
        if (tTemp >= tRemove) {
            tTemp -= tRemove;
        } else {
            tTemp = 0;
        }
        bus_.TransferDMA(tTemp);
        TotalClocks += 1;
        return tTemp;
    }
    void CPU::handle_interrupts() {
        uint8_t interr_bits = IF & IE & 0x1F;
        if (interr_bits != 0) {
            if (ime_) {
                for (int i = 0; i < 5; i++) {
                    auto bit = (interr_bits >> i) & 0x1;
                    if (bit) {
                        execute_interrupt(i);
                        halt_ = false;
                        return;
                    }
                }
            } else {
                halt_ = false;
            }
        }
    }
    void CPU::execute_interrupt(int bit) {
        ime_ = false;
        ime_scheduled_ = false;
        uint8_t old_if = IF;
        IF &= ~(1U << bit);
        rst(0x40 + bit * 0x8);
        // ie_push special behavior
        // When IE is changed during upper byte push, the ISR
        // acts differently
        if ((SP + 1) == 0xFFFF) {
            // Only if SP + 1 == 0xFFFF is this bug possible
            // if SP == 0xFFFF it's not fast enough to change IE
            PC = 0;
            IF = old_if;
            uint8_t other_interrupt = IF & IE & 0x1F;
            if (other_interrupt) {
                // If during this bug there is another interrupt queued,
                // we execute the other interrupt instead
                execute_interrupt(log2(other_interrupt));
            }
        }
        tTemp = 20;
        if (halt_) {
            tTemp = 24;
        }
        delay_dur(tTemp - tRemove);
        tRemove = 0;
    }
    // Delayed read function
    uint8_t CPU::read(uint16_t addr) {
        delay();
        return bus_.Read(addr);
    }
    // Delayed write function
    void CPU::write(uint16_t addr, uint8_t val) {
        delay();
        bus_.Write(addr, val);
    }
    /// Function that updates the dma/timer/ppu mid instruction
    /// Tests like mem_timing and gekkio acceptance tests check
    /// dma/timer values mid instruction
    /// The first read/write function of each
    /// instruction does not seem to need to be delayed
    void CPU::delay() {
        tRemove += 4;
        delay_dur(4);
    }
    void CPU::delay_dur(uint8_t dur) {
        bus_.TransferDMA(dur);
        if (timer_.Update(dur, IF)) {
            if (halt_) {
                halt_ = false;
                skip_next_ = true;
            }
        }
        ppu_.Update(dur);
        apu_.Update(dur);
    }
    // Sets hardware registers to correct values
    void CPU::setup_hwio() {
        bus_.Write(0xFF00, 0xCF);
        bus_.Write(0xFF01, 0x00);
        bus_.Write(0xFF02, 0x7E);
        bus_.Write(0xFF04, 0xAC);
        bus_.Write(0xFF05, 0x00);
        bus_.Write(0xFF06, 0x00);
        bus_.Write(0xFF07, 0xF8);
        bus_.Write(0xFF0F, 0xE1);
        bus_.Write(0xFF10, 0x80);
        bus_.Write(0xFF11, 0xBF);
        bus_.Write(0xFF12, 0xF3);
        bus_.Write(0xFF13, 0xFF);
        bus_.Write(0xFF14, 0xBF);
        bus_.Write(0xFF16, 0x3F);
        bus_.Write(0xFF17, 0x00);
        bus_.Write(0xFF18, 0xFF);
        bus_.Write(0xFF19, 0xBF);
        bus_.Write(0xFF1A, 0x7F);
        bus_.Write(0xFF1B, 0xFF);
        bus_.Write(0xFF1C, 0x9F);
        bus_.Write(0xFF1D, 0xFF);
        bus_.Write(0xFF1E, 0xBF);
        bus_.Write(0xFF20, 0xFF);
        bus_.Write(0xFF21, 0x00);
        bus_.Write(0xFF22, 0x00);
        bus_.Write(0xFF23, 0xBF);
        bus_.Write(0xFF24, 0x77);
        bus_.Write(0xFF25, 0xF3);
        bus_.Write(0xFF26, 0xF1);
        bus_.Write(0xFF40, 0x91);
        bus_.Write(0xFF41, 0x82);
        bus_.Write(0xFF42, 0x00);
        bus_.Write(0xFF43, 0x00);
        bus_.Write(0xFF44, 0x00);
        bus_.Write(0xFF45, 0x00);
        bus_.Write(0xFF46, 0xFF);
        bus_.Write(0xFF47, 0xFC);
        bus_.Write(0xFF4A, 0x00);
        bus_.Write(0xFF4B, 0x00);
        bus_.Write(0xFF4D, 0xFF);
        bus_.Write(0xFF4F, 0xFF);
        bus_.Write(0xFF51, 0xFF);
        bus_.Write(0xFF52, 0xFF);
        bus_.Write(0xFF53, 0xFF);
        bus_.Write(0xFF54, 0xFF);
        bus_.Write(0xFF55, 0xFF);
        bus_.Write(0xFF56, 0xFF);
        bus_.Write(0xFF68, 0xFF);
        bus_.Write(0xFF69, 0xFF);
        bus_.Write(0xFF6A, 0xFF);
        bus_.Write(0xFF6B, 0xFF);
        bus_.Write(0xFF70, 0xFF);
        bus_.Write(0xFFFF, 0x00);
    }
}    // namespace TKPEmu::Gameboy::Devices
