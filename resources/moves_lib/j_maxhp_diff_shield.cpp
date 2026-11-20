// J 组 J295 · 最大体力差护盾族 —— 1871×7（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1871（0 参）"若自身当前体力低于对手则为自身附加等同于双方最大体力差值的护盾"
//
// 实现口径：
//   - 门：SKILL_EFFECT 时点读 **当前体力**，自身 hp **严格低于** 对手 hp 才授予
//     （相等不授予；"低于"口径与 J125 541 / 704 同款严格读法）。
//   - 量 = **双方最大体力差值**（绝对差）：|自我 maxHP − 对手 maxHP|。
//     maxHP 读 numericalBase[HP]（缺失时逐级回退 numericalProperties/hp——burn_trio
//     pet_max_hp_of 同款，跨 .so 本地一份）。
//   - 落点 = shield_bank_.add(priority=0, amount, source=187100+owner, duration=0)
//     ——普通来源护盾（J125 2005 / 1751 同通道；只响应红伤 NORMAL）。
//   - 审计槽：991871100+owner（差值/授予量）、991871200+owner（门 0/1），场景精确断言。
//
// 载体切片（0 参效果，装载器注入 [owner,target]；7/7 自洽，无行参）：
//   m28499 地动检索 [1871]（**纯链，测试载体**）/ m28112 传承意志 / m28593 穹辉庇佑 /
//   m29028 螺壳曳光 / m29462 欢声庆语 / m36661 不懈求索 / m37272 钢能轰击破。
//
// 动工前置三查（2026-10-22）：①内核 grep 1871 无行为硬编码（仅 boss_invalid_effects
//   元数据表命中，PvE 标记不参与派发）②effect_info 仅本条模板 ③moves_lib/soul_lib
//   并集（jbatch/kbatch 各 worktree）无注册——真缺口。
//
// 场景回归：test/scenario/scenario_6280_maxhp_diff_shield.cpp（号段 6280-6289）。

#include "common.h"

namespace {

constexpr int k1871AmountBase = 991871100;   // +owner：本次差值/授予量
constexpr int k1871GateBase = 991871200;     // +owner：门结果 1/0

// 最大体力（本体优先逐级回退；burn_trio pet_max_hp_of 同款）。
int j295_max_hp_of(const ElfPet& pet) {
    int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = pet.numericalProperties[NumericalPropertyIndex::HP];
    }
    if (max_hp <= 0) {
        max_hp = pet.hp;
    }
    return max_hp;
}

void j295_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

}  // namespace

// ── 1871 —— 自身当前体力低于对手则为自身附加双方最大体力差值的护盾 ──
// int_args = [owner, target]（0 参效果，装载器注入）。
EffectResult effect_1871_maxhp_diff_shield(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1871, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1871, args,
                     "1871 布局=[owner,target]（0 参效果）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1871, args, "需要 owner/target ∈ {0,1}");
    const ElfPet& self = ctx->getPet(owner);
    const ElfPet& foe = ctx->getPet(opp);
    // 门：当前体力**严格低于**对手（相等不授予）。
    if (self.hp >= foe.hp) {
        j295_set_storage(ctx, k1871GateBase + owner, 0);
        j295_set_storage(ctx, k1871AmountBase + owner, 0);
        return EffectResult::kOk;
    }
    j295_set_storage(ctx, k1871GateBase + owner, 1);
    const int diff = j295_max_hp_of(self) - j295_max_hp_of(foe);
    const int amount = diff >= 0 ? diff : -diff;   // "双方最大体力差值" = 绝对差
    j295_set_storage(ctx, k1871AmountBase + owner, amount);
    if (amount <= 0) {
        return EffectResult::kOk;   // 差值 0 = 无盾可给（同 maxHP 的不同精灵）
    }
    // 普通来源护盾（priority 0；source 锚 owner 防跨侧同源混淆）。
    ctx->getPet(owner).shield_bank_.add(/*priority=*/0, amount, /*source_id=*/187100 + owner,
                                        /*duration=*/0);
    return EffectResult::kOk;
}

SKILL_EFFECT(1871, effect_1871_maxhp_diff_shield)
