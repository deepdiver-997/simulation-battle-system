// 对战引擎 seed 循环 fuzz（本地维护，EXISTS 守卫进构建；用法见同目录 README.md）。
//
// 思路（2026-09-30 与用户对齐）：fuzzer 不发明非法输入 ——
//   ① 队伍生成：从官方 DB 抽真实精灵（学习表 ≥5 个互异技能的），走 PetFactory 组装，
//      魂印（effect_icon 链路）/技能槽位规则/通用特性全部由引擎自己的校验把关；
//   ② 对战驱动：每个决策点问 server::compute_legal_actions（与客户端合法按钮同源），
//      在「可用技能 / 可换精灵 / 可嗑药剂 / 空过(NONE)」里均匀采样。镜像模式下如果
//      原局拒绝了合法集合给出的动作，单独输出拒绝原因，不误报成镜像分歧；
//   ③ oracle 四层：
//        不变量（hp≤体力上限[引擎自己在 reduce_max_hp_* 维护的那条]、pp≥0、槽位界内）
//          → CHECK abort（SIGABRT）；
//        确定性：同 seed 双跑整场，逐决策点状态签名序列必须一致（随机流漂移或跨局状态
//          泄漏导致状态分歧 → 在此现形，exit 3）；
//        效果参数解包失效（effect_arg_fail_fast，同场景 harness 口径）→ exit 4；
//        ASan/UBSan（构建期 -DSBS_FUZZ_SANITIZERS=ON）→ 内存/UB 直接 abort。
//   ④ 挂死保护：每局 alarm(kHangSeconds)，正常对局毫秒级，超时即疑似 FSM 挂死 → _exit(43)。
//
// ⚠️ 终局路径（2026-10-01 D3/D4 终局改造后）：引擎内建两种终局——一方全灭
//    （evaluate_battle_end 直达 FINISHED）与回合上限（ctx.max_rounds，本驱动设 60）。
//    驱动不再代收全灭尾：若死切等输入时仍无宠可换 = 引擎回归，按 anomaly 处理。
//
// 零内核改动：只依赖公开接口（ScenarioBattle 同款驱动路径 + compute_legal_actions +
// PetFactory），BattleFsm(false) 静音 DEBUG 日志。

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <sqlite3.h>

#include "../scenario/scenario_harness.h"

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/local_battle_control.h>
#include <server/legal_actions.h>
#include <utils/sbs_rand.h>

