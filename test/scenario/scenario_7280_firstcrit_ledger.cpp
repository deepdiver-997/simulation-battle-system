#include <utils/sbs_rand.h>
// 场景 7280 —— K381 首攻必爆+消强消回合+爆次对账族
//（2518，soul_lib/k2518_firstcrit_ledger.cpp；号段 7280，2026-10-24 worktree ../agents/k381）
//
// 断言口径（暴击判定走内核 query_usage ①.5；crit_rate 面板可写 → 对手爆次精确驱动；
//   a24-kprod 勘误 2026-10-24：对手爆次必须让对手真出手——foe_strike/both_strike，
//   前手用 holder_strike（对手 NONE）等"对手拉平"系驱动错位，结算恒 my>opp 每轮重刷）：
//   - a 首攻必爆（row2095）：r1 首刀必爆（爆伤带 [174,204] = raw[87,102]×2），
//     r2 起常规刀（[87,102]）——s1≥160 且 s2≤110 且 s1>1.5×s2；
//   - b 打出前消强（row2095）：r1 印主用属性技（不爆→first_crit_done 仍 false）→
//     ROUND_END 消除对手 +2 全强化（清到 0）；
//   - c 爆次高于 → 焚烬（row2095，末次申报覆盖 1 整轮）：r1 首刀爆施加；r2 双停
//     （对手被焚烬禁行——控场类）my>opp 持续重刷 end=3；手术解禁后 r3 对手爆刀
//     拉平（等于支停止重刷）→ rc=3 到期；
//   - f row2107 互锚：同形 → 末次申报覆盖 2 整轮（r2 重刷 end=4；拉平后 rc=3
//     仍活跃——对照 .c 已到期；r4 反超回血支后 rc=4 到期）；
//   - d 爆次低于 → 恢复最大体力差值：印主属性技开局（my=0），快速对手必爆刀
//     （opp=1）→ r1 ROUND_END 回 |max差|=100000（压血防封顶）→ 净变化
//     [96600,97100]（无回血世界 [−3400,−2900] 不相交）；
//   - e 爆次相等 → 下回合先制+1（row2095）：r1 双方对爆（1=1）→ r2 FMR
//     停泊读 preemptive_level==1。
//   ⚠️ 对手爆次用 skills.critical_strike_rate=1.0 逐回合精确驱动（ws.must_crit
//   每回合 reset 不可跨回合写）。回合口径：round N 全程 roundCount=N-1，
//   引擎 end=rc+rounds、活跃 rc<end（battle_mutate.cpp:92）。
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

constexpr int kPaiDa = 10004;   // 拍打：物理攻击 filler
constexpr int kSuoTou = 20001;  // 缩头：属性技 filler（b 块用）
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);  // 22
constexpr int kMaxHp = 800000;
constexpr int kSlots = BattleContext::kAbilityLevelSlotCount;
constexpr int kPreemptFlagKey = 990381500;   // +owner

SoulMark make_mark(const std::vector<int>& db_args) {
    SoulMark sm(/*effect_id=*/2518, "K2518 魂印", "test",
                EffectArgs(std::vector<int>(db_args)));
    sm.monster_id = 4908;
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
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark(db_args));
    accuracy_100(p);
    return p;
}

ElfPet mark_pet_attr(const std::vector<int>& db_args) {   // 格 5 带缩头
    ElfPet p = make_pet(1, "印主", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kSuoTou},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark(db_args));
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(int atk, int hp = kMaxHp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, hp);
    accuracy_100(p);
    return p;
}

void foe_always_crit(BattleContext& ctx, bool on) {   // 对手爆次精确驱动
    for (Skills& s : ctx.getPet(1).skills) {
        s.critical_strike_rate = on ? 100.0f : 0.0f;   // 单位=百分点（rate>=100 必爆）
    }
}

// 印主攻击、对手 NONE；返回当刀伤害（对对手）
int holder_strike(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int hp_before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp_before - ctx.getPet(1).hp;
}

// 对手攻击、印主 NONE（对手爆次驱动的正解——holder_strike 里对手 NONE 永不出爆，
// a24-kprod 勘误：前手 .c/.f/.e 用 holder_strike 等"对手拉平"系驱动错位）
int foe_strike(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int hp_before = ctx.getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp_before - ctx.getPet(0).hp;
}

