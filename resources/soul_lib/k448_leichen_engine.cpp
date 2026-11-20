// K448 · 雷辰点亮机 soul 侧引擎 —— 魂印 2376（effect_icon row 1966，4765 怒雷神裁·雷伊）
//（场景 test/scenario/scenario_8360_leichen_engine.cpp，号段 8360-8369；2026-11-10，
//  worktree k448；调研文档 = docs_local/docs/05-任务清单/
//  雷伊雷晨点亮与武道衍化缺口调研-2026-11-09.md §一/§三·提案A 的"点亮引擎"落地）
//
// ══ 官方 tips 全文（effect_icon row 1966，args "3 1 1"，kind [1,10]，尾注（boss无效）
//    覆盖全部分句 → 全节点 boss_invalid=true，k_anomaly_gate_drop_heal 同口径）══
//   ①「自身登场后，1~4号位技能分别对应雷辰·东、雷辰·南、雷辰·西、雷辰·北；自身每次
//     使用以"雷"冠名的技能时点亮对应的未被点亮的雷辰」
//   ②「雷辰每次点亮后将与上一颗被点亮的雷辰连接并令对手麻痹且自身所有技能恢复1点PP值，
//     当自身点亮四颗雷辰时，若连接的雷辰呈"ㄣ"形则回合结束后自身解除所处的异常状态与
//     能力下降状态并进入3回合的觉醒状态，否则恢复自身全部体力值并重置自身的雷辰进度」
//   ③「回合开始时，若自身处于麻痹状态，则解除自身所处的麻痹状态且当回合所有技能先制+1」
//     （2026-11-10 现行冻结库原文；旧库"下回合"已随刷库消失——时点裁决见下）
//   ④「觉醒状态：自身免疫受到的异常状态与能力下降状态、所有技能先制+1、无视对手先制
//     等级、必定打出致命一击、造成的技能伤害额外恢复至自身体力、使用技能不受PP值限制、
//     技能必定命中且命中效果强制生效，效果结束时重置自身的雷辰进度」
//
// ══ 裁决记档（BWIKI/4399/DB 复核 2026-11-10，细节见 leichen_runtime.h 头注）══
//   · 「连击」疑点：4399 原文与 BWIKI 均无连击表述 → J 组记档系转述走样，不涉段数原语。
//   · 「以雷冠名」：BWIKI 无细则 → 保守建模技名首字=雷（leichen_named_lei 单函数仲裁口子；
//     4765 推荐套 1~4 号位 + 第五技全部雷字开头，机制自洽；嵌雷技/无雷技不点亮）。
//   · 「ㄣ」形：TapTap 配置帖实证序 1423/3241/4312/2134（1-based）= 单一手性族
//     （2 无向路径 × 2 方向）；镜像族非 ㄣ。枚举表 kZhenAwakenOrders（runtime 头）。
//   · tips③ 时点裁决（2026-11-10 K450 刷库后复核）：旧库 tips 作"下回合"、analyze 作
//     "当回合"，当时按 tips 权威落"下回合"（武装旗+次回合消费）。**现行冻结库
//     （a15eca92）tips 与 analyze 双作"当回合"**（row 1966 段②）→ 推翻条件触发，
//     改为同回合 FMR 消费（FMR 双桶均先于比较，双行动方位皆可达）。引文已同步。
//   · 觉醒七子句通道逐条（先例核对）：
//     ① 免疫异常+能力下降 = grant_immunity ANOMALY（Ancient 全 mask，diwang 3512 免疫
//       异常同参）+ STAT_DROP 两票，duration=4（触发回合 R 授予 → 覆盖窗 [R+1,R+3]），
//       ON_STAGE 域（随下场掉，与窗的 on_stage 语义一致）、source_effect_id=2376。
//     ② 先制+1 = MOVE_RIGHT 桶写 ws.preemptive_level（memset 后比较前；diwang/crawford
//       soul 同款）。
//     ③ 无视对手先制等级 = ws.priority_nullified[对手]（2026-11-10 K448 验收修正：
//       旧 guaranteed_first 会把"对手必先也压过"——必先>先制层级被冒充版抹掉；现通道
//       只归零对手先制等级，对手必先照常优先，battleFsm.cpp:1792 固定点消费；
//       2479/2017/2532 同通道先例，零内核改动）。
//     ④ 必暴 = ws.must_crit=true（j_must_crit_window / 58 圣光气同通道；暴击掷点在
//       query_usage ①.5 OR 读旗，crit_rate==0 也能必暴）。
//     ⑤ 伤害回血 = DETECT 相票据（k818 攻击命中回血同窗同法：attackerId==owner &&
//       isRed && final>0 → heal_amount(final)），窗内生效（回调实时判窗，管线条目
//       "有效性回调里实时判"纪律）。
//     ⑥ 不受 PP 限制 = ctx->ignore_pp（无为觉者 2260 同槽：PP=0 技能仍可选；窗内 FMR
//       置位，窗末 ROUND_END 对称清 + 换宠由 invalidate_on_stage_effects 引擎清）。
//     ⑦ 必中 = ws.must_hit_grant（凭证物化在 query_usage，81 必中族同通道）；**「命中
//       效果强制生效」半边无内核授予通道**（force_execute 只来自技能穿透旗/PENETRATE_
//       ATTACK 票面无 force 字段/force_execute_on_pp0 异形）→ 记档内核微任务，不阻塞；
//       「命中**才**生效」边界以 ⑤ 的 DETECT 票（isRed && final>0 才回血）承载——
//       技能被打盔/无效（无命中事实）时不回血，T4z 负腿锁定。
//     + 效果结束重置 = 窗末 ROUND_END 清位图/序列 + 清窗（本文件）。
//   · 4946 万霆神律·雷伊（魂印 2548，row 2146）= 辰律生产端**已落地**（2026-11-10
//     K448 接续段，编排指令"2548 不因量级留续单"）：四段实现见
//     resources/soul_lib/k448_wanshen_2548.cpp（①场下概率附加攒辰律 / ②场上体%
//     麻痹+吸取/汲取 / ③回合开始三分支+转化 / ④低体回满限次）。
//     本文件落其消费契约面：effect_des 568「辰律额外视为点亮的雷辰；上限4点」→ 辰律
//     层数（plugin_storage kChenLvBase+side，cap4）并入 2289/2290 计数面（官方文字
//     直读）；两生产端（2376 本文件点亮 / 2548①+2572）共写同一台账。
//
// ══ 生命周期与状态槽 ══
// 全部雷辰状态（位图/序列/觉醒窗/先制旗）存 pet.on_stage_storage（leichen_runtime.h
// 4440/4441/4442）——perform_switch 漏斗（主动中切+死亡换宠共同路径）自动清 =
// 「上下场刷新从头开始」零内核支持（调研文档 §二.3 预判裁决：成立）。换宠重置腿由
// 场景 8360 直接验收：下场→再上场位图=0。
// watcher（点亮监听）与 FLOOR 票据为战斗级常驻（回调内实时判 holder/窗，管线条目
// "有效性回调里实时判"纪律），换宠后由 on_stage 清空天然关断。
//
// ══ 审计槽（plugin_storage，键 = 1023760000 + owner*32 + slot；*32 = K448 验收修正——
//    slot16 起扩展段，旧 *16 布局下 owner0-slot16 与 owner1-slot0 相撞，T23m 镜像腿实核）══
//  0 累计点亮次数 / 1 最近点亮槽（-1 无）/ 2 当前位图 / 3 序列长度 / 4 麻痹施加结果
//  （ApplyAnomalyResult）/ 5 PP 恢复槽数 / 6 ㄣ判定（1=ㄣ 0=非ㄣ -1=未判）/ 7 觉醒已进入 /
//  8 觉醒窗末回合 / 9 回合结束分支码（1=ㄣ觉醒 2=回满+重置 3=窗末重置 0=无）/
//  10 回满体实际恢复量 / 11 觉醒解异常条数 / 12 觉醒清下降条数 / 13 麻痹自解触发 /
//  14 伤害回血累计（DETECT 票）/ 15 点亮跳过原因（1=非雷名技 2=槽位无雷辰 3=已亮）
//  16 tips③ 先制旗消费次数（当回合 FMR；K450 刷库时点裁决记档）
//
// ══ 变异测试（soul 侧 4 支，2026-11-10 本 worktree 全部真转红：改→rm .o 强制重编
//    +显式 soul_plugin_deploy→单跑 8360 必红→字节级还原（cmp+md5 双验）→重编+deploy
//    →回绿）══
//   M7 点亮位图写 soulmark_storage（on_stage→soulmark 换槽）→ 换宠重置腿红（位图
//     残留非 0）——「本次上场」槽语义锁定（调研文档 §一.6/§二.3）；
//   M8 ㄣ判定放宽（表匹配 → 任四颗）→ 非ㄣ腿分支码红（2≠1）——枚举表锁定；
//   M9 觉醒回合数 3→2 → 窗末重置腿窗值红（窗末回合≠进入+3）——3 回合窗锁定；
//   MA2 审计跨度回退 *16 → T23m 镜像腿红（side1 点亮计数 2≠1）——*32 布局实核。
//   （moves 侧 6+1 支见 k448_leichen_pool.cpp / k448_leilian.cpp / k448_leiyi_2576_2591.cpp
//   头注；合计 ≥ 派发要求 6 支。）

