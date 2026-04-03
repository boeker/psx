#include "gte.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <sstream>

#include "util/log.h"

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


GTE::GTE() {
    reset();
}

void GTE::reset() {
    v0.reset();
    v1.reset();
    v2.reset();
    rgbc = 0;
    ordering_table_z = 0;
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
    leading_zeros_count_source = 0;
    leading_zeros_count_result = 0;

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
    screen_offset[0] = 0;
    screen_offset[1] = 0;
    projection_plane_distance = 0;
    depth_cueing_coefficient = 0;
    depth_cueing_offset = 0;
    average_z_scale_factors[0] = 0;
    average_z_scale_factors[1] = 0;
    flags = 0;

    instruction = 0;
    lm = false;
    sf = false;
    funct = 0;
}

void GTE::reset_flags() {
    flags = 0;
}

void GTE::update_error_flag() {
    uint32_t bits_in_error_flag = 0x7F87'E000; // 30 to 23, 18 to 13
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
            return ordering_table_z;
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
            return leading_zeros_count_source;
        case GTE_REG_LZCR:
            return std::max(std::countl_one(leading_zeros_count_source), std::countl_zero(leading_zeros_count_source));
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
            // Read-only
            break;
        case GTE_REG_IR0:
            ir0 = value;
            break;
        case GTE_REG_IR1:
            set_ir1_without_clamping(value);
            break;
        case GTE_REG_IR2:
            set_ir2_without_clamping(value);
            break;
        case GTE_REG_IR3:
            set_ir3_without_clamping(value);
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
            sxy0 = sxy1;
            sxy1 = sxy2;
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
            leading_zeros_count_source = value;
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
            return screen_offset[0];
        case GTE_REG_OFY:
            return screen_offset[1];
        case GTE_REG_H:
            // Hardware bug: sign-extend unsigned value
            return static_cast<int32_t>(static_cast<int16_t>(projection_plane_distance));
        case GTE_REG_DQA:
            return static_cast<int32_t>(depth_cueing_coefficient);
        case GTE_REG_DQB:
            return depth_cueing_offset;
        case GTE_REG_ZSF3:
            return static_cast<int32_t>(average_z_scale_factors[0]);
        case GTE_REG_ZSF4:
            return static_cast<int32_t>(average_z_scale_factors[1]);
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
            screen_offset[0] = value;
            break;
        case GTE_REG_OFY:
            screen_offset[1] = value;
            break;
        case GTE_REG_H:
            projection_plane_distance = value & 0xFFFF;
            break;
        case GTE_REG_DQA:
            depth_cueing_coefficient = value;
            break;
        case GTE_REG_DQB:
            depth_cueing_offset = value;
            break;
        case GTE_REG_ZSF3:
            average_z_scale_factors[0] = value;
            break;
        case GTE_REG_ZSF4:
            average_z_scale_factors[1] = value;
            break;
        case GTE_REG_FLAGS:
            flags = value & 0x7FFF'F000;
            update_error_flag();
            break;
        default:
            assert(false);
    }
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

int32_t GTE::clamp_to_16bit(int32_t value, bool lm) {
    int32_t clamped = std::min(0x7FFF, value);
    clamped = lm ? std::max(0, clamped) : std::max(-0x8000, clamped);
    return clamped;
}

uint8_t GTE::convert_16bit_to_5bit_color(int32_t color) {
    color = color / 0x80;
    color = std::min(0x1F, color);
    color = std::max(0x00, color);
    return color;
}

void GTE::set_ir1(int32_t value) {
    int32_t clamped = clamp_to_16bit(value, lm);
    if (clamped != value) {
        set_flag(GTE_FLAGS_IR1);
    }
    set_ir1_without_clamping(clamped);
}

void GTE::set_ir2(int32_t value) {
    int32_t clamped = clamp_to_16bit(value, lm);
    if (clamped != value) {
        set_flag(GTE_FLAGS_IR2);
    }
    set_ir2_without_clamping(clamped);
}

