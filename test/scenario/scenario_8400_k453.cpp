// 场景 8400 —— K453 两族（2564 PP 吸取 / 2456 击败回满，moves_lib/k453_*.cpp）
//（2026-11-10，worktree ~/.agent-workspaces/workspaces/sbs-local/k453；号段 8400-8409）
//
// 断言口径：
//   - 2564 = pp_reduce 原语（全槽 -{0}、clamp≥0、pp=-1 无限槽跳过、逐槽 EVENT_PP_REDUCED）；
//     时序两序：先手方 SKILL_EFFECT 吸到对手"将用未耗"值（对手消耗在其 AFTER_ACTION）；
//     后手方 SKILL_EFFECT 读到对手已耗值。吸取=纯扣除无去向（09-23 用户裁定链，
//     见 k453_pp_drain.cpp 头注）。
//   - 2456 = per-pet on-stage 击败回满票：接下来 N 次击败**每次分别恢复、各耗 1 次**
//     （N=2 前两次均回满、第三次不触发；N=0 不武装）；换宠清 + ON_STAGE watcher
//     作废（建模选择非官方实测，见 k453_defeat_restore.cpp 头注）；
//     回满体走 heal_amount 缺口喂（HEAL_BLOCK 可封），回满 PP 走 pp_restore（不受封回血）。
//   - 恢复时序：伤害落地（ATTACK_DAMAGE）→ 击杀方 AFTER_ACTION 扣用技 PP →
//     FIRST_MOVER_DEATH 漏斗发击败事件 → drain 回满（全槽 PP 含用技槽回满）。
//   - 白盒审计槽：on_stage_storage 键 990245601(计数)/990245602(武装槽)/990245603(watcher)
//     —— 与 k453_defeat_restore.cpp 常量手工同步。
//   - 载体共效应中和：atk=3000 使封魔斩伤害≥300 → 1203/793（低于 300 门）不武装；
//     对手无强化/无回合类效果 → 889/948/1974 全 no-op。
//   - 击杀驱动 = 970 同款（foe NONE 不还手 + 事前把对手在场宠 hp 直改为 1）；
//     死切 = 023 同款（CHOOSE_AFTER_DEATH 提交 CHOOSE_PET）。
#include <algorithm>
#include <any>
#include <iostream>

#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;    // 拍打：物40 无副作用（探针攻击/击杀用）
constexpr int kDrainMove = 38505;   // 星光·能源吸收 [948,894,2564]|[19,250,2]
constexpr int kCarrierMove = 38488; // 封魔斩 [889,1203,793,2456]|[2,300,1,2,300,2,300,1]
constexpr int kCarrierPet = 4931;   // 炎龙铠甲（38488 宿主，override 构造用）

// 与 k453_defeat_restore.cpp 手工同步的白盒审计键
constexpr int kAuditCountKey  = 990245601;
constexpr int kAuditSlotKey   = 990245602;
constexpr int kAuditWatcherKey = 990245603;

int max_hp_of_pet(const ElfPet& p) {
    return p.numericalBase[NumericalPropertyIndex::HP];
}

int stor(const ElfPet& pet, int key) {
    const auto it = pet.on_stage_storage.find(key);
    return it == pet.on_stage_storage.end() ? 0
           : std::any_cast<int>(it->second);   // 场景白盒：键值只由本族写入
}

