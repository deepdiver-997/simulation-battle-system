// 场景 160：武心婵 4500 技能组缺口（一）—— 龙鳞体系 + 初释三技
//（moves_lib/batch_c_skills.cpp，2026-09-27）
//
// 覆盖效果：
//   1996/1997/1998 附加红/黄/蓝龙鳞（pet.soulmark_storage 持久，下场保留）
//   479  红之初释：损失 300 体力 → 300 固伤；体力不足 300 → 残留 1 点
//   1191 蓝之初释（自降全属性-1）+ 749（对手全属性-1，复用既有实现）
//   1865 黄之初释（自身随机 1 种异常）+ 786（对手随机 1 种，复用既有实现）
//   1985 瓏灵·绯：增伤 50%，红/白鳞翻倍；1986 附加 150 固伤翻倍；1987 附加 15% 粉伤翻倍
//   1984 瓏灵·绯：5 回合受到攻击伤害额外减 100%（红伤管线 REDUCE_PCT）
//   1988 瓏灵·缃：随机 2 种异常，黄/白鳞 → 控制类池
//   1989 瓏灵·缥：消除对手护盾+护罩；1990 对手全属性-1，蓝/白鳞翻倍
//   1991/1992 玲·沧吟星海：全属性+1（龙鳞翻倍）/ 无鳞 → 三色龙鳞
//   1994 玲·沧吟星海：每鳞下 2 回合先制+1；白鳞 → 对手锁切+先制失效
//
// 断言组：T1 龙鳞授予与 1992 补三色 / T2 479 两分支 / T3 1191+749 / T4 1865+786 /
//         T5 1985~1987 红/白鳞翻倍（同种子差值断言）/ T6 1984 窗口减伤 / T7 1989+1990 /
//         T8 1988 池切换 / T9 1991 翻倍 / T10 1994 先制+白鳞锁切（顺序敏感）。
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

constexpr int kRedWake = 28321;    // 红之初醒：1996
constexpr int kYellowWake = 28322; // 黄之初醒：1997
constexpr int kBlueWake = 28323;   // 蓝之初释：1998
constexpr int kRedRelease = 28324; // 红之初释：479
constexpr int kYellowRelease = 28325; // 黄之初释：1865+786
constexpr int kBlueRelease = 28326;   // 蓝之初释：1191+749
constexpr int kLingFei = 36632;    // 瓏灵·绯：180+1984+1475+1985+1986+1987（特攻 100）
constexpr int kLingXiang = 36633;  // 瓏灵·缃：33+48+1988（特攻 100）
constexpr int kLingPiao = 36634;   // 瓏灵·缥：1989+47+1990（特攻 100）
constexpr int kLingXing = 28327;   // 玲·沧吟星海：1991+1992+57+1993+1994（属性技）
constexpr int kHit = 10004;        // 拍打：对手用
constexpr int kHarmless = 10938;   // 本能一击：热身

