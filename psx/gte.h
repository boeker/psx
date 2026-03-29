#ifndef PSX_GTE_H
#define PSX_GTE_H

#include <cstdint>
#include <iostream>

namespace PSX {

#define GTE_REG_VXY0 0
#define GTE_REG_VZ0 1
#define GTE_REG_VXY1 2
#define GTE_REG_VZ1 3
#define GTE_REG_VXY2 4
#define GTE_REG_VZ2 5
#define GTE_REG_RGBC 6
#define GTE_REG_OTZ 7
#define GTE_REG_IR0 8
#define GTE_REG_IR1 9
#define GTE_REG_IR2 10
#define GTE_REG_IR3 11
#define GTE_REG_SXY0 12
#define GTE_REG_SXY1 13
#define GTE_REG_SXY2 14
#define GTE_REG_SXYP 15
#define GTE_REG_SZ0 16
#define GTE_REG_SZ1 17
#define GTE_REG_SZ2 18
#define GTE_REG_SZ3 19
#define GTE_REG_RGB0 20
#define GTE_REG_RGB1 21
#define GTE_REG_RGB2 22
#define GTE_REG_MAC0 24
#define GTE_REG_MAC1 25
#define GTE_REG_MAC2 26
#define GTE_REG_MAC3 27

#define GTE_REG_RT11RT12 32
#define GTE_REG_RT13RT21 33
#define GTE_REG_RT22RT23 34
#define GTE_REG_RT31RT32 35
#define GTE_REG_RT33 36
#define GTE_REG_TRX 37
#define GTE_REG_TRY 38
#define GTE_REG_TRZ 39
#define GTE_REG_L11L12 40
#define GTE_REG_L13L21 41
#define GTE_REG_L22L23 42
#define GTE_REG_L31L32 43
#define GTE_REG_L33 44
#define GTE_REG_RBK 45
#define GTE_REG_GBK 46
#define GTE_REG_BBK 47
#define GTE_REG_LR1LR2 48
#define GTE_REG_LR3LG1 49
#define GTE_REG_LG2LG3 50
#define GTE_REG_LB1LB2 51
#define GTE_REG_LB3 52
#define GTE_REG_RFC 53
#define GTE_REG_GFC 54
#define GTE_REG_BFC 55
#define GTE_REG_OFX 56
#define GTE_REG_OFY 57
#define GTE_REG_H 58
#define GTE_REG_DQA 59
#define GTE_REG_DQB 60
#define GTE_REG_ZSF3 61

class GTE {
private:
    uint32_t registers[64];
    uint32_t instruction;
    uint8_t funct;

    friend std::ostream& operator<<(std::ostream &os, const GTE &gte);

public:
    static const char* REGISTER_NAMES[];
    std::string getRegisterName(uint8_t reg);
    std::string getControlRegisterName(uint8_t reg);

    GTE();
    void reset();

    uint32_t getRegister(uint8_t reg);
    void setRegister(uint8_t reg, uint32_t value);
    uint32_t getControlRegister(uint8_t reg);
    void setControlRegister(uint8_t reg, uint32_t value);

    void NCLIP();
    void RTPS();
    void RTPT();
    void NCDS();
    void AVSZ3();

    typedef void (GTE::*Opcode) ();
    static const Opcode cp2[];
    void execute(uint32_t instruction);
    void UNKCP2();
    void NCDT();
    void AVSZ4();
    void SQR();
    void OP();
    void GPF();
    void GPL();
    void NCCS();
    void NCCT();
    void NCS();
    void NCT();
    void CC();
    void DPCS();
    void DPCT();
    void INTPL();
    void CDP();
    void DCPL();
    void MVMVA();
    void UNOFF();
};

}

#endif
