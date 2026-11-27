// 场景 660 —— F6 受击响应族（817 / 897 / 850 / 68，moves_lib/f_onhit_family.cpp）
//（2026-09-29，worktree f6）
//
// 机制：
//   817  受击 watcher（TAKE_DAMAGE，damage_kind==NORMAL）→ 攻击者 NextRounds 窗口
//        → ROUND_START 写 damage_reduce_add[attacker] = -100（REDUCE_PCT 默认回调读，
//        applyDamageReduction add 钳底 -100 = ×2 整倍）；
//   897  AFTER_ACTION 桥（duration=1 + 行动方过滤）判"未击败" → 次数型 REDUCE_PCT 回调
//        （1984 同公式 d.final -= d.final*pct/100，只管红伤非直伤，final<=0 不消费）；
//   850  InRounds 窗口 + 概率授票：首跳立即掷 + NextRounds ROUND_START tick 掷，
//        命中 → grant_immune(DAMAGE, duration=1, counts=0)（只覆盖当回合）；
//   68   死亡拦截器（DeathInterceptor，sees_hp_consume=false 残留体力免死档）：
//        致死 DAMAGE 因素 + 窗口内 → hp 写回 1（"余下1点"）。
//
// 断言口径（常识速查 §五：伤害 85%~100% 随机浮动 → 相对比例/上下界，非精确等式）：
//   T1 817（重力反转 24362 [3]）：R1 挨打→R2 攻击者掉血 ≈ 2×（R3 标记过期 ≈ 1×）
//      —— 比例窗 [1.65, 2.45]×（双独立骰 2×[0.85,1]/[0.85,1] 的理论界 [1.7,2.35] 放余量）；
//   T2 850（梦生归元 24575 [3,50]）：{1} 全库均为 50% → 16 场统计断言
//      （R1 至少 1 场挡下且非全挡；R2/R3 窗口 tick 也有挡下；全 48 攻不全挡）；
//   T3 897（气临九渊怒 19324 [1,50]）：R1 未击败（AFTER_ACTION 桥）→ R1 反打即"下1次"
//      减半（比例窗 [0.40, 0.60]×），R2 次数已消费全额；
//   T4 68（星辰守护 20303 [1]）：R1 致死攻击 → 恰余 1 点（精确等式，无随机——
//      拦截器写死 1）；R2 窗口已过期 → 致死成立 hp==0（驱动停在 CHOOSE_AFTER_DEATH）。
// 粉伤 tick 不触发 817、HP_CONSUME 穿 68 免死：与 T1/T4 同一过滤分支，记档不单测。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kZhongLi = 24362;    // 重力反转：属性技 [599,817,610]，817 切片=[3]
constexpr int kMengsheng = 24575;  // 梦生归元：属性技 [585,48,850]，850 切片=[3,50]
constexpr int kQilin = 19324;      // 气临九渊怒：物攻 [760,897,650,101]，897 切片=[1,50]
constexpr int kXingchen = 20303;   // 星辰守护：属性技 [68]，68 切片=[1]
constexpr int kZhuangJi = 10001;   // 撞击：物攻威力 35

