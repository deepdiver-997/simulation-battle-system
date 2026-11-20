// J 组 J289 · 条件威力倍化族 —— 2 宏/12 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   129 [1] 对方为{0}则技能威力翻倍（{0}=**性别码**：1=雄性 / 2=雌性）
//   168 [0] 若对手处于睡眠状态，则威力翻倍
//
// ⚠️ **J336 勘误（2026-11-08）——{0} 曾被误读为 skill_types 元素 id，已证伪重实现**：
//   K435 定案证据链（k435_xingbie_fushang_pool.cpp 头注，本单独立复核通过）：
//   ① BWIKI 效果:129 页逐载体实例化——蔚蓝之击[2]/赤色旋风[2]→"对方为**雌性**则
//     技能威力翻倍"、噩梦袭击[1]/萌兔灵击[1]/致命影袭[1]/碧灵轻语[1]→"对方为
//     **雄性**则…"，与 DB 实参逐位吻合（12296=[2]雌 / 32098=[1]雄）；Flash 端模板
//     字面带"性"字。反证：赤色旋风自身是火系招（moves.type=29 火超能），元素读法
//     下"[2]=水系对手翻倍"主题荒谬，性别读法才自洽。
//   ② 同门 130/131 同码表互证（BWIKI 效果:130：12297[2,60]→雌性+60、12304[1,100]
//     →雄性+100；效果:131：蓝光幻盾21193[2]→雌、红光幻盾21197[1]→雄）。
//   ③ 链内互补：19343 的 130[1,250]"对方为雄性则附加250"与 898[250]"若对手不是
//     雄性精灵则恢复250"成对出现（BWIKI 两技页同文）——若 {0} 是元素码，898 的
//     "不是雄性精灵"措辞无从谈起。
//   ④ 引擎先例 600（qiling_skills.cpp:208-211）：gender_code==1→MALE、==2→FEMALE
//     同码表；DB monsters.gender 0=无性别/1=雄/2=雌（pet_factory.cpp:36-41
//     map_gender；Gender 枚举 MALE=0/FEMALE=1/NONE=2，include/entities/elf-pet.h:21-25）。
//   → 码表定案：**{0}=1→雄性、{0}=2→雌性、其它码=数据漂移（guard 响亮报错）**。
//
// 实现口径：
//   - 时点 = SKILL_EFFECT：此时 resolve_skill_execution 已把 executing skill 的威力物化进
//     ws.skill_power_view[owner]；写 `view = view.value * 2` 自动置 rewritten →
//     ATTACK_DAMAGE 收尾按标准变威力重算（h_elem_attr_cond 65 / j_power_view3 97 同路）。
//   - 129 的条件读**对手本体 gender 枚举** `getPet(opp).gender == 映射枚举`（130
//     K435 / 600 qiling_skills 同判：本体相等才开，无性别 NONE 不属雄/雌）；
//     码表 = 1→MALE / 2→FEMALE（600 同表，见上方勘误段证据链）。
//   - 168 的条件读 `g_core->has_active_anomaly(opp, 8)`（AbnormalStatusId::Sleep；异常槽
//     查询是插件条件查询面，core_api.h:216）。
//     ⚠️ 时点必须在 **ON_SKILL_HIT**：睡眠在 ON_SKILL_HIT 的 anomaly_on_attack_hit_hook
//     里被清除（K2"命中即醒"，先于 SKILL_EFFECT）→ 挂 SKILL_EFFECT 时条件恒假。
//     路由 = skills.cpp effect_register_state 的 case 168（该桶在 resolve_skill_execution
//     物化威力视图之后、睡眠清除钩子之前执行，条件与写视图都赶得上）。
//     129 无此问题，仍在默认 SKILL_EFFECT 桶。
//   - 威力 0/未物化（view<=0）→ 不写（属性技无红伤可翻，845/65 同守卫）。
//   - 两宏都是"翻倍"不是"加百分比"：本次倍率 = 2；审计槽记录 1/2 便于无随机断言。
//
// plugin_storage 审计槽（grep 全库无撞）：
//   991290000 + owner = 129 本次倍率（1=未触发，2=翻倍）
//   991680000 + owner = 168 本次倍率
//
// 载体切片（side_effect.arg_count 顺序切分，脚本核算 2026-10-22；性别码表 2026-11-08
// J336 复核：[2]→雌、[1]→雄与 BWIKI 效果:129 六载体文案逐位吻合）：
//   129 [性别码]：12296 蔚蓝之击 [2]（雌；纯链）、12303 赤色旋风 [2]（雌）、
//     32098 噩梦袭击 [1]（雄）、33553 萌兔灵击 [1]（雄）、34041 致命影袭 [1]（雄）、
//     37108 碧灵轻语 [129,1459,863] 切片 [1]（雄；6/6 自洽）
//   168 []：12894 噬梦之光（纯链）、16017 噩梦侵蚀、16088 燕尾斩、33133 钟声啼鸣（纯链）、
//     16503 破风极迅吼 [168,107]、34449 幽蝶起舞 [471,168,1223]（6/6）
//
// 动工前置三查（2026-10-22）：①内核 grep 129/168 无硬编码 ②effect_info 模板唯一、
//   custom_effect_programs/overrides 零行 ③全分支 moves_lib/soul_lib 并集无 SKILL_EFFECT
//   注册（179 是解析器内建、不在本单）——真缺口。
// 先例：h_elem_attr_cond.cpp 65（SKILL_EFFECT 写威力视图）、j_power_view3.cpp 97/468
//   （变威力重算）；性别条件 = qiling_skills.cpp:208-211（600 同码表）+
//   k435_xingbie_fushang_pool.cpp:142-163（130 性别门直系）。
// 场景回归：test/scenario/scenario_6220_power_cond_double.cpp（号段 6220-6229）、
//   test/scenario/scenario_8230_xingbie_family_gate.cpp（8230-8239，性别双向门+同码表隔离）。

