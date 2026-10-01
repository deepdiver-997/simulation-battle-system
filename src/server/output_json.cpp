#include <server/output_json.h>

#include <abnormal-system/abnormal-types.h>
#include <effects/state_tape.h>
#include <entities/pet_factory.h>
#include <entities/seer-robot.h>
#include <entities/skills.h>
#include <entities/soul_mark_manager.h>
#include <entities/suit_manager.h>
#include <effects/effect.h>
#include <fsm/battleContext.h>
#include <fsm/iControlBlock.h>
#include <fsm/state.h>
#include <server/protocol.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <sstream>
#include <string>

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
        case EventType::EVENT_STAT_CHANGED: return "EVENT_STAT_CHANGED";
        case EventType::EVENT_ANOMALY_EXPIRED: return "EVENT_ANOMALY_EXPIRED";
        case EventType::EVENT_ANOMALY_RESISTED: return "EVENT_ANOMALY_RESISTED";
        case EventType::EVENT_ANOMALY_IMMUNED: return "EVENT_ANOMALY_IMMUNED";
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
    oss << ",\"prospective\":" << (legal.prospective ? "true" : "false");
    oss << ",\"reason\":\"" << json_escape(legal.reason) << "\"";
    oss << ",\"mustChoosePet\":" << (legal.must_choose_pet ? "true" : "false");
    oss << ",\"canSkip\":" << (legal.can_skip ? "true" : "false");

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
            << ",\"itemId\":" << m.item_id
            << ",\"name\":\"" << json_escape(m.name) << "\""
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
    // 胜负口径（2026-10-01 D5 结算模型）：settlement_alive_count —— 终局立旗时快照
    // 存活数（其后亡语复活不改判："己方压轴击杀对手压轴重生也算自己赢"），再叠加
    // "视为存活"加账（薇尔诗黄金万象，effect_des 523）。两边同时为 0 判平（draw）。
    const int alive0 = ctx.settlement_alive_count(0);
    const int alive1 = ctx.settlement_alive_count(1);
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

// ── 注册表导出与版本指纹 ──────────────────────────────────────────────
// （内容自 2026-10-01 起是 main.cpp --dump-registry 与 HELLO 指纹的**唯一**实现：
//   两处消费同一份字符串，registry.json 和握手指纹永不漂移。）

namespace {

std::uint64_t fnv1a64(const std::string& s) {
    std::uint64_t h = 1469598103934665603ull;   // FNV offset basis
    for (const unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;                  // FNV prime
    }
    return h;
}

std::string hex64(std::uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(v));
    return std::string(buf);
}

// 快照顶层键提取（外层对象深度 1 的键名），指纹只看键集——值会随对局变，
// 键集才是 schema。键/值串区分 = 看进串前最近的显著字符（'{'或','=键，':'=值）。
std::string top_level_keys(const std::string& json) {
    std::string keys;
    int depth = 0;
    bool in_string = false, escaped = false, collecting_key = false;
    char last_sig = 0;
    std::string current;
    for (const char c : json) {
        if (in_string) {
            current += c;
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
                if (collecting_key) {
                    if (!keys.empty()) {
                        keys += ',';
                    }
                    keys += current;
                    collecting_key = false;
                }
                current.clear();
            }
            continue;
        }
        switch (c) {
            case '"':
                in_string = true;
                collecting_key = (depth == 1 && (last_sig == '{' || last_sig == ','));
                current.clear();
                current += c;
                break;
            case '{':
            case '[':
                ++depth;
                last_sig = c;
                break;
            case '}':
            case ']':
                --depth;
                last_sig = c;
                break;
            case ':':
            case ',':
                last_sig = c;
                break;
            default:
                break;   // 空白/数字/字面量不改变键判定
        }
    }
    return keys;
}

}  // namespace

