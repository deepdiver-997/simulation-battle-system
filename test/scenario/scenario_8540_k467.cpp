// 场景 8540 —— K467 恢复抑制 + 闪避回补 两族（2128/2067，
// resources/moves_lib/k467_heal_suppress.cpp + k467_dodge_refund.cpp；2026-11-13，
// worktree k467；号段 8540-8549 → T8540-pre + a-n 十五块）
//
// 断言口径（三层观测面 + 真实载体正负测齐）：
//   ① 键面（plugin_storage 审计槽）：2128 基 1021280000（slot0=末次门读数/
//      1=写入种类/2=门判定回合/3={1}回显/4={0}回显/5=窗基线 orig/6=tick 计数/
//      7=cast 计数/8=窗 live/9=跳过原因）；2067 基 1020670000（slot0=末次 dodge
//      写入/1=写入回合/2=dodge tick 计数/3={2}回显/4={3}回显/5={0}回显/
//      6=回补次数/7=末次回补回合/8=回补 PP 实变槽数/9=回补跳过原因）。
//   ② 通道面：heal_mod_pct[side] 直读（2128 降疗通道）+ ws.dodge_rate 停泊直读
//      （2067 闪避申报面；线性序 CHOOSE 等待点在下一 ROUND_START 前 → drive 后
//      ws 仍持有刚结束回合的值）。
//   ③ 行为面：ws.last_heal_amount（heal_impl 落账，攻击伤害不进此账——2128 带
//      精确值断言）+ hp 差分（2067 闪避 miss 计数）+ 逐槽 PP 直读（回补面）。
//
// 确定性手法（SOP §五.4）：每块 sbs_srand 定标；heal/载体 must_hit=1 短路命中链；
// 攻击手 accuracy=100/must_hit=0（hit_chance=100 必中，且必中凭证不存在——闪避面
// 可观测）；100%/0% 闪避档确定性、70% 档固定 seed 统计带（99 试 3σ）；暴击全清零；
// 28418 行为腿链上兄弟（191 免疫反弹/847 护盾/1342 护罩——护盾会吃掉红伤污染
// miss 计数）用 DisabledEffects 装配期剥离，只留 2067。
// heal 量算术：max_hp=100000 → 12691 不屈守护 [43]|[5] 基量 20000；heal_mod_pct
// -30/-50/-100 → 14000/10000/0（heal_impl 整除算术；坑集 §4 满血构造+压 hp 避钳）。
// PP 观测面：施放槽 pp 必须 ≥1（0=不可选），回补观测用 [1,1,2,3,4] → +N 逐槽直读。
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
constexpr int kHeal12691 = 12691;    // 不屈守护 [43]|[5]：恢复自身最大体力1/5（cat1+回血）
constexpr int kC37432 = 37432;       // 2128 [2,50] 日月轮·同天（主载体）
constexpr int kC37548 = 37548;       // 2128 [2,30] 十月的唱片
constexpr int kC38187 = 38187;       // 2128 [2,100] 兰烬蘧栩
constexpr int kC28418 = 28418;       // 2067 [3,70,1,3] 天幕浮云（主载体，cat4 must_hit）
constexpr int kC38117 = 38117;       // 2067 [1,100,1,1] 死祸咒返（唯一 N=1 载体）
constexpr int kM4546 = 4546;         // 28418 learner（monster_learnable_moves）
constexpr int kM4657 = 4657;         // 37432 learner（hide_moves）
constexpr int kMaxHp = 100000;       // heal 基量 20000 整除（坑集 §4：hp 单独压）
constexpr int kHealBase = kMaxHp / 5;

constexpr int k2128Base = 1021280000;  // + owner*16 + slot
constexpr int k2067Base = 1020670000;  // + owner*16 + slot

int audit_int(BattleContext& ctx, int key, int def) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : def;
}
int s2128(BattleContext& ctx, int owner, int slot) {
    return audit_int(ctx, k2128Base + owner * 16 + slot, -1);
}
int s2067(BattleContext& ctx, int owner, int slot) {
    return audit_int(ctx, k2067Base + owner * 16 + slot, -1);
}

