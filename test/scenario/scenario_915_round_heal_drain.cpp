// 场景 915 —— J6 每回合用技恢复/吸取窗族（12 效果，moves_lib/j_round_heal_drain.cpp）
//（2026-10-01，worktree ../agents/j6；号段 915-919 一文件覆盖）
//
// 断言口径：
//   - 本族**无门槛**（纯"每回合使用技能"窗）→ 不预置强化；R1 出招当次即在
//     AFTER_ACTION 结算（InRounds 含当回合，532/597/707 已背书）；
//   - 恢复/吸取量全部精确断言（1/N 与点数无随机；红伤 1~2 用带宽兜）；
//   - 条件变体的两路各一 run（低体/满体/对手血量比较都是预置 hp 直写）；
//   - 场下治疗（1602/1785）用 bench 槽 hp 直写预置 + 终值断言（阵亡不复活口径
//     不在本文件覆盖——阵亡槽 hp<=0 跳过是实现内显式分支）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：filler

// J6 载体（切片双源核对见实现文件头）
constexpr int kJianDao = 24660;  // 剑道莫寻   [585×6,48,791]|[…,4,3]        791=[4,3]
constexpr int kMingTu = 28201;   // 冥途誓约   [1875]|[4,3,2]（单载体）
constexpr int kXianYing = 36568; // 炫影仙尘踏 [1924,713,1662]|[4,300,50]    1924=[4,300]
constexpr int kMengYue = 27613;  // 朦月龙吟   [191,1695,440]|[4,…]          1695=[4,3,4]
constexpr int kShuLing = 27364;  // 树灵滋养   [911,1602,1026]|[1,…]         1602=[4,3,100]
constexpr int kMingZhou = 36534; // 冥咒妒命杀 [1842,…]|[3,280,350,…]        1842=[3,280,350]
constexpr int kLiangZhi = 37542; // 良址招租   [1983]|[6,233]（单载体）
constexpr int kHongJian = 28032; // 鸿渐于陆   [191,1836,…]|[4,…]            1836=[4,3,2]
constexpr int kHanShuang = 28478;// 寒霜之境   [993,2021,843]|[…]            2021=[4,300,3,300]
constexpr int kHuoLuan = 28039;  // 祸乱之躯   [1344,1839,843]|[1,…]         1839=[4,3,4]
constexpr int kYueTu = 38096;    // 月兔祝福   [2007,746]|[2,3,250,…]        2007=[2,3,250]
constexpr int kChenYun = 28866;  // 沉云浸野   [911,1687,1785,2226]|[1,…]    1785=[4,3,120]

constexpr int kMaxHp = 99999;

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk = 1, int speed = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, kMaxHp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk = 1, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, kMaxHp);
    always_hit(p);
    return p;
}

