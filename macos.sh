#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
project_dir="$PWD"
command_name="${1:-check}"
if [ "$#" -gt 0 ]; then shift; fi
build_dir="$project_dir/build/macos"
game_dir="${HP2_DATA:-$project_dir/game}"

configure() {
    cmake -S recomp -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHP2_PGO= -DHP2_MEM4G=OFF
}
build_importer() {
    mkdir -p "$project_dir/build/import"
    python3 tools/macos/embed_resources.py "$project_dir/build/import/port_resources.h"
    clang++ -std=c++17 -O2 -Irecomp/runtime -Ibuild/import tools/macos/import_main.cpp recomp/runtime/install.cpp -o "$project_dir/build/import/hp2-import"
}
case "$command_name" in
    import)
        if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
            echo 'Usage: ./macos.sh import "/path/to/PS2 disc.iso" [new game folder]' >&2
            exit 2
        fi
        build_importer
        "$project_dir/build/import/hp2-import" "$1" "${2:-$game_dir}"
        ;;
    build)
        game_dir="${1:-$game_dir}"
        if [ ! -f "$game_dir/data/SLUS_203.62" ] || [ ! -f "$game_dir/data/IRX253/STREAM.IRX" ]; then
            echo 'Import your PS2 disc first: ./macos.sh import "/path/to/disc.iso"' >&2
            exit 2
        fi
        python3 tools/recomp/gen.py --elf "$game_dir/data/SLUS_203.62"
        python3 tools/recomp/gen_iop.py --irx "$game_dir/data/IRX253/STREAM.IRX"
        configure
        cmake --build "$build_dir" --target hp2recomp --parallel "${HP2_BUILD_JOBS:-4}"
        ;;
    check)
        configure
        cmake --build "$build_dir" --parallel "${HP2_BUILD_JOBS:-4}"
        ctest --test-dir "$build_dir" --output-on-failure
        ;;
    probe)
        configure
        cmake --build "$build_dir" --target hp2-vulkan-probe --parallel "${HP2_BUILD_JOBS:-4}"
        exec "$build_dir/hp2-vulkan-probe"
        ;;
    run)
        if [ ! -x "$project_dir/recompmac/speed" ]; then
            echo 'The game is not built yet. Import the disc, then run ./macos.sh build.' >&2
            exit 2
        fi
        exec "$project_dir/recompmac/speed" --data "${1:-$game_dir}"
        ;;
    app)
        python3 tools/macos/package_app.py "$@"
        ;;
    *)
        echo 'Usage: ./macos.sh {import <disc.iso> [folder]|build [folder]|check|probe|run [folder]|app [folder]}' >&2
        exit 2
        ;;
esac
