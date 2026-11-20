// K429 · 德布比特 4499 收尾 —— 缘结·浮世诵 37021 命中期效果 2132（1 宏落地，
// 37021 链复扫全绿收官）
// （看板 K429，a01-k429 2026-11-07，worktree ~/.agent-workspaces/workspaces/sbs-local/k429，
//  分支 feat/k429；场景号段 8140-8149（派发文本指定，tasklog 该段空闲无撞），场景
//  test/scenario/scenario_8140_yuanjie_fushisong.cpp。DB 零改；本文件只消费 K417 建
//  的失体Ⅰ槽 4695010+side，**不新增任何功能键段**（审计槽 10<id>000x 段照惯例）。）
//
// 行清单（官方现查 effect_info.info 全文；args_num 与载体切片逐位=0；2026-11-07）：
//   2132 [0] "对手持有失体时造成伤害提升100%，否则为对手附加3回合的失体并令对手害怕"
//        载体：缘结·浮世诵 37021[699,1568,2132,2133,837][200,100,400]（零参切片，
//        3 参全归 837，K417/J331 头注同口径；4499 德布比特 sp_extra_moves
//        learning_lv 77，全库唯一载体）。
//   ★ 时点甄别（派发单要求区分选技期/命中期）：2132 模板**无先制句**——两支都是
//     命中期语义（"造成伤害提升"与"附加+害怕"都在本技能命中效果结算面），
//     整体可产宏，无选技期残部留续单。与 2133（选择期路由，J331 已落）结构对称
//     但时点正交：2133 消费/生产**回体**槽（自身侧），2132 消费/生产**失体**槽
//     （对手侧）。
//
// 37021 链复扫（2026-11-07，本 worktree + 主检出 + 兄弟 worktree 并集）：
//   699  "无视攻击免疫效果" —— 内核穿透凭证族（src/entities/skills.cpp:281
//        penetration_flags_for_effect 映射、加载期并入 ws.attack_credential，
//        include/entities/skills.h:381 头注"不注册 SKILL_EFFECT"）——**在库**；
//   1568 "无视护盾效果"     —— resources/moves_lib/batch_b_skills.cpp:2908
//        SKILL_EFFECT(1568, effect_1568_ignore_shield) —— **在库**；
//   2133 回体门条件先制+否则支 —— resources/moves_lib/j331_marker_cond_prio.cpp:185
//        + custom_skill_selection_routes 路由行（j4/selection_routes_expansion_3.sql，
//        现查 DB 在位）—— **在库**；
//   837  "吸取…递增"        —— resources/moves_lib/burn_trio_skills.cpp:1560
//        SKILL_EFFECT(837, effect_837_growing_drain) —— **在库**；
//   2132 —— 全库并集零注册（见四查③）→ **本单唯一缺口，1 宏落地**。链上五 id
//        至此全部可用，37021 载体链复扫收官。
//
// 动工前置四查（2026-11-07，本 worktree + 主检出现查）：
//   ① 内核 grep 2132（src/ include/ 全 *.cpp *.h 词边界）：零命中（rc=1），
//     零行为硬编码（effect_meta.cpp:33 无 2132 行——非穿透/非记账类）；
//   ② effect_info 2132 在列 args_num=0 == side_effect.arg_count=0（双表一致，
//     无 R1 型切片差异）；custom_effect_programs / custom_effect_overrides
//     对 2132 零命中；载体 37021 切片自洽（side_effect [699,1568,2132,2133,837] /
//     side_effect_arg [200,100,400]，四零参效果 + 837 吃全参，逐位核对）；
//   ③ 全库 moves_lib/soul_lib 并集无 SKILL_EFFECT(2132)/kMark2132 注册（本
//     worktree、主检出 resources/、兄弟 worktree k430/（moves_lib 无 k430 产码，
//     场景 8110-8130 为 K426-K428 段）逐处 grep rc=1）——真缺口非撞号；
//     boss_invalid_effects.json 对 2132 无命中（连魂印空间同号行也没有）；
//   ④ 废弃 parser 覆盖面（EffectUnit 兜底，2026-11-05 冻结）：2132 模板比对
//     parse_status_inflict_primary 四 pattern（令/使+对手/自身+{）——"令对手害怕"
//     无花括号占位（异常名是文案不是 {n}）零命中；第二刀 179/700/1083/1257/456
//     五条件模板（若属性相同/先出手/若后出手/不处于异常状态/若对手体力不足）与
//     "对手持有失体时…"零匹配 → 未注册期该 id = NONE 安全跳过（无 K416 1762 型
//     寄生）。本单注册后 clone_effect 先命中、parser 通道对 2132 关闭。
//
// 口径记档：
//   - 标记功能槽 = **K417 建的 side 级契约，本文件是消费端**（K417 头注"本文件
//     标记槽键段即其消费端对接面"的失体侧对接落地）：
//     plugin_storage[4695010+side]=失体Ⅰ到期回合（0/缺=未持有）。
//     判持有 = 值>0 且 >= 当前回合数（**到期残留条目不判持有**，K417/J331 同式）。
//     ⚠️ 扫描器可见性记档：4695000（回体）基键已在 tools/check_storage_keys.py
//     INTENTIONAL_SHARED 备案（J331 直读写、扫描可见）；4695010（失体）的 K417
//     侧经参数化助手访问（k417_marker_* 的 base 形参），静态扫描不可见，本文件
//     直读写后该基键仍只落单文件 → 门禁不报、暂不需备案——交接契约以 K417 头注
//     + 本头注为凭，后续若 4695010 出现第二处直写文件再补 INTENTIONAL_SHARED 行。
//   - 增伤支 = **当次命中生效**：SKILL_EFFECT 时点先于伤害结算
//     （battleFsm handle_BattleFirstSkillEffect 先于 handle_BattleFirstAttackDamage），
//     对手持失体 → ws.damage_add_pct[owner] += 100（"伤害提升100%"= +100%，
//     与"翻倍"等值；无"额外"措辞 → 通用增伤·加算通道，d5_D19 1870 直系：
//     "自身存在护罩则造成的攻击伤害翻倍"→ += 100，1178/776 同族）。
//     零参宏无行参可消费：100 与 3 回合均为模板文案定值，插件侧写死并在此注明
//     （J331 先制量同款记档口径）。
//   - 否则支失体附加 = **K417 1970/1968 附加口径同式**：附加回合在内，到期回合
//     = R+{0}-1（roundCount 在技能效果时点为人类回合 N 的 N-1，K417 场景 7950
//     "rc1+3-1=3" 已锁此折算）；写即覆盖到期残留（J331 2133 否则支同式，无需先擦）。
//     ⚠️ 附加发生在本技能伤害结算**之前**（SKILL_EFFECT 时点）——对手被本击 KO
//     时槽值已写（命中效果先于伤害的既有引擎时序，非本单私造）。
//   - 否则支害怕施加 = apply_anomaly_checked 恒 100 申报（官方行文无 % 占位，
//     K176/K418/J331 100% 档原语短路不耗 rand 口径）+ random_anomaly_duration()
//     （常量，无 rand 消费）→ 全效果无随机数消费，场景可做无随机精确断言；
//     落地判据 SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED（J331 疲惫同式）。
//     害怕 = AbnormalStatusId::Fear（官方异常码 6；与 2133 否则支疲惫 7 互斥
//     不同码，场景做隔离断言）。
//   - 与链上邻位宏的联动/隔离：2133（选择期，回体槽）与 2132（命中期，失体槽）
//     同载体同回合先后执行、读写不同槽互不污染——37021 连用两回合即可同时验
//     "R1 双否则支（各附加各的标记）→ R2 双持有支（先制+1 与 伤害×2）"的全链
//     联动；K417 1968（附加失体）是本宏增伤支的生产者、K417 1971（解放失体）是
//     本宏的门清除者——三 dylib 共槽行为对齐在场景做跨族断言。
//
// 存储键段（新增前已 grep 全库无撞；tools/check_storage_keys.py 门禁兜底）：
//   - 审计槽 10<id>000x 段（K416/K417/K418/J331 同款）：
//     1021320001(+owner)=2132 分支位（1=增伤支 2=否则支）
//     1021320002(+owner)=2132 否则支写入的失体到期回合（缺席=增伤支）
//     1021320003(+owner)=2132 否则支害怕落地位（1=落地 0=被挡；缺席=增伤支）
//   - 功能槽：4695010+side 失体Ⅰ（K417 段，只消费不新建）。
//
// 先例：d5_D19.cpp 1870（SKILL_EFFECT 当次伤害翻倍·条件门 + 通用加算通道）、
//   j331_marker_cond_prio.cpp 2133 否则支（apply_anomaly_checked 100 申报 +
//   标记槽写式 + 落地判据）、k417_debubite_pool.cpp（失体Ⅰ槽契约源）。
// 场景回归：test/scenario/scenario_8140_yuanjie_fushisong.cpp（号段 8140-8149）。

