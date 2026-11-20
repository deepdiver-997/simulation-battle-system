// J255 · 窗式对手Miss族 —— 654×7（2026-10-24，worktree ../agents/j255）
//
// 家族语义（effect_info 模板 + 4399 载体文本交叉定标）：
//
// 654（args_num=3）"{0}回合内{1}%的概率使对手{2}Miss"：
//   - {2} 技能类型码（4399 五条文本交叉解码，全吻合）：
//       0 = 物理攻击（末日泯灭咆 [2,50,0]："50%的概率使对手物理攻击miss"）
//       1 = 特殊攻击（散热环流 [3,100,1]："100%的概率使对手特殊攻击Miss"）
//       2 = 所有攻击（狂龙卸甲 [1,50,2]："50概率使对手所有攻击miss"、百花丛舞
//           [2,30,2]："对手攻击30%miss"、无影刀锋阵 [3,50,2]："对手的攻击对自身miss"）
//     ——属性技不在域内（"攻击"≠属性技；与 78/432 的"物/全类型"分档同词汇体系）。
//   - {1} 是**每次攻击的 miss 概率**，不是必 miss：通道取 hit_rate_mod_attack[opp]
//     ×= (1 - {1}/100)（乘算：accuracy100 技命中恰 100-{1}%；{1}=100 → ×0 必 miss）。
//
// 实现口径（H5 78/432 直系变形，h_miss_window.cpp）：
//   - 写入槽 = **hit_rate_mod_attack**（内核攻击技门控命中率修正，skills.cpp :894
//     `!is_attribute` 门控——"对手使用攻击技能命中率降低X%"魂印族同槽）：
//     比 H5 78 的 hit_rate_mod 压 0 更贴合：属性技天然不在消费域，{2}=2 无需自判。
//   - {2}=0/1 的物/特分型：hit_rate_mod_attack 无物/特维度 → **roundChoice 类型门**
//     （78 同款）：读对手本回合选中技能静态 type，Physical/Special 对不上就不写。
//     {2}=2 不设门（内核 !is_attribute 门已承载"所有攻击"）。
//   - 窗口 = 950/78 双写口径："含当回合"——SKILL_EFFECT 本体当回合直写 +
//     register_round_effect(ROUND_START) tick 覆盖 R+1..（先手窗 [R, R+{0})）；
//     hit_rate_mod_attack 每回合 NSDMI reset 1.0f → 每回合恰一次乘算，无累积。
//   - ⚠️ 布线：body args=[owner,target,rounds,prob,type] 与 tick payload
//     [owner,opp,prob,type] **不同构**（78 恰好只读 [0][1] 才能直调 body args）——
//     本实现把写入核心抽成 j255_apply_once，body 直写与注册 tick 各自喂参，不共用布局。
//   - roundChoice 门与 78 相同的边角：对手被控/换宠（非技能行动）不写（无命中可压）；
//     多段/变型技按选中槽静态 type 判（变型技物化改型在 resolve 期，晚于 ROUND_START——
//     记档：变型技与 654 同场的场景全库未见）。
//
// plugin_storage 审计槽（grep 全库无撞）：
//   990654000 + owner = 本次申报乘算补数（100-{1}；武装即写）。
//
// 动工前置三查（2026-10-24，worktree ../agents/j255）：
//   ①内核 grep 654 无行为硬编码（hit_rate_mod_attack 是 1314/1576 魂印族通用槽）
//   ②effect_info 654 args_num=3 与 side_effect.arg_count=3 一致、custom_effect_programs
//     零行
//   ③moves_lib/soul_lib 全库无 SKILL_EFFECT(654) 注册、parser 两刀不接（非状态主句/
//     非五类条件模板）——真缺口。
//   载体切片（side_effect.arg_count 优先，脚本核算 2026-10-24）：
//     纯链×3：末日泯灭咆 16406 [2,50,0] / 狂龙卸甲 16449 [1,50,2] / 百花丛舞 16933 [2,30,2]
//     混链×4：磨光幻刃 16978 [33,654]|[—],[2,40,2]（33 消强已实现）
//             灵光净化 23690 [57,654]|[4,3],[3,60,2]（57 每回合回血已实现）
//             无影刀锋阵 23793 [695,654,433]|[3,50],[3,50,2],[3,1,0,0,0,0,0]（695/433 已实现）
//             散热环流 28606 [599,654]|[3,0,75],[3,100,1]（599 减伤已实现）
//   混链兄弟全 implemented → 全载体链激活，场景断言只钉 654 半边（并行链陷阱守则）。
//   真实精灵覆盖 12 只。
// 先例：h_miss_window.cpp 78/432（miss 窗通道 + roundChoice 门 + 双写窗口）、950
//   （直系 f_break_round2）、内核 skills.cpp hit_rate_mod_attack 落点（:894 注释）。
// 场景回归：test/scenario/scenario_7320_miss_type_window.cpp（号段 7320-7329）。

