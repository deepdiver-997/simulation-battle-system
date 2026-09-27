// 平台 poller 实现：macOS/BSD 走 kqueue，Linux 走 epoll。
//
// 两者都是 readiness 语义，差异只在注册方式和事件翻译，所以共用一套
// "待应用变更表 + 注册状态表" 的结构：
//   - changes_   本线程内累积的 control() 请求，在 wait() 开头一次性提交，
//                避免"每次注册一次 syscall"（写队列空↔非空会频繁切关注集合）。
//   - registered_ 已经生效的 (fd → 关注位) 映像。有了它就不会对没注册过的
//                filter 发 DELETE（kqueue 会回 ENOENT、epoll 会回 ENOENT，
//                都不是错误，但日志会脏）。

#include <net/poller.h>

#include <cerrno>
#include <cstring>
#include <unordered_map>
#include <vector>

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define NET_POLLER_KQUEUE 1
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(__linux__)
#define NET_POLLER_EPOLL 1
#include <sys/epoll.h>
#include <unistd.h>
#else
#define NET_POLLER_NONE 1
#endif

namespace net {
namespace {

#if defined(NET_POLLER_KQUEUE)

constexpr uint8_t kWantRead = 1u << 0;
constexpr uint8_t kWantWrite = 1u << 1;

class KqueuePoller final : public Poller {
public:
    KqueuePoller() {
        kq_ = ::kqueue();
    }

    ~KqueuePoller() override {
        if (kq_ >= 0) {
            ::close(kq_);
        }
    }

    bool control(int fd, bool want_read, bool want_write, PollOp op) override {
        if (kq_ < 0) {
            errno = EBADF;
            return false;
        }
        changes_.push_back(Change{fd, want_read, want_write, op});
        return true;
    }

    int wait(PollEvent* out, int max_events, int timeout_ms) override {
        if (kq_ < 0) {
            errno = EBADF;
            return -1;
        }
        if (max_events <= 0) {
            return 0;
        }

        if (!apply_changes()) {
            return -1;
        }
        // max_events 受 evlist 容量约束，这里按调用方给的容量收事件。
        // 平台限制：kevent 的 nchanges 参数是 int，超出由 apply_changes 分批处理。
        if (max_events > kMaxEvents) {
            max_events = kMaxEvents;
        }
        struct kevent events[kMaxEvents];

        struct timespec ts;
        struct timespec* ts_ptr = nullptr;
        if (timeout_ms >= 0) {
            ts.tv_sec = timeout_ms / 1000;
            ts.tv_nsec = static_cast<long>(timeout_ms % 1000) * 1000000L;
            ts_ptr = &ts;
        }

        int n = 0;
        do {
            n = ::kevent(kq_, nullptr, 0, events, max_events, ts_ptr);
        } while (n < 0 && errno == EINTR);

        if (n < 0) {
            return -1;
        }

        int produced = 0;
        for (int i = 0; i < n; ++i) {
            const struct kevent& ev = events[i];
            const int fd = static_cast<int>(ev.ident);

            if (ev.flags & EV_ERROR) {
                out[produced++] = PollEvent{fd, false, false, true};
                continue;
            }
            if (ev.filter == EVFILT_READ) {
                // EV_EOF 时仍标记可读：上层 read() 会拿到 0，据此关闭连接。
                out[produced++] = PollEvent{fd, true, false, false};
            } else if (ev.filter == EVFILT_WRITE) {
                // 对端已关时写方向也会 EOF，此时写必然失败，直接标记错误让上层关掉。
                const bool peer_gone = (ev.flags & EV_EOF) != 0;
                out[produced++] = PollEvent{fd, false, !peer_gone, peer_gone};
            }
        }
        return produced;
    }

private:
    // 编译期上限，避免变长数组。
    static constexpr int kMaxEvents = 256;

    struct Change {
        int fd;
        bool read;
        bool write;
        PollOp op;
    };

