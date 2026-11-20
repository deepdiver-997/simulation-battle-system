// K469 · 血量先制族 —— 616×6（2026-11-17，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k469，分支 feat/k469；
// 场景 test/scenario/scenario_8565_k469.cpp，号段 8565-8569，基点 fda62989）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查
//   2026-11-17）══
//   616（args_num=2）"当自身血量少于1/3时先制+2"
//   ——**J53 454（"当自身血量少于1/{0}时先制+{1}"）的字面定档同形**：六载体切片
//   全部 [3,2]（=除数 3、先制量 2，与模板字面 1/3、+2 逐位互证）。实现照抄
//   j_cond_priority.cpp effect_454_lowhp_priority（J53 先例，文件:行
//   resources/moves_lib/j_cond_priority.cpp:36-51）。
//
// ══ 载体切片（unity 库 side_effect 列 = 效果 id 数组、side_effect_arg = 平铺行参，
//   按 side_effect.arg_count 依序切分，六载体全链零剩余，逐链手工核算
//   2026-11-17；场景 T8565-pre 以实际加载器 load_skill_effects 逐载体锁定）══
//   17660 破锐怒冲   [128,616]   args[1,3,2]     → 616=[3,2]（cat1 物140 p0；
//     128(1)=[1]——受击转回血窗，行为面镜像块用、门开档自伤面不观测）
//   19323 龙怒天火断 [180,680,616] args[40,6,3,3,2] → 616=[3,2]（cat1 物150 p0；
//     180(0) 零参、680(3)=[40,6,3]——先出手时 40% 害怕 3 回合，仅先手趟掷骰）
//   23413 海之契约   [616,43,57,5] args[3,2,2,3,4,5,100,-1] → 616=[3,2]
//     （cat4 属性 p0；43(1)=[2]/57(2)=[3,4]/5(3)=[5,100,-1]）
//   23836 轮回天生   [705,616]   args[3,2]       → 616=[3,2]（cat4 属性 p0；
//     705(0) 未注册 inert——registry 1566 无 705，clone 无实现静默跳过）
//   24822 心怀苍生   [616,902,749] args[3,2,1,100,1] → 616=[3,2]（cat4 属性 p2；
//     902(1) 未注册 inert、749(2)=[100,1]）
//   38016 异怪波动   [616,1627]  args[3,2,1,7,15] → 616=[3,2]（cat2 特10 p0；
//     1627(3)=[1,7,15]——K465 已注册连击+秒杀族，行为面主载体不选防污染）
//
// ══ 实现口径（三查结论 2026-11-17，本 worktree + 主库 file URI ?mode=ro）══
//   ① 内核 grep 616：零行为命中（soul_lib/k303_attack_burn_or_amp.cpp 的
//     SOUL_PROGRAM(616) = **soul 空间**玛格丽特 2977"攻技烧伤+未烧伤反增伤族"
//     ——双空间同号两套语义，与 moves 技能空间 616 互不相干；内核 src/include
//     无 616 case/硬编码）。②effect_info 616 模板在列（args_num=2），side_effect
//     定义行 arg_count=2 双表一致；effect_info 无同文模板行（616 文本全库唯一）。
//   ③全 worktree 并集 SKILL_EFFECT(616 零命中——moves 空间真缺口。
//   ★ **soul 空间同号甄别**（看板"两族键段共存"项）：soul 616 **已实现**
//     （K303，审计键 990803000 段）→ moves 侧审计键按 K465 子块拆分先例取
//     +0100 子块（1016160100），基段 1016160000 留白；全库（含兄弟 worktree）
//     四键段 grep 零占用 2026-11-17。
//   ★ **通道裁定：选择期路由（custom_skill_selection_routes）+ 插件体**——
//     条件先制族唯一正位（J66 勘误系结论：effect_register_state 路由注册进
//     用技期分支赶不上首回合先手判定；454/1484/1487/958/1174/1081/1774 等
//     全族同判）。**授权与对接**：派发词"仅插件与独立场景"未预见路由行需求，
//     a31-orch 2026-11-17 裁决批准 J335 形三件套（j4 补丁扩行 + 插件体 +
//     场景面）——tools/db_patches/j4/selection_routes_expansion_6.sql（616
//     一行 plain + 证据；expansion_5 的"精确 37 行"断言按 c01a5bb2 先例放宽
//     为 37 种子集口径，扩行后总行数 38）。行参走 plain 适配（[owner,target]
//     +[3,2]），条件判定仍在插件体运行时执行（装槽不定结果，J327 同口径）。
//   · 门判据：hp×{0} < max_hp（454 同款整数直乘，1397"严格低于"口径，无除法
//     截断——恰 1/3 边界 hp×3==max_hp → 门闭，场景 T8565.c 锁负测）。
//   · 出 = ws.preemptive_level[self] += {1}（**优先级修改面**：回合视图累加，
//     不触技能基础优先级 skill.priority/moves.priority——基值先制由内核
//     on_selected 哨兵节点从 moves.priority 累加，本效果在其后叠条件加成，
//     先手权比较前生效，K457"必先→先制→速度"真实序里的"先制"位）。
//   · 逐回合在位重判：路由节点每次 on_selected 重装（选择技能即判当前血量面）
//     → 回血后门闭（T8565.d 负测）、受击后门开（T8565.e 正测）、无账本无
//     latch。无跨回合持久票：preemptive_level 每回合 FMR 入口 memset
//     （battleFsm.cpp:1769）→ 离场/换技能自然失效（T8565.g）。
//   · 概率面 N/A（门判据确定、无掷点通道）；once 面 N/A（节点随选择期注册、
//     桶语义一次性消费，无 once 语义位）。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + **0100 子块** + owner×16 + slot；
//   1016160100 段全库 grep 零占用 2026-11-17，含兄弟 worktree；soul 616 已实现
//   用 990803000 段——不同基址无交叠）══
//   slot0={0}回显（除数） 1={1}回显（先制量） 2=末次门判（1=开/0=闭）
//     3=累计门开次数 4=累计判门次数 5=末次判门回合 6=末次出值后
//     preemptive_level 快照
//   布局自防撞：单基址 per effect + owner×16 步长 > 槽位上限 7 → 同基址内
//   owner 间无交叠；写键全走本文件 set_audit/get_audit helper（K460 封装纪律），
//   跨文件撞键由 tools/check_storage_keys.py 门禁静态扫。
//
// ══ 变异测试（权威轮 build/k469-mutations/，底本 SHA 见其 baseline.sha；
//   变异驱动删 .o 强编 + SHA 双守卫——GNU Make 3.81 mtime 整数秒截断真根因，
//   K468 实证 sleep 无效）══
//   权威轮清单与首杀特征见场景文件头注；还原后 SHA==底本 → 显式
//   moves_plugin_deploy 强编复绿。

