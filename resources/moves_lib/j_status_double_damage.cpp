// J 组 J237 · 状态门双倍伤害族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   193（args_num=1）"若对手{0}，则必定造成双倍伤害"
//   —— {0} = 官方异常码（1中毒 2烧伤 5冻伤 7疲惫；全库载体实载集合）。
//
// 实现口径：
//   - 判定 = 对手指定异常槽 end_round 未到期。`has_active_abnormal_status` 是内核
//     非 inline 符号、插件不可直调（core_api.h 注释 / 击杀线教训 #4）→ 用公开的
//     `abnormal_status_end_round` 数组自判（effect_batch8.cpp 同式）。
//   - 落点 = `ws.damage_add_pct[owner] += 100`（"双倍" = 通用增伤 +100%，890 同通道；
//     SKILL_EFFECT 时点写入，AMP 阶段读取）。
//   - **多实例去重**：16165 秘魂斩链上是 [193,193,193]|[1,2,5]（三个独立条件）。
//     三个实例都命中时会叠成 ×4——官方语义是"若对手处于中毒/烧伤/冻伤状态，则必定
//     造成双倍伤害"的单次翻倍。用 `skill_use_seq[owner]`（每次技能执行 +1、只增不减）
//     做本次使用的"已翻倍"标记：同一次技能链上只有第一个满足条件的实例生效。
//     条件不满足的实例不写标记，后续实例仍可正常判定。
//   - 0 参兜底（本宏 1 参）：装载布局 [owner, target, status]。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 8/8 自洽）：
//   13154 冰怒刺拳 物100 [5] / 13793 玄冰裂 特70 pri1 [5] / 14858 狂枭粉碎击 物150 [7] /
//   **16165 秘魂斩 物85 pri2 [193,193,193]|[1,2,5]（多实例去重载体）** /
//   17740 风雪狂息怒 物150 [5] / 17862 达摩克斯刃 物160 [193,180]|[2]。
//   （全库 8 条载体行；纯链 13154/13793/14858/17740，场景取 14858 疲惫门 + 13154 冻伤门。）
//
// 动工前置三查（2026-10-22）：①内核 grep 193 无硬编码（异常码表/end_round 数组为通用
//   设施）②effect_info 仅本条模板 ③moves_lib/soul_lib 并集（jbatch/kbatch/j186/j198/
//   k347 各 worktree + 全部本地分支）无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5680_status_double_damage.cpp（号段 5680-5689）。

#include "common.h"

namespace {

// 私有键：本次技能使用已翻倍的序号（+owner）。
constexpr int k193AppliedSeqBase = 991930100;

// plugin_storage int 槽读写（batch_c_skills.cpp 同款；any 类型不符按缺失语义回退）。
int storage_int(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

void set_storage_int(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// "处于{status}异常"= 该异常槽 end_round 未到期（effect_batch8.cpp 同式；
// has_active_abnormal_status 是内核非 inline 符号，插件不可直调）。
bool has_anomaly_active(BattleContext* ctx, int side, int status_id) {
    return ctx && side >= 0 && side <= 1 && status_id >= 0
        && status_id < kOfficialAbnormalStatusSlotCount
        && ctx->roundCount < ctx->abnormal_status_end_round[side][status_id];
}

// ── 193 —— 若对手处于{0}状态，则必定造成双倍伤害 ──
EffectResult effect_193_status_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 193, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 193, args,
                     "193 布局=[owner,target,status]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int status = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 193,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(status), 193, args,
                     "193 {0} 异常码越界");
    if (!has_anomaly_active(ctx, opp, status)) {
        return EffectResult::kOk;   // 对手不处于指定异常：本实例不触发（不写去重标记）
    }
    // 同一技能链多实例只翻一次（16165 [193,193,193]）；skill_use_seq 每次执行 +1。
    const int key = k193AppliedSeqBase + owner;
    const int seq = ctx->skill_use_seq[owner];
    if (storage_int(ctx, key, -1) == seq) {
        return EffectResult::kOk;   // 本次技能已翻倍过
    }
    set_storage_int(ctx, key, seq);
    ctx->ws.damage_add_pct[owner] += 100.0f;   // "双倍"= 通用增伤 +100%（890 同通道）
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(193, effect_193_status_double)
