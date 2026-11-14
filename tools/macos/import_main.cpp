#include "install.h"
#include "setup_iso.h"
#include <filesystem>
#include <cstdio>

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: hp2-import <PS2 disc.iso> <new game folder>\n");
        return 2;
    }
    namespace fs = std::filesystem;
    const fs::path dest = fs::absolute(argv[2]);
    if (fs::exists(dest) && !fs::is_empty(dest)) {
        fprintf(stderr, "Choose a new or empty game folder; existing files will not be overwritten.\n");
        return 2;
    }
    Iso iso;
    InstallSteps steps;
    int last = -1;
    steps.progress = [&](int pm, const std::wstring &what) {
        if (pm / 50 == last) return;
        last = pm / 50;
        fprintf(stderr, "import: %d%% %s\n", pm / 10, fs::path(what).string().c_str());
    };
    try {
        if (!iso.open(fs::path(argv[1]).wstring(), steps.why) || !install_game(iso, dest.wstring(), steps)) {
            fprintf(stderr, "Import failed: %s\n", steps.why.c_str());
            return 1;
        }
    } catch (const std::exception &e) {
        fprintf(stderr, "Import failed: %s\n", e.what());
        return 1;
    }
    fprintf(stderr, "Imported into %s\n", dest.c_str());
    return 0;
}
