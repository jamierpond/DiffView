# DiffView dev commands. Requires just, cmake, ninja and a C++20 toolchain;
# see README.md. Override the build type with `just build_type=Debug build`.

set windows-shell := ["cmd.exe", "/c"]

build_dir := "build"
build_type := "Release"

# Show the recipe list by default.
default:
    @just --list

[doc('Configure the CMake build tree (idempotent; first run fetches deps via lockstep)')]
configure:
    cmake -S . -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE={{build_type}}

[doc('Build the app')]
build: configure
    cmake --build {{build_dir}} --target DiffView

[doc('Build everything (app + every test binary, all under -Werror), run the tests')]
test: configure
    cmake --build {{build_dir}}
    ctest --test-dir {{build_dir}} --output-on-failure

[doc('Build, then run it (on this repo unless given a path / --base)')]
run *args: build
    "{{build_dir}}/diffview" {{args}}

[doc('Release build into ~/.local/bin/diffview, plus the dv link')]
install: build
    mkdir -p "$HOME/.local/bin"
    # Copy beside, then rename over: overwriting a running binary in place
    # gets it killed by macOS's code-signing check.
    cp "{{build_dir}}/diffview" "$HOME/.local/bin/.diffview.new"
    mv -f "$HOME/.local/bin/.diffview.new" "$HOME/.local/bin/diffview"
    ln -sf diffview "$HOME/.local/bin/dv"
    @echo "Installed ~/.local/bin/diffview (and dv)"

[doc('Delete the build directory')]
clean:
    rm -rf {{build_dir}}