#include "common.h"

namespace {

constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL（moves_lib 惯例）
constexpr int k654AuditBase = 990654000;

// 技能类型是否落在 {2} 的 miss 域内：0=物理 1=特殊 2=所有攻击（属性技永假）。
bool j255_type_in_scope(int type_code, SkillType type) {
    switch (type_code) {
        case 0:  return type == SkillType::Physical;
        case 1:  return type == SkillType::Special;
        case 2:  return type == SkillType::Physical || type == SkillType::Special;
        default: return false;
    }
}

// 写入核心（body 直写与 tick 共用；prob/type 已是解包后的值，规避两种布局差异）。
void j255_apply_once(BattleContext* ctx, int owner, int opp, int prob, int type_code) {
    if (!ctx || owner < 0 || owner > 1 || opp < 0 || opp > 1 || owner == opp) {
        return;
    }
    if (prob <= 0 || prob > 100) {
        return;
    }
    // {2}=2 免 roundChoice 门（内核 !is_attribute 门承载）；{2}=0/1 判对手选中技类型。
    if (type_code != 2) {
        if (ctx->roundChoice[opp][0] != kActionSelectSkill) {
            return;   // 对手本回合非技能行动：无命中可压（78 同款）
        }
        const int slot = ctx->roundChoice[opp][1];
        const auto& skills = ctx->getPet(opp).skills;
        if (slot < 0 || slot >= static_cast<int>(skills.size())) {
            return;
        }
        if (!j255_type_in_scope(type_code, skills[static_cast<std::size_t>(slot)].type)) {
            return;
        }
    }
    ctx->ws.hit_rate_mod_attack[opp] *= (100.0f - static_cast<float>(prob)) / 100.0f;
}

// ── 654 ROUND_START tick —— payload = [owner, opp, prob, type_code] ──
EffectResult j255_654_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    j255_apply_once(ctx, args.int_args[0], args.int_args[1],
                    args.int_args[2], args.int_args[3]);
    return EffectResult::kOk;
}

// ── 654 —— {0}回合内{1}%概率使对手{2}Miss ──
// int_args = [owner, target, {0}=回合数, {1}=概率, {2}=类型码]。
EffectResult effect_654_miss_type_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 654, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 654, args,
                     "654 布局=[owner,target,回合数,概率,类型码]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int prob = args.int_args[3];
    const int type_code = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 654,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 654, args, "需要 回合数 > 0");
    EFFECT_ARG_GUARD(prob > 0 && prob <= 100, 654, args, "需要 概率 ∈ (0,100]");
    EFFECT_ARG_GUARD(type_code >= 0 && type_code <= 2, 654, args, "类型码 ∈ {0,1,2}");

    ctx->plugin_storage[k654AuditBase + owner] = 100 - prob;
    // "{0}回合内"含当回合双写（950/78 同款）：当回合直写 + tick 覆盖 R+1..
    j255_apply_once(ctx, owner, opp, prob, type_code);
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 654, rounds,
                          {owner, opp, prob, type_code}, &j255_654_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(654, effect_654_miss_type_window)
