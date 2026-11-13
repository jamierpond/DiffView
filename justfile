# DiffView dev commands. Requires just, cmake, ninja and a C++20 toolchain;
# see README.md. Override the build type with `just build_type=Debug build`.

set windows-shell := ["cmd.exe", "/c"]

build_dir := "build"
build_type := "Release"
windows_bin := env("USERPROFILE", "") + '\.local\bin'

# Show the recipe list by default.
default:
    @just --list

[doc('Configure + build the app; extra arguments go to the configure step')]
build *cmake_args:
    cmake -S . -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE={{build_type}} {{cmake_args}}
    cmake --build {{build_dir}} --target DiffView

[doc('Build everything (app + every test binary, all under -Werror), run the tests')]
test: build
    cmake --build {{build_dir}}
    ctest --test-dir {{build_dir}} --output-on-failure

[doc('Build, then run it (on this repo unless given a path / --base)')]
[unix]
run *args: build
    "{{build_dir}}/diffview" {{args}}

[doc('Build, then run it (on this repo unless given a path / --base)')]
[windows]
run *args: build
    "{{build_dir}}\diffview.exe" {{args}}

[doc('Release build into ~/.local/bin/diffview, plus the dv link')]
[unix]
install: build
    mkdir -p "$HOME/.local/bin"
    # Copy beside, then rename over: overwriting a running binary in place
    # gets it killed by macOS's code-signing check.
    cp "{{build_dir}}/diffview" "$HOME/.local/bin/.diffview.new"
    mv -f "$HOME/.local/bin/.diffview.new" "$HOME/.local/bin/diffview"
    ln -sf diffview "$HOME/.local/bin/dv"
    @echo "Installed ~/.local/bin/diffview (and dv)"

# A running exe can be renamed but not overwritten, so each installed one
# moves aside first. dv.exe is a hard link: symlinks need admin or Developer Mode.
[doc('Release build into ~/.local/bin/diffview.exe, plus the dv.exe link')]
[windows]
install: build
    if not exist "{{windows_bin}}" mkdir "{{windows_bin}}"
    copy /y "{{build_dir}}\diffview.exe" "{{windows_bin}}\.diffview.new.exe" >nul
    -del /q "{{windows_bin}}\.diffview.old.exe" "{{windows_bin}}\.dv.old.exe" 2>nul
    if exist "{{windows_bin}}\diffview.exe" move /y "{{windows_bin}}\diffview.exe" "{{windows_bin}}\.diffview.old.exe" >nul
    move /y "{{windows_bin}}\.diffview.new.exe" "{{windows_bin}}\diffview.exe" >nul
    if exist "{{windows_bin}}\dv.exe" move /y "{{windows_bin}}\dv.exe" "{{windows_bin}}\.dv.old.exe" >nul
    mklink /H "{{windows_bin}}\dv.exe" "{{windows_bin}}\diffview.exe" >nul
    @echo Installed {{windows_bin}}\diffview.exe (and dv.exe)

[doc('clang-format every tracked source in place')]
[unix]
format:
    git ls-files '*.h' '*.cpp' '*.mm' | xargs clang-format -i

[doc('clang-format every tracked source in place')]
[windows]
format:
    for /f "delims=" %f in ('git ls-files "*.h" "*.cpp" "*.mm"') do @clang-format -i "%f"

[doc('Delete the build directory')]
clean:
    rm -rf {{build_dir}}
