#include "macos_setup.h"
#include "setup_iso.h"
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <unistd.h>

namespace mac_setup {
bool game_folder(const fs::path &root)
{
    if (root.empty()) return false;
    std::error_code ec;
    for (const char *name : {"data/index.txt", "data/SLUS_203.62", "data/IRX253/STREAM.IRX", "data/lang/ENGLISH.txt"}) {
        const auto file = root / name;
        if (!fs::is_regular_file(file, ec) || fs::file_size(file, ec) == 0 || ec) return false;
    }
    return true;
}

static fs::path read_location(const fs::path &file)
{
    std::ifstream input(file, std::ios::binary);
    std::string path{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (!path.empty() && path.back() == '\n') path.pop_back();
    return path;
}

fs::path remembered_game(const fs::path &support, const fs::path &legacy)
{
    // The default also recovers an import completed just before a power loss or
    // a failure writing the preference. Old locally packaged apps can migrate.
    for (const auto &candidate : {read_location(support / "game-location.txt"), support / "Game",
                                 legacy.empty() ? fs::path() : read_location(legacy)}) {
        if (game_folder(candidate)) return fs::absolute(candidate);
    }
    return {};
}

bool remember_game(const fs::path &support, const fs::path &game, std::string &why)
{
    try {
        if (!game_folder(game)) { why = "This folder does not contain an installed copy of the game."; return false; }
        fs::create_directories(support);
        auto pattern = (support / ".location-XXXXXX").string();
        const int fd = mkstemp(pattern.data());
        if (fd < 0) { why = "The installation location could not be saved: " + std::string(strerror(errno)); return false; }
        const auto value = fs::absolute(game).string() + "\n";
        FILE *file = fdopen(fd, "wb");
        bool ok = false;
        if (file) {
            ok = fwrite(value.data(), 1, value.size(), file) == value.size();
            if (fflush(file) || fsync(fd)) ok = false;
            if (fclose(file)) ok = false;
        } else close(fd);
        if (ok) ok = rename(pattern.c_str(), (support / "game-location.txt").c_str()) == 0;
        if (!ok) {
            const int error = errno;
            unlink(pattern.c_str());
            why = "The installation location could not be saved: " + std::string(strerror(error));
        }
        return ok;
    } catch (const std::exception &e) { why = e.what(); return false; }
}

fs::path new_game_folder(const fs::path &support)
{
    auto candidate = support / "Game";
    for (unsigned n = 2; fs::exists(candidate) || fs::is_symlink(candidate); ++n)
        candidate = support / ("Game " + std::to_string(n));
    return candidate;
}

bool import_game(const fs::path &image, const fs::path &destination, InstallSteps &steps)
{
    struct Staging {
        fs::path path;
        ~Staging() { if (!path.empty()) { std::error_code ec; fs::remove_all(path, ec); } }
    } staging;
    const auto cancelled = [&] { return steps.stop && steps.stop->load(); };
    try {
        if (cancelled()) { steps.why = "Cancelled."; return false; }
        if (fs::exists(destination) || fs::is_symlink(destination)) {
            steps.why = "The destination already exists. Its files have been left in place.";
            return false;
        }
        Iso iso;
        if (!iso.open(image.wstring(), steps.why)) return false;
        fs::create_directories(destination.parent_path());
        auto pattern = (destination.parent_path() / ".import-XXXXXX").string();
        if (!mkdtemp(pattern.data())) { steps.why = strerror(errno); return false; }
        staging.path = pattern;
        if (!install_game(iso, staging.path.wstring(), steps)) return false;
        if (cancelled()) { steps.why = "Cancelled."; return false; }
        if (!game_folder(staging.path)) { steps.why = "The imported game files are incomplete."; return false; }
        // An atomic, exclusive rename also protects against two installers
        // finishing at once; ordinary rename could replace an empty folder.
        if (renamex_np(staging.path.c_str(), destination.c_str(), RENAME_EXCL)) {
            steps.why = "The imported game folder could not be saved: " + std::string(strerror(errno));
            return false;
        }
        staging.path.clear();
        return true;
    } catch (const std::exception &e) { steps.why = e.what(); return false; }
}
}
