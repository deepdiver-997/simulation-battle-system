#include <utils/sbs_rand.h>
// 场景 7340 —— K383 条件先制+承伤分流族
//（1864，soul_lib/k383_cond_preempt_hit_split.cpp；号段 7340，2026-10-24 a24-kprod）
//
// 断言口径（a24-kprod，承接本会话 K381 实测定版：round N 全程 rc=N-1、
//   引擎 end=rc+rounds / 活跃 rc<end；伤害 raw 带 atk300/def100 → [87,102]、
//   随攻击面板线性缩放，atk1500 → [433,510]；粉伤 FIXED 无浮动=精确值）：
//   - a ①条件先制（row1502 db0=1）：r1 开局无提升 → FMR 停泊 pl==0（负控）；
//     r1 手术抬防 +2 → r2 ROUND_START 快照命中 → r2 FMR pl==1；
//   - b ②粉伤窗（row1502 db1=30）：对手挂 735 吸取魂印（ROUND_END 对_holder 粉伤
//     1000 精确值=粉源）+ atk1500（红伤 >300 走 ③> 支不污染）：r1 对手用攻击技
//     （EVENT_SKILL_USED 立旗）→ 同回合 ROUND_END 的 735 粉伤 ×0.7；r2 对手 NONE
//     （无使用）→ 粉伤足额（窗只在"当回合用攻击技后"）；对照无印世界/row1705(×0.5)
//     三带两两不相交；
//   - c ③>300 下回合受攻减半（row1502 db3=50）：r1 对手刀 [433,510]>300 → r2 刀
//     ×0.5 ∈ [215,256]；DETECT 在 kOrder 尾部（REDUCE_PCT 后）→ r2 刀再 >300 重刷
//     戳 → r3 刀仍减半（"每次大刀都盖住下回合"连续语义，记档）；
//   - d ③<300 回合末吸取（row1502 db5=3）：对手 atk300 刀 [87,102]<300 → ROUND_END
//     吸对手 max/3 = 300000 精确（对手 hp 差恰 -300000/-600000 两轮），印主压血后
//     净变化 = 回血300000 − 刀伤 ∈ [299898, 299913]（防 heal 封顶）；
//   - e row1705 互锚：① db0=2 → pl==2。
//   boss 无效子句：与 hamo_3809 同款 is_boss_challenge 手动门——非 boss 局路径与
//   boss 局仅差该布尔，K 系惯例不单独立 boss 场（记档）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物理攻击 filler（②触发用攻击技）
constexpr int kMarkId = 1864;
constexpr int kHolderMaxHp = 800000;

SoulMark make_mark(const std::vector<int>& db_args) {
    SoulMark sm(kMarkId, "K1864 魂印", "test",
                EffectArgs(std::vector<int>(db_args)));
    sm.monster_id = 4278;   // 阿特洛波斯
    return sm;
}

SoulMark make_mark735() {
    SoulMark sm(735, "K302 魂印", "test", EffectArgs(std::vector<int>{1000, 0}));
    sm.monster_id = 2;
    return sm;
}

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
}

ElfPet mark_pet(const std::vector<int>& db_args) {
    ElfPet p = make_pet(1, "印主", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kHolderMaxHp,
                        make_mark(db_args));
    accuracy_100(p);
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(1, "白板", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kHolderMaxHp);
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(int atk, int hp = kHolderMaxHp, bool with735 = false) {
    if (with735) {
        ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                            /*speed=*/100, atk, hp, make_mark735());
        accuracy_100(p);
        return p;
    }
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, hp);
    accuracy_100(p);
    return p;
}

// 对手攻击、印主 NONE；返回印主当刀掉血（③/②驱动）
int foe_strike(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int hp_before = ctx.getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp_before - ctx.getPet(0).hp;
}

