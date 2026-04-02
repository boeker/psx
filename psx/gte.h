#ifndef PSX_GTE_H
#define PSX_GTE_H

#include <cstdint>
#include <iostream>

namespace PSX {

#define GTE_FLAGS_ERROR 31
#define GTE_FLAGS_MAC1_POS_OVERFLOW 30
#define GTE_FLAGS_MAC2_POS_OVERFLOW 29
#define GTE_FLAGS_MAC3_POS_OVERFLOW 28
#define GTE_FLAGS_MAC1_NEG_OVERFLOW 27
#define GTE_FLAGS_MAC2_NEG_OVERFLOW 26
#define GTE_FLAGS_MAC3_NEG_OVERFLOW 25
#define GTE_FLAGS_IR1 24
#define GTE_FLAGS_IR2 23
#define GTE_FLAGS_IR3 22
#define GTE_FLAGS_MAC0_POS_OVERFLOW 16
#define GTE_FLAGS_MAC0_NEG_OVERFLOW 15

#define GTE_INST_SF 19 // Shift 12 bit fraction in IR registers
#define GTE_INST_LM 10 // Clamp IR1, IR2, and IR3 result to -0x8000/0x0000...0x7FFF
#define GTE_INST_FUNC4 4
#define GTE_INST_FUNC3 3
#define GTE_INST_FUNC2 2
#define GTE_INST_FUNC1 1
#define GTE_INST_FUNC0 0

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
#define GTE_REG_IRGB 28
#define GTE_REG_ORGB 29

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
#define GTE_REG_FLAGS 63

class GTE {
private:
    struct int16_t_pair {
        int16_t x;
        int16_t y;

        void reset() {
            x = 0;
            y = 0;
        }
    };

    struct int16_t_triple {
        int16_t x;
        int16_t y;
        int16_t z;

        void reset() {
            x = 0;
            y = 0;
            z = 0;
        }
    };

    // Registers
    uint32_t registers[64]; // TODO remove

    int16_t_triple v0;                   // VXY0, VZ0
    int16_t_triple v1;                   // VXY1, VZ1
    int16_t_triple v2;                   // VXY2, VZ2
    uint32_t rgbc;                       // RGBC
    uint16_t ordering_table_z;           // OTZ
    int16_t ir0;                         // IR0
    int16_t ir1, ir2, ir3;               // IR1, IR2, IR3
    int16_t_pair xy0, xy1, xy2, xyp;     // SXY0, SXY1, SXY2, SXYP
    uint32_t z0, z1, z2, z3;             // SZ0, SZ1, SZ2, SZ3
    uint32_t rgb0, rgb1, rgb2;           // RGB0, RGB1, RGB2
    uint32_t reserved;                   // RES1
    int64_t mac0;                        // MAC0
    int64_t mac1, mac2, mac3;            // MAC1, MAC2, MAC3
    uint32_t input_rgb;                  // IRGB
    uint32_t output_rgb;                 // ORGB
    uint32_t leading_zeros_count_source; // LZCS
    uint32_t leading_zeros_count_result; // LZCR


    // Control registers
    int16_t rotation_matrix[9];           // RT11, ..., RT33
    int32_t translation_vector[3];        // TRX, TRY, TRZ
    int16_t light_source_matrix[9];       // L11, ..., L33
    uint32_t background_color[3];         // RBK, GBK, BBK
    int16_t light_color_matrix_source[9]; // LR1, ... ,LB3
    uint32_t far_color[3];                // RFC, GFC ,BFC
    int32_t screen_offset[2];             // OFX, OFY
    uint16_t projection_plane_distance;   // H
    int16_t depth_cueing_coefficient;     // DQA
    int32_t depth_cueing_offset;          // DQB
    int16_t average_z_scale_factors[2];   // ZSF3, ZSF4
    uint32_t flags;                       // FLAG

    uint32_t instruction;
    bool lm;
    bool sf;
    uint8_t funct;

    friend std::ostream& operator<<(std::ostream &os, const GTE &gte);

public:
    static const char* REGISTER_NAMES[];
    std::string getRegisterName(uint8_t reg);
    std::string getControlRegisterName(uint8_t reg);

    GTE();
    void reset();
    void reset_flags();

    void update_error_flag();
    void set_flag(uint8_t flag);

    uint32_t get_control_register_as_uint32_t(uint8_t rt);
    uint32_t get_register_as_uint32_t(uint8_t rt);
    uint32_t getRegister(uint8_t reg);
    void setRegister(uint8_t reg, uint32_t value);
    uint32_t getControlRegister(uint8_t reg);
    void setControlRegister(uint8_t reg, uint32_t value);

    static int32_t clamp_to_16bit(int32_t value, bool lm);
    static uint8_t convert_16bit_to_5bit_color(int32_t color);
    void set_ir1(int32_t value);
    void set_ir2(int32_t value);
    void set_ir3(int32_t value);
    void set_irgb(uint32_t value);

    void set_mac0(int64_t value);
    void set_mac1(int64_t value);
    void set_mac2(int64_t value);
    void set_mac3(int64_t value);

    int64_t get_sx0() const;
    int64_t get_sy0() const;
    int64_t get_sx1() const;
    int64_t get_sy1() const;
    int64_t get_sx2() const;
    int64_t get_sy2() const;

    int64_t get_ir1() const;
    int64_t get_ir2() const;
    int64_t get_ir3() const;

    int64_t get_mac0() const;
    int64_t get_mac1() const;
    int64_t get_mac2() const;
    int64_t get_mac3() const;

    typedef void (GTE::*Opcode) ();
    static const Opcode cp2[];
    void execute(uint32_t instruction);
    void UNKCP2();
    void NCLIP();
    void RTPS();
    void RTPT();
    void NCDS();
    void AVSZ3();
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
