// 场景 8660 —— K479 两族（1436 当回合击败回满/未触发全属性 + 536 当次攻击击败回敌，
// moves_lib/k479_defeat_fullheal_boost_and_kill_foe_restore.cpp）
//（2026-11-18，worktree ~/.agent-workspaces/workspaces/sbs-local/k479；号段 8660-8669）
//
// 断言口径：
//   - 1436 = 官方击败漏斗时点（EVENT_OPPONENT_DEFEATED，K453/K464 直系）+ 窗=1
//     （"当回合"面）；S1 口径（encarlos 1040 结构双胞胎裁定）：未击败整段不结算；
//     击败 → 掷 {0}% → 恢复落地（heal_max_hp_fraction 返回 hp 差）>0 → 只回满；
//     恢复未落地（掷歪/封回血钳零/满血钳零统一）→ 全属性+{1} 六槽逐项。
//   - 536 = 同漏斗同窗；击败 → revive_pet(对手, 对手上限/{0})（登记死亡宠唯一
//     合法出口，heal_impl 死亡裁决封闭 no-op——引擎法条）。复活取消死切补位 =
//     handle_ChooseAfterDeath 安全网明文路径；全灭终局不回滚（D5 口径）。
//   - 白盒审计槽：plugin_storage 键 1014360000/1005360000 子块（与 k479_*.cpp
//     常量手工同步）；概率面用 EffectParamOverride {0}→100/0 双确定性档 + 实载体
//     {0}=50/70 不变式腿（掷中 XOR 回退，审计槽一致性），无随机精确断言。
//   - 击杀驱动 = K464 8510 同款（foe NONE 不还手 + 事前把对手在场宠 hp 直改为 1）；
//     must_hit=1 + accuracy=100 全员钉死（命中链确定性）；暴击率全 0。
//   - 载体共效应中和：1436 载体链上兄弟按腿禁用（1218 先制/920 增伤/833 吸取/
//     951 反转/942 消回合+吸取/1274 异常/1284 封属/1244 固伤——全为已实现效果，
//     不中和会污染等级与血量断言面）；共存腿显式保留链兄弟（582/471）验证互不干扰。
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

// ── 1436 载体（chaos boss kit）──
constexpr int kC32912 = 32912;       // 混沌·生死判 [1436,1218,1437]|[50,1,...] → [50,1]
constexpr int kM32912 = 3788;        //   hide 宿主
constexpr int kC33806 = 33806;       // 混沌·八荒逝 [1516,833,1436]|[3,50,1] → [50,1]
constexpr int kM33806 = 3964;
constexpr int kC34068 = 34068;       // 哀·岩崩地冲 [951,920,1436]|[1,50,50,1] → [50,1]
constexpr int kM34068 = 4020;
constexpr int kC34834 = 34834;       // 回光赤箭雨 [31,942,1274,1436]|[...,50,1] → [50,1]
constexpr int kM34834 = 4173;
constexpr int kC36775 = 36775;       // 翡翡聚星闪 [1284,1244,1436]|[...,70,1] → [70,1]
constexpr int kM36775 = 4538;        //   learnable 宿主

// ── 536 载体 ──
constexpr int kC15076 = 15076;       // 幽冥铁碎牙 [536]|[2] → {0}=2
constexpr int kM15076 = 2088;        //   戴斯
constexpr int kC15971 = 15971;       // 最后遗言 [536]|[1] → {0}=1（回满复活）
constexpr int kM15971 = 2418;        //   西姆
constexpr int kC16215 = 16215;       // 圣光创世箭 [471,536]|[2,2] → {0}=2
constexpr int kM16215 = 2488;        //   创世圣弓
constexpr int kC16266 = 16266;       // 箴言灭世咒 [593,536]|[0,4,20,2] → {0}=2
constexpr int kM16266 = 2501;        //   美狄亚
constexpr int kC31198 = 31198;       // 全能机械姬 [582,536]|[2,2] → {0}=2
constexpr int kM31198 = 3452;        //   威莉

// 与 k479_*.cpp 手工同步的白盒审计键
constexpr int kA1436Base = 1014360000;
//   slot0 击败回调计数 / 1 末次掷点(1中0歪) / 2 末次恢复落地量 / 3 末次回退 boost /
//   4 武装旗 / 5 跳过原因 / 6 {0} 回显 / 7 {1} 回显
constexpr int kA536Base = 1005360000;
//   slot0 击败回调计数 / 1 末次复活量 / 2 末次复活后 foe hp / 3 武装旗 /
//   4 跳过原因 / 5 {0} 回显

