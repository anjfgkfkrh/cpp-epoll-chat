#pragma once

// 바이트 직렬화·파싱 도구. 도메인을 모르는 최하위 계층이다.
// 어떤 필드가 어떤 순서로 오는지는 상위 계층(Message.h 등)이 정한다.

#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace codec {

/*-------- 쓰기 --------*/
inline void put(std::vector<std::byte>& out, const void* src, size_t n) {
    auto p = static_cast<const std::byte*>(src);
    out.insert(out.end(), p, p + n);
}

// 문자열은 길이(uint32)를 앞에 붙인다. 읽는 쪽이 경계를 알 수 있어야 하기 때문이다.
inline void put_str(std::vector<std::byte>& out, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    put(out, &len, sizeof(len));
    put(out, s.data(), s.size());
}

/*-------- 읽기 --------*/
// 모든 읽기가 남은 길이를 먼저 검사한다. 길이 값은 신뢰할 수 없는 입력이다.
class Reader {
public:
    explicit Reader(const std::vector<std::byte>& buf) : buf_(buf) {}

    bool take(void* dst, size_t n) {
        if (off_ + n > buf_.size()) return false;
        std::memcpy(dst, buf_.data() + off_, n);
        off_ += n;
        return true;
    }

    bool take_str(std::string& s) {
        uint32_t len = 0;
        if (!take(&len, sizeof(len)))       return false;
        if (off_ + len > buf_.size())       return false;
        s.assign(reinterpret_cast<const char*>(buf_.data() + off_), len);
        off_ += len;
        return true;
    }

    size_t remaining() const { return buf_.size() - off_; }

private:
    const std::vector<std::byte>& buf_;
    size_t off_ = 0;
};

}
