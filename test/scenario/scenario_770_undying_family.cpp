// 场景 770：G7 免死反扣族 —— 692（moves_lib/g_undying_family.cpp，看板 G7，2026-09-29）
//
// 机制（68 不死族姊妹档，DeathInterceptor 同通道）：
//   窗口内受**致死攻击**（红伤/属性直伤致死）→ 免死（hp 写回受击前体力 = 完全不掉血）
//   + 对攻击者 deal_true_damage(原伤量)（真伤直通护盾护罩；数值 = core 落血记录
//   last_landed_hit.amount——护盾/锁伤后全额）。窗口 = {0} 回合 InRounds（时限不是次数，
//   窗口内多次致死逐次拦）；窗口外/粉伤真伤斩杀/秒杀/消耗体力不拦。
//
// 载体（全库 30 条切片自查 + 4399 交叉验证，见 g_undying_family.cpp 文件头）：
//   T1/T6：圣战封印 31710=[692](2)（物攻 130——**纯载体**，无共效干扰）
//   T2    ：次元反转 31850=[692](2)（物攻 130，纯载体；窗口过期对照）
//   T3    ：破灭之夜 36100=[692](2)（特攻 120，纯载体）
//   T4    ：无尽入侵 23769=[693(2),191(150,5),692(2),125(2,260)]（属性技——看板点名载体；
//           共效 125"被攻击所受伤害不超过260"把致死伤钳到 260，仍 > hp=100 恒致死，
//           191 异常反弹无异常 no-op、693 自伤幅不碰断言面）
//   T5    ：紫电狂龙 23920=[448(...),692(2)]（属性技——看板点名载体；448 降级窗不碰断言面）
//
// 断言口径（常识速查 §五：伤害 85%~100% 随机浮动 → 精确等式用"反扣量 = 落血记录"构造）：
//   反扣量 = last_landed_hit[0][slot].amount（内核同一击写入的落血记录）→
//   "对手掉血 == 记录值"是**精确等式**（真伤不受任何修正），浮动骰被等式两边同吃；
//   免死方 hp 恒等（写回受击前体力）也是精确等式。A 方攻击的 1~2 点保底噪声用
//   B 的 100 点护盾吸收（真伤直通护盾，反扣不受影响）。
//   T1  31710 窗口 [R1,R2]：R1/R2 两次致死**逐次**免死不掉血 + 反扣精确等式（×2）
//   T2  31850 窗口过期：R1 免死；R2 空过；R3 致死成立 hp==0、对手不掉血（窗口外不反扣）
//   T3  36100 特攻载体：R1 免死 + 反扣精确等式（特攻面板同判据）
//   T4  23769 命名载体（125 钳伤面）：免死不掉血 + 反扣 == 钳后落血记录
//   T5  23920 命名载体：免死不掉血 + 反扣精确等式
//   T6  31710 反扣致死反杀：B hp=500 < 反扣下界 754 → 反扣真伤把 B 打死
// 粉伤/真伤斩杀不拦、HP_CONSUME 穿免死：与主路径同一 cause/kind 过滤分支，记档不单测
//（同 scenario_660 T4 口径——漏斗不带种类标记，插件侧无凭可单测该边界）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kShengZhan = 31710;   // 圣战封印：物攻 130，[692] 切片=[2]（纯载体）
constexpr int kCiYuan = 31850;      // 次元反转：物攻 130，[692] 切片=[2]（纯载体）
constexpr int kPoMie = 36100;       // 破灭之夜：特攻 120，[692] 切片=[2]（纯载体）
constexpr int kWuJin = 23769;       // 无尽入侵：属性技，692 切片=[2]（共效 125 钳伤 260）
constexpr int kZiDian = 23920;      // 紫电狂龙：属性技，692 切片=[2]（共效 448 降级窗）
constexpr int kZhuangJi = 10001;    // 撞击：物攻威力 35（攻击方出手 / filler 槽）

constexpr int kCarrierHp = 100;     // 载体方 hp：撞击 atk=3000 伤害 [754,886] 恒致死（660 T4 同体量）

// 技能全必中（消 miss 噪声）；crit 由 harness 默认清零。
void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

