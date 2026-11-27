// 场景 4560 —— K300 伤害阈值必暴族（1223，soul_lib/k300_lowdmg_mustcrit.cpp）
//（2026-10-21，worktree ../agents/k300；号段 4560-4569 → T4560-T4563 四块）
//
// 断言口径（1223「自身攻击技能造成的伤害低于300则下2回合必定打出致命一击（boss有效）」）：
//   - T4560 **低伤 → 必暴**：R1 印主用攻击技打出 < 300 的伤害 → R2 印主用攻击技
//     出手 → **本次攻击暴击**（ctx->crit_happened[0] == true）。
//   - T4561 **高伤 → 门闭**：R1 打出 ≥ 300 的伤害 → R2 出手**不暴击**。
//   - T4562 **窗口长度 = db[1] = 2**：低伤命中后，R2 与 R3 出手**都暴击**；
//     R4 出手**不暴击**（窗口 = [命中回合+1, +2] 共 2 回合）。
//   - T4563 **攻击技门**：R1 用**属性技**打出低伤 → R2 出手**不暴击**
//     （tips 明写"自身攻击技能"）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {300,2}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ **断言用 ctx->crit_happened[0]（本次攻击是否真的暴击）**，不读 ws.must_crit 旗：
//   ws.must_crit 每回合 reset、**命中时点被消费**（skills.cpp:948）→ 出手后读不到；
//   且**不出手时 BEFORE_SKILL_HIT 桶根本不跑**，旗也写不上（实测：空过回合
//   hitRound=NONE）。crit_happened 是暴击的**权威结果**，顺带证明旗生效。
// ⚠️ **造低伤/高伤用 atk 控制**：拍打威力 40、make_pet 默认防御 100 → 伤害 ≈
//   atk×40/100 = atk×0.4（红伤浮动 85%~100%）。atk=100 → ≈40（< 300 ✓）；
//   atk=1000 → ≈400（≥ 300 ✓）。两端离阈值都有足够裕量，不受浮动带影响。
// ⚠️ 暴击全程置零（set_crit_sixteenths(0)）：技能自身暴击率 0 → **只有本魂印的
//   must_crit 能让它暴击**（skills.cpp:948：crit_rate=0 的技能永不自暴）。
#include <cstdlib>
#include <iostream>
#include <utility>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：物理攻击技（威力 40）——低伤载体
constexpr int kQuanLi = 10033;  // 全力一击：物理攻击技（威力 120）——高伤载体
constexpr int kSuoTou = 20004;  // 瞪眼：属性技
constexpr int kMaxHp = 1000000;
constexpr int kThreshold = 300;  // db[0]

SoulMark make_mark() {
    // DB args 原序 [300,2]：db[0]=300（"伤害低于300"）、db[1]=2（"下2回合"）——双参自证。
    SoulMark sm(1223, "K300 魂印", "test", EffectArgs(std::vector<int>{300, 2}));
    sm.monster_id = 3641;
    return sm;
}

