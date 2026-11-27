// K415 光环/低体翻倍双宠族 —— effect 1950/1960/2346（3 宏 / 6 行，双宠行 args 驱动，
// 看板 K415 2026-11-04，a01-k415，worktree ~/.agent-workspaces/.../agents/k415；
// 号段 7750-7759。通道=1950 条件闪避=对手 round-effect 查询（has_round_effects，
// mortarion_4541 foe_has_round_effects 同款）+ ws.dodge_rate（K397 897/1149 直系）
// +命中后计数附加（dump 对参）；1960 双宠行 args 驱动+文本互证（row1599 概率翻倍支
// /row2025 db[0]=0 行判别转命中+1 支）；2346 光环=ROSTER 存活数实时查（K401 720①
// alive_backpack 扫槽同款）×AMP/REDUCE 双票+heal 吸血（K401 720②/K410 ③ 票形直系）
// ——boss 尾注按 dump 行文子句归属判（seg1「仅boss有效」=反向尾注 boss-only、seg2
// 「boss有效」=显式正注 boss_invalid=false、1930 seg3「boss无效」=boss_invalid=true）。
//
// 行清单（tools/soul_effects_dump.py 1950 1960 2346，2026-11-04；dump 全文为权威）：
//   1950 尤莉米特 4364 两行（row1588 本体 args[100,120,1] kind[4,5,17] / row2080
//   「星罗拂沙伙伴效果」args[101,120,1] kind[5,18]）：
//     seg1（两行同文，未命中支仅 row2080 有——args/text 双锚）：
//       「自身出战时获得群辉加护；自身拥有群辉加护时，若对手处于回合类效果则自身
//        闪避对手的技能，若对手的技能命中自身则命中后对手每有1个与此技能类型不同的
//        技能自身恢复{db1}点体力」+（row2080 独有）「，若对手的技能未命中自身则此次
//        技能结束时本次在场每闪避成功1次，吸取对手{db1}点体力」
//     seg2（板行未列 → 划界记档 1 跳过不产码）：「己方其他精灵首次出战时自身获得
//       {db2}层群芳加护（row1588 带（boss无效）尾注）…群芳加护机整天段」——db[2]
//       解码备档不消费（K410 划界记档 1/K409 1398 燃血段直系）。
//     → db[0]=回合类门闪避率%（100 本体/101 伙伴行——>100 行文无概率字样=恒闪避档，
//       ws.dodge_rate=db/100 后 hit_chance=accuracy-db<0 恒 MISS，args 原样驱动）、
//       db[1]=命中计数回复点数=未命中吸取点数（两行同 120，文本双锚「恢复120点体力」
//       /「吸取对手120点体力」）、db[2]=群芳加护层数（1，仅 seg2 消费——本单跳过）。
//   1960 双宠行（row1599 普拉薇安 4374 args[50,1,200] kind[0,1,11] / row2025
//   烟火派派 4835 args[0,1,60] kind[11,5]）：
//     row1599：「回合开始时若自身体力低于对手则自身使用攻击技能造成的伤害{db0}%
//       概率翻倍，未触发则令自身全属性+{db1}；|当回合若对手先出手则战斗阶段结束时
//       {吸取}对手{db2}点{体力}（boss有效）」
//     row2025：「回合开始时若自身体力低于对手则自身攻击命中时全属性+{db1}；战斗阶段
//       结束时若自身为后手方则吸取对手{db2}点体力|（boss有效）」
//     → 文本互证：db[0]=翻倍申报概率%（50 概率支开 / **0=行判别锚**——概率支关，
//       转文字面「攻击命中时全属性+1」支，两行 db[1] 同为 +1 增量互锚）、
//       db[1]=全属性增量（1）、db[2]=战阶末吸取点数（200/60 档位差）。
//     尾注：row2025 的「（boss有效）」独占 seg2 段=罩全行；row1599 seg1 无注 + seg2
//       显式正注 → 全节点 boss_invalid=false（K410 1035「boss有效显式正注」同判）。
//   2346 双行（row1929 皮神 4731 args[5,0,5] kind[3,0,5] / row1930 闪光皮神 4732
//   args[5,5,5,7] kind[3,0,5]）：
//     seg1「boss对战中自身拥有神话（仅boss有效）」——**反向尾注**：仅 boss 有效
//       （与「boss无效」语义相反，看板点名 dump 实证后落）→ 节点 boss_invalid=false
//       + 回调内手动门 is_boss_challenge（hamo_3809_ao watcher 手动门同形；
//       K400 票内双保险口径）。
//     seg2「己方每有一只其他存活精灵受到的攻击伤害额外减少{db0}%（，造成的攻击伤害
//       额外提升{db1}%——仅 row1930 有此句），造成伤害的{db2}%恢复至自身体力，若存活
//       的精灵为皮皮、比波、波克尔则上述效果中的百分比分别提升为10%、11%、12%
//       （boss有效）」——显式正注 → boss_invalid=false。
//       → db[0]=每友受伤减免%（5）、db[1]=每友攻击增伤%（row1929=0=行判别锚：文本无
//         增伤句 / row1930=5）、db[2]=每友吸血%（5）、db[3]=神羽片数（仅 row1930；
//         row1929 无此参 fallback 0=神羽支关）。
//     seg3（仅 row1930）「战斗开始时，自身获得{db3}片神羽，之后己方其他精灵的回合
//       结束后，若其体力低于最大体力的1/2且低于自身，则消耗一片神羽以令其体力值增加
//       至与自身相等（boss无效）」→ boss_invalid=true。
//
// ⚠️ 口径记档（majority reading，均按先例；本单通道列=权威处方）：
//   1.【1950 seg2 群芳加护整段跳过】板行 1950 通道只列「条件闪避+命中后计数附加」，
//     seg2（群芳首登场叠层+双固伤+双 20% 叠层增幅机整天段）板行未引 → 记档跳过不产码
//     （K410 划界记档 1/K409 1398/K406 1463 seg3 直系先例）；db[2] 解码备档不消费。
//   2.【1950 未命中吸取支照落】「若对手的技能未命中自身则…吸取」与命中回复支同句链
//     同 | 段（命中/未命中分支对），且是条件闪避通道的自然消费端（闪避计数）——随
//     seg1 落地，吸取量=每闪避 1 次 db[1] 点（deal_pink_damage(FIXED)+heal_amount
//     等量组合、钳不越过存量，K410 1035③/K302/K338 口径）。
//   3.【1950 闪避门时点】「若对手处于回合类效果」= ROUND_START 恒判重写（K397 897
//     「条件持续语义→每回合重授」同形；ws.dodge_rate 每回合 reset）；回合中途新挂到
//     对手的回合类效果不回溯武装本回合闪避（majority，记档）。观测面=has_round_effects
//     （battleContext O(1) 活计数，mortarion_4541 foe_has_round_effects 同源）。
//   4.【1950「闪避成功」计数】= 对手技能 EVENT_SKILL_INVALID（miss/封属同出口，skills.cpp
//     query_usage 统一 emit 点）target=自身 计数，入场清零； majority 直读（不区分
//     miss 成因——官方行文未限定闪避成因，测试面封属不存在）。命中支回复=EVENT_HIT
//     （actor=对手、target=自身）计数「与此技能类型不同的技能」：对手在场宠 5 槽静态
//     type vs 本次使用技物化类型 ws.skill_type_view[对手]（四点五钦定槽）异型计数——
//     静态表 vs 物化值 majority 互锚（变型技组合测试面不存在，记档）。
//   5.【1960 概率支相位】「使用攻击技能造成的伤害…概率翻倍」= AMP 相位票内单掷
//     （K400 1237「票内掷」判例；K191 禁预掷双掷勘误）；成功 → d.addPct+=100 +
//     d.final=d.final×200/100（K397 947「翻倍=+100% 增伤乘区」口径、K401 720② 票形
//     直系）；失败 → 当刀 stat_change 六槽+db[1]（「未触发则令自身全属性+1」字面逐刀
//     分支，K400 未触发补偿同形）。门=ROUND_START 严格低于（恰等不结，K377 口径），
//     每回合重写旗（K410 ① live 读同源）。db[0]=0 行判别 → 概率支不注册工作、转
//     DETECT 命中支（final>0 才 +1，K410 ③「被挡刀不结」同位，majority 记档）。
//   6.【1960 吸取门】「当回合若对手先出手」/「自身为后手方」同判=ROUND_END 读
//     is_second_mover(owner)（ws.preemptive_right MOVE_RIGHT 结算值；双 NONE→NONE
//     档按先手处理=不吸，battleContext is_second_mover 注释口径，记档）。
//   7.【2346 升档表】「皮皮、比波、波克尔→10%/11%/12%」为行文常量（args 无此参）——
//     按物种 id 表替换该友**全部三类贡献**的基础百分比（「上述效果中的百分比分别提升」
//     字面罩 seg2 全部百分比；物种 id 全集=monsters 表同名 id：皮皮{10,3295,5375,
//     1400016,1400700}/比波{11,5376,1400015}/波克尔{12,5377,1400014}，测试面用正典
//     10/11/12）。减伤=Σ 后单次乘区（K401 720③ REDUCE_PCT 加法叠加同款）；增伤=Σ
//     后 d.addPct+=Σ + final×(100+Σ)/100（K401 720② 票形）；吸血=d.final×Σ/100
//     （DETECT 相位 final 已过 BLOCK，K410 1764③ 同位；「造成伤害」=攻击红伤 majority，
//     同 K406 1463「上次攻击伤害」台账口径记档）。
//   8.【2346 光环实时查】ROSTER 存活友=每刀回调内扫 6 槽（跳自身槽/已消逝/hp<=0，
//     K401 720① alive_backpack_excluding_self 同款）实时求 Σ——「每有一只」随战局
//     活变化，不做登场快照（板行「实时查」点名）。票 TEAM+回调 onstage 门（K410 票形
//     ——ON_STAGE 票在 boss 局被初始化增量 epoch 作废（T7758 实测勘误），TEAM 无此
//     问题且离场由 onstage 门天然关闸）。
//   9.【2346 神羽支】「战斗开始时获得」→ enter(early) 授 db[3] 片（首次入场=battle
//     start majority，记档）；「己方其他精灵的回合结束后」→ FIRST/SECOND_AFTER_ACTION_
//     END 双桶 + SoulScope::ROSTER（恒注册跨切换，soul_mark_manager「ROSTER 恒注册
//     /TEAM 跨切换保留」语义——宿主场下也活着，官方语义自洽）；消耗 heal 走 heal_amount
//     真原语（吃封回血，K401 2087② 同款）；门=hp*2<max（防截断式，K409 1398 同式）
//     且 hp<皮神 hp 严格。⚠️ 锚点勘误（T7759 实测）：NONE/切宠/被控行动走
//     should_skip_action_flow 分支**不经过** AFTER_ACTION_END 桶——"回合结束后"按
//     "行动方真出手后的行动末"判（majority：未行动无"回合结束"可言，被控回合官方
//     亦不触发行动末类效果），测试面用出手回合钉锚点。
//   10.【2346 boss 双向】seg1 神话=boss 局才置（enter 回调 is_boss_challenge 手动门，
//     非 boss 局显式写 0 旗=测试观测面）；seg2 光环=boss 局照常（显式正注）；seg3
//     神羽=boss_invalid=true 节点级（boss 局不注册不执行）——双向对照测试
//     T7756/T7757（boss 局生效+非 boss 局不生效）。
//
// 三查记录（a01-k415 2026-11-04，worktree 内复核）：
//   ①内核 grep -w 1950/1960/2346（src/ include/）：1950/2346 零命中；1960 命中均为
//     **技能空间**（skills.cpp case 1960 希拓·神煌炎舞斩 + battle_primitives 注释引
//     用）——魂印/技能双注册空间常识（K401 2087 case 同判），魂印行为零硬编码；
//   ②effect_info 1950/1960/2346 全为技能空间异文模板（1950「命中后吸取对手最大体力
//     的{0}%…偶数翻倍」/1960「击败对手则令自身{0}回合内…强化无法被消除或吸取」/
//     2346「英雄之耀达到3层…」）≠ effect_icon.tips → tips 权威（SOP §六.1，
//     K377/K391/K401 同款记档）；effect_icon kind 已核（dump [4,5,17]/[5,18]/
//     [0,1,11]/[11,5]/[3,0,5]×2）；
//   ③registry soul 基线 grep -c "^SOUL_PROGRAM(" = 384，soul_lib 全库无
//     SOUL_PROGRAM(1950/1960/2346、存储键段 1019500000/1019600000/1023460000 全库
//     grep 无撞（tools/check_storage_keys.py 过门）、场景号段 7750-7759 全库无撞——
//     真缺口。4399 旁证：dump come 空/「星罗拂沙伙伴效果」，行文本即权威文。
// 注册：SOUL_PROGRAM ×3（1950/1960/2346 入账，只增不改），soul 384→387。
// 场景回归：test/scenario/scenario_7750_aura_lowhp_family.cpp（号段 7750-7759）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark1950 = 1950;
constexpr int kMark1960 = 1960;
constexpr int kMark2346 = 2346;

