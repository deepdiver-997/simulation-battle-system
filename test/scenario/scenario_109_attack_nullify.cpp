// 场景 109 —— 「攻击技能无法造成伤害且命中效果失效」票（effect 1090 族 / RuleCenter
// ATTACK_NULLIFY，2026-09-21 用户实测口径锁定）
//
// 机制要点（用户逐条口径，引擎落点见各断言）：
//   * **绑定 ≠ 生效**：票挂谁身上（bearer=source_owner，icon/生命周期/断回合 epoch 跟谁）
//     与压制谁打出的攻击（target=生效目标）是两个独立字段——
//       缭乱式：bearer=对手、target=对手（挂对手身上压其攻击）；
//       缔笙光环式：bearer=自己、target=对手（挂自己身上的防御光环，压对面打来的攻击）。
//   * 红伤在伤害管线**之前**归零 → **护盾/护罩不消耗**；该攻击的效果**粉伤**同样封锁
//     （deal_pink_damage 入口查 actor，强制执行放行的粉伤效果也不生效）；真伤不封。
//   * 命中效果失效走 ③层（kFullNull 白板），**强制执行绕过**——无相谛（威力 0）的强制
//     断回合在第一趟结算里先拆掉缔笙的光环（SKILL_EFFECT 时点删票），变威力重结算时
//     票已不在 → 正常伤害。
//   * 生命周期 = 回合类窗口 + bearer 的 epoch 版本号（断回合 bump 后即失效）。
//
// 宿主技能（缭乱/缔笙 35945/无为觉者 37460）未落地，本场景直接调 grant 原语锁机制。

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10162;      // 飞空俯冲（必中攻击 90）
constexpr int kGaoSu = 20017;    // 高速移动（威力 0 属性技，被攻击方用）
constexpr int kZhuangJi = 10001;

std::array<int, 5> same(int id) { return {id, id, id, id, id}; }

ElfPet attacker(int speed, int atk) {
    return make_pet(4661, "攻方", same(kHit), speed, atk, 9000);
}