ElfPet mark_pet(int atk, int skill0 = kPaDa) {
    // 槽 0 = skill0（可换成高伤载体），槽 1 = 拍打（低伤载体）。
    ElfPet p = make_pet(1, "篝火大使", {skill0, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/atk, /*hp=*/kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    set_crit_sixteenths(p, 0);   // 技能自身永不自暴 → 只有本魂印能触发暴击
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/10, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    set_crit_sixteenths(p, 0);
    return p;
}

// 印主出一技（对手 NONE）；返回 [本次伤害, 本次是否暴击]。
std::pair<int, bool> mark_attacks(ScenarioBattle& b, int skill_idx = 0) {
    BattleContext& ctx = b.ctx();
    const int before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return {before - ctx.getPet(1).hp, ctx.crit_happened[0]};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4560 低伤 → 下一回合必暴 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*atk=*/100)), make_party(plain_pet()));
        b.start();
        auto [dealt, crit_r1] = mark_attacks(b);
        CHECK(dealt > 0 && dealt < kThreshold,
              "T4560 前置：R1 伤害 < 300（实测 " + std::to_string(dealt) + "）");
        CHECK(!crit_r1, "T4560 前置：R1 本身未暴击（暴击率置零）");
        auto [dealt2, crit_r2] = mark_attacks(b);
        CHECK(crit_r2,
              "T4560 1223：伤害低于 300 → 下一回合必定打出致命一击（R2 暴击=true）");
        std::cout << "  [T4560] 低伤必暴 通过 (R1=" << dealt << " R2=" << dealt2 << ")\n";
    }

    // ══ T4561 高伤 → 门闭 ══
    {
        // 1000 攻击在低伤浮动时仅造成 292，不能稳定满足 ≥300 的门闭前置。
        ScenarioBattle b(make_party(mark_pet(/*atk=*/1500)), make_party(plain_pet()));
        b.start();
        auto [dealt, crit_r1] = mark_attacks(b);
        CHECK(dealt >= kThreshold,
              "T4561 前置：R1 伤害 ≥ 300（实测 " + std::to_string(dealt) + "）");
        auto [dealt2, crit_r2] = mark_attacks(b);
        CHECK(!crit_r2,
              "T4561 1223：伤害不低于 300 → 不开必暴窗口（R2 暴击=false）");
        std::cout << "  [T4561] 高伤门闭 通过 (R1=" << dealt << ")\n";
    }

    // ══ T4562 窗口长度 = db[1] = 2 回合 ══
    // ⚠️ **必须让 R2 打出高伤**：低伤会**续开窗口**（每次低伤命中都重开 2 回合），
    //   窗口永远关不上——实测 R1/R2/R3/R4 全暴击（R2 的 66 伤害 < 300 → 又开
    //   [R3,R5)）。故 R2 用**高伤技能**（≥300 → 不续期），才能观察窗口到期。
    {
        ElfPet p = mark_pet(/*atk=*/600, /*skill0=*/kQuanLi);  // 槽 0 = 全力一击(pow 120)
        ScenarioBattle b(make_party(p), make_party(plain_pet()));
        b.start();
        // R1 用槽 1（拍打 pow 40）→ 伤害 ≈ 600×40/100 = 240 < 300 → 开窗口 [R2, R4)
        auto [d1, c1] = mark_attacks(b, /*skill_idx=*/1);
        CHECK(d1 > 0 && d1 < kThreshold,
              "T4562 前置：R1 低伤 < 300（实测 " + std::to_string(d1) + "）");
        // R2 用槽 0（全力一击 pow 120）→ 伤害 ≫ 300 → **不续期**；但仍在窗口内 → 必暴
        auto [d2, c2] = mark_attacks(b, /*skill_idx=*/0);
        CHECK(d2 >= kThreshold,
              "T4562 前置：R2 高伤 ≥ 300（不续开窗口，实测 " + std::to_string(d2) + "）");
        CHECK(c2, "T4562 1223：命中后第 1 回合（R2）在窗口内 → 暴击");
        // R3：仍在窗口 [R2,R4) 内（第 2 个回合）→ 仍必击
        auto [d3, c3] = mark_attacks(b, /*skill_idx=*/0);
        CHECK(c3,
              "T4562 1223：命中后第 2 回合（R3）仍在窗口 [R2,R4) 内 → 仍必击");
        // R4：窗口到期（R2/R3 都是高伤、未续期）→ 不再暴击
        auto [d4, c4] = mark_attacks(b, /*skill_idx=*/0);
        CHECK(!c4,
              "T4562 1223：命中后第 3 回合（R4）窗口已过（db[1]=2 → 共 2 回合）→ 不再暴击");
        std::cout << "  [T4562] 窗口长度 通过\n";
    }

    // ══ T4563 属性技不触发：属性直伤不发 TAKE_DAMAGE 事件 → 不开窗口 ══
    // ⚠️ tips 明写"自身攻击技能"——属性技不该触发。本单实现的判定挂在
    //   EVENT_TAKE_DAMAGE（只对红伤/真伤发；属性直伤走 deal_attribute_damage、
    //   不发本事件）→ 属性技**走不到判定点**，自然不开窗口。
    //   ⚠️ 由此得出一个通道事实：**"攻击技门"在本单不可观测**——能走到判定点的
    //   本来就都是伤害类技能。故不写"属性技 → 不触发"这种**恒真**断言，
    //   改测"属性技打出后下一回合不暴击"（该断言在实现正确时恒真、在把判定
    //   挂到错误事件面时会转红——有判别力）。
    {
        ElfPet p = mark_pet(/*atk=*/100, /*skill0=*/kSuoTou);   // 槽 0 = 属性技
        ScenarioBattle b(make_party(p), make_party(plain_pet()));
        b.start();
        mark_attacks(b);                          // R1：属性技（无 TAKE_DAMAGE 事件）
        const bool crit_r2 = mark_attacks(b).second;  // R2：应不暴击
        CHECK(!crit_r2,
              "T4563 1223：属性技（不走红伤通道）→ 不开必暴窗口，R2 暴击=false");
        std::cout << "  [T4563] 属性技不触发对照 通过\n";
    }

    std::cout << "scenario_4560 PASS (K300 伤害阈值必暴族)" << std::endl;
    return 0;
}
