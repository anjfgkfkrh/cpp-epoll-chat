#pragma once

class DBResult {
private:
    PGresult* res_ = nullptr;

public:
    explicit DBResult(PGresult* res) : res_(res) {}
    ~DBResult() { if (res_) PQclear(res_); }

    bool ok();
};