cmake_minimum_required(VERSION 3.23)
project(catapult CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(LibArchive REQUIRED)
find_package(spdlog REQUIRED)
find_package(Catch2 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)

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
    src/core/font_manager.cpp)
target_compile_definitions(catapult_core PRIVATE CATAPULT_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
target_include_directories(catapult_core PUBLIC src)
target_link_libraries(catapult_core PUBLIC
    CURL::libcurl nlohmann_json::nlohmann_json LibArchive::LibArchive spdlog::spdlog)

add_executable(catapult
    src/main.cpp
    src/ui/main_window.cpp
    src/ui/tab_game.cpp
    src/ui/tab_fonts.cpp
    src/ui/log_view.cpp
    src/ui/task_runner.cpp)
target_link_libraries(catapult PRIVATE catapult_core PkgConfig::GTKMM)

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
    tests/test_fonts.cpp)
target_link_libraries(core_tests PRIVATE catapult_core Catch2::Catch2WithMain)
add_test(NAME core_tests COMMAND core_tests)