int max_hp_of_pet(const ElfPet& p) {
    return p.numericalBase[NumericalPropertyIndex::HP];
}

int audit_key(int base, int owner, int slot) { return base + owner * 16 + slot; }

int stor(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
}

// 载体宠：carrier 技能（全链）+ 拍打填充；命中链确定性、暴击关。
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

// 行参覆盖/链兄弟禁用变体宠（Skills(id, monster, overrides, disabled) 装配期基建，
// K453/K458/K467/K471/K473 同款；overrides 的 arg_index 与模板 {n} 同下标，
// effect_id = 载体技能上被覆盖的目标效果号）。
ElfPet slice_pet(int id, const char* name, int carrier, int monster_id, int effect_id,
                 const std::vector<std::pair<int, int>>& arg_overrides,
                 const std::vector<int>& disabled_effects, int speed, int atk, int hp) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(monster_id);
    if (!monster) {
        std::cerr << "slice_pet: monster " << monster_id << " load failed" << std::endl;
        std::abort();
    }
    std::vector<EffectParamOverride> overrides;
    for (const auto& [idx, value] : arg_overrides) {
        overrides.push_back({carrier, effect_id, idx, value});
    }
    DisabledEffects disabled;
    disabled.skill_effects = disabled_effects;
    std::array<Skills, 5> skills{Skills(carrier, *monster, overrides, disabled), Skills(kPaiDa),
                                 Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa)};
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
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{}, CommonTrait{},
                  base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0, /*is_locked=*/false,
                  std::move(skills));
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

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

