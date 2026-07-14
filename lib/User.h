#include <string>

// 유저 정보를 관리하는 데이터 컨테이너 클래스
class User {
private:
    int fd_;
    uint16_t user_id_;
    std::string user_name_;
    uint16_t current_room_;
public:
    User(int fd, uint16_t user_id, std::string user_name);
    ~User();

    void set_name(std::string user_name);
    int get_fd();
    uint16_t get_id();
    const std::string& get_name();

    // 복사 금지
    User(const User&) = delete;
    User& operator=(const User&) = delete;

    // 이동 허용
    User(User&& other) noexcept;
    User& operator=(User&& other) noexcept;
};