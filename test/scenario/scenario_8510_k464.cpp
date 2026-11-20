// 场景 8510 —— K464 两族（2166 击败恢复增益 / 1669 阵容存活数翻倍门，
// moves_lib/k464_defeat_restore_allboost.cpp + k464_allboost_lineup_amp.cpp）
//（2026-11-13，worktree ~/.agent-workspaces/workspaces/sbs-local/k464；号段 8510-8519）
//
// 断言口径：
//   - 2166 = 官方击败漏斗时点（EVENT_OPPONENT_DEFEATED，K453 2456 直系）+ 窗=1
//     （武装动作当回合活跃；次回合 deliver 惰性跳过、次回合末 cleanup 移除——
//     cleanup 在 ROUND_REDUCTION 以当前 roundCount 跑、advanceRound 在
//     ROUND_COMPLETION 才 ++，故武装回合末 watch_count 仍为 1、再一回合后归 0）；
//     回血 = heal_max_hp_fraction（1/{0} 缺口喂，吃 HEAL_BLOCK）；全属性 = 六槽
//     逐项 stat_change 真写；存活门 = hp<=0 早退（K453 T8413 亡语同波同款）。
//   - 1669 = SKILL_EFFECT 立即结算；存活数 = 己方 6 槽 hp>0 计数（含自身）；
//     线性翻倍 total = {0} + ({0}×N×{1})/100（整数除法 floor）；六槽逐项真写。
//   - 白盒审计槽：plugin_storage 键 1021660000/1016690000 子块（与 k464_*.cpp
//     常量手工同步）；概率类无——全部无随机精确断言。
//   - 击杀驱动 = 970/8400 同款（foe NONE 不还手 + 事前把对手在场宠 hp 直改为 1）；
//     must_hit=1 + accuracy=100 全员钉死（命中链确定性）；暴击率全 0。
//   - 载体共效应中和：37311 的 1208（无下降可反转 no-op）/1709（AFTER_ACTION 桥
//     见对手已死跳过）；1669 载体兄弟 39/843/971/1531/886 只动对手 PP/先制视图/
//     对手体力，不动己方能力等级与存活数。
#include <algorithm>
#include <any>
#include <iostream>

#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;        // 拍打：物40 无副作用（负例击杀/填充用）
constexpr int kC37745 = 37745;       // 终结·独立日 [2247,2166]|[2,1] → {0}=2,{1}=1
constexpr int kC37311 = 37311;       // 天楚灭秦令 [1208,1709,2166]|[1,3,1,1] → {0}=1,{1}=1
constexpr int kC37490 = 37490;       // 恐虐 [1261,2100,2166]|[300,2,1] → {0}=2,{1}=1
constexpr int kC38322 = 38322;       // 细胞分裂 [1669,39,843]|[1,25,...] → {0}=1,{1}=25
constexpr int kC28289 = 28289;       // 苍鹭幻翎 [1669,971,81,843]|[1,50,...] → {0}=1,{1}=50
constexpr int kC27497 = 27497;       // 浴血奋战 [1669,1531,886,843]|[1,10,...] → {0}=1,{1}=10

// 与 k464_*.cpp 手工同步的白盒审计键
constexpr int kA2166Base = 1021660000;   // slot0 触发计数/1 实际回血/2 boost 值/3 武装旗
constexpr int kA1669Base = 1016690000;   // slot0 存活数/1 实际总值/2 基准/3 翻倍点/4 施放计数

int max_hp_of_pet(const ElfPet& p) {
    return p.numericalBase[NumericalPropertyIndex::HP];
}

int audit_key(int base, int owner, int slot) { return base + owner * 16 + slot; }

int stor(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
}

// 载体宠：carrier 技能 + 拍打填充；命中链确定性（accuracy=100 + must_hit）、暴击关。
ElfPet carrier_pet(int id, const std::string& name, int carrier_skill,
                   int speed, int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(carrier_skill), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
    };
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

