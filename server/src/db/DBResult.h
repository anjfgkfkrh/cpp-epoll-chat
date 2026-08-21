#pragma once

#include <libpq-fe.h>
#include <string>
#include <cstdint>

class DBResult {
private:
    PGresult* res_ = nullptr;

public:
    DBResult() {}
    explicit DBResult(PGresult* res) : res_(res) {}
    ~DBResult() { if (res_) PQclear(res_); }

    DBResult(const DBResult&) = delete;
    DBResult& operator=(const DBResult&) = delete;
    DBResult(DBResult&& o) noexcept : res_(o.res_) { o.res_ = nullptr;}
    DBResult& operator=(DBResult&& o) noexcept {
        if (this != &o) { if(res_) PQclear(res_); res_ = o.res_; o.res_ = nullptr; }
        return *this;
    }


    /*-------- 상태 ---------*/
    bool ok() const {
        if(!res_) return false;
        ExecStatusType s = PQresultStatus(res_);
        return s == PGRES_TUPLES_OK || s == PGRES_COMMAND_OK;
    }

    const char* status_name() const {
        return res_ ? PQresStatus(PQresultStatus(res_)) : "NULL_RESULT";
    }

    std::string error() const {
        return res_ ? PQresultErrorMessage(res_) : "result it null";
    }

    std::string sqlstate() const {
        if(!res_) return {};
        const char* s = PQresultErrorField(res_, PG_DIAG_SQLSTATE);
        return s ? s : std::string{};
    }



    /*-------- 크기 ---------*/
    int rows() const { return res_ ? PQntuples(res_) : 0; }
    int cols() const { return res_ ? PQnfields(res_) : 0; }
    int affected_rows() const {                                 // INSERT/UPDATE 영향 행 수
        if(!res_) return 0;
        const char* n = PQcmdTuples(res_);
        return (n && *n) ? std::atoi(n) : 0;
    }



    /*------ 값 읽기 -------*/
    bool is_null(int row, const char* col) const {
        return PQgetisnull(res_, row, PQfnumber(res_, col)) == 1;
    }

    std::string get_string(int row, const char* col) const {
        int f = PQfnumber(res_,col);
        if(f < 0 || PQgetisnull(res_, row, f)) return {};
        return std::string(PQgetvalue(res_, row, f));       // 복사 필수
    }

    int64_t get_int64(int row, const char* col) const {
        std::string v = get_string(row, col);
        if (v.empty()) return 0;
        try { return std::stoll(v); }
        catch (...) { return 0; }
    }
};