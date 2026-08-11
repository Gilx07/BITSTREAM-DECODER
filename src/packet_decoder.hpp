#pragma once

#include "bitstream_reader.hpp"

#include <cstdint>
#include <stdexcept>

namespace bitstream_decoder {

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Quaternion {
    float w;
    float x;
    float y;
    float z;
};

struct PlayerSync207 {
    std::uint8_t packet_id;
    std::uint16_t lr_key;
    std::uint16_t ud_key;
    std::uint16_t keys;
    Vec3 position;
    Quaternion quaternion;
    std::uint8_t health;
    std::uint8_t armour;
    std::uint8_t additional_key;
    std::uint8_t weapon_id;
    std::uint8_t special_action;
    Vec3 velocity;
    Vec3 surfing_offsets;
    std::uint16_t surfing_vehicle_id;
    std::int16_t animation_id;
    std::int16_t animation_flags;
};

inline PlayerSync207 decode_player_sync_207(const std::uint8_t* data,
                                             std::size_t data_size,
                                             std::size_t bit_count) {
    if (bit_count != 552)
        throw std::invalid_argument("ID_PLAYER_SYNC requires exactly 552 bits");
    if (data_size < 69)
        throw std::invalid_argument("ID_PLAYER_SYNC requires at least 69 bytes");

    BitStreamReader bs(data, data_size, bit_count);
    PlayerSync207 out{};

    out.packet_id = bs.read_u8();
    if (out.packet_id != 207)
        throw std::invalid_argument("payload is not ID_PLAYER_SYNC (207)");

    out.lr_key = bs.read_u16();
    out.ud_key = bs.read_u16();
    out.keys = bs.read_u16();

    out.position = {bs.read_f32(), bs.read_f32(), bs.read_f32()};
    out.quaternion = {bs.read_f32(), bs.read_f32(), bs.read_f32(), bs.read_f32()};

    out.health = bs.read_u8();
    out.armour = bs.read_u8();

    // SA-MP packs these fields into one byte as 2 bits + 6 bits.
    out.additional_key = static_cast<std::uint8_t>(bs.read_bits(2));
    out.weapon_id = static_cast<std::uint8_t>(bs.read_bits(6));

    out.special_action = bs.read_u8();

    out.velocity = {bs.read_f32(), bs.read_f32(), bs.read_f32()};
    out.surfing_offsets = {bs.read_f32(), bs.read_f32(), bs.read_f32()};
    out.surfing_vehicle_id = bs.read_u16();
    out.animation_id = bs.read_i16();
    out.animation_flags = bs.read_i16();

    if (bs.position() != 552)
        throw std::logic_error("ID_PLAYER_SYNC decoder consumed an unexpected number of bits");

    return out;
}

} // namespace bitstream_decoder
