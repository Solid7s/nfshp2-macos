#pragma once
#include "install.h"
#include <filesystem>
#include <string>

namespace mac_setup {
namespace fs = std::filesystem;
bool game_folder(const fs::path &root);
fs::path remembered_game(const fs::path &support, const fs::path &legacy = {});
bool remember_game(const fs::path &support, const fs::path &game, std::string &why);
fs::path new_game_folder(const fs::path &support);
// Import in a private staging directory, publishing it only after success.
// Never writes into an existing destination, including a partially installed one.
bool import_game(const fs::path &iso, const fs::path &destination, InstallSteps &steps);

// Cocoa UI, called on the main thread after SDL video initialization.
// Returns false when the user quits or cancels setup.
bool choose_game(std::string &game, const fs::path &legacy = {});
bool import_with_progress(const fs::path &iso, const fs::path &destination);
}