// 技能全必中（消 miss 噪声）。
void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet attr_user(int skill, int atk, int hp, int speed = 300) {
    ElfPet p = make_pet(1, "用技方", {skill, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        speed, atk, hp);
    always_hit(p);
    return p;
}

ElfPet striker(int atk, int hp = 1000000, int speed = 100) {
    ElfPet p = make_pet(2, "攻击方", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        speed, atk, hp);
    always_hit(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // T1 817：重力反转（3回合受击→攻击者下回合受伤翻倍）
    //   R1 挂盾方用技、攻击方打它（watcher 触发 → 攻击者标记 R2）
    //   R2 挂盾方反击 → 攻击者掉血 ≈ 2×；R3 再反击 → 标记过期 ≈ 1×
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(attr_user(kZhongLi, 50, 5000)), make_party(striker(100)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 重力反转
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    攻击方打挂盾方
        b.drive();
        CHECK(ctx.getPet(0).hp > 0, "T1 前置：挂盾方存活");
        const int hp_before_r2 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 挂盾方反击
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);           //    攻击方不出手
        b.drive();
        const int drop2 = hp_before_r2 - ctx.getPet(1).hp;
        const int hp_before_r3 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 再反击
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int drop3 = hp_before_r3 - ctx.getPet(1).hp;
        CHECK(drop2 > 0 && drop3 > 0, "T1 前置：两刀都造成了伤害");
        // 理论界：2×r2/r3, r∈[0.85,1] → [1.7, 2.35]；放宽到 [1.65, 2.45]。
        CHECK(drop2 * 100 >= 165 * drop3,
              "T1 核心：受击后攻击者受伤翻倍（drop2 应 ≥1.65×drop3）");
        CHECK(drop2 * 100 <= 245 * drop3,
              "T1 核心：翻倍不超上限（drop2 应 ≤2.45×drop3）");
        std::cout << "  [T1] 817 受击→攻击者下回合受伤翻倍 通过 (drop2=" << drop2
                  << " drop3=" << drop3 << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2 850：梦生归元（3回合，每回合 50% 免疫当回合攻击伤害）
    //   {1} 全库均为 50 → 16 场统计断言（每场 R1 用技 + R1/R2/R3 各挨一刀）。
    // ════════════════════════════════════════════════════════════════
    {
        constexpr int kTrials = 16;
        int blocked_r1 = 0, blocked_window = 0, landed = 0;
        SbsRng trial_seeds(20261004);
        for (int t = 0; t < kTrials; ++t) {
            ScenarioBattle b(make_party(attr_user(kMengsheng, 1, 50000)),
                             make_party(striker(100)));
            b.ctx().seed_rng(trial_seeds.next());
            b.start();
            BattleContext& ctx = b.ctx();
            int hp0 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);  // R1 梦生归元
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            int taken = hp0 - ctx.getPet(0).hp;
            if (taken == 0) {
                ++blocked_r1;
            } else {
                ++landed;
            }
            for (int r = 0; r < 2; ++r) {   // R2/R3：窗口 tick 各掷一次
                hp0 = ctx.getPet(0).hp;
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.drive();
                taken = hp0 - ctx.getPet(0).hp;
                if (taken == 0) {
                    ++blocked_window;
                } else {
                    ++landed;
                }
            }
            CHECK(ctx.getPet(0).hp > 0, "T2 前置：挂票方存活（atk=1 打不死）");
        }
        CHECK(blocked_r1 >= 1, "T2 核心：用技当回合首跳概率授票应至少命中一次");
        CHECK(blocked_r1 <= kTrials - 1, "T2 核心：50% 概率不应全命中");
        CHECK(blocked_window >= 1, "T2 核心：NextRounds 窗口 tick 的后续回合授票应命中");
        CHECK(blocked_r1 + blocked_window <= 3 * kTrials - 1, "T2 核心：50% 概率不应全免疫");
        std::cout << "  [T2] 850 概率免疫当回合 通过 (R1挡=" << blocked_r1 << "/"
                  << kTrials << " 窗口挡=" << blocked_window << " 落地=" << landed << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3 897：气临九渊怒（未击败对手 → 自身下1次受到的攻击伤害额外减少50%）
    //   R1 用技方先手攻击（对手存活 → AFTER_ACTION 桥武装），对手同回合反打
    //   → 反打即"下1次"= 减半；R2 再挨 = 次数已消费 → 全额。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(attr_user(kQilin, 50, 50000)), make_party(striker(100)));
        b.start();
        BattleContext& ctx = b.ctx();
        int hp0 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 气临九渊怒
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    对手反打
        b.drive();
        CHECK(ctx.getPet(1).hp > 0, "T3 前置：对手未被击败（触发门槛）");
        const int drop1 = hp0 - ctx.getPet(0).hp;
        hp0 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R2 用技方不出手
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    再挨一刀
        b.drive();
        const int drop2 = hp0 - ctx.getPet(0).hp;
        CHECK(drop1 > 0 && drop2 > 0, "T3 前置：两刀都造成了伤害");
        // 理论界：0.5×r1/r2, r∈[0.85,1] → [0.425, 0.588]；放宽到 [0.40, 0.60]。
        CHECK(drop1 * 100 >= 40 * drop2, "T3 核心：下1次减伤应 ≥40%×全额");
        CHECK(drop1 * 100 <= 60 * drop2, "T3 核心：减半后不应接近全额（≤60%×全额）");
        std::cout << "  [T3] 897 未击败→下1次减伤50% 通过 (drop1=" << drop1
                  << " drop2=" << drop2 << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T4 68：星辰守护（1回合内受到致死攻击余下1点体力）
    //   攻击方 atk=3000 → 伤害 [754,886] 恒大于 hp=100（确定性致死）。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(attr_user(kXingchen, 1, 100)), make_party(striker(3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 星辰守护
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    致死攻击
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 1, "T4 核心：致死攻击命中 → 恰余 1 点体力");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R2 窗口已过期
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    再致死
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 0, "T4 核心：窗口外致死正常阵亡（免死随 1 回合窗口过期）");
        std::cout << "  [T4] 68 致死余1点（窗口内保/窗口外亡） 通过\n";
    }

    std::cout << "scenario_660 PASS (F6 受击响应族 817/897/850/68)" << std::endl;
    return 0;
}