#include "common.h"

namespace {

// ── 审计槽 ──────────────────────────────────────────────────────────────
constexpr int k2132BranchAudit  = 1021320001;   // +owner：分支位（1=增伤支 2=否则支）
constexpr int k2132AttachAudit  = 1021320002;   // +owner：否则支写入的失体到期回合
constexpr int k2132FearAudit    = 1021320003;   // +owner：害怕落地位（1=落地 0=被挡）

// ── 功能槽（K417 段契约，只读+否则支写）────────────────────────────────
constexpr int kShiTiBase = 4695010;   // +side：失体Ⅰ到期回合（K417；值>0 且
                                      // >= roundCount 判持有，0/缺=未持有）

// 失体Ⅰ持有判定（K417 k417_marker_held / J331 j331_marker_held 同式：
// 写入过且未到期；到期残留条目不判持有）。
bool k429_shiti_held(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    const auto it = ctx->plugin_storage.find(kShiTiBase + side);
    if (it == ctx->plugin_storage.end()) {
        return false;
    }
    const int expiry = any_int_or(it->second, 0);
    return expiry > 0 && expiry >= ctx->roundCount;
}

}  // namespace

// ── 2132 —— 对手持有失体时造成伤害提升100%，否则为对手附加3回合的失体并令对手害怕 ──
// （零参；args=[owner,target] 引擎注入双方；SKILL_EFFECT 命中期，仅命中结算面执行。）
EffectResult effect_2132_shiti_cond_amp_or_attach(BattleContext* ctx,
                                                  const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2132, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2132, args,
                     "2132 布局=[owner,target]（零参载体，引擎注入双方）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     2132, args, "需要 owner/target ∈ {0,1} 且不同侧");
    if (k429_shiti_held(ctx, target)) {
        // 增伤支：对手持有失体Ⅰ → 本次伤害 +100%（通用增伤·加算通道；
        // SKILL_EFFECT 时点先于伤害结算，当次生效；d5_D19 1870 直系）。
        ctx->ws.damage_add_pct[owner] += 100.0f;
        ctx->plugin_storage[k2132BranchAudit + owner] = 1;
        return EffectResult::kOk;
    }
    // 否则支：为对手附加 3 回合失体Ⅰ（附加回合在内，到期 = R+3-1；写即覆盖
    // 到期残留，K417 1970 附加口径同式）+ 令对手害怕（100% 申报，无 rand 消费）。
    const int expiry = ctx->roundCount + 3 - 1;
    ctx->plugin_storage[kShiTiBase + target] = expiry;
    ctx->plugin_storage[k2132AttachAudit + owner] = expiry;
    int fear_landed = 0;
    if (g_core && g_core->apply_anomaly_checked) {
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            ctx, target, static_cast<int>(AbnormalStatusId::Fear),
            random_anomaly_duration(), owner, 100, ChanceSource::Skill);
        fear_landed =
            (r == ApplyAnomalyResult::SUCCESS
             || r == ApplyAnomalyResult::REPLACED_EXISTING
             || r == ApplyAnomalyResult::DURATION_EXTENDED) ? 1 : 0;
    }
    ctx->plugin_storage[k2132FearAudit + owner] = fear_landed;
    ctx->plugin_storage[k2132BranchAudit + owner] = 2;
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(2132, effect_2132_shiti_cond_amp_or_attach)
