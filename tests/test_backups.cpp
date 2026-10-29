#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/backup_manager.h"
#include "core/paths.h"
#include "core/settings.h"

namespace fs = std::filesystem;

static fs::path make_temp_dir(const std::string& name) {
  fs::path p = fs::temp_directory_path() / ("evac_test_" + name);
  fs::remove_all(p);
  fs::create_directories(p);
  return p;
}

static void write_file(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  std::ofstream(path) << content;
}

static std::string read_file(const fs::path& path) {
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

struct BackupFixture {
  BackupFixture(const std::string& name)
      : dir(make_temp_dir(name)),
        settings(dir / "s.json", status),
        paths(dir, settings, status),
        trash(settings, paths, status),
        backups(settings, paths, status, trash) {
    settings.load();
  }

  void make_saves() {
    write_file(paths.savegames() / "Alpha/map.dat", "alpha v1");
    write_file(paths.savegames() / "Alpha/sub/more.dat", "more");
    write_file(paths.savegames() / "Beta/map.dat", "beta v1");
  }

  fs::path dir;
  Status status;
  Settings settings;
  Paths paths;
  Trash trash;
  BackupManager backups;
};

TEST_CASE("backup names") {
  REQUIRE(BackupManager::is_valid_backup_name("Manual_25-01-02_10-30"));
  REQUIRE(BackupManager::is_valid_backup_name("before the war"));
  REQUIRE(!BackupManager::is_valid_backup_name(""));
  REQUIRE(!BackupManager::is_valid_backup_name(" leading"));
  REQUIRE(!BackupManager::is_valid_backup_name("trailing "));
  REQUIRE(!BackupManager::is_valid_backup_name("../escape"));
  REQUIRE(!BackupManager::is_valid_backup_name("a:b"));
  REQUIRE(!BackupManager::is_valid_backup_name("what?"));

  std::tm t = {};
  t.tm_year = 125;  // 2025
  t.tm_mon = 0;
  t.tm_mday = 2;
  t.tm_hour = 9;
  t.tm_min = 5;
  REQUIRE(BackupManager::default_backup_name(t) == "Manual_25-01-02_09-05");
  REQUIRE(BackupManager::is_valid_backup_name(
      BackupManager::default_backup_name(t)));
}

TEST_CASE("create, list and delete backups") {
  BackupFixture f("backup_create");
  REQUIRE(f.backups.list().empty());

  f.make_saves();
  REQUIRE(f.backups.backup_current("first"));
  REQUIRE(fs::exists(f.paths.save_backups() / "first/Alpha.zip"));
  REQUIRE(fs::exists(f.paths.save_backups() / "first/Beta.zip"));

  std::vector<BackupSummary> list = f.backups.list();
  REQUIRE(list.size() == 1);
  REQUIRE(list[0].name == "first");
  REQUIRE(list[0].path == f.paths.save_backups() / "first");
  REQUIRE(list[0].worlds.size() == 2);
  REQUIRE(list[0].worlds[0] == "Alpha");

  REQUIRE(!f.backups.backup_current("first"));  // The name is taken.
  REQUIRE(!f.backups.backup_current("bad/name"));

  REQUIRE(f.backups.delete_backup("first"));
  REQUIRE(f.backups.list().empty());
  std::vector<TrashItem> trashed = f.trash.list();
  REQUIRE(trashed.size() == 1);
  REQUIRE(trashed[0].kind == "backup");
  REQUIRE(trashed[0].name == "first");
  REQUIRE(fs::exists(f.paths.trash_dir() / trashed[0].id / "item/Alpha.zip"));
  REQUIRE(!f.backups.delete_backup("first"));
  REQUIRE(!f.backups.delete_backup("../first"));
}

TEST_CASE("nothing to back up") {
  BackupFixture f("backup_empty");
  REQUIRE(!f.backups.backup_current("empty"));  // No save directory at all.
  REQUIRE(!fs::exists(f.paths.save_backups() / "empty"));

  fs::create_directories(f.paths.savegames());
  REQUIRE(!f.backups.backup_current("empty"));  // No worlds in it.
  REQUIRE(!fs::exists(f.paths.save_backups() / "empty"));
}

TEST_CASE("restore replaces the current saves") {
  BackupFixture f("backup_restore");
  f.make_saves();
  REQUIRE(f.backups.backup_current("snapshot"));

  // Play on: change a world, lose another, start a new one.
  write_file(f.paths.savegames() / "Alpha/map.dat", "alpha v2");
  fs::remove_all(f.paths.savegames() / "Beta");
  write_file(f.paths.savegames() / "Gamma/map.dat", "gamma");

  REQUIRE(f.backups.restore("snapshot"));
  REQUIRE(read_file(f.paths.savegames() / "Alpha/map.dat") == "alpha v1");
  REQUIRE(read_file(f.paths.savegames() / "Alpha/sub/more.dat") == "more");
  REQUIRE(read_file(f.paths.savegames() / "Beta/map.dat") == "beta v1");
  REQUIRE(!fs::exists(f.paths.savegames() / "Gamma"));
  REQUIRE(!fs::exists(f.paths.tmp_dir() / "restore"));

  // The saves that were replaced are in the trash, Gamma included.
  std::vector<TrashItem> trashed = f.trash.list();
  REQUIRE(trashed.size() == 1);
  REQUIRE(trashed[0].kind == "saves");
  fs::path old_saves = f.paths.trash_dir() / trashed[0].id / "item";
  REQUIRE(read_file(old_saves / "Alpha/map.dat") == "alpha v2");
  REQUIRE(read_file(old_saves / "Gamma/map.dat") == "gamma");

  REQUIRE(!f.backups.restore("no such backup"));
}

TEST_CASE("restore works when there are no saves yet") {
  BackupFixture f("backup_restore_fresh");
  f.make_saves();
  REQUIRE(f.backups.backup_current("snapshot"));
  fs::remove_all(f.paths.savegames());  // Not userdata: the backups are in it.

  REQUIRE(f.backups.restore("snapshot"));
  REQUIRE(read_file(f.paths.savegames() / "Beta/map.dat") == "beta v1");
  REQUIRE(f.trash.list().empty());  // There was nothing to replace.
}

TEST_CASE("a broken backup does not destroy the current saves") {
  BackupFixture f("backup_broken");
  f.make_saves();
  REQUIRE(f.backups.backup_current("snapshot"));

  write_file(f.paths.save_backups() / "snapshot/Zeta.zip", "not a zip at all");
  write_file(f.paths.savegames() / "Alpha/map.dat", "alpha v2");

  REQUIRE(!f.backups.restore("snapshot"));
  REQUIRE(read_file(f.paths.savegames() / "Alpha/map.dat") == "alpha v2");
  REQUIRE(!fs::exists(f.paths.tmp_dir() / "restore"));
  REQUIRE(f.trash.list().empty());
}

TEST_CASE("backups in the old folder are moved into userdata") {
  BackupFixture f("backup_move");
  fs::path old_dir = f.dir / "dda/save_backups";
  write_file(old_dir / "old one/Alpha.zip", "zip");

  std::vector<BackupSummary> list = f.backups.list();
  REQUIRE(list.size() == 1);
  REQUIRE(list[0].name == "old one");
  REQUIRE(read_file(f.paths.save_backups() / "old one/Alpha.zip") == "zip");
  REQUIRE(!fs::exists(old_dir));

  // Only when the new folder does not exist yet: never mixed into it.
  write_file(old_dir / "stray/Beta.zip", "zip");
  REQUIRE(f.backups.list().size() == 1);
  REQUIRE(fs::exists(old_dir / "stray/Beta.zip"));
}

TEST_CASE(
    "old backups go to the default userdata even when another one is active") {
  BackupFixture f("backup_move_custom");
  fs::path old_dir = f.dir / "dda/save_backups";
  write_file(old_dir / "old one/Alpha.zip", "zip");
  REQUIRE(f.paths.add_userdata(f.dir / "modded", "Modded") == "");
  f.paths.set_active_userdata(f.dir / "modded");

  REQUIRE(f.backups.list().empty());
  REQUIRE(read_file(f.dir / "dda/userdata/evac/backups/old one/Alpha.zip") ==
          "zip");
  REQUIRE(!fs::exists(f.dir / "modded"));

  f.paths.set_active_userdata("");
  REQUIRE(f.backups.list().size() == 1);
}
