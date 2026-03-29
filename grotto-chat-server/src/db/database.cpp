#include "database.hpp"

#include <spdlog/spdlog.h>

namespace grotto::db {

Database::Database(const std::string& path)
    : db_(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)
{
    db_.exec("PRAGMA journal_mode=WAL");
    db_.exec("PRAGMA foreign_keys=ON");
    execute_schema();
    spdlog::info("Database opened: {}", path);
}

void Database::execute_schema() {
    db_.exec(
        "CREATE TABLE IF NOT EXISTS users ("
        "  user_id       TEXT PRIMARY KEY,"
        "  identity_pub  BLOB NOT NULL,"
        "  password_hash BLOB,"           // Argon2id hash (NULL if password auth not used)
        "  created_at    INTEGER NOT NULL"
        ")"
    );

    db_.exec(
        "CREATE TABLE IF NOT EXISTS signed_prekeys ("
        "  user_id     TEXT NOT NULL REFERENCES users(user_id),"
        "  spk_pub     BLOB NOT NULL,"
        "  spk_sig     BLOB NOT NULL,"
        "  spk_id      INTEGER NOT NULL DEFAULT 0,"
        "  registration_id INTEGER NOT NULL DEFAULT 0,"
        "  uploaded_at INTEGER NOT NULL,"
        "  PRIMARY KEY (user_id)"
        ")"
    );
    // Compatibility: older SQLite doesn't support ALTER TABLE ... ADD COLUMN IF NOT EXISTS
    {
        bool has_spk_id = false;
        bool has_registration_id = false;
        SQLite::Statement pragma(db_, "PRAGMA table_info(signed_prekeys)");
        while (pragma.executeStep()) {
            if (pragma.getColumn(1).getString() == std::string("spk_id")) {
                has_spk_id = true;
            }
            if (pragma.getColumn(1).getString() == std::string("registration_id")) {
                has_registration_id = true;
            }
        }
        if (!has_spk_id) {
            db_.exec("ALTER TABLE signed_prekeys ADD COLUMN spk_id INTEGER NOT NULL DEFAULT 0");
        }
        if (!has_registration_id) {
            db_.exec("ALTER TABLE signed_prekeys ADD COLUMN registration_id INTEGER NOT NULL DEFAULT 0");
        }
    }

    db_.exec(
        "CREATE TABLE IF NOT EXISTS one_time_prekeys ("
        "  id       INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  user_id  TEXT NOT NULL REFERENCES users(user_id),"
        "  opk_pub  BLOB NOT NULL,"
        "  opk_id   INTEGER NOT NULL DEFAULT 0,"
        "  used     INTEGER NOT NULL DEFAULT 0"
        ")"
    );
    {
        bool has_opk_id = false;
        SQLite::Statement pragma(db_, "PRAGMA table_info(one_time_prekeys)");
        while (pragma.executeStep()) {
            if (pragma.getColumn(1).getString() == std::string("opk_id")) {
                has_opk_id = true;
                break;
            }
        }
        if (!has_opk_id) {
            db_.exec("ALTER TABLE one_time_prekeys ADD COLUMN opk_id INTEGER NOT NULL DEFAULT 0");
        }
    }

    db_.exec(
        "CREATE TABLE IF NOT EXISTS offline_messages ("
        "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  recipient_id TEXT NOT NULL,"
        "  payload      BLOB NOT NULL,"
        "  stored_at    INTEGER NOT NULL,"
        "  expires_at   INTEGER NOT NULL"
        ")"
    );

    db_.exec(
        "CREATE TABLE IF NOT EXISTS bug_reports ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  user_id     TEXT NOT NULL,"
        "  description TEXT NOT NULL,"
        "  reported_at INTEGER NOT NULL"
        ")"
    );

    spdlog::debug("Database schema initialized");
}

} // namespace grotto::db
