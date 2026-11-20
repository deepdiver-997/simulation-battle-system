// 场景 5090 —— K347 伤害对半阈值分支族（663，soul_lib/k_damage_half_threshold.cpp）
//（2026-10-23，worktree kbatch；号段 5090-5099 → T5090-T5095 六块）
//
// 机制（tips 两行同文，行≠宏 → 一份实现两套数值档）：
//   「若造成伤害小于对手体力的1/2,则追加250点固定伤害;
//     若伤害大于对手体力1/2,则造成伤害的50%恢复自身体力」
//   本体行 args[2,250,50]（db[0]=「1/2」的分母、db[1]=追加固伤、db[2]=恢复比例）
//   伙伴效果行 args[2,350,60]（同一 effect_id 663，另一行数值档）
//
// 断言口径：
//   · 红伤带锚 atk300/def100 拍打 raw ∈ [87,102] → 断言带取 [85,104]（速查 §五锚）。
//   · 「对手体力」= DETECT 时点现值（本次命中尚未落地扣血）→ 小伤害分支用
//     「对手高血量 200000」稳定落入；大伤害分支用「对手 150 血」（半血 75 < 红伤下界 85）。
//   · 大伤害分支的回血量 = floor(红伤 × db[2] / 100)，而红伤恰等于对手掉血
//     → 断言「印主回血 == floor(对手掉血 × 比例 / 100)」的**精确等式**（不靠带）。
//   · 小伤害分支的追加固伤 = 250（粉伤 FIXED，无抗性无护罩 → 精确），
//     故断言「对手总掉血 - 250 ∈ [85,104]」。
//   · 印主先压血 1000（造可观测回血空间），断言回血后精确值 → 小伤害分支印主**零回血**。
//   · boss 门：tips 无 boss 尾注 → 效果在 boss 对局照常生效（断言 +250 仍在）。
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;
constexpr int kOwnHp = 200000;
constexpr int kFoeHpBig = 200000;   // 对手高血量 → 恒入「小伤害」分支
constexpr int kFoeHpSmall = 150;    // 对手低血量 → 恒入「大伤害」分支
constexpr int kRedLo = 85;
constexpr int kRedHi = 104;
constexpr int kPressHp = 1000;       // 印主压血（造回血可观测空间）

// DB args 原序：本体行 [2,250,50] / 伙伴效果行 [2,350,60]
SoulMark make_mark(std::vector<int> args) {
    SoulMark sm(663, "K347 魂印", "test", EffectArgs(std::move(args)));
    sm.monster_id = 2939;
    return sm;
}

