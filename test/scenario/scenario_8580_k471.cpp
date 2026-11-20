// 场景 8580 —— K471 命耗百分比伤 + 自爆接续闪避 两族（2231/1416，
// resources/moves_lib/k471_hit_cost_equal_damage.cpp +
// k471_selfdestruct_dodge_relay.cpp；2026-11-17，worktree k471；
// 号段 8580-8589 → T8580-pre + a-o 十六块）
//
// 断言口径（三层观测面 + 真实载体正负测齐）：
//   ① 键面（plugin_storage 审计槽）：2231 基 1022310000（slot0={0}回显/
//      1={1}回显/2=末次触发回合/3=触发计数/4=申报cost/5=实耗/6=反噬申报/
//      7=钳零标记/8=施放计数/9=跳过原因）；1416 基 1014160000（slot0={0}回显/
//      1={1}回显/2={2}回显/3=施放计数/4=接续状态/5=接续回合/6=dodge写入计数/
//      7=末次申报pct/8=伤害申报量/9=跳过原因/10=voucher旗/11=watcher id）。
//   ② 通道面：hp 精确差分（自耗 cost=hp/{1} 整除 floor；1416 百分比伤
//      =max_hp/{0}——施放回合粉伤是对手唯一伤害源 → 严格等式）+ ws.dodge_rate
//      停泊直读（drive 后 ws 仍持有刚结束回合的值）。
//   ③ 行为面：miss/hit 计数（1416 接续窗逐回合）+ 死亡登记/换宠面
//      （自爆当场倒下、双倒双换）+ 接续窗离场清理（换宠后攻击落地）。
//
// 确定性手法（SOP §五.4）：
//   · 每块 sbs_srand 定标；2231 腿对手 = miss-only 攻击手（accuracy=0 且
//     must_hit=0 → hit_chance=0 → 恒 miss——攻击手不污染施放方 hp 精确账）；
//   · 1416 腿对手 = acc100/must_hit=0 攻击手（dodge=0 时 hit_chance=100 恒中、
//     dodge=1 时 hit_chance=0 恒 miss——申报面两档确定性；必中凭证不存在）；
//   · 载体招 accuracy=100/must_hit=1 短路命中链（武装招必中——miss 不结算
//     效果）；暴击全清零；行参变档走 EffectParamOverride（装配期官方覆盖基建）；
//   · 坑集 §4：满血构造 + set_onstage_hp 单独压当前 hp（max 不动）。
//   · 2231 精确账依赖"对手恒 miss"：反噬量精确断言走审计槽（slot5==slot6==cost
//     代码契约）+ 对手 hp 差分 ≥ 反噬（同回合对手还吃了施放方攻击——红伤
//     85%~100% 浮动不可精断，SOP §五.1 同款口径）；百分比伤公式严格等式由
//     1416 腿承担（35648 威力 0 → 施放回合除粉伤外零伤害源）。
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

constexpr int kPaiDa = 10004;        // 拍打：物40 acc100 无副作用（填充/攻击手）
constexpr int kC28867 = 28867;       // 2231 [2,2] 欲壑孽海（cat4 属性技）
constexpr int kC28971 = 28971;       // 2231 [2,3] 花满迷踪（cat4 属性技，主载体）
constexpr int kC29424 = 29424;       // 2231 [3,3] 事务排期
constexpr int kC38392 = 38392;       // 2231 [1,2] 锋毫浸血雨（cat2 威力150 攻击技载体）
constexpr int kC38456 = 38456;       // 2231 [2,2] 邪·天壤残火（cat1 威力160）
constexpr int kC27404 = 27404;       // 1416 [3,3,50] 心灵制约（纯链 cat4）
constexpr int kC32740 = 32740;       // 1416 [2,3,80] 樱之凋零（cat1 威力0）
constexpr int kC34401 = 34401;       // 1416 [3,2,100] 地能转移
constexpr int kC35648 = 35648;       // 1416 [3,3,70] 飞蛾扑火（纯链 cat2 威力0 prio1，变档载体）
constexpr int kC37053 = 37053;       // 1416 [1,3,100] 无间禁道（唯一 {0}=1）
constexpr int kC38490 = 38490;       // 1416 [2,2,100] TH第一守则（cat1 prio2）
constexpr int kM4728 = 4728;         // 28971 learner 考沃莱恩
constexpr int kM4932 = 4932;         // 29424/38490 learner 工程型TOH
constexpr int kM4903 = 4903;         // 38392 learner 帝录天锋
constexpr int kM4326 = 4326;         // 35648 learner 辛辛子
constexpr int kMaxHp = 90000;        // /2=45000 /3=30000 整除（坑集 §4：hp 单独压）

constexpr int k2231Base = 1022310000;  // + owner*16 + slot
constexpr int k1416Base = 1014160000;  // + owner*16 + slot

int audit_int(BattleContext& ctx, int key, int def) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : def;
}
int s2231(BattleContext& ctx, int owner, int slot) {
    return audit_int(ctx, k2231Base + owner * 16 + slot, -1);
}
int s1416(BattleContext& ctx, int owner, int slot) {
    return audit_int(ctx, k1416Base + owner * 16 + slot, -1);
}

