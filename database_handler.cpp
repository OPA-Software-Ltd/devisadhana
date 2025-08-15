#pragma once
#include <string>
#include <sqlite3.h>
#include <memory>

namespace sadhana {

class Database {
public:
    explicit Database(const std::string& path);
    bool init();

    // Session management
    int64_t startSession();
    void endSession(int64_t sessionId);
    void logVADEvent(int64_t sessionId, bool state);
    void logRecitation(int64_t sessionId, int64_t phraseId, int64_t timestamp);

private:
    std::string dbPath_;
    std::unique_ptr<sqlite3, void(*)(sqlite3*)> db_;

    bool createTables();
};

} // namespace sadhana