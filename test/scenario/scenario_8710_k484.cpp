// 场景 8710 —— K484 两族（1939/2334，resources/moves_lib/k484_opp_hood_and_crit_attack_immune.cpp）
//（2026-11-19，worktree k484；号段 8710-8719 → T8710-pre/a-e + T8711.a-g + T8712.a-b）
//
// 断言口径（真实载体 + 审计槽 + 结构性正负测三线并锁）：
//   - 1939 = K472 1938 直系孪生（盾→罩面替换）：方向落对手（J3 2005 直系）+ {0} 双档
//     （200/36583、300/28261）+ 重施放叠加 + 红粉分档（罩吃粉不吃红、盾吃红不吃粉
//     ——与同链 1938 盾 twin 双面互证，K472 T8590.e/f 同款实测面）。
//   - 2334 = K480 1557 crit_happened 判据直系（AFTER_ACTION 探针面）+ ImmunityType::DAMAGE
//     窗口票（受击面拦截，battleContext.cpp:720）+ d5 1091 NextRounds 授票通道
//     （R+1 ROUND_START tick → 覆盖 [R+1, R+{0}] 恰"下{0}回合"）。暴击门正负测
//     （crit0 负测 + 恒暴正测）、窗起点当回合不覆盖、逐次不消耗（counts=0）、
//     粉伤穿透差分（龙之怒 29 固定附加同技红挡粉落——通道甄别实证面）、
//     miss 趟差分（EVENT_DAMAGE_IMMUNED 事件面）、重暴击刷新、换宠清票、
//     两族键段共存 + owner 维度隔离（M10 同型杀点面）。
//   - rand 记账：恒暴 crit=100 短路零抽、must_hit 跳命中掷、攻击伤害浮动各 1 抽；
//     断言全为结构面（hp 变/不变、quantity_of 精确、审计槽精确），不依赖伤害数值
//     → 无 RandMirror 需求（固定 seed 复现即可）。
#include <any>
#include <array>
#include <iostream>
#include <vector>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;         // 拍打：物40 无副作用（观测面/填充）
constexpr int kLongZhiNu = 10080;     // 龙之怒 [29]|[40] 特攻40 —— 红挡粉落差分载体
constexpr int kTingMei = 25108;       // 停梅抒意 [47,923,843]|[4,3,200,2,1] cat4 —— 923 先手吸取 200 粉
constexpr int kC28261 = 28261;        // 欺诈戏法 [962,927,1938,1939]|[1,4,2,3,300,300] cat4
constexpr int kC29166 = 29166;        // 停霜漫怀 [993,2230,1939,1938,843]|[1,15,4,2,100,28,300,300,2,2] cat4
constexpr int kC36583 = 36583;        // 分你一些 [1835,1938,1939]|[1,200,200] cat1 {0}=200
constexpr int kC38154 = 38154;        // 派派大扫除 [2052,1938,1939,1835]|[2,300,300,1] cat1
constexpr int kC38067 = 38067;        // 平庸或毁灭 [2216,2308,1356,1355,553,2334]|…[1] cat2（loader 面）
constexpr int kC38076 = 38076;        // 王·千秋稽首 [760,2331,1837,2334,2333]|…[1] cat1（loader 面）
constexpr int kC38271 = 38271;        // 地拜·千里一息 [2227,179,2274,2334]|[2,160,30,2] cat2 {0}=2（嘲风 4871）
constexpr int kC38411 = 38411;        // 意聚魂燃 [1767,1021,2334,1355]|[100,2,1] cat2 {0}=2（行为主载体）
constexpr int kC38271Learner = 4871;  // 嘲风（38271 override 构造需要 MonsterRecord）
constexpr int kMaxHp = 99999;

constexpr int k1939Base = 1019390100;   // 与插件头注审计基址逐字一致
constexpr int k2334Base = 1023340000;

int slot(const BattleContext& ctx, int base, int owner, int idx) {
    const auto it = ctx.plugin_storage.find(base + owner * 16 + idx);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

// 通用宠构造（k480 skill_pet 同款）：全技能 crit0+acc100+must_hit。
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

// 恒暴变体（crit 16/16 = 100% 短路零抽）。
ElfPet crit_pet(int id, const char* name, const std::array<int, 5>& skill_ids, int speed,
                int atk, int hp) {
    ElfPet p = skill_pet(id, name, skill_ids, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.critical_strike_rate = 100.0f;
    }
    return p;
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
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET,
                           dead == 0 ? 1 : 0);
        b.drive();
    }
}

