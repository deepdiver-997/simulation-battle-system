#include <server/output_json.h>

#include <effects/state_tape.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>

#include <sstream>

namespace server {
namespace {

// 追加一个 JSON 字段值的公共写法：调用方负责逗号（保持与 getStateJson 一致的朴素风格，
// 手写拼接虽然土，但输出完全可预测，出问题时肉眼能直接对上）。
void append_u32(std::ostringstream& oss, std::uint32_t v) {
    oss << v;
}

}  // namespace

std::string json_escape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (unsigned char c : in) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    // UTF-8 原样透传：前端按 UTF-8 解，不必转 \u。
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

const char* event_type_name(int event_type) {
    switch (static_cast<EventType>(event_type)) {
        case EventType::EVENT_BREAK: return "EVENT_BREAK";
        case EventType::EVENT_DEATH: return "EVENT_DEATH";
        case EventType::EVENT_CONTROLLED: return "EVENT_CONTROLLED";
        case EventType::EVENT_ANOMALY_APPLIED: return "EVENT_ANOMALY_APPLIED";
        case EventType::EVENT_HIT: return "EVENT_HIT";
        case EventType::EVENT_TAKE_DAMAGE: return "EVENT_TAKE_DAMAGE";
        case EventType::EVENT_SHIELD_BROKEN: return "EVENT_SHIELD_BROKEN";
        case EventType::EVENT_ENTER_STAGE: return "EVENT_ENTER_STAGE";
        case EventType::EVENT_SWAP: return "EVENT_SWAP";
        case EventType::EVENT_OPPONENT_DEFEATED: return "EVENT_OPPONENT_DEFEATED";
        case EventType::EVENT_SKILL_INVALID: return "EVENT_SKILL_INVALID";
        case EventType::EVENT_ATTACK_BLOCKED: return "EVENT_ATTACK_BLOCKED";
        case EventType::EVENT_SKILL_ARMOR_RESOLVED: return "EVENT_SKILL_ARMOR_RESOLVED";
        case EventType::EVENT_TAKE_PINK_DAMAGE: return "EVENT_TAKE_PINK_DAMAGE";
        case EventType::EVENT_HP_TO_ZERO: return "EVENT_HP_TO_ZERO";
        case EventType::EVENT_VANISH: return "EVENT_VANISH";
    }
    return "EVENT_UNKNOWN";
}

std::string tape_to_json(int match_id, const std::vector<StateSample>& samples) {
    if (samples.empty()) {
        return std::string();
    }

    std::ostringstream oss;
    oss << "{\"match\":" << match_id;
    oss << ",\"from\":" << samples.front().seq;
    oss << ",\"to\":" << samples.back().seq;
    oss << ",\"samples\":[";

    for (std::size_t i = 0; i < samples.size(); ++i) {
        const StateSample& s = samples[i];
        if (i > 0) {
            oss << ",";
        }
        oss << "{";
        oss << "\"seq\":" << s.seq;
        oss << ",\"state\":" << s.state;
        oss << ",\"stateName\":\"" << json_escape(state_name_cn(static_cast<State>(s.state))) << "\"";
        oss << ",\"round\":" << s.round;
        oss << ",\"actor\":" << s.actor;
        oss << ",\"hp\":[" << s.hp[0] << "," << s.hp[1] << "]";
        oss << ",\"maxHp\":[" << s.max_hp[0] << "," << s.max_hp[1] << "]";
        oss << ",\"levels\":[[";
        for (int k = 0; k < kSampleLevelSlots; ++k) {
            if (k > 0) oss << ",";
            oss << s.levels[0][k];
        }
        oss << "],[";
        for (int k = 0; k < kSampleLevelSlots; ++k) {
            if (k > 0) oss << ",";
            oss << s.levels[1][k];
        }
        oss << "]]";
        oss << ",\"pendingDamage\":" << s.pending_damage;
        oss << ",\"resolvedDamage\":" << s.resolved_damage;

        oss << ",\"events\":[";
        for (std::size_t e = 0; e < s.events.size(); ++e) {
            const BattleEvent& ev = s.events[e];
            if (e > 0) {
                oss << ",";
            }
            oss << "{";
            oss << "\"type\":\"" << event_type_name(static_cast<int>(ev.type)) << "\"";
            oss << ",\"actor\":" << ev.actor;
            oss << ",\"target\":" << ev.target;
            oss << ",\"amount\":" << ev.amount;
            oss << ",\"state\":" << ev.state;
            if (ev.slot >= 0) {
                oss << ",\"slot\":" << ev.slot;
            }
            oss << "}";
        }
        oss << "]";

        oss << "}";
    }

    oss << "]}";
    return oss.str();
}

std::string legal_actions_to_json(const LegalActions& legal) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"canAct\":" << (legal.can_act ? "true" : "false");
    oss << ",\"reason\":\"" << json_escape(legal.reason) << "\"";
    oss << ",\"mustChoosePet\":" << (legal.must_choose_pet ? "true" : "false");

