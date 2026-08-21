#pragma once

#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>
#include <cstring>


namespace db::codec {
inline void put(std::vector<std::byte>& out, const void* src, size_t n) {
    auto p = static_cast<const std::byte*>(src);
    out.insert(out.end(), p, p + n);
}
inline void put_str(std::vector<std::byte>& out, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    put(out, &len, sizeof(len));
    put(out, s.data(), s.size());
}

// 읽기
struct Reader {
    const std::byte* p;
    size_t len, off = 0;

    bool take(void* dst, size_t n) {
        if (off + n > len) return false;
        std::memcpy(dst, p + off, n);
        off += n;
        return true;
    }
    bool take_str(std::string& s) {
        uint32_t l;
        if (!take(&l, sizeof(l))) return false;
        if (off + l > len) return false;
        s.assign(reinterpret_cast<const char*>(p + off), l);
        off += l;
        return true;
    }
};
}
