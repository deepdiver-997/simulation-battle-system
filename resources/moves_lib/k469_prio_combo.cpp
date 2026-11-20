// K469 · 先制连击族 —— 1930×6（2026-11-17，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k469，分支 feat/k469；
// 场景 test/scenario/scenario_8560_k469.cpp，号段 8560-8564，基点 fda62989）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查
//   2026-11-17）══
//   1930（args_num=3）"自身每有{0}级先制则连击次数+{1}，最高连击{2}次"
//
// ══ 载体切片（unity 库 side_effect 列 = 效果 id 数组、side_effect_arg = 平铺行参，
//   按 side_effect.arg_count 依序切分，六载体全链零剩余，逐链手工核算 2026-11-17；
//   场景 T8560-pre 以实际加载器 load_skill_effects 逐载体锁定）══
//   36434 魁·辰纹七闪 [1930,1029,1223] args[1,1,7,300,2,2,300,2,3]
//     → 1930=[1,1,7]（cat2 特70 p0；1029(3)=[300,2,2]/1223(3)=[300,2,3]）
//   36899 码头大劫案 [1123,1930] args[2,3,1,1,6] → 1930=[1,1,6]
//     （cat1 物30 **p2**；1123(2)=[2,3]——J 系已注册，对手无提升趟纯 no-op）
//   37756 绵幕 [1159,1930,2040] args[1,1,1,5,2,100,5] → 1930=[1,1,5]
//     （cat2 特20 p3；1159(1)=[1]/2040(3)=[2,100,5]）
//   38075 龙寤揭天 [1568,1234,1930,2337,793] args[1,1,1,5,300,3,300,2,300]
//     → 1930=[1,1,5]（cat1 物90 p1；2337 无 side_effect 行 → effect_info
//       args_num=2 兜底=[300,3]/793(3)=[300,2,300]）
//   38149 攻壳机突 [2363,1930,2116,1357] args[1,1,7,2,50,100] → 1930=[1,1,7]
//     （cat1 物30 p3；2363 零参未注册/inert、2116(2)=[2,50]、1357(1)=[100]）
//   38269 感·万绪入魂 [1930,942,1323,1286] args[1,1,4,3,50] → 1930=[1,1,4]
//     （cat2 特80 p1；942(1)=[3]/1323(1)=[50]/1286 零参）
//   全载体 {0}=1、{1}=1（六行同档）；{2} 上限 4/5/6/7 差分（场景 T8560-pre +
//   行参 override 变档腿）。
//
// ══ 实现口径（三查结论 2026-11-17，本 worktree + 主库 file URI ?mode=ro）══
//   ① 内核 grep 1930：effect_meta.cpp 显式覆盖行
//     {1930, EffectCategory::Combo, "override: 动态连击 每{0}级先制+{1}/上限{2}
//     （未实现）"}（纯元数据标注，唯一消费面 Penetration 与本 id 无关）+
//     skills.cpp:64 动态加数未覆盖清单注释——均非行为代码；effect_register_state
//     无 case。②effect_info 1930 模板在列（args_num=3），side_effect 定义行
//     arg_count=3 双表一致。③全 worktree 并集（k4xx/agents/endgame-walk/
//     web-replay/主检出）SKILL_EFFECT(1930 零命中——真缺口。
//   ★ **soul 空间同号甄别**（看板"两族键段共存"项）：官方魂印数据 1930 =
//     裁罪魔剪·塞泽丝（4347，row1571，soul_effects_dump 现查 2026-11-17）在档
//     **未实现**（soul_lib 零命中）→ 审计键按 K465（soul 1863/1627）/K458/K461
//     **子块拆分先例**取 +0100 子块（1019300100），给将来 soul 实现留出基段
//     1019300000；全库（含兄弟 worktree）四键段 10193x0000/1016160x00 grep
//     零占用 2026-11-17。
//   · **通道裁定：内核连击通道 ws.combo_count_resolver**（K465 1863 直系 /
//     j_combo_face 484/612/1685 直系）——模板词"连击次数"与官方连击公式逐字
//     对应；effect_meta 已标 EffectCategory::Combo。SKILL_EFFECT 时点读
//     ws.preemptive_level[owner]（**当前技能先制等级读面**：FMR 桶 memset 后
//     已累加基值（on_selected 选择期）+条件先制（2020/616/1070 等路由族）+
//     束缚/超频修正——battleFsm.cpp:1769-1798，MOVE 态不重置，SKILL_EFFECT
//     时点即本侧当前生效先制等级）→ 装解析器（一次性 lambda、用后即焚）。
//     无规则行（动态加数族）→ 静态 roll 退化 1~1 **不消耗 sbs_rand**
//     （skills.h roll_combo_count 短路档）→ resolver 返回完整总数。
//   · **总连击读法（官方无实测样本，保守口径记档）**：
//       total = min(1 + floor(P / {0}) × {1}, {2})   （P = preemptive_level）
//     基数 1 = 非"做N次攻击"技能的自然单段（连击次数是**增量**语义："连击次数
//     +{1}"与 1863 "每有1项PP非满则连击次数+{1}"（基数{0}+项数×{1}）同构——
//     本模板无显式基数位，基数取自然单击 1）。P=0 → total=1 → 解析值=1 档
//     （内核不置 rewritten，与普通单击完全等价，battleFsm.cpp:734）。
//     替代读法 total=floor(P/{0})×{1}（无基数、P=0 内核钳 1）在 P≥1 时少一段
//     ——官方实测出现时以实测为准，本单保守取增量读法并在此记档。
//   · {0}/{1}/{2} 行参守卫：{0}>0（除零防线，"每有0级"无语义）；
//     {1}>=0 在位即消费（"+0"=无加数关闸档合法，K314/K340 >0 守卫教训）；
//     {2}>=1（上限至少容纳自然单击）。
//   · 截断面：raw>cap → total=cap（审计槽记截断发生）；载体天然档无截断
//     （P≤3 且 cap≥4）→ 截断腿走行参 override（T8560.e {2}→2）。
//
// ══ 生命周期/清理 ══
//   · 无跨回合持久票：resolver 用后即焚（battleFsm.cpp:740 置空）+ ws 每回合
//     reset 兜底清（battleWorkspace.h:158 每攻击尝试开头再清）——"离场清理"
//     结构性成立（无账本可残留），场景 T8560.g 锁"次回合无残留 resolver/
//     审计累计不再增长"。
//   · 镜像：解析器按施放方 owner 槽挂载，先/后手 symmetric（T8560.f 双侧
//     不同载体档：双侧审计独立记账，键式含 owner 维度）。
//   · miss 趟：HIT 分支注册面（default_branch_for_effect(1930) 非无效分支白名单）
//     → miss/被封趟不注册不安装（T8560.h）。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + **0100 子块** + owner×16 + slot；
//   1019300100 段全库 grep 零占用 2026-11-17，含兄弟 worktree）══
//   slot0={0}回显 1={1}回显 2={2}上限回显 3=末次先制等级P 4=末次原始总数
//     （截断前） 5=末次总连击（=resolver 返回值，截断后） 6=累计安装次数
//     7=末次解析回合
//   布局自防撞：单基址 per effect + owner×16 步长 > 槽位上限 8 → 同基址内
//   owner 间无交叠；写键全走本文件 set_audit/get_audit helper（K460 封装纪律），
//   跨文件撞键由 tools/check_storage_keys.py 门禁静态扫。
//
// ══ 变异测试（权威轮 build/k469-mutations/，底本 SHA 见其 baseline.sha；
//   变异驱动删 .o 强编 + SHA 双守卫——GNU Make 3.81 mtime 整数秒截断真根因，
//   K468 实证 sleep 无效）══
//   权威轮清单与首杀特征见场景文件头注与本文件尾部记档；还原后 SHA==底本
//   → 显式 moves_plugin_deploy 强编复绿。

