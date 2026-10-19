cmake_minimum_required(VERSION 3.23)
project(catapult CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # For clangd.

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(LibArchive REQUIRED)
find_package(spdlog REQUIRED)
find_package(Catch2 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)
# For the font preview: fonts from files are loaded through fontconfig.
pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig pangoft2)

# Core library: no GTK in here.
add_library(catapult_core STATIC
    src/core/version.cpp
    src/core/status.cpp
    src/core/helpers.cpp
    src/core/settings.cpp
    src/core/paths.cpp
    src/core/fs.cpp
    src/core/archive.cpp
    src/core/http.cpp
    src/core/games.cpp
    src/core/release_data.cpp
    src/core/release_manager.cpp
    src/core/release_installer.cpp
    src/core/game_launcher.cpp
    src/core/font_manager.cpp
    src/core/soundpack_data.cpp
    src/core/soundpack_manager.cpp
    src/core/mod_manager.cpp
    src/core/backup_manager.cpp
    src/core/git.cpp
    src/core/tips.cpp
    src/core/window_state.cpp
    src/core/licenses.cpp
    src/core/trash.cpp)
target_compile_definitions(catapult_core PRIVATE CATAPULT_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
target_include_directories(catapult_core PUBLIC src)
target_link_libraries(catapult_core PUBLIC
    CURL::libcurl nlohmann_json::nlohmann_json LibArchive::LibArchive spdlog::spdlog)

add_executable(catapult
    src/main.cpp
    src/ui/main_window.cpp
    src/ui/tab_game.cpp
    src/ui/tab_fonts.cpp
    src/ui/font_preview.cpp
    src/ui/tab_soundpacks.cpp
    src/ui/tab_mods.cpp
    src/ui/tab_backups.cpp
    src/ui/tab_settings.cpp
    src/ui/tab_trash.cpp
    src/ui/dialogs.cpp
    src/ui/download_prompt.cpp
    src/ui/licenses_dialog.cpp
    src/ui/log_view.cpp
    src/ui/task_runner.cpp)
target_link_libraries(catapult PRIVATE catapult_core PkgConfig::GTKMM PkgConfig::FONTCONFIG)

# The bundled data (fonts, ...) goes next to the executable.
add_custom_command(TARGET catapult POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data $<TARGET_FILE_DIR:catapult>/data)

enable_testing()
add_executable(core_tests
    tests/test_version.cpp
    tests/test_status.cpp
    tests/test_settings_paths.cpp
    tests/test_fs_archive.cpp
    tests/test_http.cpp
    tests/test_releases.cpp
    tests/test_fonts.cpp
    tests/test_soundpacks.cpp
    tests/test_mods.cpp
    tests/test_backups.cpp
    tests/test_extras.cpp
    tests/test_trash.cpp
    tests/test_launcher.cpp)
target_link_libraries(core_tests PRIVATE catapult_core Catch2::Catch2WithMain)
add_test(NAME core_tests COMMAND core_tests)