constexpr int kOwnerStride = 16;   // 每 owner 键段步长（槽 0..15，K410/K411 同款）

// ── 存储键段（10^9 + id×10^4 + owner*16 + 槽；全库 grep 无撞）─────────────────
constexpr int k1950Base = 1019500000;
constexpr int k1950GateAudit = 0;   // 审计槽：本回合回合类门判定（1=开/0=关）
constexpr int k1950Wired = 1;       // watcher 接线旗（防多次入场重复挂）
constexpr int k1950DodgeCnt = 2;    // 本次在场闪避成功计数（入场清零）
constexpr int k1950CntAudit = 3;    // 审计槽：命中支异型技能计数（最近一次）
constexpr int k1950HealAudit = 4;   // 审计槽：命中支申报回复量（最近一次）
constexpr int k1950DrainAudit = 5;  // 审计槽：未命中支申报吸取量（最近一次）

constexpr int k1960Base = 1019600000;
constexpr int k1960GateAudit = 0;   // 审计槽：本回合低体门判定（1=开/0=关）
constexpr int k1960Wired = 1;       // 管线票接线旗
constexpr int k1960PctAudit = 2;    // 审计槽：翻倍申报概率（掷前写）
constexpr int k1960RollAudit = 3;   // 审计槽：本刀翻倍掷点结果（1=中/0=败）
constexpr int k1960StatAudit = 4;   // 审计槽：本刀 +db[1] 申报（1=掷败补偿/命中支）
constexpr int k1960DrainAudit = 5;  // 审计槽：战阶末吸取申报量（0=未吸）
constexpr int k1960DblCnt = 6;      // 翻倍落地计数（统计带侧证）

