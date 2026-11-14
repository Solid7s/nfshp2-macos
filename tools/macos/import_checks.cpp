#include "install.h"
#include "setup_iso.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unistd.h>

static void require(bool ok, const char *what)
{
    if (!ok) throw std::runtime_error(what);
}
static std::vector<char> bytes(const std::filesystem::path &p)
{
    std::ifstream f(p, std::ios::binary);
    require(bool(f), "resource file missing");
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    namespace fs = std::filesystem;
    char temp[] = "/tmp/hp2-import-check-XXXXXX";
    if (!mkdtemp(temp)) return 1;
    struct Cleanup { fs::path p; ~Cleanup() { std::error_code ec; fs::remove_all(p, ec); } } cleanup{temp};
    try {
        const fs::path dest = fs::path(temp) / "Game files \xC3\xA9", source = argv[1];
        require(install_port_files(dest.wstring()), "install port resources");
        for (const auto &name : {"ENGLISH", "FRENCH", "GERMAN", "ITALIAN", "SPANISH", "SWEDISH"}) {
            const fs::path rel = fs::path("lang") / (std::string(name) + ".txt");
            require(bytes(dest / "data" / rel) == bytes(source / rel), "language contents changed");
        }
        require(bytes(dest / "data/ui/cursor.cur") == bytes(source / "ui/cursor.cur"), "cursor contents changed");
        require(bytes(dest / "README.txt") == bytes(source / "README.txt"), "readme contents changed");
        require(fs::is_directory(dest / "data/replace"), "replacement directory missing");
        require(sha1_hex("abc", 3) == "a9993e364706816aba3e25717850c26c9cd0d89d", "disc hash failed");
        require(crc32_update(0, "123456789", 9) == 0xcbf43926, "CRC failed");
        // A tiny ISO containing one file checks the parser without any game data.
        std::vector<uint8_t> image(21 * 2048);
        auto put32 = [&](size_t at, uint32_t n) { memcpy(image.data() + at, &n, 4); };
        const size_t pvd = 16 * 2048, dir = 19 * 2048;
        image[pvd] = 1; memcpy(image.data() + pvd + 1, "CD001", 5); image[pvd + 6] = 1;
        put32(pvd + 158, 19); put32(pvd + 166, 2048);
        const char name[] = "TEST.BIN;1";
        image[dir] = 44; put32(dir + 2, 20); put32(dir + 10, 4);
        image[dir + 32] = sizeof(name) - 1; memcpy(image.data() + dir + 33, name, sizeof(name) - 1);
        memcpy(image.data() + 20 * 2048, "test", 4);
        const fs::path iso_path = dest / "test.iso";
        { std::ofstream f(iso_path, std::ios::binary); f.write((char *)image.data(), image.size()); }
        Iso iso; std::string why;
        require(iso.open(iso_path.wstring(), why), "synthetic ISO failed to open");
        const auto *file = iso.find("/test.bin"); char data[4];
        require(file && file->size == 4 && iso.read_file(*file, 0, data, sizeof data) && !memcmp(data, "test", 4), "ISO file read failed");
        fprintf(stderr, "PASS: resource contents, Unicode paths, disc hashes, ISO directory and sector reads.\n");
    } catch (const std::exception &e) { fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}
