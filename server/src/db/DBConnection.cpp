#include "DBConnection.h"

#include <iostream>

DBConnection::DBConnection(const std::string& conninfo) {
    conn_ = PQconnectdb(conninfo.c_str());
    if (PQstatus(conn_) != CONNECTION_OK)
        std::cerr << "DB 연결 실패: " << PQerrorMessage(conn_) << std::endl;
}

DBConnection::DBConnection(DBConnection&& other) noexcept {
    conn_ = other.conn_;
    other.conn_ = nullptr;
}

DBResult DBConnection::exec_once(const char* sql, const std::vector<std::string>& params) {
    std::vector<const char*> values;
    values.reserve(params.size());
    for (const auto& p : params)
        values.push_back(p.c_str());

    PGresult* res = PQexecParams(
        conn_, 
        sql, 
        static_cast<int>(params.size()),
        nullptr,                                    // paramsTypes: 서버가 추론
        values.empty() ? nullptr : values.data(),
        nullptr,                                    // paramsLengths: 텍스트라 불필요
        nullptr,                                    // paramsFormats: 전부 텍스트
        0);                                         // resultFormat: 텍스트

    return DBResult(res);
}

DBResult DBConnection::exec(const char* sql, const std::vector<std::string>& params) {
    DBResult res = exec_once(sql, params);

    if(!res.ok() && !is_alive()) {          // 연결이 끊겨서 실패한 경우만
        if(reconnect())
            res = exec_once(sql, params);   // 1회만 재시도
    }

    return res;
}

bool DBConnection::is_alive() {
    return conn_ != nullptr && PQstatus(conn_) == CONNECTION_OK;
}

bool DBConnection::reconnect() {
    if(!conn_) return false;
    PQreset(conn_);                             // 원래 conninof 그대로 재연결
    return PQstatus(conn_) == CONNECTION_OK;
}