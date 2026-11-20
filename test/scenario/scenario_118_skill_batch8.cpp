#include <utils/sbs_rand.h>
// 场景 118：技能批 8 —— 剩余头部家族清理（2026-09-23）。
//
// 口径来源：effect_info 模板原文（数据库为准）。本场景只锁**机制面**（各家族一个代表），
// 同族变体（645≈37、1065/1067≈532/707/597、1220≈1224、1124≈913、905/1199≈797 族）
// 复用已锁定的机器（批 2/6 场景 067/072 已绿），不重复展开。
// 概率路径（88/470 的掷点）无 100% 数据样例，掷点本身由 roll_percent 已覆盖，不在本场景锁。
//
// 验证点：
//   A  威力视图族：37 HP阈值×2 / 645 HP阈值×3 / 925 满体力+50%（视图值精确断言）
//   B  回复/固伤：466 定点回血 / 467 烧伤条件固伤 / 1084 异常翻倍固伤（三场差分=固伤值）
//   C  减伤窗口：463 点数减伤（关系断言）/ 54 乘算减伤（ws 槽 + 减半断言）
//   D  循环族：927 低血回血+等量百分比伤害（低/高血两向）
//   E  窃取族：583 随机PP归零 / 444 全PP一来一去 / 425 随机属性吸取（总和守恒）
//   F  免疫/封属：570 次数免攻击 / 980 先手封属性
//   G  消除成功族：945/485 消除成功回满 / 913 下回合额外增伤 / 905 封回血 /
//      1224 必暴窗口 / 1199 吸取成功封回血
//   H  必中/条件必暴/增伤/暴击：81 必中穿闪避 / 582 未击败必暴 / 1048 异常增伤 /
//      1356 暴击附加百分比伤害
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物攻 40（无效果）
constexpr int kGaoSu = 20017;      // 高速移动：属性技（+2 速度，无伤害）
constexpr int k37 = 10168;         // 惊雷切：[37] args[2,2] → HP<1/2 威力×2（pw55）
constexpr int k645 = 16330;        // 玄铁重盾：[645] args[3,3] → HP<1/3 威力×3（pw60）
constexpr int k925 = 30080;        // 鬼面神枪：[1025,925] → 满体力威力+50%（pw85）
constexpr int k466 = 14293;        // 卜力回血泡：[466] → 恢复50点体力
constexpr int k467 = 14049;        // 炎火击：[467] args[2,200] → 对手烧伤则+200固伤
constexpr int k1084 = 30323;       // 万古耀目破：[1084] args[250] → 固伤250，异常翻倍
constexpr int k463 = 13998;        // 玄铁天雷盾：[463] args[2,150] → 2回合受击-150
constexpr int k54 = 10240;         // 绿光波：[54] args[1,2] → 1回合对方攻击伤害减半
constexpr int k927 = 24869;        // 电闪天地：[885,927,756] → 927 拿 [4,2,2]：4回合 回1/2 低血(<1/2)附等量粉
constexpr int k583 = 17853;        // 玉面幽魂：[33,583] → 随机1项对手技能PP归零
constexpr int k444 = 13674;        // 贪婪射线：[444] → 对手全PP-1 自身+1
constexpr int k425 = 17141;        // 死亡轰炸：[425] args[2,-1] → 随机吸2项属性1级
constexpr int k570 = 15648;        // 闪光次元击：[570] args[1] → 免疫下1次攻击（必中）
constexpr int k980 = 19729;        // 雷裂残阳：[980,842,101] → 先手封对手属性技
constexpr int k945 = 19715;        // 羽之迷幻：[945] → 消回合成功回满血
constexpr int k485 = 14227;        // 断空光牙剑：[485] → 消强成功回满血
constexpr int k913 = 19663;        // 翼色魔行：[913] args[50] → 消回合成功下回合额外增伤50%
constexpr int k905 = 19474;        // 朽梦挽歌：[905,741,776] → 消强成功3回合封对手回血
constexpr int k1224 = 31102;       // 破壳而出：[1224] args[2] → 消回合成功下2回合必暴
constexpr int k1199 = 30891;       // 聚能破穹光：[1199,101,553] → 吸取成功2回合封回血
constexpr int k81 = 20463;         // 捆绑缠绕：[81] args[3] → 下3回合攻击必中（属性·必中）
constexpr int k582 = 17081;        // 天锁归魂斩：[582] args[2] → 未击败则下2回合必暴
constexpr int k1048 = 30284;       // 密宗惊梦迟：[945,1048] → 对手异常本次攻击额外增伤50%
constexpr int k1356 = 32233;       // X·深紫彗星：[1124,1356,1128] → 暴击附加最大体力1/3粉伤
constexpr int k57Opp = 11007;      // 金光绿叶：[57] args[3,10] → 3回合窗口（造"有回合类效果"的对手）

