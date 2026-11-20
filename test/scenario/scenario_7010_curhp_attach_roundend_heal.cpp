// 场景 7010 —— K359 攻击附当前体力固伤 + 命中回合末回最大体力族（1045，
// soul_lib/k_curhp_attach_roundend_heal.cpp）
//（2026-10-23，worktree kbatch；号段 7010-7019 → T7010-T7017 八块）
//
// 机制（tips 权威，effect_icon row760，args[3,3]）：
//   「攻击附加自身当前体力1/3的固定伤害；当回合自身攻击命中对手则战斗阶段结束时恢复自身最大体力的1/3」
//   → db[0]=3 = 子句①「自身**当前**体力1/3」的分母；db[1]=3 = 子句②「自身**最大**体力1/3」
//     的分母（两参同值 → 顺序不可观测）。
//   ⚠️ 两个基数不同（当前 vs 最大）——低血时附加显著变小，必须分开读。
//
// 断言口径：
//   · 子句① 触发 = 印主自己的**红伤**命中（DETECT 相位同步，K347 663 / K348 807 同款）：
//     d.attackerId==owner && isRed && final>0；属性直伤/固伤/粉伤不进本管线。
//     附加量 = 印主**DETECT 时点当前体力** / db[0]（整数除法，向下取整）。
//   · 子句② 触发 = 印主命中过 → BATTLE_ROUND_END（"战斗阶段结束时"，K250 1536 同款口径）
//     读"本回合命中旗"→ heal_amount(印主, 印主**最大**体力 / db[1])；旗读到即清
//     （天然"当回合"语义，不跨回合残留）。
//   · 红伤带锚 atk300/def100 拍打 raw ∈ [87,102] → 断言带取 [85,104]（速查 §五锚）。
//   · 附加固伤走粉伤 FIXED 档（吃护罩不吃护盾），对手无护罩/无抗性 → 精确值。
//   · 印主回血受最大体力钳制——本场景先压血造出可观测空间（当前体力同时影响子句①的基数）。
//   · 归属门：只认印主自己造成的红伤；对手打印主不触发（附加目标恒为印主的对手）。
//   · 显式 0 关闸：db[0]=0 → 子句① 关闭（仍回血）；db[1]=0 → 子句② 关闭（仍附加）；
//     两子句独立开关，且都**不回退 fallback**。
//   · boss 门：tips 无 boss 尾注 → 效果在 boss 对局照常生效。
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
constexpr int kMaxHp = 200000;     // 印主最大体力（make_pet 的 hp 兼作 max）
constexpr int kCurHp = 60000;      // 压血后的当前体力（子句① 基数）
constexpr int kCurHp2 = 30000;     // 第二档压血
constexpr int kFoeHp = 200000;     // 对手高血量
constexpr int kRedLo = 85;
constexpr int kRedHi = 104;
constexpr int kAttachOf = 20000;    // 当前体力 60000 / 3
constexpr int kAttachOf2 = 10000;   // 当前体力 30000 / 3
constexpr int kHealOf = 66666;      // 最大体力 200000 / 3

// DB args 原序：[3,3]（db[0]=子句①当前体力分母、db[1]=子句②最大体力分母）
SoulMark make_mark(std::vector<int> args) {
    SoulMark sm(1045, "K359 魂印", "test", EffectArgs(std::move(args)));
    sm.monster_id = 3445;
    return sm;
}

