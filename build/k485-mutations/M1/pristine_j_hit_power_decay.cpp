// J 组 J233 · 命中衰减威力族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   810 [1] 每次命中对手后此技能威力下降{0}点
//
// 实现口径：
//   - 衰减账本 = 自身 **on_stage_storage**[key = 990810000 + owner*100000 + skill_id]：
//     记录"本技能已命中次数"（**下场重置**——2026-11-19 用户官方口径翻案，见下节）。
//   - SKILL_EFFECT：uses = 已命中次数 → `ws.skill_power_view[owner] = max(0,
//     materialized_base - {0}×uses)`（SkillView 赋值自动打 rewritten → 引擎重算伤害；
//     j_power_view3 468/118 同通道）。首次使用 uses=0，不改写。
//   - 计次 = AFTER_ACTION 探针（1000 同构）读 ws.skill_exec_result==HIT + 行动方过滤；
//     每次命中 +1。真 miss 在 execute() 内提前 return（SKILL_EFFECT 桶不跑）→ 自然不衰减。
//   - skill_id 取 executing_skill_slot(owner) 指向的实际执行技能（替换技/变型技也正确）。
//
// ══ 2026-11-19 K485 翻案：账本作用域 soulmark_storage → on_stage_storage ══
//   原建模（2026-10-22）按"官方'此技能'是技能自身状态，非上场状态"取**跨切换保留**
//   （soulmark_storage）。用户 2026-11-18 官方实测口径：**下场重置**——换宠离场后
//   计次清零、再上场首用全额（"每次命中对手后"的账本是本次上场期状态）。落点 =
//   pet.on_stage_storage（套路目录 §一 递增类三容器之"本次上场有效、下场不保留"；
//   引擎清零面 = perform_switch old_pet.on_stage_storage.clear()（battleFsm.cpp:609）
//   + clearAllEffects（battleContext.h:1196），生命周期全自动、无手写清理）。
//   保留不变：命中才计次（miss 不计）/次用折算（decay×uses）/首用不改写 全语义。
//   回归适配：5640 场景加 (c) 腿（命中→换宠离场再回场→威力回满 200）；8410（K454
//   属性技命中失效窗族）回归双绿作无殃及参照（8410 不含 810 载体，纯参照面）。
//   键段口径：数值基 990810000 沿用（与 soul_lib/k310 kWiredKeyBase 同基不同容器——
//   on_stage_storage 独立命名空间；且本键带 skill_id ≥10000 实际不交叠），容器迁移
//   后 (on_stage_storage, 990810000) 全库唯一（check_storage_keys.py 过门）。
//
// ══ 首用口径维持现实现（2026-11-19 用户裁定 ⑤）══
//   首用 uses=0 不改写 = **非变威力通道全额打**（官方模板"每次命中后下降"——首用
//   尚无命中计数，威力=materialized_base 全额）。官方实测程序记档（用户开房间测试法，
//   待实盘回归时对账）：
//     1) 进阶房/友情赛开房，己方带 810 载体（如怒霆狂劈）；
//     2) 对手 A：不嗑药不强化（基准面板）；对手 B：嗑攻击药/强化若干级；
//     3) 己方分别对 A、B 打出**本场上首击**，记录双方受伤；
//     4) 判据：首击伤害按双方面板差换算后符合全额威力公式（B 的强化等级按面板
//        放大）→ 首用未衰减（uses=0 全额）；若统一缩水一个 decay 档 → 首用已衰减。
//     官方实测若翻案此口径（首用即按已用 1 次计），改 SKILL_EFFECT 的 uses>0 门为
//     uses+1 折算即可（单点改动，5640 (a) 腿三断言相应 -20 平移）。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 9/9 自洽）：810 [decay]：
//   34596 怒霆狂劈 [20]（纯链 pow200，场景载体）/ 17945 聚能离子炮 [100] /
//   32248 雪鹿凝冰踏 [40] / 33039 恶魔瘟疫 [100] / 34833 凝魂绝命箭 [50] /
//   37283 核能电磁炮 [100] / 37507 措手不及 [100] / 37821 森穹垂落 [100] /
//   38272 天叩·千钧一发 [1000]。
//
// 动工前置三查（2026-10-22）：①内核 grep 810 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree）无注册——真缺口。
//   （2026-11-19 K485 翻案复查：容器迁移不新增注册面，registry 810 原在册不变。）
//
// 场景回归：test/scenario/scenario_5640_hit_power_decay.cpp（号段 5640-5649；
//   2026-11-19 增 (c) 下场重置腿）+ scenario_8720_k485.cpp T8721（38272 [1000]
//   首用全额/次用 0 威力边界，号段 8720-8729）。

#include "common.h"

#include <algorithm>

namespace {

constexpr int k810StoreBase = 990810000;   // + owner*100000 + skill_id

// 本 AFTER_ACTION 桶属哪一方（1000/j129 同判据，就地重写）。
int j233_bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

// 810 探针：payload = [owner, opp, skill_id]；命中一次 → 账本 +1。
EffectResult j233_810_hit_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = a.int_args[0];
    const int skill_id = a.int_args[2];
    if (owner < 0 || owner > 1 || skill_id < 0) {
        return EffectResult::kOk;
    }
    if (j233_bucket_acting_side(c) != owner) {
        return EffectResult::kOk;   // 只在"自己出手流程结束"判
    }
    if (c->ws.skill_exec_result[owner] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // 真 miss 不衰减
    }
    auto& store = c->getPet(owner).on_stage_storage;   // 2026-11-19 翻案：下场重置作用域
    const int key = k810StoreBase + owner * 100000 + skill_id;
    const int uses = store.count(key) ? any_int_or(store[key]) : 0;
    store[key] = uses + 1;
    return EffectResult::kOk;
}

}  // namespace

// ── 810 —— 每次命中后此技能威力下降{0}点 ──
// int_args = [owner, target, decay]（args_num=1）。
EffectResult effect_810_hit_power_decay(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 810, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 810, args,
                     "810 布局=[owner,target,decay]");
    const int owner = args.int_args[0];
    const int decay = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 810, args, "owner 越界");
    const int slot = ctx->executing_skill_slot(owner);
    EFFECT_ARG_GUARD(slot >= 0 && slot < 5, 810, args, "执行技能槽位未知");
    const int skill_id = ctx->getPet(owner).skills[static_cast<std::size_t>(slot)].id;
    EFFECT_ARG_GUARD(skill_id >= 0, 810, args, "执行技能 id 未知");

    auto& store = ctx->getPet(owner).on_stage_storage;   // 2026-11-19 翻案：下场重置作用域
    const int key = k810StoreBase + owner * 100000 + skill_id;
    const int uses = store.count(key) ? any_int_or(store[key]) : 0;
    if (uses > 0 && decay > 0) {
        // 行参在位即消费：decay 直读；0 档不衰减（不回退默认值）。
        const int cur = static_cast<int>(ctx->ws.skill_power_view[owner]);
        ctx->ws.skill_power_view[owner] = std::max(0, cur - decay * uses);
    }
    // AFTER_ACTION 计次探针（1000 同款；双方桶各一条一次性，duration=1）。
    const std::vector<int> payload{owner, 1 - owner, skill_id};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(810, 0, owner, -1, EffectArgs(payload), &j233_810_hit_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 810;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(810, effect_810_hit_power_decay)
