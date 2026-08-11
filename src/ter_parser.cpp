#include "packet_decoder.hpp"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class Reader {
public:
    explicit Reader(const std::string& path) : file_(path, std::ios::binary) {
        if (!file_) throw std::runtime_error("failed to open: " + path);
    }

    template <typename T>
    T read() {
        T value{};
        file_.read(reinterpret_cast<char*>(&value), sizeof(T));
        if (!file_) throw std::runtime_error("unexpected EOF");
        return value;
    }

    std::string read_string(std::size_t n) {
        std::string s(n, '\0');
        if (n) file_.read(s.data(), static_cast<std::streamsize>(n));
        if (!file_) throw std::runtime_error("unexpected EOF while reading string");
        return s;
    }

    std::vector<std::uint8_t> read_bytes(std::size_t n) {
        std::vector<std::uint8_t> data(n);
        if (n) file_.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(n));
        if (!file_) throw std::runtime_error("unexpected EOF while reading payload");
        return data;
    }

    std::uint64_t offset() {
        const auto pos = file_.tellg();
        if (pos < 0) throw std::runtime_error("tellg failed");
        return static_cast<std::uint64_t>(pos);
    }

    std::uint64_t size() {
        const auto current = file_.tellg();
        file_.seekg(0, std::ios::end);
        const auto end = file_.tellg();
        file_.seekg(current);
        if (end < 0 || current < 0) throw std::runtime_error("failed to determine file size");
        return static_cast<std::uint64_t>(end);
    }

private:
    std::ifstream file_;
};

void hex_dump(const std::vector<std::uint8_t>& data) {
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (i) std::cout << ' ';
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<unsigned>(data[i]);
    }
    std::cout << std::dec << '\n';
}

void print_player_sync_207(const bitstream_decoder::PlayerSync207& p) {
    std::cout << "  decoded:      ID_PLAYER_SYNC (207)\n";
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "    lrKey:      " << p.lr_key << '\n';
    std::cout << "    udKey:      " << p.ud_key << '\n';
    std::cout << "    keys:       " << p.keys << '\n';
    std::cout << "    position:   (" << p.position.x << ", " << p.position.y << ", " << p.position.z << ")\n";
    std::cout << "    quaternion: (" << p.quaternion.w << ", " << p.quaternion.x << ", "
              << p.quaternion.y << ", " << p.quaternion.z << ")\n";
    std::cout << "    health:     " << static_cast<unsigned>(p.health) << '\n';
    std::cout << "    armour:     " << static_cast<unsigned>(p.armour) << '\n';
    std::cout << "    additional: " << static_cast<unsigned>(p.additional_key) << '\n';
    std::cout << "    weaponId:   " << static_cast<unsigned>(p.weapon_id) << '\n';
    std::cout << "    special:    " << static_cast<unsigned>(p.special_action) << '\n';
    std::cout << "    velocity:   (" << p.velocity.x << ", " << p.velocity.y << ", " << p.velocity.z << ")\n";
    std::cout << "    surfOffset: (" << p.surfing_offsets.x << ", " << p.surfing_offsets.y << ", " << p.surfing_offsets.z << ")\n";
    std::cout << "    surfVehId:  " << p.surfing_vehicle_id << '\n';
    std::cout << "    animId:     " << p.animation_id << '\n';
    std::cout << "    animFlags:  " << p.animation_flags << '\n';
    std::cout.unsetf(std::ios::floatfield);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: ter_parser <file.ter>\n";
        return 2;
    }

    try {
        Reader r(argv[1]);
        const auto file_size = r.size();

        // The recorder is a 32-bit Windows binary, so its serialized size_t fields are 4 bytes.
        const auto name_len = r.read<std::uint32_t>();
        const auto name = r.read_string(name_len);
        const auto date_len = r.read<std::uint32_t>();
        const auto date = r.read_string(date_len);
        const auto duration = r.read<std::uint64_t>();
        const auto record_count = r.read<std::uint32_t>();

        if (name_len > 1'000'000 || date_len > 1'000'000 || record_count > 1'000'000)
            throw std::runtime_error("sanity check failed");

        std::cout << "FILE: " << argv[1] << '\n';
        std::cout << "fileSize: " << file_size << '\n';
        std::cout << "name: " << name << '\n';
        std::cout << "date: " << date << '\n';
        std::cout << "totalDuration: " << duration << '\n';
        std::cout << "recordCount: " << record_count << '\n';
        std::cout << "headerEnd: 0x" << std::hex << r.offset() << std::dec << "\n\n";

        for (std::uint32_t i = 0; i < record_count; ++i) {
            const auto start = r.offset();
            const auto timestamp = r.read<std::uint64_t>();
            const auto is_rpc = r.read<std::uint8_t>();
            const auto rpc_id = r.read<std::int32_t>();
            const auto reliability = r.read<std::int32_t>();
            const auto data_size = r.read<std::uint32_t>();

            if (data_size > 100'000)
                throw std::runtime_error("record payload exceeds 100 KB");

            const auto data = r.read_bytes(data_size);

            std::cout << "Record #" << i << '\n';
            std::cout << "  offset:       0x" << std::hex << start << std::dec << '\n';
            std::cout << "  timestamp:    " << timestamp << '\n';
            std::cout << "  isRPC:        " << static_cast<unsigned>(is_rpc) << '\n';
            std::cout << "  id:           " << rpc_id << '\n';
            std::cout << "  reliability:  " << reliability << '\n';
            std::cout << "  dataSize:     " << data_size << '\n';
            std::cout << "  data:         ";
            hex_dump(data);

            // The confirmed recorder stores ceil(numberOfBitsUsed / 8) bytes.
            // ID_PLAYER_SYNC (207) is confirmed to consume exactly 552 bits = 69 bytes.
            if (is_rpc == 0 && rpc_id == 207 && data_size == 69) {
                try {
                    const auto decoded = bitstream_decoder::decode_player_sync_207(
                        data.data(), data.size(), static_cast<std::size_t>(data_size) * 8u);
                    print_player_sync_207(decoded);
                } catch (const std::exception& e) {
                    std::cout << "  decoded:      ERROR: " << e.what() << '\n';
                }
            }

            std::cout << "  nextOffset:   0x" << std::hex << r.offset() << std::dec << "\n\n";
        }

        const auto final_offset = r.offset();
        std::cout << "finalOffset: 0x" << std::hex << final_offset << std::dec << '\n';
        std::cout << "eofExact: " << (final_offset == file_size ? "YES" : "NO") << '\n';

        if (final_offset != file_size)
            throw std::runtime_error("parsed records do not terminate exactly at EOF");
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