ElfPet plain_pet(int id, const std::string& name, int speed, int atk, int hp) {
    return carrier_pet(id, name, kPaiDa, speed, atk, hp);
}

// 双载体宠（键段共存用）：两个 carrier 技能 + 拍打填充。
ElfPet carrier2_pet(int id, const std::string& name, int skill_a, int skill_b,
                    int speed, int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(skill_a), Skills(skill_b), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
    };
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

// 把对手在场宠 hp 直改（击杀驱动前奏）
void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

// 杀掉 side 前 count 个替补槽（on_stage=0 固定）→ 出战阵容存活数 = 6-count
void kill_bench(ScenarioBattle& b, int side, int count) {
    for (int slot = 1; slot <= count; ++slot) {
        b.ctx().seerRobot[side].elfPets[slot].hp = 0;
    }
}

// 找 side 第一个活着的非在场槽（死切补位用）
int next_replacement_slot(ScenarioBattle& b, int side) {
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == b.ctx().on_stage[side]) {
            continue;
        }
        if (b.ctx().seerRobot[side].elfPets[slot].hp > 0) {
            return slot;
        }
    }
    return 1;
}

// 一回合：双方提交操作；若停死切则给阵亡方补位后继续（停到下一选择期）。
void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    if (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead_side = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        b.submit_operation(dead_side, BattleFsm::ActionType::CHOOSE_PET,
                           next_replacement_slot(b, dead_side));
        b.drive();
    }
}

// 双方都可能倒下的回合收尾（亡语同波局）：循环补位到离开死切选择。
void run_round_both_deaths(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
                           BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET,
                           next_replacement_slot(b, dead));
        b.drive();
    }
}

void snapshot_levels(BattleContext& ctx, int side, int out[6]) {
    for (int i = 0; i < 6; ++i) {
        out[i] = ctx.ability_levels[side][i];
    }
}