#include "common.h"

namespace {

// ── 审计键基址（soul 空间 616 已实现（K303 焰魂·金斯特 2863）→ +0100 子块拆分，
//   见头注；soul 侧现行键段 990803000 与本基址无交叠）──
constexpr int k616AuditBase = 1016160100;   // 10^9 + 616×10^4 + 0100 子块

inline int audit_key_of(int base, int owner, int slot) { return base + owner * 16 + slot; }

int get_audit(const BattleContext* ctx, int base, int owner, int slot, int def = 0) {
    const auto it = ctx->plugin_storage.find(audit_key_of(base, owner, slot));
    return it == ctx->plugin_storage.end() ? def : any_int_or(it->second, def);
}

void set_audit(BattleContext* ctx, int base, int owner, int slot, int value) {
    ctx->plugin_storage[audit_key_of(base, owner, slot)] = value;
}

void add_audit(BattleContext* ctx, int base, int owner, int slot, int delta) {
    set_audit(ctx, base, owner, slot, get_audit(ctx, base, owner, slot) + delta);
}

// ════════════ 616 —— 当自身血量少于1/{0}时先制+{1}（454 同形，选择期路由体）════════════
// 行参 [owner, target, {0}=除数, {1}=先制量]（args_num=2 + [owner,target] 前缀）。
// 六载体切片恒 [3,2]（=字面 1/3、+2），行参在位即消费、不写死文案数字。
EffectResult effect_616_lowhp_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 616, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 616, args,
                     "616 布局=[owner,target,除数,先制量]");
    const int self = args.int_args[0];
    const int denom = args.int_args[2];
    const int bonus = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 616, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0 && bonus > 0, 616, args, "需要 除数/先制量 > 0");
    // 严格低于 1/{0}：hp×{0} < max_hp（454/1397 同款整数直乘，恰 1/3 → 门闭）。
    const bool open = ctx->getPet(self).hp * denom < max_hp_of(ctx, self);
    set_audit(ctx, k616AuditBase, self, 0, denom);
    set_audit(ctx, k616AuditBase, self, 1, bonus);
    set_audit(ctx, k616AuditBase, self, 2, open ? 1 : 0);
    set_audit(ctx, k616AuditBase, self, 5, ctx->roundCount);
    add_audit(ctx, k616AuditBase, self, 4, 1);
    if (!open) {
        return EffectResult::kOk;
    }
    add_audit(ctx, k616AuditBase, self, 3, 1);
    ctx->ws.preemptive_level[self] += bonus;
    set_audit(ctx, k616AuditBase, self, 6, ctx->ws.preemptive_level[self]);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(616, effect_616_lowhp_priority);