namespace {

using server::LegalActions;

struct SeedPlan;
void dump_plan(std::ostream& os, const SeedPlan& plan);

constexpr int kHangSeconds = 120;   // 正常对局毫秒级；超时即视为挂死
constexpr int kDefaultMaxPoints = 400;
constexpr int kRoundLimit = 60;
constexpr int kRoundFreezePoints = 32;  // 一个回合内的决策点上界 ~14（选择2 + 死补≤12），32 仍冻结 = 打转

volatile sig_atomic_t g_seed_for_signal = -1;
unsigned long long g_current_seed = 0;   // ASan 死亡回调用：任何 abort 自动带 seed 归属
const char* g_current_party = nullptr;   // 指向本 seed 的 dump_plan 缓存（dump_plan 后置位）

// ASan 死亡回调（#7 追捕，2026-11-06）：任何 abort 自动打印当前 seed + grants 审计尾。
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
extern "C" void __asan_set_death_callback(void (*)(void));
void fuzz_asan_death_cb() {
    std::fprintf(stderr, "[ASAN-DEATH] current seed=%llu\n", g_current_seed);
    ab_audit_dump();
}
#endif
#endif

// async-signal-safe：只用 write，seed 手工转十进制。
void hang_handler(int) {
    char buf[96];
    std::size_t n = 0;
    std::memcpy(buf, "SBS_FUZZ_HANG seed=", 19);
    n = 19;
    unsigned long long s = static_cast<unsigned long long>(g_seed_for_signal);
    char tmp[24];
    int t = 0;
    do {
        tmp[t++] = static_cast<char>('0' + s % 10);
        s /= 10;
    } while (s > 0);
    while (t > 0) {
        buf[n++] = tmp[--t];
    }
    const char* suf = " alarm timeout: battle did not finish in time (FSM hang?)\n";
    for (std::size_t i = 0; suf[i] != '\0'; ++i) {
        buf[n++] = suf[i];
    }
    ssize_t rc = ::write(2, buf, n);
    (void)rc;
    _exit(43);
}

// ── 状态签名：逐决策点记一条，确定性双跑逐条比对 ─────────────────────────
std::uint64_t fnv1a(std::uint64_t h, std::uint64_t v) {
    h ^= v;
    h *= 1099511628211ULL;
    return h;
}

std::uint64_t battle_signature(const BattleContext& ctx, const SbsRng& driver_rng) {
    std::uint64_t h = 1469598103934665603ULL;
    h = fnv1a(h, ctx.rng().state());
    h = fnv1a(h, driver_rng.state());
    h = fnv1a(h, static_cast<std::uint64_t>(ctx.roundCount));
    h = fnv1a(h, static_cast<std::uint64_t>(ctx.currentState));
    h = fnv1a(h, static_cast<std::uint64_t>(ctx.current_player_id_));
    for (int side = 0; side < 2; ++side) {
        h = fnv1a(h, static_cast<std::uint64_t>(ctx.on_stage[side]));
        h = fnv1a(h, static_cast<std::uint64_t>(ctx.seerRobot[side].allive()));
        for (const auto& [item_id, count] : ctx.seerRobot[side].medicines) {
            h = fnv1a(h, static_cast<std::uint64_t>(item_id));
            h = fnv1a(h, static_cast<std::uint64_t>(count));
        }
        for (int slot = 0; slot < 6; ++slot) {
            const ElfPet& pet = ctx.seerRobot[side].elfPets[slot];
            h = fnv1a(h, static_cast<std::uint64_t>(pet.id));
            h = fnv1a(h, static_cast<std::uint64_t>(pet.hp));
            h = fnv1a(h, static_cast<std::uint64_t>(pet.numericalBase[NumericalPropertyIndex::HP]));
            for (int s = 0; s < 5; ++s) {
                h = fnv1a(h, static_cast<std::uint64_t>(pet.skills[s].id));
                h = fnv1a(h, static_cast<std::uint64_t>(pet.skills[s].pp));
            }
            for (int lv = 0; lv < 6; ++lv) {
                h = fnv1a(h, static_cast<std::uint64_t>(ctx.ability_levels[side][lv]));
            }
        }
    }
    return h;
}

// ── oracle ①：不变量。失败先打 FAIL 行再 CHECK abort（cerr 无缓冲，不丢） ────
void check_invariants(const BattleContext& ctx, unsigned long long seed) {
    for (int side = 0; side < 2; ++side) {
        const int on_stage = ctx.on_stage[side];
        if (on_stage < 0 || on_stage >= 6) {
            std::cerr << "SBS_FUZZ_FAIL seed=" << seed << " reason=invariant_on_stage_out_of_range"
                      << " side=" << side << " on_stage=" << on_stage << std::endl;
        }
        CHECK(on_stage >= 0 && on_stage < 6, "seed=" << seed << " side=" << side);
        // 空位上场 oracle（2026-11-03，#14 伴生）：on_stage 指向的必须是真精灵——
        // 空位（id<0，面板全 0）被放上场会成为"防御=0 的被打方"→ 伤害公式 inf。
        {
            const ElfPet& on = ctx.seerRobot[side].elfPets[on_stage];
            if (on.id < 0) {
                std::cerr << "SBS_FUZZ_FAIL seed=" << seed
                          << " reason=invariant_empty_pet_on_stage side=" << side
                          << " slot=" << on_stage << " pet_id=" << on.id
                          << " hp=" << on.hp << std::endl;
            }
            CHECK(on.id >= 0, "seed=" << seed << " side=" << side
                              << " slot=" << on_stage << " pet_id=" << on.id);
        }
        for (int slot = 0; slot < 6; ++slot) {
            const ElfPet& pet = ctx.seerRobot[side].elfPets[slot];
            // 能力等级域 oracle（2026-11-03，#9 家族）：ability/view 等级必须 ∈ [-6,6]——
            // 抓一切越界直写（读端已钳，此断言负责给写入者点名 seed）。
            for (int lv_i = 0; lv_i < 6; ++lv_i) {
                const int al = ctx.ability_levels[side][lv_i];
                const int vl = ctx.ws.view_levels[side][lv_i];
                if (al < -6 || al > 6 || vl < -6 || vl > 6) {
                    std::cerr << "SBS_FUZZ_FAIL seed=" << seed
                              << " reason=invariant_level_out_of_range side=" << side
                              << " level_slot=" << lv_i << " ability=" << al
                              << " view=" << vl << std::endl;
                }
                CHECK(al >= -6 && al <= 6 && vl >= -6 && vl <= 6,
                      "seed=" << seed << " side=" << side << " slot=" << lv_i);
            }
            // 死亡裁定权威 oracle（2026-10-02）：已登记死亡的槽位 hp 必须 ≤0——
            // 抓一切绕过 revive_pet 的野路子复活（直写 hp / 互换等）。
            if (ctx.is_death_registered(side, slot) && pet.hp > 0) {
                std::cerr << "SBS_FUZZ_FAIL seed=" << seed
                          << " reason=invariant_dead_registered_but_hp_positive side=" << side
                          << " slot=" << slot << " pet_id=" << pet.id << " hp=" << pet.hp
                          << " max_hp=" << pet.numericalBase[NumericalPropertyIndex::HP]
                          << std::endl;
            }
            CHECK(!ctx.is_death_registered(side, slot) || pet.hp <= 0,
                  "seed=" << seed << " side=" << side << " slot=" << slot
                          << " pet_id=" << pet.id << " hp=" << pet.hp);
            const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
            if (max_hp < 0) {
                std::cerr << "SBS_FUZZ_FAIL seed=" << seed
                          << " reason=invariant_negative_max_hp side=" << side << " slot=" << slot
                          << " pet_id=" << pet.id << " max_hp=" << max_hp << std::endl;
            }
            CHECK(max_hp >= 0, "seed=" << seed << " side=" << side << " slot=" << slot);
            if (pet.hp > max_hp) {
                std::cerr << "SBS_FUZZ_FAIL seed=" << seed
                          << " reason=invariant_hp_above_max side=" << side << " slot=" << slot
                          << " pet_id=" << pet.id << " hp=" << pet.hp << " max_hp=" << max_hp
                          << std::endl;
            }
            CHECK(pet.hp <= max_hp,
                  "seed=" << seed << " side=" << side << " slot=" << slot << " pet_id=" << pet.id);
            for (int s = 0; s < 5; ++s) {
                if (pet.skills[s].pp < 0) {
                    std::cerr << "SBS_FUZZ_FAIL seed=" << seed
                              << " reason=invariant_negative_pp side=" << side << " slot=" << slot
                              << " skill_index=" << s << " skill_id=" << pet.skills[s].id
                              << " pp=" << pet.skills[s].pp << std::endl;
                }
                CHECK(pet.skills[s].pp >= 0, "seed=" << seed << " side=" << side);
            }
        }
    }
}

// ── 队伍/药剂生成（独立的 plan_rng，保留同 seed 的既有阵容）────────────────
struct Pool {
    std::vector<int> pets;    // 学习表里 ≥5 个互异技能且 hp>0 的官方精灵 id
    std::vector<int> traits;  // new_se stat=1 的通用特性 idx（50 种 × 等级 0-5）
};

Pool load_pool() {
    Pool pool;
    sqlite3* db = nullptr;
    if (sqlite3_open_v2("scripts/data/processed/seer_unity.sqlite", &db, SQLITE_OPEN_READONLY,
                        nullptr) != SQLITE_OK) {
        return pool;
    }
    auto run_query = [&](const char* sql, std::vector<int>& out) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return;
        }
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            out.push_back(sqlite3_column_int(stmt, 0));
        }
        sqlite3_finalize(stmt);
    };
    // ≥5 个互异学习技：1-4 槽必填学习技、第 5 槽兜底也要学习技（隐藏技可能被神谕锁吞掉）。
    run_query(
        "SELECT m.id FROM monsters m JOIN monster_learnable_moves lm ON lm.monster_id = m.id "
        "WHERE m.hp > 0 AND lm.move_id > 0 "
        "GROUP BY m.id HAVING COUNT(DISTINCT lm.move_id) >= 5 ORDER BY m.id",
        pool.pets);
    run_query("SELECT idx FROM new_se WHERE stat = 1 ORDER BY idx", pool.traits);
    sqlite3_close(db);
    return pool;
}

