# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location

project(evac-cppTests LANGUAGES CXX)

# ---- Dependencies ----

find_package(Catch2 REQUIRED)
include(Catch)

# ---- Tests ----

add_executable(
    evac-cpp_test
    src/test_version.cpp
    src/test_status.cpp
    src/test_settings_paths.cpp
    src/test_fs_archive.cpp
    src/test_http.cpp
    src/test_releases.cpp
    src/test_fonts.cpp
    src/test_soundpacks.cpp
    src/test_mods.cpp
    src/test_backups.cpp
    src/test_snapshots.cpp
    src/test_extras.cpp
    src/test_trash.cpp
    src/test_launcher.cpp
    src/test_platform.cpp
)
target_link_libraries(evac-cpp_test PRIVATE evac-cpp_lib Catch2::Catch2WithMain)
target_compile_features(evac-cpp_test PRIVATE cxx_std_17)

catch_discover_tests(evac-cpp_test)

# ---- End-of-file commands ----

add_folders(Test)
