// 场景 8800 —— K492 族一 2039（resources/moves_lib/k492_boostlost_punish_and_invalid_react.cpp）
//（2026-11-23，worktree k492；号段 8800-8809 → T8800-pre/a-h）
//
// 断言口径（真实载体 + 审计槽 + 结构性正负测三线并锁）：
//   - 2039 = "{0}回合内若自身能力提升状态被消除或吸取则{1}%令对手{2}，未触发则免疫
//     下{3}次对手的攻击"。消强观测窗 = EVENT_STAT_BOOSTS_LOST（kind 双吃：0=消除/
//     1=吸取，模板文"或吸取"字面——与 711 严格档反向）+ InRounds 窗（后手顺延定例）；
//   - 回退 = ImmunityType::DAMAGE 次数票（570 直系：查询命中即扣、扣完注销），授予
//     时点 = 窗全闭后首格（expire ROUND_END）——"未触发则"须窗全闭才可判；
//   - 正负测：kind0/kind1 双吃、窗外事件拒收、触发 void 回退（{1}=0 掷歪不改判）、
//     票逐次消耗、离场清理（换宠 epoch 作废——窗与回退双死）、{2}/{0} 行参差分。
//   - rand 记账：{1}=100 申报 >=100 短路零抽、868/948 无概率掷点、攻击伤害浮动
//     每刀 1 抽；断言全为结构面（异常在位、hp 变/不变、审计槽精确、免疫事件计数），
//     不依赖伤害数值 → 无 RandMirror 需求。
#include <abnormal-system/abnormal-types.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>

#include <array>
#include <iostream>
#include <vector>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物40 无副作用（填充/受击面）
constexpr int kC29426 = 29426;     // 修罗炼狱 [962,1855,2039,1926,843] cat4 —— {0}=4,{1}=100,{2}=11 衰弱,{3}=1（主载体）
constexpr int kC28884 = 28884;     // 千年遗梦 [48,792,2039,2238] cat4 —— {0}=3,{1}=100,{2}=0 麻痹,{3}=1
constexpr int kShangGu = 19150;    // 上古咒术 [868]=[2] cat2 p85 prio3 —— 消强源（kind=0）
constexpr int kAnYan = 19877;      // 暗焰角袭 [948]=[2] cat1 p85 prio3 —— 吸取源（kind=1 transfer）
constexpr int kJianWu = 20010;     // 剑舞 [4]=[0,100,2] cat4 —— 自身提升源（离场清理腿）
constexpr int kLearner29426 = 4933;  // 修罗炼狱学习者（override 构造）
constexpr int kMaxHp = 99999;

constexpr int k2039Base = 1020390000;   // 与插件头注审计基址逐字一致

int slot(const BattleContext& ctx, int base, int owner, int idx) {
    const auto it = ctx.plugin_storage.find(base + owner * 16 + idx);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

// 通用宠构造（k480/k484 skill_pet 同款）：全技能 crit0+acc100+must_hit。
ElfPet skill_pet(int id, const char* name, const std::array<int, 5>& skill_ids, int speed,
                 int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(skill_ids[0]), Skills(skill_ids[1]), Skills(skill_ids[2]),
        Skills(skill_ids[3]), Skills(skill_ids[4]),
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

// 覆盖变体宠（Skills(id, monster, overrides) 装配期官方覆盖基建，K480 同款）。
ElfPet override_pet(int id, const char* name, int carrier, int monster_id, int effect_id,
                    int arg_index, int value, const std::array<int, 5>& fillers, int speed,
                    int atk, int hp) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(monster_id);
    if (!monster) {
        std::cerr << "override_pet: monster " << monster_id << " load failed" << std::endl;
        std::abort();
    }
    std::vector<EffectParamOverride> overrides;
    overrides.push_back({carrier, effect_id, arg_index, value});
    std::array<Skills, 5> skills{
        Skills(carrier, *monster, overrides), Skills(fillers[0]), Skills(fillers[1]),
        Skills(fillers[2]), Skills(fillers[3]),
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

// 一回合：双方提交操作（死切自动补位）。
void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        int replacement = -1;
        for (int slot = 0; slot < 6; ++slot) {
            if (slot != b.ctx().on_stage[dead] && b.ctx().seerRobot[dead].elfPets[slot].hp > 0) {
                replacement = slot;
                break;
            }
        }
        CHECK(replacement >= 0, "死后补位必须有存活的非在场精灵");
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET, replacement);
        b.drive();
    }
}