#include "common.h"

#include <primitives/battle_primitives.h>

#include "../leichen_runtime.h"

namespace {

constexpr int kMarkId = 2376;            // effect_icon row 1966 的 effect_id（soul 空间）
constexpr int kParalysisId = static_cast<int>(AbnormalStatusId::Paralysis);

// 免疫票固定句柄（重授同句柄 = 幂等刷新；diwang kHandleAnomaly 同式）。
constexpr int kHandleAnomaly = 23761;
constexpr int kHandleStatDrop = 23762;

// 旗标槽编码（kLeiChenFlagsKey）：bit0 = 当回合先制+1 待消费；bit8..15 = 武装回合
// （tips③ 现行冻结库双"当回合"口径，K450 刷库时点裁决，见头注）。
constexpr int kFlagPrioNext = 0x01;

// 审计键跨度 32（K448 验收修正 2026-11-10）：slot 16 起为扩展段，*16 布局下
// owner0-slot16 与 owner1-slot0 相撞（1023760000+16 双读）→ 不重叠布局 *32。
inline int audit_key_of(int owner, int slot) { return k2376AuditBase + owner * 32 + slot; }

// ── 宿主行参读取（k_boostcount_attach host_row/db_at 同构；SOP §一.3 db_at 纪律）──
// 2376 官方行参 "3 1 1" → 位映射（措辞锚点）：
//   db_at(row,0)=觉醒持续回合（des444「进入3回合的觉醒状态」）
//   db_at(row,1)=点亮 ride-along 的全技能 PP 恢复点数（tips①「恢复所有技能1点PP值」）
//   db_at(row,2)=tips③ 麻痹自解的当回合先制+1 量
// 读法 = 宿主宠 soulMark.args.owned_int_args 原序（跨 watcher 读同魂印另一宠时
// 以 find_pet_with_soulmark 定位宿主，不读 side 级缓存）。
struct LeiHostRow {
    const std::vector<int>* db = nullptr;
    bool valid = false;
};
inline LeiHostRow lei_host_row(BattleContext* ctx, int owner) {
    LeiHostRow row;
    if (!ctx || owner < 0 || owner > 1) {
        return row;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}
inline int lei_db_at(const LeiHostRow& row, int idx, int fallback) {
    if (!row.valid || !row.db || idx < 0 || idx >= static_cast<int>(row.db->size())) {
        return fallback;   // 缺位/无行参 → 官方默认（3/1/1 措辞锚点）
    }
    return (*row.db)[idx];
}

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

void bump_storage_int_local(BattleContext* ctx, int key) {
    set_storage_int_local(ctx, key, storage_int_local(ctx, key, 0) + 1);
}

// 在场宠是否雷辰宿主（soulMark.id 判，不硬编码宠物 id——魂印随宠走）。
bool holder_on_stage(BattleContext* ctx, int owner) {
    return ctx && owner >= 0 && owner <= 1 && ctx->on_stage[owner] >= 0
        && ctx->on_stage[owner] < 6 && ctx->getPet(owner).soulMark.id == kMarkId;
}

// 点亮引擎核心：槽位点亮 + 连接（序列追加）+ 对手麻痹 + 自身全技能 PP+1。
// 由 EVENT_SKILL_USED watcher 驱动（E13「使用即发」锚点：miss/被封也算"使用"，
// 469 注册法同源；J291 493 窗同款 watcher）。
void leichen_on_skill_used(BattleContext* c, int owner, const BattleEvent& ev) {
    if (!c || ev.actor != owner || !holder_on_stage(c, owner)) {
        return;
    }
    if (c->is_boss_challenge) {
        return;   // 尾注（boss无效）→ watcher 手动门（hamo_3809_ao 双保险同款）
    }
    ElfPet& me = c->getPet(owner);
    if (!leichen_named_lei(ev.skill_name)) {
        set_storage_int_local(c, audit_key_of(owner, 15), 1);
        return;   // 非雷冠名技：不点亮（保守口径，见文件头裁决记档）
    }
    // 技能 → 槽位：按 skill_id 在宿主 5 槽技能表匹配（事件载荷只有 id/名）。
    int slot = -1;
    for (int i = 0; i < 5; ++i) {
        if (me.skills[static_cast<std::size_t>(i)].id == ev.skill_id) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        set_storage_int_local(c, audit_key_of(owner, 15), 2);
        return;   // 不在宿主技能表（异常态，防御档）
    }
    if (slot >= kLeiChenSlotCount) {
        set_storage_int_local(c, audit_key_of(owner, 15), 2);
        return;   // 第五技无对应雷辰（tips①「1~4号位」字面）
    }
    const int state = leichen_state_of(me);
    if (leichen_bitmap(state) & (1 << slot)) {
        set_storage_int_local(c, audit_key_of(owner, 15), 3);
        return;   // 对应雷辰已亮 → 不重复点亮（tips「未被点亮的」字面，
    }             //  也不触发点亮 ride-along——"每次点亮后"语义）
    // 点亮：位图置位 + 序列追加（连接 = 相邻点亮序）。
    leichen_set_state(me, leichen_light_slot(state, slot));
    bump_storage_int_local(c, audit_key_of(owner, 0));
    set_storage_int_local(c, audit_key_of(owner, 1), slot);
    set_storage_int_local(c, audit_key_of(owner, 2), leichen_bitmap(leichen_state_of(me)));
    set_storage_int_local(c, audit_key_of(owner, 3), leichen_seq_len(leichen_state_of(me)));
    // ride-along：令对手麻痹（官方模板无概率字 → 100% 现代施加；crawford 同 duration=-1）。
    if (g_core && g_core->apply_anomaly) {
        const ApplyAnomalyResult r =
            g_core->apply_anomaly(c, 1 - owner, kParalysisId, /*duration=*/-1, owner);
        set_storage_int_local(c, audit_key_of(owner, 4), static_cast<int>(r));
    }
    // ride-along：自身所有技能恢复 1 点 PP（只补不削、无限 PP 槽跳过——原语内查）。
    if (g_core && g_core->pp_restore) {
        // 行参 db_at(row,1) = 全技能 PP 恢复点数（官方行 "3 1 1" 第 2 位；缺位回退 1）。
        const int pp_amt = lei_db_at(lei_host_row(c, owner), 1, 1);
        const int changed = g_core->pp_restore(c, owner, pp_amt, /*only_empty=*/false);
        set_storage_int_local(c, audit_key_of(owner, 5), changed);
    }
    // 觉醒窗内不再推进评估（位图满时窗内不可再亮，见 ROUND_END 节点分支序）。
}

// ── 节点①：ENTER_EXIT_STAGE（ROSTER）—— 接线（watcher + DETECT 回血票）────────
EffectResult leichen_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int wired_key = 1023760100 + owner;   // 接线旗（战斗级一次）
    if (!ctx->plugin_storage.count(wired_key)) {
        ctx->plugin_storage.emplace(wired_key, 1);

        // 点亮监听（battle 级常驻；回调内实时判 holder + boss 手动门）。
        ctx->event_center_.register_watcher(
            EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                leichen_on_skill_used(c, owner, ev);
            },
            WatcherScope::TEAM);

        // 觉醒子句⑤：造成的技能伤害额外恢复至自身体力（DETECT 相票据，k818 同窗同法）。
        ctx->register_default_damage_effect(
            DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
            [owner](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner || !g_core || !g_core->heal_amount) {
                    return;
                }
                if (c->is_boss_challenge || !holder_on_stage(c, owner)) {
                    return;
                }
                ElfPet& me = c->getPet(owner);
                if (!leichen_awaken_active(me, c->roundCount)) {
                    return;   // 只在觉醒窗内（子句属"觉醒状态"段）
                }
                const DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                    return;
                }
                g_core->heal_amount(c, owner, d.final);
                bump_storage_int_local(c, audit_key_of(owner, 14));
            });
    }
    return EffectResult::kOk;
}

