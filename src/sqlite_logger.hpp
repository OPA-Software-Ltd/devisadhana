#pragma once
#include <sqlite3.h>
#include <cstdint>
#include <string>

// Lightweight session/event logger for VAD state changes.
// Schema:
//  - sessions(id INTEGER PK, started_ms INTEGER, ended_ms INTEGER, spoken_ms INTEGER)
//  - vad_events(id INTEGER PK, session_id INTEGER, ts_ms INTEGER, state INTEGER)
//
// Notes:
//  * Times use steady_clock millis (monotonic). If you prefer wall clock,
//    swap now_ms() to system_clock.
//  * Threading: This class is NOT thread-safe. Call its methods from the
//    same thread (we currently call from the PortAudio callback thread only
//    to enqueue events and from the main thread to open/close). If you plan
//    to log from multiple threads, add a small lock or a queue.
class SessionLogger {
public:
    explicit SessionLogger(const std::string& db_path);
    ~SessionLogger();

    // Begins a session; returns the new session id.
    std::int64_t start_session();

    // Marks end time for a session.
    void end_session(std::int64_t session_id);

    // Inserts a VAD transition event: state = true (speech) / false (silence)
    void log_vad_event(std::int64_t session_id, bool speech);

    // Accessor: the path used to open the DB.
    const std::string& path() const { return db_path_; }

private:
    void init_schema();
    static std::int64_t now_ms();

    std::string db_path_;
    sqlite3* db_ = nullptr;
};
