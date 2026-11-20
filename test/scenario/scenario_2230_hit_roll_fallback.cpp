#include <utils/sbs_rand.h>
// 场景 2230 —— J129 命中掷骰兜底族（1 宏，moves_lib/j_hit_roll_fallback.cpp）
//（2026-10-04，worktree ../agents/j129；号段 2230-2239 → T2230 一块三例）
//
// 断言口径（红伤带 = 0.84×P×A/D，A=300/D=100；kMaxHp=800000 → 异常 tick 1/8=100000 整除）：
//   - a 命中落地：33419 [50,1,200]（**纯链**；sbs_srand(1) 下掷骰实测落地）→ 对手中毒；
//     毒 tick 在对手行动起点结算（探针挂己方 AFTER_ACTION，先于对手行动）→ 扣减法
//     隔离红伤（J23 97 同式）；
//   - b 免疫兜底：对手 R1 开 48 免异常窗（4 回合），R2 自方 32975 [50,15,200] 命中
//     → apply 必 TARGET_IMMUNE（与掷点无关，确定性）→ 兜底恰 200 固伤、异常不落地；
//   - c miss 门：载体 accuracy 置 0（必 miss，execute 提前 return 探针不注册）→
//     无红伤、无固伤、无异常。
//
// ⚠️ 载体选型教训（2026-10-04 实测）：37377 星光·粉末击打 [917,1000] 是 100% 档
//   理想载体，但同链 917（"命中后{0}%使对手{1}，未触发则附加自身最大体力 1/{2}
//   百分比伤害"）虽无插件注册，会被**内核运行时文本 parser**（skills.cpp
//   parse_effect_unit 兜底路径）原生解析执行——100% 上寄生(4) → 场景多挨一段
//   1/8 tick。"未实现"实为三条通道：插件注册/内核硬编码/**文本 parser 兜底**；
//   选载体第三查必须连同 parser 覆盖面一起看，纯链载体永远最稳。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kFenMo   = 33419;  // 未来·粉末击打 [1000]|[50,1,200]：特130 纯链（a/c 用）
constexpr int kFengXue = 32975;  // 风雪狂怒 [1000]|[50,15,200]：物150 纯链（b 用）
constexpr int kNingShen = 17091; // 凝神聚力斩 [48]|[4]：物150（对手用：4 回合免疫异常）
constexpr int kPaiDa    = 10004; // 拍打：填充位

constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);  // 1
constexpr int kMaxHp  = 800000;  // 异常 tick = max/8 = 100000 整除（扣减法用）

void accuracy_set(ElfPet& p, int acc, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = acc;
        s.must_hit = must_hit ? 1 : 0;
    }
}

ElfPet self_pet(int carrier, int acc, bool must_hit) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp);
    accuracy_set(p, acc, must_hit);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, kMaxHp);
    accuracy_set(p, 100, false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2230.a 命中落地：sbs_srand(1) 掷骰落地 → 中毒 + 毒 tick 扣减（未来·粉末击打）══
    {
        sbs_srand(1);   // rand#1=红伤浮动、rand#2=50% 掷骰（实测 49<50 落地）
        ScenarioBattle b(make_party(self_pet(kFenMo, 100, true)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 自方载体
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);          // R1 对手不出手
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, kPoison),
              "T2230.a 1000：命中掷骰落地中毒（sbs_srand(1) 种子）");
        const int loss = kMaxHp - ctx.getPet(1).hp;
        const int hit = loss - kMaxHp / 8;   // 减毒 tick（=100000）
        CHECK(hit >= 278 && hit <= 328,
              "T2230.a 1000：红伤带（扣 tick 后实测 " + std::to_string(hit) + "，无固伤混杂）");
    }
    // ══ T2230.b 免疫兜底：对手免疫期命中 → 恰 200 固伤、异常不落（风雪狂怒）═════
    {
        ScenarioBattle b(make_party(self_pet(kFengXue, 100, true)),
                         make_party(foe_pet({kNingShen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);          // R1 自方不出手
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手开免疫
        b.drive();
        const int hp1 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 自方载体命中
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, 15) == false,
              "T2230.b 1000：免疫期异常不落地");
        const int loss2 = hp1 - ctx.getPet(1).hp;
        CHECK(loss2 >= 521 && loss2 <= 578,
              "T2230.b 1000：红伤 [321,378] + 兜底恰 200（实测 " + std::to_string(loss2) + "）");
    }
    // ══ T2230.c miss 门：accuracy=0 必 miss → 全链不触发（未来·粉末击打）═══════
    {
        ScenarioBattle b(make_party(self_pet(kFenMo, 0, false)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 自方载体（必 miss）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int loss = kMaxHp - ctx.getPet(1).hp;
        CHECK_EQ(loss, 0,
                 "T2230.c 1000：miss 无红伤无固伤（实测掉 " + std::to_string(loss) + "）");
        CHECK(ctx.has_active_abnormal_status(1, kPoison) == false,
              "T2230.c 1000：miss 不掷骰不施异常");
    }

    std::cout << "scenario_2230 PASS (J129 命中掷骰兜底族 1000)" << std::endl;
    return 0;
}