ElfPet receiver(int speed) {
    return make_pet(1, "受方", same(kGaoSu), speed, 100, 9000);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A 缭乱式（bearer=攻方自己）+ 护盾不消耗：红伤管线前归零
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(receiver(/*speed=*/300)),
                         make_party(attacker(/*speed=*/100, /*atk=*/500)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).shield = 400;   // 受方挂盾：若归零发生在护盾之后，盾会被消耗
        // 缭乱式：票挂在 1 号方（被缭乱的一方）身上，压它自己打出的攻击
        const int sid = ctx.rule_center_.grant_attack_nullify(
            /*source_owner=*/1, /*source_effect_id=*/109001, /*target=*/1,
            /*rounds=*/2, /*register_round=*/ctx.roundCount);
        CHECK(sid > 0, "A) 前置：票授予成功");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 9000, "A1) ★ 被压制方的攻击伤害归零");
        CHECK_EQ(ctx.getPet(0).shield, 400,
                 "A2) ★ 护盾**不消耗**（归零发生在伤害管线介入护盾之前）");
        std::cout << "  [A] 缭乱式·压制其攻击且护盾不消耗 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B 效果粉伤同步封锁（强制执行的粉伤效果也不生效；真伤不封）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(receiver(100)), make_party(attacker(100, 500)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int sid = ctx.rule_center_.grant_attack_nullify(
            1, 109002, 1, /*rounds=*/2, ctx.roundCount);
        CHECK_EQ(ctx.getPet(0).hp, 9000, "B) 前置：满血");
        CHECK_EQ(deal_pink_damage(&ctx, /*target=*/0, /*amount=*/100,
                                  DamageKind::FIXED, /*actor=*/1),
                 FixedDamageResult::SUCCESS,
                 "B) 前置：粉伤原语被调用（返回已处理）");
        CHECK_EQ(ctx.getPet(0).hp, 9000, "B1) ★ 被压制方的攻击效果粉伤被封锁（红粉双封）");
        ctx.rule_center_.revoke(sid);
        CHECK_EQ(deal_pink_damage(&ctx, 0, 100, DamageKind::FIXED, 1),
                 FixedDamageResult::SUCCESS, "B) 前置：撤票后粉伤原语照常");
        CHECK_EQ(ctx.getPet(0).hp, 8900, "B2) ★ 撤票后粉伤正常落地（封锁只跟票走）");
        std::cout << "  [B] 攻击效果粉伤同步封锁 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C 缔笙光环式（bearer=自己、压对面）+ 强制断回合拆光环 → 伤害恢复
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(receiver(/*speed=*/300)),
                         make_party(attacker(/*speed=*/100, /*atk=*/500)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 光环：缔笙（0 号方）挂自己身上，压 1 号方打来的攻击；epoch 绑 bearer 当前版本
        ctx.rule_center_.grant_attack_nullify(
            /*source_owner=*/0, /*source_effect_id=*/109003, /*target=*/1,
            /*rounds=*/3, ctx.roundCount, EffectScope::ON_STAGE,
            /*source_valid_id=*/ctx.round_effect_valid_id[0]);
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, hp_before, "C1) ★ 光环生效：对面打来的攻击伤害归零");

        // 无相谛式强制断回合（force 效果在真实对局里走 ③层绕过；这里直调内核等价操作）
        ctx.invalidate_all_round_effects(/*robotId=*/0);
        CHECK(!ctx.rule_center_.is_attack_nullified(1, ctx.roundCount),
              "C2) ★ 断回合 bump bearer 的 epoch → 光环票作废（查询不再命中）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp < hp_before,
              "C3) ★ 光环被拆后伤害正常结算（变威力重结算打得出伤害）");
        std::cout << "  [C] 缔笙光环式·强制断回合拆光环后恢复伤害 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D 回合类窗口过期
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(receiver(300)), make_party(attacker(100, 500)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.rule_center_.grant_attack_nullify(1, 109004, 1, /*rounds=*/1, ctx.roundCount);
        CHECK(ctx.rule_center_.is_attack_nullified(1, ctx.roundCount),
              "D) 前置：授予当回合内生效");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // 推进到下一回合
        CHECK(!ctx.rule_center_.is_attack_nullified(1, ctx.roundCount),
              "D1) ★ 1 回合窗口过期后不再压制");
        std::cout << "  [D] 回合类窗口过期 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E 回合类锁切（2088 同款）绑定对手：被断回合拆掉 + 计入"有无可断物"
    //   （2026-09-21 口径：回合类附加的锁切，切换/被断回合都失效）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(receiver(300)), make_party(attacker(100, 500)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 锁切票挂 1 号方（被锁方=绑定方），NextRounds 起点、epoch=被锁方当前版本号
        ctx.rule_center_.grant_switch_lock(
            /*source_owner=*/0, /*source_effect_id=*/2088, /*target=*/1,
            /*rounds=*/1, /*register_round=*/ctx.roundCount + 1, EffectScope::ON_STAGE,
            /*source_valid_id=*/ctx.round_effect_valid_id[1]);
        CHECK(ctx.rule_center_.is_switch_locked(1, ctx.roundCount + 1),
              "E) 前置：下一回合锁切生效");
        CHECK(ctx.rule_center_.has_round_type(1),
              "E1) ★ 锁切计入被锁方的\"有无可断物\"（回合类效果）");
        // 对手被断回合（阅世/1808 同款原语路径）→ 锁被一并拆掉
        ctx.invalidate_all_round_effects(/*robotId=*/1);
        ctx.rule_center_.clear_round_type(/*source=*/1);
        CHECK(!ctx.rule_center_.is_switch_locked(1, ctx.roundCount + 1),
              "E2) ★ 断回合后锁切失效（绑定方版本号 bump → 票作废）");
        std::cout << "  [E] 回合类锁切·断回合可拆 通过\n";
    }

    std::cout << "scenario_109 PASS（ATTACK_NULLIFY：缭乱式/缔笙光环式/护盾不消耗/红粉双封/"
                 "断回合拆 aura/窗口过期 + 锁切断回合可拆）" << std::endl;
    return 0;
}
