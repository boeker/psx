#ifndef PSX_GTE_H
#define PSX_GTE_H

#include <cstdint>
#include <iostream>

#include "util/bit.h"

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
#define GTE_FLAGS_SZ3_OTZ_CLAMPED 18
#define GTE_FLAGS_RTP_DIVISION_CLAMPED 17
#define GTE_FLAGS_MAC0_POS_OVERFLOW 16
#define GTE_FLAGS_MAC0_NEG_OVERFLOW 15
#define GTE_FLAGS_SX2_CLAMPED 14
#define GTE_FLAGS_SY2_CLAMPED 13
#define GTE_FLAGS_IR0_CLAMPED 12

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
#define GTE_REG_RES1 23
#define GTE_REG_MAC0 24
#define GTE_REG_MAC1 25
#define GTE_REG_MAC2 26
#define GTE_REG_MAC3 27
#define GTE_REG_IRGB 28
#define GTE_REG_ORGB 29
#define GTE_REG_LZCS 30
#define GTE_REG_LZCR 31

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
#define GTE_REG_ZSF4 62
#define GTE_REG_FLAGS 63

class GTE {
private:
    uint16_t unr_table[0x101];
    uint32_t unr_division(uint16_t h, uint16_t sz3);

    // Registers
    util::Bit::int16_t_triple v0;              // VXY0, VZ0
    util::Bit::int16_t_triple v1;              // VXY1, VZ1
    util::Bit::int16_t_triple v2;              // VXY2, VZ2
    uint32_t rgbc;                             // RGBC
    uint16_t otz;                              // OTZ
    int16_t ir0;                               // IR0
    int16_t ir1, ir2, ir3;                     // IR1, IR2, IR3
    util::Bit::int16_t_pair sxy0, sxy1, sxy2;  // SXY0, SXY1, SXY2, SXYP (pushing mirror of SXY2)
    uint16_t sz0, sz1, sz2, sz3;               // SZ0, SZ1, SZ2, SZ3
    uint32_t rgb0, rgb1, rgb2;                 // RGB0, RGB1, RGB2
    uint32_t reserved;                         // RES1
    int64_t mac0;                              // MAC0 // larger than 32 bit internally
    int64_t mac1, mac2, mac3;                  // MAC1, MAC2, MAC3 // 44 bit
    uint32_t rgb;                              // IRGB, ORGB
    uint32_t lzcs;                             // LZCS, (LZCR is computed when reading)


    // Control registers
    int16_t rotation_matrix[9];                // RT11, ..., RT33
    int32_t translation_vector[3];             // TRX, TRY, TRZ
    int16_t light_source_matrix[9];            // L11, ..., L33
    int32_t background_color[3];               // RBK, GBK, BBK
    int16_t light_color_matrix_source[9];      // LR1, ... ,LB3
    int32_t far_color[3];                      // RFC, GFC ,BFC
    int32_t ofx, ofy;                          // OFX, OFY
    uint16_t h;                                // H
    int16_t dqa;                               // DQA
    int32_t dqb;                               // DQB
    int16_t zsf3, zsf4;                        // ZSF3, ZSF4
    uint32_t flags;                            // FLAG

    uint32_t instruction;
    bool lm;
    bool sf;
    uint8_t funct;

    friend std::ostream& operator<<(std::ostream &os, const GTE &gte);

    static const char* REGISTER_NAMES[];

public:
    std::string getRegisterName(uint8_t reg);
    std::string getControlRegisterName(uint8_t reg);

    GTE();
    void reset();

    uint32_t getRegister(uint8_t reg);
    void setRegister(uint8_t reg, uint32_t value);
    uint32_t getControlRegister(uint8_t reg);
    void setControlRegister(uint8_t reg, uint32_t value);

    void execute(uint32_t instruction);

private:
    /*
     * Helper functions for internal use
     */
    void reset_flags();
    void update_error_flag();
    void set_flag(uint8_t flag);

    // Getters and setters for interaction with the CPU
    uint32_t get_register_as_uint32_t(uint8_t rt);
    void set_register_from_uint32_t(uint8_t rt, uint32_t value);
    uint32_t get_control_register_as_uint32_t(uint8_t rt);
    void set_control_register_from_uint32_t(uint8_t rt, uint32_t value);

    static int32_t clamp_to_16bit(int32_t value, bool lm);
    static uint8_t convert_16bit_to_5bit_color(int32_t color);
    void push_sxy_queue();
    void push_sz_queue();
    void push_color_queue();

    /*
     * Setters for internal use
     */
    void set_sx2(int64_t value);
    void set_sy2(int64_t value);
    void set_ir0(int64_t value);
    void set_ir1(int32_t value);
    void set_ir2(int32_t value);
    void set_ir3(int32_t value);
    void set_ir1_without_clamping(int16_t value);
    void set_ir2_without_clamping(int16_t value);
    void set_ir3_without_clamping(int16_t value);
    void set_irgb(uint32_t value);

    void set_otz(int64_t value);
    void set_sz3(int64_t value);

    void set_mac0(int64_t value);
    void set_mac1(int64_t value);
    void set_mac2(int64_t value);
    void set_mac3(int64_t value);

