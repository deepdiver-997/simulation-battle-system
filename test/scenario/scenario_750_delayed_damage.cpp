#include <utils/sbs_rand.h>
// 场景 750 —— G5 延迟固伤族（571 / 611 / 192，moves_lib/g_delayed_damage.cpp）
//（2026-09-29，worktree ../agents/g5；号段 750-759 一文件覆盖三效果）
//
// 机制：
//   571 {0}回合后对对手造成{1}点固定伤害，重复使用无法叠加 —— NextRounds 窗口挂
//        回合扣减点（收尾点），detonate_round=cast_roundCount+N 精确闸门；同源去重
//        = 重复注册覆盖刷新（单一未引爆计时）；
//   611 下{0}回合自身使用攻击技能则附加{1}点固定伤害 —— NextRounds 窗口 R+1..R+{0}
//        + 每回合 ROUND_START 武装当回合 EVENT_HIT watcher（TEAM + 槽位快照），
//        actor 过滤 + 攻击技能限定（属性技不触发）；
//   192 附加{0}%当前体力值的伤害 —— SKILL_EFFECT 时点 deal_pink_damage
//        (PERCENT_VALUE, 自身当前体力×{0}/100)。
//
// 断言口径（常识速查 §五）：
//   - 粉伤（FIXED/PERCENT_VALUE）无随机浮动 → 精确等式/唯一带；红伤 85%~100% 随机
//     浮动（(0.84×攻/100×威力+2)×(217+sbs_rand()%39)/255，攻取面板、防=100）→ 闭式界。
//   - 威力 0 的**攻击技**（物理/特殊）红伤恒 ∈ {1,2}（基值 2.0）；属性技（category 4）
//     **无伤害阶段**（本场景实测：不掉血）→ 施放回合掉血恰 0。
//   - 载体 all must_hit（消 miss 噪声）；filler 用 ActionType::NONE（不掉血——571 引爆
//     回合的 300 点等式断言依赖它）。
#include <algorithm>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJieShu   = 16869;  // 劫数·万物哀鸣：特攻 160 [143,571]|[1,300] → 571 N=1 300 点
constexpr int kBuxiang  = 23050;  // 不祥之兆：属性技 [5,571]|[5,100,-2,2,300] → 571 N=2 300 点
constexpr int kZhanJiang= 23640;  // 战将之气：属性技 [611,408]|[2,100,2,100] → 611 下2回合 +100
constexpr int kXingChen = 23655;  // 星辰剑意：属性技 [585,611]|[1×6,2,100] → 611 下2回合 +100
constexpr int kKuangSha = 13150;  // 狂沙掠地击：物攻 150 [192]|[10] → 附加自身当前体力 10%
constexpr int kSuiDi    = 14943;  // 碎地强袭斩：物攻 150 [192]|[15] → 附加自身当前体力 15%
constexpr int kZhuangJi = 10001;  // 撞击：物攻威力 35 无效果（攻击探针）
constexpr int kGaoSu    = 20017;  // 高速移动：属性技 [4]（属性技不触发 611 的探针）

// 红伤闭式界（Calculation::calculateDamage 同式：base=0.84×攻/100×威力+2，
// 浮动因子 (217+sbs_rand()%39)/255 ∈ [217,255]/255；无能力等级/克制/本系/暴击）。
int red_min(int atk, int power) {
    const double b = 0.84 * atk / 100.0 * power + 2;
    return static_cast<int>(b * 217.0 / 255.0);
}
int red_max(int atk, int power) {
    const double b = 0.84 * atk / 100.0 * power + 2;
    return static_cast<int>(b);
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk, int hp, int speed = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk = 1, int hp = 1000000) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, atk, hp);
    always_hit(p);
    return p;
}