ElfPet mark_pet(std::vector<int> args) {
    ElfPet p = make_pet(1, "赫里斯", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark(std::move(args)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet bare_pet() {
    ElfPet p = make_pet(3, "裸印主", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kFoeHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 反击用对手（atk=300 → 对印主打出同档红伤）
ElfPet foe_striker() {
    ElfPet p = make_pet(2, "反击对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/400, /*atk=*/300, kFoeHp);
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

    // ══ T7010 双子句同回合：附加 当前HP/3 固伤 + 回合末回 最大HP/3 ══════════════
    {
        ScenarioBattle b(make_party(mark_pet({3, 3})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = kCurHp;   // 压血：造回血空间 + 让子句①基数=当前体力
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kAttachOf + kRedLo && r.foe_loss <= kAttachOf + kRedHi,
              "T7010 1045 子句①：对手总掉血 = 附加 20000（当前HP 60000/3）+ 红伤（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [20085,20104]）");
        CHECK_EQ(r.own_heal, kHealOf,
                 "T7010 1045 子句②：印主回合末回血 = 最大HP 200000/3");
        std::cout << "  [T7010] 1045 双子句同回合（附加 20000 + 回血 66666）通过\n";
    }

    // ══ T7011 基数 = **当前**体力（不是最大体力）→ 压得更低则附加更小 ═══════════
    {
        ScenarioBattle b(make_party(mark_pet({3, 3})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = kCurHp2;  // 当前体力 30000 → 附加 10000
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kAttachOf2 + kRedLo && r.foe_loss <= kAttachOf2 + kRedHi,
              "T7011 1045 子句① 基数=当前体力：压到 30000 → 附加 10000（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [10085,10104]）");
        CHECK_EQ(r.own_heal, kHealOf,
                 "T7011 1045 子句② 基数=最大体力（不随当前体力变）：回血仍 66666");
        std::cout << "  [T7011] 1045 基数分离（当前→附加 / 最大→回血）通过\n";
    }

    // ══ T7012 无印对照：既不附加也不回血 ═══════════════════════════════════════
    {
        ScenarioBattle b(make_party(bare_pet()), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = kCurHp;
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kRedLo && r.foe_loss <= kRedHi,
              "T7012 1045 无印对照：对手只掉红伤（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [85,104]）");
        CHECK_EQ(r.own_heal, 0, "T7012 1045 无印对照：印主零回血");
        std::cout << "  [T7012] 1045 无印对照 通过\n";
    }

    // ══ T7013 未命中（双方 NONE）→ 不附加、不回血 ═══════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({3, 3})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = kCurHp;
        const int own_hp0 = b.ctx().getPet(0).hp;
        const int foe_hp0 = b.ctx().getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(b.ctx().getPet(1).hp == foe_hp0,
              "T7013 1045 未命中：对手零掉血（实测差 " +
                  std::to_string(foe_hp0 - b.ctx().getPet(1).hp) + "）");
        CHECK_EQ(b.ctx().getPet(0).hp - own_hp0, 0, "T7013 1045 未命中：印主零回血");
        std::cout << "  [T7013] 1045 未命中静默 通过\n";
    }

    // ══ T7014 显式 0 关闸（db[0]=0）→ 子句① 关闭，子句② 仍回血 ═════════════════
    {
        ScenarioBattle b(make_party(mark_pet({0, 3})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = kCurHp;
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kRedLo && r.foe_loss <= kRedHi,
              "T7014 1045 db[0]=0：子句① 关闭，只掉红伤（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [85,104]）");
        CHECK_EQ(r.own_heal, kHealOf, "T7014 1045 db[0]=0：子句② 不受影响，仍回 66666");
        std::cout << "  [T7014] 1045 显式 0 关闸（db[0]=0）通过\n";
    }

    // ══ T7015 显式 0 关闸（db[1]=0）→ 子句② 关闭，子句① 仍附加 ═════════════════
    {
        ScenarioBattle b(make_party(mark_pet({3, 0})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = kCurHp;
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kAttachOf + kRedLo && r.foe_loss <= kAttachOf + kRedHi,
              "T7015 1045 db[1]=0：子句① 不受影响，仍附加 20000（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [20085,20104]）");
        CHECK_EQ(r.own_heal, 0, "T7015 1045 db[1]=0：子句② 关闭，印主零回血");
        std::cout << "  [T7015] 1045 显式 0 关闸（db[1]=0）通过\n";
    }

    // ══ T7016 归属门：对手打印主 → 印主未出手，不附加、不回血 ═══════════════════
    {
        ScenarioBattle b(make_party(mark_pet({3, 3})), make_party(foe_striker()));
        b.start();
        b.ctx().getPet(0).hp = kCurHp;
        const int own_hp0 = b.ctx().getPet(0).hp;
        const int foe_hp0 = b.ctx().getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const long long own_loss = own_hp0 - b.ctx().getPet(0).hp;
        CHECK(own_loss >= kRedLo && own_loss <= kRedHi,
              "T7016 1045 归属门：对手反击 → 印主只掉红伤（实测 " +
                  std::to_string(own_loss) + "，期望带 [85,104]）");
        CHECK(b.ctx().getPet(1).hp == foe_hp0,
              "T7016 1045 归属门：对手零掉血（对手自己打的伤害不触发附加，实测差 " +
                  std::to_string(foe_hp0 - b.ctx().getPet(1).hp) + "）");
        CHECK_EQ(b.ctx().getPet(0).hp - own_hp0 + own_loss, 0,
                 "T7016 1045 归属门：印主未命中 → 回合末不回血");
        std::cout << "  [T7016] 1045 归属门（只认印主自己的伤害）通过\n";
    }

    // ══ T7017 boss 对局：tips 无 boss 尾注 → 效果照常生效 ════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({3, 3})), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = kCurHp;
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kAttachOf + kRedLo && r.foe_loss <= kAttachOf + kRedHi,
              "T7017 1045 boss 对局：效果照常（附加 20000 仍在，实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [20085,20104]）");
        CHECK_EQ(r.own_heal, kHealOf, "T7017 1045 boss 对局：回合末回血仍 66666");
        std::cout << "  [T7017] 1045 boss 对局照常生效 通过\n";
    }

    // ══ T7018 跨回合不残留：第 1 回合命中回血一次，第 2 回合双方 NONE → 不再回 ═════
    // 「当回合自身攻击命中对手」——命中旗必须**每回合消费一次**（读到即清），
    // 否则第 2 回合会白回一次血（变异测试：清旗行删掉 → 本块转红）。
    {
        ScenarioBattle b(make_party(mark_pet({3, 3})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = kCurHp;
        const int own_hp0 = b.ctx().getPet(0).hp;
        // 第 1 回合：印主出手命中 → 回合末回血 66666
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int hp_after_r1 = b.ctx().getPet(0).hp;
        // 第 2 回合：双方 NONE → 未命中 → 回合末不回血
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int hp_after_r2 = b.ctx().getPet(0).hp;
        CHECK_EQ(hp_after_r1 - own_hp0, kHealOf,
                 "T7018 1045 第 1 回合命中 → 回合末回血 66666");
        CHECK_EQ(hp_after_r2 - hp_after_r1, 0,
                 "T7018 1045 第 2 回合未命中 → 不再回血（命中旗不跨回合残留）");
        std::cout << "  [T7018] 1045 跨回合不残留 通过\n";
    }

    std::cout << "scenario_7010 PASS (K359 攻击附当前体力固伤 + 命中回合末回最大体力族)" << std::endl;
    return 0;
}
