// 场景 053：圣灵谱尼 5000 的经典基础技能（批 4）。
//
// 数据来自 moves.side_effect / side_effect_arg + effect_info.analyze 逐条核对：
//   千烈虚光闪 10999 [3]       灵魂干涉 20499 [22:100,2]   圣灵魔闪光 11000 [28:8]
//   元素 11261 [29:200]        圣堂之门 10998 [33]         轮回 20607 [43:1]
//   永恒 20608 [87]            旋灭裂空阵 10995 [60:5,30]  生命 20606 [77:5,100]
//   圣光气 20500 [58:2]        圣洁 20605 [86:5]           虚无 20498 [52:2]
//
// 验证点：
//   C1  effect 3  解除自身能力下降（走 clear_stat_drops，**不查免疫**）
//   C2  effect 33 消除对手能力提升（走 clear_stat_boosts）
//   C3  effect 43 恢复自身最大体力的 1/{0}（走 heal 原语）
//   C4  effect 87 恢复自身所有 PP
//   C5  effect 29 额外附加 {0} 点固定伤害（走粉伤管线）
//   C6  ★ effect 28 降低对方 1/{0} 的 hp —— 用 PERCENT_VALUE 精确 1/N（不是 PERCENT 的整数%）
//   C7  effect 22 {0}% 令对手疲惫 {1} 回合
//   C8  ★ effect 60 {0}回合内每回合附加{1}点固定伤害（逐回合）
//   C9  ★ effect 77 {0}回合内若使用技能则恢复{1}点体力（逐回合 + 走 heal 原语）
//   C10 ★ effect 58 下{0}回合自身攻击技能**必定**致命一击（crit_rate==0 的技能也暴）
//   C11 ★ effect 86 {0}回合内属性攻击对自身**必定 miss**
//   C12 ★ effect 52 {0}回合若本方先手攻击，使得对方的技能失效（先手权判据）
#include <cassert>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kGaoSu = 20017;    // 高速移动：属性技能
constexpr int kQuanLi = 10033;   // 全力一击：物理 power 120（无 side_effect，测暴击用）
constexpr int kMaxHp = 3000;

// p0 = 谱尼方（速度快，先手）；p1 = 对手。
scenario::ScenarioBattle make_battle(int p0_hp = kMaxHp, int p1_hp = kMaxHp) {
    auto p0 = scenario::make_pet(0, "圣灵谱尼", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/300, /*atk=*/100, p0_hp);
    auto p1 = scenario::make_pet(1, "对手", {kQuanLi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, /*atk=*/100, p1_hp);
    p1.skills[0].accuracy = 100;
    p1.skills[0].must_hit = true;
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

void call_effect(BattleContext& ctx, int id, std::vector<int> ints) {
    Effect e = EffectFactory::getInstance().getEffect(id, EffectArgs(std::move(ints)));
    CHECK(e.logic != nullptr, "effect 应已在插件中注册");
    e.logic(&ctx, e.args);
}

// p0 用技能 0（高速移动），p1 用技能 `p1_skill`。
void round_with(scenario::ScenarioBattle& b, int p1_skill) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, p1_skill);
    b.drive();
}

