// 场景 7000 —— K358 自身半血门攻击附伤族（1421，soul_lib/k_half_gate_attach.cpp）
//（2026-10-23，worktree kbatch；号段 7000-7009 → T7000-T7007 八块）
//
// 机制（tips 权威，effect_icon row1107，args[2,150]）：
//   「自身体力高于最大体力的1/2时攻击技能命中后附加150点固定伤害，自身体力低于最大体力的
//     1/2时附加的固定伤害效果翻倍」
//   → db[0]=2 = tips 字面「1/2」的分母；db[1]=150 = 附加固伤点数（低体力档 ×2 = 300）。
//   行≠宏检查：1421 全库仅一行（row1107 / 时分公主 3832）——无同 id 第二行。
//
// 断言口径：
//   · 触发 = 印主自己的**红伤**命中（DETECT 相位同步）：d.attackerId==owner && isRed
//     && final>0；属性直伤/固伤/粉伤不进本管线。
//   · 分档看「印主自身当前体力」vs「印主自身最大体力」（不是对手体力！tips 主语是自身）。
//     乘 db[0] 比大小（hp×2 vs max）而非先除后比——避免整数截断把恰好一半判错档。
//   · 等号档（hp×2 == max）：tips 只写「高于」「低于」，等号无归属 → **两分支皆不触发**
//     （与 K347 663 / K245 / K246 同款严格对照口径；备选读法见实现头注释的记档）。
//   · 红伤带锚 atk300/def100 拍打 raw ∈ [87,102] → 断言带取 [85,104]（速查 §五锚）。
//   · 附加固伤走粉伤 FIXED 档（吃护罩不吃护盾），对手无护罩/无抗性 → 精确 150 / 300。
//   · 归属门：只认印主自己造成的红伤；对手反击不触发（附加目标恒为印主的对手）。
//   · 显式 0 关闸：db[1]=0（附加点数 0）与 db[0]=0（分母非法）都关闸，**不回退 fallback**。
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
constexpr int kMaxHp = 200000;      // 印主最大体力（make_pet 的 hp 兼作 max）
constexpr int kHalfHp = 100000;     // 恰好一半（等号档）
constexpr int kLowHp = 40000;       // 明显低于一半
constexpr int kFoeHp = 200000;      // 对手高血量（不会被附加固伤打死）
constexpr int kRedLo = 85;
constexpr int kRedHi = 104;
constexpr int kFlat = 150;          // db[1]：高体力档附加点数
constexpr int kFlatLow = 300;       // db[1]×2：低体力档（翻倍）

// DB args 原序：[2,150]（db[0]=「1/2」分母、db[1]=附加固伤点数）
SoulMark make_mark(std::vector<int> args) {
    SoulMark sm(1421, "K358 魂印", "test", EffectArgs(std::move(args)));
    sm.monster_id = 3832;
    return sm;
}

