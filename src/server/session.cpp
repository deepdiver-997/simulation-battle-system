#include <server/session.h>

#include <server/battle_init.h>
#include <server/output_json.h>
#include <server/room.h>
#include <server/service.h>

#include <cstdio>

namespace server {
namespace {

void log_sess(const char* level, int id, const std::string& msg) {
    std::printf("[%s] [session %d] %s\n", level, id, msg.c_str());
    std::fflush(stdout);
}

}  // namespace

Session::Session(int id, const net::ConnectionPtr& conn, BattleService& service)
    : id_(id),
      conn_(conn),
      service_(service),
      // 成帧上限 1MB：比它大的帧一律判非法并断连（防"声称 2GB 包"的内存放大）。
      framer_(1u << 20, proto::kHeaderSize) {}

Session::~Session() = default;

bool Session::in_room() const {
    return !room_.expired();
}

RoomPtr Session::room() const {
    return room_.lock();
}

void Session::bind_room(const RoomPtr& room, int seat) {
    room_ = room;
    seat_ = seat;
}

void Session::send_frame(const std::string& frame) {
    if (!conn_) {
        return;
    }
    // Connection::write 自带跨线程投递：这里可能在战斗线程上被调用。
    conn_->write(frame);
}

void Session::send_error(const std::string& text) {
    send_payload(proto::Command::ERROR, text);
}

void Session::send_payload(proto::Command cmd, const std::string& payload) {
    RoomPtr r = room();
    const std::uint32_t uuid = r ? static_cast<std::uint32_t>(r->match_id()) : 0u;
    send_frame(proto::build_frame(cmd, uuid, payload));
}

void Session::on_read(const char* data, std::size_t len) {
    const bool ok = framer_.feed(data, len, [this](const char* frame, std::size_t frame_len) {
        proto::Header header;
        if (!proto::parse_header(frame, frame_len, header)) {
            return;
        }
        // 长度前缀成帧保证了 frame_len == header.total_length（成帧器切的就是它），
        // 所以 payload 直接按 header 算即可。
        const std::string payload(frame + proto::kHeaderSize, frame_len - proto::kHeaderSize);
        dispatch(header.command, header.uuid, payload);
    });

    if (!ok) {
        log_sess("WARN", id_, "illegal frame length, closing connection");
        if (conn_) {
            conn_->close();
        }
    }
}

void Session::on_close() {
    log_sess("INFO", id_, "connection closed");
}

void Session::dispatch(proto::Command cmd, std::uint32_t uuid, const std::string& payload) {
    switch (cmd) {
        case proto::Command::HELLO:
            // 版本握手（2026-10-01）：回引擎自描述指纹，网关/前端连接即核对。
            // payload 忽略（预留客户端自报版本）；uuid 也忽略——单连接单对局。
            (void)uuid;
            (void)payload;
            send_payload(proto::Command::HELLO, version_fingerprint_json());
            return;

        case proto::Command::INIT_BATTLE:
            handle_init_battle(uuid, payload, /*json_form=*/false);
            return;
        case proto::Command::INIT_BATTLE_JSON:
            handle_init_battle(uuid, payload, /*json_form=*/true);
            return;

        case proto::Command::SELECT_SKILL:
        case proto::Command::USE_MEDICINE:
        case proto::Command::CHOOSE_PET:
            handle_action(uuid, payload);
            return;

        case proto::Command::HEARTBEAT:
            // 心跳不改状态，直接回一条空的 HEARTBEAT。
            send_payload(proto::Command::HEARTBEAT, std::string());
            return;

        case proto::Command::SYNC_STATE: {
            RoomPtr r = room();
            if (!r) {
                send_error("no room");
                return;
            }
            send_payload(proto::Command::SYNC_STATE, r->state_json());
            return;
        }

        case proto::Command::DEBUG_FULLSTATE: {
            RoomPtr r = room();
            if (!r) {
                send_error("no room");
                return;
            }
            send_payload(proto::Command::DEBUG_FULLSTATE, r->full_state_json());
            return;
        }

        case proto::Command::DEBUG_STEP:
        case proto::Command::DEBUG_CONTINUE: {
            RoomPtr r = room();
            if (!r) {
                send_error("no room");
                return;
            }
            r->set_step_mode(cmd == proto::Command::DEBUG_STEP);
            return;
        }

        case proto::Command::DEBUG_MUTATE: {
            RoomPtr r = room();
            if (!r) {
                send_error("no room");
                return;
            }
            r->debug_mutate(payload);
            return;
        }

        case proto::Command::DEBUG_BREAKPOINT: {
            RoomPtr r = room();
            if (!r) {
                send_error("no room");
                return;
            }
            if (payload.size() < sizeof(std::int32_t)) {
                send_error("DEBUG_BREAKPOINT expects one int32 state id as payload");
                return;
            }
            r->toggle_breakpoint(static_cast<int>(proto::read_u32_be(payload.data())));
            return;
        }

        case proto::Command::LEGAL_ACTIONS: {
            RoomPtr r = room();
            if (!r) {
                send_error("no room");
                return;
            }
            // payload 可带一个 int32 指定玩家；不带则按"当前等着输入的那个玩家"。
            const int player = payload.size() >= sizeof(std::int32_t)
                                   ? static_cast<int>(proto::read_u32_be(payload.data()))
                                   : -1;
            send_payload(proto::Command::LEGAL_ACTIONS, r->legal_actions_json(player));
            return;
        }

        default:
            send_error(std::string("unsupported command: ") + proto::command_name(cmd));
            return;
    }
}

bool Session::ensure_room(std::string& err) {
    if (in_room()) {
        return true;
    }
    RoomPtr r = service_.assign_room(shared_from_this(), err);
    return r != nullptr;
}

void Session::handle_init_battle(std::uint32_t uuid, const std::string& payload, bool json_form) {
    (void)uuid;

    RoomPtr r = room();
    if (!r) {
        std::string err;
        if (!ensure_room(err)) {
            send_error("cannot join a room: " + err);
            return;
        }
        r = room();
        if (!r) {
            send_error("cannot join a room");
            return;
        }
    }

    if (json_form) {
        std::string err;
        if (r->solo()) {
            BattleCreateRequest request;
            if (!decode_lineup_json(payload, request, err)) {
                send_error("bad lineup: " + err);
                return;
            }
            if (!r->submit_lineup(request, err)) {
                send_error("lineup rejected: " + err);
            }
        } else {
            // pvp：提交的是"自己这一边的阵容"，落到自己那座对应的边。
            std::array<BattlePetMessage, 6> party{};
            if (!decode_party_json(payload, party, err)) {
                send_error("bad party: " + err);
                return;
            }
            if (!r->submit_side(seat_, party, err)) {
                send_error("party rejected: " + err);
            }
        }
        return;
    }

    // 兼容既有二进制格式：每只精灵 13 个 int32。solo 用；pvp 下用它作为"双方阵容"。
    try {
        const BattleCreateRequest request = decode_battle_create_request(
            std::vector<char>(payload.begin(), payload.end()));
        std::string err;
        if (!r->submit_lineup(request, err)) {
            send_error("lineup rejected: " + err);
        }
    } catch (const std::exception& ex) {
        send_error(std::string("bad binary lineup: ") + ex.what());
    }
}

void Session::handle_action(std::uint32_t uuid, const std::string& payload) {
    (void)uuid;
    RoomPtr r = room();
    if (!r) {
        send_error("no room (send INIT_BATTLE first)");
        return;
    }

    const proto::ActionPayload a = proto::decode_action_payload(payload.data(), payload.size());
    if (a.robot < 0 || a.action_type < 0 || a.index < 0) {
        send_error("action payload must be 3 int32: robot, action_type, index");
        return;
    }

    // 座位归属与时机校验都在 Room 里做（它才知道谁在等、等的是哪个玩家）。
    r->submit_action(seat_, a.robot, a.action_type, a.index);
}

}  // namespace server