// 双方都攻击（e 块"同回合对爆"用）
void both_strike(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

bool all_zero(BattleContext& ctx, int side) {
    for (int i = 0; i < kSlots; ++i) {
        if (ctx.ability_levels[side][i] != 0) {
            return false;
        }
    }
    return true;
}


int probe_mustcrit() {   // [probe] FMR 停泊读 must_crit
    ScenarioBattle b(make_party(mark_pet({1, 0})), make_party(foe_pet(1)));
    BattleContext& ctx = b.ctx();
    sbs_srand(7);
    b.start();
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    ctx.debug_step_mode = false;
    b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
    const int mc = ctx.ws.must_crit[0] ? 1 : 0;
    const int pre = ctx.ws.preemptive_level[0];
    b.drive();
    return mc * 10 + pre;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    { const int v = probe_mustcrit(); std::cerr << "PROBEVAL mc10pre=" << v << std::endl; }

    // ══ T7280.a 首攻必爆（row2095 [1,0]）═════════════════════════════════════
    {
        sbs_srand(20261024);
        ScenarioBattle b(make_party(mark_pet({1, 0})), make_party(foe_pet(1)));
        b.start();
        const int s1 = holder_strike(b);
        const int s2 = holder_strike(b);
        CHECK(s1 >= 160 && s1 <= 220,
              "T7280.a 首刀应必爆（爆伤带 [174,204]）：s1=" << s1);
        CHECK(s2 >= 85 && s2 <= 110,
              "T7280.a 次刀应常规（raw 带 [87,102]）：s2=" << s2);
        CHECK(s1 > 1.5 * s2, "T7280.a 爆/常比例应≈2：s1=" << s1 << " s2=" << s2);
        std::cout << "  [T7280.a] 2518 首攻必爆（s1=" << s1 << " s2=" << s2
                  << "）通过\n";
    }

    // ══ T7280.b 打出前消强（row2095）：属性技回合消对手 +2 强化 ══════════════
    {
        sbs_srand(20261025);
        ScenarioBattle b(make_party(mark_pet_attr({1, 0})), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int i = 0; i < kSlots; ++i) {
            ctx.ability_levels[1][i] = 2;   // 对手 +2 全强化
        }
        seal_skill(&ctx, /*source=*/1, /*target=*/1, /*effect_id=*/70001,
                   /*attribute=*/true, /*attack=*/true, /*count=*/1,
                   /*duration_rounds=*/2, /*penetrable=*/false);   // 对手名下可断物
        // r1：印主用缩头（属性技 idx4，不爆）→ ROUND_END 消强成功 → 消回合类
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 4);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(all_zero(ctx, 1), "T7280.b 首爆前 ROUND_END 应消除对手强化（实测 "
                  + std::to_string(ctx.ability_levels[1][0]) + "...）");
        std::cout << "  [T7280.b] 2518 打出前消强 通过\n";
    }

    // ══ T7280.c 爆次高于 → 焚烬，最后一次领先申报覆盖 1 整轮（row2095）═════════
    // 驱动（a24-kprod 勘误二）：焚烬=控场类（abnormal-types.h Control），对手被烧
    //   期间无法行动 → "对手出手拉平"在烧着时不可达。正解：①r1 首刀爆（my=1>0）
    //   施加；②r2 双停（对手被禁行）→ my>opp 持续 → 结算重刷（end 推进）；③手术
    //   清焚烬解禁对手 → r3 对手必爆拉平（等于支，停止重刷）→ 末次申报（r2）的
    //   覆盖整轮数在 r3 末见分晓：db=0 → end=1+2=3，rc=3 到期。
    {
        sbs_srand(20261026);
        ScenarioBattle b(make_party(mark_pet({1, 0})), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        holder_strike(b);   // r1：首刀爆 → my=1>0 → 焚烬 end=0+2（rc=1 活跃）
        CHECK(ctx.has_active_abnormal_status(1, kIncinerate),
              "T7280.c 爆次高于应对手焚烬（row2095）");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);   // r2：双停
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int end_r2 = ctx.abnormal_status_end_round[1][kIncinerate];
        CHECK(end_r2 == 3, "T7280.c my>opp 应重刷 end=1+2=3（实测 " << end_r2 << "）");
        CHECK(ctx.has_active_abnormal_status(1, kIncinerate),
              "T7280.c 重刷后 r3（rc=2）应仍活跃");
        ctx.set_abnormal_status_end_round(1, kIncinerate, 0);   // 手术解禁对手
        foe_always_crit(ctx, true);
        foe_strike(b);   // r3：对手必爆 → opp=1=my → 等于支（停止重刷）
        const int end_r3 = ctx.abnormal_status_end_round[1][kIncinerate];
        CHECK(!ctx.has_active_abnormal_status(1, kIncinerate) && end_r3 == 0,
              "T7280.c 等于/低于支不得重刷复活（实测 end=" << end_r3 << "）");
        std::cout << "  [T7280.c] 2518 爆次高于→焚烬(末次申报1整轮) 通过\n";
    }

    // ══ T7280.f row2107 [1,1] 互锚：末次申报覆盖 2 整轮 ══════════════════════
    // 同 .c 驱动；时长差 = r2 重刷 end 4（db=1→+3）对照 .c 的 3（db=0→+2），
    //   手术后等于/低于支均不得复活。
    {
        sbs_srand(20261027);
        ScenarioBattle b(make_party(mark_pet({1, 1})), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        holder_strike(b);   // r1：首刀爆 → 焚烬 end=0+3
        CHECK(ctx.has_active_abnormal_status(1, kIncinerate),
              "T7280.f 爆次高于应对手焚烬");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);   // r2：双停
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int end_r2 = ctx.abnormal_status_end_round[1][kIncinerate];
        CHECK(end_r2 == 4, "T7280.f my>opp 应重刷 end=1+3=4（实测 " << end_r2 << "）");
        ctx.set_abnormal_status_end_round(1, kIncinerate, 0);   // 手术解禁对手
        foe_always_crit(ctx, true);
        foe_strike(b);   // r3：对手必爆拉平（等于支，停止重刷）
        const int end_r3 = ctx.abnormal_status_end_round[1][kIncinerate];
        CHECK(end_r3 == 0,
              "T7280.f 等于支不得重刷复活（实测 end=" << end_r3 << "）");
        foe_strike(b);   // r4：my<opp → 回血支（max 相等 heal 0），不重刷
        CHECK(!ctx.has_active_abnormal_status(1, kIncinerate),
              "T7280.f row2107 焚烬不得复活");
        std::cout << "  [T7280.f] 2518 row2107 焚烬2整轮 互锚 通过\n";
    }

    // ══ T7280.d 爆次低于 → 恢复最大体力差值（差 100000）═════════════════════
    // 驱动（a24-kprod 勘误二）：印主 r1 用属性技缩头（不爆、my=0），对手（speed
    //   400 先手，避免缩头先手抬防压低刀伤带）必爆刀 → opp=1>my=0 → ROUND_END
    //   直接落"低于"支：回 |max差|=100000。印主先压到 500000 防 heal 封顶
    //   （SOP §五.2）：净变化 = 回血100000 − 爆刀[2900,3400] ∈ [96600,97100]
    //   （与无回血世界 [−3400,−2900] 不相交）；my 从未高于 opp → 对手不得被焚烬。
    {
        sbs_srand(20261029);
        ElfPet foe_fast = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                   /*speed=*/400, /*atk=*/5000, /*hp=*/900000);
        accuracy_100(foe_fast);
        ScenarioBattle b(make_party(mark_pet_attr({1, 0})), make_party(foe_fast));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 500000;
        foe_always_crit(ctx, true);
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 4);   // 缩头
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, kIncinerate),
              "T7280.d 爆次未高于对手不得被焚烬");
        const int net = ctx.getPet(0).hp - hp_before;
        CHECK(net >= 96600 && net <= 97100,
              "T7280.d 回血差值带失配：hp_before=" << hp_before << " hp_after="
                  << ctx.getPet(0).hp << " net=" << net);
        std::cout << "  [T7280.d] 2518 爆次低于→回血 100000（净 " << net
                  << "）通过\n";
    }

    // ══ T7280.e 爆次相等 → 下回合先制+1（row2095）════════════════════════════
    {
        sbs_srand(20261030);
        ScenarioBattle b(make_party(mark_pet({1, 0})), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        foe_always_crit(ctx, true);
        both_strike(b);   // r1：印主爆（my=1）+ 对手同回合爆（opp=1）→ 等于旗
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        const int pre = ctx.ws.preemptive_level[0];
        b.drive();
        CHECK_EQ(pre, 1, "T7280.e 爆次相等 → r2 FMR 先制应=1（实测 " << pre
                  << "）");
        std::cout << "  [T7280.e] 2518 爆次相等→先制+1 通过\n";
    }

    std::cout << "scenario_7280 PASS (K381 首攻必爆+消强消回合+爆次对账族)"
              << std::endl;
    return 0;
}