void play_round(ScenarioBattle& b, BattleFsm::ActionType self_act, int self_slot,
                BattleFsm::ActionType foe_act, int foe_slot) {
    b.submit_operation(0, self_act, self_slot);
    b.submit_operation(1, foe_act, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // T1 571 N=1（劫数·万物哀鸣 [1,300]）：R1 施放（威力160 atk=1 红伤 ∈ [2,3]，
    //   当回合收尾点不引爆）→ R2 收尾点引爆恰 300（"1回合后"）→ R3 无残留。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kJieShu, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*atk=*/1, /*hp=*/500000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        int hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 劫数·万物哀鸣
                   BattleFsm::ActionType::NONE, 0);
        const int loss1 = hp0 - ctx.getPet(1).hp;
        CHECK(loss1 >= red_min(1, 160) && loss1 <= red_max(1, 160),
              "T1 核心：施放回合不引爆（只有红伤 ∈ [2,3]）");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hp, hp0 - 300,
                 "T1 核心：1回合后（R2 收尾点）引爆恰 300 点固伤");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hp, hp0, "T1 核心：引爆后无残留（单次定时，不循环）");
        std::cout << "  [T1] 571 劫数·万物哀鸣 N=1（R1=" << loss1 << " R2=300）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2 571 N=2（不祥之兆 [2,300]）：R1 施放 → R2 末不引爆（差一回合）→
    //   R3 末引爆恰 300（"2回合后"）→ R4 无残留。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kBuxiang, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*atk=*/1, /*hp=*/500000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        int hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 不祥之兆
                   BattleFsm::ActionType::NONE, 0);
        const int loss1 = hp0 - ctx.getPet(1).hp;
        CHECK_EQ(loss1, 0,
              "T2 核心：施放回合不引爆（属性技无伤害阶段，掉血恰 0）");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hp, hp0, "T2 核心：R2 末不引爆（N=2 还差一回合）");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hp, hp0 - 300,
                 "T2 核心：2回合后（R3 收尾点）引爆恰 300 点固伤");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hp, hp0, "T2 核心：引爆后无残留");
        std::cout << "  [T2] 571 不祥之兆 N=2（R1=" << loss1 << " R3=300）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3 571 "重复使用无法叠加" = 同源去重覆盖刷新（本族记档语义）：
    //   R1 施放 → R2 再施放（刷新计时）→ R3 末不引爆（旧计时已被覆盖）→
    //   R4 末引爆**恰一次** 300（若叠加会 R3/R4 各 300；若忽略旧计时会 R3 就炸）。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kBuxiang, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*atk=*/1, /*hp=*/500000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 不祥之兆 #1
                   BattleFsm::ActionType::NONE, 0);
        int hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 不祥之兆 #2（重复使用）
                   BattleFsm::ActionType::NONE, 0);
        const int loss2 = hp0 - ctx.getPet(1).hp;
        CHECK_EQ(loss2, 0, "T3 核心：重复施放回合仍不引爆（属性技无伤害阶段）");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hp, hp0,
                 "T3 核心：R3 末不引爆（重复使用刷新计时，未叠加成双响）");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hp, hp0 - 300,
                 "T3 核心：R4 末引爆恰一次 300（无法叠加 = 单一计时器）");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hp, hp0, "T3 核心：引爆后无残留");
        std::cout << "  [T3] 571 重复使用无法叠加（刷新语义，R2=" << loss2
                  << " R4=300）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T4 571 持有者离场即失（ON_STAGE 作用域，本族记档保守取态）：
    //   R1 施放 → R2 换宠 → R3 末不引爆（窗口随离场失效）。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kBuxiang, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*atk=*/1, /*hp=*/500000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 不祥之兆
                   BattleFsm::ActionType::NONE, 0);
        int hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::CHOOSE_PET, 1,     // 换宠（持有者离场）
                   BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hp, hp0, "T4 前提：换宠回合无引爆（N=2 未到期）");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hp, hp0,
                 "T4 核心：持有者离场后 R3 末不引爆（ON_STAGE 窗口已作废）");
        std::cout << "  [T4] 571 换宠即失（离场后 R3 末 0 引爆）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T5 611（战将之气 [2,100]）：窗口 = R2..R3。
    //   R1 施放（属性技，当回合不附加）→ R2 撞击附加 100（掉血 ∈ [101,102]）→
    //   R3 高速移动（属性技命中不附加——攻击技能限定）→ R4 窗口过期 →
    //   R5 对手出手（actor 过滤：对手命中不给对手自己发附加）。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kZhanJiang, kZhuangJi, kGaoSu, kZhuangJi, kZhuangJi},
                                             /*atk=*/1, /*hp=*/500000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        int hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 战将之气
                   BattleFsm::ActionType::NONE, 0);
        const int loss1 = hp0 - ctx.getPet(1).hp;
        CHECK_EQ(loss1, 0,
              "T5 核心：用技当回合不附加（下2回合窗口从 R2 起；属性技无伤害阶段）");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,   // 撞击（窗口第 1 跳）
                   BattleFsm::ActionType::NONE, 0);
        const int loss2 = hp0 - ctx.getPet(1).hp;
        CHECK(loss2 >= 101 && loss2 <= 102,
              "T5 核心：窗口内攻击命中附加恰 100（红伤 ∈ {1,2}）");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 2,   // 高速移动（属性技）
                   BattleFsm::ActionType::NONE, 0);
        const int loss3 = hp0 - ctx.getPet(1).hp;
        CHECK_EQ(loss3, 0,
              "T5 核心：属性技命中不附加（攻击技能限定；属性技无伤害阶段恰 0 掉血）");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,   // 撞击（窗口已过期）
                   BattleFsm::ActionType::NONE, 0);
        const int loss4 = hp0 - ctx.getPet(1).hp;
        CHECK(loss4 >= 1 && loss4 <= 2, "T5 核心：下2回合窗口过期（R4 无附加）");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::SELECT_SKILL, 0);    // 对手出手
        CHECK_EQ(ctx.getPet(1).hp, hp0,
                 "T5 核心：对手使用技能不触发附加（EVENT_HIT actor 过滤）");
        std::cout << "  [T5] 611 战将之气（R1=" << loss1 << " R2=" << loss2
                  << " R3属性=" << loss3 << " R4过期=" << loss4 << "）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T6 611 第二载体（星辰剑意 [585,611]|[1×6,2,100]）：585 全属性+1 后红伤界
    //   现读视图重算（getTempAbilityValue 与伤害公式同源）→ R2 撞击附加 100。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kXingChen, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*atk=*/1, /*hp=*/500000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        int hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 星辰剑意（全属性+1 + 挂窗）
                   BattleFsm::ActionType::NONE, 0);
        const int loss1 = hp0 - ctx.getPet(1).hp;
        CHECK_EQ(loss1, 0, "T6 核心：施放回合不附加（属性技无伤害阶段恰 0 掉血）");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,   // 撞击（窗口第 1 跳）
                   BattleFsm::ActionType::NONE, 0);
        // 585 已把攻击面板抬过（+1 等级），红伤界现读当前视图（回合交界 sync 后等级不变）。
        const int atk_now =
            ctx.ws.getTempAbilityValue(0, NumericalPropertyIndex::PHYSICAL_ATTACK);
        const int loss2 = hp0 - ctx.getPet(1).hp;
        CHECK(loss2 >= 100 + red_min(atk_now, 35) && loss2 <= 100 + red_max(atk_now, 35),
              "T6 核心：第二载体窗口附加恰 100（红伤按提升后面板闭式界）");
        std::cout << "  [T6] 611 星辰剑意（R1=" << loss1 << " R2=" << loss2
                  << " atk视图=" << atk_now << "）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T7 192（狂沙掠地击 [10]）：附加**自身当前**体力 10%。
    //   自身压到 123456/300000 → 粉伤 = 12345（整型整除）；威力150 atk=1 红伤 ∈ {2,3}
    //   → 掉血 ∈ [12347, 12348]（若误用最大体力 300000 → 粉 30000，与本界不相交必炸）。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kKuangSha, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*atk=*/1, /*hp=*/300000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 123456;   // 压血：当前体力 123456（≠ 上限 300000）
        const int hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 狂沙掠地击
                   BattleFsm::ActionType::NONE, 0);
        const int loss = hp0 - ctx.getPet(1).hp;
        CHECK(loss >= 12345 + red_min(1, 150) && loss <= 12345 + red_max(1, 150),
              "T7 核心：附加自身当前体力10%（粉=12345 整除；当前≠上限的判别界）");
        std::cout << "  [T7] 192 狂沙掠地击 当前体力10%（loss=" << loss
                  << " 界=[12347,12348]）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T8 192 第二载体（碎地强袭斩 [15]）：自身 200000/300000 → 粉 = 30000，
    //   红伤 ∈ {2,3} → 掉血 ∈ [30002, 30003]。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kSuiDi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*atk=*/1, /*hp=*/300000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 200000;
        const int hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 碎地强袭斩
                   BattleFsm::ActionType::NONE, 0);
        const int loss = hp0 - ctx.getPet(1).hp;
        CHECK(loss >= 30000 + red_min(1, 150) && loss <= 30000 + red_max(1, 150),
              "T8 核心：第二载体附加 15%（粉=30000 精确）");
        std::cout << "  [T8] 192 碎地强袭斩 当前体力15%（loss=" << loss
                  << " 界=[30002,30003]）通过\n";
    }

    std::cout << "scenario_750 PASS (G5 延迟固伤族 571/611/192)" << std::endl;
    return 0;
}