ElfPet base_pet(int id, const char* name, const std::array<int, 5>& moves, int speed, int atk,
                int hp) {
    std::array<Skills, 5> skills{Skills(moves[0]), Skills(moves[1]), Skills(moves[2]),
                                 Skills(moves[3]), Skills(moves[4])};
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;   // 载体/武装招默认必中（miss 不结算效果——武装腿必须命中）
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

// 攻击手：must_hit=0 + acc=acc_v（acc_v=100 → dodge=0 时恒中 / dodge 面可观测；
// acc_v=0 → hit_chance=0 恒 miss——miss-only 腿保护施放方 hp 精确账）。
ElfPet attacker_pet(int id, const char* name, int speed, int acc_v) {
    ElfPet p = base_pet(id, name, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed, 100, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = acc_v;
    }
    return p;
}

// 覆盖变体宠（Skills(id, monster, overrides, disabled) 装配期官方覆盖/禁用基建，
// K453/K458/K467 同款；disabled 剥离链上兄弟——污染面见各腿注释）。
ElfPet override_pet(int id, const char* name, int carrier, int effect_id, int monster_id,
                    const std::vector<std::pair<int, int>>& arg_overrides,
                    const std::vector<int>& disabled_effects, int speed, int atk, int hp) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(monster_id);
    if (!monster) {
        std::cerr << "override_pet: monster " << monster_id << " load failed" << std::endl;
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

std::array<ElfPet, 6> party_of(const ElfPet& lead) {
    return make_party(lead);
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}
int onstage_hp(ScenarioBattle& b, int side) {
    return b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp;
}

// 在场宠指定槽 PP 直读（"攻击确实发生了"正控制：miss 也耗 PP——技能使用即扣）。
int onstage_slot_pp(ScenarioBattle& b, int side, int idx) {
    return b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].skills[idx].pp;
}

// 在场宠技能 PP 回满（长回合块防 PP 耗尽；k467 同款）。
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

    // ══ T8580-pre 11 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>>
            expect = {
                // ── 2231 五载体（{0}∈{1,2,3}×{1}∈{2,3}）──
                {28867, {{1687, {4}},
                         {2230, {4, 2, 100, 22}},
                         {2229, {3}},
                         {2231, {2, 2}},
                         {2232, {2}}}},
                {28971, {{911, {1}}, {2231, {2, 3}}, {58, {2}}, {843, {2, 2}}}},
                {29424, {{885, {1}}, {2231, {3, 3}}, {843, {2, 2}}}},
                {38392, {{2231, {1, 2}},
                         {478, {2}},
                         {2320, {2, -1, 0, -1, 0, -1, 0, 2}},
                         {2269, {}},
                         {2351, {1, 2}}}},
                {38456, {{2079, {}},
                         {1922, {2}},
                         {2116, {2, 70}},
                         {1799, {100, 22, 1}},
                         {2231, {2, 2}}}},
                // ── 1416 六载体（{0}∈{1,2,3}×{1}∈{2,3}×{2}∈{50,70,80,100}）──
                {27404, {{1416, {3, 3, 50}}}},
                {32740, {{1416, {2, 3, 80}}}},
                {34401, {{749, {100, 1}}, {1416, {3, 2, 100}}}},
                {35648, {{1416, {3, 3, 70}}}},
                {37053, {{1416, {1, 3, 100}}, {583, {2}}}},
                {38490, {{1416, {2, 2, 100}}, {1794, {2, 1, 50}}}},
            };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8580-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8580-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8580-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8580-pre] 11 载体实际加载器切片核验 通过\n";
    }

    // ══ T8580.a 2231 全链精确账（28971 [2,3] 主载体，兄弟 911/58/843 剥离）══
    //    对手 miss-only（acc0 恒 miss）→ 施放方 hp 只受自耗影响 → 严格等式。
    //    rc0 武装（属性技无攻击）→ rc1/rc2 窗内触发（cost=hp/{1} 整除 floor）→
    //    rc3 窗过期不触发。
    {
        sbs_srand(101);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC28971, 2231, kM4728, {}, {911, 58, 843}, 300, 1,
                                  kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, /*acc=*/0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：印主武装（属性技）；对手攻击恒 miss
        CHECK_EQ(s2231(ctx, 0, 8), 1, "T8580.a rc0 施放计数=1");
        CHECK_EQ(s2231(ctx, 0, 0), 2, "T8580.a {0}=2 行参回显");
        CHECK_EQ(s2231(ctx, 0, 1), 3, "T8580.a {1}=3 行参回显");
        CHECK_EQ(s2231(ctx, 0, 3), 0, "T8580.a rc0 触发计数=0（武装回合无自方攻击落血）");
        CHECK_EQ(s2231(ctx, 0, 2), -1, "T8580.a 末次触发回合=-1（未触发基线）");

        set_onstage_hp(b, 0, 60000);   // 压当前 hp（60000/3=20000 整除）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1：印主拍打命中 → 窗内首触发
        CHECK_EQ(s2231(ctx, 0, 3), 1, "T8580.a rc1 触发计数=1");
        CHECK_EQ(s2231(ctx, 0, 2), 1, "T8580.a 末次触发回合=rc1（下{0}回合窗起点）");
        CHECK_EQ(s2231(ctx, 0, 4), 20000, "T8580.a 申报 cost=60000/3=20000");
        CHECK_EQ(s2231(ctx, 0, 5), 20000, "T8580.a 实耗=20000（无护罩干净路径）");
        CHECK_EQ(s2231(ctx, 0, 6), 20000, "T8580.a 反噬申报=实耗（等量契约）");
        CHECK_EQ(s2231(ctx, 0, 7), 0, "T8580.a 钳零标记=0（正常消耗）");
        CHECK_EQ(onstage_hp(b, 0), 40000, "T8580.a 自耗后 hp=60000-20000=40000（严格等式）");
        CHECK(onstage_hp(b, 1) <= kMaxHp - 20000,
              "T8580.a 对手掉血 ≥ 反噬 20000（红伤浮动不可精断，SOP §五.1 口径）");

        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc2（窗末回合）：cost=40000/3=13333 整除 floor
        CHECK_EQ(s2231(ctx, 0, 3), 2, "T8580.a rc2 触发计数=2");
        CHECK_EQ(s2231(ctx, 0, 4), 13333, "T8580.a cost=40000/3=13333（整数除法 floor）");
        CHECK_EQ(onstage_hp(b, 0), 26667, "T8580.a hp=40000-13333=26667（严格等式）");

        set_onstage_hp(b, 0, 60000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc3：窗外（{0}=2 → 窗 rc1..rc2）→ 不触发
        CHECK_EQ(s2231(ctx, 0, 3), 2, "T8580.a rc3 窗过期不触发（计数仍 2）");
        CHECK_EQ(onstage_hp(b, 0), 60000, "T8580.a rc3 hp 无自耗（窗过期）");
        std::cout << "  [T8580.a] 2231 武装+窗内触发+整除 floor+窗过期 通过\n";
    }

    // ══ T8580.b 2231 miss 负测 + miss 不耗窗（28971 [2,3]）══
    //    rc1 攻击恒 miss（acc0 招）→ 不触发；rc2 命中 → 触发（窗未被 miss 消耗）。
    {
        sbs_srand(102);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC28971, 2231, kM4728, {}, {911, 58, 843}, 300, 1,
                                  kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0 武装
        // rc1：印主改用 accuracy=0 的拍打副本（直接改招面）→ 恒 miss → 不触发
        {
            ElfPet& me = ctx.seerRobot[0].elfPets[ctx.on_stage[0]];
            me.skills[1].must_hit = 0;
            me.skills[1].accuracy = 0;   // hit_chance=0 → 恒 miss（确定性）
        }
        set_onstage_hp(b, 0, 60000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(s2231(ctx, 0, 3), 0, "T8580.b rc1 miss 不触发（命中才消耗）");
        CHECK_EQ(onstage_hp(b, 0), 60000, "T8580.b miss 无自耗");
        CHECK_EQ(onstage_hp(b, 1), kMaxHp, "T8580.b miss 无反噬（对手满血）");
        // rc2：换回 acc100 拍打 → 命中 → 触发（miss 未耗窗）
        {
            ElfPet& me = ctx.seerRobot[0].elfPets[ctx.on_stage[0]];
            me.skills[1].accuracy = 100;
        }
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(s2231(ctx, 0, 3), 1, "T8580.b rc2 命中触发（窗仍活）");
        CHECK_EQ(s2231(ctx, 0, 2), 2, "T8580.b 末次触发回合=rc2");
        CHECK_EQ(onstage_hp(b, 0), 40000, "T8580.b rc2 自耗 20000（60000/3）");
        std::cout << "  [T8580.b] 2231 miss 负测+miss 不耗窗 通过\n";
    }

    // ══ T8580.c 2231 施放回合排除（38392 锋毫浸血雨 [1,2] 攻击技载体）══
    //    载体自身是威力 150 攻击技：rc0 施放回合的这次攻击命中**不触发**
    //    （"下{0}回合"NextRounds 窗界）；rc1 拍打触发；rc2 窗过期。
    {
        sbs_srand(103);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC38392, 2231, kM4903, {}, {478, 2320, 2269, 2351},
                                  300, 1, kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 60000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：载体攻击技命中（对对手造成红伤）但不触发
        CHECK_EQ(s2231(ctx, 0, 3), 0, "T8580.c rc0 施放回合自身攻击命中不触发（下{0}回合窗界）");
        CHECK_EQ(onstage_hp(b, 0), 60000, "T8580.c rc0 无自耗");
        CHECK(onstage_hp(b, 1) < kMaxHp, "T8580.c 载体攻击技确实命中落血（前提成立）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1（{0}=1 唯一窗回合）：触发
        CHECK_EQ(s2231(ctx, 0, 3), 1, "T8580.c rc1 触发（窗内）");
        CHECK_EQ(onstage_hp(b, 0), 30000, "T8580.c cost=60000/2=30000（{1}=2）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc2：窗外
        CHECK_EQ(s2231(ctx, 0, 3), 1, "T8580.c rc2 窗过期不触发");
        CHECK_EQ(onstage_hp(b, 0), 30000, "T8580.c rc2 hp 不变");
        std::cout << "  [T8580.c] 2231 施放回合排除（攻击技载体） 通过\n";
    }

    // ══ T8580.d 2231 镜像（双方 28971 武装）══
    //    双方各攻击命中一次：各侧 watcher 只认自方攻击（计数各 1）、各侧自耗+
    //    反噬对称（hp 双双下降）；交叉触发为零是镜像主断言。
    {
        sbs_srand(104);
        ScenarioBattle b(
            party_of(override_pet(1, "印主A", kC28971, 2231, kM4728, {}, {911, 58, 843}, 300, 1,
                                  kMaxHp)),
            party_of(override_pet(2, "印主B", kC28971, 2231, kM4728, {}, {911, 58, 843}, 200, 1,
                                  kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0 双方武装（属性技互不攻击）
        CHECK_EQ(s2231(ctx, 0, 8), 1, "T8580.d side0 施放");
        CHECK_EQ(s2231(ctx, 1, 8), 1, "T8580.d side1 施放（镜像）");
        set_onstage_hp(b, 0, 60000);
        set_onstage_hp(b, 1, 60000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc1 双方拍打互攻（都命中）
        CHECK_EQ(s2231(ctx, 0, 3), 1, "T8580.d side0 触发=1（只认自方攻击）");
        CHECK_EQ(s2231(ctx, 1, 3), 1, "T8580.d side1 触发=1（交叉触发为零）");
        CHECK_EQ(s2231(ctx, 0, 4), 20000, "T8580.d side0 cost=60000/3（先手方满 60000 时触发）");
        CHECK(s2231(ctx, 1, 4) > 0 && s2231(ctx, 1, 4) < 20000,
              "T8580.d side1 cost=后手时 hp/3（已吃 side0 反噬，<20000 且 >0）");
        CHECK(onstage_hp(b, 0) < 40000, "T8580.d side0 hp=40000-自耗-对手反噬");
        CHECK(onstage_hp(b, 1) < 40000, "T8580.d side1 hp 对称下降");
        CHECK(onstage_hp(b, 0) == onstage_hp(b, 1),
              "T8580.d 镜像对称（同构武装+同攻击序 → 终态 hp 相等）");
        std::cout << "  [T8580.d] 2231 镜像（交叉触发为零+对称） 通过\n";
    }

    // ══ T8580.e 2231 钳零（38392 [1,2]，hp=1 → 1/2=0）══
    {
        sbs_srand(105);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC38392, 2231, kM4903, {}, {478, 2320, 2269, 2351},
                                  300, 1, kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0 武装
        set_onstage_hp(b, 0, 1);   // 体力不足 1/{1}（1/2=0）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1 命中 → cost=0 钳零
        CHECK_EQ(s2231(ctx, 0, 3), 1, "T8580.e rc1 触发到达（攻击命中）");
        CHECK_EQ(s2231(ctx, 0, 7), 1, "T8580.e 钳零标记=1（cost=1/2=0）");
        CHECK_EQ(s2231(ctx, 0, 4), 0, "T8580.e 申报 cost=0");
        CHECK_EQ(s2231(ctx, 0, 5), 0, "T8580.e 实耗=0（钳零不消耗）");
        CHECK_EQ(onstage_hp(b, 0), 1, "T8580.e hp 保持 1（钳零）");
        CHECK(onstage_hp(b, 1) >= kMaxHp - 1000,
              "T8580.e 无反噬（对手只吃拍打小伤害，atk=1 vs 100 防 ≈5 点）");
        std::cout << "  [T8580.e] 2231 自耗钳零（体力不足 1/{1}） 通过\n";
    }

    // ══ T8580.f 2231 重施放刷新（28971 [2,3]）══
    //    rc0 武装 → rc1 触发 → rc2 重武装（计数复位 + 旧 watcher 摘除）→
    //    rc3 触发恰好一次（无双重消耗——卫生断言）。
    {
        sbs_srand(106);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC28971, 2231, kM4728, {}, {911, 58, 843}, 300, 1,
                                  kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0 武装
        set_onstage_hp(b, 0, 60000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1 触发
        CHECK_EQ(s2231(ctx, 0, 3), 1, "T8580.f rc1 触发");
        CHECK_EQ(onstage_hp(b, 0), 40000, "T8580.f rc1 自耗后 40000");
        refill_pp(b, 0);
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc2 重武装（28971 再施放）
        CHECK_EQ(s2231(ctx, 0, 8), 2, "T8580.f 施放计数=2");
        CHECK_EQ(s2231(ctx, 0, 3), 0, "T8580.f 重武装复位触发计数（刷新不叠加）");
        set_onstage_hp(b, 0, 60000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc3 新窗首触发
        CHECK_EQ(s2231(ctx, 0, 3), 1, "T8580.f rc3 触发=1（单 watcher——旧 watcher 已摘）");
        CHECK_EQ(onstage_hp(b, 0), 40000,
                 "T8580.f 单次消耗（双重 watcher 会双耗至 20000——卫生锁定）");
        std::cout << "  [T8580.f] 2231 重施放刷新+watcher 卫生 通过\n";
    }

    // ══ T8580.g 2231 {0}=1/{1}=2 差分（29424 [3,3] → 变档 [1,2]）＋ 行参回显 ══
    //    EffectParamOverride 变档（装配期官方基建）锁定行参驱动（非硬编码）。
    {
        sbs_srand(107);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC29424, 2231, kM4932, {{0, 1}, {1, 2}}, {885, 843},
                                  300, 1, kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0 武装（变档 [1,2]）
        CHECK_EQ(s2231(ctx, 0, 0), 1, "T8580.g {0}→1 行参回显（变档生效）");
        CHECK_EQ(s2231(ctx, 0, 1), 2, "T8580.g {1}→2 行参回显");
        set_onstage_hp(b, 0, 60000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1 窗内（唯一窗回合）触发：cost=60000/2=30000
        CHECK_EQ(s2231(ctx, 0, 3), 1, "T8580.g rc1 触发");
        CHECK_EQ(onstage_hp(b, 0), 30000, "T8580.g cost=60000/2=30000（{1}=2 差分）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc2 窗过期（{0}=1）
        CHECK_EQ(s2231(ctx, 0, 3), 1, "T8580.g rc2 窗过期（N=1 载体窗界）");
        CHECK_EQ(onstage_hp(b, 0), 30000, "T8580.g rc2 hp 不变");
        std::cout << "  [T8580.g] 2231 行参变档差分 通过\n";
    }

    // ══ 引擎事实（本场景实测入档，变异批与后单共用）：先手方行动流程内死亡
    //    （自爆）→ FSM 跳过后手方当回合流程与回合尾（"Death occurred during
    //    first mover flow, skipping second mover flow and round end"）→ 登场
    //    宠当回合**不可能被攻击**。1416 接续窗首跳的当回合面无攻击可观测
    //    （申报面 dodge_rate 直读仍可断）；闪避**行为面**自登场次回合起
    //    （ROUND_START tick 重写 + 对手攻击恒 miss），"miss vs 没攻击"以对手
    //    PP 消耗作正控制（技能使用即扣 PP——miss 也耗）。

    // ══ T8580.h 1416 主链（35648 [3,3,70]→{2}=100 变档）：自爆+百分比伤+
    //    接续窗逐回合（首跳申报面 + tick 行为面 + 窗过期）══
    //    35648 prio1 → 施放方先手：rc0 自爆（对手受 max_hp/3=30000 百分比伤）
    //    → 引擎跳过对手当回合流程（PP 正控制）→ rc1/rc2 tick 闪避 → rc3 落地。
    {
        sbs_srand(108);
        ScenarioBattle b(
            party_of(override_pet(1, "自爆手", kC35648, 1416, kM4326, {{2, 100}}, {}, 100, 1,
                                  kMaxHp)),
            party_of(attacker_pet(2, "攻击手", 200, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：自爆（prio1 先手）→ 死切模型跳过对手流程 → 换宠登场
        CHECK_EQ(s1416(ctx, 0, 3), 1, "T8580.h 施放计数=1");
        CHECK_EQ(s1416(ctx, 0, 8), 30000, "T8580.h 伤害申报=max_hp/3=30000（公式）");
        CHECK_EQ(s1416(ctx, 0, 4), 2, "T8580.h 接续状态=已消费（登场消费 voucher）");
        CHECK_EQ(s1416(ctx, 0, 5), 0, "T8580.h 接续消费回合=rc0（死亡换宠同回合）");
        CHECK_EQ(s1416(ctx, 0, 6), 1, "T8580.h dodge 写入=1（首跳；ROUND_START tick 未到）");
        CHECK_EQ(s1416(ctx, 0, 7), 100, "T8580.h 末次申报 pct=100");
        CHECK_EQ(s1416(ctx, 0, 2), 100, "T8580.h {2}→100 行参回显");
        CHECK_EQ(s1416(ctx, 0, 0), 3, "T8580.h {0}=3 行参回显");
        CHECK_EQ(s1416(ctx, 0, 1), 3, "T8580.h {1}=3 行参回显");
        CHECK(onstage_hp(b, 1) >= kMaxHp - 30000 - 2 && onstage_hp(b, 1) <= kMaxHp - 30000,
              "T8580.h 对手掉血=30000+施放攻击最低伤 ≤1（威力 0 招落 1 点红伤；"
              "百分比伤公式精确值由 slot8=30000 锚定）");
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0,
                 "T8580.h 自爆宠 hp=0（消耗自身全部体力）");
        CHECK(ctx.pet_death_notified[0][0], "T8580.h 自爆死亡已登记（HP_CONSUME 漏斗）");
        CHECK_EQ(ctx.ws.dodge_rate[0], 1.0f, "T8580.h 首跳 dodge_rate=1.0（停泊直读）");
        CHECK_EQ(onstage_slot_pp(b, 1, 0), attacker_pet(2, "x", 1, 100).skills[0].maxPP,
                 "T8580.h rc0 对手 PP 未动（死切模型：先手方死亡 → 后手方流程被跳过）");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8580.h 换宠登场宠满血（当回合无攻击发生）");

        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc1：tick 重写 → 对手攻击恒 miss
        CHECK_EQ(s1416(ctx, 0, 6), 2, "T8580.h rc1 dodge tick 写入=2");
        CHECK_EQ(ctx.ws.dodge_rate[0], 1.0f, "T8580.h rc1 tick 维持 dodge_rate=1.0");
        CHECK_EQ(onstage_slot_pp(b, 1, 1), attacker_pet(2, "x", 1, 100).skills[0].maxPP - 1,
                 "T8580.h rc1 对手攻击确实发生（PP 正控制）");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8580.h rc1 攻击被 100% 闪避（窗内）");

        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc2（窗末回合）：tick 3 → miss
        CHECK_EQ(s1416(ctx, 0, 6), 3, "T8580.h rc2 dodge tick 写入=3");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8580.h rc2 仍闪避（前{1}=3 回合末格）");

        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc3：窗过期（无 tick 重写，ws reset）→ 攻击落地
        CHECK_EQ(s1416(ctx, 0, 6), 3, "T8580.h rc3 无第 4 次写入（窗过期）");
        CHECK_EQ(ctx.ws.dodge_rate[0], 0.0f, "T8580.h rc3 dodge_rate 归零（ws reset）");
        CHECK(onstage_hp(b, 0) < kMaxHp, "T8580.h rc3 攻击落地（窗过期）");
        std::cout << "  [T8580.h] 1416 自爆+百分比伤+接续窗逐回合 通过\n";
    }

    // ══ T8580.i 1416 0% 关闸档（35648 {2}→0）＋ {0}=2 公式差分 ══
    //    0% = 关闸档在位消费：首跳申报 dodge_rate=0.0（结构性不闪避）；
    //    行为面 rc1 攻击落地（PP 正控制排除"没攻击"假绿）。
    {
        sbs_srand(109);
        ScenarioBattle b(
            party_of(override_pet(1, "自爆手", kC35648, 1416, kM4326, {{0, 2}, {2, 0}}, {}, 100,
                                  1, kMaxHp)),
            party_of(attacker_pet(2, "攻击手", 200, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 45000);   // 压施放者当前 hp（公式基准变异面：max 基准不受影响）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：自爆（伤害=max_hp/2=45000）→ 换宠（死切模型对手未行动）
        CHECK_EQ(s1416(ctx, 0, 8), 45000, "T8580.i 伤害申报=max_hp/2=45000（{0}=2 差分）");
        CHECK(onstage_hp(b, 1) >= kMaxHp - 45000 - 2 && onstage_hp(b, 1) <= kMaxHp - 45000,
              "T8580.i 对手掉血=45000+施放攻击最低伤 ≤1（{0}=2 公式差分）");
        CHECK_EQ(s1416(ctx, 0, 4), 2, "T8580.i voucher 照常消费（0% 是关闸档非关闭）");
        CHECK_EQ(s1416(ctx, 0, 6), 1, "T8580.i 首跳写入（dodge_rate=0.0 结构性）");
        CHECK_EQ(ctx.ws.dodge_rate[0], 0.0f, "T8580.i 0% 申报 dodge_rate=0.0");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc1：tick 重写 0.0 → 对手攻击落地（0% 不闪避）
        CHECK_EQ(onstage_slot_pp(b, 1, 1), attacker_pet(2, "x", 1, 100).skills[0].maxPP - 1,
                 "T8580.i rc1 对手攻击确实发生（PP 正控制）");
        CHECK(onstage_hp(b, 0) < kMaxHp,
              "T8580.i rc1 攻击落地（0% 结构性不闪避——在位即消费）");
        std::cout << "  [T8580.i] 1416 0% 关闸档+{0}=2 公式差分 通过\n";
    }

    // ══ T8580.j 1416 双倒同波（35648 {0}→1：反噬=全额最大体力击倒对手）══
    //    自爆宠与对手同波倒下：自爆死亡已同步登记 → 同归保护不救援（守卫要求
    //    双方均未登记）→ 双换；我方接续窗跨双换存活。
    {
        sbs_srand(110);
        ScenarioBattle b(
            party_of(override_pet(1, "自爆手", kC35648, 1416, kM4326, {{0, 1}, {2, 100}}, {}, 100,
                                  1, kMaxHp)),
            party_of(attacker_pet(2, "攻击手", 200, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 1, 50000);   // 对手 hp < 反噬 90000 → 必倒
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：自爆 → 对手倒（粉伤）→ 自爆倒（HP_CONSUME）→ 双换
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0, "T8580.j 自爆宠 hp=0");
        CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 0, "T8580.j 对手宠 hp=0（粉伤击倒）");
        CHECK(ctx.pet_death_notified[0][0], "T8580.j 自爆死亡已登记（当场倒下不救援）");
        CHECK(ctx.pet_death_notified[1][0], "T8580.j 对手死亡已登记");
        CHECK_EQ(s1416(ctx, 0, 4), 2, "T8580.j voucher 跨双换消费");
        CHECK_EQ(s1416(ctx, 0, 5), 0, "T8580.j 接续回合=rc0");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc1：对手新宠攻击 → 我方接续窗 100% 闪避
        CHECK_EQ(ctx.ws.dodge_rate[0], 1.0f, "T8580.j rc1 tick 维持 dodge=1.0");
        CHECK_EQ(onstage_slot_pp(b, 1, 1), attacker_pet(2, "x", 1, 100).skills[0].maxPP - 1,
                 "T8580.j rc1 对手攻击确实发生（PP 正控制）");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8580.j rc1 攻击被闪避（窗跨双换存活）");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc2（{1}=3 窗末）：仍闪避
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8580.j rc2 仍闪避");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc3：窗过期 → 落地
        CHECK(onstage_hp(b, 0) < kMaxHp, "T8580.j rc3 攻击落地（窗过期）");
        std::cout << "  [T8580.j] 1416 双倒同波时序+窗跨双换 通过\n";
    }

    // ══ T8580.k 1416 无接续负测（35648 {1}→2, {2}→100）══
    //    rc0 自爆 → 新宠接续窗（rc0 首跳+rc1 tick，死切模型 rc0 无攻击）；
    //    rc1 对手攻击被闪避；rc2 我方**主动切换**第三只 → 无窗（voucher 已
    //    一次性消费）→ 对手攻击第三只落地——"下只"一次语义 + 窗过期面。
    {
        sbs_srand(111);
        ScenarioBattle b(
            party_of(override_pet(1, "自爆手", kC35648, 1416, kM4326, {{1, 2}, {2, 100}}, {}, 100,
                                  1, kMaxHp)),
            party_of(attacker_pet(2, "攻击手", 200, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：自爆+换宠（首跳 dodge；死切模型对手未行动）
        CHECK_EQ(s1416(ctx, 0, 4), 2, "T8580.k voucher 已消费");
        CHECK_EQ(s1416(ctx, 0, 6), 1, "T8580.k 首跳写入=1");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc1：tick（窗末回合）→ 对手攻击 miss
        CHECK_EQ(s1416(ctx, 0, 6), 2, "T8580.k rc1 tick 写入=2（{1}=2 → 单 tick）");
        CHECK_EQ(onstage_slot_pp(b, 1, 1), attacker_pet(2, "x", 1, 100).skills[0].maxPP - 1,
                 "T8580.k rc1 对手攻击确实发生（PP 正控制）");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8580.k rc1 仍闪避（窗 rc0..rc1）");
        refill_pp(b, 1);
        // rc2：我方主动切换（第三只登场；窗已过+无接续）→ 对手攻击第三只落地
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 2, BattleFsm::ActionType::SELECT_SKILL,
                  1);
        CHECK_EQ(s1416(ctx, 0, 6), 2, "T8580.k 无第 3 次写入（voucher 一次性+窗过期）");
        CHECK_EQ(s1416(ctx, 0, 4), 2, "T8580.k 接续状态仍=已消费（不重挂）");
        CHECK(onstage_hp(b, 0) < kMaxHp, "T8580.k 第三只宠无窗（对手攻击落地——无接续负测）");
        std::cout << "  [T8580.k] 1416 无接续负测（第三只无窗） 通过\n";
    }

    // ══ T8580.l 1416 镜像（双方 35648 {2}=100，先后自爆 + 双窗共存差分）══
    //    rc0 A（速度 300）先自爆 → A 换宠（A 窗挂 rc0..rc1）；B 流程被死切跳过
    //    （PP 正控制）。rc1 B 自爆（prio1 先手）→ B 反噬打 A 新宠（粉伤不受
    //    闪避）→ B 换宠（B 窗挂 rc1..rc2）。rc2 A 窗已过期/B 窗仍活：A 攻击 B
    //    被 B 窗闪避、B 攻击 A 落地——双窗共存 + 错位过期差分。rc3 双窗皆过期。
    {
        sbs_srand(112);
        ScenarioBattle b(
            party_of(override_pet(1, "自爆A", kC35648, 1416, kM4326, {{1, 2}, {2, 100}}, {}, 300, 1,
                                  kMaxHp)),
            party_of(override_pet(2, "自爆B", kC35648, 1416, kM4326, {{1, 2}, {2, 100}}, {}, 200, 1,
                                  kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：A 自爆 → A 换宠；B 流程被死切跳过
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0, "T8580.l A 自爆倒下");
        CHECK_EQ(s1416(ctx, 0, 4), 2, "T8580.l A voucher 消费");
        CHECK_EQ(s1416(ctx, 1, 4), -1, "T8580.l B 未施放（slot4 无值——死切跳过）");
        CHECK_EQ(onstage_slot_pp(b, 1, 0), 1,
                 "T8580.l B 的 35648 PP 未动（rc0 B 确实未行动）");
        refill_pp(b, 1);
        refill_pp(b, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1：B prio1 先手自爆 → 反噬打 A 新宠 → B 换宠；A 流程被跳过
        CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 0, "T8580.l B 自爆倒下");
        CHECK_EQ(s1416(ctx, 1, 4), 2, "T8580.l B voucher 消费（镜像）");
        CHECK_EQ(s1416(ctx, 1, 6), 1, "T8580.l B 首跳写入=1（{1}=2 → 单 tick）");
        CHECK(onstage_hp(b, 0) >= kMaxHp - 30000 - 2 && onstage_hp(b, 0) <= kMaxHp - 30000,
              "T8580.l A 新宠吃 B 反噬 30000+最低伤 ≤1（粉伤不受闪避面）");
        refill_pp(b, 1);
        refill_pp(b, 0);
        // 双方拍打去必中（副本默认 must_hit=1 会绕过整条命中率链——K7 口径，
        // 闪避面须 must_hit=0 才可观测；35648 施放招保持必中不受影响）
        for (const int side : {0, 1}) {
            ElfPet& cur = ctx.seerRobot[side].elfPets[ctx.on_stage[side]];
            cur.skills[1].must_hit = 0;
        }
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc2：A 窗过期/B 窗仍活（速度 A 先）：A 攻击 B 被闪避、B 攻击 A 落地
        CHECK_EQ(onstage_slot_pp(b, 0, 1), attacker_pet(2, "x", 1, 100).skills[0].maxPP - 1,
                 "T8580.l rc2 A 攻击确实发生（PP 正控制）");
        CHECK_EQ(onstage_hp(b, 1), kMaxHp, "T8580.l rc2 B 新宠被 A 攻击闪避（B 窗 rc1..rc2）");
        CHECK(onstage_hp(b, 0) < kMaxHp - 30000,
              "T8580.l rc2 B 攻击 A 落地（A 窗 rc0..rc1 已过期——错位差分）");
        refill_pp(b, 1);
        refill_pp(b, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc3：双窗皆过期 → 双方攻击都落地
        CHECK(onstage_hp(b, 1) < kMaxHp, "T8580.l rc3 B 被命中（窗过期）");
        CHECK(onstage_hp(b, 0) < kMaxHp - 30000, "T8580.l rc3 A 被命中");
        std::cout << "  [T8580.l] 1416 镜像（双自爆+双窗共存+错位过期） 通过\n";
    }

    // ══ T8580.m 1416 纯链载体烟测（27404 心灵制约 [3,3,50] 直构，Skills 直构）══
    //    50% 掷点档不断言落点（申报制——掷点在引擎命中公式）；断言确定性面：
    //    自爆/伤害/公式/voucher/首跳申报。
    {
        sbs_srand(113);
        ScenarioBattle b(
            party_of(base_pet(1, "自爆手", {kC27404, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1,
                              kMaxHp)),
            party_of(attacker_pet(2, "攻击手", 200, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：自爆（[3,3,50] 原始行参）
        CHECK_EQ(s1416(ctx, 0, 8), 30000, "T8580.m 伤害申报=max_hp/3（27404 {0}=3 原始切片）");
        CHECK(onstage_hp(b, 1) >= kMaxHp - 30000 - 2 && onstage_hp(b, 1) <= kMaxHp - 30000,
              "T8580.m 对手掉血=30000+施放攻击最低伤 ≤1（27404 {0}=3 原始切片）");
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0, "T8580.m 自爆倒下");
        CHECK_EQ(s1416(ctx, 0, 4), 2, "T8580.m voucher 消费");
        CHECK_EQ(s1416(ctx, 0, 7), 50, "T8580.m 首跳申报 pct=50（原始行参）");
        CHECK_EQ(s1416(ctx, 0, 2), 50, "T8580.m {2}=50 行参回显");
        std::cout << "  [T8580.m] 1416 纯链载体烟测（27404） 通过\n";
    }

    // ══ T8580.n 1416 接续窗 {1}=2 tick 界（38490 TH第一守则 [2,2,100] 直构）══
    //    {1}=2：窗 = rc0 首跳 + rc1 tick（NextRounds ×1）；rc2 攻击落地。
    //    38490 prio2 → 施放必先手；链上 1794（技能无效恢复族）无无效趟不触发。
    {
        sbs_srand(114);
        ScenarioBattle b(
            party_of(base_pet(1, "自爆手", {kC38490, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1,
                              kMaxHp)),
            party_of(attacker_pet(2, "攻击手", 200, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：自爆（{1}=2）→ 换宠首跳（死切模型对手未行动）
        CHECK_EQ(s1416(ctx, 0, 8), 45000, "T8580.n 伤害申报=max_hp/2（{0}=2）");
        CHECK(onstage_hp(b, 1) >= kMaxHp - 45000 - 2 && onstage_hp(b, 1) <= kMaxHp - 45000,
              "T8580.n 对手掉血=45000+施放攻击最低伤 ≤1（38490 {0}=2）");
        CHECK_EQ(s1416(ctx, 0, 6), 1, "T8580.n 首跳写入=1");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc1：tick（NextRounds ×1）→ 对手攻击 miss
        CHECK_EQ(s1416(ctx, 0, 6), 2, "T8580.n rc1 tick 写入=2（{1}=2 → 单 tick）");
        CHECK_EQ(onstage_slot_pp(b, 1, 1), attacker_pet(2, "x", 1, 100).skills[0].maxPP - 1,
                 "T8580.n rc1 对手攻击确实发生（PP 正控制）");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8580.n rc1 仍闪避");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc2：窗过期 → 落地
        CHECK_EQ(s1416(ctx, 0, 6), 2, "T8580.n rc2 无第 3 次写入");
        CHECK(onstage_hp(b, 0) < kMaxHp, "T8580.n rc2 攻击落地（窗过期）");
        std::cout << "  [T8580.n] 1416 {1}=2 tick 界（38490 真实载体） 通过\n";
    }

    // ══ T8580.o 1416 重施放卫生（另宠再携 1416 施放 → voucher 刷新不叠加）══
    //    side0 换宠后新宠（同 lead 构造、带 35648）再自爆：旧 watcher 先摘 →
    //    单 voucher/单 watcher（双 watcher 会让登场消费双跳、slot6 计数翻倍）。
    {
        sbs_srand(115);
        ScenarioBattle b(
            party_of(override_pet(1, "自爆手", kC35648, 1416, kM4326, {{2, 100}}, {}, 100, 1,
                                  kMaxHp)),
            party_of(attacker_pet(2, "空过手", 200, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：第一只自爆 → 换宠（voucher 消费，窗挂新宠）
        CHECK_EQ(s1416(ctx, 0, 3), 1, "T8580.o 第一次施放");
        CHECK_EQ(s1416(ctx, 0, 4), 2, "T8580.o 第一只 voucher 已消费");
        refill_pp(b, 1);
        refill_pp(b, 0);
        // rc1：新宠（同款带 1416）再自爆 → 第二次 voucher（旧 watcher 摘除后重挂）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(s1416(ctx, 0, 3), 2, "T8580.o 第二次施放计数=2");
        CHECK_EQ(s1416(ctx, 0, 4), 2, "T8580.o 新 voucher 待命→登场即消费");
        CHECK_EQ(s1416(ctx, 0, 5), 1, "T8580.o 接续回合=rc1");
        std::cout << "  [T8580.o] 1416 重施放卫生 通过\n";
    }

    std::cout << "scenario_8580_k471 全部通过" << std::endl;
    return 0;
}