constexpr int k2346Base = 1023460000;
constexpr int k2346Myth = 0;        // 神话旗（1=boss 局已授/0=非 boss 局）
constexpr int k2346Wired = 1;       // 光环票接线旗
constexpr int k2346RedAudit = 2;    // 审计槽：本刀受伤减免 Σ%（最近一次）
constexpr int k2346AmpAudit = 3;    // 审计槽：本刀攻击增伤 Σ%（最近一次）
constexpr int k2346HealAudit = 4;   // 审计槽：本刀吸血申报量（最近一次）
constexpr int k2346Feather = 5;     // 神羽余量（片）
constexpr int k2346FHealAudit = 6;  // 审计槽：神羽消耗回复申报量（最近一次）
constexpr int k2346FUseCnt = 7;     // 神羽消耗计数（侧证）

// 升档物种表（行文常量 10/11/12%，口径记档 7；id 全集=monsters 表同名行）。
constexpr int kPipiIds[] = {10, 3295, 5375, 1400016, 1400700};       // 皮皮→10%
constexpr int kBiBoIds[] = {11, 5376, 1400015};                      // 比波→11%
constexpr int kBokeIds[] = {12, 5377, 1400014};                      // 波克尔→12%
constexpr int kPipiPct = 10;
constexpr int kBiBoPct = 11;
constexpr int kBokePct = 12;