using BattlePetMessages = std::array<BattlePetMessage, 6>;

// 一方的随机合法队伍：size ∈ [1,6] 只（空槽 pet_id=-1，走 create_empty_pet），
// 同侧精灵 id 不重复（擂台规则口径；魂印激活引擎里也按同侧同 id 去重）。
BattlePetMessages gen_side(const Pool& pool, SbsRng& plan_rng) {
    BattlePetMessages msgs;
    const int size = 1 + plan_rng.next() % 6;
    std::vector<int> bag = pool.pets;
    std::vector<int> picked;
    picked.reserve(static_cast<std::size_t>(size));
    for (int i = 0; i < size; ++i) {
        const int j = i + plan_rng.next() % static_cast<int>(bag.size() - i);
        std::swap(bag[i], bag[j]);
        picked.push_back(bag[i]);
    }

    auto& repo = official_data::OfficialDataStore::instance().repository();
    for (int slot = 0; slot < size; ++slot) {
        BattlePetMessage msg;
        msg.pet_id = picked[static_cast<std::size_t>(slot)];
        const std::optional<official_data::MonsterRecord> monster = repo.load_monster(msg.pet_id);
        if (!monster) {
            std::cerr << "SBS_FUZZ_GEN_ERROR load_monster failed pet_id=" << msg.pet_id
                      << std::endl;
            _exit(6);
        }

        // 技能池：学习表 ∪ 隐藏技，剔除神谕新技（oracle_on=false 时引擎会把它们锁死）。
        // 1-4 槽必须学习技；第 5 槽允许隐藏技（25% 概率用，踩第五技路径）。
        // create_skills_for_pet 会校验 5 个 id 互异 —— 池子 SQL 已保证学习技 ≥5 互异。
        std::vector<int> learnable;
        std::vector<int> hidden;
        for (const auto& mv : monster->learnable_moves) {
            if (mv.move_id > 0 && !monster->oracle_moves.contains(mv.move_id)) {
                learnable.push_back(mv.move_id);
            }
        }
        for (const int mv : monster->hidden_moves) {
            if (mv > 0 && !monster->oracle_moves.contains(mv)) {
                hidden.push_back(mv);
            }
        }
        if (learnable.size() < 5) {
            // 池子 SQL 已保证 ≥5；到这里说明滤神谕后跌破（理论不达，防御性 fail-fast）
            std::cerr << "SBS_FUZZ_GEN_ERROR learnable<5 pet_id=" << msg.pet_id << std::endl;
            _exit(6);
        }
        std::vector<int> chosen;
        chosen.reserve(5);
        for (int k = 0; k < 5; ++k) {
            const int j = k + plan_rng.next() % static_cast<int>(learnable.size() - k);
            std::swap(learnable[static_cast<std::size_t>(k)], learnable[static_cast<std::size_t>(j)]);
            chosen.push_back(learnable[static_cast<std::size_t>(k)]);
        }
        // 第 5 槽 25% 概率换隐藏技（踩第五技路径；与 chosen 互异才换，create_pet 会校验）
        if (!hidden.empty() && plan_rng.next() % 4 == 0) {
            const int hid = hidden[static_cast<std::size_t>(plan_rng.next() % hidden.size())];
            if (std::find(chosen.begin(), chosen.end(), hid) == chosen.end()) {
                chosen[4] = hid;
            }
        }
        for (int k = 0; k < 5; ++k) {
            msg.chosen_skills_id[static_cast<std::size_t>(k)] = chosen[static_cast<std::size_t>(k)];
        }

        // 通用特性：70% 概率随机一条（含等级 0-5）；0 = 无
        msg.common_trait_id =
            (plan_rng.next() % 10 < 7 && !pool.traits.empty())
                ? pool.traits[static_cast<std::size_t>(plan_rng.next() % pool.traits.size())]
                : 0;

        // 面板（旧协议直传语义，全合法）：攻/特攻按体力调参保证对局有击杀节奏；
        // 防/特防/速/体传 0 走种族值兜底。老板宠 hp=0 的已被池子 SQL 滤掉。
        const int hp = std::max(monster->hp, 1);
        const int atk = std::clamp(hp / 2 + 30, 40, 5000);
        msg.numerical_base = {atk, atk, 0, 0, 0, 0};
        msgs[static_cast<std::size_t>(slot)] = msg;
    }
    for (int slot = size; slot < 6; ++slot) {
        msgs[static_cast<std::size_t>(slot)].pet_id = -1;  // 空位
    }
    return msgs;
}

