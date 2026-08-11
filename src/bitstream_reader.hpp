#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace bitstream_decoder {

// Reader for the bit ordering used by RakNet::BitStream.
// The first logical bit is stored at bit 7 (MSB) of the backing byte.
// RakNet ReadBits(..., true) right-aligns a final partial byte, while
// multi-byte native values are kept in the platform's native byte order.
// This project targets the original 32-bit Windows/SA-MP environment.
class BitStreamReader {
public:
    BitStreamReader(const std::uint8_t* data, std::size_t size_bytes, std::size_t bit_count)
        : data_(data), size_bytes_(size_bytes), bit_count_(bit_count) {
        if (data_ == nullptr && bit_count_ != 0)
            throw std::invalid_argument("BitStreamReader: null data");
        if (bit_count_ > size_bytes_ * 8)
            throw std::invalid_argument("BitStreamReader: bit count exceeds buffer");
    }

    explicit BitStreamReader(const std::vector<std::uint8_t>& data, std::size_t bit_count)
        : BitStreamReader(data.data(), data.size(), bit_count) {}

    std::size_t position() const { return bit_offset_; }
    std::size_t remaining() const { return bit_count_ - bit_offset_; }
    std::size_t size_bits() const { return bit_count_; }

    void align_to_byte() {
        const std::size_t remainder = bit_offset_ & 7u;
        if (remainder != 0)
            skip(8u - remainder);
    }

    void skip(std::size_t bits) {
        require(bits);
        bit_offset_ += bits;
    }

    // Equivalent to RakNet ReadBits(..., true) for a native little-endian
    // Windows target. For N bits, the logical bits are first materialized
    // into bytes exactly as ReadBits does; a partial final byte is right
    // aligned, then the byte sequence is interpreted as a little-endian
    // integer.
    std::uint64_t read_bits(std::size_t bits) {
        if (bits > 64)
            throw std::invalid_argument("BitStreamReader: cannot read more than 64 bits");
        require(bits);
        if (bits == 0)
            return 0;

        std::uint8_t out[8]{};
        const std::size_t out_bytes = (bits + 7u) / 8u;

        for (std::size_t i = 0; i < bits; ++i) {
            const std::size_t absolute_bit = bit_offset_ + i;
            const std::size_t source_byte = absolute_bit >> 3u;
            const unsigned source_bit = static_cast<unsigned>(absolute_bit & 7u);
            const std::uint8_t bit = static_cast<std::uint8_t>(
                (data_[source_byte] >> (7u - source_bit)) & 1u);

            const std::size_t destination_byte = i >> 3u;
            const unsigned destination_bit = static_cast<unsigned>(i & 7u);
            out[destination_byte] |= static_cast<std::uint8_t>(
                bit << (7u - destination_bit));
        }

        // RakNet ReadBits(..., true) right-aligns a partial final byte.
        const unsigned remainder = static_cast<unsigned>(bits & 7u);
        if (remainder != 0)
            out[out_bytes - 1] >>= (8u - remainder);

        std::uint64_t value = 0;
        for (std::size_t i = 0; i < out_bytes; ++i)
            value |= static_cast<std::uint64_t>(out[i]) << (i * 8u);

        bit_offset_ += bits;
        return value;
    }

    bool read_bool() { return read_bits(1) != 0; }
    std::uint8_t read_u8() { return static_cast<std::uint8_t>(read_bits(8)); }
    std::uint16_t read_u16() { return static_cast<std::uint16_t>(read_bits(16)); }
    std::uint32_t read_u32() { return static_cast<std::uint32_t>(read_bits(32)); }
    std::uint64_t read_u64() { return read_bits(64); }

    std::int16_t read_i16() {
        return static_cast<std::int16_t>(read_u16());
    }

    std::int32_t read_i32() {
        return static_cast<std::int32_t>(read_u32());
    }

    float read_f32() {
        const std::uint32_t raw = read_u32();
        float value{};
        static_assert(sizeof(value) == sizeof(raw), "unexpected float size");
        std::memcpy(&value, &raw, sizeof(value));
        return value;
    }

private:
    void require(std::size_t bits) const {
        if (bits > remaining()) {
            throw std::out_of_range(
                "BitStreamReader: read past end at bit " + std::to_string(bit_offset_) +
                " (requested " + std::to_string(bits) + ", remaining " +
                std::to_string(remaining()) + ")");
        }
    }

    const std::uint8_t* data_;
    std::size_t size_bytes_;
    std::size_t bit_count_;
    std::size_t bit_offset_ = 0;
};

} // namespace bitstream_decoder