RegistryDump build_registry_dump() {
    RegistryDump dump;
    std::vector<int> moves = EffectFactory::getInstance().registered_effect_ids();
    // 内核记账效果（697/699 穿透凭证族）：功能在技能加载期并入 penetration_flags，
    // 不注册 SKILL_EFFECT——dump 端并集补账，否则控制台覆盖率误报"未实现"。
    {
        const auto& kernel_ids = kernel_penetration_effect_ids();   // skills.h 尾部自由函数
        moves.insert(moves.end(), kernel_ids.begin(), kernel_ids.end());
        std::sort(moves.begin(), moves.end());
        moves.erase(std::unique(moves.begin(), moves.end()), moves.end());
    }
    const std::vector<int> soul = SoulMarkManager::getInstance().registered_soulmark_ids();
    const std::vector<int> suit = SuitManager::getInstance().registered_suit_ids();
    dump.moves = moves.size();
    dump.soul = soul.size();
    dump.suit = suit.size();

    auto join_ids = [](const std::vector<int>& ids) {
        std::string out;
        for (std::size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) {
                out += ',';
            }
            out += std::to_string(ids[i]);
        }
        return out;
    };

    // 时点：State 枚举 -1..40 全量（kLinearStateOrder 在匿名命名空间拿不到，
    // 而且它不含 FINISHED——对照表按枚举区间遍历最稳，state_name_cn 全覆盖）。
    auto escape = [](const char* text) {
        return server::json_escape(std::string(text == nullptr ? "" : text));
    };
    auto join_states = [&escape]() {
        std::string out;
        for (int id = -1; id <= 40; ++id) {
            if (id > -1) {
                out += ',';
            }
            out += "{\"id\":" + std::to_string(id) + ",\"name\":\"" +
                   escape(state_name_cn(static_cast<State>(id))) + "\"}";
        }
        return out;
    };

    // 异常状态：官方 id 0..kOfficialAbnormalStatusMaxId，附中文名。
    auto join_anomalies = [&escape]() {
        std::string out;
        for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
            if (id > 0) {
                out += ',';
            }
            out += "{\"id\":" + std::to_string(id) + ",\"name\":\"" +
                   escape(abnormal_status_name_cn(static_cast<AbnormalStatusId>(id))) + "\"}";
        }
        return out;
    };

    // RuleCenter 票类型固定显示表（开工文档 §2.1：引擎固定枚举 → 引擎侧给对照表，
    // fullstate 的 ruleTickets 只传 id，名字由客户端按这些表解析）。
    // ⚠️ 顺序必须与枚举定义一致（include/effects/rule_center.h）。
    auto join_named = [&escape](const std::vector<std::pair<int, const char*>>& items) {
        std::string out;
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i > 0) {
                out += ',';
            }
            out += "{\"id\":" + std::to_string(items[i].first) + ",\"name\":\"" +
                   escape(items[i].second) + "\"}";
        }
        return out;
    };
    const std::vector<std::pair<int, const char*>> rule_categories = {
        {0, "免疫"}, {1, "盔/威/封属"}, {2, "命中失效·攻击"}, {3, "命中失效·属性"},
        {4, "回弹"}, {5, "穿透凭证·攻击"}, {6, "穿透凭证·属性伤害"}, {7, "场下保护"},
        {8, "异常池改写"}, {9, "技能封禁"}, {10, "附加禁令"}, {11, "锁切"},
        {12, "攻击无效化"}, {13, "药剂反噬"}, {14, "盔遮蔽"}, {15, "概率闸门"},
    };
    const std::vector<std::pair<int, const char*>> immunity_types = {
        {0, "免断"}, {1, "免伤"}, {2, "免粉伤"}, {3, "免弱化"}, {4, "免异常"},
        {5, "封回血"}, {6, "免消除强化"}, {7, "免疫秒杀"}, {8, "无法能力提升"},
        {9, "能力下降锁定"},
    };
    const std::vector<std::pair<int, const char*>> seal_kinds = {
        {0, "封攻击(狮盔)"}, {1, "全封(龙威)"}, {2, "封属性"}, {3, "封属性·命中失效"},
    };
    const std::vector<std::pair<int, const char*>> chance_tags = {
        {0, "异常附加概率"},
    };
    const std::vector<std::pair<int, const char*>> chance_sources = {
        {0, "技能"}, {1, "魂印"}, {2, "特性"}, {3, "套装"},
    };
    const std::vector<std::pair<int, const char*>> effect_scopes = {
        {0, "上场"}, {1, "全队"},
    };

    dump.json = "{\"moves\":[" + join_ids(moves) + "],\"soul\":[" + join_ids(soul)
                + "],\"suit\":[" + join_ids(suit) + "],\"states\":[" + join_states()
                + "],\"anomalies\":[" + join_anomalies()
                + "],\"ruleCategories\":[" + join_named(rule_categories)
                + "],\"immunityTypes\":[" + join_named(immunity_types)
                + "],\"sealKinds\":[" + join_named(seal_kinds)
                + "],\"chanceTags\":[" + join_named(chance_tags)
                + "],\"chanceSources\":[" + join_named(chance_sources)
                + "],\"effectScopes\":[" + join_named(effect_scopes) + "]}";
    return dump;
}

std::string version_fingerprint_json() {
    // 进程级缓存：注册表/快照 schema 在启动后不变（插件加载完就定型）。
    static const std::string cached = [] {
        const RegistryDump dump = build_registry_dump();
        const std::string registry_hash = hex64(fnv1a64(dump.json));
        // 快照 schema 探针：空对局上下文直接出 getStateJson——只需要键集，
        // 不需要真实阵容/数据库内容。任何异常都降级为空串（消费方跳过该维比对）。
        std::string state_hash;
        try {
            class ProbeControlBlock final : public IControlBlock {
                void wait_for_input(BattleContext*) override {}
                void async_write(int, const std::string&, BattleContext*, BattleFsm*) override {}
                void on_fsm_paused(BattleContext*) override {}
            };
            ProbeControlBlock block;
            std::array<ElfPet, 6> empty_party{
                PetFactory::create_empty_pet(), PetFactory::create_empty_pet(),
                PetFactory::create_empty_pet(), PetFactory::create_empty_pet(),
                PetFactory::create_empty_pet(), PetFactory::create_empty_pet()};
            std::array<SeerRobot, 2> robots{SeerRobot(empty_party), SeerRobot(empty_party)};
            BattleContext ctx(&block, robots.data(), /*pvp_battle=*/false);
            state_hash = hex64(fnv1a64(top_level_keys(ctx.getStateJson())));
        } catch (...) {
            state_hash = "";
        }
        return "{\"protoVersion\":" + std::to_string(proto::kProtocolVersion)
               + ",\"registryHash\":\"" + registry_hash
               + "\",\"stateSchemaHash\":\"" + state_hash + "\"}";
    }();
    return cached;
}

}  // namespace server