void GTE::set_ir3(int32_t value) {
    int32_t clamped = clamp_to_16bit(value, lm);
    if (clamped != value) {
        set_flag(GTE_FLAGS_IR3);
    }
    set_ir3_without_clamping(clamped);
}

void GTE::set_ir1_without_clamping(int16_t value) {
    ir1 = value;
    rgb = (rgb & 0xFFFFFFE0) | convert_16bit_to_5bit_color(value);
}

void GTE::set_ir2_without_clamping(int16_t value) {
    ir2 = value;
    rgb = (rgb & 0xFFFFFC1F) | (convert_16bit_to_5bit_color(value) << 5);
}

void GTE::set_ir3_without_clamping(int16_t value) {
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

void GTE::set_mac0(int64_t value) {
    mac0 = std::min(static_cast<int64_t>(0x7FFF'FFFF), value);
    if (mac0 != value) {
        // positive 32bit overflow
        set_flag(GTE_FLAGS_MAC0_POS_OVERFLOW);
    }
    int64_t temp = mac0;
    mac0 = std::max(static_cast<int64_t>(-0x8000'0000), mac0);
    if (mac0 != temp) {
        // negative 32bit overflow
        set_flag(GTE_FLAGS_MAC0_NEG_OVERFLOW);
    }
}

void GTE::set_mac1(int64_t value) {
    mac1 = std::min(0x7FF'FFFF'FFFF, value);
    if (mac1 != value) {
        // positive 44bit overflow
        set_flag(GTE_FLAGS_MAC1_POS_OVERFLOW);
    }
    int64_t temp = mac1;
    mac1 = std::max(-0x800'0000'0000, mac1);
    if (mac1 != temp) {
        // negative 44bit overflow
        set_flag(GTE_FLAGS_MAC1_NEG_OVERFLOW);
    }
}

void GTE::set_mac2(int64_t value) {
    mac2 = std::min(0x7FF'FFFF'FFFF, value);
    if (mac2 != value) {
        // positive 44bit overflow
        set_flag(GTE_FLAGS_MAC2_POS_OVERFLOW);
    }
    int64_t temp = mac2;
    mac2 = std::max(-0x800'0000'0000, mac2);
    if (mac2 != temp) {
        // negative 44bit overflow
        set_flag(GTE_FLAGS_MAC2_NEG_OVERFLOW);
    }
}

void GTE::set_mac3(int64_t value) {
    mac3 = std::min(0x7FF'FFFF'FFFF, value);
    if (mac3 != value) {
        // positive 44bit overflow
        set_flag(GTE_FLAGS_MAC3_POS_OVERFLOW);
    }
    int64_t temp = mac3;
    mac3 = std::max(-0x800'0000'0000, mac3);
    if (mac3 != temp) {
        // negative 44bit overflow
        set_flag(GTE_FLAGS_MAC3_NEG_OVERFLOW);
    }
}

int64_t GTE::get_sx0() const {
    return sxy0.x;
}

int64_t GTE::get_sy0() const {
    return sxy0.y;
}

int64_t GTE::get_sx1() const {
    return sxy1.x;
}

int64_t GTE::get_sy1() const {
    return sxy1.y;
}

int64_t GTE::get_sx2() const {
    return sxy2.x;
}

int64_t GTE::get_sy2() const {
    return sxy2.y;
}

int64_t GTE::get_ir1() const {
    return ir1;
}

int64_t GTE::get_ir2() const {
    return ir2;
}

int64_t GTE::get_ir3() const {
    return ir3;
}

int64_t GTE::get_mac0() const {
    return mac0;
}

int64_t GTE::get_mac1() const {
    return mac1;
}

int64_t GTE::get_mac2() const {
    return mac2;
}

int64_t GTE::get_mac3() const {
    return mac3;
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

void GTE::UNKCP2() {
    // Currently not used
    //throw exceptions::UnknownFunctionError(std::format("Unknown CP2 opcode @0x{:x}: instruction 0x{:x} = 0b{:032b} (CP2), function 0b{:06b}", instructionPC, instruction, instruction, funct));
}

void GTE::NCLIP() {
    // Normal Clipping
    LOG_GTE(std::format("NCLIP"));
    set_mac0(get_sx0() * (get_sy1() - get_sy2()) + get_sx1() * (get_sy2() - get_sy0()) + get_sx2() * (get_sy0() - get_sy1()));
}

void GTE::RTPS() {
    // Perspective Transformation (Single)
    LOGT_GTE(std::format("RTPS"));
    // TODO Implement flags
    //uint8_t sf = 0x1 & (instruction >> 19);

    //// Inputs
    //int16_t in_vx0 = getRegister(GTE_REG_VXY0) & 0xFFFF;
    //int16_t in_vy0 = getRegister(GTE_REG_VXY0) >> 16;
    //int16_t in_vz0 = getRegister(GTE_REG_VZ0) & 0xFFFF;

    //int32_t in_trx = getRegister(GTE_REG_TRX);
    //int32_t in_try = getRegister(GTE_REG_TRY);
    //int32_t in_trz = getRegister(GTE_REG_TRZ);

    //int16_t in_rt11 = getRegister(GTE_REG_RT11RT12) & 0xFFFF;
    //int16_t in_rt12 = getRegister(GTE_REG_RT11RT12) >> 16;
    //int16_t in_rt13 = getRegister(GTE_REG_RT13RT21) & 0xFFFF;
    //int16_t in_rt21 = getRegister(GTE_REG_RT13RT21) >> 16;
    //int16_t in_rt22 = getRegister(GTE_REG_RT22RT23) & 0xFFFF;
    //int16_t in_rt23 = getRegister(GTE_REG_RT22RT23) >> 16;
    //int16_t in_rt31 = getRegister(GTE_REG_RT31RT32) & 0xFFFF;
    //int16_t in_rt32 = getRegister(GTE_REG_RT31RT32) >> 16;
    //int16_t in_rt33 = getRegister(GTE_REG_RT33) & 0xFFFF;

    //uint16_t in_h = getRegister(GTE_REG_H);
    //int32_t in_ofx = getRegister(GTE_REG_OFX);
    //int32_t in_ofy = getRegister(GTE_REG_OFY);
    //int16_t in_dqa = getRegister(GTE_REG_DQA) & 0xFFFF;
    //int32_t in_dqb = getRegister(GTE_REG_DQA) & 0xFFFF;

    //int32_t temp_mac1 = (in_trx * 0x1000 + in_rt11 * in_vx0 + in_rt12 * in_vy0 + in_rt13 * in_vz0) >> (sf * 12);
    //int16_t temp_ir1 = temp_mac1;
    //int32_t temp_mac2 = (in_try * 0x1000 + in_rt21 * in_vx0 + in_rt22 * in_vy0 + in_rt23 * in_vz0) >> (sf * 12);
    //int16_t temp_ir2 = temp_mac2;
    //int32_t temp_mac3 = (in_trz * 0x1000 + in_rt31 * in_vx0 + in_rt32 * in_vy0 + in_rt33 * in_vz0) >> (sf * 12);
    //int16_t temp_ir3 = temp_mac3;
    //uint16_t temp_sz3 = temp_mac3 >> ((1-sf) * 12);

    //int32_t temp = temp_sz3 > 0 ? ((in_h * 0x20000) / temp_sz3 + 1) / 2 : 0;

    //int32_t temp_mac0 = temp * temp_ir1 + in_ofx;
    //int16_t temp_sx2 = temp_mac0 / 0x10000;

    //temp_mac0 = temp * temp_ir2 + in_ofy;
    //int16_t temp_sy2 = temp_mac0 / 0x10000;

    //temp_mac0 = temp * in_dqa + in_dqb;
    //int16_t temp_ir0 = temp_mac0 / 0x1000;

    //setRegister(GTE_REG_MAC0, temp_mac0);
    //setRegister(GTE_REG_MAC1, temp_mac1);
    //setRegister(GTE_REG_MAC2, temp_mac2);
    //setRegister(GTE_REG_MAC3, temp_mac3);
    //setRegister(GTE_REG_IR0, temp_ir0);
    //setRegister(GTE_REG_IR1, temp_ir1);
    //setRegister(GTE_REG_IR2, temp_ir2);
    //setRegister(GTE_REG_IR3, temp_ir3);

    //setRegister(GTE_REG_SXY0, getRegister(GTE_REG_SXY1));
    //setRegister(GTE_REG_SXY1, getRegister(GTE_REG_SXY2));
    //setRegister(GTE_REG_SXY2, (((uint32_t)temp_sx2) << 16) | ((uint32_t)temp_sy2));
    //setRegister(GTE_REG_SXYP, (((uint32_t)temp_sx2) << 16) | ((uint32_t)temp_sy2));
    //setRegister(GTE_REG_SZ0, getRegister(GTE_REG_SZ1));
    //setRegister(GTE_REG_SZ1, getRegister(GTE_REG_SZ2));
    //setRegister(GTE_REG_SZ2, getRegister(GTE_REG_SZ3));
    //setRegister(GTE_REG_SZ3, temp_sz3);
}

void GTE::RTPT() {
    // Perspective Transformation (Triple)
    LOGT_GTE(std::format("RTPT"));
    // TODO Implement flags
    //uint8_t sf = 0x1 & (instruction >> 19);

    //// Inputs
    //int16_t in_vx0 = getRegister(GTE_REG_VXY0) & 0xFFFF;
    //int16_t in_vy0 = getRegister(GTE_REG_VXY0) >> 16;
    //int16_t in_vz0 = getRegister(GTE_REG_VZ0) & 0xFFFF;

    //int16_t in_vx1 = getRegister(GTE_REG_VXY1) & 0xFFFF;
    //int16_t in_vy1 = getRegister(GTE_REG_VXY1) >> 16;
    //int16_t in_vz1 = getRegister(GTE_REG_VZ1) & 0xFFFF;

    //int16_t in_vx2 = getRegister(GTE_REG_VXY2) & 0xFFFF;
    //int16_t in_vy2 = getRegister(GTE_REG_VXY2) >> 16;
    //int16_t in_vz2 = getRegister(GTE_REG_VZ2) & 0xFFFF;

    //int32_t in_trx = getRegister(GTE_REG_TRX);
    //int32_t in_try = getRegister(GTE_REG_TRY);
    //int32_t in_trz = getRegister(GTE_REG_TRZ);

    //int16_t in_rt11 = getRegister(GTE_REG_RT11RT12) & 0xFFFF;
    //int16_t in_rt12 = getRegister(GTE_REG_RT11RT12) >> 16;
    //int16_t in_rt13 = getRegister(GTE_REG_RT13RT21) & 0xFFFF;
    //int16_t in_rt21 = getRegister(GTE_REG_RT13RT21) >> 16;
    //int16_t in_rt22 = getRegister(GTE_REG_RT22RT23) & 0xFFFF;
    //int16_t in_rt23 = getRegister(GTE_REG_RT22RT23) >> 16;
    //int16_t in_rt31 = getRegister(GTE_REG_RT31RT32) & 0xFFFF;
    //int16_t in_rt32 = getRegister(GTE_REG_RT31RT32) >> 16;
    //int16_t in_rt33 = getRegister(GTE_REG_RT33) & 0xFFFF;

    //uint16_t in_h = getRegister(GTE_REG_H);
    //int32_t in_ofx = getRegister(GTE_REG_OFX);
    //int32_t in_ofy = getRegister(GTE_REG_OFY);
    //int16_t in_dqa = getRegister(GTE_REG_DQA) & 0xFFFF;
    //int32_t in_dqb = getRegister(GTE_REG_DQA) & 0xFFFF;

    //int32_t temp_mac1 = (in_trx * 0x1000 + in_rt11 * in_vx0 + in_rt12 * in_vy0 + in_rt13 * in_vz0) >> (sf * 12);
    //int16_t temp_ir1 = temp_mac1;
    //int32_t temp_mac2 = (in_try * 0x1000 + in_rt21 * in_vx0 + in_rt22 * in_vy0 + in_rt23 * in_vz0) >> (sf * 12);
    //int16_t temp_ir2 = temp_mac2;
    //int32_t temp_mac3 = (in_trz * 0x1000 + in_rt31 * in_vx0 + in_rt32 * in_vy0 + in_rt33 * in_vz0) >> (sf * 12);
    //int16_t temp_ir3 = temp_mac3;
    //uint16_t temp_sz1 = temp_mac3 >> ((1-sf) * 12);
    //int32_t temp = temp_sz1 > 0 ? ((in_h * 0x20000) / temp_sz1 + 1) / 2 : 0;
    //int32_t temp_mac0 = temp * temp_ir1 + in_ofx;
    //int16_t temp_sx0 = temp_mac0 / 0x10000;
    //temp_mac0 = temp * temp_ir2 + in_ofy;
    //int16_t temp_sy0 = temp_mac0 / 0x10000;
    //temp_mac0 = temp * in_dqa + in_dqb;
    //int16_t temp_ir0 = temp_mac0 / 0x1000;

    //temp_mac1 = (in_trx * 0x1000 + in_rt11 * in_vx1 + in_rt12 * in_vy1 + in_rt13 * in_vz1) >> (sf * 12);
    //temp_ir1 = temp_mac1;
    //temp_mac2 = (in_try * 0x1000 + in_rt21 * in_vx1 + in_rt22 * in_vy1 + in_rt23 * in_vz1) >> (sf * 12);
    //temp_ir2 = temp_mac2;
    //temp_mac3 = (in_trz * 0x1000 + in_rt31 * in_vx1 + in_rt32 * in_vy1 + in_rt33 * in_vz1) >> (sf * 12);
    //temp_ir3 = temp_mac3;
    //uint16_t temp_sz2 = temp_mac3 >> ((1-sf) * 12);
    //temp = temp_sz2 > 0 ? ((in_h * 0x20000) / temp_sz2 + 1) / 2 : 0;
    //temp_mac0 = temp * temp_ir1 + in_ofx;
    //int16_t temp_sx1 = temp_mac0 / 0x10000;
    //temp_mac0 = temp * temp_ir2 + in_ofy;
    //int16_t temp_sy1 = temp_mac0 / 0x10000;
    //temp_mac0 = temp * in_dqa + in_dqb;
    //temp_ir0 = temp_mac0 / 0x1000;

    //temp_mac1 = (in_trx * 0x1000 + in_rt11 * in_vx2 + in_rt12 * in_vy2 + in_rt13 * in_vz2) >> (sf * 12);
    //temp_ir1 = temp_mac1;
    //temp_mac2 = (in_try * 0x1000 + in_rt21 * in_vx2 + in_rt22 * in_vy2 + in_rt23 * in_vz2) >> (sf * 12);
    //temp_ir2 = temp_mac2;
    //temp_mac3 = (in_trz * 0x1000 + in_rt31 * in_vx2 + in_rt32 * in_vy2 + in_rt33 * in_vz2) >> (sf * 12);
    //temp_ir3 = temp_mac3;
    //uint16_t temp_sz3 = temp_mac3 >> ((1-sf) * 12);
    //temp = temp_sz3 > 0 ? ((in_h * 0x20000) / temp_sz3 + 1) / 2 : 0;
    //temp_mac0 = temp * temp_ir1 + in_ofx;
    //int16_t temp_sx2 = temp_mac0 / 0x10000;
    //temp_mac0 = temp * temp_ir2 + in_ofy;
    //int16_t temp_sy2 = temp_mac0 / 0x10000;
    //temp_mac0 = temp * in_dqa + in_dqb;
    //temp_ir0 = temp_mac0 / 0x1000;

    //setRegister(GTE_REG_MAC0, temp_mac0);
    //setRegister(GTE_REG_MAC1, temp_mac1);
    //setRegister(GTE_REG_MAC2, temp_mac2);
    //setRegister(GTE_REG_MAC3, temp_mac3);
    //setRegister(GTE_REG_IR0, temp_ir0);
    //setRegister(GTE_REG_IR1, temp_ir1);
    //setRegister(GTE_REG_IR2, temp_ir2);
    //setRegister(GTE_REG_IR3, temp_ir3);

    //setRegister(GTE_REG_SXY0, (((uint32_t)temp_sx0) << 16) | ((uint32_t)temp_sy0));
    //setRegister(GTE_REG_SXY1, (((uint32_t)temp_sx1) << 16) | ((uint32_t)temp_sy1));
    //setRegister(GTE_REG_SXY2, (((uint32_t)temp_sx2) << 16) | ((uint32_t)temp_sy2));
    //setRegister(GTE_REG_SXYP, (((uint32_t)temp_sx2) << 16) | ((uint32_t)temp_sy2));
    //setRegister(GTE_REG_SZ0, getRegister(GTE_REG_SZ1));
    //setRegister(GTE_REG_SZ1, temp_sz1);
    //setRegister(GTE_REG_SZ2, temp_sz2);
    //setRegister(GTE_REG_SZ3, temp_sz3);
}

void GTE::NCDS() {
    // Normal Color Depth Cue (Single vector)
    // TODO Implement flags
    // TODO Fix IR register values
    LOGT_GTE(std::format("NCDS"));
    //uint8_t sf = 0x1 & (instruction >> 19);

    //int16_t in_vx0 = getRegister(GTE_REG_VXY0) & 0xFFFF;
    //int16_t in_vy0 = getRegister(GTE_REG_VXY0) >> 16;
    //int16_t in_vz0 = getRegister(GTE_REG_VZ0) & 0xFFFF;

    //int16_t in_l11 = getRegister(GTE_REG_L11L12) & 0xFFFF;
    //int16_t in_l12 = getRegister(GTE_REG_L11L12) >> 16;
    //int16_t in_l13 = getRegister(GTE_REG_L13L21) & 0xFFFF;
    //int16_t in_l21 = getRegister(GTE_REG_L13L21) >> 16;
    //int16_t in_l22 = getRegister(GTE_REG_L22L23) & 0xFFFF;
    //int16_t in_l23 = getRegister(GTE_REG_L22L23) >> 16;
    //int16_t in_l31 = getRegister(GTE_REG_L31L32) & 0xFFFF;
    //int16_t in_l32 = getRegister(GTE_REG_L31L32) >> 16;
    //int16_t in_l33 = getRegister(GTE_REG_L33) & 0xFFFF;

    //int16_t in_lr1 = getRegister(GTE_REG_LR1LR2) & 0xFFFF;
    //int16_t in_lr2 = getRegister(GTE_REG_LR1LR2) >> 16;
    //int16_t in_lr3 = getRegister(GTE_REG_LR3LG1) & 0xFFFF;
    //int16_t in_lg1 = getRegister(GTE_REG_LR3LG1) >> 16;
    //int16_t in_lg2 = getRegister(GTE_REG_LG2LG3) & 0xFFFF;
    //int16_t in_lg3 = getRegister(GTE_REG_LG2LG3) >> 16;
    //int16_t in_lb1 = getRegister(GTE_REG_LB1LB2) & 0xFFFF;
    //int16_t in_lb2 = getRegister(GTE_REG_LB1LB2) >> 16;
    //int16_t in_lb3 = getRegister(GTE_REG_LB3) & 0xFFFF;

    //int32_t in_rbk = getRegister(GTE_REG_RBK);
    //int32_t in_gbk = getRegister(GTE_REG_GBK);
    //int32_t in_bbk = getRegister(GTE_REG_BBK);

    //int32_t in_rfc = getRegister(GTE_REG_RFC);
    //int32_t in_gfc = getRegister(GTE_REG_GFC);
    //int32_t in_bfc = getRegister(GTE_REG_BFC);

    //uint8_t in_r = getRegister(GTE_REG_RGBC) >> 24;
    //uint8_t in_g = (getRegister(GTE_REG_RGBC) >> 16) & 0xFF;
    //uint8_t in_b = (getRegister(GTE_REG_RGBC) >> 8) & 0xFF;
    //uint8_t in_c = getRegister(GTE_REG_RGBC) & 0xFF;

    //int16_t in_ir0 = getRegister(GTE_REG_IR0);

    //int32_t temp_mac1 = (in_l11 * in_vx0 + in_l12 * in_vy0 + in_l13 * in_vz0) >> (sf * 12);
    //int16_t temp_ir1 = temp_mac1;
    //int32_t temp_mac2 = (in_l21 * in_vx0 + in_l22 * in_vy0 + in_l23 * in_vz0) >> (sf * 12);
    //int16_t temp_ir2 = temp_mac2;
    //int32_t temp_mac3 = (in_l31 * in_vx0 + in_l32 * in_vy0 + in_l33 * in_vz0) >> (sf * 12);
    //int16_t temp_ir3 = temp_mac3;

    //temp_mac1 = (in_rbk * 0x1000 + in_lr1 * temp_ir1 + in_lr2 * temp_ir2 + in_lr3 * temp_ir3) >> (sf * 12);
    //temp_ir1 = temp_mac1;
    //temp_mac2 = (in_gbk * 0x1000 + in_lg1 * temp_ir1 + in_lg2 * temp_ir2 + in_lg3 * temp_ir3) >> (sf * 12);
    //temp_ir2 = temp_mac2;
    //temp_mac3 = (in_bbk * 0x1000 + in_lb1 * temp_ir1 + in_lb2 * temp_ir2 + in_lb3 * temp_ir3) >> (sf * 12);
    //temp_ir3 = temp_mac3;

    //temp_mac1 = (in_r * temp_ir1) << 4;
    //temp_mac2 = (in_g * temp_ir1) << 4;
    //temp_mac3 = (in_b * temp_ir1) << 4;

    //temp_mac1 =  temp_mac1 + (in_rfc - temp_mac1) * in_ir0;
    //temp_mac2 =  temp_mac2 + (in_gfc - temp_mac2) * in_ir0;
    //temp_mac3 =  temp_mac3 + (in_bfc - temp_mac3) * in_ir0;

    //temp_mac1 =  temp_mac1 >> (sf * 12);
    //temp_mac2 =  temp_mac2 >> (sf * 12);
    //temp_mac3 =  temp_mac3 >> (sf * 12);

    //uint8_t temp_rgb2 =  ((temp_mac1 / 16) << 24) | ((temp_mac2 / 16) << 16) | ((temp_mac3 / 16) << 8) | in_c;

    //temp_ir1 = temp_mac1;
    //temp_ir2 = temp_mac2;
    //temp_ir3 = temp_mac3;

    //setRegister(GTE_REG_MAC1, temp_mac1);
    //setRegister(GTE_REG_MAC2, temp_mac2);
    //setRegister(GTE_REG_MAC3, temp_mac3);
    //setRegister(GTE_REG_IR1, temp_ir1);
    //setRegister(GTE_REG_IR2, temp_ir2);
    //setRegister(GTE_REG_IR3, temp_ir3);

    //setRegister(GTE_REG_RGB0, getRegister(GTE_REG_RGB1));
    //setRegister(GTE_REG_RGB1, getRegister(GTE_REG_RGB2));
    //setRegister(GTE_REG_RGB2, temp_rgb2);
}

void GTE::AVSZ3() {
    // Average of three Z values
    LOGT_GTE(std::format("AVSZ3"));
    //uint8_t sf = 0x1 & (instruction >> 19);

    //uint16_t in_sz1 = getRegister(GTE_REG_SZ1);
    //uint16_t in_sz2 = getRegister(GTE_REG_SZ2);
    //uint16_t in_sz3 = getRegister(GTE_REG_SZ3);

    //int16_t in_zsf3 = getRegister(GTE_REG_ZSF3) & 0xFFFF;

    //int32_t temp_mac0 = in_zsf3 * (in_sz1 + in_sz2 + in_sz3);
    //uint16_t temp_otz = temp_mac0 / 0x1000;

    //setRegister(GTE_REG_MAC0, temp_mac0);
    //setRegister(GTE_REG_OTZ, temp_otz);
}

void GTE::NCDT() {
    LOGT_GTE(std::format("GTE_NCDT"));
    //TODO
}

void GTE::AVSZ4() {
    LOGT_GTE(std::format("GTE_AVSZ4"));
    //TODO
}

void GTE::SQR() {
    LOG_GTE(std::format("GTE_SQR"));
    LOG_GTE(std::format("ir1 = 0x{:04X}, ir2 = 0x{:04X}, ir3 = 0x{:04X}, sf = {:s}", get_ir1(), get_ir2(), get_ir3(), sf));

    set_mac1((get_ir1() * get_ir1()) >> (sf * 12));
    set_mac2((get_ir2() * get_ir2()) >> (sf * 12));
    set_mac3((get_ir3() * get_ir3()) >> (sf * 12));

    set_ir1(get_mac1());
    set_ir2(get_mac2());
    set_ir3(get_mac3());

    LOG_GTE(std::format("mac1 = 0x{:011X}, mac2 = 0x{:011X}, mac3 = 0x{:011X}", get_mac1(), get_mac2(), get_mac3()));
    LOG_GTE(std::format("ir1 = 0x{:04X}, ir2 = 0x{:04X}, ir3 = 0x{:04X}", get_ir1(), get_ir2(), get_ir3()));
}

void GTE::OP() {
    LOGT_GTE(std::format("GTE_OP"));
    //TODO
}

void GTE::GPF() {
    LOGT_GTE(std::format("GTE_GPF"));
    //TODO
}

void GTE::GPL() {
    LOGT_GTE(std::format("GTE_GPL"));
    //TODO
}

void GTE::NCCS() {
    LOGT_GTE(std::format("GTE_NCCS"));
    //TODO
}

void GTE::NCCT() {
    LOGT_GTE(std::format("GTE_NCCT"));
    //TODO
}

void GTE::NCS() {
    LOGT_GTE(std::format("GTE_NCS"));
    //TODO
}

void GTE::NCT() {
    LOGT_GTE(std::format("GTE_NCT"));
    //TODO
}

void GTE::CC() {
    LOGT_GTE(std::format("GTE_CC"));
    //TODO
}

void GTE::DPCS() {
    LOGT_GTE(std::format("GTE_DPCS"));
    //TODO
}

void GTE::DPCT() {
    LOGT_GTE(std::format("GTE_DPCT"));
    //TODO
}

void GTE::INTPL() {
    LOGT_GTE(std::format("GTE_INTPL"));
    //TODO
}

void GTE::CDP() {
    LOGT_GTE(std::format("GTE_CDP"));
    //TODO
}

void GTE::DCPL() {
    LOGT_GTE(std::format("GTE_DCPL"));
    //TODO
}

void GTE::MVMVA() {
    LOGT_GTE(std::format("GTE_MVMVA"));
    //TODO
}

void GTE::UNOFF() {
    LOGT_GTE(std::format("GTE_UNOFF"));
    //TODO
}

}
