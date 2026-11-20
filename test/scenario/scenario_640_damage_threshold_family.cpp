// 场景 640：F4 高低伤判定族 —— 1106（伤害高于阈值→概率异常）/ 1066（伤害低于阈值→
// 附加固伤，先手加成）/ 594（伤害低于阈值→概率异常）（moves_lib/f_damage_threshold.cpp，
// 2026-09-29，worktree ../agents/f4）
//
// 覆盖（载体切片见 DB；三效果全部 AFTER_ACTION 桥读 resolvedDamage.final，E3 475 同款）：
//   T1 1106 耀世飞虎闪 30423（[300,100,0] 麻痹）：伤害低于/高于 300 → 不给/给麻痹
//   T2 1066 碎岩无影袭 31513（[300,300,150]）：先手触发 = 300+150 固伤；后手 = 仅 300；
//      伤害达标 → 不附加
//   T3 594  魔帝千裂破 16169（[300,100,6] 害怕）：伤害低于/高于 300 → 给/不给害怕
//   T4 1106 第二载体 诡魂绝煞破 30662（[280,100,6] 害怕）：同结构交叉验证
//
// 确定性口径：
//   - 概率切片全部用 chance=100（30423/30662/16169 的 1106/594 切片恰为 100%）→ 无掷点
//     随机（apply_anomaly_checked 100% 必过）；16626 的 594 切片是 60% 不做行为断言。
//   - 暴击率 harness 清零；must_hit=1 防 miss；元素 {0,0} 克制 1x。
//   - 伤害量级：威力 150/160、防御 100 → 伤害 ≈ 1.26~1.34×atk+2，atk 拉开与阈值 300/280
//     的距离（低档 atk=1/10 → 个位数伤害；高档 atk=10000 → 上万），85%~100% 浮动不跨档。
//   - 1066 的固伤断言用 hp 差区间：红伤浮动带 + [300,300+150] 固伤两档区间不相交。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10001;        // 撞击：无效果 priority 0（对照 filler）
constexpr int kFeihu = 30423;      // 耀世飞虎闪：特攻 150，[1106,1029]→1106 切片 [300,100,0]
constexpr int kJuesha = 30662;     // 诡魂绝煞破：物理 150，[986,1106]→1106 切片 [280,100,6]
constexpr int kSuiyan = 31513;     // 碎岩无影袭：物理 160，[1224,1066]→1066 切片 [300,300,150]
constexpr int kQianlie = 16169;    // 魔帝千裂破：特攻 160，[594,591]→594 切片 [300,100,6]

constexpr int kParaId = static_cast<int>(AbnormalStatusId::Paralysis);   // 0
constexpr int kFearId = static_cast<int>(AbnormalStatusId::Fear);        // 6