constexpr const char* kAuraMarkName = "群辉加护";   // 1950 seg1 登场印记
constexpr const char* kMythMarkName = "神话";       // 2346 seg1 boss 局印记

// ── 共用小件（HostRow/db_at 原序读 owned_int_args，K397/K400/K410 定例）────────

struct HostRow {
    const std::vector<int>* db = nullptr;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const SoulNodeArgs& a, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// 回调内 live 读行参（K404/K409/K410 零 arm 期快照口径）。
const std::vector<int>* live_row_db(BattleContext* ctx, int owner, int mark_id) {
    if (!ctx || owner < 0 || owner > 1 || ctx->on_stage[owner] < 0
        || ctx->on_stage[owner] > 5) {
        return nullptr;
    }
    const ElfPet& host = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    if (host.soulMark.id != mark_id) {
        return nullptr;
    }
    return &host.soulMark.args.owned_int_args;
}

int live_db_at(const std::vector<int>* db, int idx, int fallback) {
    if (db && idx >= 0 && idx < static_cast<int>(db->size())) {
        return (*db)[static_cast<std::size_t>(idx)];
    }
    return fallback;
}

int read_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

void write_int(BattleContext* ctx, int key, int v) {
    ctx->plugin_storage[key] = v;
}

// 定点吸取组合（K410 1035③ 直系：deal_pink_damage(FIXED)+heal_amount 等量，
// 钳「吸取不越过存量」K302/K338 口径）。返回实际吸取量。
int drain_fixed(BattleContext* ctx, int actor, int target, int amount) {
    if (!ctx || !g_core || !g_core->deal_pink_damage || !g_core->heal_amount) {
        return 0;
    }
    if (amount <= 0 || target < 0 || target > 1) {
        return 0;
    }
    int amt = amount;
    const int foe_hp = ctx->getPet(target).hp;
    if (amt > foe_hp) {
        amt = foe_hp;   // 不越过存量
    }
    if (amt <= 0) {
        return 0;
    }
    (void)g_core->deal_pink_damage(ctx, target, amt, DamageKind::FIXED, actor);
    (void)g_core->heal_amount(ctx, actor, amt);
    return amt;
}

// 全属性+delta（六项含命中，2026-09-18 口径；stat_change 真原语，
// jiyuan_ds001_4797 ④ 同式）。
void all_stats_change(BattleContext* ctx, int owner, int delta) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_change(ctx, owner, i, delta);
    }
}