    oss << ",\"skills\":[";
    for (std::size_t i = 0; i < legal.skills.size(); ++i) {
        const LegalSkill& s = legal.skills[i];
        if (i > 0) oss << ",";
        oss << "{\"index\":" << s.index
            << ",\"id\":" << s.id
            << ",\"name\":\"" << json_escape(s.name) << "\""
            << ",\"pp\":" << s.pp
            << ",\"maxPp\":" << s.max_pp
            << ",\"usable\":" << (s.usable ? "true" : "false")
            << ",\"reason\":\"" << json_escape(s.reason) << "\"}";
    }
    oss << "]";

    oss << ",\"pets\":[";
    for (std::size_t i = 0; i < legal.pets.size(); ++i) {
        const LegalPet& p = legal.pets[i];
        if (i > 0) oss << ",";
        oss << "{\"slot\":" << p.slot
            << ",\"id\":" << p.id
            << ",\"name\":\"" << json_escape(p.name) << "\""
            << ",\"hp\":" << p.hp
            << ",\"maxHp\":" << p.max_hp
            << ",\"onStage\":" << (p.on_stage ? "true" : "false")
            << ",\"usable\":" << (p.usable ? "true" : "false")
            << ",\"reason\":\"" << json_escape(p.reason) << "\"}";
    }
    oss << "]";

    oss << ",\"medicines\":[";
    for (std::size_t i = 0; i < legal.medicines.size(); ++i) {
        const LegalMedicine& m = legal.medicines[i];
        if (i > 0) oss << ",";
        oss << "{\"index\":" << m.index
            << ",\"type\":" << m.type
            << ",\"count\":" << m.count
            << ",\"usable\":" << (m.usable ? "true" : "false") << "}";
    }
    oss << "]";
    oss << "}";
    return oss.str();
}

std::string input_required_to_json(int match_id, BattleContext& ctx, int player,
                                   const std::string& snapshot_json) {
    const LegalActions legal = compute_legal_actions(ctx, player);
    std::ostringstream oss;
    oss << "{";
    oss << "\"match\":" << match_id;
    oss << ",\"player\":" << player;
    oss << ",\"round\":" << ctx.roundCount;
    oss << ",\"state\":" << static_cast<int>(ctx.currentState);
    oss << ",\"stateName\":\"" << json_escape(state_name_cn(ctx.currentState)) << "\"";
    // mustChoosePet 决定前端切哪套界面：选技能 / 死后换宠。
    oss << ",\"reason\":\"" << (legal.must_choose_pet ? "choose_pet_after_death" : "choose_action")
        << "\"";
    // 权威快照直接内嵌：客户端收到这一条就能立刻渲染，不必再追问一次 SYNC_STATE。
    oss << ",\"snapshot\":" << (snapshot_json.empty() ? "null" : snapshot_json);
    oss << ",\"legal\":" << legal_actions_to_json(legal);
    oss << "}";
    return oss.str();
}

std::string room_info_to_json(int match_id, int seat, int seat_count, bool solo) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"match\":" << match_id;
    oss << ",\"seat\":" << seat;
    oss << ",\"seats\":" << seat_count;
    oss << ",\"mode\":\"" << (solo ? "solo" : "pvp") << "\"";
    oss << "}";
    return oss.str();
}

std::string battle_over_to_json(int match_id, BattleContext& ctx, const std::string& snapshot_json,
                                const std::string& reason) {
    // 胜负口径：自己场上精灵全灭则负。两边同时全灭判平（draw）。
    int alive0 = 0;
    int alive1 = 0;
    for (int i = 0; i < 6; ++i) {
        if (ctx.seerRobot[0].elfPets[i].hp > 0) ++alive0;
        if (ctx.seerRobot[1].elfPets[i].hp > 0) ++alive1;
    }
    int winner = -1;
    if (alive0 > 0 && alive1 == 0) {
        winner = 0;
    } else if (alive1 > 0 && alive0 == 0) {
        winner = 1;
    }

    std::ostringstream oss;
    oss << "{";
    oss << "\"match\":" << match_id;
    oss << ",\"winner\":" << winner;  // -1 = 平局/未分
    oss << ",\"round\":" << ctx.roundCount;
    oss << ",\"alive\":[" << alive0 << "," << alive1 << "]";
    oss << ",\"reason\":\"" << json_escape(reason) << "\"";
    oss << ",\"snapshot\":" << (snapshot_json.empty() ? "null" : snapshot_json);
    oss << "}";
    return oss.str();
}

}  // namespace server
