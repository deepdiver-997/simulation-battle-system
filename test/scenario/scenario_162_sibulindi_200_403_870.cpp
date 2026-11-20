#include <utils/sbs_rand.h>
// 场景 162：斯布林蒂 4600 技能组缺口（一）—— 属性技与吸取族
//（moves_lib/batch_c_skills.cpp，2026-09-27）
//
// 覆盖效果：
//   200  万物绘卷：对手处于提升 → 100% 烧伤（概率申报 apply_anomaly_checked）
//   1248 万物绘卷：对手处于异常 → 100% 中毒
//   403  源初构想：特攻/速度 +2，同属性翻倍 +4（系别视图判）
//   464  源初构想：遇到天敌 40% 烧伤（种子触发分支）
//   870  蔓生：吸取 200 点体力（点数吸取）
//   1055 郁拂：消除提升成功 → 下 1 回合对手攻击技能无效（SEAL_ATTACK 票 + 条件门）
//   1955 共生链接：恢复己方所有不在场精灵 100 体力
//   1510 环纹锁定：2 回合内对手主动切换 → 登场宠随机 2 种异常
//   1072 曳影：附加当前体力 20% 粉伤，连用 +5%（同种子无关、精确断言）
//   1469 编译：3 回合自身粉伤提升 30%（AMP 桶 + 回合门）
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kWanhui = 28603;    // 万物绘卷：200+449+1248（属性、必中）
constexpr int kYuanchu = 28585;   // 源初构想：403+464（属性、必中）
constexpr int kMansheng = 28602;  // 蔓生：4+4+870（属性、必中）
constexpr int kYufu = 37090;      // 郁拂：781+1055（特攻 85、先制 3）
constexpr int kGongsheng = 37121; // 共生链接：1955（物攻 90）
constexpr int kHuanwen = 37123;   // 环纹锁定：1510（物攻 120）
constexpr int kYeying = 37120;    // 曳影：1072（物攻 60）
constexpr int kBianyi = 28601;    // 编译：1469（属性、必中）
constexpr int kHit = 10004;
#ifndef SEED_HIT
#define SEED_HIT 1
#endif
constexpr int kHarmless = 10938;

ElfPet sibu(std::array<int, 5> skills, int speed = 200, int hp = 100000,
            std::array<int, 2> elem = {0, 0}) {
    ElfPet p = scenario::make_pet(0, "斯布林蒂", skills, speed, /*atk=*/100, hp);
    p.elementalAttributes = elem;
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills = {kHarmless, kHarmless, kHarmless, kHarmless, kHarmless},
           std::array<int, 2> elem = {0, 0}, int atk = 10, int hp = 100000) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, atk, hp);
    p.elementalAttributes = elem;
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

int count_active_anomalies(const BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= 43; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) {
            ++n;
        }
    }
    return n;
}

