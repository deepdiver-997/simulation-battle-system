// 场景 8750 —— K488 必Miss窗 + 自爆接续回血 两族（1125/618，
// resources/moves_lib/k488_mustmiss_relay_heal.cpp；2026-11-23，worktree k488；
// 号段 8750-8759 → T8750-pre + a-i 十块）
//
// 断言口径（三层观测面 + 真实载体正负测齐）：
//   ① 键面（plugin_storage 审计槽）：1125 基 1011250000（slot0={0}回显/
//      1={1}回显/2=施放计数/3=dodge写入计数/4=命中触发计数/5=末次命中触发回合/
//      6=封票授予计数/7=封票授予回合/8={1}排程计数/9=跳过原因）；
//      618 基 1006180000（slot0={0}回显/1={1}回显/2=自爆施放计数/3=接续状态/
//      4=接续消费回合/5=heal tick 计数/6=累计实得回复量/7=跳过原因/8=末次 heal
//      回合/10=voucher旗/11=watcher id）。
//   ② 通道面：ws.dodge_rate 停泊直读（drive 后 ws 仍持有刚结束回合的值）+
//      hp 精确差分（618 heal=points 逐次、满血钳零 slot6 停账）+ 能力等级
//      （鸣叫 20002 effect5 对手攻-1：封=不变 / 落地=-1）。
//   ③ 行为面：miss/hit（PP 正控制——技能使用即耗 PP，miss/无效也耗）+ 死亡
//      登记/换宠面 + 接续窗逐回合 heal + 离场清理（换宠 epoch 作废面）。
//
// 确定性手法（SOP §五.4 + K471 同款）：
//   · 载体招**原生 must_hit=1**（unity 库现查 25338/23481/28148/29140/36460
//     全表 must_hit=1）——施放必中免掷（miss 不结算效果）；⚠️ 引擎事实：必中
//     凭证绕过 dodge 面（skills.cpp ①命中判定顺序不可换）→ **必中属性技施放
//     能穿过必Miss窗**（矛盾分支的施放侧表现；镜像腿因此对称成立）。
//   · 对手拍打 acc100/must_hit=0：dodge=1.0 → hit_chance=0 → 恒 miss（确定性，
//     不依赖 rand 序）；dodge=0 → hit_chance=100 → rand()%100>=100 恒假恒中
//     （同样确定性）——miss/hit 断言全程免 rand。
//   · 对手拍打 must_hit=1（矛盾分支腿）：凭证绕 dodge 面 → 必中穿透窗（"若
//     对手命中"的引擎面解：必Miss 治不了必中）。
//   · 每块 sbs_srand 定标（虽然断言面免 rand，伤害带 85%~100% 浮动仍受 seed
//     影响——带断言按 (87,102)@atk300 锚外扩）。
//   · 坑集 §4：满血构造 + set_onstage_hp 单独压当前 hp；接续宠 damaged 态
//     入场（ElfPet initial_hp < HP 属性——heal 才有实得量）。
//   · 618 对手全 NONE（行动面静默——heal 精确账不被攻击污染）。
//   · fullstate 显示断言（G2 义务）：1125 施放腿断 skillEffects 桶 effectId:1125、
//     封属腿断 ruleTickets sourceEffectId:1125、618 消费腿断 effectId:618。
#include <any>
#include <array>
#include <iostream>
#include <utility>
#include <vector>

#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;        // 拍打：物40 acc100 must_hit=0（攻击面/填充）
constexpr int kMingJiao = 20002;     // 鸣叫 [5]=[0,100,-1]：cat4 属性技，对手攻-1（封属观测面）
constexpr int kC25338 = 25338;       // 1125 [3,2] 寰宇纵横（主载体，链 894/470 已实现且不扰断言面）
constexpr int kC29140 = 29140;       // 1125 [2,2] 临时改良（{0}=2 差分载体；链 48/1814 剥离）
constexpr int kC23481 = 23481;       // 618 [3,160] 仁德天下（**纯链**主载体）
constexpr int kC28148 = 28148;       // 618 [4,300] 群蜂之凋（**纯链**差分载体）
constexpr int kC36460 = 36460;       // 618 [5,350] 片叶之佑（**纯链**cat2 攻击技载体）
constexpr int kM3311 = 3311;         // 25338 learner 圣光灵神
constexpr int kM4813 = 4813;         // 29140 learner 夏莉莉
constexpr int kM2512 = 2512;         // 23481 learner 昭烈帝刘备
constexpr int kM4446 = 4446;         // 28148/36460 learner 露泽莫尔
constexpr int kMaxHp = 90000;

