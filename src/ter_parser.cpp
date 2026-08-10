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

            // Confirmed from the original writer: rpcId is serialized with sizeof(int) = 4 bytes.
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