// 载体方（side 0，先手）：atk=1（自身攻击只有保底 1~2 点，且被对手护盾吸收）。
ElfPet carrier_user(int skill) {
    ElfPet p = make_pet(1, "载体方", {skill, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        /*speed=*/300, /*atk=*/1, kCarrierHp);
    always_hit(p);
    return p;
}

// 攻击方（side 1，后手）：atk=3000 → 撞击伤害 [754,886] 恒 > hp=100（确定性致死）。
ElfPet striker(int hp = 1000000) {
    ElfPet p = make_pet(2, "攻击方", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        /*speed=*/100, /*atk=*/3000, hp);
    always_hit(p);
    return p;
}

// core 落血记录（弹伤族数据源）：致死一击落在 side0 的 amount = 反扣原伤量。
int landed_amount(BattleContext& ctx, int side) {
    return ctx.last_landed_hit[side][ctx.on_stage[side]].amount;
}

// 开局给攻击方挂 100 点护盾（duration=0 永久）：吸收载体方 1~2 点保底攻击，
// 使"攻击方掉血 == 反扣真伤"成为纯等式（真伤直通护盾，反扣不受影响）。
void shield_striker(BattleContext& ctx) {
    ctx.getPet(1).shield_bank_.add(/*priority=*/0, /*quantity=*/100, /*source_id=*/4700,
                                   /*duration=*/0);
}

// 一个标准对局：载体方先手用技 + 攻击方致死一击。
struct Duel {
    ScenarioBattle b;
    BattleContext& ctx;
    int hp1_before;   // 攻击方出手前 hp（每回合刷新）

    explicit Duel(int carrier_skill, int striker_hp = 1000000)
        : b(make_party(carrier_user(carrier_skill)), make_party(striker(striker_hp))),
          ctx(b.ctx()),
          hp1_before(0) {
        b.start();
        shield_striker(ctx);
    }
    void round_use_and_hit() {
        hp1_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 载体方用技
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 攻击方致死一击
        b.drive();
    }
    void round_none_none() {
        hp1_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
    }
    void round_none_hit() {
        hp1_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
    }
};

// 免死 + 反扣核心断言组（一次致死一击后调用）。
void check_undying_and_counter(Duel& d, const char* tag) {
    CHECK_EQ(d.ctx.getPet(0).hp, kCarrierHp,
             tag << "：免疫致命一击伤害 → hp 写回受击前体力（完全不掉血）");
    CHECK(d.ctx.getPet(0).hp > 0 && !d.ctx.pet_death_notified[0][d.ctx.on_stage[0]],
          tag << "：免死成立（死亡未登记，回合照常继续）");
    const int counter = d.hp1_before - d.ctx.getPet(1).hp;
    const int record = landed_amount(d.ctx, 0);
    CHECK(counter > 0, tag << "：反扣真伤应已落地（攻击方掉血 >0）");
    CHECK_EQ(counter, record,
             tag << "：直接扣除对手**等量**体力（反扣 == 致死一击落血记录，精确等式；反扣="
                 << counter << " 记录=" << record << "）");
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // T1 圣战封印（[2] = 2 回合窗）：窗口内**多次**致死逐次拦
    //   R1 用技 + 致死一击 → 免死不掉血 + 反扣等量；R2 再致死 → 同样（时限不是次数）
    // ════════════════════════════════════════════════════════════════
    {
        Duel d(kShengZhan);
        d.round_use_and_hit();
        check_undying_and_counter(d, "T1-R1");
        d.round_none_hit();
        check_undying_and_counter(d, "T1-R2（窗口内第二次致死，逐次拦不消耗）");
        std::cout << "  [T1] 692 窗口内免死不掉血 + 反扣等量（R1/R2 逐次） 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2 次元反转（2 回合窗）：窗口外致死正常阵亡、不反扣
    //   R1 免死；R2 双方空过（窗口耗尽）；R3 致死成立 hp==0、攻击方不掉血
    // ════════════════════════════════════════════════════════════════
    {
        Duel d(kCiYuan);
        d.round_use_and_hit();
        CHECK_EQ(d.ctx.getPet(0).hp, kCarrierHp, "T2-R1 前置：窗口内免死");
        d.round_none_none();
        CHECK_EQ(d.ctx.getPet(0).hp, kCarrierHp, "T2-R2 前置：空过不掉血");
        d.round_none_hit();   // R3：窗口 [R1,R2] 已过期
        CHECK_EQ(d.ctx.getPet(0).hp, 0, "T2 核心：窗口外致死正常阵亡（免死随窗口过期）");
        CHECK(d.ctx.pet_death_notified[0][d.ctx.on_stage[0]], "T2 核心：死亡已登记");
        CHECK_EQ(d.hp1_before - d.ctx.getPet(1).hp, 0,
                 "T2 核心：窗口外不反扣（攻击方不掉血）");
        std::cout << "  [T2] 692 窗口过期（免死失效/不反扣） 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3 破灭之夜（特攻 120 纯载体）：特攻面板载体同判据
    // ════════════════════════════════════════════════════════════════
    {
        Duel d(kPoMie);
        d.round_use_and_hit();
        check_undying_and_counter(d, "T3");
        std::cout << "  [T3] 692 特攻载体（破灭之夜） 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T4 无尽入侵 23769（看板点名载体，属性技）：共效 125 把受击钳到 260
    //   → 致死伤 = 钳后 260（>hp=100 恒致死），反扣 == 钳后落血记录
    // ════════════════════════════════════════════════════════════════
    {
        Duel d(kWuJin);
        d.round_use_and_hit();
        check_undying_and_counter(d, "T4");
        CHECK(landed_amount(d.ctx, 0) <= 260,
              "T4 附加：共效 125 生效（落血记录被钳到 ≤260，实际 "
                  << landed_amount(d.ctx, 0) << "）");
        std::cout << "  [T4] 692 命名载体无尽入侵（125 钳伤面） 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T5 紫电狂龙 23920（看板点名载体，属性技）：448 降级窗不碰断言面
    // ════════════════════════════════════════════════════════════════
    {
        Duel d(kZiDian);
        d.round_use_and_hit();
        check_undying_and_counter(d, "T5");
        std::cout << "  [T5] 692 命名载体紫电狂龙 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T6 反扣致死反杀：攻击方 hp=500 < 反扣下界 754 → 真伤反扣直接击杀攻击方
    // ════════════════════════════════════════════════════════════════
    {
        Duel d(kShengZhan, /*striker_hp=*/500);
        d.round_use_and_hit();
        CHECK_EQ(d.ctx.getPet(0).hp, kCarrierHp, "T6 前置：免死不掉血");
        CHECK_EQ(d.ctx.getPet(1).hp, 0, "T6 核心：反扣真伤（≥754）> hp=500 → 攻击方被反杀");
        CHECK(d.ctx.pet_death_notified[1][d.ctx.on_stage[1]],
              "T6 核心：反杀走死亡漏斗（死亡已登记）");
        std::cout << "  [T6] 692 反扣致死反杀 通过\n";
    }

    std::cout << "scenario_770_undying_family PASS (G7 免死反扣族 692)" << std::endl;
    return 0;
}
