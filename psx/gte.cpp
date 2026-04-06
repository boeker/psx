#include "gte.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <sstream>

#include "util/log.h"

#define INT32(x) static_cast<int32_t>(x)
#define INT64(x) static_cast<int64_t>(x)

using namespace util;

namespace PSX {

std::ostream& operator<<(std::ostream &os, const GTE &gte) {
    return os;
}

const char* GTE::REGISTER_NAMES[] = {
    "vxy0", "vz0", // 3xS16: Vector 0 (X,Y,Z)
    "vxy1", "vz1", // 3xS16: Vector 1 (X,Y,Z)
    "vxy2", "vz2", // 3xS16: Vector 2 (X,Y,Z)
    "rgbc", // 4xU8: Color/code value
    "otz", // 1xS16: Average Z value
    "ir0", // 1xS16: 16bit accumulator (interpolate)
    "ir1", "ir2", "ir3", // 3xS16: 16bit accumulator (vector)
    "sxy0", "sxy1", "sxy2", "sxyp", // 6xS16: Screen XY-coordinate queue (3 items), what does sxyp do?
    "sz0", "sz1", "sz2", "sz3", // 4xU16: Screen Z-coordinate queue (4 items)
    "rgb0", "rgb1", "rgb2", // 12xU8: Color CRGB-code/color queue (3 items)
    "res1", // 4xU8: Prohibited
    "mac0", // 1xS32: 32bit math accumulator (value)
    "mac1", "mac2", "mac3", // 3xS32: 32bit math accumulator (vector)
    "irgb", "orgb", // 1xU15: convert RGB color (48bit vs 15bit?)
    "lzcs", "lzcr", // 2xS32: Count leading zeros/ones (sign bits)
    "rt11rt12", "rt13rt21", "rt22rt23", "rt31rt32", "rt33", // 9xS16: Rotation matrix (3x3)
    "trx", "try", "trz", // 3x32: Translaction vector (X,Y,Z)
    "l11l12", "l13l21", "l22l23", "l31l32", "l33", // 9xS16: Light source matrix (3x3)
    "rbk", "gbk", "bbk", // 3x32: Background color (R,G,B)
    "lr1lr2", "lr3lg1", "lg2lg3", "lb1lb2", "lb3", // 9xS16: Light color matrix source (3x3)
    "rfc", "gfc", "bfc", // 3x32: Far color (R,G,B)
    "ofx", "ofy", // 2x32: Screen offset (X,Y)
    "h", // BuggyU16: Projection plane distance
    "dqa", // S16: Depth queueing parameter A (coeff)
    "dqb", // 32: Depth queueing parameter B (offset)
    "zsf3", "zsf4", // 2xS16: Average Z scale factors
    "flag" // U20: Calculation errors
};

const GTE::Opcode GTE::cp2[] = {
    // 0b000000
    &GTE::UNOFF,    &GTE::RTPS,     &GTE::UNOFF,    &GTE::UNOFF,
    // 0b000100
    &GTE::UNOFF,    &GTE::UNOFF,    &GTE::NCLIP,    &GTE::UNOFF,
    // 0b001000
    &GTE::UNOFF,    &GTE::UNOFF,    &GTE::UNOFF,    &GTE::UNOFF,
    // 0b001100
    &GTE::OP,       &GTE::UNOFF,    &GTE::UNOFF,    &GTE::UNOFF,
    // 0b010000
    &GTE::DPCS,     &GTE::INTPL,    &GTE::MVMVA,    &GTE::NCDS,
    // 0b010100
    &GTE::CDP,      &GTE::UNOFF,    &GTE::NCDT,     &GTE::UNOFF,
    // 0b011000
    &GTE::UNOFF,    &GTE::UNOFF,    &GTE::UNOFF,    &GTE::NCCS,
    // 0b011100
    &GTE::CC,       &GTE::UNOFF,    &GTE::NCS,      &GTE::UNOFF,
    // 0b100000
    &GTE::NCT,      &GTE::UNOFF,    &GTE::UNOFF,    &GTE::UNOFF,
    // 0b100100
    &GTE::UNOFF,    &GTE::UNOFF,    &GTE::UNOFF,    &GTE::UNOFF,
    // 0b101000
    &GTE::SQR,      &GTE::DCPL,     &GTE::DPCT,     &GTE::UNOFF,
    // 0b101100
    &GTE::UNOFF,    &GTE::AVSZ3,    &GTE::AVSZ4,    &GTE::UNOFF,
    // 0b110000
    &GTE::RTPT,     &GTE::UNOFF,    &GTE::UNOFF,    &GTE::UNOFF,
    // 0b110100
    &GTE::UNOFF,    &GTE::UNOFF,    &GTE::UNOFF,    &GTE::UNOFF,
    // 0b111000
    &GTE::UNOFF,    &GTE::UNOFF,    &GTE::UNOFF,    &GTE::UNOFF,
    // 0b111100
    &GTE::UNOFF,    &GTE::GPF,      &GTE::GPL,      &GTE::NCCT
};


std::string GTE::getRegisterName(uint8_t reg) {
    return REGISTER_NAMES[reg];
}

std::string GTE::getControlRegisterName(uint8_t reg) {
    return REGISTER_NAMES[32 + reg];
}

uint32_t GTE::unr_division(uint16_t h, uint16_t sz3) {
    //  n = ((h * 20000h / sz3) + 1) / 2
    if (h < 2 * sz3) {
        uint8_t z = std::countl_zero(sz3);
        uint32_t n = h << z;
        uint32_t d = sz3 << z;
        uint16_t u = unr_table[(d - 0x7FC0) >> 7] + 0x101;
        d = (0x200'0080 - (d * u)) >> 8;
        d = (0x000'0080 + (d * u)) >> 8;
        return std::min(0x1'FFFFU, ((n * d) + 0x8000) >> 16);
    } else {
        set_flag(GTE_FLAGS_RTP_DIVISION_CLAMPED);
        return 0x1'FFFF;
    }
}

GTE::GTE() {
    reset();
    for (int i = 0; i < 0x101; ++i) {
        unr_table[i] = std::max(0, (0x40000 / (i + 0x100) + 1) / 2 - 0x101);
    }
}

void GTE::reset() {
    v0.reset();
    v1.reset();
    v2.reset();
    rgbc = 0;
    otz = 0;
    ir0 = 0;
    ir1 = 0;
    ir2 = 0;
    ir3 = 0;
    sxy0.reset();
    sxy1.reset();
    sxy2.reset();
    sz0 = 0;
    sz1 = 0;
    sz2 = 0;
    sz3 = 0;
    rgb0 = 0;
    rgb1 = 0;
    rgb2 = 0;
    reserved = 0;
    mac0 = 0;
    mac1 = 0;
    mac2 = 0;
    mac3 = 0;
    rgb = 0;
    lzcs = 0;

    for (int i = 0; i < 9; ++i) {
        rotation_matrix[i] = 0;
        light_source_matrix[i] = 0;
        light_color_matrix_source[i] = 0;
    }
    for (int i = 0; i < 3; ++i) {
        translation_vector[i] = 0;
        background_color[i] = 0;
        far_color[i] = 0;
    }
    ofx = 0;
    ofy = 0;
    h = 0;
    dqa = 0;
    dqb = 0;
    zsf3 = 0;
    zsf4 = 0;
    flags = 0;

    instruction = 0;
    lm = false;
    sf = false;
    funct = 0;
}

uint32_t GTE::getRegister(uint8_t rt) {
    assert (rt < 32);
    uint32_t word = get_register_as_uint32_t(rt);

    LOGT_GTE(std::format("{{register {:d} ({:s}) -> 0x{:08X}}}", rt, getRegisterName(rt), word));

    return word;
}

void GTE::setRegister(uint8_t rt, uint32_t value) {
    assert (rt < 32);
    LOGT_GTE(std::format("{{0x{:08X} -> register {:d} ({:s})}}", value, rt, getRegisterName(rt)));

    set_register_from_uint32_t(rt, value);
}

uint32_t GTE::getControlRegister(uint8_t rt) {
    assert (rt < 32);
    uint32_t word = get_control_register_as_uint32_t(rt);

    LOGT_GTE(std::format("{{control register {:d} ({:s}) -> 0x{:08X}}}", rt, getControlRegisterName(rt), word));

    return word;
}

void GTE::setControlRegister(uint8_t rt, uint32_t value) {
    assert (rt < 32);
    LOGT_GTE(std::format("{{0x{:08X} -> control register {:d} ({:s})}}", value, rt, getControlRegisterName(rt)));

    set_control_register_from_uint32_t(rt, value);
}

void GTE::execute(uint32_t instruction) {
    // Operation depends on function field
    this->instruction = instruction;
    this->sf = Bit::getBit(instruction, GTE_INST_SF);
    this->lm = Bit::getBit(instruction, GTE_INST_LM);
    funct = 0x3F & instruction;

    reset_flags();
    (this->*cp2[funct])();
}

void GTE::reset_flags() {
    flags = 0;
}

void GTE::update_error_flag() {
    uint32_t bits_in_error_flag = 0x7F87'E000; // 30 to 23, 18 to 13 (IR3 not included?!)
    Bit::setBit(flags, GTE_FLAGS_ERROR, flags & bits_in_error_flag);
}

void GTE::set_flag(uint8_t flag) {
    Bit::setBit(flags, flag);
    update_error_flag();
}

uint32_t GTE::get_register_as_uint32_t(uint8_t rt) {
    assert(rt < 32);
    switch(rt) {
        case GTE_REG_VXY0:
            return v0.packXY();
        case GTE_REG_VZ0:
            return v0.packZ();
        case GTE_REG_VXY1:
            return v1.packXY();
        case GTE_REG_VZ1:
            return v1.packZ();
        case GTE_REG_VXY2:
            return v2.packXY();
        case GTE_REG_VZ2:
            return v2.packZ();
        case GTE_REG_RGBC:
            return rgbc;
        case GTE_REG_OTZ:
            return otz;
        case GTE_REG_IR0:
            return static_cast<int32_t>(ir0);
        case GTE_REG_IR1:
            return static_cast<int32_t>(ir1);
        case GTE_REG_IR2:
            return static_cast<int32_t>(ir2);
        case GTE_REG_IR3:
            return static_cast<int32_t>(ir3);
        case GTE_REG_SXY0:
            return sxy0.pack();
        case GTE_REG_SXY1:
            return sxy1.pack();
        case GTE_REG_SXY2:
            return sxy2.pack();
        case GTE_REG_SXYP:
            // Mirror of SXY2 when reading
            return sxy2.pack();
        case GTE_REG_SZ0:
            return sz0;
        case GTE_REG_SZ1:
            return sz1;
        case GTE_REG_SZ2:
            return sz2;
        case GTE_REG_SZ3:
            return sz3;
        case GTE_REG_RGB0:
            return rgb0;
        case GTE_REG_RGB1:
            return rgb1;
        case GTE_REG_RGB2:
            return rgb2;
        case GTE_REG_RES1:
            return reserved;
        case GTE_REG_MAC0:
            return mac0;
        case GTE_REG_MAC1:
            return mac1;
        case GTE_REG_MAC2:
            return mac2;
        case GTE_REG_MAC3:
            return mac3;
        case GTE_REG_IRGB:
            return rgb;
        case GTE_REG_ORGB:
            return rgb;
        case GTE_REG_LZCS:
            return lzcs;
        case GTE_REG_LZCR:
            return std::max(std::countl_one(lzcs), std::countl_zero(lzcs));
            return 0;
        default:
            assert(false);
            return 0;
    }
}

void GTE::set_register_from_uint32_t(uint8_t rt, uint32_t value) {
    assert(rt < 32);
    switch(rt) {
        case GTE_REG_VXY0:
            v0.unpackXY(value);
            break;
        case GTE_REG_VZ0:
            v0.unpackZ(value);
            break;
        case GTE_REG_VXY1:
            v1.unpackXY(value);
            break;
        case GTE_REG_VZ1:
            v1.unpackZ(value);
            break;
        case GTE_REG_VXY2:
            v2.unpackXY(value);
            break;
        case GTE_REG_VZ2:
            v2.unpackZ(value);
            break;
        case GTE_REG_RGBC:
            rgbc = value;
            break;
        case GTE_REG_OTZ:
            // Apparently not read-only?
            otz = value;
            break;
        case GTE_REG_IR0:
            ir0 = value;
            break;
        case GTE_REG_IR1:
            set_ir1_without_clamping(static_cast<int16_t>(value));
            break;
        case GTE_REG_IR2:
            set_ir2_without_clamping(static_cast<int16_t>(value));
            break;
        case GTE_REG_IR3:
            set_ir3_without_clamping(static_cast<int16_t>(value));
            break;
        case GTE_REG_SXY0:
            sxy0.unpack(value);
            break;
        case GTE_REG_SXY1:
            sxy1.unpack(value);
            break;
        case GTE_REG_SXY2:
            sxy2.unpack(value);
            break;
        case GTE_REG_SXYP:
            // Writes to SXY2 and moves the whole queue down
            push_sxy_queue();
            sxy2.unpack(value);
            break;
        case GTE_REG_SZ0:
            sz0 = value;
            break;
        case GTE_REG_SZ1:
            sz1 = value;
            break;
        case GTE_REG_SZ2:
            sz2 = value;
            break;
        case GTE_REG_SZ3:
            sz3 = value;
            break;
        case GTE_REG_RGB0:
            rgb0 = value;
            break;
        case GTE_REG_RGB1:
            rgb1 = value;
            break;
        case GTE_REG_RGB2:
            rgb2 = value;
            break;
        case GTE_REG_RES1:
            reserved = value;
            break;
        case GTE_REG_MAC0:
            mac0 = static_cast<int32_t>(value);
            break;
        case GTE_REG_MAC1:
            mac1 = static_cast<int32_t>(value);
            break;
        case GTE_REG_MAC2:
            mac2 = static_cast<int32_t>(value);
            break;
        case GTE_REG_MAC3:
            mac3 = static_cast<int32_t>(value);
            break;
        case GTE_REG_IRGB:
            set_irgb(value);
            break;
        case GTE_REG_ORGB:
            // Read-only
            break;
        case GTE_REG_LZCS:
            lzcs = value;
            break;
        case GTE_REG_LZCR:
            // Read-only
            break;
        default:
            assert(false);
    }
}

uint32_t GTE::get_control_register_as_uint32_t(uint8_t rt) {
    assert(rt < 32);
    rt += 32;
    switch(rt) {
        case GTE_REG_RT11RT12:
            return Bit::pack_int16_ts(rotation_matrix[0], rotation_matrix[1]);
        case GTE_REG_RT13RT21:
            return Bit::pack_int16_ts(rotation_matrix[2], rotation_matrix[3]);
        case GTE_REG_RT22RT23:
            return Bit::pack_int16_ts(rotation_matrix[4], rotation_matrix[5]);
        case GTE_REG_RT31RT32:
            return Bit::pack_int16_ts(rotation_matrix[6], rotation_matrix[7]);
        case GTE_REG_RT33:
            return Bit::pack_int16_t(rotation_matrix[8]);
        case GTE_REG_TRX:
            return translation_vector[0];
        case GTE_REG_TRY:
            return translation_vector[1];
        case GTE_REG_TRZ:
            return translation_vector[2];
        case GTE_REG_L11L12:
            return Bit::pack_int16_ts(light_source_matrix[0], light_source_matrix[1]);
        case GTE_REG_L13L21:
            return Bit::pack_int16_ts(light_source_matrix[2], light_source_matrix[3]);
        case GTE_REG_L22L23:
            return Bit::pack_int16_ts(light_source_matrix[4], light_source_matrix[5]);
        case GTE_REG_L31L32:
            return Bit::pack_int16_ts(light_source_matrix[6], light_source_matrix[7]);
        case GTE_REG_L33:
            return Bit::pack_int16_t(light_source_matrix[8]);
        case GTE_REG_RBK:
            return background_color[0];
        case GTE_REG_GBK:
            return background_color[1];
        case GTE_REG_BBK:
            return background_color[2];
        case GTE_REG_LR1LR2:
            return Bit::pack_int16_ts(light_color_matrix_source[0], light_color_matrix_source[1]);
        case GTE_REG_LR3LG1:
            return Bit::pack_int16_ts(light_color_matrix_source[2], light_color_matrix_source[3]);
        case GTE_REG_LG2LG3:
            return Bit::pack_int16_ts(light_color_matrix_source[4], light_color_matrix_source[5]);
        case GTE_REG_LB1LB2:
            return Bit::pack_int16_ts(light_color_matrix_source[6], light_color_matrix_source[7]);
        case GTE_REG_LB3:
            return Bit::pack_int16_t(light_color_matrix_source[8]);
        case GTE_REG_RFC:
            return far_color[0];
        case GTE_REG_GFC:
            return far_color[1];
        case GTE_REG_BFC:
            return far_color[2];
        case GTE_REG_OFX:
            return ofx;
        case GTE_REG_OFY:
            return ofy;
        case GTE_REG_H:
            // Hardware bug: sign-extend unsigned value
            return static_cast<int32_t>(static_cast<int16_t>(h));
        case GTE_REG_DQA:
            return static_cast<int32_t>(dqa);
        case GTE_REG_DQB:
            return dqb;
        case GTE_REG_ZSF3:
            return static_cast<int32_t>(zsf3);
        case GTE_REG_ZSF4:
            return static_cast<int32_t>(zsf4);
        case GTE_REG_FLAGS:
            return flags;
        default:
            assert(false);
            return 0;
    }
}

void GTE::set_control_register_from_uint32_t(uint8_t rt, uint32_t value) {
    assert(rt < 32);
    rt += 32;
    switch(rt) {
        case GTE_REG_RT11RT12:
            rotation_matrix[0] = Bit::unpack_first_int16_t(value);
            rotation_matrix[1] = Bit::unpack_second_int16_t(value);
            break;
        case GTE_REG_RT13RT21:
            rotation_matrix[2] = Bit::unpack_first_int16_t(value);
            rotation_matrix[3] = Bit::unpack_second_int16_t(value);
            break;
        case GTE_REG_RT22RT23:
            rotation_matrix[4] = Bit::unpack_first_int16_t(value);
            rotation_matrix[5] = Bit::unpack_second_int16_t(value);
            break;
        case GTE_REG_RT31RT32:
            rotation_matrix[6] = Bit::unpack_first_int16_t(value);
            rotation_matrix[7] = Bit::unpack_second_int16_t(value);
            break;
        case GTE_REG_RT33:
            rotation_matrix[8] = Bit::unpack_first_int16_t(value);
            break;
        case GTE_REG_TRX:
            translation_vector[0] = value;
            break;
        case GTE_REG_TRY:
            translation_vector[1] = value;
            break;
        case GTE_REG_TRZ:
            translation_vector[2] = value;
            break;
        case GTE_REG_L11L12:
            light_source_matrix[0] = Bit::unpack_first_int16_t(value);
            light_source_matrix[1] = Bit::unpack_second_int16_t(value);
            break;
        case GTE_REG_L13L21:
            light_source_matrix[2] = Bit::unpack_first_int16_t(value);
            light_source_matrix[3] = Bit::unpack_second_int16_t(value);
            break;
        case GTE_REG_L22L23:
            light_source_matrix[4] = Bit::unpack_first_int16_t(value);
            light_source_matrix[5] = Bit::unpack_second_int16_t(value);
            break;
        case GTE_REG_L31L32:
            light_source_matrix[6] = Bit::unpack_first_int16_t(value);
            light_source_matrix[7] = Bit::unpack_second_int16_t(value);
            break;
        case GTE_REG_L33:
            light_source_matrix[8] = Bit::unpack_first_int16_t(value);
            break;
        case GTE_REG_RBK:
            background_color[0] = value;
            break;
        case GTE_REG_GBK:
            background_color[1] = value;
            break;
        case GTE_REG_BBK:
            background_color[2] = value;
            break;
        case GTE_REG_LR1LR2:
            light_color_matrix_source[0] = Bit::unpack_first_int16_t(value);
            light_color_matrix_source[1] = Bit::unpack_second_int16_t(value);
            break;
        case GTE_REG_LR3LG1:
            light_color_matrix_source[2] = Bit::unpack_first_int16_t(value);
            light_color_matrix_source[3] = Bit::unpack_second_int16_t(value);
            break;
        case GTE_REG_LG2LG3:
            light_color_matrix_source[4] = Bit::unpack_first_int16_t(value);
            light_color_matrix_source[5] = Bit::unpack_second_int16_t(value);
            break;
        case GTE_REG_LB1LB2:
            light_color_matrix_source[6] = Bit::unpack_first_int16_t(value);
            light_color_matrix_source[7] = Bit::unpack_second_int16_t(value);
            break;
        case GTE_REG_LB3:
            light_color_matrix_source[8] = Bit::unpack_first_int16_t(value);
            break;
        case GTE_REG_RFC:
            far_color[0] = value;
            break;
        case GTE_REG_GFC:
            far_color[1] = value;
            break;
        case GTE_REG_BFC:
            far_color[2] = value;
            break;
        case GTE_REG_OFX:
            ofx = value;
            break;
        case GTE_REG_OFY:
            ofy = value;
            break;
        case GTE_REG_H:
            h = value & 0xFFFF;
            break;
        case GTE_REG_DQA:
            dqa = value;
            break;
        case GTE_REG_DQB:
            dqb = value;
            break;
        case GTE_REG_ZSF3:
            zsf3 = value;
            break;
        case GTE_REG_ZSF4:
            zsf4 = value;
            break;
        case GTE_REG_FLAGS:
            flags = value & 0x7FFF'F000;
            update_error_flag();
            break;
        default:
            assert(false);
    }
}

uint8_t GTE::clamp_to_u8bit(int64_t value) {
    int64_t clamped = std::min(static_cast<int64_t>(0xFF), value);
    clamped = std::max(static_cast<int64_t>(0x00), clamped);
    return clamped;
}

int64_t GTE::clamp_to_16bit(int64_t value, bool lm) {
    int64_t clamped = std::min(static_cast<int64_t>(0x7FFF), value);
    clamped = lm ? std::max(static_cast<int64_t>(0), clamped) : std::max(-static_cast<int64_t>(0x8000), clamped);
    return clamped;
}

uint8_t GTE::convert_16bit_to_5bit_color(int32_t color) {
    color = color / 0x80;
    color = std::min(0x1F, color);
    color = std::max(0x00, color);
    return color;
}

void GTE::push_sxy_queue() {
    sxy0 = sxy1;
    sxy1 = sxy2;
}

void GTE::push_sz_queue() {
    sz0 = sz1;
    sz1 = sz2;
    sz2 = sz3;
}

void GTE::push_color_queue() {
    rgb0 = rgb1;
    rgb1 = rgb2;
}

void GTE::set_sx2(int64_t value) {
    int64_t clamped = std::min(static_cast<int64_t>(0x03FF), value);
    clamped = std::max(-static_cast<int64_t>(0x0400), clamped); // Minus in front of static_cast
    if (clamped != value) {
        set_flag(GTE_FLAGS_SX2_CLAMPED);
    }
    sxy2.x = clamped;
}

void GTE::set_sy2(int64_t value) {
    int64_t clamped = std::min(static_cast<int64_t>(0x03FF), value);
    clamped = std::max(-static_cast<int64_t>(0x0400), clamped); // Minus in front of static_cast
    if (clamped != value) {
        set_flag(GTE_FLAGS_SY2_CLAMPED);
    }
    sxy2.y = clamped;
}

void GTE::set_ir0(int64_t value) {
    LOG_GTE(std::format("mac0        = 0x{:016X}", get_mac0()));
    LOG_GTE(std::format("set_ir0     = 0x{:016X}", value));
    LOG_GTE(std::format("mac0u       = 0x{:016X}", static_cast<uint64_t>(get_mac0())));
    LOG_GTE(std::format("set_ir0u    = 0x{:016X}", static_cast<uint64_t>(value)));
    int64_t clamped = std::min(static_cast<int64_t>(0x1000), value);
    clamped = std::max(static_cast<int64_t>(0x0000), clamped);
    if (clamped != value) {
        set_flag(GTE_FLAGS_IR0_CLAMPED);
    }
    LOG_GTE(std::format("set_ir0clamp= 0x{:016X}", clamped));
    ir0 = clamped;
}

void GTE::set_ir1(int64_t value) {
    int32_t lower = static_cast<int32_t>(value);
    int64_t clamped = clamp_to_16bit(lower, lm);
    if (clamped != lower) {
        set_flag(GTE_FLAGS_IR1);
    }
    set_ir1_without_clamping(clamped);
}

void GTE::set_ir2(int64_t value) {
    int32_t lower = static_cast<int32_t>(value);
    int64_t clamped = clamp_to_16bit(lower, lm);
    if (clamped != lower) {
        set_flag(GTE_FLAGS_IR2);
    }
    set_ir2_without_clamping(clamped);
}

void GTE::set_ir3(int64_t value) {
    int32_t lower = static_cast<int32_t>(value);
    int64_t clamped = clamp_to_16bit(lower, lm);
    if (clamped != lower) {
        set_flag(GTE_FLAGS_IR3);
    }
    set_ir3_without_clamping(clamped);
}

void GTE::set_ir1_without_clamping(int64_t value) {
    ir1 = value;
    rgb = (rgb & 0xFFFFFFE0) | convert_16bit_to_5bit_color(value);
}

void GTE::set_ir2_without_clamping(int64_t value) {
    ir2 = value;
    rgb = (rgb & 0xFFFFFC1F) | (convert_16bit_to_5bit_color(value) << 5);
}

void GTE::set_ir3_without_clamping(int64_t value) {
    ir3 = value;
    rgb = (rgb & 0xFFFF83FF) | (convert_16bit_to_5bit_color(value) << 10);
}

void GTE::set_irgb(uint32_t value) {
    rgb = value & 0x00007FFF;

    uint8_t r = Bit::getBits<5>(value, 0);
    uint8_t g = Bit::getBits<5>(value, 5);
    uint8_t b = Bit::getBits<5>(value, 10);

    ir1 = r * 0x80;
    ir2 = g * 0x80;
    ir3 = b * 0x80;
}

void GTE::set_otz(int64_t value) {
    int64_t clamped = std::min(static_cast<int64_t>(0xFFFF), value);
    clamped = std::max(static_cast<int64_t>(0x0000), clamped);
    if (clamped != value) {
        set_flag(GTE_FLAGS_SZ3_OTZ_CLAMPED);
    }
    otz = clamped;
}

void GTE::set_sz3(int64_t value) {
    int64_t clamped = std::min(static_cast<int64_t>(0xFFFF), value);
    clamped = std::max(static_cast<int64_t>(0x0000), clamped);
    if (clamped != value) {
        set_flag(GTE_FLAGS_SZ3_OTZ_CLAMPED);
    }
    sz3 = clamped;
}

void GTE::set_mac0(int64_t value) {
    int64_t clamped = std::min(static_cast<int64_t>(0x7FFF'FFFF), value);
    if (clamped != value) {
        // positive 32bit overflow
        set_flag(GTE_FLAGS_MAC0_POS_OVERFLOW);
    }
    int64_t temp = clamped;
    clamped = std::max(-static_cast<int64_t>(0x8000'0000), clamped); // Minus has to be in front of the cast!
    if (clamped != temp) {
        // negative 32bit overflow
        set_flag(GTE_FLAGS_MAC0_NEG_OVERFLOW);
    }
    // MAC0 does not get clamped! Value is just used for checking overflow
    mac0 = value;
}

void GTE::set_mac1(int64_t value) {
    int64_t clamped = std::min(0x7FF'FFFF'FFFF, value);
    if (clamped != value) {
        // positive 44bit overflow
        set_flag(GTE_FLAGS_MAC1_POS_OVERFLOW);
    }
    int64_t temp = clamped;
    clamped = std::max(-0x800'0000'0000, clamped);
    if (clamped != temp) {
        // negative 44bit overflow
        set_flag(GTE_FLAGS_MAC1_NEG_OVERFLOW);
    }
    // MAC1 does not get clamped! Value is just used for checking overflow
    //mac1 = value & 0xFFF'FFFF'FFFF;
    //if (mac1 & 0x800'0000'0000) {
    //    mac1 |= 0xFFFF'F000'0000'0000;
    //}
    mac1 = value;
}

void GTE::set_mac2(int64_t value) {
    int64_t clamped = std::min(0x7FF'FFFF'FFFF, value);
    if (clamped != value) {
        // positive 44bit overflow
        set_flag(GTE_FLAGS_MAC2_POS_OVERFLOW);
    }
    int64_t temp = clamped;
    clamped = std::max(-0x800'0000'0000, clamped);
    if (clamped != temp) {
        // negative 44bit overflow
        set_flag(GTE_FLAGS_MAC2_NEG_OVERFLOW);
    }
    // MAC2 does not get clamped! Value is just used for checking overflow
    //mac2 = value & 0xFFF'FFFF'FFFF;
    //if (mac2 & 0x800'0000'0000) {
    //    mac2 |= 0xFFFF'F000'0000'0000;
    //}
    mac2 = value;
}

void GTE::set_mac3(int64_t value) {
    int64_t clamped = std::min(0x7FF'FFFF'FFFF, value);
    if (clamped != value) {
        // positive 44bit overflow
        set_flag(GTE_FLAGS_MAC3_POS_OVERFLOW);
    }
    int64_t temp = clamped;
    clamped = std::max(-0x800'0000'0000, clamped);
    if (clamped != temp) {
        // negative 44bit overflow
        set_flag(GTE_FLAGS_MAC3_NEG_OVERFLOW);
    }
    // MAC3 does not get clamped! Value is just used for checking overflow
    //mac3 = value & 0xFFF'FFFF'FFFF;
    //if (mac3 & 0x800'0000'0000) {
    //    mac3 |= 0xFFFF'F000'0000'0000;
    //}
    mac3 = value;
}

void GTE::set_r2(int64_t value) {
    uint8_t clamped = clamp_to_u8bit(value);
    if (clamped != value) {
        set_flag(GTE_FLAGS_COLOR_QUEUE_R_CLAMPED);
    }
    rgb2 = (rgb2 & 0xFFFF'FF00U) | static_cast<uint32_t>(clamped);
}

void GTE::set_g2(int64_t value) {
    uint8_t clamped = clamp_to_u8bit(value);
    if (clamped != value) {
        set_flag(GTE_FLAGS_COLOR_QUEUE_G_CLAMPED);
    }
    rgb2 = (rgb2 & 0xFFFF'00FFU) | (static_cast<uint32_t>(clamped) << 8);
}

void GTE::set_b2(int64_t value) {
    uint8_t clamped = clamp_to_u8bit(value);
    if (clamped != value) {
        set_flag(GTE_FLAGS_COLOR_QUEUE_B_CLAMPED);
    }
    rgb2 = (rgb2 & 0xFF00'FFFFU) | (static_cast<uint32_t>(clamped) << 16);
}

void GTE::set_c2(int64_t value) {
    rgb2 = (rgb2 & 0x00FF'FFFFU) | (static_cast<uint32_t>(value & 0xFF) << 24);
}

void GTE::UNKCP2() {
    // Currently not used
    //throw exceptions::UnknownFunctionError(std::format("Unknown CP2 opcode @0x{:x}: instruction 0x{:x} = 0b{:032b} (CP2), function 0b{:06b}", instructionPC, instruction, instruction, funct));
}

void GTE::RTPS() {
    // Perspective Transformation (Single)
    LOGT_GTE(std::format("RTPS"));

    lm = false;

    set_mac1(((get_trx() << 12) + get_rt11() * get_vx0() + get_rt12() * get_vy0() + get_rt13() * get_vz0()) >> (sf * 12));
    set_mac2(((get_try() << 12) + get_rt21() * get_vx0() + get_rt22() * get_vy0() + get_rt23() * get_vz0()) >> (sf * 12));
    set_mac3(((get_trz() << 12) + get_rt31() * get_vx0() + get_rt32() * get_vy0() + get_rt33() * get_vz0()) >> (sf * 12));
    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());

    push_sz_queue();
    set_sz3(get_mac3() >> ((1 - sf) * 12));

    push_sxy_queue();
    //int64_t accurate_div_result = (sz3 <= h / 2) ? 0x1'FFFF : (((get_h() * 0x2'0000) / get_sz3()) + 1) / 2;
    int64_t unr_div_result = static_cast<int64_t>(unr_division(h, sz3));
    int64_t division = unr_div_result;

    set_mac0(division * get_ir1() + get_ofx());
    set_sx2(get_mac0() >> 16);
    set_mac0(division * get_ir2() + get_ofy());
    set_sy2(get_mac0() >> 16);
    set_mac0(division * get_dqa() + get_dqb());
    set_ir0(get_mac0() >> 12);
}

void GTE::RTPT() {
    // Perspective Transformation (Triple)
    LOGT_GTE(std::format("RTPT"));

    // V0
    set_mac1(((get_trx() << 12) + get_rt11() * get_vx0() + get_rt12() * get_vy0() + get_rt13() * get_vz0()) >> (sf * 12));
    set_mac2(((get_try() << 12) + get_rt21() * get_vx0() + get_rt22() * get_vy0() + get_rt23() * get_vz0()) >> (sf * 12));
    set_mac3(((get_trz() << 12) + get_rt31() * get_vx0() + get_rt32() * get_vy0() + get_rt33() * get_vz0()) >> (sf * 12));
    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());

    push_sz_queue();
    set_sz3(get_mac3() >> ((1 - sf) * 12));

    push_sxy_queue();
    //int64_t accurate_div_result = (sz3 <= h / 2) ? 0x1'FFFF : (((get_h() * 0x2'0000) / get_sz3()) + 1) / 2;
    int64_t unr_div_result = static_cast<int64_t>(unr_division(h, sz3));
    int64_t division = unr_div_result;

    set_mac0(division * get_ir1() + get_ofx());
    set_sx2(get_mac0() >> 16);
    set_mac0(division * get_ir2() + get_ofy());
    set_sy2(get_mac0() >> 16);
    //set_mac0(division * get_dqa() + get_dqb());
    //set_ir0(get_mac0() >> 12);

    // V1
    set_mac1(((get_trx() << 12) + get_rt11() * get_vx1() + get_rt12() * get_vy1() + get_rt13() * get_vz1()) >> (sf * 12));
    set_mac2(((get_try() << 12) + get_rt21() * get_vx1() + get_rt22() * get_vy1() + get_rt23() * get_vz1()) >> (sf * 12));
    set_mac3(((get_trz() << 12) + get_rt31() * get_vx1() + get_rt32() * get_vy1() + get_rt33() * get_vz1()) >> (sf * 12));
    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());

    push_sz_queue();
    set_sz3(get_mac3() >> ((1 - sf) * 12));

    push_sxy_queue();
    //accurate_div_result = (sz3 <= h / 2) ? 0x1'FFFF : (((get_h() * 0x2'0000) / get_sz3()) + 1) / 2;
    unr_div_result = static_cast<int64_t>(unr_division(h, sz3));
    division = unr_div_result;

    set_mac0(division * get_ir1() + get_ofx());
    set_sx2(get_mac0() >> 16);
    set_mac0(division * get_ir2() + get_ofy());
    set_sy2(get_mac0() >> 16);
    //set_mac0(division * get_dqa() + get_dqb());
    //set_ir0(get_mac0() >> 12);

    // V2
    set_mac1(((get_trx() << 12) + get_rt11() * get_vx2() + get_rt12() * get_vy2() + get_rt13() * get_vz2()) >> (sf * 12));
    set_mac2(((get_try() << 12) + get_rt21() * get_vx2() + get_rt22() * get_vy2() + get_rt23() * get_vz2()) >> (sf * 12));
    set_mac3(((get_trz() << 12) + get_rt31() * get_vx2() + get_rt32() * get_vy2() + get_rt33() * get_vz2()) >> (sf * 12));
    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());

    push_sz_queue();
    set_sz3(get_mac3() >> ((1 - sf) * 12));

    push_sxy_queue();
    //accurate_div_result = (sz3 <= h / 2) ? 0x1'FFFF : (((get_h() * 0x2'0000) / get_sz3()) + 1) / 2;
    unr_div_result = static_cast<int64_t>(unr_division(h, sz3));
    division = unr_div_result;

    set_mac0(division * get_ir1() + get_ofx());
    set_sx2(get_mac0() >> 16);
    set_mac0(division * get_ir2() + get_ofy());
    set_sy2(get_mac0() >> 16);
    set_mac0(division * get_dqa() + get_dqb());
    set_ir0(get_mac0() >> 12);
}

void GTE::MVMVA() {
    LOG_GTE(std::format("Unimplemented command: MVMVA"));
    //TODO
}

void GTE::DCPL() {
    LOG_GTE(std::format("Unimplemented command: DCPL"));
    //TODO
}

void GTE::DPCS() {
    LOG_GTE(std::format("Unimplemented command: DPCS"));
    //TODO
}

void GTE::DPCT() {
    LOG_GTE(std::format("Unimplemented command: DPCT"));
    //TODO
}

void GTE::INTPL() {
    LOG_GTE(std::format("Unimplemented command: INTPL"));
    //TODO
}

void GTE::SQR() {
    LOGT_GTE(std::format("SQR"));

    set_mac1((get_ir1() * get_ir1()) >> (sf * 12));
    set_mac2((get_ir2() * get_ir2()) >> (sf * 12));
    set_mac3((get_ir3() * get_ir3()) >> (sf * 12));

    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());
}


void GTE::NCS() {
    // Normal Color (Single)
    LOGT_GTE(std::format("NCS"));

    set_mac1((get_l11() * get_vx0() + get_l12() * get_vy0() + get_l13() * get_vz0()) >> (sf * 12));
    set_mac2((get_l21() * get_vx0() + get_l22() * get_vy0() + get_l23() * get_vz0()) >> (sf * 12));
    set_mac3((get_l31() * get_vx0() + get_l32() * get_vy0() + get_l33() * get_vz0()) >> (sf * 12));
    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());

    set_mac1(((get_rbk() << 12) + get_lr1() * get_ir1() + get_lr2() * get_ir2() + get_lr3() * get_ir3()) >> (sf * 12));
    set_mac2(((get_gbk() << 12) + get_lg1() * get_ir1() + get_lg2() * get_ir2() + get_lg3() * get_ir3()) >> (sf * 12));
    set_mac3(((get_bbk() << 12) + get_lb1() * get_ir1() + get_lb2() * get_ir2() + get_lb3() * get_ir3()) >> (sf * 12));
    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());

    push_color_queue();
    set_r2(get_mac1() / 16);
    set_g2(get_mac2() / 16);
    set_b2(get_mac3() / 16);
    set_c2(get_c());

    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());
}

void GTE::NCT() {
    LOG_GTE(std::format("Unimplemented command: NCT"));
    //TODO
}

void GTE::NCDS() {
    // Normal Color Depth Cue (Single vector)
    LOGT_GTE(std::format("NCDS"));

    set_mac1((get_l11() * get_vx0() + get_l12() * get_vy0() + get_l13() * get_vz0()) >> (sf * 12));
    set_mac2((get_l21() * get_vx0() + get_l22() * get_vy0() + get_l23() * get_vz0()) >> (sf * 12));
    set_mac3((get_l31() * get_vx0() + get_l32() * get_vy0() + get_l33() * get_vz0()) >> (sf * 12));
    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());

    set_mac1(((get_rbk() << 12) + get_lr1() * get_ir1() + get_lr2() * get_ir2() + get_lr3() * get_ir3()) >> (sf * 12));
    set_mac2(((get_gbk() << 12) + get_lg1() * get_ir1() + get_lg2() * get_ir2() + get_lg3() * get_ir3()) >> (sf * 12));
    set_mac3(((get_bbk() << 12) + get_lb1() * get_ir1() + get_lb2() * get_ir2() + get_lb3() * get_ir3()) >> (sf * 12));
    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());

    set_mac1((get_r() * get_ir1()) << 4);
    set_mac2((get_g() * get_ir2()) << 4);
    set_mac3((get_b() * get_ir3()) << 4);

    set_mac1(get_mac1() + (get_rfc() - get_mac1()) * get_ir0());
    set_mac2(get_mac2() + (get_gfc() - get_mac2()) * get_ir0());
    set_mac3(get_mac3() + (get_bfc() - get_mac3()) * get_ir0());

    set_mac1(get_mac1() >> (sf * 12));
    set_mac2(get_mac2() >> (sf * 12));
    set_mac3(get_mac3() >> (sf * 12));

    push_color_queue();
    set_r2(get_mac1() / 16);
    set_g2(get_mac2() / 16);
    set_b2(get_mac3() / 16);
    set_c2(get_c());

    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());
}

void GTE::NCDT() {
    LOG_GTE(std::format("Unimplemented command: NCDT"));
    //TODO
}

void GTE::NCCS() {
    LOG_GTE(std::format("Unimplemented command: NCCS"));
    //TODO
}

void GTE::NCCT() {
    LOG_GTE(std::format("Unimplemented command: NCCT"));
    //TODO
}

void GTE::CDP() {
    LOG_GTE(std::format("Unimplemented command: CDP"));
    //TODO
}

void GTE::CC() {
    LOG_GTE(std::format("Unimplemented command: CC"));
    //TODO
}

void GTE::NCLIP() {
    // Normal Clipping
    LOGT_GTE(std::format("NCLIP"));
    set_mac0(get_sx0() * (get_sy1() - get_sy2()) + get_sx1() * (get_sy2() - get_sy0()) + get_sx2() * (get_sy0() - get_sy1()));
}

void GTE::AVSZ3() {
    // Average of three Z values
    LOGT_GTE(std::format("AVSZ3"));

    set_mac0(get_zsf3() * (get_sz1() + get_sz2() + get_sz3()));
    set_otz(get_mac0() >> 12);
}

void GTE::AVSZ4() {
    // Average of four Z values
    LOGT_GTE(std::format("AVSZ4"));

    set_mac0(get_zsf4() * (get_sz0() + get_sz1() + get_sz2() + get_sz3()));
    set_otz(get_mac0() >> 12);
}

void GTE::OP() {
    LOGT_GTE(std::format("OP"));

    set_mac1((get_ir3() * get_rt22() - get_ir2() * get_rt33()) >> (sf * 12));
    set_mac2((get_ir1() * get_rt33() - get_ir3() * get_rt11()) >> (sf * 12));
    set_mac3((get_ir2() * get_rt11() - get_ir1() * get_rt22()) >> (sf * 12));

    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());
}