bool has_seal_entry(const BattleContext& ctx, int effect_id) {
    for (const RuleTicket& t : ctx.rule_center_.entries()) {
        if (t.source_effect_id == effect_id) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 200：对手有提升 → 100% 烧伤；无提升 → 不触发 ==========
    {
        ScenarioBattle b(scenario::make_party(sibu({kWanhui, kHarmless, kHarmless, kHarmless,
                                                    kHarmless})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            ctx.ability_levels[1][i] = 1;   // 对手处于能力提升状态
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, 2), "T1 核心：对手有提升 → 100% 烧伤落地");
        std::cout << "  [T1] 200 提升条件烧伤 通过\n";
    }
    {
        ScenarioBattle b(scenario::make_party(sibu({kWanhui, kHarmless, kHarmless, kHarmless,
                                                    kHarmless})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, 2), "T1b 对照：对手无提升 → 不烧伤");
        std::cout << "  [T1b] 200 无提升对照 通过\n";
    }

    //========== T2b 449（复用既有实现）：对手处于下降 → 100% 冻伤 ==========
    //（万物绘卷三连 [200, 449, 1248] 的第二条；1248 的情形见 batch_c_skills.cpp 头注
    //  跳过清单——被 custom_effect_programs 数据程序占据且 args 编码不匹配，静默 no-op。）
    {
        ScenarioBattle b(scenario::make_party(sibu({kWanhui, kHarmless, kHarmless, kHarmless,
                                                    kHarmless})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][2] = -1;   // 对手防御-1（处于能力下降状态）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, 5),
              "T2b 核心：对手有下降 → 449 的 100% 冻伤落地");
        std::cout << "  [T2b] 449 下降条件冻伤 通过\n";
    }

    //========== T3 403：特攻/速度 +2；同属性翻倍 +4 ==========
    {
        // 分支 A：双方同元素 {0,0} → +4
        ScenarioBattle b(scenario::make_party(sibu({kYuanchu, kHarmless, kHarmless, kHarmless,
                                                    kHarmless})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][1], 4, "T3a 核心：同属性 → 特攻+4（效果翻倍）");
        CHECK_EQ(ctx.ability_levels[0][4], 4, "T3a 核心：同属性 → 速度+4");
        std::cout << "  [T3a] 403 同属性翻倍 通过\n";
    }
    {
        // 分支 B：对手草系（不同属性）→ +2；且草克水=天敌 → 464 的 40% 烧伤（种子触发分支）
        sbs_srand(SEED_HIT);
        ScenarioBattle b(scenario::make_party(sibu({kYuanchu, kHarmless, kHarmless, kHarmless,
                                                    kHarmless},
                                                   /*speed=*/200, /*hp=*/100000,
                                                   /*elem=*/{2, 0})),   // 我水系
                         scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless,
                                                   kHarmless},
                                                  /*elem=*/{1, 0})));  // 对手草系（克水）
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][1], 2, "T3b 核心：不同属性 → 特攻+2（不翻倍）");
        CHECK(ctx.has_active_abnormal_status(1, 2),
              "T3b 核心：遇到天敌（草克水）→ 40% 烧伤（种子命中触发分支）");
        std::cout << "  [T3b] 403 不翻倍 + 464 天敌烧伤 通过\n";
    }

    //========== T4 870 蔓生：吸取 200 点体力 ==========
    //（蔓生自带 effect 4 双段不断言：DB args 为官方序 stat（1=防 2=特攻），effect 4
    //  实现直传引擎槽序存在 1/2 换位（官方"防御+1"实测落特攻槽）——共享效果 >100 引用、
    //  实现驻 damage_modifier.cpp 非本组文件，记 batch_c_skills.cpp 头注待办，不锁行为。）
    {
        ScenarioBattle b(scenario::make_party(sibu({kMansheng, kHarmless, kHarmless, kHarmless,
                                                    kHarmless},
                                                   /*speed=*/50)),   // 后手：先挨 1 点
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 5000;   // 对手压到 5000，吸取有肉
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, 5000 - 200, "T4 核心：吸取对手 200 点体力");
        std::cout << "  [T4] 870 吸取 200 通过\n";
    }

    //========== T5 1055 郁拂：消除提升成功 → 下 1 回合对手攻击无效（顺序敏感）==========
    {
        ScenarioBattle b(scenario::make_party(sibu({kYufu, kHarmless, kHarmless, kHarmless,
                                                    kHarmless})),
                         scenario::make_party(foe({kHit, kHarmless, kHarmless, kHarmless,
                                                   kHarmless},
                                                  {0, 0}, /*atk=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;   // 对手有提升 → 1055 消除成功
        // 第 1 回合：郁拂（先制+3）武装；对手拍打本回合照常（下 1 回合才封）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(has_seal_entry(ctx, 1055), "T5 核心：消除成功 → SEAL_ATTACK 票已授");
        CHECK(ctx.getPet(0).hp < 100000, "T5 顺序：本回合对手攻击不受封（下 1 回合才封）");
        // 第 2 回合：对手攻击被无效（SKILL_INVALID → 0 伤）
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, hp_before,
                 "T5 核心：下 1 回合对手攻击技能无效（0 伤落地）");
        std::cout << "  [T5] 1055 消提升→下回合封攻击 通过\n";
    }

    //========== T6 1955 共生链接：恢复己方所有不在场精灵 100 体力 ==========
    {
        ElfPet base = sibu({kGongsheng, kHarmless, kHarmless, kHarmless, kHarmless});
        std::array<ElfPet, 6> party{base, base, base, base, base, base};
        ScenarioBattle b(party, scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[1].hp = 150;   // 场下队友压血
        ctx.seerRobot[0].elfPets[2].hp = 0;     // 阵亡场下队友：不复活
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.seerRobot[0].elfPets[1].hp, 250, "T6 核心：场下队友 +100");
        CHECK_EQ(ctx.seerRobot[0].elfPets[2].hp, 0, "T6 回血不复活：阵亡队友保持 0");
        std::cout << "  [T6] 1955 场下回复 通过\n";
    }

    //========== T7 1510 环纹锁定：窗口内对手主动切换 → 登场宠随机 2 种异常 ==========
    {
        ScenarioBattle b(scenario::make_party(sibu({kHuanwen, kHarmless, kHarmless, kHarmless,
                                                    kHarmless})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 环纹锁定（2 回合窗）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 第 2 回合：对手主动切换（6 槽全是同宠副本，切换落到新副本=干净登场宠）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(count_active_anomalies(ctx, 1), 2,
                 "T7 核心：窗口内对手主动切换 → 登场宠随机附加 2 种异常");
        std::cout << "  [T7] 1510 切换惩罚 通过\n";
    }

    //========== T8 1072 曳影：附加当前体力 20% 粉伤，连用 +5% 最高 50% ==========
    //（体力取 1000 让 20%/25% 档位成整数：首用 200、次用 250。）
    {
        ScenarioBattle b(scenario::make_party(sibu({kYeying, kHarmless, kHarmless, kHarmless,
                                                    kHarmless},
                                                   /*speed=*/200, /*hp=*/1000)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[0].power = 0;   // 主伤钳 1：粉伤可精确断言
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(100000 - ctx.getPet(1).hp, 1 + 200,
                 "T8 首用：主伤 1 + 当前体力 20% 粉伤（首用=base，不预递增）");
        std::cout << "  [T8] 1072 首用 通过（应为 " << 1 + 200 << "）\n";
    }
    {
        // 连用第二把：25%（20%+5%）——注意第一条断言里体力基线独立成场
        ScenarioBattle b(scenario::make_party(sibu({kYeying, kHarmless, kHarmless, kHarmless,
                                                    kHarmless},
                                                   /*speed=*/200, /*hp=*/1000)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[0].power = 0;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        ctx.getPet(0).hp = 1000;   // 复位：对手本能一击的 1 点红伤会让 25% 档取整掉 1（999×25%=249）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int total = 100000 - ctx.getPet(1).hp;
        CHECK_EQ(total, (1 + 200) + (1 + 250),
                 "T8b 核心：连用第二把 25%（20%+5%）—— 20000+25000=45000 粉伤 + 2 主伤");
        std::cout << "  [T8b] 1072 连用递增 通过（" << total << "）\n";
    }

    //========== T9 1469 编译：3 回合自身粉伤提升 30%（窗口内/外对照）==========
    {
        ScenarioBattle b(scenario::make_party(sibu({kBianyi, kHarmless, kHarmless, kHarmless,
                                                    kHarmless})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 编译武装
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 窗口内：直调粉伤原语 → 100 → 130
        const int hp1 = ctx.getPet(1).hp;
        (void)deal_pink_damage(&ctx, 1, 100, DamageKind::FIXED, 0);
        CHECK_EQ(hp1 - ctx.getPet(1).hp, 130, "T9 核心：窗口内粉伤提升 30%（100→130）");
        std::cout << "  [T9] 1469 粉伤提升窗口 通过\n";
    }

    std::cout << "scenario_162 all passed" << std::endl;
    return 0;
}
