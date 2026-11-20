// K448 · 雷辰点亮机 moves 侧两宏 —— 2289「自身每点亮1颗雷辰威力提升{0}点，
// 若自身未点亮雷辰则己方每存活1只精灵造成的攻击伤害不低于{1}」（args_num=2）+
// 2290「自身每点亮1颗雷辰全属性+1」（args_num=0）
//（场景 test/scenario/scenario_8360_leichen_engine.cpp，号段 8360-8369；2026-11-10，
//  worktree k448，基点 = main-local 最新，registry moves=1520 / soul=411 起算）
//
// ══ 立单依据（调研文档 = docs_local/docs/05-任务清单/
//    雷伊雷晨点亮与武道衍化缺口调研-2026-11-09.md §一/§三·提案A）══
// 官方名勘误：「雷晨」=「雷辰」同音别字（effect_des/effect_icon 全库唯一拼法）。
// 2289 唯一载体 37908 雷神·天明闪（4765 怒雷神裁·雷伊 extra_moves lv76）——
//   链 [1523,2289,964,1960] args=[350,160,100,2] → 2289 切片 [160,100]
//   （1523 消回合吸 350 体 J1 已落 / 964 未击败强化翻倍 J12 已落 / 1960 击败 2 回合
//   强化不可消已落；主库+20261022 备库切片一致）。
// 2290 唯一载体 29044 雷·天霆之怒（4765 learnable lv76，tag3）——
//   链 [2290,924,1683,843] args=[3,3,3,2,2] → 2290 零参切片
//   （924[3,3] 下3回合吸 1/3 / 1683[3] 强化不可消 / 843[2,2] 先制+2 均已落）。
// ⚠️ 派发行注「29044（4946）」与 DB 冲突：monsters.raw_json 现查 29044 仅出现在
//   4765 学面（4946 万霆神律·雷伊 learnable lv71-77 为 38536/29455/29456/38537，
//   无 29044）——按 DB 权威口径落 4765，4946 侧是魂印 2548 辰律生产端（**已落地**
//   resources/soul_lib/k448_wanshen_2548.cpp，见其头注与 leichen_runtime.h 键段）。
//
// ══ 4399/BWIKI 复核（2026-11-10，k448 worktree）══
// · 37908 原文（reference/scrape_4399/out/pets_full.jsonl 4765 页 + BWIKI 页面）：
//   「必中；消除对手回合类效果，消除成功则吸取对手350点体力；自身每点亮1颗雷辰威力
//   提升160点，若自身未点亮雷辰则己方每存活1只精灵造成的攻击伤害不低于100；未击败
//   对手则自身强化状态翻倍；击败对手则令自身2回合内的能力提升状态无法被消除或吸取」
//   ——**无任何「连击」表述**（J 组"连击变体"记档系转述走样，调研文档 §1.4 判断成立，
//   不涉 F10 段数原语）；「威力提升160点」= 定点加算（非 %乘区，与 2080 异族）。
// · 29044 原文：「必中；自身每点亮1颗雷辰全属性+1；下3回合攻击技能吸取对手1/3最大
//   体力值；3回合内自身的能力提升状态无法被消除或吸取；下2回合令自身所有技能先制+2」。
//
// ══ 实现形态（同形套路照抄先例）══
// · 时点 = SKILL_EFFECT（K440-K446 同口径；行参布局 [owner, opp, 行参…]——loader
//   注入 owner/target 前缀 + 行参原序）。2289/2290 动作面全打自身/自身威力视图
//   → 不设对手生存门（K446 2080 口径）；2289 另设「对手已倒 → 整条跳过」防死靶
//   写保底武装（K422/K440-K446 生存门同款）。
// · 2289 威力支 = SKILL_EFFECT 时点写 ws.skill_power_view（burn_trio 53/1377 /
//   batch_b:560 / K446 2080 同面：整体赋值自动置 rewritten=变威力声明）。
//   **定点加算**：view = 写前 + 点亮颗数×{0}；不加 2080 的「view>0 才写」守卫——
//   37908 官方威力=0，写前视图 0 是合法物化值（4399「威力提升160点」语义）。
// · 2289 保底支 = **FLOOR 相**（damage_pipeline.h:51「保底伤害——减伤之后、锁伤之前」；
//   该相此前零注册，本单为首个 FLOOR 生产者——管线头注 L37 记档点名的落点）。
//   SKILL_EFFECT 时点亮颗数==0 → 武装 plugin_storage 保底键 = 己方存活数×{1}
//   （存活数 = 6 - count_dead(ROSTER)，spirit_lifecycle 四口径纪律，不手写 for 扫包）；
//   FLOOR 回调（register_default_damage_effect，TEAM 常驻 + 回调内实时判门——
//   管线条目生命周期注释 L88「有效性由效果自己的条件在回调里实时判」）消费武装键：
//   attackerId==owner && isRed && roundCount 对齐才抬 final=max(final,保底)，消费即清。
// · 2290 = 六槽逐项 stat_change 增益（common.h:145 all_stats_up 六槽含命中直系，
//   K439 1134 失败支同式；增益走 stat_change 真原语不吃免弱）。
// · 点亮颗数（两宏共用）= 位图点亮数 + 辰律伪雷辰（effect_des 568「辰律额外视为
//   点亮的雷辰；上限4点」——消费端按官方文字并入计数；生产端 = 2376 点亮引擎 +
//   2548①/2572，见 soul_lib/k448_wanshen_2548.cpp）。
//   位图存 pet.on_stage_storage（leichen_runtime.h 4440，「本次上场」槽，换宠引擎
//   自动清=上下场刷新从头开始；crawford_runtime.h 构造位图同形先例）。
//
// ══ boss_invalid_effects.json 记档 ══
// 2289/2290/2376 均在名单（resources/boss_invalid_effects.json）。该面只被魂印程序
// 注册口消费（soul_mark_manager boss_invalid 节点过滤；K446 头注同款记档）——本文件
// 两宏为 moves SKILL_EFFECT 注册（skills.cpp 注册口无此标签消费），boss 对局不摘除；
// 2376 soul 侧已全节点 boss_invalid=true（见 k448_leichen_engine.cpp）。**场景 8360
// 按派发纪律避开 boss 口径**（默认 is_boss_challenge=false 的普通对局验证）。
//
// ══ 动工前置四查（2026-11-10 本 worktree）══
// ① 内核硬编码：(^|[^0-9.])(2289|2290)([^0-9.]|$) 对 src/ include/ grep rc=1 零命中。
// ② effect_info 2289 args_num=2 / 2290 args_num=0 在列且 = 切片消耗（37908 [160,100] /
//   29044 []）；custom_effect_programs / overrides / custom_skill_combo_rules /
//   custom_skill_selection_routes 两 id 全 0 命中。
// ③ 全库注册并集无注册：resources/ test/ examples/ `SKILL_EFFECT\((2289|2290),` 零命中；
//   跨 worktree（k449 / 主仓）同款零命中——真缺口。registry moves=1520 无两 id。
//   ⚠️ moves 空间 2376（K419 梅赫维特 SKILL_EFFECT(2376)）与本单 soul 空间 2376
//   （effect_icon row 1966）是两个注册空间——registry 分册各查，soul 分册无 2376。
// ④ parser 覆盖面：2289「自身每点亮…威力提升{0}点」{ 前有字 → parse_effect_unit
//   主刀不命中（src/effects/effect_unit_parser.cpp:75-76 { 紧随要求）；2290 同；
//   两 id 均纯惰性注册，clone_effect 命中插件函数。
// 存储键段（leichen_runtime.h；tools/check_storage_keys.py --quiet gate 内复扫）：
//   on_stage 4440/4441/4442、plugin 4443000+side（辰律）/ 1022891000+owner（保底武装）/
//   1022891100+owner（武装回合对齐）/ 1022891200+owner（FLOOR 回调安装旗）/
//   审计 1022890000 / 1022900000（+owner*32+slot）——grep 全库零占用。
//
// ══ 审计槽布局（K437-K446 同构；plugin_storage，键 = base + owner*32 + slot；
//     *32 = K448 验收修正，见 audit_key_of 注）══
//  2289（1022890000）：slot 0 行参 amt / 1 行参 floor / 2 点亮颗数（含辰律）/ 3 存活数 /
//    4 写前威力视图 / 5 写后威力视图 / 6 保底武装量 / 7 FLOOR 写前 final / 10 FLOOR
//    写后 final / 11 累计使用计数 / 12 保底支进入次数 / 9 跳过原因（0=无/1=对手已倒）
//  2290（1022900000）：slot 0 点亮颗数 / 1 六槽调用数 / 2 六槽实变数（写后与写前差
//    非零计数，坑集 §1① 快照逐槽口径）/ 8 累计使用计数 / 9 跳过原因（0=无/1=零灯
//    如实档/2=缺原语槽）
//
// 场景回归：test/scenario/scenario_8360_leichen_engine.cpp（号段 8360-8369，确定性：
//   全量值走审计槽精确断言，概率面无——点亮/保底/全属性皆确定性通道）。
//
// ══ 变异测试（6 支，2026-11-10 本 worktree 全部真转红：改→rm .o 强制重编+显式
//    moves_plugin_deploy→单跑 8360 必红（红点全落预期断言腿）→字节级还原（cmp+md5
//    双验）→rm .o 重编+deploy→回绿）══
//   M1 2289 点亮颗数改位图-only（辰律项剔除）→ 辰律联动腿红——568 伪雷辰计数锁定；
//   M2 2289 威力改乘区（+amt×N → ×(100+amt)/100）→ 威力腿写后视图红——定点加算锁定；
//   M3 2289 保底基数改全队定值 1（存活数 → 1）→ 保底武装量红——「每存活1只」锁定；
//   M4 2290 点亮颗数改位图-only → 辰律联动腿红——计数面同源锁定；
//   M5 2290 delta 折半（lit → lit/2）→ 六槽快照红——「每点亮一颗 +1」逐字面锁定；
//   M6 2289 保底分支条件松绑（lit==0 → lit<0 恒假）→ 保底腿武装审计红——未亮门锁定。
//   （soul 侧 3 支见 k448_leichen_engine.cpp 头注；合计 9 支 ≥ 派发要求 6 支。）

