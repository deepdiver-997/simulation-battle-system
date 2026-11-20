// K423 普鲁修托 4489 收尾批 —— 咒终·阴阳辞标记消费端（1946/1947 两宏落地）+ 72 内核缺口记档
// （看板 K423，a01-k423 2026-11-07，worktree ~/.agent-workspaces/workspaces/sbs-local/k423，
//  分支 feat/k423；场景号段 8060-8069（tasklog alloc 2026-11-07T03:31:35 a01-k423），
//  场景 test/scenario/scenario_8060_puluxiutuo_pool.cpp。
//  选载体依据：K418 头注遗留记档（1946/1947 = 派发段外标记消费端，"本文件槽键段即
//  其对接面"）+ 散号 72 缺口；本文件 = 该记档的对接落地。registry 基线
//  （2026-11-07 主库 dump 现查）：moves=1452 / soul=411。）
//
// 行清单（官方现查 effect_info.info 全文 2026-11-07；side_effect.arg_count 与
// effect_info.args_num 逐位一致；切片零剩余；两载体 friend_side_effect 全空）：
//   1946 [1] "技能威力提升{0}%，若自身未持有回能标记则提升效果翻倍"
//        载体：咒终·阴阳辞 36508[494,1750,1946,1947][50,200]（494/1750 零参，
//        1946 切片 [50]、1947 切片 [200]；36508 = 4489 extra_moves.move
//        learning_lv 76——monsters.raw_json extra_moves 现查、monster_learnable_moves
//        无 36508 行——全库唯一）
//   1947 [1] "附加给对手{0}点固定伤害，若对手未持有失能标记则附加的固定伤害翻倍"
//        载体：咒终·阴阳辞 36508（1947 切片 [200]；专属）
//   72   [0] "若没有命中对手则自己失去全部体力"（**记档不产码**，见"72 内核缺口记档"）
//        载体：无为·魂切 36506[36,72][100]（36 切片 [100]、72 零参切片；
//        36506 = 4489 monster_learnable_moves learning_lv 53 全库唯一）
//   载体技静态（moves 行现查）：36508 咒终·阴阳辞 = 69 系/特攻/威力160/命中95/
//   must_hit=1（永不 miss，1946/1947 每次施放必跑）；36506 无为·魂切 = 69 系/物攻/
//   威力0/命中50/must_hit=0/PP1（可真 miss，72 的触发面）。
//
// 动工前置四查（2026-11-07，本 worktree + 主库现查）：
//   ① 内核 grep 72/1946/1947：src/ include/ 零命中（逐词 -w 与宽松子串双过，均 rc=1
//     零行），零行为硬编码；
//   ② effect_info 3/3 在列且 args_num == side_effect.arg_count == 切片消耗
//     （72|0、1946|1、1947|1 逐位 OK；36506 [36,72][100] → 36[1]+72[0]=100、
//     36508 [494,1750,1946,1947][50,200] → 494[0]+1750[0]+1946[1]+1947[1]=50+200，
//     零剩余）；custom_effect_programs / custom_effect_overrides 对 3 id 零命中；
//   ③ 全库并集无注册：主库 registry moves=1452 / soul=411 无 3 id；moves_lib/soul_lib
//     SKILL_EFFECT/SOUL_PROGRAM 精确 grep（主库 + 全部 worktree k422/k423/agents/
//     night-fuzz）零命中——真缺口，非撞号；boss_invalid_effects.json 命中 1947 一条
//     ——溯源 = **魂印空间** effect_icon id 1947 行（tips=「入梦」族，与 moves 空间
//     1946/1947 语义无关；SKILL_EFFECT 与 SOUL_MARK 两注册空间互不相干，K418 同款
//     口径）；链上 494（无视对手提升）/1750（无视自身下降）均已注册
//     （e_ignore_boost.cpp / j_negative_view_rewrite.cpp）——36508 全链本单补完后
//     无缺口；
//   ④ 废弃 parser 覆盖面（2026-11-05 冻结）：三条模板对 parse_status_inflict_primary
//     四 pattern（令/使+对手/自身+{）零命中；第二刀 179/700/1083/1257/456 五条件
//     模板零命中（1946 含"威力提升"但无"若属性相同"前件）；"附加…固定伤害" matcher
//     只活在 parse_fallback_unit（仅状态主句 + "未触发则"分支可达，1947 行文两前提
//     皆无）→ 3 id 全 NONE 安全跳过（无 K416 1762 型寄生）。本单注册后 clone_effect
//     先命中、parser 通道对 1946/1947 关闭；72 保持 NONE（缺口记档，现状无效果）。
//
// 落地口径：
//   - 标记功能槽 = K418 建的 side 级契约，本文件是**消费端**（K418 头注"1946/1947 …
//     本文件槽键段即其对接面"记档的对接落地，与 J331 1940/1941 消费端同构）：
//     plugin_storage[4696000+side]=回能Ⅰ到期回合（1946 读 **自身侧**）
//     plugin_storage[4696010+side]=失能Ⅰ到期回合（1947 读 **对手侧**）
//     判持有 = 值>0 且 >= 当前回合数（0/缺=未持有；**到期残留条目不判持有**，
//     K418 k418_marker_held / J331 j331_marker_held 同式）；本文件对两槽**只读**，
//     不擦不写（官方行文无"解放/附加标记"字样）。跨 dylib 共用键
//     4696000/4696010 已在 tools/check_storage_keys.py INTENTIONAL_SHARED 备案。
//   - 1946 翻倍语义 = "提升效果翻倍"：翻倍作用于**提升量**而非最终威力——view ×
//     (100+2×{0})/100（{0}=50 切片 → 单倍 ×1.5 / 翻倍 ×2.0）。写 ws.skill_power_view
//     （J185 1953 同款 SKILL_EFFECT 时点 view 改写，ATTACK_DAMAGE 阶段消费）。
//   - 1947 翻倍语义 = "附加的固定伤害翻倍"：{0}=200 切片 → 单倍 200 / 翻倍 400。
//     落点走 fixed_damage 原语（J144 515/685"附加{0}点固定伤害"同式 SKILL_EFFECT
//     即时挂——固伤不吃 final、无伤害阈值，不走 DETECT 工单口径；吃护盾/事件）。
//   - 反直觉设计注记：1946/1947 都是"**未持有**标记才翻倍"——官方平衡点在"挂标记
//     （1944/1945）换先制门（1940/1941）与解放异常（1942/1943），咒终·阴阳辞则奖励
//     裸奔"；实现按模板字面，不做价值判断。
//   - 审计槽 +owner 侧别键（J331 同款）：双侧同回合各持咒终·阴阳辞时审计互不覆盖。
//
// 72 内核缺口记档（不产码 → ✅ 已由 J335 落地，2026-11-07：内核 case 72 一行
//   src/entities/skills.cpp default_branch_for_effect + 插件自灭体
//   j335_miss_self_destruct.cpp + 场景 8090 / 8060 T8060f 翻正——下段为缺口期
//   记档原文，留档佐证）：
//   - 官方语义明确：「若没有命中对手则自己失去全部体力」= miss 路径自灭——136
//     「若Miss则自己恢复1/{0}体力」的同族反相，skills.cpp default_branch_for_effect
//     注释自证 136 属 SKILL_INVALID 分支（miss/被封趟照常注册）。
//   - 缺口：该分支归属是**内核硬编码 switch**（default_branch_for_effect，72 不在列
//     → 默认 HIT 分支）——miss 趟 register_branch(SKILL_INVALID) 只注册
//     SKILL_INVALID 分支效果，72 永不注册；插件侧 SKILL_EFFECT(72) 注册了也只在
//     命中趟跑（命中趟"没有命中"恒假 = 白注册）。→ 补齐 = 内核一行
//     （case 72: return SkillExecResult::SKILL_INVALID;）+ 插件 SKILL_EFFECT(72)
//     miss 趟自灭体（hp=0 走原语不直写），两件套须同发；本单 src/ 冻结 → 如实
//     记档，留内核微任务（授权后由续单完成）。
//   - 现状行为：36506 miss 趟无任何效果（36 = HIT 分支 on-hit 秒杀掷骰不跑；72 无
//     注册）；场景 8060 以失明门做确定性 miss 探针把"缺口现状"锁进回归——若未来
//     内核补 case，本断言翻红提醒改写（fail-loud 记档）。
//
// 存储键段（新增前已 grep 全库无撞；tools/check_storage_keys.py 门禁兜底）：
//   - 审计槽 10<id>000x 段（K416/K417/K418/J331 同款）：
//     1019460001(+owner)=1946 判持有位（1=自身持有回能标记→单倍 / 0=未持有→翻倍）
//     1019460002(+owner)=1946 实际提升量 pct（50/100）
//     1019470001(+owner)=1947 判持有位（1=对手持有失能标记→单倍 / 0=未持有→翻倍）
//     1019470002(+owner)=1947 实际固伤量（200/400）
//     1019470003(+owner)=1947 fixed_damage 返回枚举（FixedDamageResult 序值）
//   - 功能槽：4696000/4696010 段只读消费（INTENTIONAL_SHARED 备案，K418 写端）。
//
// 先例：J185 1953（SKILL_EFFECT view 改写威力，j_dropcond_power_up.cpp）、
//   J144 515/685（附加固伤 SKILL_EFFECT 即时，j_cond_fixed.cpp）、
//   K418 k418_marker_held / J331 j331_marker_held（槽契约+判持有式）。
// 场景回归：test/scenario/scenario_8060_puluxiutuo_pool.cpp（号段 8060-8069）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── 审计槽（+owner 侧别键）───────────────────────────────────────────────
constexpr int k1946HeldAudit   = 1019460001;   // 1=持有（单倍） 0=未持有（翻倍）
constexpr int k1946PctAudit    = 1019460002;   // 实际提升量 pct
constexpr int k1947HeldAudit   = 1019470001;   // 1=持有（单倍） 0=未持有（翻倍）
constexpr int k1947AmountAudit = 1019470002;   // 实际固伤量
constexpr int k1947ResultAudit = 1019470003;   // FixedDamageResult 序值