// 双方都空转。
void idle_round(scenario::ScenarioBattle& b) { round_with(b, 1); }

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== C1 effect 3：解除自身能力下降 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        b.ctx().ability_levels[0][0] = -2;          // 自身攻击 -2
        b.ctx().ws.view_levels[0][0] = -2;
        call_effect(b.ctx(), 3, {0, 1});
        CHECK_EQ(b.ctx().ability_levels[0][0], 0, "C1) 解除自身能力下降 → 0");
        std::cout << "  [C1] 千烈虚光闪 effect 3：自身攻击 -2 → "
                  << b.ctx().ability_levels[0][0] << "\n";
    }

    //========== C2 effect 33：消除对手能力提升 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        b.ctx().ability_levels[1][0] = 3;
        b.ctx().ws.view_levels[1][0] = 3;
        call_effect(b.ctx(), 33, {0, 1});
        CHECK_EQ(b.ctx().ability_levels[1][0], 0, "C2) 消除对手能力提升 → 0");
        std::cout << "  [C2] 圣堂之门 effect 33：对手攻击 +3 → "
                  << b.ctx().ability_levels[1][0] << "\n";
    }

    //========== C3 effect 43：恢复自身最大体力的 1/{0} ==========
    {
        scenario::ScenarioBattle b = make_battle(/*p0_hp=*/kMaxHp);
        b.start();
        b.ctx().getPet(0).hp = 1000;   // 掉到 1000
        call_effect(b.ctx(), 43, {0, 1, /*分母=*/2});
        CHECK_EQ(b.ctx().getPet(0).hp, 1000 + kMaxHp / 2, "C3) 恢复最大体力 1/2 → +1500");
        std::cout << "  [C3] 轮回 effect 43：体力 1000 → " << b.ctx().getPet(0).hp
                  << "（恢复 1/2 最大体力）\n";
    }

    //========== C4 effect 87：恢复自身所有 PP ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        for (Skills& sk : b.ctx().getPet(0).skills) {
            sk.pp = 1;
        }
        call_effect(b.ctx(), 87, {0, 1});
        bool all_full = true;
        for (const Skills& sk : b.ctx().getPet(0).skills) {
            all_full = all_full && (sk.pp == sk.maxPP);
        }
        CHECK(all_full, "C4) 恢复自身所有 PP");
        std::cout << "  [C4] 永恒 effect 87：全部技能 PP 恢复满\n";
    }

    //========== C5 effect 29：额外附加固定伤害 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        const int h = b.ctx().getPet(1).hp;
        call_effect(b.ctx(), 29, {0, 1, /*值=*/200});
        CHECK_EQ(h - b.ctx().getPet(1).hp, 200, "C5) 元素 effect 29：附加 200 点固定伤害");
        std::cout << "  [C5] 元素 effect 29：对手掉 " << (h - b.ctx().getPet(1).hp) << "\n";
    }

    //========== C6 ★ effect 28：降低对方 1/{0} 的 hp ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        const int h = b.ctx().getPet(1).hp;
        call_effect(b.ctx(), 28, {0, 1, /*分母=*/8});
        // ★ 用 PERCENT_VALUE 传精确的 max/8 = 375；若用 PERCENT 只能传整数 12% → 360。
        CHECK_EQ(h - b.ctx().getPet(1).hp, kMaxHp / 8,
                 "C6) ★ 圣灵魔闪光：掉**精确的** 1/8（375），不是 12%（360）");
        std::cout << "  [C6] 圣灵魔闪光 effect 28：掉 " << (h - b.ctx().getPet(1).hp)
                  << "（精确 1/8；整数百分比口径会得 360）\n";
    }

    //========== C7 effect 22：{0}% 令对手疲惫 {1} 回合 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        call_effect(b.ctx(), 22, {0, 1, /*概率=*/100, /*回合=*/2});
        CHECK(b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)),
              "C7) 灵魂干涉：100% 令对手疲惫");
        std::cout << "  [C7] 灵魂干涉 effect 22：对手进入疲惫\n";
    }

    //========== C8 ★ effect 60：逐回合附加固定伤害 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        call_effect(b.ctx(), 60, {0, 1, /*回合=*/3, /*值=*/30});
        const int h0 = b.ctx().getPet(1).hp;
        idle_round(b);
        CHECK_EQ(h0 - b.ctx().getPet(1).hp, 30, "C8) ★ 每回合附加 30 点固定伤害");
        const int h1 = b.ctx().getPet(1).hp;
        idle_round(b);
        CHECK_EQ(h1 - b.ctx().getPet(1).hp, 30, "C8) ★ 连续两回合各附加一次");
        std::cout << "  [C8] 旋灭裂空阵 effect 60：连续两回合各掉 30\n";
    }

    //========== C9 ★ effect 77：用技能则恢复体力 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        b.ctx().getPet(0).hp = 1000;
        call_effect(b.ctx(), 77, {0, 1, /*回合=*/3, /*恢复=*/100});
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 1100, "C9) ★ 使用技能那回合恢复 100 点体力");
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 1200, "C9) ★ 每回合用技能各恢复一次");
        std::cout << "  [C9] 生命 effect 77：1000 → " << b.ctx().getPet(0).hp
                  << "（每回合用技能 +100）\n";
    }

    //========== C10 ★ effect 58：下 N 回合必定致命一击 ==========
    {
        // 对照组：没有圣光气 → crit_rate==0 的技能**不暴**
        scenario::ScenarioBattle c = make_battle();
        c.start();
        idle_round(c);
        round_with(c, /*p1_skill=*/0);
        CHECK(!c.ctx().crit_happened[1], "C10) 对照：没有圣光气 → 不暴击");
        const int plain = kMaxHp - c.ctx().getPet(0).hp;

        // 实验组：圣光气（下2回合必定致命一击）
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        call_effect(b.ctx(), 58, {1, 0, /*回合=*/2});   // p1 获得"必定暴击"
        const int before = b.ctx().getPet(0).hp;
        round_with(b, /*p1_skill=*/0);
        CHECK(b.ctx().crit_happened[1],
              "C10) ★ 圣光气 → crit_rate==0 的技能**也**必定暴击");
        const int crit = before - b.ctx().getPet(0).hp;
        CHECK(crit > plain, "C10) ★ 暴击伤害高于未暴击（" + std::to_string(crit) + " > "
                            + std::to_string(plain) + "）");
        std::cout << "  [C10] 圣光气 effect 58：未暴 " << plain << " → 必暴 " << crit << "\n";
    }

    //========== C11 ★ effect 86：属性攻击对自身必定 miss ==========
    {
        // 观察口径：p0 用**属性技能「灵魂干涉」**（100% 令对手疲惫）。
        // ① 对照：没有圣洁 → 疲惫挂上；
        // ② 实验：p1 有圣洁 → p0 的属性技能**必定打空** → 疲惫**挂不上**。
        constexpr int kLingHun = 20499;   // 灵魂干涉（属性技能）
        auto make_with_linghun = []() {
            auto p0 = scenario::make_pet(0, "圣灵谱尼",
                                         {kLingHun, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                         /*speed=*/300, /*atk=*/100, kMaxHp);
            auto p1 = scenario::make_pet(1, "对手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                         /*speed=*/100, /*atk=*/100, kMaxHp);
            return scenario::ScenarioBattle(scenario::make_party(p0),
                                            scenario::make_party(p1));
        };
        const int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);

        scenario::ScenarioBattle ctrl = make_with_linghun();
        ctrl.start();
        idle_round(ctrl);
        ctrl.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 灵魂干涉
        ctrl.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctrl.drive();
        CHECK(ctrl.ctx().has_active_abnormal_status(1, kFatigue),
              "C11) 对照：没有圣洁 → 属性技能命中，对手进入疲惫");

        scenario::ScenarioBattle b = make_with_linghun();
        b.start();
        idle_round(b);
        call_effect(b.ctx(), 86, {1, 0, /*回合=*/5});
        idle_round(b);   // 让 ROUND_START 的回合效果置上 ws 标志
        CHECK(b.ctx().ws.attribute_must_miss[1], "C11) 圣洁：被保护方的标志已置位");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 灵魂干涉
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!b.ctx().has_active_abnormal_status(1, kFatigue),
              "C11) ★ 圣洁：p0 的属性技能**必定打空** → 疲惫挂不上");
        std::cout << "  [C11] 圣洁 effect 86：对照挂上疲惫 / 带圣洁时打空（疲惫未挂上）\n";
    }

    //========== C12 ★ effect 52：先手则令对手技能失效 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        call_effect(b.ctx(), 52, {0, 1, /*回合=*/2});
        // p0 速度 300 > p1 100 → p0 先手 → 本回合 p1 的技能应被拦下（伤害 0）
        const int before = b.ctx().getPet(0).hp;
        round_with(b, /*p1_skill=*/0);   // p1 全力一击
        const int taken = before - b.ctx().getPet(0).hp;
        CHECK_EQ(taken, 0, "C12) ★ 虚无：本方先手 → 对手技能失效（全力一击打不出伤害）");
        std::cout << "  [C12] 虚无 effect 52：p0 先手 → p1 技能失效，掉 " << taken << "\n";
    }

    //========== C13 ★ effect 34：弹伤 = 受到的红伤 × 倍率，直写 hp（不管对方减伤）==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        // p1 先打 p0 一回合 → 管线结果里 defender=p0，final 就是本回合受到的红伤
        round_with(b, /*p1_skill=*/0);
        const int taken = b.ctx().resolvedDamage.final;
        CHECK(taken > 0, "C13) 前置：本回合 p0 挨了红伤");
        const int p1_before = b.ctx().getPet(1).hp;
        call_effect(b.ctx(), 34, {0, 1, /*倍率=*/2});
        const int reflected = p1_before - b.ctx().getPet(1).hp;
        CHECK_EQ(reflected, taken * 2, "C13) ★ 弹伤 = 受到的红伤 × 2");
        std::cout << "  [C13] 能量 effect 34：受到 " << taken << " → 反弹 " << reflected
                  << "（×2，直写 hp）\n";

        // ★ 自己**先出手**时：管线里还没有对手造成的伤害 → 弹 0
        scenario::ScenarioBattle c = make_battle();
        c.start();
        idle_round(c);
        c.ctx().ws.resolvedDamage = DamageSnapshot{};   // 模拟"本回合还没人造成伤害"
        const int h = c.ctx().getPet(1).hp;
        call_effect(c.ctx(), 34, {0, 1, 2});
        CHECK_EQ(h - c.ctx().getPet(1).hp, 0,
                 "C13) ★ 自己先出手（管线里还没有对手造成的伤害）→ 弹 0");
        std::cout << "  [C13] 能量 effect 34：先出手时弹 0\n";
    }

    //========== C14 ★ effect 816：免疫 N 次攻击伤害 + 附加**免疫量**的真实伤害 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        call_effect(b.ctx(), 816, {0, 1, /*次数=*/1});
        // p1 打 p0：p0 免疫本次伤害，并把**免疫量**以真伤还给 p1
        const int p0_before = b.ctx().getPet(0).hp;
        const int p1_before = b.ctx().getPet(1).hp;
        round_with(b, /*p1_skill=*/0);
        CHECK_EQ(b.ctx().getPet(0).hp, p0_before, "C14) 免疫下 1 次攻击技能伤害 → p0 未掉血");
        CHECK(b.ctx().getPet(1).hp < p1_before,
              "C14) ★ 附加**免疫量**的真实伤害给攻击方");
        const int reflected = p1_before - b.ctx().getPet(1).hp;
        CHECK(reflected > 0, "C14) 反弹量 > 0");
        std::cout << "  [C14] 弦月锋华 effect 816：p0 免疫（掉 0）→ p1 被真伤 " << reflected << "\n";

        // 次数用完 → 第二次不再免疫
        const int p0b = b.ctx().getPet(0).hp;
        round_with(b, /*p1_skill=*/0);
        CHECK(b.ctx().getPet(0).hp < p0b, "C14) 次数用完 → 第二次攻击照常造成伤害");
        std::cout << "  [C14] 弦月锋华 effect 816：次数用完 → 第二次掉 "
                  << (p0b - b.ctx().getPet(0).hp) << "（不再免疫）\n";
    }

    //========== C15 同生共死（effect 7）：老代码直写 hp = 真伤，不吃任何减免 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        b.ctx().getPet(0).hp = 1000;                       // 自己 1000
        b.ctx().getPet(1).hood_bank_.add(0, 100000, 7, 0); // 对手厚护罩
        b.ctx().pink_reduce_pct[1] = 90;                   // 对手高额减粉
        call_effect(b.ctx(), 7, {0, 1});   // effect 7 = 同生共死（走 TRUE 伤害）
        CHECK_EQ(b.ctx().getPet(1).hp, 1000,
                 "C15) ★ 真伤直通护罩/减粉 → 体力被拉到与自己相同（护罩没参与）");
        std::cout << "  [C15] 同生共死（effect 7，真伤口径）：对手体力 → "
                  << b.ctx().getPet(1).hp << "（厚护罩 + 90% 减粉都不参与）\n";
    }

    //========== C16 ★ 蚀砚之泪≥4滴 → 挡伤/弹伤失效（掩码抑制）==========
    {
        // 掩码的真相源 = `ElfPet::damage_suppress_mask`（bit = DamageEffectCategory）。
        // 管线里的条目由 `DamagePipeline::run` 按类别机械跳过（零两两硬编码）；
        // **不在管线里**的时点桶效果（34 能量这种）必须**自检**同一个掩码。
        constexpr int kSuppressBits =
            (1 << static_cast<int>(DamageEffectCategory::BLOCK))
          | (1 << static_cast<int>(DamageEffectCategory::DETECT));

        // ① 34 弹伤（时点桶效果 → 自检）
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        round_with(b, /*p1_skill=*/0);          // 让 p0 先挨一下，管线里有可弹的量
        CHECK(b.ctx().resolvedDamage.final > 0, "C16) 前置：p0 本回合挨了红伤");
        b.ctx().getPet(0).damage_suppress_mask |= kSuppressBits;   // 等价于挂了 ≥4 滴泪
        const int h = b.ctx().getPet(1).hp;
        call_effect(b.ctx(), 34, {0, 1, /*倍率=*/2});
        CHECK_EQ(h - b.ctx().getPet(1).hp, 0,
                 "C16) ★ 泪≥4滴 → 34 弹伤失效（自检掩码，时点桶效果绕不过去）");
        std::cout << "  [C16] 泪≥4滴：能量 effect 34 弹 " << (h - b.ctx().getPet(1).hp)
                  << "（被抑制）\n";

        // ② 816（注册在 `DamagePhase::BLOCK` → 管线自动跳过）
        scenario::ScenarioBattle c = make_battle();
        c.start();
        idle_round(c);
        call_effect(c.ctx(), 816, {0, 1, /*次数=*/1});
        c.ctx().getPet(0).damage_suppress_mask |= kSuppressBits;   // 先挂泪
        const int c0 = c.ctx().getPet(0).hp;
        round_with(c, /*p1_skill=*/0);
        CHECK(c.ctx().getPet(0).hp < c0,
              "C16) ★ 泪≥4滴 → 管线里的 816 被跳过（不免疫、照常掉血）");

        // ③ 撤掉泪 → 816 仍然有效 ⇒ 次数**没被消耗**
        c.ctx().getPet(0).damage_suppress_mask &= ~kSuppressBits;
        const int c1 = c.ctx().getPet(0).hp;
        round_with(c, /*p1_skill=*/0);
        CHECK_EQ(c.ctx().getPet(0).hp, c1,
                 "C16) ★ 撤掉泪 → 816 照常免疫 ⇒ 被抑制期间**次数没被消耗**");
        std::cout << "  [C16] 泪≥4滴：816 被跳过（该回合掉 " << (c0 - c1)
                  << "）；撤掉泪后仍能免疫（次数未被消耗）\n";
    }

    std::cout << "\n场景 053 通过：谱尼 12 条经典基础技能全部落地——\n"
                 "  伤害类（29/28/60）走粉伤管线；恢复类（43/77/87）走 heal 原语或直写 PP；\n"
                 "  能力类（3/33）走 clear_stat_drops / clear_stat_boosts；\n"
                 "  状态类（22 疲惫 / 58 必暴 / 86 属性必miss / 52 先手失效）各就各位。\n";
    return 0;
}