ElfPet self_pet(std::array<int, 5> skills, int speed, int atk) {
    ElfPet p = make_pet(0, "己方", skills, speed, atk, /*hp=*/5000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 对手恒用撞击 filler、atk=1（打己方个位数血，不干扰断言——断言全在对手 hp/异常上）。
ElfPet foe_pet(int speed = 100) {
    ElfPet p = make_pet(1, "对手", {kHit, kHit, kHit, kHit, kHit}, speed, /*atk=*/1,
                        /*hp=*/1000000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

int active_anomaly_count(const BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) {
            ++n;
        }
    }
    return n;
}

void play_round(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 1106（耀世飞虎闪 [300,100,0] 麻痹）==========
    {   // T1a 伤害低于阈值 → 不给异常
        ScenarioBattle b(make_party(self_pet({kFeihu, kHit, kHit, kHit, kHit},
                                              /*speed=*/200, /*atk=*/10)),
                         make_party(foe_pet()));
        b.start();
        play_round(b, 0, 0);   // 伤害 ≈14（<300）
        CHECK(!b.ctx().has_active_abnormal_status(1, kParaId),
              "T1a：伤害低于 300 → 不令对手麻痹");
        CHECK_EQ(active_anomaly_count(b.ctx(), 1), 0, "T1a：对手无任何异常");
        std::cout << "  [T1a] 1106 低伤不触发 通过\n";
    }
    {   // T1b 伤害高于阈值 → 麻痹落在对手身上（不是自己）
        const int foe_hp0 = foe_pet().hp;
        ScenarioBattle b(make_party(self_pet({kFeihu, kHit, kHit, kHit, kHit},
                                              /*speed=*/200, /*atk=*/10000)),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // 伤害 ≈12600（>300）
        CHECK(ctx.has_active_abnormal_status(1, kParaId),
              "T1b：伤害高于 300 → 100% 令对手麻痹");
        CHECK(!ctx.has_active_abnormal_status(0, kParaId),
              "T1b：异常只落对手，不弹自己");
        CHECK_EQ(active_anomaly_count(ctx, 1), 1, "T1b：对手恰一条异常（无多挂）");
        CHECK(ctx.getPet(1).hp < foe_hp0, "T1b：红伤已落地（桥读的是攻击后 final）");
        std::cout << "  [T1b] 1106 高伤触发 通过\n";
    }

    //========== T2 1066（碎岩无影袭 [300,300,150]）==========
    {   // T2a 先出手 + 伤害低于阈值 → 固伤 = 300+150 + 红伤
        ScenarioBattle b(make_party(self_pet({kSuiyan, kHit, kHit, kHit, kHit},
                                              /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_hp0 = ctx.getPet(1).hp;
        play_round(b, 0, 0);   // 红伤 ≈3（atk=1），先手 → 固伤 450
        const int loss = foe_hp0 - ctx.getPet(1).hp;
        CHECK(loss >= 451 && loss <= 465,
              "T2a：先手低伤 → 红伤+450 固伤（红伤≈2~4）；实际扣血=" + std::to_string(loss));
        std::cout << "  [T2a] 1066 先手触发（+450） 通过，扣血=" << loss << "\n";
    }
    {   // T2b 后出手 + 伤害低于阈值 → 固伤仅 300（无先手加成）；
        //     对手先攻把快照写成 attackerId=1 → 顺带验证身份门后自己的红伤正确覆盖读取
        ScenarioBattle b(make_party(self_pet({kSuiyan, kHit, kHit, kHit, kHit},
                                              /*speed=*/50, /*atk=*/1)),
                         make_party(foe_pet(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_hp0 = ctx.getPet(1).hp;
        play_round(b, 0, 0);   // 红伤 ≈3，后手 → 固伤 300
        const int loss = foe_hp0 - ctx.getPet(1).hp;
        CHECK(loss >= 301 && loss <= 315,
              "T2b：后手低伤 → 红伤+300 固伤（无 +150）；实际扣血=" + std::to_string(loss));
        std::cout << "  [T2b] 1066 后手触发（仅 +300） 通过，扣血=" << loss << "\n";
    }
    {   // T2c 伤害达标 → 不附加固伤
        ScenarioBattle b(make_party(self_pet({kSuiyan, kHit, kHit, kHit, kHit},
                                              /*speed=*/300, /*atk=*/300)),
                         make_party(foe_pet(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_hp0 = ctx.getPet(1).hp;
        play_round(b, 0, 0);   // 红伤 ≈344~406（85% 浮动后仍 ≥300）
        const int loss = foe_hp0 - ctx.getPet(1).hp;
        CHECK(loss >= 340 && loss <= 420,
              "T2c：伤害达标 → 纯红伤、无固伤附加；实际扣血=" + std::to_string(loss));
        std::cout << "  [T2c] 1066 达标反例 通过，扣血=" << loss << "\n";
    }

    //========== T3 594（魔帝千裂破 [300,100,6] 害怕）==========
    {   // T3a 伤害低于阈值 → 害怕
        ScenarioBattle b(make_party(self_pet({kQianlie, kHit, kHit, kHit, kHit},
                                              /*speed=*/200, /*atk=*/10)),
                         make_party(foe_pet()));
        b.start();
        play_round(b, 0, 0);   // 伤害 ≈15（<300）
        CHECK(b.ctx().has_active_abnormal_status(1, kFearId),
              "T3a：伤害低于 300 → 100% 令对手害怕");
        CHECK_EQ(active_anomaly_count(b.ctx(), 1), 1, "T3a：对手恰一条异常");
        std::cout << "  [T3a] 594 低伤触发 通过\n";
    }
    {   // T3b 伤害高于阈值 → 不给
        ScenarioBattle b(make_party(self_pet({kQianlie, kHit, kHit, kHit, kHit},
                                              /*speed=*/200, /*atk=*/10000)),
                         make_party(foe_pet()));
        b.start();
        play_round(b, 0, 0);   // 伤害 ≈13400（>300）
        CHECK(!b.ctx().has_active_abnormal_status(1, kFearId),
              "T3b：伤害高于 300 → 不令对手害怕");
        std::cout << "  [T3b] 594 高伤反例 通过\n";
    }

    //========== T4 1106 第二载体（诡魂绝煞破 [280,100,6] 害怕）==========
    {   // T4a 低伤（atk=1；986 先手 +50% 也推不过 280）→ 不给
        ScenarioBattle b(make_party(self_pet({kJuesha, kHit, kHit, kHit, kHit},
                                              /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet(/*speed=*/100)));
        b.start();
        play_round(b, 0, 0);
        CHECK(!b.ctx().has_active_abnormal_status(1, kFearId),
              "T4a：伤害低于 280 → 不令对手害怕");
        std::cout << "  [T4a] 30662 低伤不触发 通过\n";
    }
    {   // T4b 高伤 → 害怕（1106 阈值 280 与异常码 6 的第二组切片）
        ScenarioBattle b(make_party(self_pet({kJuesha, kHit, kHit, kHit, kHit},
                                              /*speed=*/300, /*atk=*/10000)),
                         make_party(foe_pet(/*speed=*/100)));
        b.start();
        play_round(b, 0, 0);
        CHECK(b.ctx().has_active_abnormal_status(1, kFearId),
              "T4b：伤害高于 280 → 100% 令对手害怕");
        CHECK_EQ(active_anomaly_count(b.ctx(), 1), 1, "T4b：对手恰一条异常");
        std::cout << "  [T4b] 30662 高伤触发 通过\n";
    }

    std::cout << "scenario_640 全部通过" << std::endl;
    return 0;
}
