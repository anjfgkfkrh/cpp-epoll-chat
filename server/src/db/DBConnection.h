#pragma once

#include <libpq-fe.h>
#include <string>
#include <vector>
#include "DBResult.h"

class DBConnection {
private:
    PGconn* conn_ = nullptr;

public:
    explicit DBConnection(const std::string& conninfo = "");
    ~DBConnection() { if (conn_) PQfinish(conn_); }

    DBConnection(const DBConnection&) = delete;
    DBConnection& operator=(const DBConnection&) = delete;
    DBConnection(DBConnection&& other) noexcept : conn_(other.conn_) { other.conn_ = nullptr; }

    DBResult exec_once(const char* sql, const std::vector<std::string>& params);
    DBResult exec(const char* sql, const std::vector<std::string>& params);

    bool is_alive();
    bool reconnect();
};