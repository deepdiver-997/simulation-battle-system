// 场景 781 —— 帝皇之御 3512：魂印「御」（1106）+ 技能组（1275/753/1276/995/1277）
//
// 机制口径（详见 resources/soul_lib/diwang_3512.cpp 与
// resources/moves_lib/diwang_3512_skills.cpp 文件头，2026-09-30 用户实测定档）：
//   · 魂印首登窗口 [R, R+1]：免异常（Ancient）+ 先制+2 + 免死（死亡拦截器，HP_CONSUME/
//     秒杀/EFFECT 不拦）+ 反弹（拦截时把 last_landed_hit 等量按**固定伤害**弹回攻击方，
//     kind 门 = 红伤/属性直伤）；窗口是时限不是次数；复活/再登场不重置不重授。
//   · 守御八方自爆：全队 2 次"必先(tier1)+必爆+命中修正"（嗑药/切换/被控不消耗；
//     命中修正 = 基础命中率钉 100+必闪无效〔accuracy_fix〕+ 无弱化时另予必中凭证）。
//   · 生命永恒自爆：对手 5 回合属性技能命中效果失效（SEAL_ATTRIBUTE_HIT，TEAM 域，
//     施放方死亡不解除 → 无法被断）。
//   · 死亡传承：己方下一只登场 2 回合内受攻击伤害/固定伤害 -50%。
//   · 救赎之羽尾段：自爆转 1/2 消耗量的百分比粉伤；华光洗礼尾段：消强成功吸取 max_hp/3；
//     神谕涤罪曲：体力高于对手→消耗至残留 1 + 350~550 伤害；低于→自爆转等量固定伤+瘫痪。
//
// 断言组：
//   A 魂印窗口：A1 免异常（毒粉 100% 被挡） A2 免死+反弹（=受击前体力等量固定伤）
//     A3 窗口内二次拦截（时限不是次数） A4 窗口过期死亡、无反弹
//   B 守御八方：B1 自爆即死（免死不响应 HP_CONSUME）+死亡跳过对手出手
//     B2 必先+必爆+命中修正旗标、传承次数 2→1  B3 嗑药不消耗
//     B4 弱化 → 无必中凭证（accuracy_fix 仍在）、次数 1→0  B5 次数耗尽：必闪打空（对照）
//   C 生命永恒：C1 自爆+封属存活（TEAM/施放方死亡不解除） C2 属性技命中效果失效、
//     C3 攻击技照常  C4 窗口过期恢复
//   D 救赎之羽：疲惫 100% + 自爆转 1/2 消耗量粉伤
//   E 神谕涤罪曲：E1 体力高→残留 1+350~550  E2 体力低→等量固定伤+瘫痪+自爆
//   F 华光洗礼：消强成功 → 吸取 max_hp/3（满血封顶）
//   G 死亡传承：下只登场 2 回合红伤减半、第 3 回合恢复
//
// 确定性纪律：全部技能 crit_rate=0（make_pet 默认）、命中率 100/必中或走凭证，
// 伤害断言用"掉血 = 精确等式/区间"，rand 只出现在允许区间断言的 350~550 与 B4 弱化轮
// （该轮不断言命中结果，只断言旗标与消费）。
#include <array>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kDiwangId = 3512;
constexpr int kGaoSu = 20017;      // 高速移动：属性技，自身加速（无伤害噪声）
constexpr int kYanFan = 10037;     // 燕返：威力 60，必中
constexpr int kZhuangJi = 10001;   // 撞击：威力 35，命中 95（非必中——验凭证/必闪对照）
constexpr int kDuFen = 20020;      // 毒粉：属性技，100% 中毒（验免异常）
constexpr int kShouYuBaFang = 31505;
constexpr int kShengMingYongHeng = 25861;
constexpr int kJiuShuZhiYu = 31506;
constexpr int kHuaGuangXiLi = 31502;
constexpr int kShenYuDiZuiQu = 31507;

// 与 resources/moves_lib/diwang_3512_skills.cpp 的 plugin_storage 段对齐。
constexpr int kVoucherChargesKey = 35120100;

ElfPet diwang(std::array<int, 5> skills, int speed = 50, int atk = 100, int hp = 5000) {
    return scenario::make_pet(kDiwangId, "帝皇之御", skills, speed, atk, hp,
                              SoulMark(1106, "御", "帝皇之御魂印"));
}