constexpr int k1125Base = 1011250000;  // + owner*16 + slot
constexpr int k618Base = 1006180000;   // + owner*16 + slot

int audit_int(BattleContext& ctx, int key, int def) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : def;
}
int s1125(BattleContext& ctx, int owner, int slot) {
    return audit_int(ctx, k1125Base + owner * 16 + slot, -1);
}
int s618(BattleContext& ctx, int owner, int slot) {
    return audit_int(ctx, k618Base + owner * 16 + slot, -1);
}

// 1125 载体宠：首槽=载体（原生 must_hit=1），其余拍打；crit0。
ElfPet caster1125_pet(int id, const char* name, int carrier, int monster_id,
                      const std::vector<std::pair<int, int>>& arg_overrides,
                      const std::vector<int>& disabled_effects, int speed, int atk) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(monster_id);
    if (!monster) {
        std::cerr << "caster1125_pet: monster " << monster_id << " load failed" << std::endl;
        std::abort();
    }
    std::vector<EffectParamOverride> overrides;
    for (const auto& [idx, value] : arg_overrides) {
        overrides.push_back({carrier, 1125, idx, value});
    }
    DisabledEffects disabled;
    disabled.skill_effects = disabled_effects;
    std::array<Skills, 5> skills{Skills(carrier, *monster, overrides, disabled), Skills(kPaiDa),
                                 Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa)};
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 0;   // 填充拍打非必中（镜像互打腿要吃 dodge 面）
    }
    skills[0].must_hit = 1;   // 载体施放必中（原生亦 1；统一钉死——miss 不结算效果）
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = kMaxHp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{}, CommonTrait{},
                  base, /*initial_hp=*/kMaxHp, /*shield=*/0, /*cover=*/0, /*is_locked=*/false,
                  std::move(skills));
}

// 618 载体宠：首槽=载体（纯链），其余拍打（自爆后不再行动）。
ElfPet caster618_pet(int id, const char* name, int carrier, int monster_id, int speed) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(monster_id);
    if (!monster) {
        std::cerr << "caster618_pet: monster " << monster_id << " load failed" << std::endl;
        std::abort();
    }
    std::array<Skills, 5> skills{Skills(carrier, *monster, {}, DisabledEffects{}), Skills(kPaiDa),
                                 Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa)};
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = 100;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = 100;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = kMaxHp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{}, CommonTrait{},
                  base, /*initial_hp=*/kMaxHp, /*shield=*/0, /*cover=*/0, /*is_locked=*/false,
                  std::move(skills));
}

// 接续替补宠：damaged 态入场（initial < max）——heal 才有实得量。
ElfPet relay_pet(int id, const char* name, int speed, int current_hp) {
    std::array<Skills, 5> skills{Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
                                 Skills(kPaiDa)};
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = 100;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = 100;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = kMaxHp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{}, CommonTrait{},
                  base, /*initial_hp=*/current_hp, /*shield=*/0, /*cover=*/0, /*is_locked=*/false,
                  std::move(skills));
}

// 攻击手：acc/must_hit 可调——
//   acc100+must_hit=0：dodge=1.0 → hit_chance=0 恒 miss；dodge=0 → 恒中（免 rand 双档）。
//   acc100+must_hit=1：凭证绕 dodge 面 → 必中穿透（矛盾分支腿）。
//   双技能表：slot0=拍打（攻击面）、slot1=鸣叫（属性技/封属观测面）。
ElfPet attacker_pet(int id, const char* name, int speed, int acc_v, int must_hit_v) {
    std::array<Skills, 5> skills{Skills(kPaiDa), Skills(kMingJiao), Skills(kPaiDa), Skills(kPaiDa),
                                 Skills(kPaiDa)};
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = acc_v;
        s.must_hit = must_hit_v;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = 300;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = 300;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = kMaxHp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{}, CommonTrait{},
                  base, /*initial_hp=*/kMaxHp, /*shield=*/0, /*cover=*/0, /*is_locked=*/false,
                  std::move(skills));
}

