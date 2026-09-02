#include "DBWorker.h"

#include "AccountRepository.h"
#include "MessageRepository.h"
#include "RoomRepository.h"

#include <iostream>

DBWorker::DBWorker() : conn_("application_name=chatserver connect_timeout=5") {}
DBWorker::~DBWorker() { stop(); }

void DBWorker::start() {
    running_.store(true);

    thread_ = std::thread(&DBWorker::thread_main, this);
}

void DBWorker::stop() {
    {
        std::lock_guard lock(job_queue_mutex_);

        running_.store(false);
    }

    cv_.notify_one();

    if(thread_.joinable())
        thread_.join();

    // 스레드 종료 후 큐에 남은 job을 실패로 마무리한다.
    // 그냥 버리면 콜백이 호출되지 않아 요청자가 응답을 영영 기다린다.
    std::lock_guard lock(job_queue_mutex_);
    while(!job_queue_.empty()) {
        auto job = std::move(job_queue_.front());
        job_queue_.pop();

        if(job.on_result)
            job.on_result(DBStatus::Error, {});
    }
}

void DBWorker::post_job(DBJob&& job) {
    {
        std::lock_guard lock(job_queue_mutex_);

        job_queue_.emplace(std::move(job));
    }

    cv_.notify_one();
}

void DBWorker::thread_main() {
    while(running_.load()){
        std::unique_lock lock(job_queue_mutex_);
        cv_.wait(lock, [this]
        {
            return !running_ || !job_queue_.empty();
        });

        while(!job_queue_.empty()) {
            auto job = std::move(job_queue_.front());
            job_queue_.pop();

            lock.unlock();

            try { process_job(std::move(job)); } 
            catch ( const std::exception& e ) { std::cerr << "DBWorker 예외: " << e.what() << std::endl; }

            lock.lock();
        }
    }
}


void DBWorker::process_job(DBJob&& job) {
    DBResult res;
    std::vector<std::byte> data;

    switch(job.type) {
    case DBJobType::SaveMessage:
        res = db::message::save(conn_, job.params);
        data = db::message::serialize(res);
        break;
    case DBJobType::LoadHistory:
        res = db::message::load_recent(conn_, job.params);
        data = db::message::serialize(res);
        break;
    case DBJobType::CreateAccount:
        res = db::account::create(conn_, job.params);
        data = db::account::serialize(res);
        break;
    case DBJobType::CreateRoom:
        res = db::room::create(conn_, job.params);
        data = db::room::serialize(res);
        break;
    case DBJobType::FindAccount:
        res = db::account::find(conn_, job.params);
        data = db::account::serialize(res);
        break;
    }

    if(job.on_result)
        job.on_result(status_of(job.type, res), std::move(data));
}


DBStatus DBWorker::status_of(DBJobType type, DBResult& result) {
    if(!result.ok()) {
        const std::string ss = result.sqlstate();
        if(ss == "23505") return DBStatus::Duplicate;
        return DBStatus::Error;
    }

    // 여기부터는 쿼리 성공. 0행의 의미가 job마다 다르다
    switch (type) {
    case DBJobType::FindAccount:
        return result.rows() == 0 ? DBStatus::NotFound : DBStatus::Success;
    case DBJobType::SaveMessage:    // INSERT — 0행이 정상
    case DBJobType::LoadHistory:    // 빈 방 — 0행이 정상
    case DBJobType::CreateAccount:  // RETURNING — 1행
    case DBJobType::CreateRoom:
        return DBStatus::Success;
    }
    return DBStatus::Success;
}