MedicineStock gen_medicines(SbsRng& plan_rng) {
    static const int kItems[] = {300011, 300012, 300013, 300014, 300016, 300017, 300018, 300019};
    MedicineStock stock;
    const int kinds = plan_rng.next() % 3;  // 0~2 种
    for (int k = 0; k < kinds; ++k) {
        const int item_id = kItems[plan_rng.next() % 8];
        stock[item_id] = 1 + plan_rng.next() % 3;  // 1~3 瓶（map 语义：同 id 取最后一次）
    }
    return stock;
}

struct SeedPlan {
    BattlePetMessages sides[2];
    MedicineStock medicines[2];
};

SeedPlan gen_plan(const Pool& pool, unsigned long long seed) {
    SbsRng plan_rng(static_cast<unsigned int>(seed));
    SeedPlan plan;
    plan.sides[0] = gen_side(pool, plan_rng);
    plan.sides[1] = gen_side(pool, plan_rng);
    plan.medicines[0] = gen_medicines(plan_rng);
    plan.medicines[1] = gen_medicines(plan_rng);
    return plan;
}

SeedPlan mirror_fixture_plan() {
    SeedPlan plan;
    for (auto& side : plan.sides) {
        for (auto& pet : side) pet.pet_id = -1;
    }
    // Naturally learned moves; 20010 is the audited, native effect-4 cast.
    plan.sides[0][0] = {.pet_id = 6, .chosen_skills_id = {20010, 10004, 10018, 10024, 10027}};
    plan.sides[1][0] = {.pet_id = 12, .chosen_skills_id = {10037, 10001, 10008, 10009, 10033}};
    plan.sides[1][1] = {.pet_id = 120, .chosen_skills_id = {10001, 10088, 10297, 10354, 10355}};
    plan.medicines[0] = {{300011, 1}, {300012, 1}};
    plan.medicines[1] = {{300011, 1}};
    return plan;
}

std::array<SeerRobot, 2> build_robots(const SeedPlan& plan) {
    return {SeerRobotFactory::create_robot(plan.sides[0], plan.medicines[0]),
            SeerRobotFactory::create_robot(plan.sides[1], plan.medicines[1])};
}

// ── 对战驱动：ScenarioBattle 的静音版（enable_debug=false 灭 DEBUG 刷屏）─────
class FuzzBattle {
public:
    FuzzBattle(const SeedPlan& plan, unsigned long long seed)
        : robots_(build_robots(plan))
        , ctx_(&cb_, robots_.data(), /*pvp_battle=*/false, static_cast<unsigned int>(seed))
        , fsm_(/*enable_debug=*/false) {
        // 效果参数解包错误 fail-fast（同场景 harness 口径）：解包失效 = 假绿，宁可真红。
        ctx_.effect_arg_fail_fast = true;
        // D3/D4（2026-10-01 终局改造）：回合上限内建——对局必然可终结，长局到顶走
        // FINISHED（原先靠决策点上界兜成 stalled）；on_finish 计数观察终局路径占比。
        ctx_.max_rounds = kRoundLimit;
        ctx_.on_finish = [this](BattleContext&) { ++finish_callbacks_; };
    }

    int finish_callbacks() const { return finish_callbacks_; }

    void start() { fsm_.run(&ctx_); }
    void at_decision(int) noexcept {}
    void drive() { fsm_.run(&ctx_); }
    void submit_operation(int player, BattleFsm::ActionType action, int index) {
        const int buf[4] = {player, static_cast<int>(action), index, 0};
        ctx_.m_buffer.assign(reinterpret_cast<const char*>(buf),
                             reinterpret_cast<const char*>(buf) + 4 * sizeof(int));
        ctx_.is_empty = false;
        fsm_.run(&ctx_);
    }
    BattleContext& ctx() { return ctx_; }

private:
    scenario::StubControlBlock cb_;
    std::array<SeerRobot, 2> robots_;
    BattleContext ctx_;
    BattleFsm fsm_;
    int finish_callbacks_ = 0;
};

struct MirrorCoverage {
    std::uint64_t attempted = 0;
    std::uint64_t attached = 0;
    std::uint64_t unsupported = 0;
    std::uint64_t compared = 0;
    std::uint64_t completed = 0;
};

class MirrorMismatch : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class MirrorFuzzBattle {
public:
    MirrorFuzzBattle(const SeedPlan& plan, unsigned long long seed, MirrorCoverage& coverage, bool verbose)
        : robots_(build_robots(plan)), seed_(seed), attach_point_(1 + static_cast<int>(seed % 4)),
          coverage_(coverage), verbose_(verbose) {}

    void start() {
        const auto opened = control_.start(robots_, static_cast<unsigned int>(seed_), kRoundLimit,
                                           /*effect_arg_fail_fast=*/true);
        if (!opened) throw std::runtime_error("local start: " + opened.error);
        id_ = opened.id;
    }