    bool apply_changes() {
        if (changes_.empty()) {
            return true;
        }

        std::vector<struct kevent> kevs;
        kevs.reserve(changes_.size() * 2);

        for (const Change& c : changes_) {
            const uint8_t prev = registered_.count(c.fd) ? registered_[c.fd] : 0u;
            const bool had_read = (prev & kWantRead) != 0;
            const bool had_write = (prev & kWantWrite) != 0;

            const bool want_read = (c.op == PollOp::kDel) ? false : c.read;
            const bool want_write = (c.op == PollOp::kDel) ? false : c.write;

            auto push = [&kevs](int fd, int16_t filter, uint16_t flags) {
                struct kevent ev;
                EV_SET(&ev, static_cast<uintptr_t>(fd), filter, flags, 0, 0, nullptr);
                kevs.push_back(ev);
            };

            // 只在关注位真的变化时才动 filter，减 syscall、也避免无意义的 ENOENT。
            if (want_read && !had_read) {
                push(c.fd, EVFILT_READ, EV_ADD | EV_ENABLE);
            } else if (!want_read && had_read) {
                push(c.fd, EVFILT_READ, EV_DELETE);
            }
            if (want_write && !had_write) {
                push(c.fd, EVFILT_WRITE, EV_ADD | EV_ENABLE);
            } else if (!want_write && had_write) {
                push(c.fd, EVFILT_WRITE, EV_DELETE);
            }

            if (c.op == PollOp::kDel) {
                registered_.erase(c.fd);
            } else {
                uint8_t next = 0;
                if (want_read) next |= kWantRead;
                if (want_write) next |= kWantWrite;
                if (next == 0) {
                    registered_.erase(c.fd);
                } else {
                    registered_[c.fd] = next;
                }
            }
        }

        changes_.clear();
        if (kevs.empty()) {
            return true;
        }

        // kevent 的 nchanges 是 int；本层连接数远小于 INT_MAX，直接提交。
        //
        // ⚠️ 必须给**同等大小的返回数组**：kqueue 处理 changelist 时遇到出错条目，
        // 若没有地方放错误事件，会**立即中止整批**——排在出错项之后的 EV_ADD 全部
        // 静默丢弃，而本层的 registered_ 簿记已把整批记为生效 → 之后永远不再注册
        // → 那条连接永远不会可读 → 客户端阻塞在 send 里无限挂死（实测 2026-09-22，
        // fd 高速复用时 EV_DELETE 打在已关闭 fd 上的 ENOENT/EBADF 触发，~1-2% 频率）。
        // 配了返回数组后，内核把每条出错项作为 EV_ERROR 事件逐项报告并**继续处理
        // 剩余变更**；ENOENT/EBADF 是"注销已消失 fd"的良性结果，其余错误才算真失败。
        if (kevs.size() > kMaxChanges) {
            return false;  // 单批过大：理论上到不了（连接数 << INT_MAX），防御
        }
        std::vector<struct kevent> errors(kevs.size());
        int rc = 0;
        do {
            rc = ::kevent(kq_, kevs.data(), static_cast<int>(kevs.size()), errors.data(),
                          static_cast<int>(errors.size()), nullptr);
        } while (rc < 0 && errno == EINTR);

        if (rc < 0) {
            return false;
        }
        for (int i = 0; i < rc; ++i) {
            if (errors[i].flags & EV_ERROR) {
                const int err = static_cast<int>(errors[i].data);
                if (err != ENOENT && err != EBADF) {
                    errno = err;
                    return false;
                }
                // 良性：注销打在已被内核自动摘除的 fd 上，簿记已同步，忽略。
            }
        }
        return true;
    }

    int kq_ = -1;
    std::vector<Change> changes_;
    std::unordered_map<int, uint8_t> registered_;
    static constexpr std::size_t kMaxChanges = 4096;
};

#elif defined(NET_POLLER_EPOLL)

constexpr uint32_t kWantRead = EPOLLIN;
constexpr uint32_t kWantWrite = EPOLLOUT;

class EpollPoller final : public Poller {
public:
    EpollPoller() {
        ep_ = ::epoll_create1(EPOLL_CLOEXEC);
    }