void GTE::GPF() {
    LOGT_GTE(std::format("GPF"));

    set_mac1((get_ir1() * get_ir0()) >> (sf * 12));
    set_mac2((get_ir2() * get_ir0()) >> (sf * 12));
    set_mac3((get_ir3() * get_ir0()) >> (sf * 12));

    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());

    push_color_queue();
    set_r2(get_mac1() >> 4);
    set_g2(get_mac2() >> 4);
    set_b2(get_mac3() >> 4);
    set_c2(get_c());
}

void GTE::GPL() {
    LOG_GTE(std::format("GPL"));

    set_mac1(get_mac1() << (sf * 12));
    set_mac2(get_mac2() << (sf * 12));
    set_mac3(get_mac3() << (sf * 12));

    set_mac1((get_ir1() * get_ir0() + get_mac1()) >> (sf * 12));
    set_mac2((get_ir2() * get_ir0() + get_mac2()) >> (sf * 12));
    set_mac3((get_ir3() * get_ir0() + get_mac3()) >> (sf * 12));

    //set_mac1(get_ir1() * get_ir0() + get_mac1());
    //set_mac2(get_ir2() * get_ir0() + get_mac2());
    //set_mac3(get_ir3() * get_ir0() + get_mac3());

    //set_mac1(get_mac1() >> (sf * 12));
    //set_mac2(get_mac2() >> (sf * 12));
    //set_mac3(get_mac3() >> (sf * 12));

    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());

    push_color_queue();
    set_r2(get_mac1() / 16);
    set_g2(get_mac2() / 16);
    set_b2(get_mac3() / 16);
    set_c2(get_c());
}

void GTE::UNOFF() {
    LOG_GTE(std::format("Unimplemented command: UNOFF"));
    //TODO
}

}
