// 场景 8320：K444 · 探索十批余量自选四族
// —— 1680「对手不处于异常状态时附加{0}点真实伤害」×5 +
//    996「自身体力高于1/{0}时，附加自身当前体力{1}%的百分比伤害」×5 +
//    825「若对手处于能力提升状态则附加对手最大体力1/{0}的百分比伤害」×5 +
//    1477「消除敌我双方能力提升状态，消除任意一方成功则吸取对手最大体力的1/{0}」×5
//（resources/moves_lib/k444_gate_append_pool.cpp，2026-11-09；号段 8320-8329 一文件
//  覆盖四族，scenario_8300 同式）
//
// 断言口径（详见插件头注）：四宏全落 SKILL_EFFECT 桶，断言三腿 = 门开/门闭/边界或
// 变体载体，量值全走审计槽精确断言（概率零裸跑——四族全部无概率子句）。
//   1680 通道 = has_any_active_abnormal_status 取反门（1339 判据直系）+ 真伤
//     deal_true_damage。门开腿真伤 200/250 审计精确（红伤随机只做方向断言）；
//     门闭腿挂失明20（控制类不自扣血——坑集 §5）审计 dealt=0；变体载体 35792/38433。
//   996 通道 = 高体门（严格 >，1447 同门）+ PERCENT_VALUE 百分比伤害
//     （hp×30% checked_pct_mul）。纯链载体 35988/36966 [2,30] 全同参：门开 90000
//     审计精确；恰 1/2 边界（严格不含等号）与低于阈值两腿门闭；变体载体 30840/19784
//     （19784 链 997 低体恢复门与 996 高体门互斥 → 自身高血时链尾惰性，一并锁）。
//   825 通道 = 对手六槽提升门 + PERCENT_VALUE（opp_max/3 = 200000 审计精确）。
//     纯链载体 36438；门闭两腿（净槽/只降不升——「提升」只数正槽）；变体 36623
//     （双提升种 slot1=2、2000 先制惰性）。
//   1477 通道 = clear_stat_boosts 双侧循环（1211 直系）+ 成功门 + kernel drain_hp
//     分数版真原语。双侧腿（self+2/opp+1 → 全清零 + 吸取 200000 精确 + 回血 200000
//     精确——半血观测防 clamp 坑集 §4/§9）；opp-only / self-only（任意一方成功）两腿；
//     双净腿（drain 0、self hp 零变化精确——drain 不发则回血也无）；变体 37998
//     （2192 未注册静默跳过、718 窗注册不动血，审计自洽）。
#include <algorithm>
#include <any>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa  = 10004;  // 拍打：物40 无效果填充刀（crit_rate=0）
constexpr int k34983  = 34983;  // 彼岸挽尘花 [881,1221,1048,1680]|[1,50,200]：特150
constexpr int k35792  = 35792;  // 凡尘弥天蚀 [1056,1048,1680]|[2,100,250]：特150
constexpr int k38433  = 38433;  // 创·黄金纪元 [1956,2034,1680,1163]|[…,200,…]：特160
constexpr int k35988  = 35988;  // 末路流沙 [996]|[2,30]：物140（996 纯链）
constexpr int k36966  = 36966;  // 妙偶咒言 [996]|[2,30]：特130（996 纯链）
constexpr int k30840  = 30840;  // 时空枪击 [928,996]|[1|2,30]：特85
constexpr int k19784  = 19784;  // 光宇天轮诀 [996,997]|[2,30|2,4]：特150
constexpr int k37483  = 37483;  // 生死盘灵 [1080,996,997,1933]|[…|2,30|…]：物150
constexpr int k36438  = 36438;  // 雷牙之撕 [825]|[3]：特140（825 纯链）
constexpr int k36623  = 36623;  // 刺骨凛 [2000,494,825]|[3]：特130
constexpr int k35135  = 35135;  // 黑暗扭转 [1477]|[3]：物140（1477 纯链）
constexpr int k36849  = 36849;  // 根源调和 [1477]|[3]：特120（1477 纯链）
constexpr int k37998  = 37998;  // 愈色森罗 [2192,1477,718]|[3,3,2]：特160

constexpr int kAudit1680 = 1016800000;
constexpr int kAudit996  = 1009960000;
constexpr int kAudit825  = 1008250000;
constexpr int kAudit1477 = 1014770000;