// 混编队伍（前 N 只依序、其余以末只复制占位——6 格）。
std::array<ElfPet, 6> party_mixed(const std::vector<ElfPet>& pets) {
    std::array<ElfPet, 6> arr{pets[0], pets[0], pets[0], pets[0], pets[0], pets[0]};
    for (std::size_t i = 0; i < pets.size() && i < 6; ++i) {
        arr[i] = pets[i];
    }
    return arr;
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}
int onstage_hp(ScenarioBattle& b, int side) {
    return b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp;
}
int onstage_slot_pp(ScenarioBattle& b, int side, int idx) {
    return b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].skills[idx].pp;
}
void refill_pp(ScenarioBattle& b, int side) {
    ElfPet& pet = b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]];
    for (Skills& s : pet.skills) {
        if (s.pp >= 0) {
            s.pp = s.maxPP;
        }
    }
}
bool onstage_dead_registered(ScenarioBattle& b, int side) {
    const int slot = b.ctx().on_stage[side];
    return b.ctx().seerRobot[side].elfPets[slot].hp <= 0
        && b.ctx().pet_death_notified[side][slot];
}

void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0, BattleFsm::ActionType a1,
               int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        int slot = 1;
        for (int i = 0; i < 6; ++i) {
            if (i != b.ctx().on_stage[dead] && b.ctx().seerRobot[dead].elfPets[i].hp > 0) {
                slot = i;
                break;
            }
        }
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET, slot);
        b.drive();
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8750-pre 10 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>>
            expect = {
                // ── 1125 五载体（{0}∈{2,3}×{1}=2）──
                {25338, {{894, {200}}, {470, {3, 70, 7}}, {1125, {3, 2}}}},
                {28288, {{191, {4}}, {1125, {3, 2}}, {1475, {4, 50}}, {1693, {4, 3, 300}}}},
                {29140, {{48, {4}}, {1125, {2, 2}}, {1814, {2, 50, 0, 3}}}},
                {29143, {{1932, {4, 16}}, {2196, {100, 16}}, {522, {3, 400}}, {1125, {3, 2}}}},
                {38091, {{948, {15}}, {2084, {2}}, {1125, {2, 2}}}},
                // ── 618 五载体（{0}∈{3,4,5}×{1}∈{160,300,350}）──
                {23481, {{618, {3, 160}}}},
                {27334, {{5, {5, 100, -2}}, {5, {4, 100, -1}}, {618, {3, 300}}}},
                {28148, {{618, {4, 300}}}},
                {36460, {{618, {5, 350}}}},
                {37325, {{618, {3, 300}}, {1319, {1, 1, 1, 1, 1, 1}}}},
            };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8750-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8750-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8750-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8750-pre] 10 载体实际加载器切片核验 通过\n";
    }

    // ══ T8750.a 1125 必Miss窗主链（25338 [3,2] 全链）══
    //    rc0 施放（先手）→ 窗 rc0..rc2；对手拍打 rc0/1/2 恒 miss（PP 正控制）；
    //    rc3 窗过期 → 落地。无必中穿透 → 全程零命中触发（slot4==0）。
    {
        sbs_srand(8750);
        ScenarioBattle b(party_mixed({caster1125_pet(1, "印主", kC25338, kM3311, {}, {}, 300, 100)}),
                         party_mixed({attacker_pet(2, "攻击手", 200, 100, 0)}));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：施放 + 对手拍打 → miss
        CHECK_EQ(s1125(ctx, 0, 0), 3, "T8750.a {0}=3 行参回显");
        CHECK_EQ(s1125(ctx, 0, 1), 2, "T8750.a {1}=2 行参回显");
        CHECK_EQ(s1125(ctx, 0, 2), 1, "T8750.a 施放计数=1");
        CHECK_EQ(s1125(ctx, 0, 3), 1, "T8750.a rc0 dodge 写入=1（立即写）");
        CHECK_EQ(s1125(ctx, 0, 4), 0, "T8750.a rc0 零命中触发（必Miss 窗正测）");
        CHECK_EQ(ctx.ws.dodge_rate[0], 1.0f, "T8750.a 停泊直读 dodge_rate=1.0");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8750.a rc0 印主满血（miss 不落伤；894 吸取是己方回满钳零）");
        CHECK_EQ(onstage_slot_pp(b, 1, 0), attacker_pet(2, "x", 1, 100, 0).skills[0].maxPP - 1,
                 "T8750.a rc0 对手攻击确实发生（PP 正控制）");
        CHECK(b.dump().find("\"effectId\":1125") != std::string::npos,
              "T8750.a fullstate 含 1125 桶条目（G2 显示面：回合桶驻留态）");

        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(s1125(ctx, 0, 3), 2, "T8750.a rc1 tick 写入=2");
        CHECK_EQ(ctx.ws.dodge_rate[0], 1.0f, "T8750.a rc1 tick 维持 dodge_rate=1.0");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8750.a rc1 仍 miss（窗内）");

        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(s1125(ctx, 0, 3), 3, "T8750.a rc2 tick 写入=3（窗末回合）");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8750.a rc2 仍 miss（窗 rc0..rc2 末格）");

        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(s1125(ctx, 0, 3), 3, "T8750.a rc3 无第 4 次写入（窗过期负测）");
        CHECK_EQ(ctx.ws.dodge_rate[0], 0.0f, "T8750.a rc3 dodge_rate 归零（ws reset）");
        CHECK(onstage_hp(b, 0) < kMaxHp, "T8750.a rc3 攻击落地（窗过期）");
        CHECK_EQ(s1125(ctx, 0, 4), 0, "T8750.a 全程零命中触发（无必中穿透）");
        std::cout << "  [T8750.a] 1125 必Miss窗主链（窗正负） 通过\n";
    }

    // ══ T8750.b 1125 矛盾分支：必中穿透 → 下{1}回合封属 ══
    //    rc0 必中拍打穿透窗落地（"若对手命中"引擎面解）→ 命中触发+排程；
    //    rc1 ROUND_START 实授 SEAL_ATTRIBUTE{1}=2 → rc1/rc2 鸣叫被无效（等级
    //    不变）；rc3 封过期 → 鸣叫落地（攻-1）。排程≠实授双计数差分锁"下{1}
    //    回合"起点（即时化变异面：rc2 仍封）。
    {
        sbs_srand(8751);
        ScenarioBattle b(party_mixed({caster1125_pet(1, "印主", kC25338, kM3311, {}, {}, 300, 100)}),
                         party_mixed({attacker_pet(2, "攻击手", 200, 100, 1)}));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：施放 + 必中拍打穿透
        CHECK(onstage_hp(b, 0) < kMaxHp, "T8750.b rc0 必中拍打穿透必Miss窗落地（矛盾分支实测）");
        CHECK_EQ(s1125(ctx, 0, 4), 1, "T8750.b 命中触发计数=1");
        CHECK_EQ(s1125(ctx, 0, 5), 0, "T8750.b 触发回合=rc0");
        CHECK_EQ(s1125(ctx, 0, 8), 1, "T8750.b {1} 排程计数=1");
        CHECK_EQ(s1125(ctx, 0, 6), 0, "T8750.b rc0 未实授（下{1}回合才封——排程差分）");

        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(s1125(ctx, 0, 6), 1, "T8750.b rc1 实授计数=1（ROUND_START 排程兑现）");
        CHECK_EQ(s1125(ctx, 0, 7), 1, "T8750.b 实授回合=rc1");
        CHECK_EQ(ctx.ability_levels[0][0], 0,
                 "T8750.b rc1 鸣叫被 SEAL_ATTRIBUTE 拦下（印主攻不变）");
        CHECK_EQ(onstage_slot_pp(b, 1, 1), attacker_pet(2, "x", 1, 100, 1).skills[1].maxPP - 1,
                 "T8750.b rc1 鸣叫确实使用（PP 正控制——无效也耗）");
        CHECK(b.dump().find("\"sourceEffectId\":1125") != std::string::npos,
              "T8750.b fullstate 含 1125 封属票（G2 显示面：ruleTickets 驻留态）");

        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8750.b rc2 鸣叫仍被封（{1}=2 → rc1..rc2）");

        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[0][0], -1, "T8750.b rc3 封过期 → 鸣叫落地（印主攻-1）");
        CHECK_EQ(s1125(ctx, 0, 6), 1, "T8750.b 无重复实授（覆盖键刷新不叠加）");
        std::cout << "  [T8750.b] 1125 必中穿透矛盾分支+下{1}回合封属 通过\n";
    }

    // ══ T8750.c 1125 属性技命中过滤（"对手所有攻击"字面）══
    //    必中鸣叫（cat4）穿透窗命中 → EVENT_HIT 发但 executed_skill_is_attack
    //    过滤 → 零触发、零封属；鸣叫两回合连落（攻-2）。
    {
        sbs_srand(8752);
        ScenarioBattle b(party_mixed({caster1125_pet(1, "印主", kC25338, kM3311, {}, {}, 300, 100)}),
                         party_mixed({attacker_pet(2, "属性手", 200, 100, 1)}));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc0：施放 + 必中鸣叫穿透（属性技命中）
        CHECK_EQ(ctx.ability_levels[0][0], -1, "T8750.c rc0 必中鸣照落地（印主攻-1）");
        CHECK_EQ(s1125(ctx, 0, 4), 0, "T8750.c 属性技命中不触发封属（过滤面）");
        CHECK_EQ(s1125(ctx, 0, 8), 0, "T8750.c 零排程");

        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[0][0], -2, "T8750.c rc1 鸣叫再落（无封属负测）");
        CHECK_EQ(s1125(ctx, 0, 4), 0, "T8750.c 全程零触发");
        std::cout << "  [T8750.c] 1125 属性技命中过滤 通过\n";
    }

    // ══ T8750.d 1125 镜像（双方 25338 剥 894/470；A 先手/B 后手错位双窗）══
    //    载体原生必中 → 双方施放都能穿过对方窗（镜像对称结构性成立——先手
    //    必Miss窗拦不住后手必中施放）。窗界按 round_effect_start_round 引擎
    //    约定错位：先手 A 窗 rc0..rc2（写入 1+2tick=3）、后手 B 窗 rc0..rc3
    //    （后出手顺延——写入 1+3tick=4，"后出手顺延一回合"官方回合数判定，
    //    battleContext.h 定版口径）。rc1/rc2 互 miss；rc3 A 被击中/B 仍闪避；
    //    rc4 B 窗过期被击中。剥兄弟防 894 吸取翻倍差污染 hp 面。
    {
        sbs_srand(8753);
        ScenarioBattle b(
            party_mixed({caster1125_pet(1, "印主A", kC25338, kM3311, {}, {894, 470}, 300, 100)}),
            party_mixed({caster1125_pet(2, "印主B", kC25338, kM3311, {}, {894, 470}, 200, 100)}));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：双施放（互穿对方窗）
        CHECK_EQ(s1125(ctx, 0, 2), 1, "T8750.d A 施放=1");
        CHECK_EQ(s1125(ctx, 1, 2), 1, "T8750.d B 施放=1（必中施放穿 A 窗）");
        CHECK_EQ(s1125(ctx, 0, 3), 1, "T8750.d A dodge 写入=1");
        CHECK_EQ(s1125(ctx, 1, 3), 1, "T8750.d B dodge 写入=1（镜像对称）");
        CHECK_EQ(s1125(ctx, 0, 8), 0, "T8750.d A 排程计数=0（必中属性施放被过滤——键位面）");
        CHECK_EQ(s1125(ctx, 1, 8), 0, "T8750.d B 排程计数=0（镜像）");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8750.d rc0 A 满血（无伤害源）");
        CHECK_EQ(onstage_hp(b, 1), kMaxHp, "T8750.d rc0 B 满血（镜像）");

        refill_pp(b, 0);
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc1：双拍打互 miss（skill slot 1=拍打，slot 0=载体）
        CHECK_EQ(s1125(ctx, 0, 3), 2, "T8750.d rc1 A tick=2");
        CHECK_EQ(s1125(ctx, 1, 3), 2, "T8750.d rc1 B tick=2（镜像）");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8750.d rc1 A 仍满血（互 miss）");
        CHECK_EQ(onstage_slot_pp(b, 1, 1), Skills(kPaiDa).maxPP - 1,
                 "T8750.d rc1 B 攻击发生（PP 正控制）");
        CHECK_EQ(onstage_slot_pp(b, 0, 1), Skills(kPaiDa).maxPP - 1,
                 "T8750.d rc1 A 攻击发生（镜像正控制）");

        refill_pp(b, 0);
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc2：互 miss（A 窗末）
        CHECK_EQ(s1125(ctx, 0, 3), 3, "T8750.d rc2 A tick=3（A 窗末回合）");
        CHECK_EQ(s1125(ctx, 1, 3), 3, "T8750.d rc2 B tick=3");

        refill_pp(b, 0);
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc3：A 窗过期（被 B 击中）；B 窗后手顺延仍活（躲开 A）
        CHECK_EQ(s1125(ctx, 0, 3), 3, "T8750.d rc3 A 无第 4 写（先手窗过期）");
        CHECK_EQ(s1125(ctx, 1, 3), 4, "T8750.d rc3 B tick=4（后手窗顺延一回合——引擎约定）");
        CHECK(onstage_hp(b, 0) < kMaxHp, "T8750.d rc3 A 被击中（先手窗 rc0..rc2 过期）");
        CHECK_EQ(onstage_hp(b, 1), kMaxHp, "T8750.d rc3 B 仍闪避（后手窗 rc0..rc3）");

        refill_pp(b, 0);
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc4：B 窗过期 → B 被击中
        CHECK_EQ(s1125(ctx, 1, 3), 4, "T8750.d rc4 B 无第 5 写（后手窗过期）");
        CHECK(onstage_hp(b, 1) < kMaxHp, "T8750.d rc4 B 被击中（后手窗 rc4 过期）");
        std::cout << "  [T8750.d] 1125 镜像双窗（先后手顺延错位） 通过\n";
    }

    // ══ T8750.e 1125 离场清理（换宠 epoch 作废）══
    //    rc0 施放+必中拍打穿透（触发+排程）；rc1 选择期提交 CHOOSE_PET——切换
    //    发生在**操作阶段**（先于 ROUND_START）→ 窗 tick 条目/封属排程条目/
    //    watcher 同回合全部 epoch 作废（rc1 无 tick 重写、排程不兑现）；rc2
    //    必中拍打落地（无 dodge）且零新触发（watcher 已死）。
    {
        sbs_srand(8754);
        ScenarioBattle b(party_mixed({caster1125_pet(1, "印主", kC25338, kM3311, {}, {}, 300, 100),
                                      relay_pet(11, "替补", 100, kMaxHp)}),
                         party_mixed({attacker_pet(2, "攻击手", 200, 100, 1)}));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：施放 + 必中拍打穿透（窗内命中——watcher 存活）
        CHECK_EQ(s1125(ctx, 0, 3), 1, "T8750.e rc0 dodge 写入=1");
        CHECK_EQ(s1125(ctx, 0, 4), 1, "T8750.e rc0 命中触发=1（必中拍打穿透窗）");
        CHECK_EQ(s1125(ctx, 0, 8), 1, "T8750.e rc0 排程=1");

        // rc1：切换在操作阶段（先于 ROUND_START）→ tick 与排程兑现双双作废。
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s1125(ctx, 0, 3), 1, "T8750.e rc1 无 tick 重写（窗条目随切换作废）");
        CHECK_EQ(s1125(ctx, 0, 6), 0, "T8750.e rc1 排程不兑现（封属随施放者离场未落地）");

        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        // rc2：印主已下场（epoch 作废）→ dodge 不再重写 → 必中拍打打替补落地；
        // watcher 作废 → 零新触发零排程。
        CHECK_EQ(ctx.ws.dodge_rate[0], 0.0f, "T8750.e rc2 dodge_rate 归零（离场清理）");
        CHECK(onstage_hp(b, 0) < kMaxHp, "T8750.e rc2 必中拍打打替补落地（窗随人走）");
        CHECK_EQ(s1125(ctx, 0, 4), 1, "T8750.e rc2 零新触发（watcher 随离场作废）");
        CHECK_EQ(s1125(ctx, 0, 8), 1, "T8750.e rc2 零新排程");
        std::cout << "  [T8750.e] 1125 离场清理 通过\n";
    }

    // ══ T8750.a2 1125 {0}=2 差分（29140，链 48/1814 剥离）══
    //    窗 rc0..rc1（两回合）→ rc2 落地（与 {0}=3 的 rc3 过期差分锁行参）。
    {
        sbs_srand(8755);
        ScenarioBattle b(party_mixed({caster1125_pet(1, "印主", kC29140, kM4813, {},
                                                     {48, 1814}, 300, 100)}),
                         party_mixed({attacker_pet(2, "攻击手", 200, 100, 0)}));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：施放 + miss
        CHECK_EQ(s1125(ctx, 0, 0), 2, "T8750.a2 {0}=2 行参回显（29140 差分）");
        CHECK_EQ(s1125(ctx, 0, 3), 1, "T8750.a2 rc0 dodge 写入=1");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8750.a2 rc0 miss");

        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(s1125(ctx, 0, 3), 2, "T8750.a2 rc1 tick=2（窗末回合）");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8750.a2 rc1 仍 miss");

        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(s1125(ctx, 0, 3), 2, "T8750.a2 rc2 无第 3 写（{0}=2 窗过期差分）");
        CHECK(onstage_hp(b, 0) < kMaxHp, "T8750.a2 rc2 落地（{0}=2 → rc2 即过期）");
        std::cout << "  [T8750.a2] 1125 {0}=2 窗界差分 通过\n";
    }

    // ══ T8750.f 618 主链（23481 [3,160] 纯链）══
    //    rc0 自爆（先手）→ 死切模型对手流程被跳过 → 替补登场即消费 voucher；
    //    rc1/rc2/rc3 扣减点各 heal 160；rc4 窗过期。接续宠 damaged 入场
    //    （heal 有实得量）。对手全程 NONE（heal 精确账）。
    {
        sbs_srand(8756);
        ScenarioBattle b(party_mixed({caster618_pet(1, "自爆手", kC23481, kM2512, 100),
                                      relay_pet(11, "替补A", 100, 89500),
                                      relay_pet(12, "替补B", 100, kMaxHp)}),
                         party_mixed({attacker_pet(2, "观战", 200, 0, 0)}));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s618(ctx, 0, 0), 3, "T8750.f {0}=3 行参回显");
        CHECK_EQ(s618(ctx, 0, 1), 160, "T8750.f {1}=160 行参回显");
        CHECK_EQ(s618(ctx, 0, 2), 1, "T8750.f 自爆施放计数=1");
        CHECK_EQ(s618(ctx, 0, 3), 2, "T8750.f 接续状态=已消费（登场消费 voucher）");
        CHECK_EQ(s618(ctx, 0, 4), 0, "T8750.f 接续消费回合=rc0（死亡换宠同回合）");
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0, "T8750.f 自爆宠 hp=0（消耗自身全部体力）");
        CHECK(ctx.pet_death_notified[0][0], "T8750.f 自爆死亡已登记（HP_CONSUME 漏斗）");
        CHECK_EQ(onstage_hp(b, 0), 89500, "T8750.f rc0 替补满伤入场（消费回合扣减点已过——首 heal 下回合）");
        CHECK(b.dump().find("\"effectId\":618") != std::string::npos,
              "T8750.f fullstate 含 618 桶条目（G2 显示面：接续回血窗）");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 89500 + 160, "T8750.f rc1 heal=160");
        CHECK_EQ(s618(ctx, 0, 5), 1, "T8750.f rc1 heal tick=1");
        CHECK_EQ(s618(ctx, 0, 6), 160, "T8750.f 累计实得=160");
        CHECK_EQ(s618(ctx, 0, 8), 1, "T8750.f 末次 heal 回合=rc1");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 89500 + 320, "T8750.f rc2 heal=160（累计 320）");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 89500 + 480, "T8750.f rc3 heal=160（{0}=3 窗末格）");
        CHECK_EQ(s618(ctx, 0, 5), 3, "T8750.f heal 计数=3");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 89500 + 480, "T8750.f rc4 窗过期（无第 4 次 heal）");
        CHECK_EQ(s618(ctx, 0, 5), 3, "T8750.f rc4 heal 计数停账（窗过期负测）");
        std::cout << "  [T8750.f] 618 自爆+接续回血主链 通过\n";
    }

    // ══ T8750.g 618 行参差分+满血钳零（28148 [4,300] 纯链）══
    //    替补 89500 入场（缺口 500）：rc1 heal 300 实得 300（{1} 行参可观测——
    //    160 硬编码变异在此被杀）；rc2 heal 300→钳满实得 100（部分钳零）；
    //    rc3/rc4 满血实得 0（满血钳零停账）；{0}=4 → tick rc1..rc4。
    {
        sbs_srand(8757);
        ScenarioBattle b(party_mixed({caster618_pet(1, "自爆手", kC28148, kM4446, 100),
                                      relay_pet(11, "替补A", 100, 89500),
                                      relay_pet(12, "替补B", 100, kMaxHp)}),
                         party_mixed({attacker_pet(2, "观战", 200, 0, 0)}));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s618(ctx, 0, 0), 4, "T8750.g {0}=4 行参回显（28148 差分）");
        CHECK_EQ(s618(ctx, 0, 1), 300, "T8750.g {1}=300 行参回显");
        CHECK_EQ(s618(ctx, 0, 3), 2, "T8750.g voucher 已消费");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 89500 + 300, "T8750.g rc1 heal=300（{1} 行参面）");
        CHECK_EQ(s618(ctx, 0, 6), 300, "T8750.g rc1 实得=300");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8750.g rc2 heal 300 钳满（89800+200=90000）");
        CHECK_EQ(s618(ctx, 0, 6), 500, "T8750.g 累计=500（部分钳零 200 可观测）");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8750.g rc3 满血 heal 实得 0");
        CHECK_EQ(s618(ctx, 0, 6), 500, "T8750.g 累计停账 500（满血钳零）");
        CHECK_EQ(s618(ctx, 0, 5), 3, "T8750.g rc3 tick 仍计（在位即结算、量钳零）");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s618(ctx, 0, 5), 4, "T8750.g {0}=4 → tick rc1..rc4 共 4 次");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s618(ctx, 0, 5), 4, "T8750.g rc5 窗过期（无第 5 tick）");
        std::cout << "  [T8750.g] 618 行参差分+满血钳零 通过\n";
    }

    // ══ T8750.h 618 只保下只 + 接续窗离场清理 ══
    //    rc0 自爆 → A 消费 voucher（一次性）；rc1 heal#1；rc2 A 主动切 B →
    //    heal 窗条目 epoch 作废（rc2 无 heal）+ B 登场不再触发（voucher 已消费
    //    一次性语义）。
    {
        sbs_srand(8758);
        ScenarioBattle b(party_mixed({caster618_pet(1, "自爆手", kC23481, kM2512, 100),
                                      relay_pet(11, "替补A", 100, 89500),
                                      relay_pet(12, "替补B", 100, 89000)}),
                         party_mixed({attacker_pet(2, "观战", 200, 0, 0)}));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s618(ctx, 0, 3), 2, "T8750.h voucher 已消费");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 89500 + 160, "T8750.h rc1 heal#1");
        CHECK_EQ(s618(ctx, 0, 5), 1, "T8750.h heal tick=1");

        // rc2：A 主动切 B（替补B damaged 89000 入场）
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 2, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 89000, "T8750.h rc2 B 入场（A 切下）");
        CHECK_EQ(s618(ctx, 0, 5), 1, "T8750.h rc2 无 heal（窗条目随 A 离场作废）");
        CHECK_EQ(s618(ctx, 0, 3), 2, "T8750.h 接续状态仍=已消费（voucher 一次性，B 无窗）");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 89000, "T8750.h rc3 B 无 heal（无接续负测）");
        CHECK_EQ(s618(ctx, 0, 5), 1, "T8750.h heal 计数停账 1");
        std::cout << "  [T8750.h] 618 只保下只+接续窗离场清理 通过\n";
    }

    // ══ T8750.i 618 镜像（双方 23481，先后自爆错位双窗）══
    //    rc0 A 先自爆（死切模型跳过 B 流程）→ A' 接续（窗 rc1..rc3）；
    //    rc1 B 自爆（其回合先手）→ 死切跳过 A' 流程 → B' 接续（窗 rc2..rc4）；
    //    rc4 A 窗已过期/B 窗仍活——错位过期差分。
    {
        sbs_srand(8759);
        ScenarioBattle b(
            party_mixed({caster618_pet(1, "自爆A", kC23481, kM2512, 300),
                         relay_pet(11, "A'", 100, 89500), relay_pet(12, "A''", 100, kMaxHp)}),
            party_mixed({caster618_pet(2, "自爆B", kC23481, kM2512, 200),
                         relay_pet(21, "B'", 100, 89500), relay_pet(22, "B''", 100, kMaxHp)}));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s618(ctx, 0, 3), 2, "T8750.i rc0 A voucher 已消费");
        CHECK_EQ(audit_int(ctx, k618Base + 1 * 16 + 2, 0), 0,
                 "T8750.i rc0 B 未施放（死切模型跳过其流程——键未落为默认 0）");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(s618(ctx, 1, 3), 2, "T8750.i rc1 B 自爆+消费（先手）");
        CHECK_EQ(onstage_hp(b, 0), 89500 + 160, "T8750.i rc1 A' heal#1（rc1 扣减点）");
        CHECK_EQ(s618(ctx, 0, 5), 1, "T8750.i A' heal tick=1");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 89500 + 320, "T8750.i rc2 A' heal#2");
        CHECK_EQ(onstage_hp(b, 1), 89500 + 160, "T8750.i rc2 B' heal#1（错位起点）");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 89500 + 480, "T8750.i rc3 A' heal#3（A 窗 rc1..rc3 末格）");
        CHECK_EQ(onstage_hp(b, 1), 89500 + 320, "T8750.i rc3 B' heal#2");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 89500 + 480, "T8750.i rc4 A 窗过期（无 heal#4）");
        CHECK_EQ(onstage_hp(b, 1), 89500 + 480, "T8750.i rc4 B' heal#3（{0}=3 窗末格——错位过期差分）");
        CHECK_EQ(s618(ctx, 0, 5), 3, "T8750.i A' heal 计数=3");
        CHECK_EQ(s618(ctx, 1, 5), 3, "T8750.i B' heal 计数=3（镜像对称计数）");

        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 1), 89500 + 480, "T8750.i rc5 B 窗过期（无 heal#4）");
        std::cout << "  [T8750.i] 618 镜像错位双窗 通过\n";
    }

    std::cout << "scenario_8750_k488 全部通过" << std::endl;
    return 0;
}