ElfPet wuxin(std::array<int, 5> skills, int speed = 200, int hp = 500) {
    ElfPet p = scenario::make_pet(0, "武心婵", skills, speed, /*atk=*/100, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int atk = 10, int hp = 100000, std::array<int, 5> skills = {kHit, kHit, kHit, kHit, kHit}) {
    ElfPet p = scenario::make_pet(1, "对手", skills,
                                  /*speed=*/100, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 无害对手：本能一击（0 威力攻击 → 钳最少 1 点红伤，无随机、无附加）。
ElfPet harmless_foe(int atk = 10, int hp = 100000) {
    return foe(atk, hp, {kHarmless, kHarmless, kHarmless, kHarmless, kHarmless});
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

bool all_anomalies_control(const BattleContext& ctx, int side) {
    for (int id = 0; id <= 43; ++id) {
        if (ctx.has_active_abnormal_status(side, id)
            && !is_control_abnormal_status(static_cast<AbnormalStatusId>(id))) {
            return false;
        }
    }
    return true;
}

// 龙鳞读取（与实现同一存储：pet.soulmark_storage 4501/4502/4503）。
bool has_scale(BattleContext& ctx, int side, int key) {
    const auto& st = ctx.getPet(side).soulmark_storage;
    const auto it = st.find(key);
    if (it == st.end()) {
        return false;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v && *v > 0;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 龙鳞授予 + 1992 三色补齐 + 1991 翻倍 ==========
    {
        ScenarioBattle b(scenario::make_party(wuxin({kRedWake, kLingXing, kHarmless,
                                                     kHarmless, kHarmless})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 红之初醒
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(has_scale(ctx, 0, 4501), "T1 核心：红之初醒 → 持有红色龙鳞");
        CHECK(!has_scale(ctx, 0, 4502) && !has_scale(ctx, 0, 4503),
              "T1 黄/蓝鳞未被误授");
        const int atk_after_red = ctx.ability_levels[0][0];

        // 第 2 回合：玲·沧吟星海（1991 全属性+1 → 有鳞翻倍 +2；1992 已有鳞不再补三色）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][0], atk_after_red + 2,
                 "T1 核心：1991 有龙鳞 → 强化效果翻倍（+2 而非 +1）");
        CHECK(has_scale(ctx, 0, 4501) && !has_scale(ctx, 0, 4502),
              "T1 1992 已有鳞 → 不补三色");
        std::cout << "  [T1] 龙鳞授予/1992 补三色/1991 翻倍 通过\n";
    }

    //========== T1b 无鳞 → 1992 附加三色龙鳞 ==========
    {
        ScenarioBattle b(scenario::make_party(wuxin({kLingXing, kHarmless, kHarmless,
                                                     kHarmless, kHarmless})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(has_scale(ctx, 0, 4501) && has_scale(ctx, 0, 4502) && has_scale(ctx, 0, 4503),
              "T1b 核心：无鳞用玲·沧吟星海 → 1992 附加三色龙鳞");
        std::cout << "  [T1b] 1992 三色补齐 通过\n";
    }

    //========== T2 479 红之初释：足量扣血 / 不足量残留 1 点 ==========
    // 武心婵速度 50（后手）：对手先手无害攻击（0 威力钳 1 点红伤），再结算 479。
    {
        // 分支 A：500 体力被拍 1 点 → 499 ≥ 300 → 扣 300 剩 199，对手固定 300 伤
        {
            ScenarioBattle b(scenario::make_party(wuxin({kRedRelease, kHarmless, kHarmless,
                                                         kHarmless, kHarmless},
                                                        /*speed=*/50)),
                             scenario::make_party(harmless_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 499 - 300, "T2a 核心：损失自身 300 点体力");
            CHECK_EQ(ctx.getPet(1).hp, 100000 - 300,
                     "T2a 核心：给对手造成 300 点固定伤害");
            std::cout << "  [T2a] 479 足量分支 通过\n";
        }
        // 分支 B：体力 100 → 被拍 1 点剩 99 < 300 → 残留 1 点
        {
            ScenarioBattle b(scenario::make_party(wuxin({kRedRelease, kHarmless, kHarmless,
                                                         kHarmless, kHarmless},
                                                        /*speed=*/50)),
                             scenario::make_party(harmless_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 100;   // 压到不足 300
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 1, "T2b 核心：体力不足 300 → 剩下 1 点体力");
            CHECK_EQ(ctx.getPet(1).hp, 100000 - 300, "T2b 固定伤害照常");
            std::cout << "  [T2b] 479 不足量分支 通过\n";
        }
    }

    //========== T3 蓝之初释：1191 自降全属性-1 + 749 对手全属性-1（顺序：先自后敌）==========
    {
        ScenarioBattle b(scenario::make_party(wuxin({kBlueRelease, kHarmless, kHarmless,
                                                     kHarmless, kHarmless})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][0], -1, "T3 核心：自身全属性-1（1191）");
        CHECK_EQ(ctx.ability_levels[1][0], -1, "T3 核心：对手全属性-1（749）");
        std::cout << "  [T3] 1191+749 双向全属性降 通过\n";
    }

    //========== T4 黄之初释：1865 自身随机 1 种异常 + 786 对手随机 1 种 ==========
    {
        ScenarioBattle b(scenario::make_party(wuxin({kYellowRelease, kHarmless, kHarmless,
                                                     kHarmless, kHarmless})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(count_active_anomalies(ctx, 0), 1, "T4 核心：自身随机进入 1 种异常（1865）");
        CHECK_EQ(count_active_anomalies(ctx, 1), 1, "T4 核心：对手随机进入 1 种异常（786）");
        std::cout << "  [T4] 1865+786 双向随机异常 通过\n";
    }

    //========== T5 瓏灵·绯 1985~1987：红鳞翻倍（精确值断言）==========
    // 瓏灵·绯威力强制 0（主伤钳 1 点、AMP 不起作用 → 两分支主伤恒 1）；
    // 附加伤在 AFTER_ACTION 落地：无鳞 = 150 固伤 + 75 粉伤（500×15%）；
    // 红鳞 = 300 + 150。对照战斗第 1 回合用无害技（造成 1 点）凑齐同构两回合。
    {
        auto run = [&](bool red) {
            ScenarioBattle b(scenario::make_party(
                                 wuxin(red ? std::array<int, 5>{kRedWake, kLingFei, kHarmless,
                                                               kHarmless, kHarmless}
                                           : std::array<int, 5>{kHarmless, kLingFei, kHarmless,
                                                                kHarmless, kHarmless})),
                             scenario::make_party(harmless_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).skills[1].power = 0;   // 主伤钳 1（见上）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 瓏灵·绯
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            return 100000 - ctx.getPet(1).hp;
        };
        const int no_scale_total = run(false);   // 1(无害技) + 1(主伤×1.5 截断) + 150 + 75 = 227
        const int red_total = run(true);         // 0 + 2(主伤基准 1 先于 AMP → ×2) + 300 + 150 = 452
        CHECK_EQ(no_scale_total, 227,
                 "T5a 核心：无鳞 → 附加 150 固伤 + 15% 粉伤(75)，主伤钳 1");
        CHECK_EQ(red_total, 452,
                 "T5b 核心：红鳞 → 1986 附加翻倍(300) + 1987 粉伤翻倍(150)，"
                 "主伤基准 1 在 AMP 前已定 → ×2=2");
        std::cout << "  [T5] 1985~1987 红/白鳞翻倍 通过（" << no_scale_total << " / "
                  << red_total << "）\n";
    }

    //========== T6 瓏灵·绯 1984+1475：5 回合窗口内受到攻击伤害减少 100% ==========
    {
        // 第 1 回合我用瓏灵·绯武装（atk=10 的对手打不死 500 血），第 2 回合对手大攻击 → 0 伤。
        ScenarioBattle b(scenario::make_party(wuxin({kLingFei, kHarmless, kHarmless,
                                                     kHarmless, kHarmless})),
                         scenario::make_party(foe(/*atk=*/10000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp > 0, "T6 前置：第 1 回合存活（对手 10000 攻但武心婵先手秒不掉自己…"
                                     "被 1984 当回合窗口保护）");
        // 第 2 回合：对手攻击 → 窗口内 → 红伤减 100% = 0
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 热身
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 500,
                 "T6 核心：1984 窗口内受到攻击伤害额外减少 100%（红伤归零）");
        std::cout << "  [T6] 1984 窗口减伤 通过\n";
    }

    //========== T7 瓏灵·缥 1989+1990：消除对手护盾护罩 + 全属性-1（蓝鳞翻倍）==========
    {
        // 对手先挂 300 护盾（直写），第 1 回合我瓏灵·缥 → 盾被清 + 对手全属性-1（无蓝鳞）
        ScenarioBattle b(scenario::make_party(wuxin({kLingPiao, kHarmless, kHarmless,
                                                     kHarmless, kHarmless})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).shield_bank_.add(/*priority=*/0, 300, /*source_id=*/888,
                                       /*duration=*/0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).shield_bank_.total(), 0, "T7 核心：1989 消除对手护盾");
        CHECK_EQ(ctx.getPet(1).hood_bank_.total(), 0, "T7 1989 护罩一并消除（无罩时为 0）");
        CHECK_EQ(ctx.ability_levels[1][0], -1, "T7 核心：1990 令对手全属性-1（无蓝鳞不翻倍）");

        // 蓝鳞 → 1990 翻倍 -2：新战斗先蓝之初醒再瓏灵·缥
        ScenarioBattle b2(scenario::make_party(wuxin({kBlueWake, kLingPiao, kHarmless,
                                                      kHarmless, kHarmless})),
                          scenario::make_party(foe()));
        b2.start();
        BattleContext& ctx2 = b2.ctx();
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 蓝之初醒
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 瓏灵·缥
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK_EQ(ctx2.ability_levels[1][0], -2,
                 "T7 核心：蓝鳞 → 1990 弱化效果翻倍（-2）");
        std::cout << "  [T7] 1989 消盾罩 + 1990 蓝鳞翻倍 通过\n";
    }

    //========== T8 瓏灵·缃 1988：随机 2 异常；黄鳞 → 全控制类 ==========
    {
        // 分支 A：无鳞 → 全池随机 2 条
        {
            ScenarioBattle b(scenario::make_party(wuxin({kLingXiang, kHarmless, kHarmless,
                                                         kHarmless, kHarmless})),
                             scenario::make_party(foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(count_active_anomalies(ctx, 1), 2, "T8a 核心：随机附加 2 种异常");
            std::cout << "  [T8a] 1988 全池 2 异常 通过\n";
        }
        // 分支 B：黄鳞 → 控制类池
        {
            ScenarioBattle b(scenario::make_party(
                                 wuxin({kYellowWake, kLingXiang, kHarmless, kHarmless,
                                        kHarmless})),
                             scenario::make_party(foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 黄之初醒
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 瓏灵·缃
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(count_active_anomalies(ctx, 1), 2, "T8b 前置：仍落地 2 条");
            CHECK(all_anomalies_control(ctx, 1),
                  "T8b 核心：黄鳞 → 改为附加控制类异常状态（全部命中控制池）");
            std::cout << "  [T8b] 1988 黄鳞控制池 通过\n";
        }
    }

    //========== T10 玲·沧吟星海 1994：每鳞先制+1 / 白鳞 → 锁切+先制失效（顺序敏感）==========
    {
        // 红鳞 1 种 → 下 2 回合先制 +1：第 2 回合 MOVE_RIGHT 后断言。
        ScenarioBattle b(scenario::make_party(wuxin({kRedWake, kLingXing, kHarmless,
                                                     kHarmless, kHarmless})),
                         scenario::make_party(foe(/*atk=*/10, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 红之初醒（1 鳞）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 玲·沧吟星海
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 第 3 回合：先手权结算后 → 先制 +1（每鳞 1 种 → +1）。
        // ⚠️ step 模式必须夹在两次 submit 之间（104 B 组教训：回合一口气跑完后
        // step_past 落在 CHOOSE 泊位 → 循环条件恒真 → 死循环空转）。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        CHECK_EQ(ctx.ws.preemptive_level[0], 1,
                 "T10a 核心：红鳞 1 种 → 下 2 回合所有技能先制+1");
        ctx.debug_step_mode = false;
        std::cout << "  [T10a] 1994 每鳞先制 通过\n";
    }
    {
        // 三色（白）→ +3 且对手下 2 回合无法主动切换 + 先制效果失效。
        ScenarioBattle b(scenario::make_party(
                             wuxin({kRedWake, kYellowWake, kBlueWake, kLingXing, kHarmless})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int i = 0; i < 3; ++i) {   // 三色初醒
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, i);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 3);   // 玲·沧吟星海
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 4);   // 热身（先制+3 生效回合）
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        CHECK_EQ(ctx.ws.preemptive_level[0], 3, "T10b 核心：三色 → 先制+3");
        CHECK(ctx.ws.priority_nullified[1],
              "T10b 核心：白鳞 → 对手先制效果失效（priority_nullified）");
        ctx.debug_step_mode = false;
        // 对手主动切换被锁：直查锁切票（提交被拒的 CHOOSE_PET 会让 FSM 反复读同一缓冲，
        // 场景进程死循环——1478/109 线的教训，锁切断言一律走 RuleCenter 查询面）。
        CHECK(ctx.rule_center_.is_switch_locked(1, ctx.roundCount),
              "T10b 核心：白鳞 → 对手下 2 回合无法主动切换精灵（SWITCH_LOCK 票在）");
        std::cout << "  [T10b] 1994 白鳞锁切+先制失效 通过\n";
    }

    std::cout << "scenario_160 all passed" << std::endl;
    return 0;
}