// 免疫票在位查询（timing 位用 coverage_all 全通过——只看窗口存在性）。
// ⚠️ current_round 必须传**行动中**的 rc：advanceRound 在 ROUND_COMPLETION（battleFsm
// .cpp:2379）→ drive() 返回后 roundCount 已 +1——查刚跑完那回合的票要用进入该回合前
// 捕获的 rc（= 该回合行动面实际生效的 rc），否则窗末回合会被误判过期。
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

    // ══ T8710-pre 8 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 1939 四载体（{0}: 36583=200，其余 300）──
                {kC28261, {{962, {1}}, {927, {4, 2, 3}}, {1938, {300}}, {1939, {300}}}},
                {kC29166, {{993, {1, 15}}, {2230, {4, 2, 100, 28}}, {1939, {300}},
                           {1938, {300}}, {843, {2, 2}}}},
                {kC36583, {{1835, {1}}, {1938, {200}}, {1939, {200}}}},
                {kC38154, {{2052, {2}}, {1938, {300}}, {1939, {300}}, {1835, {1}}}},
                // ── 2334 四载体（{0}: 38067/38076=1，38271/38411=2）──
                {kC38067, {{2216, {100}}, {2308, {2}}, {1356, {3}}, {1355, {1}},
                           {553, {100, 22}}, {2334, {1}}}},
                {kC38076, {{760, {}}, {2331, {}}, {1837, {100, 31, 1}}, {2334, {1}},
                           {2333, {45, 45}}}},
                {kC38271, {{2227, {2}}, {179, {160}}, {2274, {30}}, {2334, {2}}}},
                {kC38411, {{1767, {}}, {1021, {100}}, {2334, {2}}, {1355, {1}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8710-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8710-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8710-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8710-pre] 8 载体实际加载器切片核验 通过\n";
    }

    // ══ T8710.a 1939 方向+{0}=200 档（36583 cat1 攻击载体；seed 11）══
    // cat1 side_effect 链先于攻击伤害执行（K472 引擎事实）→ 罩/盾先落，载体本体红伤
    // ~1 被同链 1938 盾吸收。rand：R1 伤害浮动 1 抽。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC36583, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(11);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hood_bank_.quantity_of(1939), 200,
                 "T8710.a 对手 1939 罩=200（{0} 档精确，红伤不碰罩）");
        CHECK_EQ(ctx.getPet(0).hood_bank_.quantity_of(1939), 0,
                 "T8710.a 方向负测：施罩方自身无 1939 罩");
        CHECK(ctx.getPet(1).shield_bank_.quantity_of(1938) > 0 &&
                  ctx.getPet(1).shield_bank_.quantity_of(1938) < 200,
              "T8710.a 同链 1938 盾落对手并被载体红伤吸收（0<余<200，孪生共存）");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp,
                 "T8710.a 对手 hp 不变（红伤被盾全吸收——罩不响应红伤面）");
        CHECK_EQ(slot(ctx, k1939Base, 0, 0), 1, "T8710.a 落地次数=1");
        CHECK_EQ(slot(ctx, k1939Base, 0, 2), 200, "T8710.a {0} 回显=200（档位精确面）");
        CHECK_EQ(slot(ctx, k1939Base, 0, 3), 1, "T8710.a target 回显=1（对手）");
        CHECK_EQ(slot(ctx, k1939Base, 0, 4), 200, "T8710.a 附加后余量审计=200");
        CHECK_EQ(slot(ctx, k1939Base, 0, 5), 200, "T8710.a 累计附加总点数=200");
        CHECK_EQ(slot(ctx, k1939Base, 0, 6), 1, "T8710.a cast 计数=1");
        std::cout << "  [T8710.a] 1939 方向+{0}=200 通过\n";
    }

    // ══ T8710.b 红粉分档差分（罩吃粉不吃红、盾吃红不吃粉；seed 12）══
    // R1 36583 落罩 200+盾 200 → R2 印主拍打（红）→ 盾吸收、罩不动 → R3 停梅抒意
    // （923 先手吸取 200 粉）→ 罩吸粉归零（破）、盾不动。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC36583, kPaiDa, kTingMei, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(12);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int hp_r1 = ctx.getPet(1).hp;
        const int hood_r1 = ctx.getPet(1).hood_bank_.quantity_of(1939);
        const int shield_r1 = ctx.getPet(1).shield_bank_.quantity_of(1938);
        CHECK_EQ(hood_r1, 200, "T8710.b 前置：罩 200 在位");
        // R2：红伤（拍打）→ 盾吃、罩不动。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.getPet(1).shield_bank_.quantity_of(1938) > 0 &&
                  ctx.getPet(1).shield_bank_.quantity_of(1938) < shield_r1,
              "T8710.b R2 红伤被盾吸收（余量下降）");
        CHECK_EQ(ctx.getPet(1).hood_bank_.quantity_of(1939), hood_r1,
                 "T8710.b R2 罩不响应红伤（余量不变）");
        CHECK_EQ(ctx.getPet(1).hp, hp_r1, "T8710.b R2 对手 hp 不变（盾全吸收）");
        // R3：粉伤（923 先手吸取 200 FIXED）→ 罩吃、盾不动。
        const int shield_after_r2 = ctx.getPet(1).shield_bank_.quantity_of(1938);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hood_bank_.quantity_of(1939), 0,
                 "T8710.b R3 罩吃掉 200 粉伤归零（200 恰耗尽）");
        CHECK_EQ(ctx.getPet(1).shield_bank_.quantity_of(1938), shield_after_r2,
                 "T8710.b R3 盾不响应粉伤（余量不变）");
        CHECK_EQ(ctx.getPet(1).hp, hp_r1, "T8710.b R3 对手 hp 不变（粉全被罩吃）");
        std::cout << "  [T8710.b] 红粉分档差分 通过\n";
    }

    // ══ T8710.c {0}=300 档（28261 cat4 纯辅助载体；seed 13）══
    // cat4 无攻击伤害 → 罩 300 无吸收干扰；链上 927 回血窗（满血钳 0 无粉伤）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC28261, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hood_bank_.quantity_of(1939), 300,
                 "T8710.c 对手 1939 罩=300（{0} 档差分面）");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp, "T8710.c cat4 无伤害（对手 hp 不变）");
        CHECK_EQ(slot(ctx, k1939Base, 0, 2), 300, "T8710.c {0} 回显=300");
        CHECK_EQ(slot(ctx, k1939Base, 0, 0), 1, "T8710.c 落地次数=1");
        std::cout << "  [T8710.c] 1939 {0}=300 档 通过\n";
    }

    // ══ T8710.d 重施放叠加（36583×2；seed 14）══
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC36583, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(14);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hood_bank_.quantity_of(1939), 400,
                 "T8710.d 重施放 plain add 叠加（200+200=400）");
        CHECK_EQ(slot(ctx, k1939Base, 0, 0), 2, "T8710.d 落地次数=2");
        CHECK_EQ(slot(ctx, k1939Base, 0, 5), 400, "T8710.d 累计附加总点数=400");
        CHECK_EQ(slot(ctx, k1939Base, 0, 6), 2, "T8710.d cast 计数=2");
        CHECK_EQ(slot(ctx, k1939Base, 1, 0), 0,
                 "T8710.d owner 维度隔离：owner1 落地数=0（M10 同型杀点面）");
        std::cout << "  [T8710.d] 重施放叠加 通过\n";
    }

    // ══ T8710.e 38154 孪生全链（罩 300+盾 300 同落对手；seed 15）══
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC38154, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(15);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hood_bank_.quantity_of(1939), 300,
                 "T8710.e 38154 罩=300（红伤不碰罩）");
        CHECK(ctx.getPet(1).shield_bank_.quantity_of(1938) > 0 &&
                  ctx.getPet(1).shield_bank_.quantity_of(1938) < 300,
              "T8710.e 38154 盾落对手且被载体红伤吸收（0<余<300）");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp, "T8710.e 对手 hp 不变（盾全吸收）");
        CHECK_EQ(slot(ctx, k1939Base, 0, 2), 300, "T8710.e {0} 回显=300");
        std::cout << "  [T8710.e] 38154 孪生全链 通过\n";
    }

    // ══ T8711.a 2334 正测 {0}=2（38411 恒暴主载体；seed 21）+ 免疫事件计数 ══
    // R1 印主攻击（恒暴）→ 武装；R2 授票 tick；R2/R3 对手攻击被挡（窗 [2,3]，
    // counts=0 不逐次消耗）；R4 窗过攻击落地。
    {
        ScenarioBattle b(make_party(crit_pet(1, "印主", {kC38411, kPaiDa, kPaiDa, kPaiDa,
                                                         kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(21);
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
        CHECK_EQ(ctx.crit_happened[0], true, "T8711.a 前置：恒暴载体确已暴击");
        CHECK_EQ(slot(ctx, k2334Base, 0, 0), 2, "T8711.a {0} 回显=2");
        CHECK_EQ(slot(ctx, k2334Base, 0, 1), 1, "T8711.a 暴击旗=1（门面实证）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 2), 0, "T8711.a 武装回合=0（R1，roundCount 0 基）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 7), 1, "T8711.a 暴击门通过数=1");
        CHECK_EQ(slot(ctx, k2334Base, 0, 8), 1, "T8711.a cast 计数=1");
        CHECK_EQ(slot(ctx, k2334Base, 0, 5), 0, "T8711.a R1 尚未授票（tick 在下回合 ROUND_START）");
        // R2：授票 tick 先跑，对手攻击被挡。窗 [rc1, rc2]=日历 R2/R3。
        const int self_hp_r1 = ctx.getPet(0).hp;
        const int rc_r2 = ctx.roundCount;   // 行动面 rc（drive 后已 +1，见 helper 注）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(rc_r2, 1, "T8711.a R2 行动面 rc=1（0 基）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 5), 1, "T8711.a R2 授票 tick 执行=1");
        CHECK_EQ(slot(ctx, k2334Base, 0, 6), 1, "T8711.a 授票回合=1（R2 ROUND_START，0 基）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 3), 1, "T8711.a 窗起点=1（rc0+1，NextRounds）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 4), 2, "T8711.a 窗终点=2（rc1+{0}-1 含）");
        CHECK_EQ(ctx.getPet(0).hp, self_hp_r1, "T8711.a R2 红伤被免疫票挡下（hp 不变）");
        CHECK_EQ(immune_events, 1, "T8711.a R2 EVENT_DAMAGE_IMMUNED=1（受击面拦截实证）");
        CHECK(damage_ticket_alive(ctx, 0, rc_r2), "T8711.a R2 免疫票在位（窗口查询面）");
        // R3：窗内第二击——counts=0 窗口票不逐次消耗。
        const int rc_r3 = ctx.roundCount;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(rc_r3, 2, "T8711.a R3 行动面 rc=2");
        CHECK_EQ(ctx.getPet(0).hp, self_hp_r1, "T8711.a R3 仍被挡（hp 不变）");
        CHECK_EQ(immune_events, 2, "T8711.a R3 免疫事件=2（逐击拦截）");
        CHECK(damage_ticket_alive(ctx, 0, rc_r3), "T8711.a R3 票未被消耗（counts=0 窗口语义）");
        // R4：窗 [2,3] 已过 → 攻击落地。
        const int rc_r4 = ctx.roundCount;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(rc_r4, 3, "T8711.a R4 行动面 rc=3（窗外首回合）");
        CHECK(ctx.getPet(0).hp < self_hp_r1, "T8711.a R4 窗过攻击落地（hp 下降）");
        CHECK_EQ(immune_events, 2, "T8711.a R4 无新免疫事件");
        CHECK(!damage_ticket_alive(ctx, 0, rc_r4), "T8711.a R4 票过期（absolute window 判定式）");
        std::cout << "  [T8711.a] 2334 正测 {0}=2 窗界+不消耗 通过\n";
    }

    // ══ T8711.b 2334 暴击门负测（38271 crit0；seed 22）══
    // crit0 技能永不暴 → 门闭不武装 → R2 对手攻击照常落地。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC38271, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(22);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.crit_happened[0], false, "T8711.b 前置：crit0 未暴");
        CHECK_EQ(slot(ctx, k2334Base, 0, 1), 0, "T8711.b 暴击旗=0（门面）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 7), 0, "T8711.b 门通过数=0（未武装）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 2), 0, "T8711.b 无武装回合");
        const int self_hp_r1 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2334Base, 0, 5), 0, "T8711.b R2 无授票 tick");
        CHECK(ctx.getPet(0).hp < self_hp_r1, "T8711.b R2 攻击照常落地（无免疫）");
        CHECK(!damage_ticket_alive(ctx, 0, ctx.roundCount - 1), "T8711.b 无 DAMAGE 票");
        std::cout << "  [T8711.b] 2334 暴击门负测 通过\n";
    }

    // ══ T8711.c {0}=1 档（38271+override {0}=1，嘲风 4871；seed 23）══
    // 链上 2274 回满 PP 并 30/点吸取对手（R1 对手 -30 粉，结构面记录不锁值——
    // 843 无、2227 锁切不干扰）。窗 [2,2]：R2 挡、R3 落。
    {
        ScenarioBattle b(make_party(override_pet(1, "印主", kC38271, kC38271Learner,
                                                 /*effect_id=*/2334, /*arg_index=*/0,
                                                 /*value=*/1,
                                                 {kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        for (Skills& sk : b.pet(0).skills) {
            sk.critical_strike_rate = 100.0f;   // 恒暴（override_pet 基建清零了暴击率）
        }
        b.start();
        sbs_srand(23);
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
        CHECK_EQ(ctx.crit_happened[0], true, "T8711.c 前置：恒暴已暴");
        CHECK_EQ(slot(ctx, k2334Base, 0, 0), 1, "T8711.c {0} 回显=1（override 生效）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2334Base, 0, 3), 1, "T8711.c 窗起点=1（rc0+1，NextRounds）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 4), 1, "T8711.c 窗终点=1（{0}=1 单回合窗）");
        CHECK_EQ(immune_events, 1, "T8711.c R2 免疫事件=1（红伤被挡）");
        // R3：窗已过 → 落地。
        const int rc_r3c = ctx.roundCount;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(immune_events, 1, "T8711.c R3 无新免疫事件");
        CHECK(!damage_ticket_alive(ctx, 0, rc_r3c), "T8711.c R3 票过期");
        std::cout << "  [T8711.c] 2334 {0}=1 档差分 通过\n";
    }

    // ══ T8711.d 粉伤穿透差分 + miss 趟差分（seed 24）══
    // A 局：窗 [2,3] 内对手用龙之怒（红 40 被挡 + 29 固定附加粉 40 落地）——
    //   同一技能红/粉分通道，"免疫对手的攻击"≠免粉（通道甄别实证面）。
    {
        ScenarioBattle b(make_party(crit_pet(1, "印主", {kC38411, kPaiDa, kPaiDa, kPaiDa,
                                                         kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kLongZhiNu, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(24);
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
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2334Base, 0, 5), 1, "T8711.d A R2 授票=1（窗 [2,3] 活）");
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp - 40,
                 "T8711.d A R2 红伤被挡+29 固定附加粉 40 穿透（DAMAGE 票不碰粉伤）");
        CHECK_EQ(immune_events, 1, "T8711.d A R2 免疫事件=1（红组件被拦截）");
        CHECK(damage_ticket_alive(ctx, 0, 1), "T8711.d A R2 票在位");
        std::cout << "  [T8711.d] 粉伤穿透差分 通过\n";
    }
    // B 局：窗内对手 miss 趟（accuracy 0 无必中）——票不动、无免疫事件、hp 不变；
    //   再正常趟被挡（事件+1）。miss/无效趟与免疫触发的分辨面 = EVENT_DAMAGE_IMMUNED
    //   （K455 1794/1115 差分范式；无效趟 observables 与 miss 同构，记档不另立局）。
    {
        ScenarioBattle b(make_party(crit_pet(1, "印主", {kC38411, kPaiDa, kPaiDa, kPaiDa,
                                                         kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(25);
        BattleContext& ctx = b.ctx();
        // 对手槽 0 改成必 miss 武器（accuracy 0、去必中）：hit_chance=0 → 恒 miss。
        ctx.getPet(1).skills[0].accuracy = 0;
        ctx.getPet(1).skills[0].must_hit = 0;
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
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2334Base, 0, 5), 1, "T8711.d B R2 授票=1");
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T8711.d B R2 miss 趟 hp 不变");
        CHECK_EQ(immune_events, 0, "T8711.d B R2 miss 趟无免疫事件（票未被触发面）");
        CHECK(damage_ticket_alive(ctx, 0, 1), "T8711.d B R2 票在位（miss 不动票）");
        // R3：槽 1 正常攻击 → 被挡（事件 +1）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T8711.d B R3 正常趟被挡");
        CHECK_EQ(immune_events, 1, "T8711.d B R3 免疫事件=1（与 miss 趟分辨）");
        std::cout << "  [T8711.d] miss 趟差分 通过\n";
    }

    // ══ T8711.e 窗起点边界：当回合不覆盖（{0}=1 override；seed 26）══
    // 印主先手暴击武装 → 同回合后手对手攻击**落地**（"下{0}回合"不含当回合；
    // M5 杀点面：InRounds 化直授会把当回合也盖住）。
    {
        ScenarioBattle b(make_party(override_pet(1, "印主", kC38271, kC38271Learner,
                                                 /*effect_id=*/2334, /*arg_index=*/0,
                                                 /*value=*/1,
                                                 {kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        for (Skills& sk : b.pet(0).skills) {
            sk.critical_strike_rate = 100.0f;   // 恒暴（override_pet 基建清零了暴击率）
        }
        b.start();
        sbs_srand(26);
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
        // R1：印主（先手）攻击暴击；对手（后手）同回合攻击 → 落地。
        const int opp_hp0 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.crit_happened[0], true, "T8711.e 前置：R1 已暴");
        CHECK(ctx.getPet(0).hp < opp_hp0, "T8711.e R1 同回合对手攻击落地（当回合不覆盖）");
        CHECK_EQ(immune_events, 0, "T8711.e R1 无免疫事件（武装当回合票未授）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 7), 1,
                 "T8711.e R1 暴击门通过=1（acting_side 过滤：对手桶不双计，M9 杀点面）");
        // R2：窗 [2,2] 挡。R3：落。
        const int self_hp_r1 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp, self_hp_r1, "T8711.e R2 被挡");
        CHECK_EQ(immune_events, 1, "T8711.e R2 免疫事件=1");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.getPet(0).hp < self_hp_r1, "T8711.e R3 窗过落地");
        std::cout << "  [T8711.e] 窗起点边界 通过\n";
    }

    // ══ T8711.f 重暴击刷新（{0}=1 override；seed 27）══
    // R1 暴 → 窗 [2,2]；R2 印主再暴（先手）→ 同 source_id 刷新 → 窗 [3,3]；
    // R3 挡、R4 落（连续暴击窗连续）。
    {
        ScenarioBattle b(make_party(override_pet(1, "印主", kC38271, kC38271Learner,
                                                 /*effect_id=*/2334, /*arg_index=*/0,
                                                 /*value=*/1,
                                                 {kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        for (Skills& sk : b.pet(0).skills) {
            sk.critical_strike_rate = 100.0f;   // 恒暴（override_pet 基建清零了暴击率）
        }
        b.start();
        sbs_srand(27);
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
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2334Base, 0, 7), 2, "T8711.f R2 再暴：门通过数=2");
        CHECK_EQ(slot(ctx, k2334Base, 0, 5), 1,
                 "T8711.f R2 末已授 1 次 tick（首暴窗 [rc1,rc1]；再暴 tick 在 R3 ROUND_START）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 3), 1, "T8711.f 首暴窗起点=1");
        CHECK_EQ(slot(ctx, k2334Base, 0, 4), 1, "T8711.f 首暴窗终点=1");
        const int self_hp_r2 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2334Base, 0, 5), 2, "T8711.f R3 第二次授票 tick（重暴击刷新）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 3), 2, "T8711.f 刷新窗起点=2（rc1+1）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 4), 2, "T8711.f 刷新窗终点=2");
        CHECK_EQ(ctx.getPet(0).hp, self_hp_r2, "T8711.f R3 刷新窗内被挡");
        CHECK_EQ(immune_events, 1, "T8711.f R3 免疫事件=1");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.getPet(0).hp < self_hp_r2, "T8711.f R4 窗过落地");
        std::cout << "  [T8711.f] 重暴击刷新 通过\n";
    }

    // ══ T8711.g 换宠清票（38411 {0}=2；seed 28）══
    // R1 暴武装 → R2 印主换宠：ON_STAGE 票当场清（pending tick 同作废）→
    // 对手攻击替补落地。
    {
        ScenarioBattle b(make_party(crit_pet(1, "印主", {kC38411, kPaiDa, kPaiDa, kPaiDa,
                                                         kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(28);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2334Base, 0, 5), 1, "T8711.g R2 授票=1（票活）");
        CHECK(damage_ticket_alive(ctx, 0, 1), "T8711.g R2 票在位");
        // R3：印主换宠（替补 1）+ 对手攻击 → 票已清 → 落地替补。
        const int rc_r3g = ctx.roundCount;   // 换宠回合行动面 rc=2（窗 [1,2] 内——
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(rc_r3g, 2, "T8711.g R3 行动面 rc=2（若未清票此 rc 本应活着——换宠清票判别面）");
        CHECK(!damage_ticket_alive(ctx, 0, rc_r3g), "T8711.g R3 换宠后票已清（clear_on_stage）");
        CHECK(ctx.getPet(0).hp < kMaxHp, "T8711.g R3 替补挨击落地（无票保护）");
        std::cout << "  [T8711.g] 换宠清票 通过\n";
    }

    // ══ T8711.h 探针生命周期：次回合非载体出手不武装（seed 29）══
    // R1 载体暴击武装（探针 duration=1 当轮即焚）→ R2 印主改用拍打（非载体、恒暴
    // 照暴）→ 无探针=不武装（slot7 停 1）；窗仍为 R1 那扇（[rc1,rc2]）。M10 杀点面：
    // 续命探针会在 R2 拍打暴击后再武装（slot7=2）。
    {
        ScenarioBattle b(make_party(crit_pet(1, "印主", {kC38411, kPaiDa, kPaiDa, kPaiDa,
                                                         kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(29);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2334Base, 0, 7), 1, "T8711.h R1 武装=1");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.crit_happened[0], true, "T8711.h 前置：R2 拍打也暴了（恒暴宠）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 7), 1,
                 "T8711.h R2 拍打暴击不武装（探针 duration=1 已焚——生命周期面）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 8), 1,
                 "T8711.h R2 cast 计数停 1（拍打无 2334 切片——SKILL_EFFECT 未进）");
        const int rc_r3h = ctx.roundCount;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2334Base, 0, 5), 1, "T8711.h R3 授票 tick 仍=1（未重武装）");
        CHECK(damage_ticket_alive(ctx, 0, rc_r3h), "T8711.h R3 窗 [rc1,rc2] 仍活（R1 那扇）");
        const int rc_r4h = ctx.roundCount;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(!damage_ticket_alive(ctx, 0, rc_r4h), "T8711.h R4 窗过（未续窗）");
        std::cout << "  [T8711.h] 探针生命周期 通过\n";
    }

    // ══ T8712.a 两族键段共存 + owner 维度隔离（seed 31）══
    // 同局双族：R1 28261（罩 300 落对手 + 1939 审计 owner0）→ R2 38411 恒暴（2334
    // 审计 owner0 + 票窗 [3,4]）→ R3/R4 挡、R5 落；两族审计互不串、罩面存活、
    // owner1 维度全零。
    {
        ScenarioBattle b(make_party(crit_pet(1, "印主", {kC28261, kC38411, kPaiDa, kPaiDa,
                                                         kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(31);
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
        CHECK_EQ(ctx.getPet(1).hood_bank_.quantity_of(1939), 300, "T8712.a R1 罩=300（1939 面）");
        CHECK_EQ(slot(ctx, k1939Base, 0, 2), 300, "T8712.a R1 1939 {0} 回显=300");
        CHECK_EQ(slot(ctx, k2334Base, 0, 8), 0, "T8712.a R1 2334 cast=0（未用 38411）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.crit_happened[0], true, "T8712.a R2 已暴（38411）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 0), 2, "T8712.a R2 2334 {0} 回显=2");
        CHECK_EQ(slot(ctx, k2334Base, 0, 8), 1, "T8712.a R2 2334 cast=1");
        CHECK_EQ(ctx.getPet(1).hood_bank_.quantity_of(1939), 300,
                 "T8712.a R2 罩面存活（红伤不碰罩）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2334Base, 0, 3), 2, "T8712.a R3 窗起点=2（rc1+1）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 4), 3, "T8712.a R3 窗终点=3（rc2+{0}-1 含）");
        CHECK_EQ(immune_events, 1, "T8712.a R3 红伤被挡");
        CHECK_EQ(ctx.getPet(1).hood_bank_.quantity_of(1939), 300, "T8712.a R3 罩仍=300");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(immune_events, 2, "T8712.a R4 窗内第二击仍挡（不逐次消耗）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.getPet(0).hp < kMaxHp, "T8712.a R5 窗过落地");
        // owner1 维度全零（M10 同型杀点面：owner 维度拉平会读穿）。
        CHECK_EQ(slot(ctx, k1939Base, 1, 0), 0, "T8712.a owner1 1939 落地=0");
        CHECK_EQ(slot(ctx, k1939Base, 1, 2), 0, "T8712.a owner1 1939 {0} 回显=0");
        CHECK_EQ(slot(ctx, k2334Base, 1, 0), 0, "T8712.a owner1 2334 {0} 回显=0");
        CHECK_EQ(slot(ctx, k2334Base, 1, 7), 0, "T8712.a owner1 2334 门通过=0");
        std::cout << "  [T8712.a] 两族键段共存 通过\n";
    }

    // ══ T8712.b 38067/38076 loader 面行为抽检（38076 cat1 恒暴武装面；seed 32）══
    // 38067/38076 链上 crit 兄弟（2308 洗牌/553 异常/1837 臣服）rand 与状态面重——
    // 行为面由 38411/38271(+override) 同 {0} 档覆盖（K480 取舍同款）；本腿只锁
    // 38076 的武装审计面（{0}=1）。
    {
        ScenarioBattle b(make_party(crit_pet(1, "印主", {kC38076, kPaiDa, kPaiDa, kPaiDa,
                                                         kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(32);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.crit_happened[0], true, "T8712.b 前置：恒暴已暴");
        CHECK_EQ(slot(ctx, k2334Base, 0, 0), 1, "T8712.b {0} 回显=1（38076 原始切片）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 1), 1, "T8712.b 暴击旗=1");
        CHECK_EQ(slot(ctx, k2334Base, 0, 7), 1, "T8712.b 门通过数=1（武装面）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2334Base, 0, 3), 1, "T8712.b 窗起点=1（rc0+1）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 4), 1, "T8712.b 窗终点=1（{0}=1）");
        std::cout << "  [T8712.b] 38076 武装面抽检 通过\n";
    }

    // ══ T8712.c 双边施法 owner 键位碰撞差分（M6 同型杀点面；seed 33）══
    // 双方各用一次 36583：owner0 与 owner1 各自 cast 计数=1、各自罩余量=200——
    // 审计键 owner×16 布局的**正面**（有 owner1 写入时的隔离面；拉平变异会把
    // 两边写串到同一键位 → cast 计数变 2）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC36583, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kC36583, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(33);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1939Base, 0, 6), 1, "T8712.c owner0 cast 计数=1（不被 owner1 串账）");
        CHECK_EQ(slot(ctx, k1939Base, 1, 6), 1, "T8712.c owner1 cast 计数=1（owner 维度隔离正面）");
        CHECK_EQ(slot(ctx, k1939Base, 0, 0), 1, "T8712.c owner0 落地次数=1");
        CHECK_EQ(slot(ctx, k1939Base, 1, 0), 1, "T8712.c owner1 落地次数=1");
        CHECK_EQ(ctx.getPet(1).hood_bank_.quantity_of(1939), 200,
                 "T8712.c 对手罩 200（owner0 施）");
        CHECK_EQ(ctx.getPet(0).hood_bank_.quantity_of(1939), 200,
                 "T8712.c 印主罩 200（owner1 施——方向面 owner1 维度）");
        std::cout << "  [T8712.c] 双边施法 owner 键位碰撞差分 通过\n";
    }

    std::cout << "scenario_8710_k484 ALL PASS" << std::endl;
    return 0;
}