// 双停过一回合（a 块推进用）
void pass_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// 在下一回合 FMR 停泊读 pl（须上一回合已走完）；读后把该回合走完
int probe_pl_next_fmr(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    ctx.debug_step_mode = false;
    b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
    const int pl = ctx.ws.preemptive_level[0];
    b.drive();
    return pl;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7340.a ①条件先制（row1502 db0=1）：无提升不触发；有提升当回合 +1 ══════
    {
        sbs_srand(20261024);
        ScenarioBattle b(make_party(mark_pet({1, 30, 300, 50, 300, 3, 1})),
                         make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int pl_r1 = probe_pl_next_fmr(b);   // r1：开局无提升
        CHECK_EQ(pl_r1, 0, "T7340.a r1 无提升不应先制（实测 " << pl_r1 << "）");
        ctx.ability_levels[0][2] = 2;   // r1 内手术抬防（r2 ROUND_START 快照可见）
        const int pl_r2 = probe_pl_next_fmr(b);   // r2：有提升 → +1
        CHECK_EQ(pl_r2, 1, "T7340.a 回合开始有提升 → 当回合先制应=1（实测 "
                  << pl_r2 << "）");
        std::cout << "  [T7340.a] 1864 ①条件先制+1（row1502）通过\n";
    }

    // ══ T7340.b ②粉伤窗（row1502 db1=30）：对手用攻击技当回合，粉伤 ×0.7 ══════
    // 对手挂 735（ROUND_END 对印主粉伤 1000 精确）+ atk1500（红伤走 ③> 支，
    // 不落 ③< 吸取支污染）。三带：无印 -[1433,1510] / ×0.7 -[1133,1210] /
    // r2 无使用足额 -[999,1001] 两两不相交。
    {
        sbs_srand(20261025);
        ScenarioBattle b(make_party(mark_pet({1, 30, 300, 50, 300, 3, 1})),
                         make_party(foe_pet(1500, kHolderMaxHp, /*with735=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int d1 = foe_strike(b);   // r1：对手攻击技立旗 → 735 粉伤 ×0.7
        CHECK(d1 >= 1133 && d1 <= 1210,
              "T7340.b row1502 用技当回合粉伤应 ×0.7（红[433,510]+700）：d1=" << d1);
        const int hp_before = ctx.getPet(0).hp;
        pass_round(b);                  // r2：对手 NONE → 无旗 → 粉伤足额（无红伤）
        const int d2 = hp_before - ctx.getPet(0).hp;
        CHECK(d2 >= 999 && d2 <= 1001,
              "T7340.b 无使用回合粉伤应足额 1000：d2=" << d2);
        std::cout << "  [T7340.b] 1864 ②粉伤窗 ×0.7/足额（row1502）通过\n";
    }
    {   // b 对照：无印世界同形 → 粉伤足额
        sbs_srand(20261026);
        ScenarioBattle b(make_party(plain_pet()),
                         make_party(foe_pet(1500, kHolderMaxHp, /*with735=*/true)));
        b.start();
        const int d1 = foe_strike(b);
        CHECK(d1 >= 1433 && d1 <= 1510,
              "T7340.b 对照无印应足额（红[433,510]+1000）：d1=" << d1);
        std::cout << "  [T7340.b] 1864 ②对照无印足额 通过\n";
    }
    {   // b 互锚：row1705 db1=50 → ×0.5
        sbs_srand(20261027);
        ScenarioBattle b(make_party(mark_pet({2, 50, 300, 50, 300, 3, 1})),
                         make_party(foe_pet(1500, kHolderMaxHp, /*with735=*/true)));
        b.start();
        const int d1 = foe_strike(b);
        CHECK(d1 >= 933 && d1 <= 1010,
              "T7340.b row1705 粉伤应 ×0.5（红[433,510]+500）：d1=" << d1);
        std::cout << "  [T7340.b] 1864 ②row1705 ×0.5 互锚 通过\n";
    }

    // ══ T7340.c ③>300 下回合受攻减半（row1502 db2=300/db3=50）═════════════════
    // DETECT 在 kOrder 尾部（REDUCE_PCT 后）→ 大刀每刀重刷戳：r2/r3 连续减半
    //（"每次大刀盖住下回合"，实现头注记档）。带：r1 [433,510]、r2/r3 ×0.5 [215,256]。
    {
        sbs_srand(20261028);
        ScenarioBattle b(make_party(mark_pet({1, 30, 300, 50, 300, 3, 1})),
                         make_party(foe_pet(1500)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int d1 = foe_strike(b);   // r1：>300 → 戳 r1（当刀不减）
        CHECK(d1 >= 430 && d1 <= 512,
              "T7340.c r1 刀应原价（[433,510]）：d1=" << d1);
        const int d2 = foe_strike(b);   // r2：上回合大刀 → 减半
        CHECK(d2 >= 215 && d2 <= 256,
              "T7340.c r2 刀应 ×0.5（[217,255]）：d2=" << d2);
        const int d3 = foe_strike(b);   // r3：r2 刀仍 >300 → 重刷戳 → 仍减半
        CHECK(d3 >= 215 && d3 <= 256,
              "T7340.c r3 大刀重刷戳应仍 ×0.5：d3=" << d3);
        std::cout << "  [T7340.c] 1864 ③>300 下回合减半（d1=" << d1 << " d2=" << d2
                  << " d3=" << d3 << "）通过\n";
    }

    // ══ T7340.d ③<300 回合末吸取（row1502 db5=3）：max/3 = 300000 精确 ═════════
    {
        sbs_srand(20261029);
        ScenarioBattle b(make_party(mark_pet({1, 30, 300, 50, 300, 3, 1})),
                         make_party(foe_pet(300, /*hp=*/900000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 500000;   // 压血防 heal 封顶（SOP §五.2）
        const int foe_hp0 = ctx.getPet(1).hp;
        const int holder_hp0 = ctx.getPet(0).hp;
        foe_strike(b);   // r1：刀 [87,102] < 300 → ROUND_END 吸 300000
        const int foe_d1 = foe_hp0 - ctx.getPet(1).hp;
        const int net1 = ctx.getPet(0).hp - holder_hp0;
        CHECK_EQ(foe_d1, 300000, "T7340.d 对手应被吸 300000（实测 " << foe_d1
                  << "）");
        CHECK(net1 >= 299898 && net1 <= 299913,
              "T7340.d 印主净变化应 = 回血300000 − 刀伤[87,102]（实测 " << net1
                  << "）");
        foe_strike(b);   // r2：再 <300 → 再吸（逐回合结算）
        const int foe_d2 = foe_hp0 - ctx.getPet(1).hp;
        CHECK_EQ(foe_d2, 600000, "T7340.d 两轮应累计吸 600000（实测 " << foe_d2
                  << "）");
        std::cout << "  [T7340.d] 1864 ③<300 吸取 max/3（" << foe_d1 << "/"
                  << foe_d2 << "）通过\n";
    }

    // ══ T7340.e row1705 互锚：① db0=2 → pl==2 ═════════════════════════════════
    {
        sbs_srand(20261030);
        ScenarioBattle b(make_party(mark_pet({2, 50, 300, 50, 300, 3, 1})),
                         make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int pl_r1 = probe_pl_next_fmr(b);
        CHECK_EQ(pl_r1, 0, "T7340.e r1 无提升不应先制（实测 " << pl_r1 << "）");
        ctx.ability_levels[0][2] = 2;
        const int pl_r2 = probe_pl_next_fmr(b);
        CHECK_EQ(pl_r2, 2, "T7340.e row1705 有提升 → 当回合先制应=2（实测 "
                  << pl_r2 << "）");
        std::cout << "  [T7340.e] 1864 ①row1705 先制+2 互锚 通过\n";
    }

    std::cout << "scenario_7340 PASS (K383 条件先制+承伤分流族)" << std::endl;
    return 0;
}
