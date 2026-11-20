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

[doc("Build everything, then run clang-tidy (eacp's checks) over DiffView's sources; fails on any finding")]
[unix]
tidy: build
    cmake --build {{build_dir}}
    scripts/tidy.sh {{build_dir}}

[doc('Build, then run it (on this repo unless given a path / --base)')]
[unix]
run *args: build
    "{{build_dir}}/diffview" {{args}}

[doc('Build, then run it (on this repo unless given a path / --base)')]
[windows]
run *args: build
    "{{build_dir}}\diffview.exe" {{args}}

[doc('For now the default: the line-coverage build into ~/.local/bin/diffview, plus the dv link, keeping the old and new programs for coverage-report')]
[unix]
install: coverage-build
    {{line_coverage}} keep
    just build_dir={{coverage_build_dir}} copy-into-local-bin
    {{line_coverage}} keep

[doc('Release build without line coverage into ~/.local/bin/diffview, plus the dv link')]
[unix]
install-without-coverage: build
    just copy-into-local-bin

[private]
[unix]
copy-into-local-bin:
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
    cmake -E rm -rf {{build_dir}}

[doc('Move branch-pinned dependencies to their branch heads and rewrite lockstep.lock')]
update-deps:
    cmake -S . -B {{build_dir}} -G Ninja -DLOCKSTEP_MODE=update
    cmake -S . -B {{build_dir}} -G Ninja -DLOCKSTEP_MODE=locked

[doc('Build, then save what dv draws at phone, threshold and desktop widths, run on <repo> in a throwaway tmux session')]
[unix]
capture-layouts repo out="build/layouts": build
    scripts/capture-layouts.sh "{{build_dir}}/diffview" "{{repo}}" "{{out}}"

# ---- Line coverage: which lines of DiffView's own code a normal day runs ----

coverage_build_dir := "build-coverage"
coverage_cmake_arguments := "-DPOND_LINE_COVERAGE=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++"
# Quoted so line-coverage.py expands ~ on each machine it runs on, not the shell here.
line_coverage := coverage_build_dir + "/PondLineCoverage/line-coverage.py --app DiffView --program diffview --installed-in '~/.local/bin' --own-code-folder Source --own-code-folder Lib --not-own-code-folder Tests"

[doc('Build the app with line-coverage counters, in build-coverage')]
[unix]
coverage-build:
    just build_dir={{coverage_build_dir}} build {{coverage_cmake_arguments}}

[doc('Print the code that never ran in any installed coverage build, largest first, plus an HTML per-line view')]
[unix]
coverage-report *arguments: coverage-build
    {{line_coverage}} report {{arguments}}

[doc("Copy a host's line-coverage counts and its programs here, for coverage-report")]
[unix]
coverage-collect-from-host host *arguments: coverage-build
    {{line_coverage}} collect {{host}} {{arguments}}

[doc('Delete the line-coverage counts and kept programs here, or on user@host')]
[unix]
coverage-reset host="" *arguments: coverage-build
    {{line_coverage}} reset {{host}} {{arguments}}

[doc('Build, then pipe a diff of <repo> into dv in a throwaway tmux session and save what it drew after each key')]
[unix]
check-piped-diff repo="." out="build/piped-diff": build
    scripts/check-piped-diff.sh "{{build_dir}}/diffview" "{{repo}}" "{{out}}"