#include "common.h"

#include <numerical-calculation/calculation.h>
#include <primitives/battle_primitives.h>

#include "../leichen_runtime.h"

namespace {

// 审计键跨度 32（K448 验收修正 2026-11-10）：全 K448 插件统一布局，防 slot 扩展
// 段与邻 owner 槽 0 相撞（旧 *16 布局 owner0-slot16 == owner1-slot0）。
inline int audit_key_of(int base, int owner, int slot) { return base + owner * 32 + slot; }

int storage_int_local(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

void set_storage_int_local(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// 己方存活数（含在场与场下 6 槽；count_dead 原语四口径纪律——不手写 for(slot<6)）。
int leichen_alive_count(BattleContext* ctx, int side) {
    if (!ctx || !g_core || !g_core->count_dead) {
        return 0;
    }
    const int dead = g_core->count_dead(ctx, side, DeadScope::ROSTER);
    return dead < 0 ? 0 : (6 - dead);
}

// 2289 保底武装键（FLOOR 相消费）。plugin_storage 战斗级 + roundCount 对齐键：
// 只对"武装后同回合内第一刀己方红伤"生效，消费即清（残键自愈，见文件头实现形态段）。
constexpr int k2289FloorArmRoundBase = 1022891100;   // +owner：武装回合

// FLOOR 相回调：保底伤害（减伤之后、锁伤之前——damage_pipeline.h kOrder 官方序）。
// 类别 MITIGATE（管线头注：保底/锁伤属削减面，不可被"挡伤失效"无视）。
void leichen_floor_install(BattleContext* ctx, int owner) {
    const int key = 1022891200 + owner;   // 安装旗（战斗级一次；回调内实时判门）
    if (ctx->plugin_storage.count(key)) {
        return;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->register_default_damage_effect(
        DamagePhase::FLOOR, owner, DamageEffectCategory::MITIGATE,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const auto arm = c->plugin_storage.find(k2289FloorArmBase + owner);
            if (arm == c->plugin_storage.end()) {
                return;
            }
            const int floor_amt = leichen_int_or(arm->second, 0);
            const auto arm_round = c->plugin_storage.find(k2289FloorArmRoundBase + owner);
            const int armed_round = arm_round == c->plugin_storage.end()
                                        ? -1
                                        : leichen_int_or(arm_round->second, -1);
            // 消费即清（无论是否命中本轮）：武装只服务武装后第一刀。
            c->plugin_storage.erase(arm);
            c->plugin_storage.erase(arm_round);
            DamageSnapshot& d = c->resolvedDamage;
            const bool this_attack = d.attackerId == owner && d.isRed
                                  && armed_round == c->roundCount;
            set_storage_int_local(c, audit_key_of(k2289AuditBase, owner, 12),
                                  storage_int_local(c, audit_key_of(k2289AuditBase, owner, 12), 0) + 1);
            if (!this_attack || floor_amt <= 0) {
                return;
            }
            set_storage_int_local(c, audit_key_of(k2289AuditBase, owner, 7), d.final);
            if (d.final < floor_amt) {
                d.final = floor_amt;   // 保底抬升（减伤后、锁伤前的官方时点语义）
            }
            set_storage_int_local(c, audit_key_of(k2289AuditBase, owner, 10), d.final);
        });
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 2289 —— 自身每点亮1颗雷辰威力提升{0}点；未点亮则己方每存活1只精灵造成的
//         攻击伤害不低于{1}
// 行参布局 [owner, opp, amt, floor]（args_num=2；唯一载体 37908 [160,100]）。
// 点亮颗数 > 0 → 威力支（视图定点加颗数×{0}）；== 0 → 保底支（武装 FLOOR 相，
// 保底量 = 己方存活数×{1}）。两支互斥（模板"若未点亮"字面）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_2289_leichen_power_or_floor(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2289, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2289, args,
                     "2289 布局=[owner,opp,amt,floor]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amt = args.int_args[2];
    const int floor_amt = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     2289, args, "需要 owner/opp ∈ {0,1}");
    set_storage_int_local(ctx, audit_key_of(k2289AuditBase, owner, 11),
                          storage_int_local(ctx, audit_key_of(k2289AuditBase, owner, 11), 0) + 1);
    set_storage_int_local(ctx, audit_key_of(k2289AuditBase, owner, 0), amt);
    set_storage_int_local(ctx, audit_key_of(k2289AuditBase, owner, 1), floor_amt);
    // 对手已倒（链上先行效果 1523 吸体可能击倒）→ 整条跳过（不追发、不留保底残键）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key_of(k2289AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    ElfPet& me = ctx->getPet(owner);
    const int lit = leichen_lit_count(me, ctx->plugin_storage, owner);
    set_storage_int_local(ctx, audit_key_of(k2289AuditBase, owner, 2), lit);
    leichen_floor_install(ctx, owner);
    if (lit > 0) {
        // 威力支：定点加算（4399「威力提升160点」逐字面——非 %乘区）。写前视图 0
        // 是合法物化值（37908 官方威力 0），不加 2080 的 >0 守卫。
        auto& view = ctx->ws.skill_power_view[owner];
        set_storage_int_local(ctx, audit_key_of(k2289AuditBase, owner, 4), view.value);
        const long long after = static_cast<long long>(view.value)
                              + static_cast<long long>(lit) * amt;
        view = static_cast<int>(after);   // operator= 自动置 rewritten（变威力声明）
        set_storage_int_local(ctx, audit_key_of(k2289AuditBase, owner, 5), view.value);
        set_storage_int_local(ctx, audit_key_of(k2289AuditBase, owner, 9), 0);
        return EffectResult::kOk;
    }
    // 保底支：己方每存活 1 只精灵 → 攻击伤害不低于 {1}（保底量 = 存活数×{1}）。
    // FLOOR 相武装：本刀红伤结算时由管线回调抬 final（减伤后、锁伤前）。
    const int alive = leichen_alive_count(ctx, owner);
    set_storage_int_local(ctx, audit_key_of(k2289AuditBase, owner, 3), alive);
    const int floor_total = alive * floor_amt;   // alive ≤6、{1} 全库=100 → 有界直乘
    set_storage_int_local(ctx, audit_key_of(k2289AuditBase, owner, 6), floor_total);
    ctx->plugin_storage[k2289FloorArmBase + owner] = floor_total;
    ctx->plugin_storage[k2289FloorArmRoundBase + owner] = ctx->roundCount;
    set_storage_int_local(ctx, audit_key_of(k2289AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

SKILL_EFFECT(2289, effect_2289_leichen_power_or_floor)

// ═══════════════════════════════════════════════════════════════════════════
// 2290 —— 自身每点亮1颗雷辰全属性+1
// 行参布局 [owner, opp]（args_num=0；唯一载体 29044）。点亮颗数（含辰律伪雷辰）
// = delta，六槽逐项 stat_change（common.h:145 all_stats_up 同式展开以便逐槽审计；
// 增益语义不吃免弱）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_2290_leichen_all_stats(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2290, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2290, args,
                     "2290 布局=[owner,opp]（零参载体，引擎注入双方）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 2290, args, "需要 owner∈{0,1}");
    set_storage_int_local(ctx, audit_key_of(k2290AuditBase, owner, 8),
                          storage_int_local(ctx, audit_key_of(k2290AuditBase, owner, 8), 0) + 1);
    if (!g_core || !g_core->stat_change) {
        set_storage_int_local(ctx, audit_key_of(k2290AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    ElfPet& me = ctx->getPet(owner);
    const int lit = leichen_lit_count(me, ctx->plugin_storage, owner);
    set_storage_int_local(ctx, audit_key_of(k2290AuditBase, owner, 0), lit);
    if (lit <= 0) {
        // 零颗点亮 → 如实零强化档（模板字面「每点亮1颗」；记档不造兜底）。
        set_storage_int_local(ctx, audit_key_of(k2290AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    int called = 0;
    int changed = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int before = ctx->ability_levels[owner][i];
        (void)g_core->stat_change(ctx, owner, i, lit);
        ++called;
        const int after = ctx->ability_levels[owner][i];
        if (after != before) {
            ++changed;   // 原语到顶钳住（±6）不算实变——坑集 §1② 快照逐槽口径
        }
    }
    set_storage_int_local(ctx, audit_key_of(k2290AuditBase, owner, 1), called);
    set_storage_int_local(ctx, audit_key_of(k2290AuditBase, owner, 2), changed);
    set_storage_int_local(ctx, audit_key_of(k2290AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

SKILL_EFFECT(2290, effect_2290_leichen_all_stats)
