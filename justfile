# DiffView: build / test / run / install.
# Requires: just, cmake, ninja (+ a C++20 toolchain). See README.md.
#
#   just build                 configure + build the app
#   just test                  build + run the unit tests
#   just run [args]            build, then run it on this repo (or args)
#   just install               Release build into ~/.local/bin (diffview + dv)
#   just build_type=Debug build

build_dir  := "build"
build_type := "Release"

# Show the recipe list by default.
default:
    @just --list

# Configure the CMake build tree (idempotent; first run fetches deps via CPM).
configure:
    cmake -S . -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE={{build_type}}

# Build the app.
build: configure
    cmake --build {{build_dir}} --target DiffView

# Build everything (app + every test binary, all under -Werror), run the tests.
test: configure
    cmake --build {{build_dir}}
    ctest --test-dir {{build_dir}} --output-on-failure

# Build, then run it (on this repo unless given a path / --base).
run *args: build
    "{{build_dir}}/diffview" {{args}}

# Release build into ~/.local/bin/diffview.
install: build
    mkdir -p "$HOME/.local/bin"
    # Copy beside, then rename over: overwriting a running binary in place
    # gets it killed by macOS's code-signing check.
    cp "{{build_dir}}/diffview" "$HOME/.local/bin/.diffview.new"
    mv -f "$HOME/.local/bin/.diffview.new" "$HOME/.local/bin/diffview"
    ln -sf diffview "$HOME/.local/bin/dv"
    @echo "Installed ~/.local/bin/diffview (and dv)"

# Remove the build tree.
clean:
    rm -rf {{build_dir}}