void check_levels(BattleContext& ctx, int side, const int before[6], int delta,
                  const char* tag) {
    for (int i = 0; i < 6; ++i) {
        CHECK_EQ(ctx.ability_levels[side][i], before[i] + delta,
                 tag << " 六槽逐项 slot" << i);
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    auto& store = official_data::OfficialDataStore::instance();
    if (!store.ready() && !store.initialize()) {
        std::cerr << "official data store unavailable\n";
        return 1;
    }

    // ══ T8510 2166 基础正例（37745 {0}=2,{1}=1）：击败回 1/2 + 六槽逐项 +1 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "甲", kC37745, 300, 3000, 99999)),
                         make_party(plain_pet(2, "敌", 100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100;                     // 压缺口（1/2 非"回满"可观测）
        int before[6];
        snapshot_levels(ctx, 0, before);
        set_onstage_hp(b, 1, 1);                    // 击杀驱动
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, 100 + 99999 / 2,
                 "T8510 回血=最大体力 1/2（49999，非全恢复非缺口喂满）");
        check_levels(ctx, 0, before, 1, "T8510");
        CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 0)), 1, "T8510 触发计数=1");
        CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 1)), 99999 / 2, "T8510 实际回血量");
        CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 2)), 1, "T8510 boost 值={1}");
        CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 3)), 1, "T8510 武装旗");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 1,
                 "T8510 窗=1：武装回合末 watcher 未清（cleanup 在 advanceRound 前）");
        std::cout << "  [T8510] 2166 基础正例 通过\n";
    }

    // ══ T8511 2166 负例三腿：未武装击败 / 武装未击败 / 被击败 ══
    {
        {   // a) 未武装（拍打杀）：不回血不增益
            ScenarioBattle b(make_party(carrier_pet(1, "甲", kC37745, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;
            int before[6];
            snapshot_levels(ctx, 0, before);
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.getPet(0).hp, 100, "T8511.a 未武装击败：不回血");
            check_levels(ctx, 0, before, 0, "T8511.a");
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 0)), 0, "T8511.a 无触发");
        }
        {   // b) 武装未击败（对手满血）：不回血不增益、武装旗在
            ScenarioBattle b(make_party(carrier_pet(1, "甲", kC37745, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;
            int before[6];
            snapshot_levels(ctx, 0, before);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK(ctx.getPet(1).hp > 0, "T8511.b 前置：对手存活");
            CHECK_EQ(ctx.getPet(0).hp, 100, "T8511.b 武装未击败：不回血");
            check_levels(ctx, 0, before, 0, "T8511.b");
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 0)), 0, "T8511.b 无触发");
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 3)), 1, "T8511.b 武装旗在");
        }
        {   // c) 被击败不触发：R1 武装（未击败）→ R2 对手先手击倒武装宠
            ScenarioBattle b(make_party(carrier_pet(1, "甲", kC37745, 100, 1, 99999)),
                             make_party(plain_pet(2, "敌", 300, 300, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 3)), 1, "T8511.c R1 武装");
            ctx.getPet(0).hp = 1;                    // R2 对手先手拍死武装宠
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0,
                     "T8511.c 武装宠被击败（阵亡槽 hp=0；死切后 getPet 已是替补）");
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 0)), 0,
                     "T8511.c 被击败：无触发（actor 过滤 + 窗惰性双保险）");
            std::cout << "  [T8511] 2166 负例三腿 通过\n";
        }
    }

    // ══ T8512 2166 {0}=1 载体（37311 天楚灭秦令）：1/1 全恢复 + 链兄弟共存 ══
    // 1208（无下降可反转 no-op）与 1709（AFTER_ACTION 桥见对手已死跳过）不干扰：
    // healed 审计=99899 恰为缺口，若 1709 误先回血该值会变小。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "甲", kC37311, 300, 3000, 99999)),
                         make_party(plain_pet(2, "敌", 100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100;
        int before[6];
        snapshot_levels(ctx, 0, before);
        set_onstage_hp(b, 1, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, max_hp_of_pet(ctx.getPet(0)), "T8512 {0}=1 全恢复");
        check_levels(ctx, 0, before, 1, "T8512");
        CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 1)), 99999 - 100,
                 "T8512 实际回血=恰缺口（1709 桥未抢先回血）");
        CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 0)), 1, "T8512 触发计数=1");
        std::cout << "  [T8512] 2166 {0}=1 载体+链兄弟共存 通过\n";
    }

    // ══ T8513 2166 亡语同波负例（K453 T8413 同款）：存活门锁"奶活倒下者" ══
    // 对手 EVENT_DEATH 亡语在 drain 同波先击倒击败方 → OPPONENT_DEFEATED 投递时
    // 击败方 hp=0 且未登记 → 存活门早退：不回血（保持 0）、不增益（等级 0）、
    // 触发计数 0（变异"删存活门"→ stat_change 对死者 +1 / 审计 +1，必红）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "甲", kC37745, 300, 3000, 99999)),
                         make_party(plain_pet(2, "敌", 100, 1, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100;                     // 压缺口（亡语必杀）
        struct Obs { int type; int hp0; };          // type: 0=DEATH 1=OPP_DEFEATED
        std::vector<Obs> timeline;
        auto record = [&timeline, &ctx](int type) {
            timeline.push_back({type, ctx.getPet(0).hp});
        };
        // 亡语：对手死亡时（ev.target==1）用合法原语击倒击败方（once，真漏斗触发）
        ctx.event_center_.register_watcher(
            EventType::EVENT_DEATH, 1, ctx.roundCount, /*duration_rounds=*/0,
            /*once=*/true,
            [](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.target != 1) {
                    return;
                }
                (void)deal_true_damage(c, /*target=*/0, 99999, /*actor=*/1);
            },
            WatcherScope::TEAM, ctx.watcher_valid_id[1]);
        // 观测器（先于本回合 SKILL_EFFECT 注册 → 同波先投递）
        ctx.event_center_.register_watcher(
            EventType::EVENT_DEATH, 0, ctx.roundCount, 0, false,
            [&record](BattleContext* c, const BattleEvent& ev) {
                if (ev.target == 1) record(0);
                (void)c;
            },
            WatcherScope::TEAM, ctx.watcher_valid_id[0]);
        ctx.event_center_.register_watcher(
            EventType::EVENT_OPPONENT_DEFEATED, 0, ctx.roundCount, 0, false,
            [&record](BattleContext* c, const BattleEvent& ev) {
                if (ev.actor == 0 && ev.target == 1) record(1);
                (void)c;
            },
            WatcherScope::TEAM, ctx.watcher_valid_id[0]);
        int before[6];
        snapshot_levels(ctx, 0, before);
        set_onstage_hp(b, 1, 1);
        run_round_both_deaths(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                              BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(timeline.size(), static_cast<std::size_t>(2),
                 "T8513 事件序：DEATH(对手) 与 OPPONENT_DEFEATED 各投递一次");
        CHECK_EQ(timeline[0].type, 0, "T8513 DEATH 先投递");
        CHECK_EQ(timeline[1].type, 1, "T8513 OPPONENT_DEFEATED 同波紧随");
        CHECK_EQ(timeline[1].hp0, 0,
                 "T8513 窗口证据：OPPONENT_DEFEATED 投递时击败方 hp=0");
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0,
                 "T8513 存活门：亡语击倒者不回血（阵亡槽保持 0；死切后 getPet 已是替补）");
        CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 0)), 0, "T8513 存活门：无触发计数");
        CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 1)), 0, "T8513 存活门：无回血审计");
        std::cout << "  [T8513] 2166 亡语同波负例（存活门回归锁） 通过\n";
    }

    // ══ T8514 2166 同归保护正例（K453 T8410 同款）：内核保 1 血存活者击败成立 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "甲", kC37745, 300, 3000, 99999)),
                         make_party(plain_pet(2, "敌", 100, 1, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100;                     // 压缺口（反射真伤必杀）
        ctx.event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, 1, ctx.roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.target != 1 || ev.actor < 0 || ev.actor > 1) {
                    return;
                }
                (void)deal_true_damage(c, ev.actor, 99999, /*actor=*/1);
            },
            WatcherScope::TEAM, ctx.watcher_valid_id[1]);
        int before[6];
        snapshot_levels(ctx, 0, before);
        set_onstage_hp(b, 1, 1);
        run_round_both_deaths(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                              BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.getPet(0).hp > 0, "T8514 前置：武装宠被内核同归保护存活");
        CHECK_EQ(ctx.getPet(0).hp, 1 + 99999 / 2,
                 "T8514 同归存活者击败成立 → 1 血起回 1/2（1+49999）");
        check_levels(ctx, 0, before, 1, "T8514");
        CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 0)), 1, "T8514 触发计数=1");
        CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 1)), 99999 / 2, "T8514 实际回血量");
        std::cout << "  [T8514] 2166 同归保护正例（内核防护链回归锁） 通过\n";
    }

    // ══ T8515 2166 镜像（双边武装各响应自己的击败）+ 封回血交互 ══
    {
        {   // 镜像：击杀必须与施放同回合（窗=1）——A 施放当回合杀 → A 得奖励；
            // B 替补施放当回合杀 → B 得奖励；互不串。
            ScenarioBattle b(make_party(carrier_pet(1, "甲0", kC37745, 300, 3000, 99999)),
                             make_party(carrier_pet(2, "甲1", kC37745, 100, 3000, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            // R1 双边各自武装（互不击败——双方满血吃 ~900）
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 3)), 1, "T8515 双方武装（己方）");
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 1, 3)), 1, "T8515 双方武装（对手）");
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 0)), 0, "T8515 R1 无触发（己）");
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 1, 0)), 0, "T8515 R1 无触发（对）");
            // R2 己方施放当回合击败（对手在场 hp=1）→ 己方 +1 档（显式回满 → 缺口 0）
            ctx.getPet(0).hp = max_hp_of_pet(ctx.getPet(0));   // R1 互殴缺口清零
            set_onstage_hp(b, 1, 1);
            int before0[6];
            snapshot_levels(ctx, 0, before0);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            check_levels(ctx, 0, before0, 1, "T8515.R2 己方");
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 1)), 0,
                     "T8515.R2 满血缺口 0 → 实际回血 0");
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 1, 0)), 0, "T8515.R2 对手方无触发");
            // R3 对手替补"施放当回合击败"（窗=1 → 武装与击杀同回合）：
            // 己方在场压 1、替补压缺口 → 替补施放 37745 击败 → 对手方得奖励（镜像对称）
            ctx.getPet(1).hp = 500;
            set_onstage_hp(b, 0, 1);
            int before1[6];
            snapshot_levels(ctx, 1, before1);
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.getPet(1).hp, 500 + 99999 / 2, "T8515.R3 对手击败→对手回 1/2");
            check_levels(ctx, 1, before1, 1, "T8515.R3 对手");
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 1, 0)), 1, "T8515.R3 对手方触发=1");
            std::cout << "  [T8515.a] 2166 镜像双边武装 通过\n";
        }
        {   // 封回血：体力回 0、增益照常（HEAL_BLOCK 只封体力轴）
            ScenarioBattle b(make_party(carrier_pet(1, "甲", kC37745, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;
            ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(),
                               /*duration_rounds=*/0, /*counts=*/0, /*source_id=*/8515);
            int before[6];
            snapshot_levels(ctx, 0, before);
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.getPet(0).hp, 100, "T8515.b 封回血：体力不恢复");
            check_levels(ctx, 0, before, 1, "T8515.b");
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 1)), 0, "T8515.b 实际回血审计=0");
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 2)), 1, "T8515.b boost 值照记");
            std::cout << "  [T8515.b] 2166 封回血但增益照常 通过\n";
        }
    }

    // ══ T8516 2166 跨回合不携带 + watcher 生命周期（窗=1）══
    // R1 武装未击败 → R2 用别的技能击败：不触发（窗惰性）；R2 末 cleanup 移除。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "甲", kC37745, 300, 3000, 99999)),
                         make_party(plain_pet(2, "敌", 100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 3)), 1, "T8516 R1 武装");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 1,
                 "T8516 R1 末 watcher 在（未清）");
        set_onstage_hp(b, 1, 1);
        int before[6];
        snapshot_levels(ctx, 0, before);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, 100, "T8516 R2 跨回合击败：不回血（窗=1 惰性）");
        check_levels(ctx, 0, before, 0, "T8516");
        CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 0)), 0, "T8516 R2 无触发");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 0,
                 "T8516 R2 末 watcher 已 cleanup（生命周期全自动）");
        std::cout << "  [T8516] 2166 跨回合不携带+生命周期 通过\n";
    }

    // ══ T8517 1669 档位差分（28289 苍鹭幻翎 {0}=1,{1}=50）：存活 1..6 七档 ══
    // total = 1 + (1×N×50)/100 → N=1..6 依次 1,2,2,3,3,4（floor 线性）。
    for (int n = 1; n <= 6; ++n) {
        const int bench_dead = 6 - n;
        const int expect = 1 + (n * 50) / 100;
        ScenarioBattle b(make_party(carrier_pet(1, "甲", kC28289, 300, 3000, 99999)),
                         make_party(plain_pet(2, "敌", 100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        kill_bench(b, 0, bench_dead);
        int before[6];
        snapshot_levels(ctx, 0, before);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, before, expect, "T8517");
        CHECK_EQ(stor(ctx, audit_key(kA1669Base, 0, 0)), n, "T8517 存活数读面");
        CHECK_EQ(stor(ctx, audit_key(kA1669Base, 0, 1)), expect, "T8517 实际总值");
        CHECK_EQ(stor(ctx, audit_key(kA1669Base, 0, 2)), 1, "T8517 基准={0}");
        CHECK_EQ(stor(ctx, audit_key(kA1669Base, 0, 3)), 50, "T8517 翻倍点={1}");
        std::cout << "  [T8517 N=" << n << "] 1669 档位 total=" << expect << " 通过\n";
    }

    // ══ T8518 1669 floor 锁与逐载体切片：27497 {1}=10 / 38322 {1}=25 / 对手面负测 ══
    {
        {   // {1}=10 全存活 N=6：1 + 60/100 = 1（floor 锁：ceil/四舍五入变异必红）
            ScenarioBattle b(make_party(carrier_pet(1, "甲", kC27497, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            int before[6];
            snapshot_levels(ctx, 0, before);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            check_levels(ctx, 0, before, 1, "T8518.a");
            CHECK_EQ(stor(ctx, audit_key(kA1669Base, 0, 1)), 1, "T8518.a floor：总值仍=1");
            std::cout << "  [T8518.a] 1669 {1}=10 floor 锁 通过\n";
        }
        {   // {1}=25 N=3：1 + 75/100 = 1（未过进位档）
            ScenarioBattle b(make_party(carrier_pet(1, "甲", kC38322, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            kill_bench(b, 0, 3);
            int before[6];
            snapshot_levels(ctx, 0, before);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            check_levels(ctx, 0, before, 1, "T8518.b");
            CHECK_EQ(stor(ctx, audit_key(kA1669Base, 0, 1)), 1, "T8518.b N=3 总值=1");
            std::cout << "  [T8518.b] 1669 {1}=25 N=3 通过\n";
        }
        {   // {1}=25 N=4：1 + 100/100 = 2（恰过进位档）
            ScenarioBattle b(make_party(carrier_pet(1, "甲", kC38322, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            kill_bench(b, 0, 2);
            int before[6];
            snapshot_levels(ctx, 0, before);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            check_levels(ctx, 0, before, 2, "T8518.c");
            CHECK_EQ(stor(ctx, audit_key(kA1669Base, 0, 0)), 4, "T8518.c 存活数=4");
            CHECK_EQ(stor(ctx, audit_key(kA1669Base, 0, 1)), 2, "T8518.c N=4 总值=2");
            std::cout << "  [T8518.c] 1669 {1}=25 N=4 通过\n";
        }
        {   // 对手面负测：己方 N=1（5 替补倒）而对手 6 只全存活 → 仍只 +1
            ScenarioBattle b(make_party(carrier_pet(1, "甲", kC28289, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            kill_bench(b, 0, 5);
            int before[6];
            snapshot_levels(ctx, 0, before);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            check_levels(ctx, 0, before, 1, "T8518.d");
            CHECK_EQ(stor(ctx, audit_key(kA1669Base, 0, 0)), 1, "T8518.d 读面=己方 N=1");
            std::cout << "  [T8518.d] 1669 对手面负测 通过\n";
        }
    }

    // ══ T8519 1669 镜像 + 两族键段共存 + 满级钳制 + 重复施放叠加 ══
    {
        {   // 镜像：side1 持印按己方存活数翻倍，side0 不动
            ScenarioBattle b(make_party(plain_pet(1, "敌", 300, 1, 99999)),
                             make_party(carrier_pet(2, "甲1", kC28289, 100, 3000, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            int before1[6];
            snapshot_levels(ctx, 1, before1);
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            check_levels(ctx, 1, before1, 4, "T8519.a 对手方 N=6 → +4");
            for (int i = 0; i < 6; ++i) {
                CHECK_EQ(ctx.ability_levels[0][i], 0, "T8519.a 己方不动");
            }
            CHECK_EQ(stor(ctx, audit_key(kA1669Base, 1, 0)), 6, "T8519.a 对手方读面 N=6");
            CHECK_EQ(stor(ctx, audit_key(kA1669Base, 1, 1)), 4, "T8519.a 对手方总值=4");
            std::cout << "  [T8519.a] 1669 镜像 通过\n";
        }
        {   // 两族键段共存：同宠先施 38322（1669 +2）再施 37745 击败（2166 回 1/2+1）
            ScenarioBattle b(make_party(carrier2_pet(1, "甲", kC38322, kC37745, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            // R1：38322（1669 链首 +{1}=25 N=6 → +2）
            int before[6];
            snapshot_levels(ctx, 0, before);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            check_levels(ctx, 0, before, 2, "T8519.b R1 1669");
            CHECK_EQ(stor(ctx, audit_key(kA1669Base, 0, 1)), 2, "T8519.b 1669 总值=2");
            CHECK_EQ(stor(ctx, audit_key(kA1669Base, 0, 4)), 1, "T8519.b 1669 施放计数=1");
            // R2：压缺口 → 37745 击败 → 2166 回 1/2 + 全属性再 +1
            ctx.getPet(0).hp = 300;
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.getPet(0).hp, 300 + 99999 / 2, "T8519.b R2 2166 回 1/2");
            check_levels(ctx, 0, before, 3, "T8519.b R2 累计 2+1");
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 0)), 1, "T8519.b 2166 触发=1");
            CHECK_EQ(stor(ctx, audit_key(kA2166Base, 0, 1)), 99999 / 2, "T8519.b 2166 回血审计");
            CHECK_EQ(stor(ctx, audit_key(kA1669Base, 0, 4)), 1, "T8519.b 1669 计数不被 2166 污染");
            std::cout << "  [T8519.b] 两族键段共存 通过\n";
        }
        {   // 满级钳制：slot2 预置 +5 → N=4 的 +2 整笔越顶被 AT_CAP 拒（原语到顶不变更，
            // 坑集 §1② 合法通过态）；其余槽 +2；重复施放叠加、slot2 恒 5
            ScenarioBattle b(make_party(carrier_pet(1, "甲", kC38322, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            kill_bench(b, 0, 2);                     // N=4 → total=2
            ctx.ability_levels[0][2] = 5;            // 预置防御槽近顶（5+2 越顶）
            int before[6];
            snapshot_levels(ctx, 0, before);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            for (int i = 0; i < 6; ++i) {
                if (i == 2) {
                    CHECK_EQ(ctx.ability_levels[0][i], 5,
                             "T8519.c slot2 5+2 越顶 → AT_CAP 整笔拒（保持 5，合法通过态）");
                } else {
                    CHECK_EQ(ctx.ability_levels[0][i], before[i] + 2, "T8519.c 其余槽 +2");
                }
            }
            CHECK_EQ(stor(ctx, audit_key(kA1669Base, 0, 1)), 2, "T8519.c 审计总值=2（钳制不改审计）");
            // 重复施放：其余槽再 +2 叠加、slot2 恒 5
            int mid[6];
            snapshot_levels(ctx, 0, mid);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            for (int i = 0; i < 6; ++i) {
                if (i == 2) {
                    CHECK_EQ(ctx.ability_levels[0][i], 5, "T8519.c 重复施放 slot2 恒 5");
                } else {
                    CHECK_EQ(ctx.ability_levels[0][i], mid[i] + 2, "T8519.c 重复施放叠加");
                }
            }
            CHECK_EQ(stor(ctx, audit_key(kA1669Base, 0, 4)), 2, "T8519.c 施放计数=2");
            std::cout << "  [T8519.c] 1669 满级钳制+重复施放 通过\n";
        }
    }

    std::cout << "scenario_8510 全部通过\n";
    return 0;
}