    ~EpollPoller() override {
        if (ep_ >= 0) {
            ::close(ep_);
        }
    }

    bool control(int fd, bool want_read, bool want_write, PollOp op) override {
        if (ep_ < 0) {
            errno = EBADF;
            return false;
        }
        changes_.push_back(Change{fd, want_read, want_write, op});
        return true;
    }

    int wait(PollEvent* out, int max_events, int timeout_ms) override {
        if (ep_ < 0) {
            errno = EBADF;
            return -1;
        }
        if (max_events <= 0) {
            return 0;
        }
        if (!apply_changes()) {
            return -1;
        }
        if (max_events > kMaxEvents) {
            max_events = kMaxEvents;
        }
        struct epoll_event events[kMaxEvents];

        int n = 0;
        do {
            n = ::epoll_wait(ep_, events, max_events, timeout_ms);
        } while (n < 0 && errno == EINTR);

        if (n < 0) {
            return -1;
        }

        int produced = 0;
        for (int i = 0; i < n; ++i) {
            const struct epoll_event& ev = events[i];
            const int fd = ev.data.fd;
            const uint32_t mask = ev.events;

            const bool error = (mask & (EPOLLERR | EPOLLHUP)) != 0;
            const bool readable = (mask & (EPOLLIN | EPOLLRDHUP)) != 0;
            const bool writable = (mask & EPOLLOUT) != 0;

            // 同一个 fd 在 LT 模式下只会产生一条事件，直接落。
            out[produced++] = PollEvent{fd, readable, writable, error};
        }
        return produced;
    }

private:
    static constexpr int kMaxEvents = 256;

    struct Change {
        int fd;
        bool read;
        bool write;
        PollOp op;
    };

    bool apply_changes() {
        if (changes_.empty()) {
            return true;
        }

        bool ok = true;
        for (const Change& c : changes_) {
            if (c.op == PollOp::kDel) {
                if (registered_.erase(c.fd) > 0) {
                    if (::epoll_ctl(ep_, EPOLL_CTL_DEL, c.fd, nullptr) < 0 && errno != ENOENT &&
                        errno != EBADF) {
                        ok = false;
                    }
                }
                continue;
            }

            uint32_t mask = 0;
            if (c.read) mask |= kWantRead;
            if (c.write) mask |= kWantWrite;

            const bool exists = registered_.count(c.fd) > 0;
            if (mask == 0) {
                if (exists) {
                    registered_.erase(c.fd);
                    if (::epoll_ctl(ep_, EPOLL_CTL_DEL, c.fd, nullptr) < 0 && errno != ENOENT &&
                        errno != EBADF) {
                        ok = false;
                    }
                }
                continue;
            }

            struct epoll_event ev;
            std::memset(&ev, 0, sizeof(ev));
            ev.events = mask;
            ev.data.fd = c.fd;

            const int op = exists ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
            if (::epoll_ctl(ep_, op, c.fd, &ev) < 0) {
                if (errno != ENOENT && errno != EBADF) {
                    ok = false;
                }
            } else {
                registered_[c.fd] = mask;
            }
        }
        changes_.clear();
        return ok;
    }

    int ep_ = -1;
    std::vector<Change> changes_;
    std::unordered_map<int, uint32_t> registered_;
};

#endif  // platform

}  // namespace

std::unique_ptr<Poller> Poller::create() {
#if defined(NET_POLLER_KQUEUE)
    return std::unique_ptr<Poller>(new KqueuePoller());
#elif defined(NET_POLLER_EPOLL)
    return std::unique_ptr<Poller>(new EpollPoller());
#else
    return nullptr;
#endif
}

const char* Poller::backend_name() {
#if defined(NET_POLLER_KQUEUE)
    return "kqueue";
#elif defined(NET_POLLER_EPOLL)
    return "epoll";
#else
    return "none";
#endif
}

}  // namespace net