// ════════════════ 1950 尤莉米特（群辉加护条件闪避+命中计数附加）════════════════

// ROUND_START：群辉在身 且 对手处于回合类效果 → ws.dodge_rate=db[0]/100（K397 897
// 赋值形）+ 审计槽门旗；否则旗 0（每回合重写，口径记档 3）。
EffectResult k415_1950_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1950)) {
        return EffectResult::kOk;
    }
    const int base = k1950Base + owner * kOwnerStride;
    const ElfPet& self = ctx->getPet(owner);
    const bool aura_on = get_mark_count(self, kAuraMarkName) > 0;
    const bool foe_rounded = ctx->has_round_effects(ctx->opponent(owner));
    const int gate = (aura_on && foe_rounded) ? 1 : 0;
    write_int(ctx, base + k1950GateAudit, gate);
    if (!gate) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1950);
    const int dodge_pct = db_at(row, 0, 100);
    if (dodge_pct > 0) {
        ctx->ws.dodge_rate[owner] = dodge_pct / 100.0f;
    }
    return EffectResult::kOk;
}

// enter(early)：授群辉加护印记 + 闪避计数清零（本次在场）+ 首次接线双 watcher。
EffectResult k415_1950_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    ElfPet& self = ctx->getPet(owner);
    // 「自身出战时获得群辉加护」：入场防重（enter(early) 战斗开始/入场双触发实测）
    // → 已有印记不叠加（presence 语义，majority；记档）。
    if (get_mark_count(self, kAuraMarkName) == 0) {
        add_mark_stacks(self, kAuraMarkName, 1);
    }
    write_int(ctx, k1950Base + owner * kOwnerStride + k1950DodgeCnt, 0);

    const int wired_key = k1950Base + owner * kOwnerStride + k1950Wired;
    if (read_int(ctx, wired_key) == 1) {
        return EffectResult::kOk;   // watcher 只挂一次（TEAM 常驻 + onstage 门）
    }
    write_int(ctx, wired_key, 1);
    const int opp = ctx->opponent(owner);

    // 命中支：对手技能命中自身 → 异型技能计数 × db[1] 回复（口径记档 4）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_HIT, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, opp](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->heal_amount) {
                return;
            }
            if (ev.actor != opp || ev.target != owner) {
                return;   // 只认对手技能命中自身
            }
            if (!onstage_has_soul_mark(c, owner, kMark1950)) {
                return;
            }
            const std::vector<int>* db = live_row_db(c, owner, kMark1950);
            const int per = live_db_at(db, 1, 120);
            if (per <= 0) {
                return;
            }
            // 对手在场宠 5 槽静态 type vs 本次使用技物化类型（口径记档 4）。
            const int used_type = c->ws.skill_type_view[opp];
            const ElfPet& foe = c->seerRobot[opp].elfPets[c->on_stage[opp]];
            int diff = 0;
            for (const Skills& sk : foe.skills) {
                if (sk.id >= 0 && static_cast<int>(sk.type) != used_type) {
                    ++diff;
                }
            }
            const int heal = per * diff;
            write_int(c, k1950Base + owner * kOwnerStride + k1950CntAudit, diff);
            write_int(c, k1950Base + owner * kOwnerStride + k1950HealAudit, heal);
            if (heal > 0) {
                (void)g_core->heal_amount(c, owner, heal);
            }
        },
        WatcherScope::TEAM);

    // 未命中支（row2080）：对手技能未命中自身 → 闪避计数 +1、按计数吸取（口径记档 2）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner, opp](BattleContext* c, const BattleEvent& ev) {
            if (!c) {
                return;
            }
            if (ev.actor != opp || ev.target != owner) {
                return;   // 只认对手技能未命中自身
            }
            if (!onstage_has_soul_mark(c, owner, kMark1950)) {
                return;
            }
            const int base = k1950Base + owner * kOwnerStride;
            const int cnt = read_int(c, base + k1950DodgeCnt) + 1;
            write_int(c, base + k1950DodgeCnt, cnt);
            const int per = live_db_at(live_row_db(c, owner, kMark1950), 1, 120);
            const int drain = (per > 0) ? drain_fixed(c, owner, opp, per * cnt) : 0;
            write_int(c, base + k1950DrainAudit, drain);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ════════════════ 1960 低体翻倍/命中+1 双宠行 ═════════════════════════════════

