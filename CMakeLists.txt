cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    evac-cpp
    VERSION 0.1.0
    DESCRIPTION "Evac: Cataclysm Launcher"
    HOMEPAGE_URL "https://example.com/"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(
    evac-cpp_lib
    OBJECT
    src/core/version.cpp
    src/core/platform.cpp
    src/core/process.cpp
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
    src/core/snapshot_manager.cpp
    src/core/tips.cpp
    src/core/window_state.cpp
    src/core/licenses.cpp
    src/core/trash.cpp
)

target_compile_definitions(
    evac-cpp_lib
    PRIVATE EVAC_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)
target_include_directories(
    evac-cpp_lib
    ${warning_guard}
    PUBLIC "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
)

target_compile_features(evac-cpp_lib PUBLIC cxx_std_17)

find_package(fmt REQUIRED)
find_package(cpr REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(LibArchive REQUIRED)
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED)
find_package(zoe REQUIRED)
target_link_libraries(
    evac-cpp_lib
    PUBLIC
        fmt::fmt
        cpr::cpr
        nlohmann_json::nlohmann_json
        LibArchive::LibArchive
        spdlog::spdlog
        Boost::process
        zoe::zoe
)

# ---- Declare executable ----

add_executable(
    evac-cpp_exe
    src/main.cpp
    src/ui/qt_helpers.cpp
    src/ui/main_window.cpp
    src/ui/tab_game.cpp
    src/ui/tab_fonts.cpp
    src/ui/font_preview.cpp
    src/ui/tab_soundpacks.cpp
    src/ui/tab_mods.cpp
    src/ui/tab_snapshots.cpp
    src/ui/tab_backups.cpp
    src/ui/tab_settings.cpp
    src/ui/tab_trash.cpp
    src/ui/dialogs.cpp
    src/ui/download_prompt.cpp
    src/ui/licenses_dialog.cpp
    src/ui/log_view.cpp
    src/ui/task_runner.cpp
)
add_executable(evac-cpp::exe ALIAS evac-cpp_exe)

set_target_properties(evac-cpp_exe PROPERTIES AUTOMOC ON)
set_property(TARGET evac-cpp_exe PROPERTY OUTPUT_NAME evac-cpp)

target_compile_features(evac-cpp_exe PRIVATE cxx_std_17)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
target_link_libraries(evac-cpp_exe PRIVATE evac-cpp_lib Qt6::Widgets)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
    include(cmake/install-rules.cmake)
endif()

# ---- Custom commands ----

add_custom_command(
    TARGET evac-cpp_exe
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:evac-cpp_exe>/data
)

# ---- Developer mode ----

if(NOT evac-cpp_DEVELOPER_MODE)
    return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
    message(
        AUTHOR_WARNING
        "Developer mode is intended for developers of evac-cpp"
    )
endif()

include(cmake/dev-mode.cmake)
