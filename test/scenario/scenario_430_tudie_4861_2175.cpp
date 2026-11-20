// 场景 430 —— 涂蝶 4861 第五技「混色神·虚之彩 29242」效果 2175
//（moves_lib/d5_D27.cpp，2026-09-29）
//
// 2175 "技能无效时，随机附加2种非限制类异常状态"（SKILL_INVALID 分支，D27）：
//   T1 龙威封技路径：对手封涂蝶攻+属 → 29242 被无效（SEALED→SKILL_INVALID）→
//      对手恰 2 条异常（scenario_002 同款注入）；
//   T2 沉默第五技路径：涂蝶沉默 + 29242 在第 5 槽（kFifthSkillSlot）→ 被无效 →
//      对手恰 2 条异常（沉默锁第五技 = 29242 的官方无效通道之一）；
//   T3 正常命中路径：干净对手 → 对手 0 异常（SKILL_INVALID 桶不跑，效果不误触）；
//   T4 非限制池排除：30 场 T1 路径并集不含瘫痪 19 / 凝滞 32。统计断言——过滤就在
//      d5_D27.cpp 的 non_restriction_pool()（全池⊖限制类）；抽样最多 false-pass
//      （漏抓未过滤的 bug），不可能 false-fail；
//   T5 异常全免：对手免疫全部异常 → 落地 0 条、安静无声（attach 单通道兜底，
//      2175 无"未触发则"补偿子句）。
//
// 随机性口径：attach_random_anomalies 概率门 100% + 不重复挑选 → 干净对手"恰 2 条"
// 是确定断言（146 T3a 同款）。2392 召唤子句跳过（见 d5_D27.cpp 头注），本场景不覆盖。
#include <iostream>
#include <set>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kXuzhicai = 29242;   // 混色神·虚之彩：属性技 [2392, 2175]
constexpr int kHarmless = 10938;   // 本能一击：威力 0（146 同款填充技）
constexpr int kCrippledId = static_cast<int>(AbnormalStatusId::Crippled);  // 瘫痪 19（限制类）
constexpr int kStasisId = static_cast<int>(AbnormalStatusId::Stasis);      // 凝滞 32（限制类）

std::array<int, 5> tudie_skills() {
    // 29242 放第 5 槽：T2 沉默路径按"替换后的技能槽"判第五技（skills.cpp 沉默收口）。
    return {kHarmless, kHarmless, kHarmless, kHarmless, kXuzhicai};
}

ElfPet tudie(int speed = 200) {
    ElfPet p = make_pet(4861, "涂蝶", tudie_skills(), speed, /*atk=*/1, /*hp=*/5000,
                        SoulMark(2468, "涂蝶", "幕系统"));
    for (Skills& s : p.skills) {
        s.must_hit = 1;   // 排除 miss 变量：无效路径全部走"被拦"（龙威/沉默）
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int speed = 100) {
    ElfPet p = make_pet(1, "对手", {kHarmless, kHarmless, kHarmless, kHarmless, kHarmless},
                        speed, /*atk=*/1, /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

int count_active_anomalies(const BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) {
            ++n;
        }
    }
    return n;
}

std::set<int> active_anomaly_ids(const BattleContext& ctx, int side) {
    std::set<int> ids;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) {
            ids.insert(id);
        }
    }
    return ids;
}

// 打一回合：涂蝶用第 5 槽（29242）、对手普攻。
void play_xuzhicai_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 4);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // T1 龙威封技 → SKILL_INVALID → 恰 2 条非限制异常
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(tudie()), make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/70430,
                   /*attribute=*/true, /*attack=*/true, /*count=*/1,
                   /*duration_rounds=*/0, /*penetrable=*/false);
        play_xuzhicai_round(b);
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID,
                 "T1 前置：29242 被龙威拦下（SKILL_INVALID）");
        CHECK_EQ(count_active_anomalies(ctx, 1), 2,
                 "T1 核心：技能无效 → 对手随机附加 2 种异常（恰 2 条活跃）");
        std::cout << "  [T1] 龙威封技路径 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2 沉默第五技 → SKILL_INVALID → 恰 2 条异常
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(tudie()), make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, /*owner=*/0, static_cast<int>(AbnormalStatusId::Silence),
                      /*duration_rounds=*/2, /*actor=*/1);
        play_xuzhicai_round(b);
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID,
                 "T2 前置：沉默下第 5 槽 29242 被无效（SEALED）");
        CHECK_EQ(count_active_anomalies(ctx, 1), 2,
                 "T2 核心：沉默锁第五技 → 补偿照跑（对手恰 2 条异常）");
        std::cout << "  [T2] 沉默第五技路径 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3 正常命中 → 0 异常（SKILL_INVALID 桶不跑）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(tudie()), make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        play_xuzhicai_round(b);
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::HIT,
                 "T3 前置：29242 正常命中");
        CHECK_EQ(count_active_anomalies(ctx, 1), 0,
                 "T3 核心：命中路径不触发 2175（对手 0 异常）");
        std::cout << "  [T3] 正常命中路径 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T4 非限制池排除：30 场并集不含瘫痪 19 / 凝滞 32
    // ════════════════════════════════════════════════════════════════
    {
        std::set<int> seen;
        for (int i = 0; i < 30; ++i) {
            ScenarioBattle b(make_party(tudie()), make_party(foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            seal_skill(&ctx, 1, 0, /*effect_id=*/70431, true, true, /*count=*/1,
                       0, /*penetrable=*/false);
            play_xuzhicai_round(b);
            REQUIRE(ctx.ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID);
            CHECK_EQ(count_active_anomalies(ctx, 1), 2, "T4 每场恰 2 条");
            for (int id : active_anomaly_ids(ctx, 1)) {
                seen.insert(id);
            }
        }
        CHECK(seen.find(kCrippledId) == seen.end(),
              "T4 核心：并集不含瘫痪 19（限制类被池排除）");
        CHECK(seen.find(kStasisId) == seen.end(),
              "T4 核心：并集不含凝滞 32（限制类被池排除）");
        CHECK(!seen.empty(), "T4 前置 sanity：30 场并集非空");
        std::cout << "  [T4] 非限制池排除 通过（并集 " << seen.size() << " 种，均非限制类）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T5 异常全免 → 落地 0 条、不误补偿不崩
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(tudie()), make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(), /*mask=*/0,
                           /*duration_rounds=*/9, /*source_id=*/70432,
                           /*soul_immunity=*/false, EffectScope::ON_STAGE,
                           /*counts=*/0, /*source_effect_id=*/70432);
        seal_skill(&ctx, 1, 0, /*effect_id=*/70433, true, true, /*count=*/1,
                   0, /*penetrable=*/false);
        play_xuzhicai_round(b);
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID,
                 "T5 前置：29242 仍被无效");
        CHECK_EQ(count_active_anomalies(ctx, 1), 0,
                 "T5 核心：全免异常 → 落地 0 条（免疫挡下不计，无补偿子句安静无声）");
        std::cout << "  [T5] 异常全免路径 通过\n";
    }

    std::cout << "scenario_430 PASS (涂蝶 4861 混色神·虚之彩 2175：技能无效→随机2种非限制类异常)"
              << std::endl;
    return 0;
}