// ROUND_START：低体门（严格低于，恰等不结 K377）→ 门旗每回合重写（口径记档 5）。
EffectResult k415_1960_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1960)) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const ElfPet& foe = ctx->getPet(ctx->opponent(owner));
    const bool gate = self.hp > 0 && foe.hp > 0 && self.hp < foe.hp;
    write_int(ctx, k1960Base + owner * kOwnerStride + k1960GateAudit, gate ? 1 : 0);
    return EffectResult::kOk;
}

// enter(early)：接线双票（TEAM+wired，K410 形）：
//   ① AMP 票：db[0]>0 行 → 门内掷 db[0]%，中 = 翻倍（addPct+=100、final×2），
//     败 = 六槽 +db[1]（口径记档 5）；db[0]==0 行 → 票内直退（行判别）。
//   ② DETECT 票：db[0]==0 行 → 门内攻击命中（final>0）六槽 +db[1]。
EffectResult k415_1960_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1960Base + owner * kOwnerStride;
    if (read_int(ctx, base + k1960Wired) == 1) {
        return EffectResult::kOk;   // enter 早挂双触发防重（K391/K409/K410 同款）
    }
    write_int(ctx, base + k1960Wired, 1);

    // ① 概率翻倍票（AMP 相位，票内单掷——K400 1237「票内掷」判例）。
    ctx->register_default_damage_effect(
        DamagePhase::AMP, owner, DamageEffectCategory::AMP,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;   // 翻倍要写 final（K401 720② 形）
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤判（K410 ① 同门）
            }
            if (!onstage_has_soul_mark(c, owner, kMark1960)) {
                return;
            }
            const int key_base = k1960Base + owner * kOwnerStride;
            if (read_int(c, key_base + k1960GateAudit) != 1) {
                return;   // 低体门未开（回合开始时体力不低于对手）
            }
            const std::vector<int>* db = live_row_db(c, owner, kMark1960);
            const int pct = live_db_at(db, 0, 50);
            if (pct <= 0) {
                return;   // db[0]=0 行判别：概率支关（row2025，文本互证）
            }
            write_int(c, key_base + k1960PctAudit, pct);   // 掷前落账（K662/F9 782）
            if (!roll_soul_percent(*c, kMark1960, 0, owner, d.defenderId, pct)) {
                write_int(c, key_base + k1960RollAudit, 0);
                const int delta = live_db_at(db, 1, 1);
                if (delta > 0) {
                    all_stats_change(c, owner, delta);   // 未触发 → 全属性+db[1]
                    write_int(c, key_base + k1960StatAudit, delta);
                }
                return;
            }
            write_int(c, key_base + k1960RollAudit, 1);
            write_int(c, key_base + k1960DblCnt,
                      read_int(c, key_base + k1960DblCnt) + 1);   // 统计带侧证
            d.addPct += 100;
            d.final = d.final * 200 / 100;   // 翻倍=+100% 乘区（K397 947 口径）
        });
    // ② 命中+1 票（DETECT；db[0]==0 行判别消费，口径记档 5）。
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark1960)) {
                return;
            }
            const int key_base = k1960Base + owner * kOwnerStride;
            const std::vector<int>* db = live_row_db(c, owner, kMark1960);
            const int pct = live_db_at(db, 0, 50);
            if (pct > 0) {
                return;   // 概率支行不走命中+1 支（文本互证：两支互斥）
            }
            if (read_int(c, key_base + k1960GateAudit) != 1) {
                return;
            }
            const int delta = live_db_at(db, 1, 1);
            if (delta > 0) {
                all_stats_change(c, owner, delta);
                write_int(c, key_base + k1960StatAudit, delta);
            }
        });
    return EffectResult::kOk;
}

