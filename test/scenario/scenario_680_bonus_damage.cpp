#include <utils/sbs_rand.h>
// 场景 680 —— F8 固伤附加族（593 / 455 / 1004 / 1244，moves_lib/f_bonus_damage.cpp）
//（2026-09-29，worktree ../agents/f8；号段 680-689 一文件覆盖四效果）
//
// 机制：
//   593  附加{取值方}的{官方属性码}值的{pct}%的百分比伤害 —— deal_pink_damage
//        (PERCENT_VALUE, 面板值×pct/100)。⚠️ {1} 是**官方属性码序**
//        （0攻 1防 2特攻 3特防 4速）→ 引擎槽 {0,2,1,3,4}，与引擎序(0攻1特攻2防)
//        在 1/2 两位互换——T2b/T2c 用第三载体专门钉死这两位；
//   455  每损失{0}点体力附加{1}点固伤 —— (max_hp−hp)/{0}×{1} 点 FIXED；
//   1004 NextRounds 窗口（R+1..R+{0}）+ 每回合 ROUND_START 武装当回合 EVENT_HIT
//        watcher（TEAM）→ 自己技能命中即附加 {1} 点 FIXED，自身体力**严格高于**
//        对手时 ×2；用技当回合（R）不结算（窗口天然 R+1 起，且当回合 EVENT_HIT
//        已在 ON_SKILL_HIT 桶后 drain——实现头记档）；
//   1244 随机 {0}-{1} 点 FIXED，对手六槽能力等级任一 <0 → 变 {2}-{3} 点。
//
// 断言口径（常识速查 §五）：
//   - 粉伤（FIXED/PERCENT_VALUE）无随机浮动 → 精确分量；红伤 85%~100% 浮动
//     （(0.84×攻/防×威力+2)×(217+sbs_rand()%39)/255，攻取面板、防=100、无克制/本系/
//     暴击）→ 用闭式上下界夹总掉血，粉伤作为界内确定分量被钉死。
//   - atk=1 时红伤基值：power0→2.0、power35→2.294、power85→2.714、power90→2.756；
//     其中 power85/90 的基值与 ×217/255 截断同为 2 → 红伤恒 2（确定性），
//     power0/35 红伤 ∈ {1,2}。
//   - 1244 双档随机：统计断言（24 试/档，F6 T2 的 16 场同款思路）——硬界逐场判，
//     再各要一条"专属带"观测钉死档位选择（P(全 miss)≈(101/201)^24≈5e-8）。
//   - 探针/载体 all must_hit（消 miss 噪声）；暴击率由 harness 清零；filler 用
//     ActionType::NONE（威力 0 有 1 点保底伤）。
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

