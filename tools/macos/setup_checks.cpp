#include "macos_setup.h"
#include <fstream>
#include <stdexcept>
#include <unistd.h>

namespace fs = std::filesystem;
static void require(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
static void write(const fs::path &p, const std::string &value)
{
    fs::create_directories(p.parent_path());
    std::ofstream file(p); file << value;
    require(bool(file), "could not write fixture");
}
static std::string read(const fs::path &p)
{
    std::ifstream file(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
static void fixture(const fs::path &p)
{
    for (const char *name : {"data/index.txt", "data/SLUS_203.62", "data/IRX253/STREAM.IRX", "data/lang/ENGLISH.txt"})
        write(p / name, "fixture");
    write(p / "saves/player/save.dat", "existing save");
    write(p / "hp2.ini", "existing settings");
}
int main()
{
    char pattern[] = "/tmp/hp2-setup-check-XXXXXX";
    if (!mkdtemp(pattern)) return 1;
    struct Cleanup { fs::path p; ~Cleanup() { std::error_code ec; fs::remove_all(p, ec); } } cleanup{pattern};
    try {
        const fs::path root = pattern, support = root / "User Library", game = root / "Game é 日本語", moved = root / "Moved game";
        const auto legacy = root / "old.app/Contents/Resources/game-location.txt";
        std::string why;
        require(mac_setup::remembered_game(support, legacy).empty(), "fresh user should need setup");
        require(!mac_setup::remember_game(support, game, why), "must reject missing game folder");
        fixture(game);
        write(legacy, game.string() + "\n");
        require(mac_setup::remembered_game(support, legacy) == game, "legacy installation not discovered");
        require(mac_setup::remember_game(support, game, why), "could not save Unicode location");
        require(mac_setup::remembered_game(support) == game, "second launch did not reuse saved location");
        fs::rename(game, moved);
        require(mac_setup::remembered_game(support, legacy).empty(), "moved installation should request setup");
        require(mac_setup::remember_game(support, moved, why), "could not remember moved folder");
        require(mac_setup::remembered_game(support) == moved, "moved location not persisted");
        require(read(moved / "saves/player/save.dat") == "existing save", "save changed");
        require(read(moved / "hp2.ini") == "existing settings", "settings changed");
        const auto interrupted = support / "Game";
        write(interrupted / "user-file", "keep me");
        require(mac_setup::new_game_folder(support) == support / "Game 2", "must not reuse nonempty destination");
        InstallSteps steps;
        steps.progress = [](int, const std::wstring &) {};
        const auto bad_iso = root / "invalid.iso";
        write(bad_iso, "not a disc");
        const auto destination = support / "Game 2";
        require(!mac_setup::import_game(bad_iso, destination, steps), "accepted invalid disc");
        require(!fs::exists(destination), "invalid disc published a partial installation");
        require(!mac_setup::import_game(bad_iso, moved, steps), "accepted existing destination");
        require(read(moved / "saves/player/save.dat") == "existing save", "import overwrote save");
        const auto empty = root / "Empty";
        fs::create_directory(empty);
        require(!mac_setup::import_game(bad_iso, empty, steps) && fs::is_empty(empty), "must preserve even empty existing folder");
        std::atomic<bool> stop{true}; steps.stop = &stop;
        require(!mac_setup::import_game(bad_iso, destination, steps) && steps.why == "Cancelled.", "cancel before import failed");
        fixture(interrupted);
        fs::remove(support / "game-location.txt");
        require(mac_setup::remembered_game(support) == interrupted, "completed import not recovered without preference");
        // A preference write failure must leave both the installation and saves intact.
        write(root / "not-a-directory", "obstacle");
        require(!mac_setup::remember_game(root / "not-a-directory", moved, why), "preference write failure hidden");
        require(read(moved / "saves/player/save.dat") == "existing save", "failed preference write affected save");
        fprintf(stderr, "PASS: fresh user, persistence, Unicode, migration, moved folders, recovery, invalid disc, cancellation, existing saves.\n");
    } catch (const std::exception &e) { fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}