    void at_decision(int point) {
        point_ = point;
        if (point != attach_point_ || !ctx().need_input()) return;
        ++coverage_.attempted;
        const auto opened = control_.mirror(id_);
        if (!opened) {
            ++coverage_.unsupported;
            if (verbose_ || coverage_.unsupported == 1) {
                std::cerr << "SBS_FUZZ_MIRROR_SKIP seed=" << seed_ << " point=" << point
                          << " round=" << ctx().roundCount << " reason=" << opened.error << "\n";
            }
            return;
        }
        ++coverage_.attached;
        mirrored_ = true;
        if (verbose_) {
            std::cerr << "[fuzz] mirror attached seed=" << seed_ << " point=" << point
                      << " round=" << ctx().roundCount << "\n";
        }
    }

    void drive() { throw std::runtime_error("local FSM paused outside an actionable decision"); }

    void submit_operation(int player, BattleFsm::ActionType type, int index) {
        using Action = BattleFsm::ActionType;
        BattleCloneAction action;
        switch (type) {
            case Action::CHOOSE_PET: action = BattleCloneAction::ChoosePet; break;
            case Action::SELECT_SKILL: action = BattleCloneAction::SelectSkill; break;
            case Action::USE_MEDICINE: action = BattleCloneAction::UseMedicine; break;
            case Action::NONE: action = BattleCloneAction::Skip; break;
            default: throw std::logic_error("fuzzer selected an unsupported action");
        }
        const int round = ctx().roundCount;
        const auto result = control_.submit(id_, player, action, index);
        if (result.difference || !result.primary.ok) {
            const auto where = "point=" + std::to_string(point_) + " round=" + std::to_string(round) +
                               " player=" + std::to_string(player) +
                               " action=" + std::to_string(static_cast<int>(type)) +
                               " index=" + std::to_string(index);
            if (const auto& diff = result.difference) {
                throw MirrorMismatch(where + " input=" + std::to_string(diff->input) +
                                     " detail=" + diff->detail);
            }
            throw std::runtime_error("local source rejected generated input: " + where +
                                     " error=" + result.primary.error);
        }
        if (mirrored_) {
            ++coverage_.compared;
            if (ctx().finish_flag) ++coverage_.completed;
        }
    }

    const BattleContext& ctx() const { return *control_.context(id_); }
    // Neither branch installs the opaque callback that the raw driver counts.
    int finish_callbacks() const noexcept { return 0; }

private:
    std::array<SeerRobot, 2> robots_;
    unsigned long long seed_;
    int attach_point_;
    MirrorCoverage& coverage_;
    bool verbose_;
    LocalBattleControl control_;
    LocalBattleControl::Id id_ = 0;
    int point_ = 0;
    bool mirrored_ = false;
};

struct Option {
    BattleFsm::ActionType type;
    int index;
};

// 决策点采样：合法集合均匀分布（NONE 恒在 —— 引擎无条件接受，客户端超时语义同款）。
Option pick_action(const LegalActions& legal, SbsRng& driver_rng) {
    std::vector<Option> opts;
    opts.push_back({BattleFsm::ActionType::NONE, -1});
    for (const auto& s : legal.skills) {
        if (s.usable) {
            opts.push_back({BattleFsm::ActionType::SELECT_SKILL, s.index});
        }
    }
    for (const auto& p : legal.pets) {
        if (p.usable) {
            opts.push_back({BattleFsm::ActionType::CHOOSE_PET, p.slot});
        }
    }
    for (const auto& m : legal.medicines) {
        if (m.usable) {
            opts.push_back({BattleFsm::ActionType::USE_MEDICINE, m.index});
        }
    }
    return opts[static_cast<std::size_t>(driver_rng.next()) % opts.size()];
}

struct Outcome {
    bool finished = false;       // FSM 走到 FINISHED（正常终局：全灭 / 回合上限）
    bool stalled = false;        // 决策点上限 / 回合冻结
    bool anomaly = false;        // 未预期的等待状态（算真发现）
    std::string anomaly_reason;
    bool arg_faulted = false;    // 效果参数解包失效（fail-fast 终局）
    int finish_callbacks = 0;    // ctx.on_finish 触发次数（终局旗路径观察）
    int rounds = 0;
    int points = 0;              // 决策点数
    int none_picks = 0;
    int after_death_switches = 0;
};