using scenario::ScenarioBattle;

ScenarioBattle make_battle(const ElfPet& p0, const ElfPet& p1) {
    sbs_srand(20260916);
    return ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

void round_of(ScenarioBattle& b, int idx0, int idx1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, idx1);
    b.drive();
}

// 对手"空操作"回合（不出手、不耗 rand——配对实验保持序列一致）
void round_with_opp_none(ScenarioBattle& b, int idx0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

constexpr int kSlow = 50;
constexpr int kFast = 300;

ElfPet mine(int skill, int speed = 200, int atk = 200) {
    return scenario::make_pet(0, "我方", {skill, kPaiDa, kGaoSu, kGaoSu, kGaoSu},
                              speed, atk, 4000);
}
ElfPet foe(int skill = kGaoSu, int speed = 100, int atk = 30) {
    return scenario::make_pet(1, "对手", {skill, kPaiDa, kGaoSu, kGaoSu, kGaoSu},
                              speed, atk, 4000);
}

int levels_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== A 威力视图族：视图值精确断言（写入即"变威力"）==========
    {
        // 37：HP<1/2 → 威力×2（55→110）；满血不动。
        ScenarioBattle a = make_battle(mine(k37), foe());
        a.start();
        round_with_opp_none(a, 0);
        CHECK_EQ(static_cast<int>(a.ctx().ws.skill_power_view[0]), 55, "A) 满血无倍率");
        ScenarioBattle b = make_battle(mine(k37), foe());
        b.start();
        b.pet(0).hp = 1500;   // < 4000/2
        round_with_opp_none(b, 0);
        CHECK_EQ(static_cast<int>(b.ctx().ws.skill_power_view[0]), 110, "A) 37 HP<1/2 → ×2");
        CHECK(b.pet(1).hp < a.pet(1).hp, "A) 伤害随之提高");
        std::cout << "A1 PASS 37 HP阈值威力×2\n";

        // 645：HP<1/3 → ×3（60→180）
        ScenarioBattle c = make_battle(mine(k645), foe());
        c.start();
        c.pet(0).hp = 1000;   // < 4000/3
        round_with_opp_none(c, 0);
        CHECK_EQ(static_cast<int>(c.ctx().ws.skill_power_view[0]), 180, "A) 645 HP<1/3 → ×3");
        std::cout << "A2 PASS 645 HP阈值威力×3\n";

        // 925：满体力 +50%（85→127）；非满体力不动
        ScenarioBattle d = make_battle(mine(k925), foe());
        d.start();
        round_with_opp_none(d, 0);
        CHECK_EQ(static_cast<int>(d.ctx().ws.skill_power_view[0]), 127, "A) 925 满血 +50%");
        ScenarioBattle e = make_battle(mine(k925), foe());
        e.start();
        e.pet(0).hp = 3999;
        round_with_opp_none(e, 0);
        CHECK_EQ(static_cast<int>(e.ctx().ws.skill_power_view[0]), 85, "A) 925 非满血不加成");
        std::cout << "A3 PASS 925 满体力威力提升\n";
    }

    //========== B 回复/条件固伤族 ==========
    int drop_c = 0;   // 烧伤 tick 单独量（H3 复用）
    {
        // 466：恢复50点体力（heal_amount 精确值）
        ScenarioBattle a = make_battle(mine(k466), foe());
        a.start();
        a.pet(0).hp = 1000;
        round_with_opp_none(a, 0);
        CHECK_EQ(a.pet(0).hp, 1050, "B) 466 恰好回 50");
        std::cout << "B1 PASS 466 定点回血\n";

        // 467：对手处于烧伤 → 附加 200 固伤。三场差分：
        //   drop_B(有烧伤+炎火击) - drop_A(炎火击) - drop_C(只有烧伤的回合扣血) == 200
        //   A/B 脚本全同 + 施加异常不耗 rand → 红伤相同；C 单独量烧伤 tick。
        int drop_a = 0, drop_b = 0;
        {
            ScenarioBattle x = make_battle(mine(k467), foe());
            x.start();
            const int hp0 = x.pet(1).hp;
            round_with_opp_none(x, 0);
            drop_a = hp0 - x.pet(1).hp;
        }
        {
            ScenarioBattle x = make_battle(mine(k467), foe());
            x.start();
            (void)apply_anomaly(&x.ctx(), 1, /*burn=*/2, 3, 0);
            const int hp0 = x.pet(1).hp;
            round_with_opp_none(x, 0);
            drop_b = hp0 - x.pet(1).hp;
        }
        {
            ScenarioBattle x = make_battle(mine(kGaoSu), foe());
            x.start();
            (void)apply_anomaly(&x.ctx(), 1, /*burn=*/2, 3, 0);
            const int hp0 = x.pet(1).hp;
            round_with_opp_none(x, 0);
            drop_c = hp0 - x.pet(1).hp;   // 烧伤回合扣血（无 467 固伤）
        }
        CHECK_EQ(drop_b - drop_a - drop_c, 200, "B) 467 烧伤条件下附加 200 固伤");
        std::cout << "B2 PASS 467 条件固伤（差分=" << (drop_b - drop_a - drop_c) << "）\n";

        // 1084：无异常也附加 250；异常时翻倍 → 增量 250（250→500），差分同上
        int d4_a = 0, d4_b = 0;
        {
            ScenarioBattle x = make_battle(mine(k1084), foe());
            x.start();
            const int hp0 = x.pet(1).hp;
            round_with_opp_none(x, 0);
            d4_a = hp0 - x.pet(1).hp;
        }
        {
            ScenarioBattle x = make_battle(mine(k1084), foe());
            x.start();
            (void)apply_anomaly(&x.ctx(), 1, /*burn=*/2, 3, 0);
            const int hp0 = x.pet(1).hp;
            round_with_opp_none(x, 0);
            d4_b = hp0 - x.pet(1).hp;
        }
        // 翻倍增量 = 基础粉伤（250→500 差 250），一条断言同时证明"有附加"与"异常翻倍"。
        // A/B 同种子同序列 → 红伤相同；B 场掉血里还有烧伤 tick（drop_c），一并减去。
        CHECK_EQ(d4_b - d4_a - drop_c, 250, "B) 1084 异常翻倍增量（250→500）");
        std::cout << "B3 PASS 1084 异常翻倍固伤\n";
    }

    //========== C 减伤窗口族 ==========
    {
        // 463：2回合内受击-150。对手攻击力压到红伤≈250 量级，浮动的 ±8% 盖不过 150。
        ElfPet me = mine(k463, kFast);
        ElfPet hitter = foe(kGaoSu, kSlow, /*atk=*/700);
        ScenarioBattle a = make_battle(me, hitter);
        a.start();
        round_of(a, 0, 1);            // 我开窗（对手同回合拍打）
        const int d1 = 4000 - a.pet(0).hp;
        ScenarioBattle c = make_battle(mine(kGaoSu, kFast), hitter);
        c.start();
        round_of(c, 0, 1);            // 无窗口对照
        const int d3 = 4000 - c.pet(0).hp;
        CHECK(d1 + 100 < d3, "C) 463 开窗受击少 150（d1=" + std::to_string(d1)
              + " d3=" + std::to_string(d3) + "）");
        std::cout << "C1 PASS 463 点数减伤窗口\n";

        // 54：1回合对方攻击伤害减半（乘算槽 50）+ 功能减半断言
        ScenarioBattle x = make_battle(mine(k54, kFast), hitter);
        x.start();
        round_of(x, 0, 1);
        CHECK_EQ(x.ctx().ws.damage_reduce_mul[0][0], 50, "C) 54 乘算减伤槽=50");
        const int d_half = 4000 - x.pet(0).hp;
        ScenarioBattle y = make_battle(mine(kPaiDa, kFast), hitter);
        y.start();
        round_of(y, 0, 1);            // 无 54：全伤
        const int d_full = 4000 - y.pet(0).hp;
        // 同种子同序列 → 红伤完全一致，减半是精确等式
        CHECK_EQ(d_half * 2, d_full, "C) 54 受击精确减半");
        std::cout << "C2 PASS 54 乘算减伤\n";
    }

    //========== D 循环族代表：927 低血回血+等量百分比伤害 ==========
    {
        // 电闪天地是属性技（无攻击伤害）→ opp 掉血 = 粉伤，精确。
        ScenarioBattle a = make_battle(mine(k927, kFast), foe());
        a.start();
        a.pet(0).hp = 1000;           // < max/2 → 低血
        round_with_opp_none(a, 0);
        CHECK_EQ(a.pet(0).hp, 3000, "D) 927 低血回 1/2（1000+2000）");
        CHECK_EQ(4000 - a.pet(1).hp, 2000, "D) 927 低血附加等量百分比伤害 2000");
        ScenarioBattle b = make_battle(mine(k927, kFast), foe());
        b.start();
        b.pet(0).hp = 2100;           // ≥ max/2 → 不附加
        round_with_opp_none(b, 0);
        CHECK_EQ(b.pet(0).hp, 4000, "D) 927 非低血照常回 1/2（2100+2000 钳满）");
        CHECK_EQ(4000 - b.pet(1).hp, 0, "D) 927 非低血不附加粉伤");
        std::cout << "D1 PASS 927 低血条件回血+等量粉伤\n";
    }

    //========== E 窃取族 ==========
    {
        // 583：随机 1 项对手技能 PP 归零（对手 5 技全满 PP → 恰好 1 项为 0）
        ScenarioBattle a = make_battle(mine(k583), foe());
        a.start();
        round_with_opp_none(a, 0);
        int zeroed = 0;
        for (const Skills& sk : a.pet(1).skills) {
            if (sk.pp == 0) {
                ++zeroed;
            }
        }
        CHECK_EQ(zeroed, 1, "E) 583 恰好归零 1 项");
        std::cout << "E1 PASS 583 随机PP归零\n";

        // 444：对手每槽 -1，自身同槽 +1（钳满）
        ScenarioBattle b = make_battle(mine(k444), foe());
        b.start();
        round_with_opp_none(b, 0);
        for (std::size_t i = 0; i < b.pet(1).skills.size(); ++i) {
            const Skills& sk = b.pet(1).skills[i];
            CHECK_EQ(sk.pp, sk.maxPP - 1, "E) 444 对手每槽 PP-1");
        }
        std::cout << "E2 PASS 444 全PP一来一去\n";

        // 425：随机吸 2 项属性 1 级——对手总和 -2、自身总和 +2（选哪项随机，总和确定）
        ScenarioBattle c = make_battle(mine(k425), foe());
        c.start();
        round_with_opp_none(c, 0);
        CHECK_EQ(levels_sum(c.ctx(), 1), -2, "E) 425 对手能力总和 -2");
        CHECK_EQ(levels_sum(c.ctx(), 0), 2, "E) 425 吸到自身总和 +2");
        std::cout << "E3 PASS 425 随机属性吸取\n";

        // 425b：★ 两个独立动作（2026-09-23 口径）——对手挂免弱票挡下全部下降，
        // 自身的附加提升**照常**（同吸血：粉伤被免掉，回复照回，"说吸多少就是多少"）
        ElfPet immune_foe = foe();
        immune_foe.soulMark = scenario::make_stat_drop_immunity_soul_mark();
        ScenarioBattle d = make_battle(mine(k425), immune_foe);
        d.start();
        round_with_opp_none(d, 0);
        CHECK_EQ(levels_sum(d.ctx(), 1), 0, "E) 425b 免弱挡下对手全部下降");
        CHECK_EQ(levels_sum(d.ctx(), 0), 2, "E) 425b ★ 免弱不影响自身的附加提升");
        std::cout << "E3b PASS 425 下降/提升两半独立（免弱只挡对手半）\n";

        // E4：★ PP 降低/清除走专用原语并 emit EVENT_PP_REDUCED（监听面）
        // 444 = pp_reduce(opp,1)：对手 5 槽全变 → 挂在对手侧的监听器应收到 5 次。
        ScenarioBattle e = make_battle(mine(k444), foe());
        e.start();
        int pp_events = 0;
        (void)e.ctx().event_center_.register_watcher(
            EventType::EVENT_PP_REDUCED, /*owner=*/1, e.ctx().roundCount,
            /*window_rounds=*/0, /*once=*/false,
            [&pp_events](BattleContext*, const BattleEvent&) {
                ++pp_events;
            });
        round_with_opp_none(e, 0);
        CHECK(pp_events >= 5, "E) 444 走 pp_reduce 原语（对手 5 槽各发一次 PP 事件，实际 "
              + std::to_string(pp_events) + "）");
        std::cout << "E4 PASS PP 原语发事件（监听面可用）\n";
    }

    //========== F 免疫 / 封属 ==========
    {
        // 570：免疫下 1 次攻击——开窗回合对手攻击被挡（0 掉血），次回合照常
        ScenarioBattle a = make_battle(mine(k570, kFast), foe(kPaiDa, kSlow, 700));
        a.start();
        round_of(a, 0, 1);
        CHECK_EQ(a.pet(0).hp, 4000, "F) 570 开窗回合攻击被免");
        round_of(a, 1, 1);
        CHECK(a.pet(0).hp < 4000, "F) 570 次数用完后照常受伤");
        std::cout << "F1 PASS 570 次数免攻击\n";

        // 980：先手 → 对手当回合属性技能无效；攻击技能不受影响
        ScenarioBattle b = make_battle(mine(k980, kFast), foe(kGaoSu, kSlow));
        b.start();
        round_of(b, 0, 0);
        CHECK(b.ctx().ws.skill_exec_result[1] == SkillExecResult::SKILL_INVALID,
              "F) 980 先手封对手属性技能");
        ScenarioBattle c = make_battle(mine(k980, kFast), foe(kPaiDa, kSlow));
        c.start();
        round_of(c, 0, 1);
        CHECK(c.ctx().ws.skill_exec_result[1] == SkillExecResult::HIT,
              "F) 980 不封攻击技能");
        std::cout << "F2 PASS 980 先手封属性\n";
    }

    //========== G 消除成功族（对手先开"回合类效果"/能力提升）==========
    {
        // 945：消回合成功 → 回满血（对手 R1 用 57 开窗 → R2 我消 → SUCCESS）
        ElfPet me945 = mine(k945, kFast, /*atk=*/50);
        ScenarioBattle a = make_battle(me945, foe(k57Opp, kSlow, 10));
        a.start();
        a.pet(0).hp = 500;
        round_of(a, 0, 0);            // 对手开窗（金光绿叶伤害微弱）
        round_with_opp_none(a, 0);    // 我消回合
        CHECK_EQ(a.pet(0).hp, 4000, "G) 945 消除成功回满");
        ScenarioBattle c = make_battle(mine(k945, kFast, 50), foe(kGaoSu, kSlow, 10));
        c.start();
        c.pet(0).hp = 500;
        // ⚠️ 对照的对手全程空操作：任何技能节点都会被引擎归一化成"1 回合的回合类效果"，
        //    残留未清理时会把 break 误判成 SUCCESS——对照必须给对手留一只"干净桶"。
        round_with_opp_none(c, 0);    // R1：我消（无可消），对手不出手
        round_with_opp_none(c, 0);    // R2：仍无可消 → 不回血
        CHECK(c.pet(0).hp < 4000, "G) 945 无可消不回血");
        std::cout << "G1 PASS 945 消回合成功回满\n";

        // 485：消强成功 → 回满血（对手 R1 高速移动 +2 → R2 我消强）
        ScenarioBattle d = make_battle(mine(k485, kFast, 50), foe(kGaoSu, kSlow, 10));
        d.start();
        d.pet(0).hp = 500;
        round_of(d, 0, 0);
        round_with_opp_none(d, 0);
        CHECK_EQ(d.pet(0).hp, 4000, "G) 485 消强成功回满");
        ScenarioBattle e = make_battle(mine(k485, kFast, 50), foe(kPaiDa, kSlow, 10));
        e.start();
        e.pet(0).hp = 500;
        round_of(e, 0, 1);            // 对手拍打：无提升可消
        round_with_opp_none(e, 0);
        CHECK(e.pet(0).hp < 4000, "G) 485 消强失败不回血");
        std::cout << "G2 PASS 485 消强成功回满\n";

        // 913：消回合成功 → 下回合"额外增伤50%"（AMP_EXTRA 槽，非通用乘法通道）
        ScenarioBattle f = make_battle(mine(k913, kFast, 50), foe(k57Opp, kSlow, 10));
        f.start();
        round_of(f, 0, 0);
        round_with_opp_none(f, 0);    // 消成功（下回合窗口）
        round_with_opp_none(f, 1);    // 下回合：ROUND_START 写增伤槽
        CHECK_EQ(f.ctx().ws.damage_add_extra_mul[0][0], 50, "G) 913 下回合额外增伤 50%");
        std::cout << "G3 PASS 913 消回合成功下回合增伤\n";

        // 905：消强成功 → 3回合封对手回血
        ScenarioBattle g = make_battle(mine(k905, kFast, 50), foe(kGaoSu, kSlow, 10));
        g.start();
        round_of(g, 0, 0);            // 对手 +2 速度
        round_with_opp_none(g, 0);    // 我消强 → 封回血
        g.pet(1).hp = 1000;
        (void)heal_amount(&g.ctx(), 1, 500);
        CHECK_EQ(g.pet(1).hp, 1000, "G) 905 消强成功封对手回血");
        std::cout << "G4 PASS 905 消强成功封回血\n";

        // 1224：消回合成功 → 下2回合必暴（crit_rate=0 也暴）
        ElfPet me1224 = mine(k1224, kFast, 50);
        ScenarioBattle h = make_battle(me1224, foe(k57Opp, kSlow, 10));
        h.start();
        round_of(h, 0, 0);
        round_with_opp_none(h, 0);    // 消成功 → 下2回合必暴
        round_with_opp_none(h, 1);    // 下回合拍打
        CHECK_EQ(h.ctx().crit_happened[0], true, "G) 1224 下回合必定暴击");
        ScenarioBattle i = make_battle(mine(k1224, kFast, 50), foe(kGaoSu, kSlow, 10));
        i.start();
        round_with_opp_none(i, 0);    // 对手全程空操作 → 无回合类效果可消
        round_with_opp_none(i, 0);
        round_with_opp_none(i, 1);
        CHECK_EQ(i.ctx().crit_happened[0], false, "G) 1224 消除失败不暴");
        std::cout << "G5 PASS 1224 消回合成功必暴窗口\n";

        // 1199：吸取对手提升成功 → 能力转移 + 封对手回血。
        // R1 我先手先打（对手还没强化 → 吸取失败，不封回血），随后对手高速移动 +2；
        // R2 对手改用拍打；R3 我先手再打（吸取成功 → 转移 + 封回血）。
        ScenarioBattle j = make_battle(mine(k1199, kFast, 50), foe(kGaoSu, kSlow, 10));
        j.start();
        round_of(j, 0, 0);            // R1：我吸取失败（对手无提升）
        CHECK_EQ(j.ctx().ability_levels[0][4], 0, "G) 1199 无提升可吸不转移");
        round_of(j, 1, 1);            // R2：对手拍打（不再加强化）；我拍打，先手
        round_with_opp_none(j, 0);    // R3：我吸取成功 → 转移 + 封回血
        CHECK_EQ(j.ctx().ability_levels[0][4], 2, "G) 1199 提升转到自身");
        CHECK_EQ(j.ctx().ability_levels[1][4], 0, "G) 1199 对手提升被转移走（归 0）");
        j.pet(1).hp = 1000;
        (void)heal_amount(&j.ctx(), 1, 500);
        CHECK_EQ(j.pet(1).hp, 1000, "G) 1199 吸取成功封对手回血");
        std::cout << "G6 PASS 1199 吸取成功转移+封回血\n";
    }

    //========== H 必中 / 条件必暴 / 增伤 / 暴击附加 ==========
    {
        // 81：下3回合攻击必中——对手 100% 闪避魂印，开窗后拍打必中
        ElfPet dodgy = foe(kGaoSu, kSlow);
        dodgy.soulMark = scenario::make_dodge_soul_mark();
        ElfPet me81 = mine(k81, kFast);
        ScenarioBattle a = make_battle(me81, dodgy);
        a.start();
        round_with_opp_none(a, 0);    // 捆绑缠绕（必中，开窗）
        round_with_opp_none(a, 1);    // 窗口内拍打
        CHECK(a.ctx().ws.skill_exec_result[0] == SkillExecResult::HIT,
              "H) 81 窗口内攻击穿透 100% 闪避");
        ElfPet me81c = mine(kGaoSu, kFast);
        ElfPet dodgy2 = foe(kGaoSu, kSlow);
        dodgy2.soulMark = scenario::make_dodge_soul_mark();
        ScenarioBattle c = make_battle(me81c, dodgy2);
        c.start();
        round_with_opp_none(c, 0);    // 无开窗
        round_with_opp_none(c, 1);
        CHECK(c.ctx().ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID,
              "H) 81 对照：无窗口被闪避");
        std::cout << "H1 PASS 81 下N回合必中\n";

        // 582：未击败对手 → 下1回合必暴（对手存活）；对照组无窗口不暴
        ElfPet me582 = mine(k582, kFast, 50);
        ScenarioBattle d = make_battle(me582, foe(kGaoSu, kSlow, 10));
        d.start();
        round_with_opp_none(d, 0);    // 攻击未击败 → 开必暴窗
        round_with_opp_none(d, 1);    // 拍打（crit_rate=0）
        CHECK_EQ(d.ctx().crit_happened[0], true, "H) 582 未击败 → 下回合必暴");
        ElfPet me582c = mine(kGaoSu, kFast, 50);
        ScenarioBattle e = make_battle(me582c, foe(kGaoSu, kSlow, 10));
        e.start();
        round_with_opp_none(e, 0);
        round_with_opp_none(e, 1);
        CHECK_EQ(e.ctx().crit_happened[0], false, "H) 582 对照不暴");
        std::cout << "H2 PASS 582 条件必暴\n";

        // 1048：对手处于异常 → 本次攻击额外增伤 50%（乘法槽）+ 掉血更多
        ScenarioBattle f = make_battle(mine(k1048, kFast, 50), foe(kGaoSu, kSlow, 10));
        f.start();
        round_with_opp_none(f, 0);
        CHECK_EQ(f.ctx().ws.damage_add_extra_mul[0][0], 0, "H) 1048 无异常不加成");
        const int drop_plain = 4000 - f.pet(1).hp;
        ScenarioBattle g = make_battle(mine(k1048, kFast, 50), foe(kGaoSu, kSlow, 10));
        g.start();
        (void)apply_anomaly(&g.ctx(), 1, /*burn=*/2, 3, 0);
        round_with_opp_none(g, 0);
        CHECK_EQ(g.ctx().ws.damage_add_extra_mul[0][0], 50, "H) 1048 异常时额外增伤槽=50");
        const int drop_amp = 4000 - g.pet(1).hp - drop_c;   // 去掉烧伤 tick
        CHECK(drop_amp > drop_plain, "H) 1048 增伤落地");
        std::cout << "H3 PASS 1048 异常条件增伤\n";

        // 1356：暴击 → 附加对手最大体力 1/3 的百分比伤害（4000/3=1333）
        ElfPet critter = mine(k1356, kFast, /*atk=*/10);
        scenario::set_crit_sixteenths(critter, 16);   // 必暴
        ScenarioBattle h = make_battle(critter, foe(kGaoSu, kSlow, 10));
        h.start();
        round_with_opp_none(h, 0);
        CHECK_EQ(h.ctx().crit_happened[0], true, "H) 1356 前提：暴击成立");
        const int drop_crit = 4000 - h.pet(1).hp;
        CHECK(1300 <= drop_crit && drop_crit <= 1400,
              "H) 1356 暴击附加 1333 百分比伤害（实际 " + std::to_string(drop_crit) + "）");
        ScenarioBattle i = make_battle(mine(k1356, kFast, 10), foe(kGaoSu, kSlow, 10));
        i.start();
        round_with_opp_none(i, 0);
        const int drop_nocrit = 4000 - i.pet(1).hp;
        CHECK(drop_nocrit < 200, "H) 1356 不暴只掉红伤");
        std::cout << "H4 PASS 1356 暴击附加百分比伤害\n";
    }

    std::cout << "\n场景 118 全部通过：技能批 8（37/645/925/466/467/1084/463/54/927/"
                 "583/444/425/570/980/945/485/913/905/1224/1199/81/582/1048/1356）✓"
              << std::endl;
    return 0;
}