// ── 节点②：ROUND_START（STAGE）—— tips③ 麻痹自解 + 下回合先制+1 武装 ──────────
EffectResult leichen_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !holder_on_stage(ctx, a.owner)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    ElfPet& me = ctx->getPet(owner);
    if (g_core && g_core->has_active_anomaly
        && g_core->has_active_anomaly(ctx, owner, kParalysisId)) {
        // 解除自身麻痹（名单式解除，只清麻痹一种——cure 原语圣甲·盖亚 544 同式）。
        if (g_core->cure_anomalies) {
            const int cured = g_core->cure_anomalies(ctx, owner, &kParalysisId, 1);
            set_storage_int_local(ctx, audit_key_of(owner, 13), cured > 0 ? 1 : 0);
        }
        // 当回合所有技能先制+1：武装旗带武装回合（同回合 FMR 消费——现行 DB
        // tips+analyze 双"当回合"，K450 刷库定案，见节点③与头注裁决记档）。
        const int flags = leichen_flags_of(me);
        leichen_set_flags(me, (flags & ~0xFFFF) | kFlagPrioNext
                                  | ((ctx->roundCount & 0xFF) << 8));
    }
    return EffectResult::kOk;
}

// ── 节点③：FIRST_MOVE_RIGHT（STAGE）—— 先制旗消费 + 觉醒窗逐回合重写 ──────────
EffectResult leichen_move_right(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !holder_on_stage(ctx, a.owner)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    ElfPet& me = ctx->getPet(owner);
    const int flags = leichen_flags_of(me);
    // tips③ 消费门（2026-11-10 K450 刷库定案）：现行冻结库 tips+analyze 双「当回合」
    // （effect_icon row 1966 段②）→ SOP §六.1 tips 权威落**当回合**（旧库"下回合"
    // 已随刷库消失）。武装回合 == 当前回合 → 本回合 FMR 桶（双桶均先于比较）消费。
    if ((flags & kFlagPrioNext) && ((flags >> 8) & 0xFF) == (ctx->roundCount & 0xFF)) {
        // 行参 db_at(row,2) = 当回合先制+1 量（官方行第 3 位；缺位回退 1）。
        ctx->ws.preemptive_level[owner] += lei_db_at(lei_host_row(ctx, owner), 2, 1);
        leichen_set_flags(me, flags & ~kFlagPrioNext);
        bump_storage_int_local(ctx, audit_key_of(owner, 16));
    }
    if (leichen_awaken_active(me, ctx->roundCount)) {
        // 觉醒子句②③④⑥⑦（每回合重写；ws 每回合 reset，写点在 memset 后比较前）。
        ctx->ws.preemptive_level[owner] += 1;       // ② 所有技能先制+1
        // ③ 无视对手先制等级 = **对手先制等级归零**（priority_nullified 通道，非必先）：
        //    咤克斯 2017 / 圣光格拉奥瑞 2532 / k_high_dmg_convert 同通道——效果在 FMR 桶写
        //    ws.priority_nullified[对手]，内核在双 FMR 桶之后、先制比较之前的固定点统一
        //    清零该方先制等级（battleFsm.cpp:1792-1794，2479"先制效果失效"同点）。
        //    比较语义=我方等级 vs 对手归零：自带先制+1（子句②）即压过对手任意先制；
        //    对手**必先（guaranteed_first）不受影响**（必先比较更靠前，官方"必先>先制"
        //    层级保留）——这正是与旧 guaranteed_first 冒充版的判别差（T4x 锁定）。
        ctx->ws.priority_nullified[1 - owner] = true;
        ctx->ws.must_crit[owner] = true;            // ④ 必定打出致命一击
        // ⑥ 使用技能不受 PP 值限制：ignore_pp 已在**觉醒进入时**置位（ctx 字段不随
        //    回合 memset，窗内持续有效；FMR 重复置位是晚期信号——验收修正移除）。
        ctx->ws.must_hit_grant[owner] = true;       // ⑦ 必中半边（强制生效半边记档）
    }
    return EffectResult::kOk;
}