// make_pet 的带覆盖变体： carrier 槽用 Skills(id, monster, overrides) 构造。
ElfPet make_pet_with_overrides(int id, const std::string& name,
                               const official_data::MonsterRecord& monster,
                               int carrier_skill, int n_override /*<0 = 不覆盖*/,
                               int speed, int atk, int hp) {
    std::vector<EffectParamOverride> overrides;
    if (n_override >= 0) {
        overrides.push_back({carrier_skill, 2456, 0, n_override});
    }
    std::array<Skills, 5> skills{
        Skills(carrier_skill, monster, overrides),
        Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
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

// 2564 侧载体宠（38505 直载，DB 切片 arg=2）
ElfPet drain_pet(int id, const std::string& name, int speed, int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(kDrainMove), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
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
    return make_pet(id, name, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed, atk, hp);
}

// 把对手在场宠 hp 直改（击杀驱动前奏；970/023 同款白盒突变）
void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

// 把在场宠 hp/技能 PP 压损（制造"回满可观测缺口"）
void damage_pet(ElfPet& p, int hp, int pp_loss) {
    p.hp = hp;
    for (Skills& s : p.skills) {
        if (s.maxPP > 0 && s.pp >= 0) {
            s.pp = std::max(0, s.maxPP - pp_loss);
        }
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
    return side == 0 ? 1 : 1;
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
    const auto monster4931 = store.repository().load_monster(kCarrierPet);
    CHECK(static_cast<bool>(monster4931), "4931 monster record loads");

    // ══ T8400 2564 基础：先手吸取，对手全槽 -2、无限外不动、自身不受影响 ══
    {
        ScenarioBattle b(make_party(drain_pet(1, "吸方", 300, 1, 99999)),
                         make_party(plain_pet(2, "对手", 100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_max_pp = ctx.getPet(1).skills[0].maxPP;
        CHECK(foe_max_pp >= 3, "拍打 maxPP 足够放下 -2 断言");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(ctx.getPet(1).skills[i].pp, foe_max_pp - 2,
                     "T8400 对手全槽 -2（含未用槽）");
        }
        CHECK_EQ(ctx.getPet(0).skills[0].pp, ctx.getPet(0).skills[0].maxPP - 1,
                 "T8400 自身仅用技消耗 1（吸取不给自己）");
        for (int i = 1; i < 5; ++i) {
            CHECK_EQ(ctx.getPet(0).skills[i].pp, ctx.getPet(0).skills[i].maxPP,
                     "T8400 自身其余槽满");
        }
        std::cout << "  [T8400] 2564 全槽 -2 纯扣除 通过\n";
    }

    // ══ T8401 2564 clamp≥0 与无限 PP 槽跳过 ══
    {
        ScenarioBattle b(make_party(drain_pet(1, "吸方", 300, 1, 99999)),
                         make_party(plain_pet(2, "对手", 100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).skills[1].pp = 1;    // 不足 2 → 钳到 0 不为负
        ctx.getPet(1).skills[2].pp = -1;   // 无限 PP 槽 → 跳过
        const int foe_max_pp = ctx.getPet(1).skills[0].maxPP;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).skills[0].pp, foe_max_pp - 2, "T8401 满槽 -2");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8401 pp=1 钳到 0");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, -1, "T8401 无限 PP 槽不动");
        std::cout << "  [T8401] 2564 钳零/无限槽 通过\n";
    }

    // ══ T8402 2564 时序·先手：SKILL_EFFECT 已吸、对手消耗在其 AFTER_ACTION ══
    {
        ScenarioBattle b(make_party(drain_pet(1, "吸方", 300, 1, 99999)),
                         make_party(plain_pet(2, "对手", 100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_max_pp = ctx.getPet(1).skills[0].maxPP;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.ctx().debug_step_mode = true;   // 之后单步推进（观察窗）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK_EQ(ctx.getPet(1).skills[0].pp, foe_max_pp - 2,
                 "T8402 先手 SKILL_EFFECT 后：对手用技槽已 -2（未消耗）");
        b.ctx().debug_step_mode = false;
        b.drive();
        CHECK_EQ(ctx.getPet(1).skills[0].pp, foe_max_pp - 3,
                 "T8402 回合完成：对手用技槽 -2(吸)-1(耗)");
        std::cout << "  [T8402] 2564 先手时序 通过\n";
    }

    // ══ T8403 2564 时序·后手：对手 AFTER_ACTION 先扣、我方 SKILL_EFFECT 后吸 ══
    {
        ScenarioBattle b(make_party(drain_pet(1, "吸方", 100, 1, 99999)),
                         make_party(plain_pet(2, "对手", 300, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_max_pp = ctx.getPet(1).skills[0].maxPP;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.ctx().debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION);
        CHECK_EQ(ctx.getPet(1).skills[0].pp, foe_max_pp - 1,
                 "T8403 对手（先手）AFTER_ACTION 已耗 1、未被吸");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, foe_max_pp, "T8403 对手未用槽未动");
        b.ctx().debug_step_mode = false;
        b.drive();
        CHECK_EQ(ctx.getPet(1).skills[0].pp, foe_max_pp - 3, "T8403 末态：-1(耗)-2(吸)");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, foe_max_pp - 2, "T8403 末态：未用槽 -2");
        std::cout << "  [T8403] 2564 后手时序 通过\n";
    }

    // ══ T8404 2456 N=1：同回合击败 → 体/PP 全回满（含用技槽）══
    {
        ScenarioBattle b(make_party(make_pet_with_overrides(1, "铠甲方", *monster4931,
                                                            kCarrierMove, -1, 300, 3000, 99999)),
                         make_party(plain_pet(2, "对手", 100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        damage_pet(ctx.getPet(0), 300, 4);           // 制造回满缺口
        CHECK(ctx.getPet(0).skills[2].pp < ctx.getPet(0).skills[2].maxPP, "前置：PP 有缺口");
        set_onstage_hp(b, 1, 1);                     // 击杀驱动
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, max_hp_of_pet(ctx.getPet(0)),
                 "T8404 击败后体力回满");
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(ctx.getPet(0).skills[i].pp, ctx.getPet(0).skills[i].maxPP,
                     "T8404 击败后 PP 全槽回满（含用技槽——恢复在消耗后）");
        }
        CHECK_EQ(stor(ctx.getPet(0), kAuditCountKey), 0, "T8404 N=1 消耗到 0");
        CHECK_EQ(stor(ctx.getPet(0), kAuditSlotKey), 0, "T8404 武装槽=0 号位");
        std::cout << "  [T8404] 2456 同回合击败回满 通过\n";
    }

    // ══ T8405 2456 负例：未武装击败不恢复；武装但未击败不恢复 ══
    {
        {   // 未武装（拍打杀）：不恢复
            ScenarioBattle b(make_party(make_pet_with_overrides(1, "普攻方", *monster4931,
                                                                kCarrierMove, -1, 300, 3000, 99999)),
                             make_party(plain_pet(2, "对手", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            damage_pet(ctx.getPet(0), 300, 4);
            set_onstage_hp(b, 1, 1);
            // 用 1 号拍打杀（38488 不出手 → 不武装）
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.getPet(0).hp, 300, "T8405.a 未武装击败：不恢复");
            CHECK_EQ(stor(ctx.getPet(0), kAuditCountKey), 0, "T8405.a 无武装票");
        }
        {   // 武装但未击败（对手 99999 吃 4800）：不恢复
            ScenarioBattle b(make_party(make_pet_with_overrides(1, "铠甲方", *monster4931,
                                                                kCarrierMove, -1, 300, 3000, 99999)),
                             make_party(plain_pet(2, "对手", 100, 1, 99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            damage_pet(ctx.getPet(0), 300, 4);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK(ctx.getPet(1).hp > 0, "T8405.b 前置：对手存活");
            CHECK_EQ(ctx.getPet(0).hp, 300, "T8405.b 武装未击败：不恢复");
            CHECK_EQ(stor(ctx.getPet(0), kAuditCountKey), 1, "T8405.b 武装票在（未消耗）");
            CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 1,
                     "T8405.b watcher 挂 1");
        }
        std::cout << "  [T8405] 2456 未武装/未击败负例 通过\n";
    }

    // ══ T8406 2456 N=2/N=0：每次击败分别恢复各耗 1 次；第三次不触发；N=0 不武装 ══
    {
        ScenarioBattle b(make_party(make_pet_with_overrides(1, "铠甲方", *monster4931,
                                                            kCarrierMove, 2, 300, 3000, 99999)),
                         make_party(plain_pet(2, "对手", 100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1 武装（不杀）：38488 出手、对手 NONE、对手满血存活
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(stor(ctx.getPet(0), kAuditCountKey), 2, "T8406 N=2 武装");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 1,
                 "T8406 watcher 挂 1");
        // R2 第一次击败 → 恢复 #1
        damage_pet(ctx.getPet(0), 300, 4);
        set_onstage_hp(b, 1, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, max_hp_of_pet(ctx.getPet(0)), "T8406 R2 第一次击败回满");
        CHECK_EQ(ctx.getPet(0).skills[2].pp, ctx.getPet(0).skills[2].maxPP, "T8406 R2 PP 回满");
        CHECK_EQ(stor(ctx.getPet(0), kAuditCountKey), 1, "T8406 R2 计数 2→1");
        // R3 第二次击败 → 恢复 #2（每次分别恢复）
        damage_pet(ctx.getPet(0), 300, 4);
        set_onstage_hp(b, 1, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, max_hp_of_pet(ctx.getPet(0)), "T8406 R3 第二次击败也回满");
        CHECK_EQ(stor(ctx.getPet(0), kAuditCountKey), 0, "T8406 R3 计数 1→0");
        // R4 第三次击败 → 不触发
        damage_pet(ctx.getPet(0), 300, 4);
        set_onstage_hp(b, 1, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, 300, "T8406 R4 第三次击败：不恢复");
        CHECK(ctx.getPet(0).skills[2].pp < ctx.getPet(0).skills[2].maxPP,
              "T8406 R4 PP 也不恢复");
        CHECK_EQ(stor(ctx.getPet(0), kAuditCountKey), 0, "T8406 R4 计数保持 0");
        std::cout << "  [T8406] 2456 N=2 逐次恢复/第三次不触发 通过\n";
    }
    {   // N=0：不武装
        ScenarioBattle b(make_party(make_pet_with_overrides(1, "铠甲方", *monster4931,
                                                            kCarrierMove, 0, 300, 3000, 99999)),
                         make_party(plain_pet(2, "对手", 100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        damage_pet(ctx.getPet(0), 300, 4);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(stor(ctx.getPet(0), kAuditCountKey), 0, "T8406b N=0 不武装");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 0,
                 "T8406b 无 watcher");
        set_onstage_hp(b, 1, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, 300, "T8406b N=0 击败不恢复");
        std::cout << "  [T8406b] 2456 N=0 不武装 通过\n";
    }

    // ══ T8407 2456 换宠：下场清票；队友击败不串；回场不复活；重施放可重武装 ══
    {
        ElfPet a = make_pet_with_overrides(11, "铠甲A", *monster4931, kCarrierMove, -1,
                                           300, 3000, 99999);
        ElfPet c = make_pet_with_overrides(12, "铠甲B", *monster4931, kCarrierMove, -1,
                                           300, 3000, 99999);
        std::array<ElfPet, 6> party{a, c, a, a, a, a};
        ScenarioBattle b(party, make_party(plain_pet(2, "对手", 100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        damage_pet(ctx.getPet(0), 300, 4);           // A 压损
        // R1：A 武装（不杀）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(stor(ctx.getPet(0), kAuditCountKey), 1, "T8407 A 武装");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 1,
                 "T8407 A watcher 在");
        // R2：A 主动换下（B 上）→ 票清、watcher 作废
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.on_stage[0], 1, "T8407 B 登场");
        CHECK_EQ(stor(ctx.seerRobot[0].elfPets[0], kAuditCountKey), 0,
                 "T8407 A 下场即清票");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 0,
                 "T8407 A watcher 随下场作废");
        // R3：B（队友）用拍打（1 号槽，不武装）击败 → A 不恢复、A 票仍空（换宠不串权）
        damage_pet(ctx.getPet(0), 300, 4);
        set_onstage_hp(b, 1, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, 300, "T8407 队友击败不消耗 A 的票（票已清）");
        CHECK_EQ(stor(ctx.seerRobot[0].elfPets[0], kAuditCountKey), 0, "T8407 A 票保持空");
        // R4：A 换回、不再施放 → 不复活旧票；A 用拍打击败 → 不恢复
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.on_stage[0], 0, "T8407 A 回场");
        CHECK_EQ(stor(ctx.getPet(0), kAuditCountKey), 0, "T8407 回场不复活旧票");
        damage_pet(ctx.getPet(0), 300, 4);
        set_onstage_hp(b, 1, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, 300, "T8407 回场未施放：击败不恢复");
        // R5：A 重施放 38488（本回合直接击败 hp=1 对手）→ 新票武装并当场触发
        set_onstage_hp(b, 1, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, max_hp_of_pet(ctx.getPet(0)),
                 "T8407 重施放武装 + 当回合击败触发恢复");
        std::cout << "  [T8407] 2456 换宠清/队友不串/回场不复活/重施放重武装 通过\n";
    }

    // ══ T8408 2456 镜像：双边武装互不干扰，各只响应自己的击败 ══
    {
        ScenarioBattle b(make_party(make_pet_with_overrides(1, "铠甲0", *monster4931,
                                                            kCarrierMove, -1, 300, 3000, 99999)),
                         make_party(make_pet_with_overrides(2, "铠甲1", *monster4931,
                                                            kCarrierMove, -1, 100, 3000, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        damage_pet(ctx.getPet(0), 50000, 4);
        damage_pet(ctx.getPet(1), 50000, 4);
        // R1：双边各自武装（互不击败——双方都 99999 满血吃 4800）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(stor(ctx.getPet(0), kAuditCountKey), 1, "T8408 双方武装（己方）");
        CHECK_EQ(stor(ctx.getPet(1), kAuditCountKey), 1, "T8408 双方武装（对手）");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 2,
                 "T8408 watcher×2");
        // R2：己方击败对手 → 己方恢复、对手票随死 irrelevant
        set_onstage_hp(b, 1, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, max_hp_of_pet(ctx.getPet(0)), "T8408 己方击败→己方回满");
        // R3：对手替补武装 → 己方 NONE
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(stor(ctx.getPet(1), kAuditCountKey), 1, "T8408 对手替补武装");
        // R4：对手击败己方在场宠 → 对手回满（镜像对称；先给替补压缺口）
        damage_pet(ctx.getPet(1), 50000, 4);
        set_onstage_hp(b, 0, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.getPet(1).hp, max_hp_of_pet(ctx.getPet(1)),
                 "T8408 对手击败→对手回满");
        std::cout << "  [T8408] 2456 镜像双边武装 通过\n";
    }

    // ══ T8409 2456 封回血：体力被封、PP 照回满 ══
    {
        ScenarioBattle b(make_party(make_pet_with_overrides(1, "铠甲方", *monster4931,
                                                            kCarrierMove, -1, 300, 3000, 99999)),
                         make_party(plain_pet(2, "对手", 100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        damage_pet(ctx.getPet(0), 300, 4);
        ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(), /*duration_rounds=*/0,
                           /*counts=*/0, /*source_id=*/8401);
        set_onstage_hp(b, 1, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, 300, "T8409 封回血：体力不恢复");
        CHECK_EQ(ctx.getPet(0).skills[2].pp, ctx.getPet(0).skills[2].maxPP,
                 "T8409 PP 不受封回血：照回满");
        CHECK_EQ(ctx.getPet(0).skills[4].pp, ctx.getPet(0).skills[4].maxPP,
                 "T8409 PP 全槽回满");
        std::cout << "  [T8409] 2456 封回血但 PP 照恢复 通过\n";
    }

    // ══ T8410 2456 同归保护正例：真实管线同归（反射真伤击倒武装宠）——验收补点1 ══
    // 内核防护链（2026-11-10 验收补点，本腿为回归锁）：
    //   ① 同归保护（battleFsm.cpp funnel_on_stage_deaths 开头）：进死亡时点双方在场
    //      都 ≤0 时，先打倒对手的一方（ws.first_to_exhaust_foe，deal_damage 落血
    //      mark_first_exhaust_flag 写入）被直写保留 1 点体力——武装宠不会以 hp=0
    //      未登记状态进 drain；
    //   ② 本腿反射发生于死亡漏斗之前，因此击败事件投递时武装宠已经受到同归保护。
    //      这不证明 drain 过程中的亡语同波窗口安全：DEATH 回调仍可在漏斗结束后
    //      击倒击败方，造成 hp=0 且未登记；该独立负例及显式存活门由 T8413 锁定。
    // 官方名词解释「击败」= 未死亡的精灵于击败时点触发 → 反射真伤同归时武装宠被
    //   内核流程保护在 1 血存活、击败成立，恢复对其生效是正确语义（本腿锁定）。
    {
        // 对手反射桩：受到任何伤害 → 对攻击方回敬 99999 真伤（deal_true_damage 原语，
        // 与 1737"粉转真"同通道——真实管线，非手 emit 伪造击败）
        ScenarioBattle b(make_party(make_pet_with_overrides(1, "铠甲方", *monster4931,
                                                            kCarrierMove, -1, 300, 3000, 99999)),
                         make_party(plain_pet(2, "对手", 100, 1, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        damage_pet(ctx.getPet(0), 300, 4);   // A 压损（反射必杀）
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
        set_onstage_hp(b, 1, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.getPet(0).hp > 0, "T8410 前置：武装宠被内核同归保护存活");
        CHECK_EQ(ctx.getPet(0).hp, max_hp_of_pet(ctx.getPet(0)),
                 "T8410 同归存活者击败成立 → 恢复生效（hp=1→满）");
        CHECK_EQ(ctx.getPet(0).skills[2].pp, ctx.getPet(0).skills[2].maxPP,
                 "T8410 PP 恢复生效");
        CHECK_EQ(stor(ctx.getPet(0), kAuditCountKey), 0, "T8410 票正常消耗 1 次");
        std::cout << "  [T8410] 2456 同归保护正例（内核防护链回归锁） 通过\n";
    }

    // ══ T8411 2564 EVENT_PP_REDUCED 事件断言（验收补点2）══
    // 载荷（battle_primitives.cpp:1892-1895）：actor=-1、target=PP 变化方、
    //   amount=实际减少量、slot=槽位；仅 skill.pp != before 才发（pp=0 槽不发）。
    {
        ScenarioBattle b(make_party(drain_pet(1, "吸方", 300, 1, 99999)),
                         make_party(plain_pet(2, "对手", 100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).skills[1].pp = 1;    // 只扣 1（clamp）
        ctx.getPet(1).skills[2].pp = 2;    // 恰归零
        ctx.getPet(1).skills[3].pp = 0;    // 无变化 → 不发事件
        ctx.getPet(1).skills[4].pp = -1;   // 无限 PP → 跳过不发
        struct PpEv { int target; int amount; int slot; };
        std::vector<PpEv> events;
        ctx.event_center_.register_watcher(
            EventType::EVENT_PP_REDUCED, 0, ctx.roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [&events](BattleContext* c, const BattleEvent& ev) {
                events.push_back({ev.target, ev.amount, ev.slot});
                (void)c;
            },
            WatcherScope::TEAM, ctx.watcher_valid_id[0]);
        const int foe_max_pp = ctx.getPet(1).skills[0].maxPP;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(events.size(), static_cast<std::size_t>(3),
                 "T8411 事件数=3（slot0/1/2 各一；pp=0 与无限槽不发）");
        if (events.size() == 3) {
            CHECK_EQ(events[0].target, 1, "T8411 target=PP 变化方");
            CHECK_EQ(events[0].slot, 0, "T8411 slot0");
            CHECK_EQ(events[0].amount, 2, "T8411 slot0 满槽 -2");
            CHECK_EQ(events[1].slot, 1, "T8411 slot1");
            CHECK_EQ(events[1].amount, 1, "T8411 slot1 pp=1 只扣 1");
            CHECK_EQ(events[2].slot, 2, "T8411 slot2");
            CHECK_EQ(events[2].amount, 2, "T8411 slot2 pp=2 恰归零 amount=2");
        }
        CHECK_EQ(ctx.getPet(1).skills[0].pp, foe_max_pp - 2, "T8411 slot0 数值");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8411 slot1 钳零");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0, "T8411 slot2 恰归零");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, 0, "T8411 slot3 原为 0 不动");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, -1, "T8411 slot4 无限不动");
        std::cout << "  [T8411] 2564 EVENT_PP_REDUCED 逐槽事件 通过\n";
    }

    // ══ T8412 2456 同场连续重施放：watch_count 保持 1、一次击败只耗一次（验收补点3）══
    // 补 T8407 未覆盖的"同一次上场连投两次"路径（T8407 的重施放隔了下场/回场）。
    {
        ScenarioBattle b(make_party(make_pet_with_overrides(1, "铠甲方", *monster4931,
                                                            kCarrierMove, 2, 300, 3000, 99999)),
                         make_party(plain_pet(2, "对手", 100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1 第一次施放（武装 W1，不杀）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 1,
                 "T8412 R1 后 watcher=1");
        // R2 同场连投第二次：旧 watcher 摘除 + 新注册 → 总数仍 1（防每杀双恢复）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_OPPONENT_DEFEATED), 1,
                 "T8412 R2 连投后仍=1（remove_watcher 换绑）");
        CHECK_EQ(stor(ctx.getPet(0), kAuditCountKey), 2, "T8412 计数刷新为 N=2");
        // R3 击败一次：只耗一次（2→1）、恢复一次
        damage_pet(ctx.getPet(0), 300, 4);
        set_onstage_hp(b, 1, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, max_hp_of_pet(ctx.getPet(0)), "T8412 击败恢复生效");
        CHECK_EQ(stor(ctx.getPet(0), kAuditCountKey), 1,
                 "T8412 一次击败只耗一次（2→1 非双 watcher 连扣到 0）");
        std::cout << "  [T8412] 2456 同场连续重施放 通过\n";
    }

    // ══ T8413 2456 亡语窗口负例：对手 EVENT_DEATH 亡语在 drain 同波击倒击败方 ══
    // 与 T8410 的区别（T8410 反射挂 TAKE_DAMAGE，发生在漏斗之前，同归保护可救）：
    //   defeat_pet 把 DEATH 与 OPPONENT_DEFEATED **相继放同波**（battle_primitives.cpp
    //   :2175/:2187）→ EventCenter FIFO 先执行 DEATH 回调（亡语用合法 deal_true_damage
    //   击倒已武装的击败方）→ 同波紧随的 OPPONENT_DEFEATED 回调触发恢复时，
    //   击败方 hp=0 且 pet_death_notified=false（死亡漏斗已结束、BATTLE_AFTER_DEFEATED
    //   sync 尚未跑）→ heal_impl:1477 的登记门拦不住 → 未加存活门时"奶活倒下者"。
    // 观测：事件序（DEATH 先于 OPPONENT_DEFEATED）+ OPPONENT_DEFEATED 投递时点的
    //   hp/notified 快照；击倒后 hp 保持 0、PP 不恢复、票不消费（死亡切换前审计）。
    {
        ScenarioBattle b(make_party(make_pet_with_overrides(1, "铠甲方", *monster4931,
                                                            kCarrierMove, -1, 300, 3000, 99999)),
                         make_party(plain_pet(2, "对手", 100, 1, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        damage_pet(ctx.getPet(0), 300, 4);   // A 压损（亡语必杀）
        struct Obs { int type; int hp0; bool notified0; };   // type: 0=DEATH 1=OPP_DEFEATED
        std::vector<Obs> timeline;
        auto record = [&timeline, &ctx](int type) {
            timeline.push_back({type, ctx.getPet(0).hp,
                                ctx.pet_death_notified[0][ctx.on_stage[0]]});
        };
        // 亡语：对手死亡时（ev.target==1）用合法原语击倒击败方（once，真漏斗触发不手 emit）
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
        // 观测器（DEATH + OPPONENT_DEFEATED 各记一笔；先于 2456 watcher 注册→先投递）
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
        const int pp_gap_slot2 = ctx.getPet(0).skills[2].maxPP - 4;   // 压损后基准
        set_onstage_hp(b, 1, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        // 死切前断言（击倒方已亡、票未被消费、恢复未发生）
        CHECK_EQ(timeline.size(), static_cast<std::size_t>(2),
                 "T8413 事件序：DEATH(对手) 与 OPPONENT_DEFEATED 各投递一次");
        CHECK_EQ(timeline[0].type, 0, "T8413 DEATH 先投递");
        CHECK_EQ(timeline[1].type, 1, "T8413 OPPONENT_DEFEATED 同波紧随");
        CHECK_EQ(timeline[1].hp0, 0,
                 "T8413 窗口证据：OPPONENT_DEFEATED 投递时击败方 hp=0");
        CHECK(!timeline[1].notified0,
              "T8413 窗口证据：该时点击败方未登记（heal_impl 登记门拦不住）");
        CHECK_EQ(ctx.getPet(0).hp, 0,
                 "T8413 存活门：亡语击倒者不回血（保持 0，死亡切换前）");
        CHECK_EQ(ctx.getPet(0).skills[2].pp, pp_gap_slot2,
                 "T8413 存活门：PP 不恢复");
        CHECK_EQ(stor(ctx.seerRobot[0].elfPets[0], kAuditCountKey), 1,
                 "T8413 存活门：票不消费（死亡切换前审计）");
        // 双方死切收尾（亡语把击败方也打倒了：先 0 号方后 1 号方）
        while (b.state() == State::CHOOSE_AFTER_DEATH) {
            const int dead = ctx.getPet(0).hp <= 0 ? 0 : 1;
            b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET,
                               next_replacement_slot(b, dead));
            b.drive();
        }
        std::cout << "  [T8413] 2456 亡语窗口负例（存活门回归锁） 通过\n";
    }

    std::cout << "scenario_8400 全部通过\n";
    return 0;
}