template<class Battle, class... Args>
Outcome run_battle(const SeedPlan& plan, int max_points, std::vector<std::uint64_t>* sigs,
                   unsigned long long seed, Args&&... args) {
    // 驱动与对局各自从原 seed 起跑，保持已有 --seed 的复现口径。
    SbsRng driver_rng(static_cast<unsigned int>(seed));
    // 组装抛错 = 生成器产出了引擎判定非法的输入（fuzzer 自身 bug），单列退出码
    std::unique_ptr<Battle> battle;
    try {
        battle = std::make_unique<Battle>(plan, seed, std::forward<Args>(args)...);
    } catch (const std::exception& e) {
        std::cerr << "SBS_FUZZ_GEN_ERROR seed=" << seed << " what=" << e.what() << "\n";
        dump_plan(std::cerr, plan);
        _exit(6);
    }
    Outcome out;
    battle->start();

    int points = 0;
    int last_round = -1;
    int points_since_round_advance = 0;
    int no_submit_guard = 0;
    while (true) {
        const BattleContext& ctx = battle->ctx();
        if (ctx.currentState == State::FINISHED) {
            out.finished = true;
            break;
        }
        if (!ctx.is_empty) {
            // run() 只在 need_input / FINISHED 停，此处不应到达；drive 兜底防打转。
            battle->drive();
            if (++points > max_points) {
                out.stalled = true;
                break;
            }
            continue;
        }

        // 决策点：先 oracle 再行动
        check_invariants(ctx, seed);
        if (sigs != nullptr) {
            sigs->push_back(battle_signature(ctx, driver_rng));
        }
        ++points;
        out.points = points;
        if (ctx.roundCount != last_round) {
            last_round = ctx.roundCount;
            points_since_round_advance = 0;
        } else if (++points_since_round_advance > kRoundFreezePoints) {
            out.stalled = true;
            break;
        }
        if (points > max_points) {
            out.stalled = true;
            break;
        }

        battle->at_decision(points);
        if (ctx.currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT) {
            bool submitted = false;
            for (int actor = 0; actor <= 1; ++actor) {
                if (ctx.operation_collected[actor]) {
                    continue;
                }
                if (ctx.currentState != State::OPERATION_CHOOSE_SKILL_MEDICAMENT) {
                    break;  // 上一手提交已让 FSM 走进回合
                }
                const LegalActions legal = server::compute_legal_actions(ctx, actor);
                const Option opt = pick_action(legal, driver_rng);
                if (opt.type == BattleFsm::ActionType::NONE) {
                    ++out.none_picks;
                }
                battle->submit_operation(actor, opt.type, opt.index);
                submitted = true;
                break;  // 每轮循环只发一手；双操作齐后 FSM 自动跑完整回合
            }
            if (!submitted) {
                // 双方都 collected 却仍在等输入 → 引擎状态矛盾，放它跑两拍看会不会自愈
                if (++no_submit_guard > 4) {
                    out.anomaly = true;
                    out.anomaly_reason = "choose_state_but_all_collected";
                    break;
                }
                battle->drive();
            }
        } else if (ctx.currentState == State::CHOOSE_AFTER_DEATH) {
            const int actor =
                server::player_must_switch_pet(ctx, 0) ? 0
                : server::player_must_switch_pet(ctx, 1) ? 1 : -1;
            if (actor < 0) {
                // need_input=true（场上有人倒下）却没有"必须换宠"的一方 → 状态矛盾
                out.anomaly = true;
                out.anomaly_reason = "choose_after_death_but_nobody_must_switch";
                break;
            }
            const LegalActions legal = server::compute_legal_actions(ctx, actor);
            std::vector<int> slots;
            for (const auto& p : legal.pets) {
                if (p.usable) {
                    slots.push_back(p.slot);
                }
            }
            if (slots.empty()) {
                // D4 后不再可达（2026-10-01）：全灭在 AFTER_DEFEATED 的终局评估就直达
                // FINISHED，不会进死切等输入。到达 = 引擎回归 → 真发现。
                out.anomaly = true;
                out.anomaly_reason = "wipeout_reached_death_switch";
                break;
            }
            battle->submit_operation(actor, BattleFsm::ActionType::CHOOSE_PET,
                                     slots[static_cast<std::size_t>(driver_rng.next()) % slots.size()]);
            ++out.after_death_switches;
        } else {
            // 未预期的等待状态：先放一拍，打转由 points/冻结守卫兜住
            battle->drive();
        }
    }

    const BattleContext& ctx = battle->ctx();
    out.rounds = ctx.roundCount;
    out.arg_faulted = ctx.effect_arg_faulted;
    out.finish_callbacks = battle->finish_callbacks();
    if (sigs != nullptr) {
        sigs->push_back(battle_signature(ctx, driver_rng));  // 终局签名也纳入比对
    }
    return out;
}

void dump_plan(std::ostream& os, const SeedPlan& plan) {
    for (int side = 0; side < 2; ++side) {
        os << "  side" << side << ":";
        for (const BattlePetMessage& msg : plan.sides[side]) {
            if (msg.pet_id < 0) {
                continue;
            }
            os << " [pet " << msg.pet_id << " skills";
            for (const int sk : msg.chosen_skills_id) {
                os << " " << sk;
            }
            os << " trait " << msg.common_trait_id << "]";
        }
        os << " meds:";
        for (const auto& [item_id, count] : plan.medicines[side]) {
            os << " " << item_id << "x" << count;
        }
        os << "\n";
    }
}

struct Options {
    unsigned long long from = 0;
    unsigned long long count = 1000;
    int max_points = kDefaultMaxPoints;
    bool determinism = true;
    bool mirror = false;
    bool mirror_fixture = false;
    bool allow_arg_fault = false;
    bool verbose = false;
    bool engine_logs = false;  // 默认把 stdout 重定向 /dev/null：引擎的 [INFO] 日志无门控
                               // （log_battle_line 直写 std::cout 且 endl 强刷），刷屏拖慢 fuzz
    unsigned long long show_party = 0;
    bool show_party_set = false;
};

Options parse_args(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* what) -> unsigned long long {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << what << "\n";
                _exit(9);
            }
            return std::strtoull(argv[++i], nullptr, 0);
        };
        if (a == "--from") opt.from = next("--from");
        else if (a == "--count") opt.count = next("--count");
        else if (a == "--seed") { opt.from = next("--seed"); opt.count = 1; }
        else if (a == "--max-points") opt.max_points = static_cast<int>(next("--max-points"));
        else if (a == "--no-determinism") opt.determinism = false;
        else if (a == "--mirror") opt.mirror = true;
        else if (a == "--mirror-fixture") { opt.mirror = true; opt.mirror_fixture = true; }
        else if (a == "--allow-arg-fault") opt.allow_arg_fault = true;
        else if (a == "--verbose") opt.verbose = true;
        else if (a == "--engine-logs") opt.engine_logs = true;
        else if (a == "--show-party") { opt.show_party = next("--show-party"); opt.show_party_set = true; }
        else {
            std::cerr << "unknown arg: " << a << "\n"
                      << "usage: sim_fuzz_battle [--from N] [--count N] [--seed S] [--max-points P]\n"
                         "                       [--no-determinism] [--allow-arg-fault] [--verbose]\n"
                         "                       [--mirror | --mirror-fixture]\n"
                         "                       [--engine-logs] [--show-party S]\n";
            _exit(9);
        }
    }
    return opt;
}