// ROUND_END：对手先出手（=自身后手，is_second_mover）→ 吸取 db[2] 点（口径记档 6）。
EffectResult k415_1960_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1960)) {
        return EffectResult::kOk;
    }
    const int base = k1960Base + owner * kOwnerStride;
    write_int(ctx, base + k1960DrainAudit, 0);   // 每回合重写（0=未吸）
    if (!ctx->is_second_mover(owner)) {
        return EffectResult::kOk;   // 自身先手/NONE 档 → 不吸（口径记档 6）
    }
    const HostRow row = host_row(ctx, a, kMark1960);
    const int amount0 = db_at(row, 2, 0);
    if (amount0 <= 0) {
        return EffectResult::kOk;
    }
    const int drained =
        drain_fixed(ctx, owner, ctx->opponent(owner), amount0);
    write_int(ctx, base + k1960DrainAudit, drained);
    return EffectResult::kOk;
}

// ════════════════ 2346 皮神光环（ROSTER 实时查×双票+吸血+神羽）════════════════

// 单友贡献百分比：物种升档表替换基础值（口径记档 7）。
int ally_pct(const ElfPet& ally, int base_pct) {
    if (base_pct <= 0) {
        return 0;
    }
    for (const int id : kPipiIds) {
        if (ally.id == id) {
            return kPipiPct;
        }
    }
    for (const int id : kBiBoIds) {
        if (ally.id == id) {
            return kBiBoPct;
        }
    }
    for (const int id : kBokeIds) {
        if (ally.id == id) {
            return kBokePct;
        }
    }
    return base_pct;
}

// 其他存活友的 Σ 贡献（实时扫 6 槽：跳自身槽/已消逝/hp<=0，口径记档 8）。
int sum_ally_pct(BattleContext* ctx, int owner, int base_pct) {
    if (base_pct <= 0 || !ctx) {
        return 0;
    }
    const int self_slot = ctx->on_stage[owner];
    int sum = 0;
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == self_slot || ctx->is_vanished(owner, slot)) {
            continue;
        }
        const ElfPet& ally = ctx->seerRobot[owner].elfPets[slot];
        if (ally.hp > 0) {
            sum += ally_pct(ally, base_pct);
        }
    }
    return sum;
}

// enter(early)：神话旗（boss-only 手动门，双向观测面）+ 光环三票接线 + 神羽授片。
EffectResult k415_2346_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k2346Base + owner * kOwnerStride;

    // seg1 神话（仅 boss 有效=反向尾注，口径记档 10）：boss 局置 1+印记；非 boss 显式 0。
    if (ctx->is_boss_challenge) {
        write_int(ctx, base + k2346Myth, 1);
        add_mark_stacks(ctx->getPet(owner), kMythMarkName, 1);
    } else {
        write_int(ctx, base + k2346Myth, 0);
    }

    // seg3 神羽授片（db[3]>0 才有；「战斗开始时」=首次入场 majority，口径记档 9）。
    // seg3 带（boss无效）尾注：授片随子句 boss 门——boss 局不授（消耗双桶已由节点级
    // boss_invalid=true 拦，此处授片在 boss_invalid=false 的 enter 载体上 → 手动门）。
    if (!ctx->is_boss_challenge) {
        const HostRow row = host_row(ctx, a, kMark2346);
        const int feathers = db_at(row, 3, 0);
        if (feathers > 0) {
            write_int(ctx, base + k2346Feather, feathers);
        }
    }

    // seg2 光环三票（K410 票形：TEAM+wired 防重挂+回调 onstage 门——ON_STAGE 票在
    // boss 局会被初始化增量 epoch 作废（T7758 实测），TEAM 票无此问题；记档 8 同步）。
    if (read_int(ctx, base + k2346Wired) == 1) {
        return EffectResult::kOk;
    }
    write_int(ctx, base + k2346Wired, 1);

    // ① 攻击增伤票（AMP）：Σ db[1] 贡献 → addPct+=Σ、final×(100+Σ)/100。
    ctx->register_default_damage_effect(
        DamagePhase::AMP, owner, DamageEffectCategory::AMP,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;   // 增伤要写 final（K401 720② 形）
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark2346)) {
                return;
            }
            const int amp =
                sum_ally_pct(c, owner,
                             live_db_at(live_row_db(c, owner, kMark2346), 1, 0));
            write_int(c, k2346Base + owner * kOwnerStride + k2346AmpAudit, amp);
            if (amp <= 0) {
                return;
            }
            d.addPct += amp;
            d.final = d.final * (100 + amp) / 100;
        });
    // ② 受伤减免票（REDUCE_PCT）：Σ db[0] 贡献 → final×(100-Σ)/100（钳 Σ≤99）。
    ctx->register_default_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;   // 减免要写 final（K401 720③ 形）
            if (d.defenderId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark2346)) {
                return;
            }
            int red =
                sum_ally_pct(c, owner,
                             live_db_at(live_row_db(c, owner, kMark2346), 0, 5));
            if (red > 99) {
                red = 99;   // 防御性钳（小面板达不到，记档）
            }
            write_int(c, k2346Base + owner * kOwnerStride + k2346RedAudit, red);
            if (red <= 0) {
                return;
            }
            d.final = d.final * (100 - red) / 100;
        });
    // ③ 吸血票（DETECT）：d.final×Σ db[2] 贡献/100 → heal_amount（口径记档 7）。
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->heal_amount) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark2346)) {
                return;
            }
            const int heal_pct =
                sum_ally_pct(c, owner,
                             live_db_at(live_row_db(c, owner, kMark2346), 2, 5));
            const int heal = d.final * heal_pct / 100;
            write_int(c, k2346Base + owner * kOwnerStride + k2346HealAudit, heal);
            if (heal > 0) {
                (void)g_core->heal_amount(c, owner, heal);
            }
        });
    return EffectResult::kOk;
}