ElfPet mark_pet(std::vector<int> args) {
    ElfPet p = make_pet(1, "光之子·米瑞斯", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kOwnHp, make_mark(std::move(args)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet bare_pet() {
    ElfPet p = make_pet(3, "裸印主", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kOwnHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int hp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 反击用对手（atk=300 → 对印主打出同档红伤）
ElfPet foe_striker(int hp) {
    ElfPet p = make_pet(2, "反击对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/400, /*atk=*/300, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 印主出一刀（拍打），对手 NONE；返回 {对手掉血, 印主回血}
struct Round {
    long long foe_loss = 0;
    long long own_heal = 0;
};

Round one_strike(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int foe_hp0 = ctx.getPet(1).hp;
    const int own_hp0 = ctx.getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    Round r;
    r.foe_loss = foe_hp0 - ctx.getPet(1).hp;
    r.own_heal = ctx.getPet(0).hp - own_hp0;
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5090 本体行「小伤害」分支：伤害 < 对手半血 → 追加 250 固伤 ==========
    // 对手 200000 血（半血 100000 ≫ 红伤上界 104）→ 稳定落入小伤害分支。
    {
        ScenarioBattle b(make_party(mark_pet({2, 250, 50})), make_party(foe_pet(kFoeHpBig)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp -= kPressHp;   // 压血：万一触发回血也能观测到
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= 250 + kRedLo && r.foe_loss <= 250 + kRedHi,
              "T5090 663 本体行小伤害：对手总掉血 = 追加 250 固伤 + 红伤（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [335,354]）");
        CHECK(r.own_heal == 0,
              "T5090 663 小伤害分支：印主零回血（实测 " + std::to_string(r.own_heal) + "）");
        std::cout << "  [T5090] 663 本体行小伤害分支（+250 固伤 / 零回血）通过\n";
    }

    // ══ T5091 本体行「大伤害」分支：伤害 > 对手半血 → 回血 50% ==============
    // 对手 150 血（半血 75 < 红伤下界 85）→ 稳定落入大伤害分支。
    {
        ScenarioBattle b(make_party(mark_pet({2, 250, 50})), make_party(foe_pet(kFoeHpSmall)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp -= kPressHp;
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kRedLo && r.foe_loss <= kRedHi,
              "T5091 663 本体行大伤害：对手掉血 = 纯红伤（无追加固伤，实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [85,104]）");
        const long long expect_heal = r.foe_loss * 50 / 100;
        CHECK(r.own_heal == expect_heal,
              "T5091 663 大伤害分支：印主回血 = floor(红伤×50%)（实测回血 " +
                  std::to_string(r.own_heal) + "，期望 " + std::to_string(expect_heal) + "）");
        CHECK(r.own_heal > 0,
              "T5091 663 大伤害分支：回血必须可观测（实测 " + std::to_string(r.own_heal) + "）");
        std::cout << "  [T5091] 663 本体行大伤害分支（回血 = 红伤×50%）通过\n";
    }

    // ══ T5092 无印对照：既无追加固伤也无回血 ================================
    {
        ScenarioBattle b(make_party(bare_pet()), make_party(foe_pet(kFoeHpBig)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp -= kPressHp;
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kRedLo && r.foe_loss <= kRedHi,
              "T5092 无印对照：对手只掉红伤（实测 " + std::to_string(r.foe_loss) +
                  "，期望带 [85,104]）");
        CHECK(r.own_heal == 0,
              "T5092 无印对照：印主零回血（实测 " + std::to_string(r.own_heal) + "）");
        std::cout << "  [T5092] 663 无印对照 通过\n";
    }

    // ══ T5093 伙伴效果行 [2,350,60]：小伤害分支追加 350 ======================
    {
        ScenarioBattle b(make_party(mark_pet({2, 350, 60})), make_party(foe_pet(kFoeHpBig)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp -= kPressHp;
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= 350 + kRedLo && r.foe_loss <= 350 + kRedHi,
              "T5093 663 伙伴效果行小伤害：对手总掉血 = 追加 350 固伤 + 红伤（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [435,454]）");
        CHECK(r.own_heal == 0,
              "T5093 663 伙伴效果行小伤害：印主零回血（实测 " + std::to_string(r.own_heal) + "）");
        std::cout << "  [T5093] 663 伙伴效果行小伤害分支（+350 固伤）通过\n";
    }

    // ══ T5094 伙伴效果行 [2,350,60]：大伤害分支回血 60% ======================
    {
        ScenarioBattle b(make_party(mark_pet({2, 350, 60})), make_party(foe_pet(kFoeHpSmall)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp -= kPressHp;
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kRedLo && r.foe_loss <= kRedHi,
              "T5094 663 伙伴效果行大伤害：对手掉血 = 纯红伤（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [85,104]）");
        const long long expect_heal = r.foe_loss * 60 / 100;
        CHECK(r.own_heal == expect_heal,
              "T5094 663 伙伴效果行大伤害：印主回血 = floor(红伤×60%)（实测回血 " +
                  std::to_string(r.own_heal) + "，期望 " + std::to_string(expect_heal) + "）");
        std::cout << "  [T5094] 663 伙伴效果行大伤害分支（回血 = 红伤×60%）通过\n";
    }

    // ══ T5095 boss 对局：tips 无 boss 尾注 → 效果照常生效 ====================
    {
        ScenarioBattle b(make_party(mark_pet({2, 250, 50})), make_party(foe_pet(kFoeHpBig)));
        b.ctx().is_boss_challenge = true;
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp -= kPressHp;
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= 250 + kRedLo && r.foe_loss <= 250 + kRedHi,
              "T5095 663 boss 对局：效果照常（追加 250 固伤仍在，实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [335,354]）");
        std::cout << "  [T5095] 663 boss 对局照常生效 通过\n";
    }

    // ══ T5096 显式 0 关闸档：db[2]=0（恢复 0%）→ 走大伤害分支但零回血 ════
    // 「在位即消费」纪律：显式 0 = 关闸，**不得**回退到 fallback 50。
    {
        ScenarioBattle b(make_party(mark_pet({2, 250, 0})), make_party(foe_pet(kFoeHpSmall)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp -= kPressHp;
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kRedLo && r.foe_loss <= kRedHi,
              "T5096 663 显式 0 档：大伤害分支仍不加固伤（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [85,104]）");
        CHECK(r.own_heal == 0,
              "T5096 663 显式 0 档：恢复比例 0 → 零回血且不回退 50%（实测回血 " +
                  std::to_string(r.own_heal) + "）");
        std::cout << "  [T5096] 663 显式 0 关闸档（恢复 0% 不回退）通过\n";
    }

    // ══ T5097 归属门：只认**印主自己**造成的红伤（对手反击不触发）══════════
    {
        ScenarioBattle b(make_party(mark_pet({2, 250, 50})),
                         make_party(foe_striker(kFoeHpBig)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp -= kPressHp;
        const int own_hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const long long own_loss = own_hp0 - ctx.getPet(0).hp;
        CHECK(own_loss >= kRedLo && own_loss <= kRedHi,
              "T5097 663 归属门：对手反击 → 印主只掉红伤，不被追加 250 固伤（实测 " +
                  std::to_string(own_loss) + "，期望带 [85,104]）");
        CHECK(ctx.getPet(1).hp == foe_hp0,
              "T5097 663 归属门：对手零回血（实测差 " +
                  std::to_string(foe_hp0 - ctx.getPet(1).hp) + "）");
        std::cout << "  [T5097] 663 归属门（只认印主自己的伤害）通过\n";
    }

    std::cout << "scenario_5090 PASS (K347 伤害对半阈值分支族)" << std::endl;
    return 0;
}