int onstage_hp(ScenarioBattle& b, int side) {
    return b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp;
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

    // ══ T8660 1436 两分支确定性（EffectParamOverride {0}→100/0）+ S1 负测 ══
    {
        {   // a) {0}=100 必掷中：击败 → 回满（恢复落地>0）→ 无全属性（触发不回退）
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC32912, kM32912, 1436,
                                                  {{0, 100}}, {1218, 1437}, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;                 // 压缺口（回满可观测）
            int before[6];
            snapshot_levels(ctx, 0, before);
            set_onstage_hp(b, 1, 1);                // 击杀驱动
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.getPet(0).hp, max_hp_of_pet(ctx.getPet(0)),
                     "T8660.a {0}=100 掷中 → 回满");
            check_levels(ctx, 0, before, 0, "T8660.a");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 0)), 1, "T8660.a 击败回调计数=1");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 1)), 1, "T8660.a 掷点=1（中）");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 2)), 99999 - 100,
                     "T8660.a 恢复落地=恰缺口");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 3)), 0, "T8660.a 无回退 boost");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 4)), 1, "T8660.a 武装旗");
            std::cout << "  [T8660.a] 1436 {0}=100 掷中回满 通过\n";
        }
        {   // b) {0}=0 必掷歪（关闸档在位消费）：击败 → 恢复未落地 → 全属性+{1}
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC32912, kM32912, 1436,
                                                  {{0, 0}}, {1218, 1437}, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;
            int before[6];
            snapshot_levels(ctx, 0, before);
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.getPet(0).hp, 100, "T8660.b {0}=0 掷歪 → 不回血");
            check_levels(ctx, 0, before, 1, "T8660.b 全属性+1 六槽逐项");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 1)), 0, "T8660.b 掷点=0（歪）");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 2)), 0, "T8660.b 恢复落地=0");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 3)), 1, "T8660.b 回退 boost={1}");
            std::cout << "  [T8660.b] 1436 {0}=0 掷歪走回退 通过\n";
        }
        {   // c) S1 负测（未击败整段不结算）：武装未击败 → 不掷点不回退不回血
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC32912, kM32912, 1436,
                                                  {{0, 100}}, {1218, 1437}, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;
            int before[6];
            snapshot_levels(ctx, 0, before);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK(onstage_hp(b, 1) > 0, "T8660.c 前置：对手存活（未击败）");
            CHECK_EQ(ctx.getPet(0).hp, 100, "T8660.c 未击败 → 不回血");
            check_levels(ctx, 0, before, 0, "T8660.c 未击败 → 无全属性（门关整段不结算）");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 0)), 0, "T8660.c 击败回调=0");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 1)), 0, "T8660.c 未掷点（默认 0）");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 4)), 1, "T8660.c 武装旗在");
            std::cout << "  [T8660.c] 1436 S1 未击败整段不结算 通过\n";
        }
        {   // d) 被击败不触发：R1 武装（未击败）→ R2 对手先手击倒武装宠
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC32912, kM32912, 1436,
                                                  {{0, 100}}, {1218, 1437}, 100, 1, 99999)),
                             make_party(plain_pet(2, "敌", 300, 300, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 4)), 1, "T8660.d R1 武装");
            ctx.getPet(0).hp = 1;                   // R2 对手先手拍死武装宠
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0,
                     "T8660.d 武装宠被击败（阵亡槽 hp=0；死切后 getPet 已是替补）");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 0)), 0,
                     "T8660.d 被击败：击败回调=0（actor 过滤 + 窗惰性双保险）");
            std::cout << "  [T8660.d] 1436 被击败不触发 通过\n";
        }
    }

    // ══ T8661 1436 存活门/同归/封回血/满血（S1 落地判据四腿）══
    {
        {   // a) 亡语同波存活门（K453 T8413 同款）：对手 DEATH 亡语先击倒击败方
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC32912, kM32912, 1436,
                                                  {{0, 100}}, {1218, 1437}, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;                 // 压缺口（亡语必杀）
            struct Obs { int type; int hp0; };      // type: 0=DEATH 1=OPP_DEFEATED
            std::vector<Obs> timeline;
            auto record = [&timeline, &ctx](int type) {
                timeline.push_back({type, ctx.getPet(0).hp});
            };
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
                     "T8661.a 事件序：DEATH(对手) 与 OPPONENT_DEFEATED 各投递一次");
            CHECK_EQ(timeline[0].type, 0, "T8661.a DEATH 先投递");
            CHECK_EQ(timeline[1].type, 1, "T8661.a OPPONENT_DEFEATED 同波紧随");
            CHECK_EQ(timeline[1].hp0, 0,
                     "T8661.a 窗口证据：OPPONENT_DEFEATED 投递时击败方 hp=0");
            CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0,
                     "T8661.a 存活门：亡语击倒者不回满（阵亡槽保持 0）");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 0)), 1, "T8661.a 回调到达计数=1");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 5)), 1, "T8661.a 跳过原因=存活门");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 2)), 0, "T8661.a 无恢复落地");
            std::cout << "  [T8661.a] 1436 亡语同波存活门 通过\n";
        }
        {   // b) 同归保护正例（K453 T8410/K464 T8514 同款）：内核保 1 血存活者击败成立
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC32912, kM32912, 1436,
                                                  {{0, 100}}, {1218, 1437}, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;
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
            CHECK(ctx.getPet(0).hp > 0, "T8661.b 前置：武装宠被内核同归保护存活");
            CHECK_EQ(ctx.getPet(0).hp, max_hp_of_pet(ctx.getPet(0)),
                     "T8661.b 同归存活者击败成立 → 1 血起回满");
            check_levels(ctx, 0, before, 0, "T8661.b 掷中无回退");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 0)), 1, "T8661.b 击败回调=1");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 2)), 99999 - 1, "T8661.b 恢复落地");
            std::cout << "  [T8661.b] 1436 同归保护正例 通过\n";
        }
        {   // c) 封回血 → 恢复未落地 → 回退（S1 落地判据；1040「未触发=没成功落上」
            //   对偶：HEAL_BLOCK 之于恢复 ≡ 免疫之于异常）
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC32912, kM32912, 1436,
                                                  {{0, 100}}, {1218, 1437}, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;
            ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(),
                               /*duration_rounds=*/0, /*counts=*/0, /*source_id=*/8661);
            int before[6];
            snapshot_levels(ctx, 0, before);
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.getPet(0).hp, 100, "T8661.c 封回血：体力不恢复");
            check_levels(ctx, 0, before, 1, "T8661.c 恢复未落地 → 全属性+1");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 1)), 1, "T8661.c 掷点=1（中）");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 2)), 0, "T8661.c 恢复落地=0（被封）");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 3)), 1, "T8661.c 回退 boost={1}");
            std::cout << "  [T8661.c] 1436 封回血→回退 通过\n";
        }
        {   // d) 满血钳零 → 恢复未落地 → 回退（K473 全零档入回退同哲学）
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC32912, kM32912, 1436,
                                                  {{0, 100}}, {1218, 1437}, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            int before[6];
            snapshot_levels(ctx, 0, before);
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.getPet(0).hp, max_hp_of_pet(ctx.getPet(0)),
                     "T8661.d 前置：满血（缺口 0）");
            check_levels(ctx, 0, before, 1, "T8661.d 满血钳零 → 全属性+1");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 1)), 1, "T8661.d 掷点=1");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 2)), 0, "T8661.d 恢复落地=0");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 3)), 1, "T8661.d 回退 boost={1}");
            std::cout << "  [T8661.d] 1436 满血钳零→回退 通过\n";
        }
    }

    // ══ T8662 1436 生命周期/重武装/镜像 ══
    {
        {   // a) 窗=1 跨回合不携带：R1 武装未击败 → R2 拍打击败 → 不结算；R2 末 cleanup
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC32912, kM32912, 1436,
                                                  {{0, 100}}, {1218, 1437}, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 4)), 1, "T8662.a R1 武装");
            CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 1,
                     "T8662.a R1 末 watcher 在（未清）");
            set_onstage_hp(b, 1, 1);
            int before[6];
            snapshot_levels(ctx, 0, before);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.getPet(0).hp, 100, "T8662.a R2 跨回合击败：不回血（窗=1 惰性）");
            check_levels(ctx, 0, before, 0, "T8662.a R2 无全属性");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 0)), 0, "T8662.a R2 击败回调=0");
            CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 0,
                     "T8662.a R2 末 watcher 已 cleanup（生命周期全自动）");
            std::cout << "  [T8662.a] 1436 跨回合不携带+生命周期 通过\n";
        }
        {   // b) 重武装防双 watcher（K453/K464 卫生）：R1 武装 → R2 重施放同回合击败
            //   → 恰结算一次（旧 watcher 已摘，无双投）
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC32912, kM32912, 1436,
                                                  {{0, 100}}, {1218, 1437}, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 1,
                     "T8662.b R1 末 watcher=1");
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.getPet(0).hp, max_hp_of_pet(ctx.getPet(0)), "T8662.b R2 回满");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 0)), 1,
                     "T8662.b 重武装后击败回调恰=1（无双 watcher 双投）");
            CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 1,
                     "T8662.b R2 末 watcher 仍=1（摘旧+新 1 条）");
            std::cout << "  [T8662.b] 1436 重武装防双 watcher 通过\n";
        }
        {   // c) 镜像：双边武装各响应自己的击败（T8515.a 同构）
            ScenarioBattle b(make_party(slice_pet(1, "甲0", kC32912, kM32912, 1436,
                                                  {{0, 0}}, {1218, 1437}, 300, 3000, 99999)),
                             make_party(slice_pet(2, "甲1", kC32912, kM32912, 1436,
                                                  {{0, 0}}, {1218, 1437}, 100, 3000, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            // R1 双边各自武装（互不击败——双方满血吃小伤）
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 4)), 1, "T8662.c 双方武装（己方）");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 1, 4)), 1, "T8662.c 双方武装（对手）");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 0)), 0, "T8662.c R1 无触发（己）");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 1, 0)), 0, "T8662.c R1 无触发（对）");
            // R2 己方施放当回合击败（对手在场 hp=1）→ 己方 {0}=0 → 全属性+1
            set_onstage_hp(b, 1, 1);
            int before0[6];
            snapshot_levels(ctx, 0, before0);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            check_levels(ctx, 0, before0, 1, "T8662.c R2 己方掷歪回退");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 1, 0)), 0, "T8662.c R2 对手方无触发");
            // R3 对手替补施放当回合击败（窗=1）→ 对手方得回退（镜像对称）
            ctx.getPet(1).hp = 500;
            set_onstage_hp(b, 0, 1);
            int before1[6];
            snapshot_levels(ctx, 1, before1);
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            check_levels(ctx, 1, before1, 1, "T8662.c R3 对手镜像回退");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 1, 0)), 1, "T8662.c R3 对手方回调=1");
            std::cout << "  [T8662.c] 1436 镜像双边武装 通过\n";
        }
    }

    // ══ T8663 1436 五载体切片机验 + 实载体不变式 + 两族键段共存 ══
    {
        {   // a) 五载体 arm + {0}/{1} 行参回显（切片机验：50/1 ×4、70/1 ×1）
            const struct { int carrier; int monster; int effect; int pct; int boost; } rows[] = {
                {kC32912, kM32912, 1436, 50, 1}, {kC33806, kM33806, 1436, 50, 1},
                {kC34068, kM34068, 1436, 50, 1}, {kC34834, kM34834, 1436, 50, 1},
                {kC36775, kM36775, 1436, 70, 1},
            };
            for (const auto& r : rows) {
                ScenarioBattle b(make_party(slice_pet(1, "甲", r.carrier, r.monster, r.effect, {},
                                                      {}, 300, 3000, 99999)),
                                 make_party(plain_pet(2, "敌", 100, 1, 99999)));
                b.start();
                BattleContext& ctx = b.ctx();
                run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
                CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 4)), 1, "T8663.a 武装旗");
                CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 6)), r.pct, "T8663.a {0} 回显");
                CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 7)), r.boost, "T8663.a {1} 回显");
            }
            std::cout << "  [T8663.a] 1436 五载体切片回显 通过\n";
        }
        {   // b) 实载体 {0}=50 不变式：掷中 XOR 回退（审计槽一致性；无随机精确断言）
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC32912, kM32912, 1436, {}, {},
                                                  300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;
            int before[6];
            snapshot_levels(ctx, 0, before);
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            const int rolled = stor(ctx, audit_key(kA1436Base, 0, 1));
            const int healed = stor(ctx, audit_key(kA1436Base, 0, 2));
            const int boosted = stor(ctx, audit_key(kA1436Base, 0, 3));
            CHECK(rolled == 0 || rolled == 1, "T8663.b 掷点记录 ∈ {0,1}");
            CHECK((rolled == 1) == (healed > 0), "T8663.b 掷中 ⇔ 恢复落地>0");
            CHECK((rolled == 1) == (boosted == 0), "T8663.b 掷中 ⇔ 无回退");
            const bool full = ctx.getPet(0).hp == max_hp_of_pet(ctx.getPet(0));
            int delta = ctx.ability_levels[0][0] - before[0];
            CHECK(full != (delta == 1), "T8663.b 不变式：回满 XOR 全属性+1");
            for (int i = 1; i < 6; ++i) {
                CHECK_EQ(ctx.ability_levels[0][i] - before[i], delta,
                         "T8663.b 六槽一致（全或无）");
            }
            std::cout << "  [T8663.b] 1436 实载体 {0}=50 不变式（掷点实测="
                      << rolled << "） 通过\n";
        }
        {   // c) 实载体 {0}=70 不变式（36775 翡翡聚星闪；全链含 1244 粉伤 100-200——
            //   引擎事实：粉伤击杀不写 last_damage_actor（run_pink_damage 无归因写，
            //   battle_primitives.cpp:975-1051）→ 击败事件 actor=-1；对手压血 300 让
            //   粉伤先削、**红伤落杀**保归因（deal_damage :1121 写 actor），不变式腿干净）
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC36775, kM36775, 1436, {}, {},
                                                  300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;
            int before[6];
            snapshot_levels(ctx, 0, before);
            set_onstage_hp(b, 1, 300);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            const int rolled = stor(ctx, audit_key(kA1436Base, 0, 1));
            const bool full = ctx.getPet(0).hp == max_hp_of_pet(ctx.getPet(0));
            const int delta = ctx.ability_levels[0][0] - before[0];
            CHECK((rolled == 1) == full, "T8663.c 掷中 ⇔ 回满");
            CHECK(full != (delta == 1), "T8663.c 不变式：回满 XOR 全属性+1");
            std::cout << "  [T8663.c] 1436 实载体 {0}=70 不变式（掷点实测="
                      << rolled << "） 通过\n";
        }
        {   // d) 两族键段共存 + 窗惰性分工：R1 武装 1436（32912）→ R2 施 15971（536）
            //   击败 → 536 结算、1436 窗过期不结算；键段互不污染
            ScenarioBattle b(make_party(
                                 carrier2_pet(1, "甲", kC32912, kC15971, 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            // R1：32912（链 1218 反转对手提升 no-op/1437 无"辛" no-op）武装 1436，无击败
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 4)), 1, "T8663.d R1 1436 武装");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 3)), 0, "T8663.d R1 536 未武装");
            // R2：15971 击败（对手在场压 1）→ 536 复活对手满血；1436 窗过期不结算
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(onstage_hp(b, 1), max_hp_of_pet(ctx.getPet(1)),
                     "T8663.d R2 536 回满复活对手");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 0)), 1, "T8663.d R2 536 回调=1");
            CHECK_EQ(stor(ctx, audit_key(kA1436Base, 0, 0)), 0,
                     "T8663.d R2 1436 窗过期无回调（键段不串）");
            // R3：再施 32912 击败 → 1436 结算；536 窗过期不结算（镜像分工）
            ctx.getPet(0).hp = 100;
            set_onstage_hp(b, 1, 1);
            int before[6];
            snapshot_levels(ctx, 0, before);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            const int rolled = stor(ctx, audit_key(kA1436Base, 0, 1));
            CHECK(rolled == 0 || rolled == 1, "T8663.d R3 1436 掷点已发生");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 0)), 1,
                     "T8663.d R3 536 计数仍=1（窗过期，R3 击败不再复活）");
            CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 0,
                     "T8663.d R3 击败未被复活（536 窗过期）→ 被击败宠保持死亡");
            std::cout << "  [T8663.d] 两族键段共存+窗惰性分工 通过\n";
        }
    }

    // ══ T8664 536 分支正负（真载体）══
    {
        {   // a) 15971 {0}=1 回满复活：击败 → 对手复活满血、无死切补位；DEATH 先投递
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC15971, kM15971, 536, {}, {},
                                                  300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            int death_actor = -99;
            ctx.event_center_.register_watcher(
                EventType::EVENT_DEATH, 0, ctx.roundCount, 0, false,
                [&death_actor](BattleContext* c, const BattleEvent& ev) {
                    if (ev.target == 1) death_actor = ev.actor;
                    (void)c;
                },
                WatcherScope::TEAM, ctx.watcher_valid_id[0]);
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(death_actor, 0, "T8664.a DEATH 归因=己方（击败成立）");
            CHECK_EQ(ctx.on_stage[1], 0, "T8664.a 对手无补位（复活取消死切）");
            CHECK_EQ(onstage_hp(b, 1), max_hp_of_pet(ctx.getPet(1)),
                     "T8664.a {0}=1 复活满血");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 0)), 1, "T8664.a 回调=1");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 1)), 99999, "T8664.a 复活量=上限");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 2)), 99999, "T8664.a 复活后 hp");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 3)), 1, "T8664.a 武装旗");
            std::cout << "  [T8664.a] 536 {0}=1 回满复活 通过\n";
        }
        {   // b) 15076 {0}=2 半血复活
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC15076, kM15076, 536, {}, {},
                                                  300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.on_stage[1], 0, "T8664.b 对手无补位");
            CHECK_EQ(onstage_hp(b, 1), 99999 / 2, "T8664.b {0}=2 复活半血（整除 floor）");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 1)), 99999 / 2, "T8664.b 复活量");
            std::cout << "  [T8664.b] 536 {0}=2 半血复活 通过\n";
        }
        {   // c) 未击败负测：武装未击败 → 对手只吃伤害不回血、无复活
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC15076, kM15076, 536, {}, {},
                                                  300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int foe_max = max_hp_of_pet(ctx.getPet(1));
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK(onstage_hp(b, 1) > 0, "T8664.c 前置：对手存活");
            CHECK(onstage_hp(b, 1) < foe_max, "T8664.c 对手只吃伤害（未被回血到满）");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 0)), 0, "T8664.c 未击败：回调=0");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 3)), 1, "T8664.c 武装旗在");
            std::cout << "  [T8664.c] 536 未击败负测 通过\n";
        }
        {   // d) 未武装负测：拍打杀 → 无复活 → 死切补位照走
            ScenarioBattle b(make_party(plain_pet(1, "甲", 300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 0, "T8664.d 无武装：被击败宠保持死亡");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 0)), 0, "T8664.d 无回调");
            std::cout << "  [T8664.d] 536 未武装负测 通过\n";
        }
        {   // e) 跨回合窗负测：R1 武装未击败 → R2 拍打击败 → 不复活（窗=1 惰性）
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC15076, kM15076, 536, {}, {},
                                                  300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 3)), 1, "T8664.e R1 武装");
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 0, "T8664.e R2 跨回合击败不复活");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 0)), 0, "T8664.e R2 回调=0");
            CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 0,
                     "T8664.e R2 末 watcher 已 cleanup");
            std::cout << "  [T8664.e] 536 跨回合窗负测 通过\n";
        }
    }

    // ══ T8665 536 归因/存活/封回血精密腿 ══
    {
        {   // a) 异常跳死不复活（"当次攻击击败"归因精密）：烧伤 tick 自因致死
            //   （actor=被击败方自己，battleFsm settle_staged 归因口径）→ 回调过滤不中
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC15076, kM15076, 536, {}, {},
                                                  300, 100, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            int death_actor = -99;
            ctx.event_center_.register_watcher(
                EventType::EVENT_DEATH, 0, ctx.roundCount, 0, false,
                [&death_actor](BattleContext* c, const BattleEvent& ev) {
                    if (ev.target == 1) death_actor = ev.actor;
                    (void)c;
                },
                WatcherScope::TEAM, ctx.watcher_valid_id[0]);
            // R1 开局直接对对手挂烧伤（测试面原语）：tick=最大体力 1/8=12499，
            // 对手压血到恰 1/8：己方攻击（atk=100 小伤）后残血 <12499 → tick 击杀
            apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Burn),
                          /*duration_rounds=*/2, /*actor=*/0);
            set_onstage_hp(b, 1, 99999 / 8);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
            CHECK_EQ(death_actor, 1,
                     "T8665.a DEATH 归因=对手自身（烧伤 tick 自因，非己方攻击）");
            CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 0, "T8665.a 对手死亡无复活");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 0)), 0,
                     "T8665.a 异常跳死：回调过滤不中（actor≠owner）");
            std::cout << "  [T8665.a] 536 异常跳死不复活 通过\n";
        }
        {   // b) 亡语同波存活门（536 版 T8413）：击败方被亡语击倒 → 不复活对手
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC15076, kM15076, 536, {}, {},
                                                  300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;
            ctx.event_center_.register_watcher(
                EventType::EVENT_DEATH, 1, ctx.roundCount, 0, true,
                [](BattleContext* c, const BattleEvent& ev) {
                    if (!c || ev.target != 1) {
                        return;
                    }
                    (void)deal_true_damage(c, /*target=*/0, 99999, /*actor=*/1);
                },
                WatcherScope::TEAM, ctx.watcher_valid_id[1]);
            set_onstage_hp(b, 1, 1);
            run_round_both_deaths(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                                  BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 0)), 1, "T8665.b 回调到达=1");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 4)), 1, "T8665.b 跳过原因=存活门");
            CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 0,
                     "T8665.b 击败方已倒：对手不复活（官方「击败」=未死者触发）");
            std::cout << "  [T8665.b] 536 亡语同波存活门 通过\n";
        }
        {   // c) 同归保护存活者复活正例：内核保 1 血 → 击败成立 → 复活照常
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC15076, kM15076, 536, {}, {},
                                                  300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;
            ctx.event_center_.register_watcher(
                EventType::EVENT_TAKE_DAMAGE, 1, ctx.roundCount, 0, false,
                [](BattleContext* c, const BattleEvent& ev) {
                    if (!c || ev.target != 1 || ev.actor < 0 || ev.actor > 1) {
                        return;
                    }
                    (void)deal_true_damage(c, ev.actor, 99999, /*actor=*/1);
                },
                WatcherScope::TEAM, ctx.watcher_valid_id[1]);
            set_onstage_hp(b, 1, 1);
            run_round_both_deaths(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                                  BattleFsm::ActionType::NONE, 0);
            CHECK(ctx.getPet(0).hp > 0, "T8665.c 前置：击败方被同归保护存活");
            CHECK_EQ(onstage_hp(b, 1), 99999 / 2, "T8665.c 存活者击败成立 → 复活半血");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 0)), 1, "T8665.c 回调=1");
            std::cout << "  [T8665.c] 536 同归保护正例 通过\n";
        }
        {   // d) 封回血不拦复活：对手 HEAL_BLOCK → 仍复活（复活走 revive_pet 出口，
            //   不经 heal 管线——引擎法条记档腿）
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC15076, kM15076, 536, {}, {},
                                                  300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.grant_immunity(1, ImmunityType::HEAL_BLOCK, coverage_all(),
                               /*duration_rounds=*/0, /*counts=*/0, /*source_id=*/8665);
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(onstage_hp(b, 1), 99999 / 2, "T8665.d 封回血不拦复活（半血拉回）");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 1)), 99999 / 2, "T8665.d 复活量照记");
            std::cout << "  [T8665.d] 536 封回血不拦复活 通过\n";
        }
    }

    // ══ T8666 536 重武装/镜像/全灭终局 ══
    {
        {   // a) 重武装防双 watcher：R1 武装 → R2 重施放同回合击败 → 恰复活一次
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC15076, kM15076, 536, {}, {},
                                                  300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 1,
                     "T8666.a R1 末 watcher=1");
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(onstage_hp(b, 1), 99999 / 2, "T8666.a R2 复活半血");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 0)), 1,
                     "T8666.a 重武装后回调恰=1（无双 watcher 双投）");
            std::cout << "  [T8666.a] 536 重武装防双 watcher 通过\n";
        }
        {   // b) 镜像：side1 武装并击败 → side0 的倒地宠被 side1 复活
            ScenarioBattle b(make_party(plain_pet(1, "敌", 100, 1, 99999)),
                             make_party(slice_pet(2, "甲1", kC15076, kM15076, 536, {}, {},
                                                  300, 3000, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            set_onstage_hp(b, 0, 1);                // 击杀驱动（side1 先手击败 side0）
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 1, 3)), 1, "T8666.b side1 武装");
            CHECK_EQ(onstage_hp(b, 0), 99999 / 2, "T8666.b side0 倒地宠被复活半血");
            CHECK_EQ(ctx.on_stage[0], 0, "T8666.b side0 无补位（复活取消死切）");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 1, 0)), 1, "T8666.b side1 回调=1");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 0)), 0, "T8666.b side0 无回调");
            std::cout << "  [T8666.b] 536 镜像 通过\n";
        }
        {   // c) 全灭终局不回滚（D5 口径）：对手仅剩在场宠被击败 → 漏斗尾终局旗先立，
            //   drain 里复活照常结算（hp 拉回）但对局 FINISHED
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC15076, kM15076, 536, {}, {},
                                                  300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int slot = 1; slot <= 5; ++slot) {
                ctx.seerRobot[1].elfPets[slot].hp = 0;   // 对手替补全灭
            }
            set_onstage_hp(b, 1, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(b.state(), State::FINISHED, "T8666.c 全灭终局成立（终局不回滚）");
            CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 99999 / 2,
                     "T8666.c 复活 watcher 在终局前 drain 照常结算（重生之翼口径）");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 0)), 1, "T8666.c 回调=1");
            std::cout << "  [T8666.c] 536 全灭终局不回滚 通过\n";
        }
    }

    // ══ T8667 536 五载体切片机验 + 链兄弟共存 ══
    {
        {   // a) 五载体 arm + {0} 回显（[2]/[1]/[2]/[2]/[2]）
            const struct { int carrier; int monster; int effect; int denom; } rows[] = {
                {kC15076, kM15076, 536, 2}, {kC15971, kM15971, 536, 1}, {kC16215, kM16215, 536, 2},
                {kC16266, kM16266, 536, 2}, {kC31198, kM31198, 536, 2},
            };
            for (const auto& r : rows) {
                ScenarioBattle b(make_party(slice_pet(1, "甲", r.carrier, r.monster, r.effect, {},
                                                      {}, 300, 3000, 99999)),
                                 make_party(plain_pet(2, "敌", 100, 1, 99999)));
                b.start();
                BattleContext& ctx = b.ctx();
                run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
                CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 3)), 1, "T8667.a 武装旗");
                CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 5)), r.denom, "T8667.a {0} 回显");
            }
            std::cout << "  [T8667.a] 536 五载体切片回显 通过\n";
        }
        {   // b) 链兄弟共存（31198 全链 582 active）：R1 武装未击败（582 臂下2回合必暴）
            //   → R2 击败 → 复活照常（链兄弟不干扰 536 结算面）
            ScenarioBattle b(make_party(slice_pet(1, "甲", kC31198, kM31198, 536, {}, {},
                                                  300, 3000, 99999)),
                             make_party(plain_pet(2, "敌", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 3)), 1, "T8667.b R1 武装");
            set_onstage_hp(b, 1, 1);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(onstage_hp(b, 1), 99999 / 2, "T8667.b 链共存下复活照常");
            CHECK_EQ(stor(ctx, audit_key(kA536Base, 0, 0)), 1, "T8667.b 回调=1");
            std::cout << "  [T8667.b] 536 链兄弟共存（582 active） 通过\n";
        }
    }

    std::cout << "scenario_8660 全部通过\n";
    return 0;
}