#include "common.h"

namespace {

// ── 审计键基址（soul 空间同号（裁罪魔剪·塞泽丝 4347）→ +0100 子块拆分，见头注）──
constexpr int k1930AuditBase = 1019300100;   // 10^9 + 1930×10^4 + 0100 子块

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

// ════════════ 1930 —— 每有{0}级先制连击+{1}，最高连击{2}次 ════════════
// 主体：SKILL_EFFECT 时点读先制等级 + 安装 combo_count_resolver（1863/484/1685 直系）。
// 行参 [owner, target, {0}=门槛级数, {1}=加数, {2}=上限]（args_num=3 + [owner,target] 前缀）。
EffectResult effect_1930_prio_combo(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1930, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1930, args,
                     "1930 布局=[owner,target,门槛,加数,上限]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int gate = args.int_args[2];
    const int bonus = args.int_args[3];
    const int cap = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1930, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(gate > 0, 1930, args, "需要 {0}门槛级数 > 0（除零防线）");
    EFFECT_ARG_GUARD(bonus >= 0, 1930, args, "需要 {1}加数 >= 0（+0 关闸档合法）");
    EFFECT_ARG_GUARD(cap >= 1, 1930, args, "需要 {2}上限 >= 1（至少容纳自然单击）");

    // 先制等级读面：当前技能先制（FMR 后视图：基值先制 + 条件先制 + 异常修正；
    // MOVE 态不重置，SKILL_EFFECT 时点即本侧当前生效值）。
    const int prio = ctx->ws.preemptive_level[owner];
    const int tiers = prio > 0 ? prio / gate : 0;   // 每有{0}级 → 一档
    const int raw_total = 1 + tiers * bonus;        // 增量读法（基数=自然单击，见头注记档）
    const bool capped = raw_total > cap;
    const int total = capped ? cap : raw_total;

    set_audit(ctx, k1930AuditBase, owner, 0, gate);
    set_audit(ctx, k1930AuditBase, owner, 1, bonus);
    set_audit(ctx, k1930AuditBase, owner, 2, cap);
    set_audit(ctx, k1930AuditBase, owner, 3, prio);
    set_audit(ctx, k1930AuditBase, owner, 4, raw_total);
    set_audit(ctx, k1930AuditBase, owner, 5, total);
    set_audit(ctx, k1930AuditBase, owner, 7, ctx->roundCount);
    add_audit(ctx, k1930AuditBase, owner, 6, 1);

    // 内核连击通道：第一次公式前咨询一次、materialize 写回 combo_view、用后即焚。
    // total=1 → 内核不置 rewritten，与普通单击完全等价（battleFsm.cpp:734）。
    ctx->ws.combo_count_resolver[owner] = [total](BattleContext*) { return total; };
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1930, effect_1930_prio_combo);