void play_self(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════ T1  791 剑道莫寻：回1/3，自身 HP<对手 → 翻倍 ════
    {
        int gain_low = 0;
        {
            ScenarioBattle b(make_party(self_pet({kJianDao, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 50000;   // < 对手 99999 → 翻倍
            const int h0 = ctx.getPet(0).hp;
            play_self(b, 0);
            gain_low = ctx.getPet(0).hp - h0;   // min(66666, 49999) = 49999
        }
        int gain_normal = 0;
        {
            ScenarioBattle b(make_party(self_pet({kJianDao, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 50000;
            ctx.getPet(1).hp = 100;     // 对手更低 → 不翻倍
            const int h0 = ctx.getPet(0).hp;
            play_self(b, 0);
            gain_normal = ctx.getPet(0).hp - h0;   // 33333
        }
        CHECK_EQ(gain_low, 49999, "T1 低体（相对对手）翻倍 2/3 → 钳满 49999");
        CHECK_EQ(gain_normal, 33333, "T1 对照（自身血更高）→ 1/3");
        std::cout << "  [T1] 791: 翻倍=" << gain_low << " 常规=" << gain_normal << "\n";
    }

    // ════ T2  1875 冥途誓约：回1/3+等量粉伤；自身 HP<1/2 → 翻倍 ════
    {
        int gain = 0;
        int opp_delta = 0;
        {
            ScenarioBattle b(make_party(self_pet({kMingTu, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 30000;   // < 99999/2 → 翻倍
            const int h0 = ctx.getPet(0).hp;
            const int o0 = ctx.getPet(1).hp;
            play_self(b, 0);
            gain = ctx.getPet(0).hp - h0;         // 33333×2 钳 69999
            opp_delta = o0 - ctx.getPet(1).hp;    // 粉伤 69999 + 红伤 1~2
        }
        CHECK_EQ(gain, 66666, "T2 低体翻倍 → 回 66666（2×33333，30000+66666 不触顶）");
        CHECK(opp_delta >= 66666 && opp_delta <= 66668, "T2 等量粉伤 66666（载体属性技无红伤）");
        std::cout << "  [T2] 1875: 回=" << gain << " 粉伤=" << opp_delta << "\n";
    }

    // ════ T3  1924 炫影仙尘踏：回 300 点 + 等量固定伤害 ════
    {
        int gain = 0;
        int opp_delta = 0;
        {
            ScenarioBattle b(make_party(self_pet({kXianYing, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp - 1000;
            const int h0 = ctx.getPet(0).hp;
            const int o0 = ctx.getPet(1).hp;
            play_self(b, 0);
            gain = ctx.getPet(0).hp - h0;
            opp_delta = o0 - ctx.getPet(1).hp;
        }
        CHECK_EQ(gain, 300, "T3 回 300 点");
        CHECK(opp_delta >= 301 && opp_delta <= 310, "T3 等量固定伤 300（+红伤，atk=1/p=160 有浮动）");
        std::cout << "  [T3] 1924: 回=" << gain << " 固定伤=" << opp_delta << "\n";
    }

    // ════ T4  1695 朦月龙吟：回1/4+等量粉伤；满体力 → 转变 1/4 粉伤 ════
    {
        int gain_a = 0;
        int opp_a = 0;
        {
            ScenarioBattle b(make_party(self_pet({kMengYue, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp - 1000;   // 非满 → 回血路
            const int h0 = ctx.getPet(0).hp;
            const int o0 = ctx.getPet(1).hp;
            play_self(b, 0);
            gain_a = ctx.getPet(0).hp - h0;     // 钳 1000
            opp_a = o0 - ctx.getPet(1).hp;      // 粉 1000 + 红 1~2
        }
        int opp_b = 0;
        {
            ScenarioBattle b(make_party(self_pet({kMengYue, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int o0 = ctx.getPet(1).hp;    // 满体力 → 转变 1/4 粉伤
            play_self(b, 0);
            opp_b = o0 - ctx.getPet(1).hp;      // 粉 24999 + 红 1~2
        }
        CHECK_EQ(gain_a, 1000, "T4 非满 → 回血钳 1000");
        CHECK(opp_a >= 1000 && opp_a <= 1002, "T4 等量粉伤 1000（载体属性技无红伤）");
        CHECK(opp_b >= 24999 && opp_b <= 25001, "T4 满体 → 转变 1/4 粉伤 24999");
        std::cout << "  [T4] 1695: 回=" << gain_a << " 粉a=" << opp_a << " 粉b=" << opp_b << "\n";
    }

    // ════ T5  1602 树灵滋养：回1/3；满体力 → 场下全体 +100 ════
    {
        int gain_a = 0;
        int bench_a = 0;
        {
            ScenarioBattle b(make_party(self_pet({kShuLing, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp - 500;       // 非满 → 自身回血
            ctx.seerRobot[0].elfPets[1].hp = 98999;  // bench 预置 -1000
            const int h0 = ctx.getPet(0).hp;
            play_self(b, 0);
            gain_a = ctx.getPet(0).hp - h0;        // 钳 500
            bench_a = ctx.seerRobot[0].elfPets[1].hp;   // 不动
        }
        int bench_b = 0;
        {
            ScenarioBattle b(make_party(self_pet({kShuLing, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seerRobot[0].elfPets[1].hp = 98999;    // 满体力 → 场下 +100
            play_self(b, 0);
            bench_b = ctx.seerRobot[0].elfPets[1].hp;  // 98999+100
        }
        CHECK_EQ(gain_a, 500, "T5 非满 → 自身回血钳 500");
        CHECK_EQ(bench_a, 98999, "T5 非满 → 场下不动");
        CHECK_EQ(bench_b, 99099, "T5 满体 → 场下 +100");
        std::cout << "  [T5] 1602: 回=" << gain_a << " bench=" << bench_a << "/" << bench_b << "\n";
    }

    // ════ T6  1842 冥咒妒命杀：吸 280 点；自身 HP<对手 → 提升至 350 ════
    {
        int gain_a = 0;
        {
            ScenarioBattle b(make_party(self_pet({kMingZhou, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 40000;   // < 对手 99999 → 350
            const int h0 = ctx.getPet(0).hp;
            play_self(b, 0);
            gain_a = ctx.getPet(0).hp - h0;
        }
        int gain_b = 0;
        {
            ScenarioBattle b(make_party(self_pet({kMingZhou, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 50000;
            ctx.getPet(1).hp = 30000;   // 自身更高 → 280
            const int h0 = ctx.getPet(0).hp;
            play_self(b, 0);
            gain_b = ctx.getPet(0).hp - h0;
        }
        CHECK_EQ(gain_a, 350, "T6 低体 → 提升 350");
        CHECK_EQ(gain_b, 280, "T6 对照 → 280");
        std::cout << "  [T6] 1842: 提升=" << gain_a << " 常规=" << gain_b << "\n";
    }

    // ════ T7  1983 良址招租：吸 233 点 ════
    {
        int gain = 0;
        int opp_delta = 0;
        {
            ScenarioBattle b(make_party(self_pet({kLiangZhi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp - 300;
            const int h0 = ctx.getPet(0).hp;
            const int o0 = ctx.getPet(1).hp;
            play_self(b, 0);
            gain = ctx.getPet(0).hp - h0;
            opp_delta = o0 - ctx.getPet(1).hp;
        }
        CHECK_EQ(gain, 233, "T7 吸 233 点");
        CHECK(opp_delta >= 233 && opp_delta <= 235, "T7 对手掉 233（+红伤 1~2）");
        std::cout << "  [T7] 1983: 吸=" << gain << "\n";
    }

    // ════ T8  1836 鸿渐于陆：吸1/3；自身 HP<1/2 → 转变翻倍粉伤 ════
    {
        int gain_a = 0;
        {
            ScenarioBattle b(make_party(self_pet({kHongJian, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 50000;   // > 1/2 → 正常吸取
            const int h0 = ctx.getPet(0).hp;
            play_self(b, 0);
            gain_a = ctx.getPet(0).hp - h0;   // 33333
        }
        int gain_b = 0;
        int opp_b = 0;
        {
            ScenarioBattle b(make_party(self_pet({kHongJian, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 30000;   // < 1/2 → 转变粉伤 66666
            const int h0 = ctx.getPet(0).hp;
            const int o0 = ctx.getPet(1).hp;
            play_self(b, 0);
            gain_b = ctx.getPet(0).hp - h0;
            opp_b = o0 - ctx.getPet(1).hp;
        }
        CHECK_EQ(gain_a, 33333, "T8 正常吸取 1/3");
        CHECK_EQ(gain_b, 0, "T8 低体 → 不回血");
        CHECK(opp_b >= 66666 && opp_b <= 66668, "T8 转变翻倍粉伤 66666（载体属性技无红伤）");
        std::cout << "  [T8] 1836: 吸=" << gain_a << " 转变粉=" << opp_b << "\n";
    }

    // ════ T9  2021 寒霜之境：回 300；自身 HP<1/3 → 转变吸 300 ════
    {
        int gain_a = 0;
        int opp_a = 0;
        {
            ScenarioBattle b(make_party(self_pet({kHanShuang, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 30000;   // < 99999/3 → 转变吸取
            const int h0 = ctx.getPet(0).hp;
            const int o0 = ctx.getPet(1).hp;
            play_self(b, 0);
            gain_a = ctx.getPet(0).hp - h0;   // 0（不恢复）
            opp_a = o0 - ctx.getPet(1).hp;    // 吸 300 + 红 1~2
        }
        int gain_b = 0;
        {
            ScenarioBattle b(make_party(self_pet({kHanShuang, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp - 1000;   // 非低 → 回 300
            const int h0 = ctx.getPet(0).hp;
            play_self(b, 0);
            gain_b = ctx.getPet(0).hp - h0;
        }
        CHECK_EQ(gain_a, 300, "T9 低体 → 转变吸取（吸血自愈 300，非 heal 路径）");
        CHECK(opp_a >= 300 && opp_a <= 302, "T9 转变吸取 300（+红伤）");
        CHECK_EQ(gain_b, 300, "T9 对照 → 回 300");
        std::cout << "  [T9] 2021: 转变吸=" << opp_a << " 常规回=" << gain_b << "\n";
    }

    // ════ T10  1839 祸乱之躯：回1/3（用技窗）＋ 回合结束吸1/4（未击败） ════
    {
        int gain = 0;
        int opp_delta = 0;
        {
            ScenarioBattle b(make_party(self_pet({kHuoLuan, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 19999;   // 33333 + 24999 不触顶
            const int h0 = ctx.getPet(0).hp;
            const int o0 = ctx.getPet(1).hp;
            play_self(b, 0);
            gain = ctx.getPet(0).hp - h0;       // 33333 + 24999 = 58332
            opp_delta = o0 - ctx.getPet(1).hp;  // 吸 24999 + 红 1~2
        }
        CHECK_EQ(gain, 58332, "T10 回 33333 + 回末吸取回 24999");
        CHECK(opp_delta >= 24999 && opp_delta <= 25001, "T10 对手掉 24999（+红伤）");
        std::cout << "  [T10] 1839: 总回=" << gain << " 对手掉=" << opp_delta << "\n";
    }

    // ════ T11  2007 月兔祝福：回1/3（用技窗）＋ 回合结束对手有强化 → 吸250+等量护盾 ════
    {
        int gain_ok = 0;
        int shield_ok = 0;
        {
            ScenarioBattle b(make_party(self_pet({kYueTu, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.ability_levels[1][0] = 2;           // 对手有强化 → 触发
            ctx.getPet(0).hp = kMaxHp - 40000;      // 33333 + 250 不触顶
            const int h0 = ctx.getPet(0).hp;
            play_self(b, 0);
            gain_ok = ctx.getPet(0).hp - h0;        // 33333 + 250
            shield_ok = ctx.getPet(1).shield_bank_.quantity_of(2007);
        }
        int gain_ctl = 0;
        int shield_ctl = -1;
        {
            ScenarioBattle b(make_party(self_pet({kYueTu, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp - 40000;      // 无强化 → 只回血
            const int h0 = ctx.getPet(0).hp;
            play_self(b, 0);
            gain_ctl = ctx.getPet(0).hp - h0;
            shield_ctl = ctx.getPet(1).shield_bank_.quantity_of(2007);
        }
        CHECK_EQ(gain_ok, 33583, "T11 回 33333 + 吸 250");
        CHECK_EQ(shield_ok, 250, "T11 对手获等量护盾 250");
        CHECK_EQ(gain_ctl, 33333, "T11 对照 → 只回血");
        CHECK_EQ(shield_ctl, 0, "T11 对照 → 无护盾");
        std::cout << "  [T11] 2007: 回=" << gain_ok << "/" << gain_ctl
                  << " 护盾=" << shield_ok << "/" << shield_ctl << "\n";
    }

    // ════ T12  1785 沉云浸野：吸1/3；满体力 → 场下全体 +120 ════
    {
        int bench_a = 0;
        {
            ScenarioBattle b(make_party(self_pet({kChenYun, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seerRobot[0].elfPets[1].hp = 98999;   // 满体力 → 场下 +120
            play_self(b, 0);
            bench_a = ctx.seerRobot[0].elfPets[1].hp;
        }
        int gain_b = 0;
        int bench_b = 0;
        {
            ScenarioBattle b(make_party(self_pet({kChenYun, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp - 40000;   // 非满 → 正常吸取
            ctx.seerRobot[0].elfPets[1].hp = 98999;
            const int h0 = ctx.getPet(0).hp;
            play_self(b, 0);
            gain_b = ctx.getPet(0).hp - h0;      // 33333
            bench_b = ctx.seerRobot[0].elfPets[1].hp;
        }
        CHECK_EQ(bench_a, 99119, "T12 满体 → 场下 +120");
        CHECK_EQ(gain_b, 33333, "T12 非满 → 吸取 1/3");
        CHECK_EQ(bench_b, 98999, "T12 非满 → 场下不动");
        std::cout << "  [T12] 1785: bench=" << bench_a << "/" << bench_b
                  << " 吸=" << gain_b << "\n";
    }

    std::cout << "scenario_915 round_heal_drain all passed" << std::endl;
    return 0;
}