    /*
     * Getters for internal use
     */
    int64_t get_vx0() const { return v0.x; }
    int64_t get_vy0() const { return v0.y; }
    int64_t get_vz0() const { return v0.z; }
    int64_t get_vx1() const { return v1.x; }
    int64_t get_vy1() const { return v1.y; }
    int64_t get_vz1() const { return v1.z; }
    int64_t get_vx2() const { return v2.x; }
    int64_t get_vy2() const { return v2.y; }
    int64_t get_vz2() const { return v2.z; }

    int64_t get_r() const { return (rgbc >> 24) & 0xFF; }
    int64_t get_g() const { return (rgbc >> 16) & 0xFF; }
    int64_t get_b() const { return (rgbc >> 8) & 0xFF; }
    int64_t get_c() const { return rgbc & 0xFF; }

    int64_t get_sx0() const { return sxy0.x; }
    int64_t get_sy0() const { return sxy0.y; }
    int64_t get_sz0() const { return sz0; }
    int64_t get_sx1() const { return sxy1.x; }
    int64_t get_sy1() const { return sxy1.y; }
    int64_t get_sz1() const { return sz1; }
    int64_t get_sx2() const { return sxy2.x; }
    int64_t get_sy2() const { return sxy2.y; }
    int64_t get_sz2() const { return sz2; }
    int64_t get_sz3() const { return sz3; }

    int64_t get_ir0() const { return ir0; }
    int64_t get_ir1() const { return ir1; }
    int64_t get_ir2() const { return ir2; }
    int64_t get_ir3() const { return ir3; }

    int64_t get_mac0() const { return mac0; }
    int64_t get_mac1() const { return mac1; }
    int64_t get_mac2() const { return mac2; }
    int64_t get_mac3() const { return mac3; }

    int64_t get_rt11() const { return rotation_matrix[0]; }
    int64_t get_rt12() const { return rotation_matrix[1]; }
    int64_t get_rt13() const { return rotation_matrix[2]; }
    int64_t get_rt21() const { return rotation_matrix[3]; }
    int64_t get_rt22() const { return rotation_matrix[4]; }
    int64_t get_rt23() const { return rotation_matrix[5]; }
    int64_t get_rt31() const { return rotation_matrix[6]; }
    int64_t get_rt32() const { return rotation_matrix[7]; }
    int64_t get_rt33() const { return rotation_matrix[8]; }

    int64_t get_trx() const { return translation_vector[0]; }
    int64_t get_try() const { return translation_vector[1]; }
    int64_t get_trz() const { return translation_vector[2]; }

    int64_t get_l11() const { return light_source_matrix[0]; }
    int64_t get_l12() const { return light_source_matrix[1]; }
    int64_t get_l13() const { return light_source_matrix[2]; }
    int64_t get_l21() const { return light_source_matrix[3]; }
    int64_t get_l22() const { return light_source_matrix[4]; }
    int64_t get_l23() const { return light_source_matrix[5]; }
    int64_t get_l31() const { return light_source_matrix[6]; }
    int64_t get_l32() const { return light_source_matrix[7]; }
    int64_t get_l33() const { return light_source_matrix[8]; }

    int64_t get_rbk() const { return background_color[0]; }
    int64_t get_gbk() const { return background_color[1]; }
    int64_t get_bbk() const { return background_color[2]; }

    int64_t get_lr1() const { return light_color_matrix_source[0]; }
    int64_t get_lr2() const { return light_color_matrix_source[1]; }
    int64_t get_lr3() const { return light_color_matrix_source[2]; }
    int64_t get_lg1() const { return light_color_matrix_source[3]; }
    int64_t get_lg2() const { return light_color_matrix_source[4]; }
    int64_t get_lg3() const { return light_color_matrix_source[5]; }
    int64_t get_lb1() const { return light_color_matrix_source[6]; }
    int64_t get_lb2() const { return light_color_matrix_source[7]; }
    int64_t get_lb3() const { return light_color_matrix_source[8]; }

    int64_t get_rfc() const { return far_color[0]; }
    int64_t get_gfc() const { return far_color[1]; }
    int64_t get_bfc() const { return far_color[2]; }

    int64_t get_ofx() const { return ofx; }
    int64_t get_ofy() const { return ofy; }

    int64_t get_h() const { return h; }
    int64_t get_dqa() const { return dqa; }
    int64_t get_dqb() const { return dqb; }
    int64_t get_zsf3() const { return zsf3; }
    int64_t get_zsf4() const { return zsf4; }

    /*
     * Command implementations
     */
    typedef void (GTE::*Opcode) ();
    static const Opcode cp2[];
    void UNKCP2();
    void RTPS();
    void RTPT();
    void MVMVA();
    void DCPL();
    void DPCS();
    void DPCT();
    void INTPL();
    void SQR();
    void NCS();
    void NCT();
    void NCDS();
    void NCDT();
    void NCCS();
    void NCCT();
    void CDP();
    void CC();
    void NCLIP();
    void AVSZ3();
    void AVSZ4();
    void OP();
    void GPF();
    void GPL();
    void UNOFF();
};

}

#endif
