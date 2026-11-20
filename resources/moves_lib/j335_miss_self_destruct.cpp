// J335 · case 72 miss 自灭体 —— 72「若没有命中对手则自己失去全部体力」（1 宏落地，
// K423 记档续单；内核配套 = src/entities/skills.cpp default_branch_for_effect
// case 72 → SKILL_INVALID 一行，两件套须同发）
// （看板 J335，a01-j335 2026-11-07，worktree ~/.agent-workspaces/workspaces/sbs-local/j335，
//  分支 feat/j335；场景号段 8090-8099，场景 test/scenario/scenario_8090_miss_self_destruct.cpp
//  + 场景 8060 T8060f 探针翻正（K423 fail-loud 记档的缺口闭合验收）。）
//
// 行清单（官方现查 effect_info.info 全文；args_num=0=side_effect.arg_count；K423 头注
//   行清单同源，本单复验）：
//   72 [0] "若没有命中对手则自己失去全部体力"
//        载体：无为·魂切 36506[36,72][100]（36 切片 [100]、72 零参切片；
//        36506 = 4489 普鲁修托 monster_learnable_moves learning_lv 53 全库唯一）
//        载体技静态（moves 行现查，K423 记档同文）：69 系/物攻/威力 0/命中 50/
//        must_hit=0/PP1（可真 miss，72 的触发面）。
//
// 动工前置四查（2026-11-07，本 worktree + 主库现查；K423 头注四查同结论复验）：
//   ① 内核 grep 72：本单前 src/ include/ 零行为硬编码（K423 四查 rc=1）；本单
//     按授权在 default_branch_for_effect 补 case 72 一行（语义归属，非行为实现）；
//   ② effect_info 在列（args_num=0=side_effect.arg_count=切片消耗 0）；
//     custom_effect_programs / custom_effect_overrides 零命中；
//   ③ 全库并集无注册：registry moves=1456 无 72；resources/ SKILL_EFFECT(72 /
//     SOUL_PROGRAM(72 零命中（K423 记档"缺口不产码"与现查一致）——真缺口，非撞号；
//   ④ 废弃 parser 覆盖面：模板无"令/使+对手/自身+{"直随花括号形态、无 % 占位
//     （K423 头注 ④ 同文）→ 72 保持 NONE 安全跳过；本单注册后 clone_effect 先命中。
//
// 语义考证（对派发理解的复核，结论一致）：
//   - 官方语义「若没有命中对手则自己失去全部体力」= **未命中趟自灭**：136「若Miss
//     则自己恢复1/{0}体力」的同族反相（burn_trio_skills.cpp effect_136_miss_heal，
//     SKILL_INVALID 分支）；136 族口径 miss/被封/盔挡下同趟（136 注释"盔挡下同算，
//     口径同 2126"）——"没有命中对手"在这些趟恒真。命中趟不注册（SKILL_INVALID
//     单桶），72 零条件、miss 趟即无条件自灭。
//   - 「失去全部体力」= 对己 fatal（自己倒下）：体力消耗语义归 DefeatCause::
//     HP_CONSUME（自爆族，spirit_lifecycle.h——不响应免疫秒杀、穿残留免死）；
//     hp<=0 生存门在先（已死/被反弹打死则自灭无从谈起，59/2213/1276 同式守卫），
//     hp 归零在后、defeat_pet 死亡漏斗收尾（登记 EVENT_DEATH）。actor=对手侧
//     （1-owner，与帝王自爆 self_destruct_hp_consume / 59 / 2213 同口径）。
//   - 不直写 hp 的通道核查：本文件 hp=0 写法沿 59/2213/1275/1276/1551 自爆族
//     既定形态（"体力修正通道"——hp=0 + defeat_pet 漏斗，套路目录 §一口径的
//     自爆族例外形态，先例密度全库最高）；fixed_damage 对己不取（固伤吃护盾/
//     事件面，与"失去全部体力"语义不符——753/1276 自爆族均不走固伤通道）。
//
// 存储键段（新增前已 grep 全库无撞；tools/check_storage_keys.py 门禁兜底）：
//   - 审计槽 10<id>000x 段（J331/K423 同款）：
//     1007200001(+owner)=72 自灭执行位（1=hp 已归零走漏斗；缺席=未执行）
//     1007200002(+owner)=defeat_pet 返回枚举序值（DefeatResult，登记漏斗结果）
//
// 先例：136（miss 趟 SKILL_INVALID 效果体，burn_trio_skills.cpp）、
//   59（自爆 hp=0 → defeat_pet(HP_CONSUME)，j_self_destruct_relay_boost.cpp）、
//   self_destruct_hp_consume（diwang_3512_skills.cpp，1275/753/1276 公共尾段）。
// 场景回归：test/scenario/scenario_8090_miss_self_destruct.cpp（号段 8090-8099）。

#include "common.h"

namespace {

// ── 审计槽（+owner 侧别键）───────────────────────────────────────────────
constexpr int k72ExecAudit   = 1007200001;   // 1=hp 已归零走漏斗（缺席=未执行）
constexpr int k72ResultAudit = 1007200002;   // DefeatResult 序值（漏斗登记结果）

}  // namespace

// ── 72 —— 若没有命中对手则自己失去全部体力（零参；SKILL_INVALID 分支体）──
// int_args = [owner, target]（register_branch bind_participants 绑定；零参无行参槽）。
// 本效果只在 miss/被封/盔挡下趟注册（内核 case 72 → SKILL_INVALID），命中趟不进
// 本体——"没有命中"条件由分支承载，效果体内无需再判趟别（900001 先例同口径）。
EffectResult effect_72_miss_self_destruct(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 72, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 72, args,
                     "72 布局=[owner,target]（register_branch 绑定）");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 72, args, "需要 owner∈{0,1}");
    if (!g_core || !g_core->defeat_pet) {
        return EffectResult::kOk;
    }
    const int slot = ctx->on_stage[self];
    ElfPet& me = ctx->seerRobot[self].elfPets[slot];
    if (me.hp <= 0) {
        return EffectResult::kOk;   // 生存门：已死（反弹/反伤等）——自灭无从谈起
    }
    me.hp = 0;   // 失去全部体力（体力修正通道——自爆族既定形态，59/2213/1276 同式）
    const DefeatResult r = g_core->defeat_pet(ctx, self, slot, /*actor=*/1 - self,
                                              DefeatCause::HP_CONSUME);
    ctx->plugin_storage[k72ExecAudit + self] = 1;
    ctx->plugin_storage[k72ResultAudit + self] = static_cast<int>(r);
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(72, effect_72_miss_self_destruct)