// 从 cwd / 可执行文件目录向上找仓根（判据：DB 文件），chdir 过去 ——
// DB（scripts/...）与插件（resources/*_lib）都是相对路径加载。
bool locate_and_chdir_repo(const char* argv0) {
    namespace fs = std::filesystem;
    std::vector<fs::path> bases;
    std::error_code ec;
    bases.push_back(fs::current_path(ec));
    if (argv0 != nullptr) {
        bases.push_back(fs::absolute(argv0, ec).parent_path());
    }
    for (const fs::path& base : bases) {
        for (fs::path cur = base; !cur.empty(); cur = cur.parent_path()) {
            if (fs::exists(cur / "scripts/data/processed/seer_unity.sqlite", ec)) {
                fs::current_path(cur, ec);
                return true;
            }
            if (cur == cur.root_path()) {
                break;
            }
        }
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    const Options opt = parse_args(argc, argv);
    if (!opt.engine_logs && !opt.show_party_set) {
        // 引擎的 [INFO] 战斗日志无门控直写 stdout（log_battle_line，endl 强刷）——
        // fuzz 默认 /dev/null 静音；fuzz 自身输出全走 stderr（见 README）。
        if (std::freopen("/dev/null", "w", stdout) == nullptr) {
            std::cerr << "SBS_FUZZ_ERROR freopen /dev/null failed" << std::endl;
            return 9;
        }
    }
    ::signal(SIGALRM, hang_handler);
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
    __asan_set_death_callback(&fuzz_asan_death_cb);
#endif
#endif
    ::unsetenv("BATTLE_FSM_VERBOSE");  // 灭 verbose trace（构造函数读环境变量）

    if (!locate_and_chdir_repo(argc > 0 ? argv[0] : nullptr)) {
        std::cerr << "SBS_FUZZ_ERROR repo root not found (need scripts/data/processed/*.sqlite)"
                  << std::endl;
        return 9;
    }
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "SBS_FUZZ_ERROR initialize_runtime_data failed" << std::endl;
        return 9;
    }
    const Pool pool = opt.mirror_fixture ? Pool{} : load_pool();
    if (opt.mirror_fixture) {
        std::cerr << "[fuzz] mirror factory fixture\n";
    } else {
        if (pool.pets.empty() || pool.traits.empty()) {
            std::cerr << "SBS_FUZZ_ERROR empty pool (pets=" << pool.pets.size()
                      << " traits=" << pool.traits.size() << ")" << std::endl;
            return 9;
        }
        std::cerr << "[fuzz] pool: pets=" << pool.pets.size() << " traits=" << pool.traits.size() << "\n";
    }

    if (opt.show_party_set) {
        const SeedPlan plan = opt.mirror_fixture ? mirror_fixture_plan() : gen_plan(pool, opt.show_party);
        std::cout << "party for seed " << opt.show_party << ":\n";
        dump_plan(std::cout, plan);
        return 0;
    }

    struct Stats {
        unsigned long long seeds = 0;
        unsigned long long finished = 0;
        unsigned long long finish_callbacks = 0;
        unsigned long long stalled = 0;
        unsigned long long determinism_checked = 0;
        unsigned long long none_picks = 0;
        unsigned long long after_death_switches = 0;
        unsigned long long any_cast_fault = 0;
        int max_rounds = 0;
        int max_points = 0;
    } stats;
    std::vector<unsigned long long> any_cast_fault_seeds;
    MirrorCoverage mirror_coverage;

    for (unsigned long long seed = opt.from; seed < opt.from + opt.count; ++seed) {
        g_seed_for_signal = static_cast<sig_atomic_t>(seed);
        g_current_seed = seed;
        const SeedPlan plan = opt.mirror_fixture ? mirror_fixture_plan() : gen_plan(pool, seed);

        std::vector<std::uint64_t> sigs_a;
        auto* signatures = opt.determinism ? &sigs_a : nullptr;
        ::alarm(kHangSeconds);
        Outcome a;
        try {
            a = opt.mirror
                ? run_battle<MirrorFuzzBattle>(plan, opt.max_points, signatures, seed,
                                               mirror_coverage, opt.verbose)
                : run_battle<FuzzBattle>(plan, opt.max_points, signatures, seed);
        } catch (const MirrorMismatch& e) {
            ::alarm(0);
            std::cerr << "SBS_FUZZ_FAIL seed=" << seed << " reason=mirror_mismatch " << e.what() << "\n";
            dump_plan(std::cerr, plan);
            return 3;
        } catch (const std::bad_any_cast&) {
            // 已知插件层系统性缺陷（约 42 文件用抛式 any_cast；其中"连续键+owner"键位
            // 碰撞会把守卫布尔/别家窗口写进读键位 → any_cast<int>(bool) 即炸，
            // 见 test/fuzz/README.md）。带 seed 可复现；对局状态已被异常打断、不可复用，
            // 计数后跳过本 seed，战役继续扫其他类别。
            ++stats.any_cast_fault;
            any_cast_fault_seeds.push_back(seed);
            ::alarm(0);
            continue;
        } catch (const std::exception& e) {
            // 非预期异常逃逸到驱动层 = 真发现（引擎/效果契约被破坏）
            ::alarm(0);
            std::cerr << "SBS_FUZZ_FAIL seed=" << seed << " reason=uncaught_exception what="
                      << e.what() << "\n";
            dump_plan(std::cerr, plan);
            return 5;
        }
        ::alarm(0);

        std::vector<std::uint64_t> sigs_b;
        if (opt.determinism) {
            ::alarm(kHangSeconds);
            Outcome b;
            try {
                b = run_battle<FuzzBattle>(plan, opt.max_points, &sigs_b, seed);
            } catch (const std::bad_any_cast&) {
                // A 跑通 B 却炸：同输入不同命运，本身就是非确定性发现
                ::alarm(0);
                std::cerr << "SBS_FUZZ_FAIL seed=" << seed
                          << " reason=determinism_bad_any_cast (A 跑通、B 抛 bad_any_cast)\n";
                dump_plan(std::cerr, plan);
                return 3;
            } catch (const std::exception& e) {
                ::alarm(0);
                std::cerr << "SBS_FUZZ_FAIL seed=" << seed
                          << " reason=uncaught_exception_in_b what=" << e.what() << "\n";
                dump_plan(std::cerr, plan);
                return 5;
            }
            ::alarm(0);
            ++stats.determinism_checked;
            if (sigs_a != sigs_b) {
                std::size_t diverge = 0;
                while (diverge < sigs_a.size() && diverge < sigs_b.size()
                       && sigs_a[diverge] == sigs_b[diverge]) {
                    ++diverge;
                }
                std::cerr << "SBS_FUZZ_FAIL seed=" << seed
                          << " reason=determinism_mismatch sig_index=" << diverge << "/"
                          << sigs_a.size() << "\n";
                dump_plan(std::cerr, plan);
                return 3;
            }
        }

        ++stats.seeds;
        stats.finished += a.finished ? 1 : 0;
        stats.finish_callbacks += static_cast<unsigned long long>(a.finish_callbacks);
        stats.stalled += a.stalled ? 1 : 0;
        stats.none_picks += a.none_picks;
        stats.after_death_switches += a.after_death_switches;
        stats.max_rounds = std::max(stats.max_rounds, a.rounds);
        stats.max_points = std::max(stats.max_points, a.points);

        if (a.anomaly) {
            std::cerr << "SBS_FUZZ_FAIL seed=" << seed << " reason=" << a.anomaly_reason << "\n";
            dump_plan(std::cerr, plan);
            return 5;
        }
        if (a.arg_faulted && !opt.allow_arg_fault) {
            std::cerr << "SBS_FUZZ_FAIL seed=" << seed
                      << " reason=effect_arg_faulted (效果参数解包失效，fail-fast 终局；"
                         "--allow-arg-fault 可降级为计数)\n";
            dump_plan(std::cerr, plan);
            return 4;
        }
        if (a.stalled) {
            std::cerr << "SBS_FUZZ_STALL seed=" << seed << " points=" << a.points
                      << " rounds=" << a.rounds << " (决策点上限/回合冻结；不算失败但值得看)\n";
        }
        if (opt.verbose) {
            std::cerr << "[fuzz] seed=" << seed << (a.finished ? " finished" : "")
                      << (a.stalled ? " stalled" : "")
                      << " rounds=" << a.rounds << " points=" << a.points
                      << " finish_cb=" << a.finish_callbacks << "\n";
        } else if (stats.seeds % 200 == 0) {
            std::cerr << "[fuzz] " << stats.seeds << "/" << opt.count
                      << " | finished=" << stats.finished
                      << " finish_callbacks=" << stats.finish_callbacks
                      << " stalled=" << stats.stalled << " max_rounds=" << stats.max_rounds
                      << "\n";
        }
    }

    // 摘要走 stderr：stdout 可能已被 /dev/null 静音（--engine-logs 关闭时）
    std::cerr << "SBS_FUZZ_SUMMARY"
              << " seeds=" << stats.seeds << " finished=" << stats.finished
              << " finish_callbacks=" << stats.finish_callbacks
              << " stalled=" << stats.stalled
              << " determinism_checked=" << stats.determinism_checked
              << " none_picks=" << stats.none_picks
              << " after_death_switches=" << stats.after_death_switches
              << " any_cast_fault=" << stats.any_cast_fault
              << " max_rounds=" << stats.max_rounds << " max_points=" << stats.max_points
              << std::endl;
    if (!any_cast_fault_seeds.empty()) {
        std::cerr << "SBS_FUZZ_ANY_CAST_FAULT_SEEDS";
        for (const unsigned long long s : any_cast_fault_seeds) {
            std::cerr << " " << s;
        }
        std::cerr << std::endl;
    }
    if (opt.mirror) {
        std::cerr << "SBS_FUZZ_MIRROR_SUMMARY profile=" << (opt.mirror_fixture ? "fixture" : "random")
                  << " attempted=" << mirror_coverage.attempted
                  << " attached=" << mirror_coverage.attached
                  << " unsupported=" << mirror_coverage.unsupported
                  << " compared=" << mirror_coverage.compared
                  << " completed=" << mirror_coverage.completed << '\n';
        if (mirror_coverage.compared == 0) {
            std::cerr << "SBS_FUZZ_MIRROR_NO_COVERAGE (no inputs compared)\n";
            return 8;
        }
    }
    return 0;
}