constexpr int kGuangZhiYi = 16115;  // 光之翼：物攻 80 先制+2 [593][0,0,15] → 自身攻击15%
constexpr int kShengXi    = 16134;  // 圣熄之力：物攻 0 [593][1,3,100] → 对方特防100%
constexpr int kChuangShiDun = 16333;  // 圣光创世盾：物攻 160 [593][0,1,25] → 自身防御25%（映射钉子）
constexpr int kQianYan    = 16440;  // 真龙千言诀：特攻 150 [593][0,2,20] → 自身特攻20%（映射钉子）
constexpr int kChuangShi  = 13552;  // 创世弹：特攻 90 [455][1,1] → 每损失1体附1固伤
constexpr int kKuangLong  = 19814;  // 狂龙啸傲：特攻 0 [63,1003,1004][80,6,1,5,150] → 1004 切片 [5,150]
constexpr int kHuangWu    = 31164;  // 荒芜之爪：物攻 85 先制+3 [889,1244][6,100,300,200,400] → 1244 切片 [100,300,200,400]
constexpr int kZhuangJi   = 10001;  // 撞击：物攻威力 35 无效果（普攻探针）
constexpr int kLoss200k   = 200000; // T3 压出的已损失体力（300000-100000）

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
    // T1 593 光之翼 [0,0,15]：附加自身攻击面板值的 15%（{0}=0 自身、码 0=攻击）
    //   atk=2000 → 粉伤 = 300（PERCENT_VALUE 精确）；红伤（威力80）∈ [1145,1346]
    //   → 总掉血 ∈ [1445, 1646]，粉伤作为界内确定分量被钉死。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kGuangZhiYi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*atk=*/2000, /*hp=*/500000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(1).hp;
        const int self_hp0 = ctx.getPet(0).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 光之翼
                   BattleFsm::ActionType::NONE, 0);
        const int loss = hp0 - ctx.getPet(1).hp;
        const int lo = red_min(2000, 80) + 300;
        const int hi = red_max(2000, 80) + 300;
        CHECK(loss >= lo, "T1 核心：总掉血 ≥ 红伤下界+300（附加自身攻击15%落地）");
        CHECK(loss <= hi, "T1 核心：总掉血 ≤ 红伤上界+300（附加不多不少恰 300）");
        CHECK_EQ(ctx.getPet(0).hp, self_hp0, "T1：对手未出手，自身无伤");
        std::cout << "  [T1] 593 光之翼 自身攻击15%（粉=" << loss - red_min(2000, 80)
                  << ".." << loss - red_max(2000, 80) << " 界=[300,300]）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2a 593 圣熄之力 [1,3,100]：{0}=1 取**对方**面板、码 3=特防 → 100%
    //   直写对手特防面板 137 → 粉伤 = 137；自身 atk=1 威力0 红伤 ∈ {1,2}
    //   → 总掉血 ∈ [138, 139]。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kShengXi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*atk=*/1, /*hp=*/500000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).numericalProperties[NumericalPropertyIndex::SPECIAL_DEFENSE] = 137;
        const int hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 圣熄之力
                   BattleFsm::ActionType::NONE, 0);
        const int loss = hp0 - ctx.getPet(1).hp;
        CHECK(loss >= 138, "T2a 核心：附加对方特防值的100%（粉=137 落地，红伤≥1）");
        CHECK(loss <= 139, "T2a 核心：粉伤恰 137（红伤 ∈ {1,2} 封顶）");
        std::cout << "  [T2a] 593 圣熄之力 对方特防100% 通过 (loss=" << loss << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2b 593 官方码 1 = **防御**（非引擎槽 1=特攻）：圣光创世盾 [0,1,25]
    //   自身防御面板 100 → 粉伤 = 25；atk=1 威力160 红伤 ∈ [2,3]
    //   → 总掉血 ∈ [27, 28]。若映射误按引擎序读"特攻"（=1）→ 粉 0、掉血 ≤3，必炸。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kChuangShiDun, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*atk=*/1, /*hp=*/500000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 圣光创世盾
                   BattleFsm::ActionType::NONE, 0);
        const int loss = hp0 - ctx.getPet(1).hp;
        CHECK(loss >= 27, "T2b 核心：官方码1=防御（粉=100×25%=25 落地）");
        CHECK(loss <= 28, "T2b 核心：粉伤恰 25（红伤威力160 atk=1 ∈ [2,3] 封顶）");
        std::cout << "  [T2b] 593 官方码1→防御映射钉子 通过 (loss=" << loss << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2c 593 官方码 2 = **特攻**（非引擎槽 2=防御）：真龙千言诀 [0,2,20]
    //   自身特攻面板 2000 → 粉伤 = 400；威力150 红伤 ∈ [2146,2522]
    //   → 总掉血 ∈ [2546, 2922]。若误按引擎序读"防御"（=100）→ 粉 20、
    //   掉血 ≤ 2542，与本界不相交。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kQianYan, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*atk=*/2000, /*hp=*/500000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 真龙千言诀
                   BattleFsm::ActionType::NONE, 0);
        const int loss = hp0 - ctx.getPet(1).hp;
        CHECK(loss >= 2546, "T2c 核心：官方码2=特攻（粉=2000×20%=400 落地）");
        CHECK(loss <= 2922, "T2c 核心：粉伤恰 400（红伤界封顶）");
        std::cout << "  [T2c] 593 官方码2→特攻映射钉子 通过 (loss=" << loss << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3 455 创世弹 [1,1]：每损失 1 点体力附加 1 点固伤
    //   自身压到 100000/300000 → 损失 200000 → 粉伤 = 200000（FIXED 精确）；
    //   atk=1 威力90 特攻红伤基值 2.756，×217/255 与本体截断同为 2 → 红伤恒 2
    //   → 对手恰掉 200002（精确等式）。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kChuangShi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*atk=*/1, /*hp=*/300000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;   // 压血：损失 200000（017/T2a 直写同款）
        const int hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 创世弹
                   BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hp, hp0 - kLoss200k - 2,
                 "T3 核心：附加损失体力×1（粉=200000 精确）+ 红伤恒 2（威力90 atk=1 确定性）");
        std::cout << "  [T3] 455 创世弹 损体折算固伤（精确 200002）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T4 1004 狂龙啸傲 [5,150]：NextRounds 窗口 R2..R6 + EVENT_HIT 消费 + 高体翻倍
    //   R1 用狂龙啸傲：当回合不结算（红伤 ∈ {1,2}，若误结算必 ≥151）
    //   R2 撞击（自身 1000000 > 对手）：附加×2=300 → 掉血 ∈ [301,302]
    //   R3 压自身血 < 对手：附加 150 → 掉血 ∈ [151,152]
    //   R4 自身不出手：无附加（对手掉血 = 0）
    //   R5/R6 窗口内再命中：150 档；R7 窗口过期：只剩红伤 ∈ {1,2}
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kKuangLong, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*atk=*/1, /*hp=*/1000000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：狂龙啸傲（窗口 R+1 起，当回合自身的 EVENT_HIT 已 drain，不附加）
        int hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 狂龙啸傲
                   BattleFsm::ActionType::NONE, 0);
        const int loss1 = hp0 - ctx.getPet(1).hp;
        CHECK(loss1 >= 1 && loss1 <= 2,
              "T4 核心：用技当回合不附加（NextRounds 窗口从下回合起，只有威力0保底红伤）");
        // R2：自身满血 1000000 > 对手（已被 R1 打掉 1-2 点）→ 附加翻倍 300
        CHECK(ctx.getPet(0).hp > ctx.getPet(1).hp, "T4 前提：自身体力严格高于对手");
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,   // 撞击
                   BattleFsm::ActionType::NONE, 0);
        const int loss2 = hp0 - ctx.getPet(1).hp;
        CHECK(loss2 >= 301 && loss2 <= 302,
              "T4 核心：窗口内命中附加 150 且体力高于对手翻倍（红伤 ∈ {1,2}）");
        // R3：压自身血低于对手 → 附加恢复 150
        ctx.getPet(0).hp = 500000;
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                   BattleFsm::ActionType::NONE, 0);
        const int loss3 = hp0 - ctx.getPet(1).hp;
        CHECK(loss3 >= 151 && loss3 <= 152,
              "T4 核心：体力不高于对手 → 附加恰 150（不翻倍）");
        // R4：自身不出手（对手出手）→ 无"使用技能"，不附加
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(1).hp, hp0, "T4 核心：自身没用技能 → 对手不掉血（EVENT_HIT actor 过滤）");
        // R5：窗口内（第 4 跳）照常附加 150
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                   BattleFsm::ActionType::NONE, 0);
        const int loss5 = hp0 - ctx.getPet(1).hp;
        CHECK(loss5 >= 151 && loss5 <= 152, "T4 核心：窗口中段（R5）附加仍 150");
        // R6：窗口最后一跳（R+1..R+{0} = R2..R6）
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                   BattleFsm::ActionType::NONE, 0);
        const int loss6 = hp0 - ctx.getPet(1).hp;
        CHECK(loss6 >= 151 && loss6 <= 152, "T4 核心：窗口末跳（R6）附加仍 150");
        // R7：窗口已过期 → 只剩红伤
        hp0 = ctx.getPet(1).hp;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                   BattleFsm::ActionType::NONE, 0);
        const int loss7 = hp0 - ctx.getPet(1).hp;
        CHECK(loss7 >= 1 && loss7 <= 2, "T4 核心：5 回合窗口过期（R7 无附加）");
        std::cout << "  [T4] 1004 狂龙啸傲 窗口固伤+高体翻倍 通过 (R1=" << loss1
                  << " R2=" << loss2 << " R3=" << loss3 << " R5=" << loss5
                  << " R6=" << loss6 << " R7=" << loss7 << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T5 1244 荒芜之爪 [100,300,200,400]：随机固伤双档（对手能力下降 → 高档）
    //   atk=1 威力85 红伤恒 2（基值 2.714 与 ×217/255 同截断）→ 粉伤 = 掉血-2。
    //   普通档 roll ∈ [100,300]（掉血 ∈ [102,302]）、下降档 ∈ [200,400]
    //   （掉血 ∈ [202,402]）；各 24 试取专属带证据（[102,201] / [303,402]）。
    // ════════════════════════════════════════════════════════════════
    {
        constexpr int kTrials = 24;
        int min_a = 1 << 30, max_a = 0;
        for (int t = 0; t < kTrials; ++t) {
            ScenarioBattle b(make_party(self_pet({kHuangWu, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                                 /*atk=*/1, /*hp=*/500000)),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int hp0 = ctx.getPet(1).hp;
            play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 荒芜之爪
                       BattleFsm::ActionType::NONE, 0);
            const int loss = hp0 - ctx.getPet(1).hp;
            CHECK(loss >= 102, "T5a 硬界：普通档下限（粉 ≥100 + 红 2）");
            CHECK(loss <= 302, "T5a 硬界：普通档上限（粉 ≤300 + 红 2）");
            min_a = std::min(min_a, loss);
            max_a = std::max(max_a, loss);
        }
        CHECK(min_a <= 201,
              "T5a 专属带：24 试中至少一次掉进普通档专属带 [102,201]（概率性断言，P(全 miss)≈5e-8）");
        std::cout << "  [T5a] 1244 普通档 [100,300] 通过 (min=" << min_a << " max=" << max_a << ")\n";

        int min_b = 1 << 30, max_b = 0;
        for (int t = 0; t < kTrials; ++t) {
            ScenarioBattle b(make_party(self_pet({kHuangWu, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                                 /*atk=*/1, /*hp=*/500000)),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
            b.start();
            BattleContext& ctx = b.ctx();
            // 注入对手攻击 -1（六槽权威态直写，630 grant_foe_boosts 同款）→ 下降档
            ctx.ability_levels[1][0] = -1;
            const int hp0 = ctx.getPet(1).hp;
            play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,   // 荒芜之爪
                       BattleFsm::ActionType::NONE, 0);
            const int loss = hp0 - ctx.getPet(1).hp;
            CHECK(loss >= 202, "T5b 硬界：下降档下限（粉 ≥200 + 红 2）");
            CHECK(loss <= 402, "T5b 硬界：下降档上限（粉 ≤400 + 红 2）");
            min_b = std::min(min_b, loss);
            max_b = std::max(max_b, loss);
        }
        CHECK(max_b >= 303,
              "T5b 专属带：24 试中至少一次掉进下降档专属带 [303,402]（概率性断言，P(全 miss)≈5e-8）");
        std::cout << "  [T5b] 1244 下降档 [200,400] 通过 (min=" << min_b << " max=" << max_b << ")\n";
    }

    std::cout << "scenario_680 PASS (F8 固伤附加族 593/455/1004/1244)" << std::endl;
    return 0;
}