// 免疫票在位查询（K484 同款：rc 必须传该回合行动面实际生效值）。
bool damage_ticket_alive(const BattleContext& ctx, int side, int current_round) {
    return ctx.rule_center_.is_immune(side, static_cast<int>(ImmunityType::DAMAGE),
                                      coverage_all(), current_round);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8800-pre 4 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                {kC28884, {{48, {4}}, {792, {}}, {2039, {3, 100, 0, 1}}, {2238, {3, 2}}}},
                {kC29426, {{962, {1}}, {1855, {4, 2, 3}}, {2039, {4, 100, 11, 1}},
                           {1926, {2}}, {843, {2, 2}}}},
                {28373, {{191, {4}}, {2039, {3, 100, 15, 1}}, {2040, {3, 100, 5}}}},
                {29370, {{946, {1}}, {2076, {4, 3, 2, 170}}, {2039, {3, 100, 15, 1}},
                         {843, {2, 2}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8800-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8800-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8800-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8800-pre] 4 载体实际加载器切片核验 通过\n";
    }

    // ══ T8800.a kind=0 消除触发正测（29426 主载体；seed 11）══
    // R0 印主施放（962 自身全属性+1=有强可消；843 先制+2 R1-R2）→ R1 对手上古咒术
    // （868 消强，prio3 对手先动）→ kind=0 事件窗内 → {1}=100 衰弱(11) 落地对手。
    // rand：载体 cat4 无伤害零抽、上古咒术攻击浮动 1 抽（atk=1 打不死人）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC29426, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kShangGu, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(11);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8800.a 前置：962 自身攻击+2（全属性+1 且施放时无提升→翻倍档）");
        CHECK_EQ(slot(ctx, k2039Base, 0, 0), 4, "T8800.a {0} 回显=4（行参面）");
        CHECK_EQ(slot(ctx, k2039Base, 0, 1), 100, "T8800.a {1} 回显=100");
        CHECK_EQ(slot(ctx, k2039Base, 0, 2), 11, "T8800.a {2} 回显=11（衰弱档）");
        CHECK_EQ(slot(ctx, k2039Base, 0, 3), 1, "T8800.a {3} 回显=1");
        CHECK_EQ(slot(ctx, k2039Base, 0, 4), 0, "T8800.a 窗起点=0（先手 InRounds 含施放轮）");
        CHECK_EQ(slot(ctx, k2039Base, 0, 5), 4, "T8800.a 窗终点=4（不含）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8800.a 前置：868 消强落地（自身攻击回 0）");
        CHECK_EQ(slot(ctx, k2039Base, 0, 7), 1, "T8800.a 窗内触发数=1");
        CHECK_EQ(slot(ctx, k2039Base, 0, 8), 0, "T8800.a 末次触发 kind=0（消除）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Weakness)),
              "T8800.a 对手衰弱(11)落地（{1}=100 → 必定档）");
        CHECK(!ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Weakness)),
              "T8800.a 方向负测：施放方自身无衰弱");
        std::cout << "  [T8800.a] kind=0 消除触发正测 通过\n";
    }

    // ══ T8800.b kind=1 吸取双吃（29426 + 暗焰角袭 948 transfer；seed 12）══
    // R0 施放 → R1 对手暗焰角袭（948 transfer_stat_boosts 把自身提升搬给对手）→
    // kind=1 事件 → 双吃触发 → 衰弱落地。transfer 本体面互证（对手攻击+1、自身归 0）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC29426, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kAnYan, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(12);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8800.b 前置：948 吸取把提升搬走（自身归 0）");
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T8800.b 前置：对手获得搬来的攻击+2（transfer 实证——962 翻倍档整体搬运）");
        CHECK_EQ(slot(ctx, k2039Base, 0, 7), 1, "T8800.b 窗内触发数=1");
        CHECK_EQ(slot(ctx, k2039Base, 0, 8), 1, "T8800.b 末次触发 kind=1（吸取——双吃面）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Weakness)),
              "T8800.b 吸取(kind=1)同样触发衰弱落地（「或吸取」字面锁定）");
        std::cout << "  [T8800.b] kind=1 吸取双吃 通过\n";
    }

    // ══ T8800.c 未触发回退 + 次数票逐次消耗（29426；seed 13）══
    // R0 施放（无消强发生）→ R1-R3 安静 → R4 ROUND_END 回退评估（旗=0）→ 授
    // DAMAGE 次数票 counts=1 → R5 对手拍打被挡（免疫事件+1、hp 不变、票注销）→
    // R6 拍打落地（hp 下降）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC29426, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        int immune_events = 0;
        ctx.event_center_.register_watcher(
            EventType::EVENT_DAMAGE_IMMUNED, /*owner=*/0, ctx.roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [&immune_events](BattleContext*, const BattleEvent& ev) {
                if (ev.target == 0) {
                    ++immune_events;
                }
            },
            WatcherScope::TEAM);
        // R0 施放（index 0），R1-R4 安静拍打（index 1）——回退评估在 R4 ROUND_END。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        for (int r = 1; r <= 4; ++r) {
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        }
        CHECK_EQ(slot(ctx, k2039Base, 0, 7), 0, "T8800.c 前置：窗内零触发");
        CHECK_EQ(slot(ctx, k2039Base, 0, 9), 0, "T8800.c 回退评估执行（未触发→授予路径）");
        const int rc_r5 = 5;   // R5 行动面 rc（0 基第 6 回合）
        CHECK(damage_ticket_alive(ctx, 0, rc_r5),
              "T8800.c R5 回退票在位（DAMAGE counts=1）");
        const int hp_before_r5 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp, hp_before_r5, "T8800.c R5 对手拍打被票挡下（hp 不变）");
        CHECK_EQ(immune_events, 1, "T8800.c R5 EVENT_DAMAGE_IMMUNED=1（受击面拦截）");
        const int hp_before_r6 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.getPet(0).hp < hp_before_r6, "T8800.c R6 票已消耗注销（counts 票逐次扣）");
        CHECK_EQ(immune_events, 1, "T8800.c R6 无新免疫事件");
        CHECK(!damage_ticket_alive(ctx, 0, 6), "T8800.c R6 票已不在位（扣完注销）");
        std::cout << "  [T8800.c] 未触发回退 + 次数票逐次消耗 通过\n";
    }

    // ══ T8800.d 触发 void 回退（29426；seed 14）══
    // R0 施放 → R1 上古咒术消强（触发、衰弱落地）→ R4 回退评估旗>0 → 不授票 →
    // R5 对手拍打落地（hp 下降、零免疫事件）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC29426, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kShangGu, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(14);
        BattleContext& ctx = b.ctx();
        int immune_events = 0;
        ctx.event_center_.register_watcher(
            EventType::EVENT_DAMAGE_IMMUNED, /*owner=*/0, ctx.roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [&immune_events](BattleContext*, const BattleEvent& ev) {
                if (ev.target == 0) {
                    ++immune_events;
                }
            },
            WatcherScope::TEAM);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2039Base, 0, 7), 1, "T8800.d 前置：R1 窗内触发");
        for (int r = 2; r <= 4; ++r) {
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        }
        CHECK_EQ(slot(ctx, k2039Base, 0, 9), 4,
                 "T8800.d 回退评估=被触发旗跳过（slot9=4）");
        CHECK(!damage_ticket_alive(ctx, 0, 5), "T8800.d R5 无回退票（触发 void 回退）");
        const int hp_before_r5 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.getPet(0).hp < hp_before_r5, "T8800.d R5 对手拍打落地（hp 下降）");
        CHECK_EQ(immune_events, 0, "T8800.d 全程零免疫事件");
        std::cout << "  [T8800.d] 触发 void 回退 通过\n";
    }

    // ══ T8800.e 窗外事件拒收 + 回退不受窗外影响（29426；seed 15）══
    // R0 施放 → R1-R4 安静 → R5（0基4 = expire 轮，窗 [0,4) 外）上古咒术消强：
    // watcher 原始活动段仍达（duration=+1）但 lambda 窗界拒收（slot6++、旗不置）→
    // 无衰弱；同轮 ROUND_END 旗=0 → 照常授票 → R6 拍打被挡。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC29426, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kShangGu, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(15);
        BattleContext& ctx = b.ctx();
        int immune_events = 0;
        ctx.event_center_.register_watcher(
            EventType::EVENT_DAMAGE_IMMUNED, /*owner=*/0, ctx.roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [&immune_events](BattleContext*, const BattleEvent& ev) {
                if (ev.target == 0) {
                    ++immune_events;
                }
            },
            WatcherScope::TEAM);
        // R0 施放（index 0）；R1-R4 安静；R5（0基4）＝expire 轮（窗 [0,4) 外）：
        // 对手消强（窗外）。印主拍打让回合真跑。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        for (int r = 1; r <= 4; ++r) {
            if (r == 4) {
                run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                          BattleFsm::ActionType::SELECT_SKILL, 0);
            } else {
                run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                          BattleFsm::ActionType::NONE, 0);
            }
        }
        CHECK_EQ(slot(ctx, k2039Base, 0, 6), 1, "T8800.e 窗外事件拒收计数=1");
        CHECK_EQ(slot(ctx, k2039Base, 0, 7), 0, "T8800.e 窗内触发数=0（窗外不置旗）");
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Weakness)),
              "T8800.e 窗外消强不触发衰弱（窗界锁定）");
        CHECK_EQ(slot(ctx, k2039Base, 0, 9), 0, "T8800.e 回退评估=授予路径（窗外不影响）");
        CHECK(damage_ticket_alive(ctx, 0, 5), "T8800.e R5 回退票在位（窗外不 void 回退）");
        const int hp_before_r5 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp, hp_before_r5, "T8800.e R5 拍打被票挡下");
        CHECK_EQ(immune_events, 1, "T8800.e R5 免疫事件=1");
        std::cout << "  [T8800.e] 窗外事件拒收 + 回退不受窗外影响 通过\n";
    }

    // ══ T8800.f {1}=0 掷歪：无异常落地 + 触发仍 void 回退（override 变档；seed 16）══
    // 29426 {1} 100→0（学习者 4933）：R1 消强触发（旗置位）但 0% 申报 ROLL_FAILED
    // 短路 → 无衰弱；R4 回退评估旗>0 → 不授票 → R5 拍打落地。
    // —— 锁定「掷歪不改判未触发」的判别面（条件发生即触发）+ 0% 在位即消费。
    {
        ScenarioBattle b(make_party(override_pet(1, "印主", kC29426, kLearner29426, 2039,
                                                 /*arg_index=*/1, /*value=*/0,
                                                 {kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kShangGu, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(16);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2039Base, 0, 1), 0, "T8800.f {1} 回显=0（override 面）");
        CHECK_EQ(slot(ctx, k2039Base, 0, 7), 1, "T8800.f 窗内触发数=1（条件发生）");
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Weakness)),
              "T8800.f 0% 申报无衰弱落地（ROLL_FAILED 短路面）");
        for (int r = 2; r <= 4; ++r) {
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        }
        CHECK_EQ(slot(ctx, k2039Base, 0, 9), 4, "T8800.f 回退评估=被触发旗跳过（掷歪不改判）");
        const int hp_before_r5 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.getPet(0).hp < hp_before_r5, "T8800.f R5 拍打落地（无回退票）");
        std::cout << "  [T8800.f] {1}=0 掷歪判别面 通过\n";
    }

    // ══ T8800.g 离场清理（换宠 epoch 作废：窗与回退双死；seed 17）══
    // R0 施放（自身+1）→ R1 印主切人 → R2 新宠剑舞自身+2 → R3（窗内）对手消强：
    // watcher 已随换宠 epoch 作废 → 无衰弱（不继承给新宠）；R4 回退评估 tick 桶条目
    // 同 epoch 死 → 无票；R5 拍打落地。
    {
        auto party0 = make_party(skill_pet(1, "印主", {kC29426, kPaiDa, kPaiDa, kPaiDa,
                                                       kPaiDa}, 300, 1, kMaxHp));
        party0[1] = skill_pet(11, "替补", {kJianWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              300, 1, kMaxHp);
        ScenarioBattle b(party0,
                         make_party(skill_pet(2, "对手", {kShangGu, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(17);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        // R1：主动切换（旁置替补换上）。
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 1, "T8800.g 前置：替补已登场");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8800.g 前置：剑舞自身攻击+2");
        // R3（0基2，窗内）：对手消强 → watcher 已死 → 无触发。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8800.g 前置：消强落地（新宠提升被清）");
        CHECK_EQ(slot(ctx, k2039Base, 0, 7), 0, "T8800.g 窗内触发数=0（watcher 不继承）");
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Weakness)),
              "T8800.g 换宠后消强无衰弱（离场清理：窗死）");
        for (int r = 4; r <= 5; ++r) {
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        }
        CHECK(!damage_ticket_alive(ctx, 0, 5), "T8800.g R5 无回退票（桶条目同 epoch 死）");
        const int hp_before_r6 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.getPet(0).hp < hp_before_r6, "T8800.g R6 拍打落地（回退也死）");
        std::cout << "  [T8800.g] 离场清理（窗+回退双死） 通过\n";
    }

    // ══ T8800.h {2}=0 麻痹 + {0}=3 窗长差分（28884 千年遗梦；seed 18）══
    // R0 施放千年遗梦（{0}=3,{1}=100,{2}=0 麻痹）→ R1 剑舞自身+1 → R2 对手消强
    // （窗 [0,3) 内）→ 麻痹(0) 落地（{2} 行参驱动异常 id）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC28884, kJianWu, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kShangGu, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(18);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2039Base, 0, 0), 3, "T8800.h {0} 回显=3（差分载体）");
        CHECK_EQ(slot(ctx, k2039Base, 0, 2), 0, "T8800.h {2} 回显=0（麻痹档）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8800.h 前置：剑舞攻击+2");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2039Base, 0, 7), 1, "T8800.h 窗内触发数=1");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
              "T8800.h 对手麻痹(0)落地（{2} 行参逐字面消费）");
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Weakness)),
              "T8800.h 无衰弱（异常码来自行参非硬编码）");
        std::cout << "  [T8800.h] {2}=0 麻痹 + {0}=3 差分 通过\n";
    }

    std::cout << "scenario_8800_k492: 全部断言通过" << std::endl;
    return 0;
}