ElfPet mark_pet(std::vector<int> args) {
    ElfPet p = make_pet(1, "时分公主", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

// 印主出一刀（拍打），对手 NONE；返回 {对手掉血, 印主掉血}
struct Round {
    long long foe_loss = 0;
    long long own_loss = 0;
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
    r.own_loss = own_hp0 - ctx.getPet(0).hp;
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7000 高体力档（hp=max=200000 > 100000）→ 附加 150 点固伤 ══════════════
    {
        ScenarioBattle b(make_party(mark_pet({2, kFlat})), make_party(foe_pet()));
        b.start();
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kFlat + kRedLo && r.foe_loss <= kFlat + kRedHi,
              "T7000 1421 高体力档：对手总掉血 = 附加 150 固伤 + 红伤（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [235,254]）");
        CHECK(r.own_loss == 0,
              "T7000 1421 高体力档：印主零掉血（对手 NONE，实测 " +
                  std::to_string(r.own_loss) + "）");
        std::cout << "  [T7000] 1421 高体力档（+150 固伤）通过\n";
    }

    // ══ T7001 低体力档（hp=40000 < 100000）→ 翻倍附加 300 点固伤 ════════════════
    {
        ScenarioBattle b(make_party(mark_pet({2, kFlat})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = kLowHp;   // 压到一半以下（最大体力仍是 200000）
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kFlatLow + kRedLo && r.foe_loss <= kFlatLow + kRedHi,
              "T7001 1421 低体力档：对手总掉血 = 翻倍 300 固伤 + 红伤（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [385,404]）");
        std::cout << "  [T7001] 1421 低体力档（+300 固伤 = 150×2）通过\n";
    }

    // ══ T7002 等号档（hp=100000，恰好一半）→ 两分支皆不触发 ═════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({2, kFlat})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = kHalfHp;
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kRedLo && r.foe_loss <= kRedHi,
              "T7002 1421 等号档：只掉红伤，无附加固伤（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [85,104]）");
        std::cout << "  [T7002] 1421 等号档（恰好一半，两分支皆不触发）通过\n";
    }

    // ══ T7003 无印对照：既不附加也不翻倍 ═══════════════════════════════════════
    {
        ScenarioBattle b(make_party(bare_pet()), make_party(foe_pet()));
        b.start();
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kRedLo && r.foe_loss <= kRedHi,
              "T7003 1421 无印对照：对手只掉红伤（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [85,104]）");
        std::cout << "  [T7003] 1421 无印对照 通过\n";
    }

    // ══ T7004 boss 对局：tips 无 boss 尾注 → 效果照常生效 ════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({2, kFlat})), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kFlat + kRedLo && r.foe_loss <= kFlat + kRedHi,
              "T7004 1421 boss 对局：效果照常（附加 150 固伤仍在，实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [235,254]）");
        std::cout << "  [T7004] 1421 boss 对局照常生效 通过\n";
    }

    // ══ T7005 显式 0 关闸（db[1]=0）→ 不附加，且不回退 150 ═════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({2, 0})), make_party(foe_pet()));
        b.start();
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kRedLo && r.foe_loss <= kRedHi,
              "T7005 1421 显式 0 档（db[1]=0）：只掉红伤，不附加（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [85,104]）");
        std::cout << "  [T7005] 1421 显式 0 关闸（db[1]=0 不回退）通过\n";
    }

    // ══ T7006 分母非法关闸（db[0]=0）→ 不附加，且不回退分母 2 ════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({0, kFlat})), make_party(foe_pet()));
        b.start();
        const Round r = one_strike(b);
        CHECK(r.foe_loss >= kRedLo && r.foe_loss <= kRedHi,
              "T7006 1421 分母 0 关闸（db[0]=0）：只掉红伤，不附加（实测 " +
                  std::to_string(r.foe_loss) + "，期望带 [85,104]）");
        std::cout << "  [T7006] 1421 分母 0 关闸 通过\n";
    }

    // ══ T7007 归属门：只认印主自己的红伤（对手反击不触发）════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({2, kFlat})), make_party(foe_striker()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int own_hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const long long own_loss = own_hp0 - ctx.getPet(0).hp;
        CHECK(own_loss >= kRedLo && own_loss <= kRedHi,
              "T7007 1421 归属门：对手反击 → 印主只掉红伤（实测 " +
                  std::to_string(own_loss) + "，期望带 [85,104]）");
        CHECK(ctx.getPet(1).hp == foe_hp0,
              "T7007 1421 归属门：对手零掉血（对手自己打的伤害不触发印主的附加，实测差 " +
                  std::to_string(foe_hp0 - ctx.getPet(1).hp) + "）");
        std::cout << "  [T7007] 1421 归属门（只认印主自己的伤害）通过\n";
    }

    std::cout << "scenario_7000 PASS (K358 自身半血门攻击附伤族)" << std::endl;
    return 0;
}