// 神羽消耗（己方其他精灵回合结束后；双桶 ROSTER 恒注册——宿主场下也活，口径记档 9）。
// 门：行动宠=宿主以外己方在场宠 且 其 hp*2<max 且 hp<皮神 hp 且 神羽>0
//     → 消耗 1 片，heal_amount(行动宠, 皮神.hp - 行动宠.hp)。
EffectResult k415_2346_feather_tick(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k2346Base + owner * kOwnerStride;
    const int host_slot = ctx->find_pet_with_soulmark(owner, kMark2346);
    if (host_slot < 0 || host_slot > 5) {
        return EffectResult::kOk;   // 宿主已消逝/不在列——神羽支随宿主失效（记档）
    }
    const int acting_slot = ctx->on_stage[owner];
    if (acting_slot == host_slot) {
        return EffectResult::kOk;   // 行动者是宿主自身 → 非「其他精灵」（字面）
    }
    int feathers = read_int(ctx, base + k2346Feather);
    if (feathers <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& acting = ctx->seerRobot[owner].elfPets[acting_slot];
    const ElfPet& host = ctx->seerRobot[owner].elfPets[host_slot];
    const int max_hp = resolve_pet_max_hp(acting);
    if (acting.hp <= 0 || max_hp <= 0) {
        return EffectResult::kOk;
    }
    if (acting.hp * 2 >= max_hp) {
        return EffectResult::kOk;   // 未低于最大体力的 1/2（防截断式）
    }
    if (acting.hp >= host.hp) {
        return EffectResult::kOk;   // 未低于自身（皮神）体力
    }
    const int heal = host.hp - acting.hp;   // 「增加至与自身相等」
    if (heal <= 0) {
        return EffectResult::kOk;
    }
    --feathers;
    write_int(ctx, base + k2346Feather, feathers);
    write_int(ctx, base + k2346FUseCnt, read_int(ctx, base + k2346FUseCnt) + 1);
    write_int(ctx, base + k2346FHealAudit, heal);
    (void)g_core->heal_amount(ctx, owner, heal);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（1950/1960/2346 入账，只增不改）──
// boss 尾注归属（dump 行文子句判，口径记档 10）：1950 seg1/1960 全行/2346 seg1+seg2
// 无（boss无效）注 → boss_invalid=false；2346 seg3（神羽，仅 row1930 有行）→ true。
SOUL_PROGRAM(kMark1950, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k415_1950_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/false),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k415_1950_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/false)});
SOUL_PROGRAM(kMark1960, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k415_1960_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/false),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k415_1960_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/false),
                         SOUL_NODE(State::BATTLE_ROUND_END, &k415_1960_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/false)});
SOUL_PROGRAM(kMark2346, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k415_2346_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/false),
                         SOUL_NODE(State::BATTLE_FIRST_AFTER_ACTION_END,
                              &k415_2346_feather_tick,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER,
                              /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_SECOND_AFTER_ACTION_END,
                              &k415_2346_feather_tick,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER,
                              /*boss_invalid=*/true)});
