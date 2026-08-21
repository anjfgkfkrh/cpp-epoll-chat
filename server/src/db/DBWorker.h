#pragma once

#include <libpq-fe.h>
#include "DBConnection.h"
#include "DBJob.h"
#include "DBResult.h"

#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <unordered_map>
#include <cstdint>

class DBWorker {
private:
    DBConnection conn_;

    std::thread thread_;
    std::atomic<bool> running_;

    std::queue<DBJob> job_queue_;
    std::mutex job_queue_mutex_;
    std::condition_variable cv_;

public:
    DBWorker();
    ~DBWorker();

    void start();
    void stop();

    void post_job(DBJob&&);

private:
    void thread_main();
    void process_job(DBJob&&);
    void on_complete(DBResult&&);
    DBStatus status_of(DBJobType, DBResult&);
};