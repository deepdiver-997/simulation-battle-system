#include <utils/sbs_rand.h>
// 场景 760：G6 PP 受击族 —— 780（moves_lib/g_pp_onhit.cpp，看板 G6，2026-09-29）
//
// 载体（全库切片自查 + 4399 交叉验证，见 g_pp_onhit.cpp 文件头）：
//   TA：扣人心弦 26491=[191(4),780(3,100,2),776(2)]（属性；颜洛夕 4399："3回合内若受到
//       攻击则有100%概率使对手携带的2个技能PP归零；使自身下2回合的攻击威力翻倍"——
//       链 191 异常反弹（无异常→no-op）/776 攻击威力窗（不断言）皆不碰红伤接收面）
//   TA2：不休禁锢 24855=[813,191(4),780(3,100,1)]（属性；缚影炼狱魔神 4399："将自身的
//       能力下降状态双倍反馈给对手；4回合内免疫并反弹所受到的异常状态；3回合内受到
//       攻击则100%令对手随机1个技能pp值归零"——813 无下降态→no-op）
//   TB：不灭天尊 24119=[57(4,3),780(3,50,1),60(5,120)]（属性；战神·盖亚 4399：
//       "3回合内若受到攻击则有50%概率使对手携带的1个技能PP归零"——50% 概率门切片）
//   TC 对照：对手用蓄气（属性技、无伤害阶段不掉红血）→ 不触发；换拍打（红伤）→ 触发
//
// 断言组：
//   TA 780 count=2（100% 切片）：每挨一次红伤归零 2 个不重复槽（R1→2、R2→4、R3→5 全归零）
//   TA2 780 count=1（100% 切片）：R1→1→2→3，R4 窗过期不增
//   TB 780 50% 门：20 局独立对局（每局恰 1 次受击 1 次掷骰），成功局数带 [3,17]（期望 10；
//       门失效为全 0 / 全 20 都会被抓）；单局归零数 ∈ {0,1} 防"一次受击归零多项"
//   TC 780 受击门：挂窗后对手属性技（无红伤）→ 0；同窗内对手改用攻击技（红伤）→ 归零出现
//
// 确定性：固定种子（TA/TA2 种子扫描选定：随机抽槽不重复抽中已归零槽会扰动精确计数，
// 与 716 TB 同口径）；载体/拍打 crit_rate=0（harness 默认清零）、must_hit=1、maxPP=100；
// PP 计数不受伤害浮动影响（只看归零槽位数，不看血量）。
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kKouXin = 26491;   // 扣人心弦：属性、780 切片 [3,100,2]（链 191/776 惰性）
constexpr int kBuXiu = 24855;    // 不休禁锢：属性、780 切片 [3,100,1]（链 813/191 惰性）
constexpr int kBuMie = 24119;    // 不灭天尊：属性、780 切片 [3,50,1]（链 57/60 惰性）
constexpr int kXuQi = 20039;     // 蓄气：属性、[32]（filler：无红伤）
constexpr int kPaDa = 10004;     // 拍打：物攻 40（filler 攻击）

// 惯例 Build：清杂音（必中/满PP），crit 默认 0 由 harness 保证。
ElfPet make_carrier(int id, const char* name, const std::array<int, 5>& skills, int speed,
                    int atk, int hp) {
    ElfPet p = scenario::make_pet(id, name, skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
        s.maxPP = 100;
        s.pp = 100;
    }
    return p;
}

void drive_round(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, slot1 >= 0 ? BattleFsm::ActionType::SELECT_SKILL
                                     : BattleFsm::ActionType::NONE,
                       slot1 >= 0 ? slot1 : 0);
    b.drive();
}

int zero_pp_slots(BattleContext& ctx, int side) {
    int n = 0;
    for (const Skills& s : ctx.getPet(side).skills) {
        if (s.pp == 0) {
            ++n;
        }
    }
    return n;
}