ElfPet plain(std::array<int, 5> skills, int speed, int atk, int hp, int id = 20201) {
    return scenario::make_pet(id, "对手", skills, speed, atk, hp);
}

void submit_skills(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
}

int voucher_charges(BattleContext& ctx, int side) {
    auto it = ctx.plugin_storage.find(kVoucherChargesKey + side);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 魂印首登窗口（免异常单开一战；主战 R1/R2 两次致死拦截、R3 过期）═════
    {
        auto foe = plain({kDuFen, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/300, /*atk=*/100, /*hp=*/1000000000);
        scenario::ScenarioBattle battle(
            scenario::make_party(diwang({kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu})),
            scenario::make_party(foe));
        BattleContext& ctx = battle.ctx();
        battle.start();
        submit_skills(battle, 0, 0);
        battle.drive();
        CHECK(!ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Poison)),
              "A1 免疫异常：毒粉未命中中毒");
        CHECK_EQ(ctx.getPet(0).hp, 5000, "A1 毒粉无伤害");
        std::cout << "  [A1] 免异常：100% 毒粉被窗口免疫挡下 ✓" << std::endl;
    }
    {
        scenario::ScenarioBattle battle(
            scenario::make_party(diwang({kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu})),
            scenario::make_party(plain({kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
                                       /*speed=*/300, /*atk=*/100000, /*hp=*/1000000000)));
        BattleContext& ctx = battle.ctx();
        battle.start();

        // A2 R1 致死：免死残留 1 点 + 反弹。落血记录 = 护盾后**全额**伤害（2026-09-24
        // 机盖族口径：过量击杀不缩水）→ 打出的红伤远超 5000，反弹跟着全额走。
        submit_skills(battle, 1, 1);
        battle.drive();
        CHECK_EQ(ctx.getPet(0).hp, 1, "A2 免死：残留 1 点体力");
        const int dealt1 = 1000000000 - ctx.getPet(1).hp;
        CHECK(dealt1 > 5000, "A2 反弹 = 攻击方打出的全额红伤（过量不缩水）");

        // A3 R2 窗口内二次拦截：本次实际只受 1 点（体力只剩 1），反弹仍是全额量级——
        // 证明弹的是"造成量"不是"承伤量"。（伤害有 ±1% 级随机浮动 → 只断量级。）
        submit_skills(battle, 1, 1);
        battle.drive();
        CHECK_EQ(ctx.getPet(0).hp, 1, "A3 二次拦截：仍残留 1 点");
        const int dealt2 = 1000000000 - ctx.getPet(1).hp - dealt1;
        CHECK(dealt2 > 5000, "A3 受 1 点仍全额反弹");

        // A4 R3 窗口过期（[R1,R2]）：致死直接死亡，无反弹。
        submit_skills(battle, 1, 1);
        battle.drive();
        CHECK_EQ(ctx.getPet(0).hp, 0, "A4 窗口过期：死亡成立");
        CHECK_EQ(ctx.getPet(1).hp, 1000000000 - dealt1 - dealt2, "A4 死亡无反弹");
        std::cout << "  [A] 魂印窗口：免死残留1/等量固定伤反弹/二次拦截/过期死亡 ✓" << std::endl;
    }

    // ══ B 守御八方（次数型必先+必爆+命中修正）══════════════════════════════
    {
        auto heir = plain({kYanFan, kZhuangJi, kGaoSu, kGaoSu, kGaoSu},
                          /*speed=*/100, /*atk=*/100, /*hp=*/50000, /*id=*/202);
        auto enemy = plain({kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
                           /*speed=*/300, /*atk=*/100, /*hp=*/1000000000);
        enemy.soulMark = scenario::make_dodge_soul_mark();   // 对手 100% 闪避（验必闪对照）
        scenario::ScenarioBattle battle(
            {diwang({kShouYuBaFang, kGaoSu, kGaoSu, kGaoSu, kGaoSu}), heir, heir, heir, heir,
             heir},
            {enemy, enemy, enemy, enemy, enemy, enemy},
            MedicineStock{{300011, 5}}, MedicineStock{});
        BattleContext& ctx = battle.ctx();
        battle.start();

        // B1 自爆：HP_CONSUME 穿残留免死 → 即死；死亡跳过对手出手。
        submit_skills(battle, 0, 0);
        battle.drive();
        CHECK_EQ(ctx.getPet(0).hp, 0, "B1 自爆即死（免死不响应 HP_CONSUME）");
        CHECK_EQ(voucher_charges(ctx, 0), 2, "B1 传承 2 次挂账");
        // 守御八方是攻击类技能（威力 0 保底 1 点红伤）；除这 1 点外对手未行动
        // （死亡 → 本回合直接进换宠流程，对手出手被跳过）。
        CHECK_EQ(ctx.getPet(1).hp, 1000000000 - 1, "B1 死亡跳过对手出手（仅自爆保底 1 点红伤）");
        battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        battle.drive();

        // B2 必先+必爆+命中修正旗标；传承 2→1。
        submit_skills(battle, 0, 0);   // 继承者燕返 vs 对手燕返
        battle.drive();
        CHECK(ctx.ws.must_hit_grant[0], "B2 无弱化 → 条件必中凭证授予");
        CHECK(ctx.ws.accuracy_fix_grant[0], "B2 基础命中率修正 100 武装");
        CHECK(ctx.crit_happened[0], "B2 必定暴击（crit_rate=0 也必暴）");
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "B2 必先：速度 100 先于速度 300");
        CHECK_EQ(voucher_charges(ctx, 0), 1, "B2 出手一次 → 2→1");
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::HIT,
                 "B2 燕返（必中）在 100% 闪避面前照常命中");

        // B3 嗑药回合：完全跳过出手 → 不武装、不消耗。
        battle.submit_operation(0, BattleFsm::ActionType::USE_MEDICINE, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        CHECK_EQ(voucher_charges(ctx, 0), 1, "B3 嗑药不消耗（仍 1 次）");
        CHECK(!ctx.ws.must_hit_grant[0], "B3 嗑药回合未武装必中");

        // B4 弱化（命中-1）→ 凭证不授、accuracy_fix 仍武装；该轮消费最后一次（结果不断言）。
        REQUIRE(stat_drop(&ctx, /*target=*/0, /*stat=*/5, /*amount=*/1, /*actor=*/1)
                == StatDropResult::SUCCESS);
        submit_skills(battle, 1, 0);   // 继承者撞击 vs 对手高速移动
        battle.drive();
        CHECK(!ctx.ws.must_hit_grant[0], "B4 有命中弱化 → 必中凭证不授（弱化仍可能 Miss）");
        CHECK(ctx.ws.accuracy_fix_grant[0], "B4 基础命中率修正仍在（钉 100、必闪无效）");
        CHECK_EQ(voucher_charges(ctx, 0), 0, "B4 真实出手 → 1→0");

        // B5 次数耗尽对照：撞击（95%）打 100% 闪避 → 必 miss；必爆也退场。
        const int enemy_before = ctx.getPet(1).hp;
        submit_skills(battle, 1, 0);
        battle.drive();
        CHECK_EQ(ctx.getPet(1).hp, enemy_before, "B5 撞击被 100% 闪避打空（无修正对照）");
        CHECK(!ctx.crit_happened[0], "B5 无必爆传承 → 不暴击");
        std::cout << "  [B] 守御八方：自爆即死/全队2次必先必爆/嗑药不消耗/弱化劈裂凭证/耗尽对照 ✓"
                  << std::endl;
    }

    // ══ C 生命永恒（自爆 → 对手 5 回合属性技能命中效果失效）═════════════════
    {
        auto heir = plain({kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                          /*speed=*/100, /*atk=*/100, /*hp=*/50000, /*id=*/202);
        auto enemy = plain({kGaoSu, kYanFan, kGaoSu, kGaoSu, kGaoSu},
                           /*speed=*/300, /*atk=*/100, /*hp=*/1000000000);
        scenario::ScenarioBattle battle(
            {diwang({kShengMingYongHeng, kGaoSu, kGaoSu, kGaoSu, kGaoSu}), heir, heir, heir,
             heir, heir},
            scenario::make_party(enemy));
        BattleContext& ctx = battle.ctx();
        battle.start();

        // C1 自爆 + 封属挂上、施放方死亡不解除（TEAM 域）。
        submit_skills(battle, 0, 0);
        battle.drive();
        CHECK_EQ(ctx.getPet(0).hp, 0, "C1 自爆即死");
        bool seal_found = false;
        for (const RuleTicket& t : ctx.rule_center_.entries()) {
            if (t.source_effect_id == 753) {
                seal_found = true;
            }
        }
        CHECK(seal_found, "C1 属性技能命中失效封属在场（施放方已死仍存活）");
        battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        battle.drive();

        // C2 对手属性技能 → 命中效果失效：技能照常命中（exec=HIT，无无效补偿），
        // 但**效果不注册**——高速移动的速度等级没有加上（速度=槽 4）。
        submit_skills(battle, 0, 0);   // 继承者高速移动 vs 对手高速移动
        battle.drive();
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::HIT,
                 "C2 命中效果失效=技能照常命中（不走无效补偿）");
        CHECK_EQ(ctx.ability_levels[1][4], 0, "C2 效果未注册：速度等级没有 +2");

        // C3 攻击技能照常命中（不受封属影响）。
        submit_skills(battle, 0, 1);   // 继承者高速移动 vs 对手燕返
        battle.drive();
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::HIT, "C3 攻击技不受封属影响");

        // C4 窗口过期（自爆在 R1，[R1..R5]）：R5 仍失效、R6 恢复（速度等级真的加上）。
        for (int r = 0; r < 2; ++r) {   // R4、R5
            submit_skills(battle, 0, 0);
            battle.drive();
        }
        CHECK_EQ(ctx.ability_levels[1][4], 0, "C4 R5 仍失效（5 回合窗）");
        submit_skills(battle, 0, 0);   // R6
        battle.drive();
        CHECK_EQ(ctx.ability_levels[1][4], 2, "C4 R6 过期恢复：速度 +2 生效");
        std::cout << "  [C] 生命永恒：自爆封属存活/属性技失效/攻击技照常/5回合过期 ✓" << std::endl;
    }

    // ══ D 救赎之羽（疲惫 + 自爆转 1/2 消耗量粉伤）═══════════════════════════
    {
        auto enemy = plain({kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                           /*speed=*/300, /*atk=*/100, /*hp=*/1000000000);
        scenario::ScenarioBattle battle(
            scenario::make_party(diwang({kJiuShuZhiYu, kGaoSu, kGaoSu, kGaoSu, kGaoSu})),
            scenario::make_party(enemy));
        BattleContext& ctx = battle.ctx();
        battle.start();
        submit_skills(battle, 0, 0);
        battle.drive();
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)),
              "D 100% 疲惫命中");
        // 救赎之羽同为威力 0 攻击类技能：1 点保底红伤 + 2500 粉伤。
        CHECK_EQ(ctx.getPet(1).hp, 1000000000 - 2501, "D 自爆转 1/2 消耗量（5000/2）粉伤 + 保底 1 红伤");
        CHECK_EQ(ctx.getPet(0).hp, 0, "D 自爆即死");
        std::cout << "  [D] 救赎之羽：疲惫 + 消耗量 1/2 粉伤 ✓" << std::endl;
    }

    // ══ E 神谕涤罪曲（双分支）══════════════════════════════════════════════
    {
        // E1 体力高于对手：消耗至残留 1 点 + 350~550 伤害。
        auto foe_low = plain({kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/100, /*hp=*/3000);
        scenario::ScenarioBattle battle1(
            scenario::make_party(diwang({kShenYuDiZuiQu, kGaoSu, kGaoSu, kGaoSu, kGaoSu})),
            scenario::make_party(foe_low));
        BattleContext& ctx1 = battle1.ctx();
        battle1.start();
        submit_skills(battle1, 0, 0);
        battle1.drive();
        CHECK_EQ(ctx1.getPet(0).hp, 1, "E1 消耗至残留 1 点 HP（不是死亡）");
        const int loss1 = 3000 - ctx1.getPet(1).hp;
        CHECK(loss1 >= 351 && loss1 <= 551, "E1 伤害 350~550 + 保底 1 红伤");
        std::cout << "  [E1] 涤罪曲体力高分支：残留 1 + " << loss1 << " 伤害 ✓" << std::endl;
    }
    {
        // E2 体力低于对手：自爆转等量固定伤 + 瘫痪 2 回合。
        auto foe_high = plain({kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/300, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle2(
            scenario::make_party(diwang({kShenYuDiZuiQu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                        /*speed=*/50, /*atk=*/100, /*hp=*/3000)),
            scenario::make_party(foe_high));
        BattleContext& ctx2 = battle2.ctx();
        battle2.start();
        submit_skills(battle2, 0, 0);
        battle2.drive();
        CHECK_EQ(ctx2.getPet(1).hp, 5000 - 3001, "E2 等量消耗量固定伤（3000）+ 保底 1 红伤");
        CHECK(ctx2.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
              "E2 瘫痪命中");
        CHECK_EQ(ctx2.getPet(0).hp, 0, "E2 自爆即死");
        std::cout << "  [E2] 涤罪曲体力低分支：等量固定伤 + 瘫痪 ✓" << std::endl;
    }

    // ══ F 华光洗礼（消强成功 → 吸取 max_hp/3）══════════════════════════════
    {
        // 对手体力取 300000（max/3=100000）：heal_impl 的修正乘算是 int 通道，
        // 10^9 级测试数值会溢出成负数被钳 0（真实精灵体力 ~10^5，无此问题）。
        auto enemy = plain({kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                           /*speed=*/300, /*atk=*/100, /*hp=*/300000);
        scenario::ScenarioBattle battle(
            scenario::make_party(diwang({kHuaGuangXiLi, kGaoSu, kGaoSu, kGaoSu, kGaoSu})),
            scenario::make_party(enemy));
        BattleContext& ctx = battle.ctx();
        battle.start();
        ctx.getPet(0).hp = 2000;   // 留出回复空间（吸取 100000 → 满血封顶）
        REQUIRE(stat_change(&ctx, /*target=*/1, /*stat=*/0, /*delta=*/2)
                == StatChangeResult::SUCCESS);
        submit_skills(battle, 0, 0);
        battle.drive();
        CHECK_EQ(ctx.getPet(0).hp, 5000, "F 吸取 max_hp/3（回满封顶 → 消强成功）");
        CHECK(ctx.getPet(1).hp <= 200000, "F 对手被吸取 ≥ 1/3 最大体力");
        std::cout << "  [F] 华光洗礼：消强成功吸取 1/3 最大体力 ✓" << std::endl;
    }

    // ══ G 死亡传承（下只登场 2 回合受攻击伤害/固定伤害 -50%）════════════════
    {
        auto heir = plain({kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
                          /*speed=*/100, /*atk=*/100, /*hp=*/50000, /*id=*/202);
        auto enemy = plain({kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
                           /*speed=*/300, /*atk=*/100, /*hp=*/1000000000);
        scenario::ScenarioBattle battle(
            {diwang({kShengMingYongHeng, kGaoSu, kGaoSu, kGaoSu, kGaoSu}), heir, heir, heir,
             heir, heir},
            scenario::make_party(enemy));
        BattleContext& ctx = battle.ctx();
        battle.start();

        submit_skills(battle, 0, 0);   // R1 表姐自爆（HP_CONSUME，干净死亡）
        battle.drive();
        battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        battle.drive();

        auto heir_loss = [&battle, &ctx]() {
            const int before = ctx.getPet(0).hp;
            submit_skills(battle, 0, 0);   // 继承者燕返（必中）vs 对手燕返（必中）
            battle.drive();
            return before - ctx.getPet(0).hp;
        };
        const int r2 = heir_loss();   // 传承窗（继承者第 1 个完整回合 = R2）
        const int r3 = heir_loss();   // R3：已过期
        const int r4 = heir_loss();   // 过期对照
        // 红伤有 ±1% 级随机浮动 → 用宽带宽断。继承者 R1 尾场登场，按"登场回合算第 1 回合"
        // 官方计数（幻影蝶帖：到登场后第 3 回合的回合开始时结束）→ 保护窗 = R1 残段 + R2，
        // R3 起恢复全额。
        CHECK(r2 > 0 && r2 * 10 > r4 * 4 && r2 * 10 < r4 * 6, "G 传承期红伤减半（R2 ≈ R4/2）");
        CHECK(r3 * 10 > r4 * 9 && r3 * 10 < r4 * 11, "G R3 起恢复全额（窗口按登场回合计数）");
        std::cout << "  [G] 死亡传承：下只登场 2 回合减半（按登场回合计数）、R3 恢复 ✓" << std::endl;
    }

    std::cout << "scenario_781 PASS (帝皇之御：魂印御 + 守御八方/生命永恒/救赎之羽/华光洗礼/涤罪曲)"
              << std::endl;
    return 0;
}