// ── 节点④：ROUND_END（STAGE）—— 四颗判定 / 回满重置 / 窗末重置 ────────────────
EffectResult leichen_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !holder_on_stage(ctx, a.owner)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    ElfPet& me = ctx->getPet(owner);
    // 分支序：先窗（窗内位图满且不可再亮，不重复评估），再四颗判定。
    const int awaken_end = leichen_awaken_end_of(me);
    if (awaken_end > 0) {
        if (ctx->roundCount >= awaken_end) {
            // 觉醒效果结束：重置雷辰进度（tips④ 尾句）+ ignore_pp 对称清
            // （该旗非 ws，不随回合 reset；窗末不换宠路径由此清，换宠路径由引擎清）。
            leichen_set_state(me, kLeiChenResetState);
            leichen_set_awaken_end(me, 0);
            ctx->ignore_pp[owner] = false;
            set_storage_int_local(ctx, audit_key_of(owner, 9), 3);
        }
        return EffectResult::kOk;
    }
    const int state = leichen_state_of(me);
    if (leichen_seq_len(state) < kLeiChenSlotCount) {
        return EffectResult::kOk;   // 未满四颗：本回合无结算
    }
    // 四颗全亮 → 连线形状判定（点亮序枚举，kZhenAwakenOrders）。
    bool zhen = false;
    for (const auto& order : kZhenAwakenOrders) {
        bool match = true;
        for (int i = 0; i < kLeiChenSlotCount; ++i) {
            if (leichen_seq_slot(state, i) != order[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            zhen = true;
            break;
        }
    }
    set_storage_int_local(ctx, audit_key_of(owner, 6), zhen ? 1 : 0);
    if (zhen) {
        // ㄣ形 → 解自身异常 + 解能力下降 + 进入 3 回合觉醒（窗 [R+1, R+3]）。
        if (g_core && g_core->dispel_active_anomalies) {
            const int n = g_core->dispel_active_anomalies(ctx, owner);
            set_storage_int_local(ctx, audit_key_of(owner, 11), n);
        }
        if (g_core && g_core->clear_stat_drops) {
            const int n = g_core->clear_stat_drops(ctx, owner);
            set_storage_int_local(ctx, audit_key_of(owner, 12), n);
        }
        // 行参 db_at(row,0) = 觉醒持续回合（des444「进入3回合的觉醒状态」；缺位回退 3）。
        const int awaken_rounds = lei_db_at(lei_host_row(ctx, owner), 0, 3);
        const int end_round = ctx->roundCount + awaken_rounds;
        leichen_set_awaken_end(me, end_round);
        set_storage_int_local(ctx, audit_key_of(owner, 7), 1);
        set_storage_int_local(ctx, audit_key_of(owner, 8), end_round);
        set_storage_int_local(ctx, audit_key_of(owner, 9), 1);
        // ③ PP 信号前移（K448 验收修正 2026-11-10）：ignore_pp 在**觉醒进入时**置位——
        // ctx 字段不随回合 memset，窗 [R+1,R+3] 的技能可选性判定（skills.cpp:745/771）
        // 读的是选择期信号，FMR 才置位会漏掉觉醒首回合的真实选择入口。清点 = 窗末
        // ROUND_END 对称清（下方）+ 换宠 invalidate_on_stage_effects 引擎清。
        ctx->ignore_pp[owner] = true;
        set_storage_int_local(ctx, audit_key_of(owner, 17), 1);
        // 子句① 免疫票：异常（Ancient 全 mask，diwang 免疫异常同参）+ 能力下降。
        const int immune_rounds = awaken_rounds + 1;   // 触发回合余段 + 窗 [R+1,R+n]
        ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                            /*anomaly_mask=*/0, /*duration_rounds=*/immune_rounds, kHandleAnomaly,
                            /*soul_immunity=*/true, EffectScope::ON_STAGE,
                            /*counts=*/0, /*source_effect_id=*/kMarkId,
                            ImmunityTier::Ancient);
        ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                            /*anomaly_mask=*/0, /*duration_rounds=*/immune_rounds, kHandleStatDrop,
                            /*soul_immunity=*/true, EffectScope::ON_STAGE,
                            /*counts=*/0, /*source_effect_id=*/kMarkId,
                            ImmunityTier::Modern);
    } else {
        // 非 ㄣ → 恢复自身全部体力值 + 重置雷辰进度（effect_des 444 原文口径）。
        if (g_core && g_core->heal_amount) {
            const int max_hp = std::max(1, me.numericalBase[NumericalPropertyIndex::HP]);
            const int before = me.hp;
            g_core->heal_amount(ctx, owner, max_hp);   // 封回血吃原语内查；上限封顶原语内钳
            set_storage_int_local(ctx, audit_key_of(owner, 10), me.hp - before);
        }
        leichen_set_state(me, kLeiChenResetState);
        set_storage_int_local(ctx, audit_key_of(owner, 9), 2);
    }
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &leichen_arm,
                        /*once=*/false, /*early=*/false, SoulScope::ROSTER,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &leichen_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &leichen_move_right,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &leichen_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