// 对手"最小 pp>0 槽"自适应选槽：防随机归零槽打断出招（700/TB 同款）。
int first_usable_slot(BattleContext& ctx, int side) {
    const auto& skills = ctx.getPet(side).skills;
    for (int i = 0; i < static_cast<int>(skills.size()); ++i) {
        if (skills[static_cast<std::size_t>(i)].pp > 0) {
            return i;
        }
    }
    return 0;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== TA 780 count=2：100% 切片，每击归零 2 个**不重复**槽 ==========
    // 每局独立、只触发一次（单次触发内部分 Fisher-Yates 天然不重复抽槽 → 恰 2 个，
    // 断言与种子无关）；跨触发累积/过期口径由 TA2（count=1）覆盖。
    for (int t = 0; t < 3; ++t) {
        sbs_srand(20260950 + t);
        ScenarioBattle b(
            scenario::make_party(
                make_carrier(1, "颜洛夕", {kKouXin, kXuQi, kPaDa, kPaDa, kPaDa},
                             /*speed=*/200, /*atk=*/10, /*hp=*/9999)),
            scenario::make_party(
                make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                             /*speed=*/100, /*atk=*/10, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // R1：载体挂窗（先手）+ 对手拍打 → 恰 1 次受击掷骰
        CHECK_EQ(zero_pp_slots(ctx, 1), 2,
                 "TA 核心：第 " << t << " 局单次受击（100% 门）应恰归零 2 个不重复槽");
    }
    std::cout << "  [TA] 780 count=2 单击恰归零 2 不重复槽（×3 局）通过\n";

    //========== TA2 780 count=1：100% 切片，R1→1→2→3，R4 过期不增 ==========
    {
        sbs_srand(100);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "魔神", {kBuXiu, kXuQi, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/10, /*hp=*/9999)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 1; r <= 4; ++r) {
            drive_round(b, r == 1 ? 0 : 1, first_usable_slot(ctx, 1));
            const int expect = (r <= 3) ? r : 3;   // 1→2→3；R4 窗过期不增
            CHECK_EQ(zero_pp_slots(ctx, 1), expect,
                     "TA2 核心：第 " << r << " 回合对手 PP 归零槽位应为 " << expect
                                     << "（每击恰 +1，随机不重复槽；R4 过期）");
        }
        std::cout << "  [TA2] 780 count=1（1→2→3→不增）通过\n";
    }

    //========== TB 780 50% 门：20 局独立对局统计带 [3,17]（期望 10） ==========
    {
        int successes = 0;
        for (int t = 0; t < 20; ++t) {
            sbs_srand(990000 + t);
            ScenarioBattle b(
                scenario::make_party(
                    make_carrier(1, "盖亚", {kBuMie, kXuQi, kPaDa, kPaDa, kPaDa},
                                 /*speed=*/200, /*atk=*/10, /*hp=*/9999)),
                scenario::make_party(
                    make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                 /*speed=*/100, /*atk=*/10, /*hp=*/9999)));
            b.start();
            BattleContext& ctx = b.ctx();
            drive_round(b, 0, 0);   // 载体挂窗（先手）+ 对手拍打：本局恰 1 次受击掷骰
            const int zeroed = zero_pp_slots(ctx, 1);
            CHECK(zeroed == 0 || zeroed == 1,
                  "TB 前提：第 " << t << " 局 50% 门下单次受击归零数应 ∈ {0,1}（实际 "
                                 << zeroed << "）");
            successes += zeroed;
        }
        CHECK(successes >= 3 && successes <= 17,
              "TB 核心：20 局 50% 门成功局数应落带 [3,17]（实际 " << successes
                                                                  << "；期望 10，"
                                                                     "全 0=门失效、全 20=门缺失）");
        std::cout << "  [TB] 780 50% 概率门统计（" << successes << "/20）通过\n";
    }

    //========== TC 780 受击门：无红伤不触发（对照），红伤即触发 ==========
    {
        sbs_srand(20260952);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "颜洛夕", {kKouXin, kXuQi, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/10, /*hp=*/9999)),
            scenario::make_party(make_carrier(2, "对手", {kXuQi, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：载体挂窗；对手用蓄气（属性技、无伤害阶段）→ 我不掉红血 → 不触发。
        drive_round(b, 0, 0);
        CHECK_EQ(zero_pp_slots(ctx, 1), 0,
                 "TC 核心：未受红伤（对手属性技）→ 780 不触发");
        // R2：对手换拍打（红伤）→ 同一窗口内触发、归零 2 槽。
        drive_round(b, 1, 1);
        CHECK_EQ(zero_pp_slots(ctx, 1), 2,
                 "TC 核心：对手改用攻击技（红伤）→ 同窗内触发、每击 +2");
        std::cout << "  [TC] 780 受击门（属性技不触发 / 红伤触发）通过\n";
    }

    std::cout << "scenario_760_pp_onhit: all blocks passed" << std::endl;
    return 0;
}