int audit_int(BattleContext& ctx, int base, int owner, int slot, int def = -99) {
    const auto it = ctx.plugin_storage.find(base + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a1680(BattleContext& ctx, int o, int s)  { return audit_int(ctx, kAudit1680, o, s); }
int a996(BattleContext& ctx, int o, int s)   { return audit_int(ctx, kAudit996, o, s); }
int a825(BattleContext& ctx, int o, int s)   { return audit_int(ctx, kAudit825, o, s); }
int a1477(BattleContext& ctx, int o, int s)  { return audit_int(ctx, kAudit1477, o, s); }

ElfPet caster(std::array<int, 5> skills, int speed = 300, int hp = 400000, int atk = 1) {
    ElfPet p = scenario::make_pet(0, "施加方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}
ElfPet foe(std::array<int, 5> skills, int speed = 100, int hp = 600000) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, /*atk=*/1, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 一回合：我方出 index 技能，对手不动（NONE）——四族全为"我方施放"面，对手不出刀
// 防污染血差断言。
void drive_round(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}
void run_cast(ScenarioBattle& b, int skill_index, unsigned seed) {
    sbs_srand(seed);
    drive_round(b, skill_index);
}

int level(BattleContext& ctx, int side, int slot) { return ctx.ability_levels[side][slot]; }

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══════════ 1680：对手不处于异常状态时附加{0}点真实伤害 ══════════

    // T1 门开腿（34983 [881,1221,1048,1680][1,50,200]）：自身净槽（881/1221 惰性）+
    //   对手净异常（1048 惰性）→ 真伤恰 200 审计精确；真伤不回血 → 自身血零变化。
    {
        ScenarioBattle b(scenario::make_party(caster({k34983, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int self0 = ctx.getPet(0).hp;
        const int foe0 = ctx.getPet(1).hp;
        run_cast(b, 0, 20261201);
        CHECK_EQ(a1680(ctx, 0, 0), 200, "T1 行参 n=200");
        CHECK_EQ(a1680(ctx, 0, 1), 1, "T1 门判定=开");
        CHECK_EQ(a1680(ctx, 0, 2), 0, "T1 对手异常实测=0");
        CHECK_EQ(a1680(ctx, 0, 3), 200, "T1 核心：真伤实参=200");
        CHECK_EQ(a1680(ctx, 0, 4), 200, "T1 核心：对手实扣=200（真伤全扣）");
        CHECK_EQ(a1680(ctx, 0, 8), 1, "T1 使用计数=1");
        CHECK_EQ(a1680(ctx, 0, 9), 0, "T1 无跳过");
        CHECK_EQ(ctx.getPet(0).hp, self0, "T1 真伤无回血半边 → 自身血零变化");
        CHECK(ctx.getPet(1).hp < foe0, "T1 对手掉血（红伤+真伤）");
        std::cout << "  [T1] 1680 门开腿（真伤 200 精确）通过\n";
    }

    // T2 门闭腿（34983，对手挂失明20 九回合——控制类不自扣血，坑集 §5）：门闭 →
    //   真伤 0（审计 dealt=0）；对手仍掉血（红伤 + 链 1048 对手异常增伤，只方向）。
    {
        ScenarioBattle b(scenario::make_party(caster({k34983, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(apply_anomaly(&ctx, 1, /*失明=*/20, /*duration_rounds=*/9, /*actor=*/0)
                == ApplyAnomalyResult::SUCCESS);
        const int self0 = ctx.getPet(0).hp;
        const int foe0 = ctx.getPet(1).hp;
        run_cast(b, 0, 20261201);
        CHECK_EQ(a1680(ctx, 0, 1), 0, "T2 核心：门判定=闭（对手带异常）");
        CHECK_EQ(a1680(ctx, 0, 2), 1, "T2 对手异常实测=1");
        CHECK_EQ(a1680(ctx, 0, 3), 0, "T2 真伤实参=0");
        CHECK_EQ(a1680(ctx, 0, 4), 0, "T2 核心：对手实扣=0（无真伤）");
        CHECK_EQ(ctx.getPet(0).hp, self0, "T2 自身血零变化");
        CHECK(ctx.getPet(1).hp < foe0, "T2 对手仍掉红伤（门闭只关真伤不关红伤）");
        std::cout << "  [T2] 1680 门闭腿通过\n";
    }

    // T3 变体载体腿（35792 [1056,1048,1680][2,100,250]）：自身净槽 1056 反转惰性 →
    //   真伤恰 250（行参逐载体消费）。
    {
        ScenarioBattle b(scenario::make_party(caster({k35792, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261202);
        CHECK_EQ(a1680(ctx, 0, 0), 250, "T3 行参 n=250");
        CHECK_EQ(a1680(ctx, 0, 3), 250, "T3 核心：真伤实参=250");
        CHECK_EQ(a1680(ctx, 0, 4), 250, "T3 对手实扣=250");
        CHECK_EQ(a1680(ctx, 0, 9), 0, "T3 无跳过");
        std::cout << "  [T3] 1680 变体载体腿（35792/250）通过\n";
    }

    // T4 变体载体腿（38433 [1956,2034,1680,1163][…,200,20,10,40]）：2034 消除未触发
    //   分支只挂自身减伤窗、1163 递增百分比（atk=1 数值和 2 → 20% 取整 0 无落点）——
    //   审计 1680 真伤恰 200 自洽。
    {
        ScenarioBattle b(scenario::make_party(caster({k38433, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261202);
        CHECK_EQ(a1680(ctx, 0, 0), 200, "T4 行参 n=200（链尾切片自洽）");
        CHECK_EQ(a1680(ctx, 0, 4), 200, "T4 核心：对手实扣=200（混链共存审计精确）");
        std::cout << "  [T4] 1680 变体载体腿（38433 混链）通过\n";
    }

    // ══════════ 996：自身体力高于1/{0}时，附加自身当前体力{1}%的百分比伤害 ══════════

    // T5 门开腿（35988 [996][2,30] 纯链物140）：300000 > 200000 → 90000 审计精确；
    //   百分比伤害不吃封回血也无回血半边 → 自身血零变化。
    {
        ScenarioBattle b(scenario::make_party(caster({k35988, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300000;   // 压当前体力（最大 400000 不变）
        const int self0 = ctx.getPet(0).hp;
        const int foe0 = ctx.getPet(1).hp;
        run_cast(b, 0, 20261203);
        CHECK_EQ(a996(ctx, 0, 0), 2, "T5 行参 denom=2");
        CHECK_EQ(a996(ctx, 0, 1), 30, "T5 行参 pct=30");
        CHECK_EQ(a996(ctx, 0, 2), 1, "T5 门判定=开");
        CHECK_EQ(a996(ctx, 0, 3), 200000, "T5 阈值=max/2=200000");
        CHECK_EQ(a996(ctx, 0, 4), 300000, "T5 实测自身 hp=300000");
        CHECK_EQ(a996(ctx, 0, 5), 90000, "T5 核心：百分比伤害实参=300000×30%=90000");
        CHECK_EQ(a996(ctx, 0, 6), 90000, "T5 核心：对手实扣=90000（满血全扣）");
        CHECK_EQ(a996(ctx, 0, 8), 1, "T5 使用计数=1");
        CHECK_EQ(a996(ctx, 0, 9), 0, "T5 无跳过");
        CHECK_EQ(ctx.getPet(0).hp, self0, "T5 附加伤害无自伤/回血 → 自身血零变化");
        CHECK(ctx.getPet(1).hp < foe0, "T5 对手掉血（红伤+百分比）");
        std::cout << "  [T5] 996 门开腿（90000 精确）通过\n";
    }

    // T6 边界腿：自身恰 = max/2（200000）→「高于」不含等号 → 门闭不附加
    {
        ScenarioBattle b(scenario::make_party(caster({k35988, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 200000;
        const int foe0 = ctx.getPet(1).hp;
        run_cast(b, 0, 20261203);
        CHECK_EQ(a996(ctx, 0, 2), 0, "T6 核心：恰 1/2 不触发（严格高于，1447 同门）");
        CHECK_EQ(a996(ctx, 0, 3), 200000, "T6 阈值=200000");
        CHECK_EQ(a996(ctx, 0, 4), 200000, "T6 实测自身 hp=200000");
        CHECK_EQ(a996(ctx, 0, 5), 0, "T6 伤害实参=0");
        CHECK_EQ(a996(ctx, 0, 6), 0, "T6 对手实扣=0（无附加）");
        CHECK(ctx.getPet(1).hp < foe0, "T6 对手仍掉红伤（只关附加）");
        std::cout << "  [T6] 996 边界腿（恰等不取）通过\n";
    }

    // T7 低于阈值腿（199999 < 200000）：门闭
    {
        ScenarioBattle b(scenario::make_party(caster({k35988, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 199999;
        run_cast(b, 0, 20261203);
        CHECK_EQ(a996(ctx, 0, 2), 0, "T7 核心：低于阈值 → 门闭");
        CHECK_EQ(a996(ctx, 0, 6), 0, "T7 对手实扣=0");
        std::cout << "  [T7] 996 低于阈值腿通过\n";
    }

    // T8 变体载体腿（36966 [996] 纯链特130）：同参 [2,30] → 90000 再证
    {
        ScenarioBattle b(scenario::make_party(caster({k36966, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300000;
        run_cast(b, 0, 20261204);
        CHECK_EQ(a996(ctx, 0, 5), 90000, "T8 核心：纯链特攻载体同参 90000");
        CHECK_EQ(a996(ctx, 0, 9), 0, "T8 无跳过");
        std::cout << "  [T8] 996 变体载体腿（36966）通过\n";
    }

    // T9 变体载体腿（19784 [996,997][2,30|2,4]）：自身 300000 高血 → 997 低体恢复门
    //   （需 hp < max/2）惰性 → 只有 996 的 90000；链尾切片咬合。
    {
        ScenarioBattle b(scenario::make_party(caster({k19784, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300000;
        run_cast(b, 0, 20261204);
        CHECK_EQ(a996(ctx, 0, 5), 90000, "T9 核心：996[2,30] → 90000（链 997 惰性不污染）");
        CHECK_EQ(a996(ctx, 0, 2), 1, "T9 门判定=开");
        std::cout << "  [T9] 996 变体载体腿（19784 双门互斥咬合）通过\n";
    }

    // T10 变体载体腿（30840 [928,996][1|2,30]）：928 对手无回合类效果惰性 → 90000
    {
        ScenarioBattle b(scenario::make_party(caster({k30840, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300000;
        run_cast(b, 0, 20261205);
        CHECK_EQ(a996(ctx, 0, 5), 90000, "T10 核心：928 链共存下 90000 审计精确");
        std::cout << "  [T10] 996 变体载体腿（30840）通过\n";
    }

    // T11 变体载体腿（37483 [1080,996,997,1933]）：四链共存 → 996 的 90000 自洽
    {
        ScenarioBattle b(scenario::make_party(caster({k37483, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300000;
        run_cast(b, 0, 20261205);
        CHECK_EQ(a996(ctx, 0, 5), 90000, "T11 核心：四链载体 996 仍 90000 审计精确");
        std::cout << "  [T11] 996 变体载体腿（37483）通过\n";
    }

    // ══════════ 825：若对手处于能力提升状态则附加对手最大体力1/{0}的百分比伤害 ══════════

    // T12 门开腿（36438 [825][3] 纯链特140）：对手 +1 → 伤害恰 600000/3 = 200000
    {
        ScenarioBattle b(scenario::make_party(caster({k36438, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, 0, 1) == StatChangeResult::SUCCESS);  // 对手攻击 +1
        const int foe0 = ctx.getPet(1).hp;
        run_cast(b, 0, 20261206);
        CHECK_EQ(a825(ctx, 0, 0), 3, "T12 行参 denom=3");
        CHECK_EQ(a825(ctx, 0, 1), 1, "T12 对手提升种数=1");
        CHECK_EQ(a825(ctx, 0, 2), 1, "T12 门判定=开");
        CHECK_EQ(a825(ctx, 0, 3), 200000, "T12 核心：伤害实参=600000/3=200000");
        CHECK_EQ(a825(ctx, 0, 4), 200000, "T12 核心：对手实扣=200000（满血全扣）");
        CHECK_EQ(a825(ctx, 0, 8), 1, "T12 使用计数=1");
        CHECK_EQ(a825(ctx, 0, 9), 0, "T12 无跳过");
        CHECK(ctx.getPet(1).hp < foe0, "T12 对手掉血（红伤+百分比）");
        std::cout << "  [T12] 825 门开腿（200000 精确）通过\n";
    }

    // T13 门闭·净槽腿：对手无提升 → dealt=0（审计槽 1=0 证明确实数过）
    {
        ScenarioBattle b(scenario::make_party(caster({k36438, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261206);
        CHECK_EQ(a825(ctx, 0, 1), 0, "T13 对手提升种数=0");
        CHECK_EQ(a825(ctx, 0, 2), 0, "T13 核心：门判定=闭");
        CHECK_EQ(a825(ctx, 0, 4), 0, "T13 核心：对手实扣=0（无附加）");
        std::cout << "  [T13] 825 门闭·净槽腿通过\n";
    }

    // T14 门闭·只降不升腿（「提升」只数正槽——§七点五 门判据②型）：对手 -1 → 闭
    {
        ScenarioBattle b(scenario::make_party(caster({k36438, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, 2, -1) == StatChangeResult::SUCCESS);  // 对手防御 -1
        run_cast(b, 0, 20261206);
        CHECK_EQ(a825(ctx, 0, 1), 0, "T14 核心：负槽不计入提升种数");
        CHECK_EQ(a825(ctx, 0, 2), 0, "T14 门判定=闭");
        CHECK_EQ(a825(ctx, 0, 4), 0, "T14 对手实扣=0");
        std::cout << "  [T14] 825 门闭·只降不升腿通过\n";
    }

    // T15 变体载体腿（36623 [2000,494,825][3]）：对手 +2/+1 双种 → slot1=2、200000
    //   精确（2000 对手有提升 → 先制+1 必中惰性不污染；494 无视提升只作用红伤）
    {
        ScenarioBattle b(scenario::make_party(caster({k36623, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, 0, 2) == StatChangeResult::SUCCESS);  // 对手攻击 +2
        REQUIRE(stat_change(&ctx, 1, 2, 1) == StatChangeResult::SUCCESS);  // 对手防御 +1
        run_cast(b, 0, 20261207);
        CHECK_EQ(a825(ctx, 0, 1), 2, "T15 对手提升种数=2（计种非计级）");
        CHECK_EQ(a825(ctx, 0, 3), 200000, "T15 核心：伤害实参=200000（与种数无关）");
        CHECK_EQ(a825(ctx, 0, 4), 200000, "T15 对手实扣=200000");
        std::cout << "  [T15] 825 变体载体腿（36623 双种）通过\n";
    }

    // ══════════ 1477：消除敌我双方能力提升状态，消除任意一方成功则吸取对手最大体力的1/{0} ══════════

    // T16 双侧腿（35135 [1477][3] 纯链物140）：自身 +2/-1、对手 +1 → 全部清零（负槽
    //   不动），吸取恰 200000、回血恰 200000（半血观测——坑集 §4/§9）。
    {
        ScenarioBattle b(scenario::make_party(caster({k35135, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 0, 2) == StatChangeResult::SUCCESS);   // 自身攻击 +2
        REQUIRE(stat_change(&ctx, 0, 2, -1) == StatChangeResult::SUCCESS);  // 自身防御 -1（负槽不动）
        REQUIRE(stat_change(&ctx, 1, 1, 1) == StatChangeResult::SUCCESS);   // 对手特攻 +1
        ctx.getPet(0).hp = 1000;   // 压当前体力防回血 clamp
        const int foe0 = ctx.getPet(1).hp;
        run_cast(b, 0, 20261208);
        CHECK_EQ(a1477(ctx, 0, 0), 3, "T16 行参 denom=3");
        CHECK_EQ(a1477(ctx, 0, 1), 1, "T16 自身清除项数=1（+2 一项；负槽不清）");
        CHECK_EQ(a1477(ctx, 0, 2), 1, "T16 对手清除项数=1");
        CHECK_EQ(a1477(ctx, 0, 3), 1, "T16 成功门=任一成功");
        CHECK_EQ(a1477(ctx, 0, 4), 200000, "T16 核心：对手实扣=600000/3=200000");
        CHECK_EQ(a1477(ctx, 0, 5), 200000, "T16 核心：回血实绩=200000（全额字面）");
        CHECK_EQ(a1477(ctx, 0, 6), 0, "T16 drain 返回=SUCCESS");
        CHECK_EQ(level(ctx, 0, 0), 0, "T16 自身攻击槽清零");
        CHECK_EQ(level(ctx, 0, 2), -1, "T16 自身防御负槽保留（只清提升）");
        CHECK_EQ(level(ctx, 1, 1), 0, "T16 对手特攻槽清零");
        CHECK_EQ(ctx.getPet(0).hp, 201000, "T16 核心：自身 1000+200000=201000");
        CHECK(ctx.getPet(1).hp < foe0, "T16 对手掉血（红伤+吸取）");
        std::cout << "  [T16] 1477 双侧腿（全清 + 200000 吸取/回血）通过\n";
    }

    // T17 对手侧单边腿：自身净槽、对手 +1 → 任一成功 → 吸取照发；自身槽不受影响
    {
        ScenarioBattle b(scenario::make_party(caster({k35135, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, 0, 1) == StatChangeResult::SUCCESS);  // 对手攻击 +1
        ctx.getPet(0).hp = 1000;
        run_cast(b, 0, 20261208);
        CHECK_EQ(a1477(ctx, 0, 1), 0, "T17 自身清除项数=0");
        CHECK_EQ(a1477(ctx, 0, 2), 1, "T17 对手清除项数=1");
        CHECK_EQ(a1477(ctx, 0, 3), 1, "T17 核心：对手侧单边成功即开门");
        CHECK_EQ(a1477(ctx, 0, 4), 200000, "T17 对手实扣=200000");
        CHECK_EQ(ctx.getPet(0).hp, 201000, "T17 回血=200000");
        std::cout << "  [T17] 1477 对手侧单边腿通过\n";
    }

    // T18 自身侧单边腿：自身 +1、对手净槽 →「消除任意一方成功」含自身 → 吸取照发
    {
        ScenarioBattle b(scenario::make_party(caster({k35135, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 0, 1) == StatChangeResult::SUCCESS);  // 自身攻击 +1
        ctx.getPet(0).hp = 1000;
        run_cast(b, 0, 20261208);
        CHECK_EQ(a1477(ctx, 0, 1), 1, "T18 自身清除项数=1");
        CHECK_EQ(a1477(ctx, 0, 2), 0, "T18 对手清除项数=0");
        CHECK_EQ(a1477(ctx, 0, 3), 1, "T18 核心：自身侧单边成功即开门（任意一方字面）");
        CHECK_EQ(ctx.getPet(0).hp, 201000, "T18 回血=200000");
        std::cout << "  [T18] 1477 自身侧单边腿通过\n";
    }

    // T19 双净腿：双方无提升 → 清除 0/0、无吸取（drain 不发则回血也无）——
    //   自身血零变化精确（红伤只打对手）。
    {
        ScenarioBattle b(scenario::make_party(caster({k35135, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int self0 = ctx.getPet(0).hp;
        run_cast(b, 0, 20261208);
        CHECK_EQ(a1477(ctx, 0, 1), 0, "T19 自身清除=0");
        CHECK_EQ(a1477(ctx, 0, 2), 0, "T19 对手清除=0");
        CHECK_EQ(a1477(ctx, 0, 3), 0, "T19 核心：成功门=双空");
        CHECK_EQ(a1477(ctx, 0, 4), 0, "T19 对手实扣=0（无吸取）");
        CHECK_EQ(a1477(ctx, 0, 5), 0, "T19 回血=0");
        CHECK_EQ(a1477(ctx, 0, 9), 3, "T19 跳过原因=3（无成功消除）");
        CHECK_EQ(ctx.getPet(0).hp, self0, "T19 核心：自身血零变化（吸取不发生）");
        std::cout << "  [T19] 1477 双净腿通过\n";
    }

    // T20 变体载体腿（36849 [1477][3] 纯链特120）：对手 +1 → 200000 审计精确
    {
        ScenarioBattle b(scenario::make_party(caster({k36849, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, 3, 1) == StatChangeResult::SUCCESS);  // 对手特防 +1（槽3）
        CHECK_EQ(level(ctx, 1, 3), 1, "T20 前置：对手特防槽已 +1（施放前）");
        ctx.getPet(0).hp = 1000;
        run_cast(b, 0, 20261209);
        CHECK_EQ(a1477(ctx, 0, 2), 1, "T20 对手清除=1");
        CHECK_EQ(a1477(ctx, 0, 4), 200000, "T20 核心：变体载体吸取 200000 精确");
        CHECK_EQ(a1477(ctx, 0, 5), 200000, "T20 回血=200000");
        std::cout << "  [T20] 1477 变体载体腿（36849）通过\n";
    }

    // T21 变体载体腿（37998 [2192,1477,718][3,3,2]）：2192 未注册静默跳过、718 窗
    //   注册不动血 → 1477 的 [3] 切片审计自洽（对手 +1 → 200000）。
    {
        ScenarioBattle b(scenario::make_party(caster({k37998, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, 4, 1) == StatChangeResult::SUCCESS);  // 对手速度 +1
        ctx.getPet(0).hp = 1000;
        run_cast(b, 0, 20261209);
        CHECK_EQ(a1477(ctx, 0, 0), 3, "T21 行参 denom=3（链中切片自洽）");
        CHECK_EQ(a1477(ctx, 0, 4), 200000, "T21 核心：混链共存吸取 200000 精确");
        CHECK_EQ(a1477(ctx, 0, 9), 0, "T21 无跳过");
        std::cout << "  [T21] 1477 变体载体腿（37998 混链）通过\n";
    }

    std::cout << "scenario_8320_k444_gate_append_pool: ALL PASS\n";
    return 0;
}
