#include "sqlite_logger.hpp"
#include <chrono>
#include <filesystem>
#include <stdexcept>

static void exec_sql(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("SQLite exec failed: " + msg);
    }
}

SessionLogger::SessionLogger(const std::string& db_path) {
    std::filesystem::path p(db_path);
    std::filesystem::create_directories(p.parent_path());
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Cannot open SQLite DB at " + db_path);
    }
    init_schema();
}

SessionLogger::~SessionLogger() {
    if (db_) sqlite3_close(db_);
}

void SessionLogger::init_schema() {
    const char* schema = R"SQL(
    PRAGMA journal_mode=WAL;
    CREATE TABLE IF NOT EXISTS sessions (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        started_ms INTEGER NOT NULL,
        ended_ms INTEGER,
        spoken_ms INTEGER DEFAULT 0
    );
    CREATE TABLE IF NOT EXISTS vad_events (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id INTEGER NOT NULL,
        ts_ms INTEGER NOT NULL,
        state INTEGER NOT NULL,
        FOREIGN KEY(session_id) REFERENCES sessions(id)
    );
    )SQL";
    exec_sql(db_, schema);
}

std::int64_t SessionLogger::now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::int64_t SessionLogger::start_session() {
    const auto t = now_ms();
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "INSERT INTO sessions (started_ms) VALUES (?);", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, t);
    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        throw std::runtime_error("Failed to insert session");
    }
    sqlite3_finalize(st);
    return sqlite3_last_insert_rowid(db_);
}

void SessionLogger::end_session(std::int64_t session_id) {
    const auto t = now_ms();
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "UPDATE sessions SET ended_ms=? WHERE id=?;", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, t);
    sqlite3_bind_int64(st, 2, session_id);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

void SessionLogger::log_vad_event(std::int64_t session_id, bool speech) {
    const auto t = now_ms();
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "INSERT INTO vad_events (session_id, ts_ms, state) VALUES (?, ?, ?);", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, session_id);
    sqlite3_bind_int64(st, 2, t);
    sqlite3_bind_int(st, 3, speech ? 1 : 0);
    sqlite3_step(st);
    sqlite3_finalize(st);
}