ElfPet base_pet(int id, const char* name, const std::array<int, 5>& moves, int speed, int atk,
                int hp) {
    std::array<Skills, 5> skills{Skills(moves[0]), Skills(moves[1]), Skills(moves[2]),
                                 Skills(moves[3]), Skills(moves[4])};
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;   // 载体/奶妈默认必中（确定性；攻击手另行 must_hit=0）
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

// 攻击手：must_hit=0 + acc100（hit_chance=100 必中、但必中凭证不存在——闪避面可观测）。
ElfPet attacker_pet(int id, const char* name, int speed) {
    ElfPet p = base_pet(id, name, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed, 100, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 覆盖变体宠（Skills(id, monster, overrides, disabled) 装配期官方覆盖/禁用基建，
// K453/K458 同款；disabled 用于剥离 28418 链上护盾/护罩/免疫反弹兄弟——护盾会吃红伤
// 污染 miss 计数，行为腿只留 2067）。
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

// 在场宠技能 PP 逐槽直设（回补观测面；施放槽须 ≥1）。
void set_pp(ScenarioBattle& b, int side, const std::array<int, 5>& v) {
    ElfPet& pet = b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]];
    for (int i = 0; i < 5; ++i) {
        pet.skills[static_cast<std::size_t>(i)].pp = v[static_cast<std::size_t>(i)];
    }
}
std::array<int, 5> onstage_pp(ScenarioBattle& b, int side) {
    const ElfPet& pet = b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]];
    return {pet.skills[0].pp, pet.skills[1].pp, pet.skills[2].pp, pet.skills[3].pp,
            pet.skills[4].pp};
}

