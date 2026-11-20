// 场景 2000 —— J106 属性和粉伤族（2 宏，moves_lib/j_panel_pink.cpp）
//（2026-10-03，worktree ../agents/j106；号段 2000-2009 → T2000-T2001 两块）
//
// 断言口径：
//   - 1186 = 逆反之骨（1186=[3,40]；原载体壁垒模式 25525 链首 1185 被 J72 并行
//     实现，合并后链激活翻车 → 换 27876：191=自身免疫异常（无伤害）、998=被消
//     watcher（无触发）全惰性）：双防视图和 200 → 40% 粉 = 80/次。
//     R1 武装（当次不附加）；R2 拍打（攻击技触发：红 [58,69] + 粉 80 = [138,149]）；
//     R3 用属性技（再上：used_attribute 门 → 无粉，对手 hp 持平）；
//   - 1163 = 灿烂极光 37306 [1163]|[20,10,40]（特130 纯链）：SA 面板 200 + SPEED
//     面板 300 = 500 → R1 20%=100、R2 30%=150、R3 40%=200（封顶）——红伤带 [187,220]
//     叠加后三段带互不重叠；对手全程 NONE（承伤唯一来源=本族）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa    = 10004;   // 拍打
constexpr int kNiFan    = 27876;   // 逆反之骨[191,1186,998]|[4,3,40,3,300,300]：属性技（1186=[3,40]；
                                   // 191=自身免疫反弹异常（无伤害面）、998=被消 watcher（本局无触发）——
                                   // 合并后仍惰性；原壁垒模式 25525 链首 1185 被 J72 实现，链激活弃用）
constexpr int kCanLan   = 37306;   // 灿烂极光[1163]|[20,10,40]：特130（特攻+速面板和粉伤递增封顶）

constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, 200, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2000 1186 每回合攻击附加双防和 30% 粉伤（魔钢之盾 [3,30]）══
    {
        ScenarioBattle b(make_party(self_pet({kNiFan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装（当次不附加）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(kMaxHp - ctx.getPet(1).hp, 0,
                 "T2000 前置：R1 载体当次不附加（对手满血，实测掉 " +
                     std::to_string(kMaxHp - ctx.getPet(1).hp) + "）");
        int prev = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（触发）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int delta_r2 = prev - ctx.getPet(1).hp;
        CHECK(delta_r2 >= 138 && delta_r2 <= 149,
              "T2000 1186：攻击技触发红伤+粉 80（实测 " + std::to_string(delta_r2) +
                  "，带 [138,149]）");
        prev = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 属性技（不触发）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int delta_r3 = prev - ctx.getPet(1).hp;
        CHECK_EQ(delta_r3, 0,
                 "T2000 1186：属性技不触发（实测 " + std::to_string(delta_r3) + "）");
        std::cout << "  [T2000] 1186 双防粉伤窗 通过\n";
    }

    // ══ T2001 1163 面板和粉伤·连用递增封顶（灿烂极光 [20,10,40]）══
    {
        ScenarioBattle b(make_party(self_pet({kCanLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        struct Expect {
            int lo;
            int hi;
            const char* tag;
        };
        const Expect rounds[] = {
            {287, 320, "R1 20%（累计）"},
            {624, 690, "R2 30%（累计）"},
            {1011, 1110, "R3 40% 封顶（累计）"},
        };
        for (const auto& r : rounds) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 连用
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(loss >= r.lo && loss <= r.hi,
                  std::string("T2001 1163：") + r.tag + "（累计实测 " +
                      std::to_string(loss) + "，带 [" + std::to_string(r.lo) + "," +
                      std::to_string(r.hi) + "]）");
        }
        std::cout << "  [T2001] 1163 面板粉伤递增 通过\n";
    }

    std::cout << "scenario_2000 PASS (J106 属性和粉伤族 1186/1163)" << std::endl;
    return 0;
}