#include "common.h"

namespace {

constexpr int k129AuditBase = 991290000;  // +owner：本次倍率
constexpr int k168AuditBase = 991680000;  // +owner：本次倍率

// 威力视图翻倍（<=0 不写；返回是否写入）。
bool j289_double_power_view(BattleContext* ctx, int owner) {
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value <= 0) {
        return false;
    }
    view = view.value * 2;   // 置 rewritten → 标准变威力重算
    return true;
}

// ── 129 —— 对方为{0}则技能威力翻倍 ──
// int_args = [owner, target, 性别码]（args_num=1；{0}=1雄/2雌，J336 勘误定案）。
EffectResult effect_129_foe_gender_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 129, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 129, args,
                     "129 布局=[owner,target,性别码]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int gender_code = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 129, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(gender_code == 1 || gender_code == 2, 129, args,
                     "性别码 " + std::to_string(gender_code) + " 越界（官方码 1=雄 / 2=雌；"
                     "BWIKI 效果:129/130/131 三页互证，J336 勘误）");
    // 性别码 → Gender 枚举（600 同表：1=MALE / 2=FEMALE；pet_factory map_gender 同源）。
    const Gender want = (gender_code == 1) ? Gender::MALE : Gender::FEMALE;
    // 性别门：对手**本体** gender 枚举相等才开（无性别 NONE 不属雄/雌——600/130 同判）。
    const bool match = ctx->getPet(opp).gender == want;
    ctx->plugin_storage[k129AuditBase + owner] = match ? 2 : 1;
    if (match) {
        (void)j289_double_power_view(ctx, owner);
    }
    return EffectResult::kOk;
}

// ── 168 —— 若对手处于睡眠状态，则威力翻倍 ──
// int_args = [owner, target]（args_num=0）。
EffectResult effect_168_sleep_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 168, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 168, args,
                     "168 布局=[owner,target]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 168, args,
                     "需要 owner/target ∈ {0,1}");
    const bool sleeping = g_core && g_core->has_active_anomaly
        && g_core->has_active_anomaly(ctx, opp, static_cast<int>(AbnormalStatusId::Sleep));
    ctx->plugin_storage[k168AuditBase + owner] = sleeping ? 2 : 1;
    if (sleeping) {
        (void)j289_double_power_view(ctx, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(129, effect_129_foe_gender_double)
SKILL_EFFECT(168, effect_168_sleep_double)
