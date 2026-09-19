#ifndef NET_FRAMING_H
#define NET_FRAMING_H

// 长度前缀成帧：与既有协议一致 —— [4B 大端 total_length（含前缀自身）][2B cmd][4B uuid][payload...]
//
// 只负责"把字节流切成完整帧"，不理解命令，也不管 IO。放在 net 层是因为
// 成帧是传输层的事，而它必须能单测（见 test/test_net_reactor.cpp）。

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace net {

class LengthPrefixedFramer {
public:
    using FrameHandler = std::function<void(const char* frame, std::size_t len)>;

    // max_frame: 单帧上限，防"声称 4GB 包"的内存放大攻击。
    // min_frame: 协议最小帧长（现有协议 10 字节头）。
    explicit LengthPrefixedFramer(std::size_t max_frame = 1u << 20, std::size_t min_frame = 10)
        : max_frame_(max_frame), min_frame_(min_frame) {}

    // 追加字节并尽力切帧；每切出一帧回调一次。
    // 返回 false 表示帧头声明长度非法 —— 调用方必须断开该连接（流已经不可能同步）。
    bool feed(const char* data, std::size_t len, const FrameHandler& on_frame) {
        if (!ok_) {
            return false;
        }
        buf_.insert(buf_.end(), data, data + len);

        for (;;) {
            const std::size_t available = buf_.size() - consumed_;
            if (available < kPrefixBytes) {
                break;
            }
            const unsigned char* p = reinterpret_cast<const unsigned char*>(buf_.data() + consumed_);
            const std::uint32_t total = (static_cast<std::uint32_t>(p[0]) << 24) |
                                        (static_cast<std::uint32_t>(p[1]) << 16) |
                                        (static_cast<std::uint32_t>(p[2]) << 8) |
                                        static_cast<std::uint32_t>(p[3]);
            if (total < min_frame_ || total > max_frame_) {
                ok_ = false;
                return false;
            }
            if (available < total) {
                break;
            }
            if (on_frame) {
                on_frame(buf_.data() + consumed_, total);
            }
            consumed_ += total;
        }

        compact();
        return ok_;
    }

    bool ok() const { return ok_; }
    std::size_t buffered() const { return buf_.size() - consumed_; }
    std::size_t max_frame() const { return max_frame_; }

    void clear() {
        buf_.clear();
        consumed_ = 0;
        ok_ = true;
    }

private:
    // 已消费前缀攒够一半就搬一次，避免每帧一次 O(n) 前移。
    void compact() {
        if (consumed_ == 0) {
            return;
        }
        if (consumed_ == buf_.size()) {
            buf_.clear();
            consumed_ = 0;
            return;
        }
        if (consumed_ >= 64 * 1024 || consumed_ * 2 >= buf_.size()) {
            buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(consumed_));
            consumed_ = 0;
        }
    }

    static constexpr std::size_t kPrefixBytes = 4;

    std::vector<char> buf_;
    std::size_t consumed_ = 0;
    std::size_t max_frame_ = 1u << 20;
    std::size_t min_frame_ = 10;
    bool ok_ = true;
};

}  // namespace net

#endif  // NET_FRAMING_H