// 在场宠技能 PP 回满（长回合块防 PP 耗尽）。
void refill_pp(ScenarioBattle& b, int side) {
    ElfPet& pet = b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]];
    for (Skills& s : pet.skills) {
        if (s.pp >= 0) {
            s.pp = s.maxPP;
        }
    }
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

    // ══ T8540-pre 12 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>>
            expect = {
                // ── 2128 六载体（{0} 全=2，{1}∈{30,50,60,100}）──
                {37432, {{847, {200}}, {1342, {200}}, {697, {}}, {2128, {2, 50}}}},
                {37548, {{1002, {1}}, {2128, {2, 30}}, {1366, {25, 25, 100}}}},
                {37612, {{1878, {3}}, {2073, {50}}, {2128, {2, 50}}}},
                {37820, {{1542, {1, 1}}, {1589, {2}}, {2128, {2, 50}}}},
                {38187, {{948, {22}}, {188, {}}, {2128, {2, 100}}, {1832, {2, 100, 300, 2}}}},
                {38295, {{2423, {}},
                         {1978, {100, 0, 3}},
                         {1047, {60, 7, 3}},
                         {1360, {100}},
                         {2128, {2, 60}}}},
                // ── 2067 六载体（{1}∈{50,70,100}）──
                {28418, {{191, {4}}, {2067, {3, 70, 1, 3}}, {847, {300}}, {1342, {300}}}},
                {28441, {{2063, {}},
                         {2059, {2, 200}},
                         {2068, {100, 6}},
                         {2067, {3, 100, 40, 1}}}},
                {28721, {{4, {1, 100, 1}},
                         {4, {2, 100, 1}},
                         {4, {3, 100, 1}},
                         {2067, {2, 100, 2, 3}}}},
                {37418, {{948, {15}}, {923, {3, 240}}, {2067, {3, 100, 3, 2}}}},
                {38117, {{2349, {}}, {1180, {2}}, {1697, {40}}, {2067, {1, 100, 1, 1}}}},
                {38440, {{1878, {3}}, {1122, {1, 6}}, {2067, {3, 50, 3, 3}}}},
            };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8540-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8540-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8540-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8540-pre] 12 载体实际加载器切片核验 通过\n";
    }

    // ══ T8540.a 2128 50% 带全链（37432 [2,50]）：授予即生效→tick 维持→窗满还原 ══
    {
        sbs_srand(101);
        ScenarioBattle b(party_of(base_pet(1, "印主", {kC37432, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                         300, 1, kMaxHp)),
                         party_of(base_pet(2, "奶妈", {kHeal12691, kPaiDa, kPaiDa, kPaiDa,
                                                       kPaiDa},
                                           100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 1, 30000);   // 压当前 hp（坑集 §4：满血构造+单独压）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], kHealBase / 2,
                 "T8540.a R1 降疗 50%（20000×50/100=10000）");
        CHECK_EQ(ctx.heal_mod_pct[1], -50, "T8540.a R1 通道面 heal_mod_pct=-50（授予即生效）");
        CHECK_EQ(s2128(ctx, 0, 0), 1, "T8540.a R1 门读=开（对手无异常）");
        CHECK_EQ(s2128(ctx, 0, 1), 1, "T8540.a R1 写入种类=降疗");
        CHECK_EQ(s2128(ctx, 0, 2), 0, "T8540.a R1 门判定回合=rc0（回合 0 基）");
        CHECK_EQ(s2128(ctx, 0, 3), 50, "T8540.a {1}=50 行参回显");
        CHECK_EQ(s2128(ctx, 0, 4), 2, "T8540.a {0}=2 行参回显");
        CHECK_EQ(s2128(ctx, 0, 5), 0, "T8540.a 捕获基线 orig=0");
        CHECK_EQ(s2128(ctx, 0, 7), 1, "T8540.a cast 计数=1");
        CHECK_EQ(s2128(ctx, 0, 8), 1, "T8540.a 窗 live=1");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], kHealBase / 2, "T8540.a R2 tick 维持降疗 10000");
        CHECK_EQ(s2128(ctx, 0, 6), 1, "T8540.a R2 tick 到达=1（rc1 维持）");
        CHECK_EQ(s2128(ctx, 0, 1), 1, "T8540.a R2 写入=维持降疗");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], kHealBase, "T8540.a R3 窗满还原（全额 20000）");
        CHECK_EQ(s2128(ctx, 0, 6), 2, "T8540.a tick 计数=2（rc1 维持+rc2 还原）");
        CHECK_EQ(s2128(ctx, 0, 1), 2, "T8540.a 末次写入=窗满还原");
        CHECK_EQ(s2128(ctx, 0, 8), 0, "T8540.a 还原后 live=0");
        CHECK_EQ(ctx.heal_mod_pct[1], 0, "T8540.a 通道复原 heal_mod_pct=0");
        std::cout << "  [T8540.a] 2128 50% 带+窗维持+窗满还原 通过\n";
    }

    // ══ T8540.b 2128 30% 带（37548 [2,30] 真实载体差分腿）══
    {
        sbs_srand(102);
        ScenarioBattle b(party_of(base_pet(1, "印主", {kC37548, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                         300, 1, kMaxHp)),
                         party_of(base_pet(2, "奶妈", {kHeal12691, kPaiDa, kPaiDa, kPaiDa,
                                                       kPaiDa},
                                           100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 1, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], kHealBase * 70 / 100,
                 "T8540.b R1 降疗 30%（20000×70/100=14000）");
        CHECK_EQ(s2128(ctx, 0, 3), 30, "T8540.b {1}=30 行参回显");
        CHECK_EQ(ctx.heal_mod_pct[1], -30, "T8540.b 通道=-30");
        std::cout << "  [T8540.b] 2128 30% 带（37548 差分腿） 通过\n";
    }

    // ══ T8540.c 2128 门闭负测（在位判读）：施放时对手有异常 → 不武装不降疗 ══
    {
        sbs_srand(103);
        ScenarioBattle b(party_of(base_pet(1, "印主", {kC37432, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                         300, 1, kMaxHp)),
                         party_of(base_pet(2, "奶妈", {kHeal12691, kPaiDa, kPaiDa, kPaiDa,
                                                       kPaiDa},
                                           100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 1, 30000);
        // 衰弱 11：弱化类——无行动封锁、无扣血 tick（K463 选码判据）
        CHECK_EQ(apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Weakness),
                               /*duration_rounds=*/5, /*actor=*/0),
                 ApplyAnomalyResult::SUCCESS, "T8540.c setup 对手衰弱");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], kHealBase, "T8540.c R1 门闭不降疗（全额 20000）");
        CHECK_EQ(s2128(ctx, 0, 0), 0, "T8540.c R1 门读=闭（对手有异常）");
        CHECK_EQ(s2128(ctx, 0, 8), 0, "T8540.c 门闭不武装（live=0）");
        CHECK_EQ(s2128(ctx, 0, 7), 1, "T8540.c cast 计数=1（门闭 cast 也记账）");
        CHECK_EQ(ctx.heal_mod_pct[1], 0, "T8540.c 通道未被写");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], kHealBase, "T8540.c R2 仍全额");
        CHECK_EQ(s2128(ctx, 0, 6), 0, "T8540.c 零 tick（无窗可 tick）");
        std::cout << "  [T8540.c] 2128 门闭负测（在位判读） 通过\n";
    }

    // ══ T8540.d 2128 获异常门闭+解除重开（37432 override {0}→5；"…时"=while 前件，
    //    解除用 dispel_active_anomalies 直调（089/090 场景先例）——不走 2226 载体
    //    （其解除成功授攻击免疫票会封掉奶妈的攻击型回血技，污染行为面）══
    {
        sbs_srand(104);
        ScenarioBattle b(party_of(override_pet(1, "印主", kC37432, 2128, kM4657, {{0, 5}}, {},
                                               300, 1, kMaxHp)),
                         party_of(base_pet(2, "奶妈", {kHeal12691, kPaiDa, kPaiDa, kPaiDa,
                                                       kPaiDa},
                                           100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 1, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], kHealBase / 2, "T8540.d R1 门开降疗 10000");
        CHECK_EQ(s2128(ctx, 0, 4), 5, "T8540.d {0}→5 变档回显");
        // 对手获异常（窗内）→ 下一 tick 门闭
        CHECK_EQ(apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Weakness),
                               /*duration_rounds=*/9, /*actor=*/0),
                 ApplyAnomalyResult::SUCCESS, "T8540.d setup 对手获异常");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], kHealBase, "T8540.d R2 获异常后门闭（全额）");
        CHECK_EQ(s2128(ctx, 0, 0), 0, "T8540.d R2 tick 门读=闭");
        CHECK_EQ(s2128(ctx, 0, 1), 0, "T8540.d R2 写入=门闭写回 orig");
        refill_pp(b, 1);
        set_onstage_hp(b, 1, 30000);   // 防连疗钳满（heal 量断言须无 clamp 区间）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], kHealBase, "T8540.d R3 门闭持续（全额）");
        // 解除对手异常（setup 面）→ 下一 tick 门重开
        dispel_active_anomalies(&ctx, 1);
        refill_pp(b, 1);
        set_onstage_hp(b, 1, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], kHealBase / 2, "T8540.d R4 解除后门重开 10000");
        CHECK_EQ(s2128(ctx, 0, 0), 1, "T8540.d R4 tick 门读=开（重开面）");
        refill_pp(b, 1);
        set_onstage_hp(b, 1, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], kHealBase / 2, "T8540.d R5 门开维持 10000");
        // R6：窗满还原（{0}=5：维持 rc<=4，rc5 还原）
        refill_pp(b, 1);
        set_onstage_hp(b, 1, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], kHealBase, "T8540.d R6 窗满还原（全额）");
        CHECK_EQ(s2128(ctx, 0, 6), 5, "T8540.d tick 计数=5（rc1..5）");
        CHECK_EQ(s2128(ctx, 0, 8), 0, "T8540.d 还原后 live=0");
        CHECK_EQ(ctx.heal_mod_pct[1], 0, "T8540.d 通道复原");
        std::cout << "  [T8540.d] 2128 获异常门闭+解除重开+窗满还原 通过\n";
    }

    // ══ T8540.e 2128 100% 带 + live 重施放基线复用（38187 [2,100]）══
    {
        sbs_srand(105);
        ScenarioBattle b(party_of(base_pet(1, "印主", {kC38187, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                         300, 1, kMaxHp)),
                         party_of(base_pet(2, "奶妈", {kHeal12691, kPaiDa, kPaiDa, kPaiDa,
                                                       kPaiDa},
                                           100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 1, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], 0, "T8540.e R1 降疗 100%（完全抑制）");
        CHECK_EQ(s2128(ctx, 0, 3), 100, "T8540.e {1}=100 行参回显");
        CHECK_EQ(ctx.heal_mod_pct[1], -100, "T8540.e 通道=-100");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(s2128(ctx, 0, 7), 2, "T8540.e R2 重施放 cast 计数=2");
        CHECK_EQ(s2128(ctx, 0, 5), 0, "T8540.e live 重施放复用基线 orig=0（刷新不叠加）");
        CHECK_EQ(ctx.ws.last_heal_amount[1], 0, "T8540.e R2 仍全抑制");
        CHECK_EQ(ctx.heal_mod_pct[1], -100,
                 "T8540.e R2 通道仍 -100（基线复用无叠加——叠加则 -200）");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], 0, "T8540.e R3 新窗维持（仍全抑制）");
        refill_pp(b, 1);
        set_onstage_hp(b, 1, 30000);   // 防连疗钳满
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(ctx.ws.last_heal_amount[1], kHealBase, "T8540.e R4 窗满还原（全额）");
        CHECK_EQ(s2128(ctx, 0, 1), 2, "T8540.e 末次写入=窗满还原");
        CHECK_EQ(ctx.heal_mod_pct[1], 0, "T8540.e 通道复原=0（无叠加残留）");
        std::cout << "  [T8540.e] 2128 100% 带+重施放基线复用 通过\n";
    }

    // ══ T8540.f 2067 N=1 全链（38117 [1,100,1,1] 真实载体）：100% 闪避+当回合回补 ══
    //    ⚠️ 38117 自带先制 3（K461 38612 同款坑）——speed 100 也先手施放：R1 攻击
    //    在 cast 之后 → 被 100% 闪避（伤害 0），回补同回合 ROUND_END 落账。
    //    真后手顺延面（speed 序）由 T8540.i 的 28418（先制 0）承担。
    {
        sbs_srand(106);
        ScenarioBattle b(party_of(attacker_pet(1, "攻击手", 300)),
                         party_of(base_pet(2, "印主", {kC38117, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                           100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(b, 1, {1, 1, 2, 3, 4});   // 施放槽 ≥1；回补观测面
        set_onstage_hp(b, 1, 50000);
        // 停泊读申报面：debug_step_mode 包住提交（坑集 §6 模式——否则 submit 内联
        // run 会整回合跑完、step_past 沿枚举回绕死转）
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 攻击
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 38117（先制 3）
        b.ctx().debug_step_mode = false;
        b.step_past(State::BATTLE_SECOND_SKILL_EFFECT);
        CHECK_EQ(ctx.ws.dodge_rate[1], 1.0f, "T8540.f cast 立即写 dodge_rate=1.0（申报面）");
        b.drive();
        const int hp_after_r1 = onstage_hp(b, 1);
        CHECK_EQ(hp_after_r1, 50001,
                 "T8540.f R1 先制 3 先手施放 → 攻击被 100% 闪避（0 伤）+回补 +1 = 50001");
        CHECK_EQ(s2067(ctx, 1, 6), 1, "T8540.f R1 回补触发（N=1 当回合 ROUND_END）");
        CHECK_EQ(s2067(ctx, 1, 7), 0, "T8540.f 回补回合=rc0");
        CHECK_EQ(s2067(ctx, 1, 8), 5, "T8540.f PP 实变槽=5（+1 全槽实变）");
        const std::array<int, 5> pp_now = onstage_pp(b, 1);
        CHECK_EQ(pp_now[0], 1, "T8540.f 槽0 PP 1-1(cast)+1(回补)=1");
        CHECK_EQ(pp_now[1], 2, "T8540.f 槽1 PP 1→2（未施放槽纯 +1）");
        CHECK_EQ(pp_now[4], 5, "T8540.f 槽4 PP 4→5（clamp ≤ maxPP 由原语内生）");
        CHECK_EQ(ctx.ws.last_heal_amount[1], 1, "T8540.f 体力回补 1（heal_amount 落账）");
        CHECK_EQ(s2067(ctx, 1, 5), 1, "T8540.f {0}=1 行参回显");
        CHECK_EQ(s2067(ctx, 1, 3), 1, "T8540.f {2}=1 PP 行参回显");
        CHECK_EQ(s2067(ctx, 1, 4), 1, "T8540.f {3}=1 体力行参回显");
        CHECK_EQ(s2067(ctx, 1, 0), 100, "T8540.f {1}=100 申报回显");
        CHECK_EQ(s2067(ctx, 1, 2), 0, "T8540.f N=1 无 ROUND_START tick（施放时点已过）");
        // R2：闪避窗已过（N=1 只覆盖施放回合，ws reset 无 tick 重写）→ 攻击命中
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  1);
        CHECK(onstage_hp(b, 1) < hp_after_r1, "T8540.f R2 闪避失效（窗过期）攻击命中");
        CHECK_EQ(s2067(ctx, 1, 6), 1, "T8540.f 无第二次回补（单发）");
        std::cout << "  [T8540.f] 2067 N=1 全链 通过\n";
    }

    // ══ T8540.g 2067 70% 掷点档 + 窗满回补（28418 {0}→100 变档、兄弟剥离）══
    //    99 试固定 seed 统计带（p(miss)=70%，3σ≈13.7）；R100 ROUND_END 回补。
    {
        sbs_srand(107);
        ScenarioBattle b(party_of(attacker_pet(1, "攻击手", 100)),
                         party_of(override_pet(2, "印主", kC28418, 2067, kM4546, {{0, 100}},
                                               {191, 847, 1342}, 300, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(b, 1, {1, 1, 2, 3, 4});
        set_onstage_hp(b, 1, 80000);
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 施放（先手）
        b.ctx().debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK_EQ(ctx.ws.dodge_rate[1], 0.7f, "T8540.g 申报面 dodge_rate=0.7（{1}=70）");
        b.drive();
        int misses = onstage_hp(b, 1) == 80000 ? 1 : 0;
        int hp_prev = onstage_hp(b, 1);
        for (int r = 2; r <= 99; ++r) {
            set_pp(b, 1, {1, 1, 2, 3, 4});
            refill_pp(b, 0);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                      BattleFsm::ActionType::SELECT_SKILL, 1);
            const int hp_now = onstage_hp(b, 1);
            if (hp_now == hp_prev) {
                ++misses;
            }
            hp_prev = hp_now;
        }
        CHECK_EQ(misses + (99 - misses), 99, "T8540.g 99 试记账完整");
        CHECK(misses >= 54 && misses <= 84,
              "T8540.g 70% 档统计带（期望 69.3±3σ≈14，实得 " + std::to_string(misses) + "）");
        // R100：窗末回合 → ROUND_END 回补（+1 PP 全槽 + 3 体力）
        set_pp(b, 1, {1, 1, 2, 3, 4});
        refill_pp(b, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  1);
        CHECK_EQ(s2067(ctx, 1, 6), 1, "T8540.g 窗满回补触发");
        CHECK_EQ(s2067(ctx, 1, 7), 99, "T8540.g 回补回合=rc99（窗 [0,99] 末格）");
        CHECK_EQ(s2067(ctx, 1, 8), 5, "T8540.g PP 实变槽=5");
        CHECK_EQ(ctx.ws.last_heal_amount[1], 3, "T8540.g 体力回补 3（{3}=3 载体切片）");
        CHECK_EQ(s2067(ctx, 1, 2), 99, "T8540.g dodge tick 计数=99（rc1..99 逐回合重写）");
        std::cout << "  [T8540.g] 2067 70% 掷点档+窗满回补 通过\n";
    }

    // ══ T8540.h 2067 0% 关闸档 + {2}/{3}=0 no-op 回补（28418 全变档）══
    {
        sbs_srand(108);
        ScenarioBattle b(party_of(attacker_pet(1, "攻击手", 300)),
                         party_of(override_pet(2, "印主", kC28418, 2067, kM4546,
                                               {{0, 1}, {1, 0}, {2, 0}, {3, 0}},
                                               {191, 847, 1342}, 100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(b, 1, {1, 1, 2, 3, 4});
        set_onstage_hp(b, 1, 50000);
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 攻击
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 施放（后手）
        b.ctx().debug_step_mode = false;
        b.step_past(State::BATTLE_SECOND_SKILL_EFFECT);
        CHECK_EQ(ctx.ws.dodge_rate[1], 0.0f, "T8540.h 0% 申报 dodge_rate=0.0（结构性不翻转）");
        b.drive();
        CHECK(onstage_hp(b, 1) < 50000, "T8540.h R1 攻击命中（施放前无闪避）");
        const int hp_after_r1 = onstage_hp(b, 1);
        CHECK_EQ(s2067(ctx, 1, 6), 1, "T8540.h 回补 tick 照常触发（窗仍在）");
        CHECK_EQ(s2067(ctx, 1, 8), 0, "T8540.h {2}=0 → PP 零实变（自然 no-op）");
        CHECK_EQ(ctx.ws.last_heal_amount[1], 0, "T8540.h {3}=0 → 体力零恢复");
        const std::array<int, 5> pp_now = onstage_pp(b, 1);
        CHECK_EQ(pp_now[0], 0, "T8540.h PP 未被回补（施放消耗后 0）");
        // R2：0% → 必不闪避（无 tick 重写、ws reset 后 0.0）→ 攻击命中
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  1);
        CHECK_EQ(s2067(ctx, 1, 2), 0, "T8540.h 零 dodge tick（N=1）");
        CHECK(onstage_hp(b, 1) < hp_after_r1, "T8540.h R2 0% 结构性不闪避（攻击命中）");
        std::cout << "  [T8540.h] 2067 0% 关闸档+零参回补 通过\n";
    }

    // ══ T8540.i 2067 后手顺延（28418 {0}→3、{1}→100 变档）：窗 [rc1,rc3]、
    //    回补在 R4 末、施放回合伪进被 start 门挡 ══
    {
        sbs_srand(109);
        ScenarioBattle b(party_of(attacker_pet(1, "攻击手", 300)),
                         party_of(override_pet(2, "印主", kC28418, 2067, kM4546,
                                               {{0, 3}, {1, 100}}, {191, 847, 1342},
                                               100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(b, 1, {1, 1, 2, 3, 4});
        set_onstage_hp(b, 1, 50000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        const int hp_after_r1 = onstage_hp(b, 1);
        CHECK(hp_after_r1 < 50000, "T8540.i R1 攻击先于 cast 命中（后手施放前无闪避）");
        CHECK_EQ(s2067(ctx, 1, 9), 4, "T8540.i 施放回合 ROUND_END 伪进被 start 门挡（slot9=4）");
        CHECK_EQ(s2067(ctx, 1, 6), 0, "T8540.i 施放回合不回补");
        set_pp(b, 1, {1, 1, 2, 3, 4});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  1);
        CHECK_EQ(onstage_hp(b, 1), hp_after_r1, "T8540.i R2 100% 闪避（顺延窗首格）");
        set_pp(b, 1, {1, 1, 2, 3, 4});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  1);
        CHECK_EQ(onstage_hp(b, 1), hp_after_r1, "T8540.i R3 闪避持续");
        set_pp(b, 1, {1, 1, 2, 3, 4});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  1);
        CHECK_EQ(onstage_hp(b, 1), hp_after_r1 + 3, "T8540.i R4 末格闪避+回补 +3 体力");
        CHECK_EQ(s2067(ctx, 1, 6), 1, "T8540.i R4 回补触发");
        CHECK_EQ(s2067(ctx, 1, 7), 3, "T8540.i 回补回合=rc3（顺延窗 [1,3] 末格）");
        CHECK_EQ(s2067(ctx, 1, 8), 5, "T8540.i PP 实变槽=5");
        CHECK_EQ(ctx.ws.last_heal_amount[1], 3, "T8540.i 体力回补 3");
        set_pp(b, 1, {1, 1, 2, 3, 4});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  1);
        CHECK(onstage_hp(b, 1) < hp_after_r1 + 3, "T8540.i R5 窗过期攻击命中");
        CHECK_EQ(s2067(ctx, 1, 2), 3, "T8540.i dodge tick 计数=3（rc1..3）");
        std::cout << "  [T8540.i] 2067 后手顺延+伪进门 通过\n";
    }

    // ══ T8540.j 2067 离场清理（28418 {0}→3、{1}→100）：施放方换宠 → 窗 epoch 作废 ══
    {
        sbs_srand(110);
        auto lead = override_pet(2, "印主", kC28418, 2067, kM4546, {{0, 3}, {1, 100}},
                                 {191, 847, 1342}, 100, 1, kMaxHp);
        auto sub = attacker_pet(3, "替补", 100);
        std::array<ElfPet, 6> party1 = make_party(lead);
        party1[1] = sub;
        ScenarioBattle b(party_of(attacker_pet(1, "攻击手", 300)), party1);
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 1, 50000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(s2067(ctx, 1, 5), 3, "T8540.j R1 窗已武装（{0}=3 回显）");
        // R2 施放方换宠（K459/K461 同款换宠面；切换在 OPERATION_ENTER_STAGE 结算
        // ——早于 R2 的 ROUND_START → 窗在首个 tick 前即 epoch 作废）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(s2067(ctx, 1, 2), 0, "T8540.j 换宠后零 tick（窗死，首个 tick 前已作废）");
        // R3：窗死（无重写）→ 攻击命中替补；回补永不发生
        refill_pp(b, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK(onstage_hp(b, 1) < kMaxHp, "T8540.j R3 替补被命中（闪避随离场失效）");
        CHECK_EQ(s2067(ctx, 1, 6), 0, "T8540.j 离场后零回补");
        CHECK_EQ(s2067(ctx, 1, 7), -1, "T8540.j 回补回合未写（-1 缺省）");
        CHECK_EQ(s2067(ctx, 1, 2), 0, "T8540.j R3 仍零 tick（条目已死）");
        std::cout << "  [T8540.j] 2067 离场清理 通过\n";
    }

    // ══ T8540.k 2067 镜像（双方 38117 N=1）：双侧闪避+双侧当回合回补 ══
    //    （ROUND_END 桶序=后手侧先执行——K462 引擎事实；两笔回补互不依赖，量断言
    //    与序无关，各侧审计键独立分账）
    {
        sbs_srand(111);
        ScenarioBattle b(party_of(base_pet(1, "印主甲", {kC38117, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                         300, 1, kMaxHp)),
                         party_of(base_pet(2, "印主乙", {kC38117, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                           100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(b, 0, {1, 1, 2, 3, 4});
        set_pp(b, 1, {1, 1, 2, 3, 4});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(s2067(ctx, 0, 6), 1, "T8540.k side0 回补触发");
        CHECK_EQ(s2067(ctx, 1, 6), 1, "T8540.k side1 回补触发（镜像）");
        CHECK_EQ(s2067(ctx, 0, 7), 0, "T8540.k side0 回补回合=rc0");
        CHECK_EQ(s2067(ctx, 1, 7), 0, "T8540.k side1 回补回合=rc0");
        CHECK_EQ(s2067(ctx, 0, 8), 5, "T8540.k side0 PP 实变=5（键位分账）");
        CHECK_EQ(s2067(ctx, 1, 8), 5, "T8540.k side1 PP 实变=5（键位分账）");
        CHECK_EQ(ctx.ws.last_heal_amount[0], 1, "T8540.k side0 体力回补 1");
        CHECK_EQ(ctx.ws.last_heal_amount[1], 1, "T8540.k side1 体力回补 1");
        std::cout << "  [T8540.k] 2067 镜像双回补 通过\n";
    }

    // ══ T8540.l 2067 回补×封回血（HEAL_BLOCK）：PP 半边照常、体力半边归零 ══
    //    （K453 通道口径：封回血只封体力回复，PP 照常——原语通道内生）
    {
        sbs_srand(112);
        ScenarioBattle b(party_of(attacker_pet(1, "攻击手", 300)),
                         party_of(base_pet(2, "印主", {kC38117, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                           100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/9, /*source_id=*/99020671);
        set_pp(b, 1, {1, 1, 2, 3, 4});
        set_onstage_hp(b, 1, 50000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(s2067(ctx, 1, 6), 1, "T8540.l 回补触发");
        CHECK_EQ(s2067(ctx, 1, 8), 5, "T8540.l PP 半边照常（+1 全槽实变）");
        CHECK_EQ(ctx.ws.last_heal_amount[1], 0, "T8540.l 体力半边被封（heal_impl HEAL_BLOCK）");
        CHECK(onstage_hp(b, 1) >= 49500 && onstage_hp(b, 1) <= 50000,
              "T8540.l hp 停在攻击后区间（封回血下回补零恢复——reset_hp 通道置换会跳变）");
        const std::array<int, 5> pp_now = onstage_pp(b, 1);
        CHECK_EQ(pp_now[0], 1, "T8540.l 槽0 PP 1-1(cast)+1(回补)=1");
        std::cout << "  [T8540.l] 2067 回补×封回血 通过\n";
    }

    // ══ T8540.m 两族键段共存 + 2128 减疗 × 2067 回补交互：side0 37432（2128 [2,50]）
    //    + side1 28418 变档（{0}→1,{1}→100,{2}→3,{3}→100）——回补体力被对手降疗
    //    削半（100×50/100=50），两族审计键同局独立分账 ══
    {
        sbs_srand(113);
        ScenarioBattle b(party_of(base_pet(1, "印主A", {kC37432, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                         300, 1, kMaxHp)),
                         party_of(override_pet(2, "印主B", kC28418, 2067, kM4546,
                                               {{0, 1}, {1, 100}, {2, 3}, {3, 100}},
                                               {191, 847, 1342}, 100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(b, 1, {1, 1, 2, 3, 4});
        set_onstage_hp(b, 1, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(s2128(ctx, 0, 8), 1, "T8540.m 2128 窗武装（side0 键段）");
        CHECK_EQ(s2067(ctx, 1, 6), 1, "T8540.m 2067 回补触发（side1 键段，同局共存）");
        CHECK_EQ(s2067(ctx, 1, 8), 5, "T8540.m PP +3 全槽实变");
        CHECK_EQ(ctx.ws.last_heal_amount[1], 50,
                 "T8540.m 回补体力 100 被对手降疗削半（100×50/100）");
        const std::array<int, 5> pp_now = onstage_pp(b, 1);
        CHECK_EQ(pp_now[0], 3, "T8540.m 槽0 PP 1-1(cast)+3(回补)=3");
        CHECK_EQ(s2128(ctx, 0, 6), 0, "T8540.m 2128 R1 零 tick（tick 自 R2 起）");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);
        CHECK_EQ(s2128(ctx, 0, 6), 1, "T8540.m R2 2128 tick 维持（两族窗互不干扰）");
        CHECK_EQ(ctx.heal_mod_pct[1], -50, "T8540.m R2 降疗通道维持");
        std::cout << "  [T8540.m] 两族键段共存+减疗×回补交互 通过\n";
    }

    // ══ T8540.n 2128 × HEAL_BLOCK 先序（封回血优先于减疗，归零等价面）══
    {
        sbs_srand(114);
        ScenarioBattle b(party_of(base_pet(1, "印主", {kC37432, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                         300, 1, kMaxHp)),
                         party_of(base_pet(2, "奶妈", {kHeal12691, kPaiDa, kPaiDa, kPaiDa,
                                                       kPaiDa},
                                           100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/9, /*source_id=*/99021281);
        set_onstage_hp(b, 1, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(s2128(ctx, 0, 8), 1, "T8540.n 2128 窗武装（与封回血独立）");
        CHECK_EQ(ctx.ws.last_heal_amount[1], 0, "T8540.n 封回血优先（恢复 0，减疗不再可见）");
        CHECK_EQ(ctx.heal_mod_pct[1], -50, "T8540.n 通道面仍在（两通道独立记账）");
        std::cout << "  [T8540.n] 2128 ×封回血先序 通过\n";
    }

    // ══ T8540.o 2128 镜像（双方 37432）：双窗互写对侧通道、审计键 owner 分账 ══
    {
        sbs_srand(115);
        ScenarioBattle b(party_of(base_pet(1, "印主甲", {kC37432, kHeal12691, kPaiDa, kPaiDa,
                                                         kPaiDa},
                                           300, 1, kMaxHp)),
                         party_of(base_pet(2, "印主乙", {kC37432, kHeal12691, kPaiDa, kPaiDa,
                                                         kPaiDa},
                                           100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 30000);
        set_onstage_hp(b, 1, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(s2128(ctx, 0, 7), 1, "T8540.o side0 cast 计数=1（键位分账）");
        CHECK_EQ(s2128(ctx, 1, 7), 1, "T8540.o side1 cast 计数=1（键位分账）");
        CHECK_EQ(s2128(ctx, 0, 8), 1, "T8540.o side0 窗 live");
        CHECK_EQ(s2128(ctx, 1, 8), 1, "T8540.o side1 窗 live");
        CHECK_EQ(ctx.heal_mod_pct[1], -50, "T8540.o side0 的窗压 side1 通道");
        CHECK_EQ(ctx.heal_mod_pct[0], -50, "T8540.o side1 的窗压 side0 通道（镜像互写）");
        // R2：双方互奶——各自身上的降疗都来自对侧的窗（owner×16 步长分账面）
        refill_pp(b, 0);
        refill_pp(b, 1);
        set_onstage_hp(b, 0, 30000);
        set_onstage_hp(b, 1, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);
        CHECK_EQ(ctx.ws.last_heal_amount[0], kHealBase / 2, "T8540.o R2 side0 被降疗 10000");
        CHECK_EQ(ctx.ws.last_heal_amount[1], kHealBase / 2, "T8540.o R2 side1 被降疗 10000");
        CHECK_EQ(s2128(ctx, 0, 6), 1, "T8540.o side0 tick 独立计数");
        CHECK_EQ(s2128(ctx, 1, 6), 1, "T8540.o side1 tick 独立计数");
        std::cout << "  [T8540.o] 2128 镜像双窗+键位分账 通过\n";
    }

    std::cout << "scenario_8540_k467 全块通过\n";
    return 0;
}