// ── 功能槽（K418 段契约，只读消费）────────────────────────────────────────
constexpr int kHuiNengBase = 4696000;   // +side：回能Ⅰ到期回合（K418 写端）
constexpr int kShiNengBase = 4696010;   // +side：失能Ⅰ到期回合（K418 写端）

// 标记持有判定（K418/J331 同式：写入过且未到期；到期残留条目不判持有）。
bool k423_marker_held(const BattleContext* ctx, int base, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    const auto it = ctx->plugin_storage.find(base + side);
    if (it == ctx->plugin_storage.end()) {
        return false;
    }
    const int expiry = any_int_or(it->second, 0);
    return expiry > 0 && expiry >= ctx->roundCount;
}

// ═══════════════ 1946 威力提升{0}%（未持有回能标记则提升效果翻倍）═════════
EffectResult effect_1946_power_amp_marker_gate(BattleContext* ctx,
                                               const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1946, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1946, args,
                     "1946 布局=[owner,target,威力提升%]");
    const int owner = args.int_args[0];
    const int base_pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1946, args, "需要 owner∈{0,1}");
    EFFECT_ARG_GUARD(base_pct > 0, 1946, args, "需要 提升%>0");
    const bool held = k423_marker_held(ctx, kHuiNengBase, owner);
    const int pct = held ? base_pct : base_pct * 2;   // "提升效果翻倍"作用于提升量
    auto& view = ctx->ws.skill_power_view[owner];
    view = view * (100 + pct) / 100;
    ctx->plugin_storage[k1946HeldAudit + owner] = held ? 1 : 0;
    ctx->plugin_storage[k1946PctAudit + owner] = pct;
    return EffectResult::kOk;
}

// ═══════════════ 1947 附加{0}点固定伤害（对手未持失能标记则翻倍）══════════
EffectResult effect_1947_fixed_damage_marker_gate(BattleContext* ctx,
                                                  const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1947, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1947, args,
                     "1947 布局=[owner,target,固定伤害点数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int base_fixed = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner,
                     1947, args, "需要 owner/target ∈ {0,1} 且互异");
    EFFECT_ARG_GUARD(base_fixed > 0, 1947, args, "需要 固伤>0");
    const bool held = k423_marker_held(ctx, kShiNengBase, opp);
    const int amount = held ? base_fixed : base_fixed * 2;   // "附加的固定伤害翻倍"
    ctx->plugin_storage[k1947HeldAudit + owner] = held ? 1 : 0;
    ctx->plugin_storage[k1947AmountAudit + owner] = amount;
    if (g_core && g_core->fixed_damage) {
        const FixedDamageResult r = g_core->fixed_damage(ctx, opp, amount);
        ctx->plugin_storage[k1947ResultAudit + owner] = static_cast<int>(r);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(1946, effect_1946_power_amp_marker_gate)
SKILL_EFFECT(1947, effect_1947_fixed_damage_marker_gate)
