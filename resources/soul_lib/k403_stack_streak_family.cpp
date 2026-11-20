// K403 叠层/连续计数族 —— effect 1772/1680/1856（3 宏 / 6 行，双行档位差）
//（看板 K403「叠层/连续计数族 2026-11-02 编排挖掘」，a01-knight7，
//  worktree ../agents/k403；号段 7600-7609；
//  通道=层数账本=plugin_storage（K162 逐回合 tick 范式）+「连续同型计数」=K240/K382
//  ledger 直系 + stat_change=K4 + pp_reduce/pp_restore 原语（K6/5830 口径）+
//  粉伤=deal_pink_damage + 六异常申报=attach_random_anomalies（K2 通道收口、
//  chaos_mozun_1263/k266 同槽；K164 116 同窗 EVENT_SKILL_USED））
//
// 行清单（tools/soul_effects_dump.py 1772 1680 1856，2026-11-02）：
//   1772 暮月之弦 4178 两行（kind 同 [10,11,12]、args 计数同 8——纯 args 档位差，无
//   运行期行判别需求，绑定行自带 db，见【行判别】记档）：
//     row1403 本体 args[5,100,1,1,1,10,5,1]：「回合开始时为对手附加一层暮月玄音，
//       最高5层|自身攻击技能命中后，若当回合自身为先手方则攻击命中后100%附加烧伤、
//       冻伤、中毒、麻痹、害怕、睡眠中的1种异常状态，未触发则令自身全属性+1，若当
//       回合自身为后手方则令对手全属性-1|（boss无效）」
//     row2111 伙伴行（霜月悬天伙伴效果）args[10,100,2,1,1,10,5,1]：同文但最高10层、
//       「中的2种异常状态」。
//     → db[0]=暮月玄音上限（5/10）；db[1]=附加申报率（100 恒）；db[2]=附加种类数
//       （1/2）；db[3]=未触发自身全属性+1 量（1，tips 互锚）；db[4]=后手对手全属性
//       -1 量（1，tips 互锚）；db[5]=10 / db[6]=5——**两行恒同、本宏 tips 无对应子句**
//       → 不消费（K240/K1「解析不了不猜」纪律）。旁证：4399 载体页 gloss「暮月玄音：
//       命中率降低10%，攻击技能5%的几率失效」与 db[5]/db[6] 恰合，但 effect_icon tips
//       （dump 权威）与看板行均无此子句 → 记档待官方行文参数化后再迁，不动码。
//       db[7]=1 旗不消费（K1 纪律）。
//   1680 环·圣芒 4088 两行（kind [4,11,14]/[11,14] 互异但两行子句结构全同，差异全在
//   args 档位——同上无需运行期判别）：
//     row1330 本体 args[1,1,6,30,40,20,3,1]：「回合开始时，使自身防御+1、特防+1；
//       自身受到攻击后，若双防提升等级总和≥6则当回合战斗阶段结束时附加自身最大体力
//       值30%的百分比伤害；自身被击败时，己方下只精灵出战时恢复最大体力的40%（…），
//       之后3回合内每回合结束时此精灵恢复最大体力的20%（…）（boss无效）」
//     row2138 进化行 args[2,2,0,40,40,20,3,1]：防/特防+2、门≥0、40% 粉伤，其余同文。
//     → db[0]=防+（1/2）、db[1]=特防+（1/2）、db[2]=门（6/0）、db[3]=粉伤%（30/40）
//       ——四参即看板行两子句全量；db[4]=40%/db[5]=20%/db[6]=3 回合=「自身被击败时…
//       出战恢复+后续回复」子句（看板行范围外）→ **记档跳过不产码**（K3 1057 先例：
//       板行未列子句记档），含 环小队成员 50%/25% 分支；db[7]=1 旗不消费。
//   1856 提西弗涅 4270 两行（kind 同 [5,10,15]、args 档位差）：
//     row1492 本体 args[1,3,1,30,1,5,10,1]：「对手连续使用相同类型的技能后减少对手
//       所有技能1点PP值，自身连续使用相同类型的技能后{吸取}对手最大{体力}的1/3并
//       恢复自身所有技能1点PP值；|自身每次使用技能后有30%的概率令对手害怕，回合开始
//       时自身每有1个技能PP值≥5则概率提升10%（boss无效）」
//     row1760 伙伴行（暗之女神伙伴效果）args[2,3,2,50,1,5,10,1]：PP 1→2 点、恢复
//       1→2 点、害怕 30%→50%。
//     → db[0]=对手全技 PP-（1/2）、db[1]=吸取分母（3 恒，"1/3"互锚）、db[2]=自身全技
//       PP 恢复（1/2）——三参即看板行两子句全量；db[3]=害怕率（30/50）/db[4]=每技
//       爬坡旗/db[5]=PP≥5 门/db[6]=+10% 步=「30%/50% 害怕概率爬坡」子句（看板行范围
//       外）→ **记档跳过不产码**；db[7]=1 旗不消费。
//
// ⚠️ 口径记档（majority reading，均按先例）：
//   【行判别】三宏双行全部为 **args 档位差**（1772/1856 两行 kind 相同、args 计数相同，
//     1680 两行 kind 互异但子句结构全同）——绑定行自带 db（bind_soulmark_args），实现
//     纯 args 驱动，无运行期行判别需求（K400 1237「args 档位差常规行≠宏」同形；场景
//     侧以两行参数化直证行≠宏）。
//   【1772① 叠层】暮月玄音 = plugin_storage 层数账本（看板通道列钦定，K162 逐回合
//     tick 范式）：ROUND_START +1 层、钳 db[0] 上限；对手 hp>0 才挂（K367 口径）。
//     账本按攻方（效果宿主）owner 键、**不随对手换宠重置**（官方无重置文句，K240
//     "切换不断连"同口径；暮月玄音无 tips 内消费面——4399 gloss 的每层效果见 db[5]
//     /db[6] 记档，不在本宏行文内）。
//   【1772② 窗口】"自身攻击技能命中后" = EVENT_SKILL_USED own+非属性技（看板通道列
//     钦定 K164 116 同窗；E13 使用即发、miss 也算——K396 ③定例；K382 ②的 DETECT
//     读法系"命中对账"需伤害值，本宏申报不需，不采）。
//   【1772② 先手/后手】is_second_mover(owner)（K169 直系；ws.preemptive_right 在
//     MOVE_RIGHT 时点已结算，SKILL_USED 时读当回合值；NONE 按先手处理=内核同款）。
//     先手支：attach_random_anomalies（池=tips 序六码 {烧伤2,冻伤5,中毒1,麻痹0,
//     害怕6,睡眠8}，count=db[2]，pct=db[1]；100% 短路不耗概率 rand、只耗挑池 rand）
//     ——K401 888 主池三选一直系、"随机进入…中的 N 种"模板族原语收口；**未触发=
//     landed==0 才补偿**（db[2]=2 时部分落地不补偿，majority：子句级概率门+落地账
//     见 battle_primitives.h RandomAnomalyResult 注释；免异常夹具下两支同败、场景
//     无差）。后手支：对手六槽 stat_drop（amount=db[4]，K1210 drop_all_six 同款，
//     查免弱）。两支互斥（若/则结构）。
//   【1772② 未触发补偿】"未触发则令自身全属性+1"：六槽 stat_change(+db[3])（K4
//     真原语；"全属性"=六项能力等级含命中——chaos_sorensen/jiyuan_ds001 口径；
//     stat_change 不查免弱=自身增益钦定通道）。
//   【1680①】ROUND_START stat_change(防槽2 +db[0]) + stat_change(特防槽3 +db[1])
//     （K4 直系；±6 钳制内核自理；等级存 context 不随换宠清——battleFsm sync 注释）。
//   【1680② 门】"自身受到攻击后"=EVENT_TAKE_DAMAGE（target==own/actor≠own/amount>0/
//     攻击伤害时点，K241/K400 ②同门）；"双防提升等级总和"=ability_levels[own][2]+
//     [3] **raw 和**（负等级照算——"提升等级总和"字面，记档）；达门 → 置
//     k1680Pend=roundCount 戳；"当回合战斗阶段结束时"=ROUND_END 桶（K302 定例、
//     K400 ④同）：戳==roundCount 且自身存活 → 对手侧 deal_pink_damage(自身maxHP×
//     db[3]/100, PERCENT_VALUE)（K400 ④/K201 334 同 kind；"百分比伤害"以 dump 行文
//     为准，4399 异文"固定伤害"不采——SOP §六.1）；多段/多次受击当回合只结一次
//     （子句为回合级结算，戳覆盖，记档）；阵亡不结（K367 口径）。
//   【1856① 类型账本】"相同类型"=ws.skill_type_view[actor]（物化类型，0=物理/1=特殊
//     /2=属性三类分型——K382 ①"记录…技能的类型"同读法；battleFsm.cpp:530 在
//     SKILL_USED 发出前物化、当次用技可读；K240 的 used_attribute 二分读法系 776
//     行文明示"攻击/属性"二类，本宏行文无明示 → 取三类的 K382 majority）。账本
//     plugin_storage 侧账（own/opp 各一，键 +owner；侧级跟踪=对手换宠不断连，K382
//     同口径记档；换宠不重置=K240"无异型使用插入即不断连"）。
//   【1856① 对手支】EVENT_SKILL_USED actor==opp：type==last（且 last≥0）→
//     pp_reduce(对手, db[0])（"减少对手所有技能 PP"——内核原语：钳 0、无限 PP 槽跳过、
//     逐槽 emit EVENT_PP_REDUCED；5830"PP 操作一律走原语"口径）。首用建立基线不减。
//   【1856① 自身支】EVENT_SKILL_USED actor==own：type==last（且 last≥0）→
//     ①吸取：drain=min(对手maxHP/db[1], 对手当前 hp)（"吸取不越过存量"K302/K338/K382
//     ③口径）→ deal_pink_damage(FIXED) + heal_amount 等量（K382 ③同形）；②恢复：
//     pp_restore(自身, db[2], only_empty=false)（"恢复自身所有技能 N 点 PP"——钳
//     maxPP、无限 PP 槽跳过；只补不削）。属性技也入账（"使用技能"无系别过滤——
//     K114"使用所有技能"口径；场景以属性双连直证）。
//   【boss 尾注】三行 dump 行文尾注（boss无效）全部**全句尾** → 三宏全节点
//     boss_invalid=true（K400 ⑨惯例：boss 局节点不注册不执行）。
//
// ⚠️ 三查记录（a01-knight7 2026-11-02，worktree 内复核）：
//   ①内核 grep -rw 1772/1680/1856（src/ include/）零魂印硬编码；
//   ②effect_info 三 id 均为技能空间异文模板（1772"附加对手能力下降等级总和…固定伤"
//     /1680"对手不处于异常状态时附加{0}点真实伤害"/1856"全属性+{0}，若强化前…"）
//     ——effect_icon tips 为权威（SOP §六.1）；
//   ③全 feat 分支并集 git grep 无 SOUL_PROGRAM(1772/1680/1856)；场景号段 7600-7609
//     全分支无撞（现存 760/7620 为异段）；registry 基线以 --dump-registry-exit 落盘
//     后比对 soul 只增不改。
// 存储键段：9917720xxxx（1772）/ 9916800xxxx（1680）/ 9918560xxxx（1856）+owner，
//   tools/check_storage_keys.py 全绿；plugin_storage 键 int 空间。
// 注册：SOUL_PROGRAM ×3（1772/1680/1856 入账，只增不改），soul 369→372
//   （--dump-registry-exit 实数：本文件入账后 registry soul=372）。
// 场景回归：test/scenario/scenario_7600_moonmark_1772.cpp /
//   scenario_7603_ring_1680.cpp / scenario_7606_tisiphone_1856.cpp（号段 7600-7609）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

// ── 存储键段（+owner；tools/check_storage_keys.py 已核无撞）──────────────────
constexpr int k1772WireSkill = 991772000;   // SKILL_USED watcher 已挂旗
constexpr int k1772Stacks = 991772010;      // 暮月玄音层数账本（逐回合 tick，钳 db[0]）
constexpr int k1772AuditPct = 991772020;    // 六异常申报概率（审计槽，恒 100）
constexpr int k1772Land = 991772030;        // 申报落地计数（统计带侧证）
constexpr int k1772Fb = 991772040;          // 未触发→全属性+1 补偿计数

constexpr int k1680WireTake = 991680000;    // TAKE_DAMAGE watcher 已挂旗
constexpr int k1680Pend = 991680010;        // 粉伤结算戳（=roundCount，0/缺=无）
constexpr int k1680Pink = 991680020;        // 粉伤落地计数（侧证）

constexpr int k1856WireSkill = 991856000;   // SKILL_USED watcher 已挂旗
constexpr int k1856OwnLast = 991856010;     // 自身上次用技类型账本（-1=未使用）
constexpr int k1856OppLast = 991856020;     // 对手上次用技类型账本（-1=未使用）
constexpr int k1856Drain = 991856030;       // 自身同型吸取计数（侧证）
constexpr int k1856Cut = 991856040;         // 对手同型 PP 削减计数（侧证）

// 1772 六异常池（tips 行文序：烧伤、冻伤、中毒、麻痹、害怕、睡眠；码=abnormal-types.h）。
constexpr int k1772Pool[6] = {
    static_cast<int>(AbnormalStatusId::Burn),       // 2 烧伤
    static_cast<int>(AbnormalStatusId::Frostbite),  // 5 冻伤
    static_cast<int>(AbnormalStatusId::Poison),     // 1 中毒
    static_cast<int>(AbnormalStatusId::Paralysis),  // 0 麻痹
    static_cast<int>(AbnormalStatusId::Fear),       // 6 害怕
    static_cast<int>(AbnormalStatusId::Sleep),      // 8 睡眠
};
constexpr int k1772PoolSize = 6;

// ── 共用小件（db_at 原序读 owned_int_args = 方案 B，K172/K193/K400 定例）──────

struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
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
    const ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    row.db = &host.soulMark.args.owned_int_args;
    row.monster_id = host.soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

void write_int(BattleContext* ctx, int key, int v) {
    ctx->plugin_storage[key] = v;
}

int read_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

// "全属性±N"（六项能力等级：0攻 1特攻 2防 3特防 4速 5命中——速查 §三；chaos_sorensen
// /jiyuan_ds001 "全属性=六项" 口径）。自身增益走 stat_change（不查免弱）、对手施加走
// stat_drop（amount=正数降幅，amount<=0 INVALID_PARAM——K1210 记档）。
void boost_all_six(BattleContext* ctx, int target, int delta) {
    if (!ctx || !g_core || !g_core->stat_change || delta == 0) {
        return;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_change(ctx, target, i, delta);   // 自身增益：不查免弱钦定通道
    }
}

void drop_all_six(BattleContext* ctx, int target, int amount, int actor) {
    if (!ctx || !g_core || !g_core->stat_drop || amount <= 0) {
        return;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, target, i, amount, actor);
    }
}

// 攻击伤害事件时点（K185/K395/K400 同款私副本）。
bool attack_damage_event(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1772 暮月之弦：回合开始为对手附加一层暮月玄音（max db[0]）；
//   攻击技窗：先手 → 100% 附加六异常之一/之二（未触发→自身全属性+db[3]）；
//             后手 → 对手全属性-db[4]
// ═════════════════════════════════════════════════════════════════════════════

// enter：接线攻击技窗（watcher 一场一次；账本跨换宠不清——记档【1772①】）。
EffectResult k403_1772_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->attach_random_anomalies || !g_core->stat_drop
        || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->plugin_storage.count(k1772WireSkill + owner)) {
        return EffectResult::kOk;   // early 双触发防重（k189/K400 同款）
    }
    ctx->plugin_storage.emplace(k1772WireSkill + owner, 1);
    const HostRow row = host_row(ctx, a, 1772);
    const int anomaly_pct = db_at(row, 1, 100);   // db[1]，兜底字面 100
    const int anomaly_cnt = db_at(row, 2, 1);     // db[2]，兜底字面 1 种
    const int fb_boost = db_at(row, 3, 1);        // db[3]，兜底 tips 字面 +1
    const int back_drop = db_at(row, 4, 1);       // db[4]，兜底 tips 字面 -1
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, anomaly_pct, anomaly_cnt, fb_boost, back_drop](BattleContext* c,
                                                               const BattleEvent& ev) {
            if (!c || !g_core || !g_core->attach_random_anomalies || !g_core->stat_drop
                || !g_core->stat_change) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;                            // 攻击技窗（K164 116 同窗，记档②）
            }
            if (!onstage_has_soul_mark(c, owner, 1772)) {
                return;
            }
            const int opp = 1 - owner;
            if (c->is_second_mover(owner)) {
                // 后手支：对手全属性 -db[4]（六槽 stat_drop 查免弱；无申报概率、不写审计槽）。
                drop_all_six(c, opp, back_drop, owner);
                return;
            }
            // 先手支：100% 附加六异常中 db[2] 种（attach_random_anomalies 单掷子句级
            // 概率门——K164 116 同窗申报；100% 档概率门短路不耗 rand，只耗挑池 rand）。
            write_int(c, k1772AuditPct + owner, anomaly_pct);   // 审计槽：掷前写（K662 形）
            const std::vector<int> pool(k1772Pool, k1772Pool + k1772PoolSize);
            const RandomAnomalyResult r = g_core->attach_random_anomalies(
                c, opp, pool, anomaly_cnt, anomaly_pct, owner, ChanceSource::SoulMark);
            if (r.landed > 0) {
                write_int(c, k1772Land + owner, read_int(c, k1772Land + owner) + 1);
                return;
            }
            // 未触发（landed==0，100% 档只可能免异常/抗性/弹控）→ 自身全属性 +db[3]。
            boost_all_six(c, owner, fb_boost);
            write_int(c, k1772Fb + owner, read_int(c, k1772Fb + owner) + 1);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ROUND_START：暮月玄音层数账本逐回合 tick（+1 钳 db[0]；对手在场才挂）。
EffectResult k403_1772_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, 1772)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - a.owner;
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;                       // 对手无在场活体不挂（K367 口径）
    }
    const HostRow row = host_row(ctx, a, 1772);
    const int max_stacks = db_at(row, 0, 5);            // db[0]，兜底本体字面 5
    if (max_stacks <= 0) {
        return EffectResult::kOk;
    }
    const int key = k1772Stacks + a.owner;
    const int cur = read_int(ctx, key);
    int next = cur + 1;
    if (next > max_stacks) {
        next = max_stacks;                              // "最高 N 层"钳制
    }
    write_int(ctx, key, next);
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1680 环·圣芒：回合开始防+db[0]/特防+db[1]；受攻击后双防提升等级总和≥db[2]
//   → 当回合战斗阶段结束附加自身 maxHP×db[3]% 百分比伤害（PERCENT_VALUE）
// ═════════════════════════════════════════════════════════════════════════════

// enter：接线受击门（TAKE_DAMAGE 攻击伤害时点；K241/K400 ②同门）。
EffectResult k403_1680_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->plugin_storage.count(k1680WireTake + owner)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(k1680WireTake + owner, 1);
    const HostRow row = host_row(ctx, a, 1680);
    const int gate = db_at(row, 2, 6);                  // db[2]，兜底本体字面 6
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, gate](BattleContext* c, const BattleEvent& ev) {
            if (!c) {
                return;
            }
            if (ev.target != owner || ev.actor == owner || ev.amount <= 0
                || !attack_damage_event(ev)) {
                return;                                 // 只认"被对手攻击命中"
            }
            if (!onstage_has_soul_mark(c, owner, 1680)) {
                return;
            }
            // 双防提升等级总和（槽 2=防御、3=特防；raw 和，负等级照算——记档）。
            const int sum = c->ability_levels[owner][2] + c->ability_levels[owner][3];
            if (sum < gate) {
                return;                                 // 门未达
            }
            write_int(c, k1680Pend + owner, c->roundCount);   // 当回合战阶末结算戳
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ROUND_START：自身防/特防自升（K4 直系）。
EffectResult k403_1680_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1680)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 1680);
    const int def_up = db_at(row, 0, 1);                // db[0]，兜底字面 +1
    const int spdef_up = db_at(row, 1, 1);              // db[1]，兜底字面 +1
    if (def_up != 0) {
        (void)g_core->stat_change(ctx, owner, /*防御槽=*/2, def_up);
    }
    if (spdef_up != 0) {
        (void)g_core->stat_change(ctx, owner, /*特防槽=*/3, spdef_up);
    }
    return EffectResult::kOk;
}

// ROUND_END：战阶末结算戳消费 → 对手侧 PERCENT_VALUE 粉伤（自身 maxHP×db[3]%）。
EffectResult k403_1680_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1680)) {
        return EffectResult::kOk;
    }
    const int key = k1680Pend + owner;
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end() || any_int_or(it->second, -1) != ctx->roundCount) {
        return EffectResult::kOk;                       // 本回合门未达（无戳/旧戳）
    }
    ctx->plugin_storage.erase(it);                      // 一次性消费
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;                       // 阵亡不结（K367 口径）
    }
    const int opp = 1 - owner;
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 1680);
    const int pct = db_at(row, 3, 30);                  // db[3]，兜底本体字面 30%
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    const int amount = resolve_pet_max_hp(ctx->getPet(owner)) * pct / 100;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE, owner);
    write_int(ctx, k1680Pink + owner, read_int(ctx, k1680Pink + owner) + 1);
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1856 提西弗涅：对手连续同型技 → 对手全技 PP-db[0]；自身连续同型技 →
//   吸取对手 maxHP/db[1]（FIXED 粉伤+等量回复，不越存量）+ 自身全技 PP+db[2]
// ═════════════════════════════════════════════════════════════════════════════

EffectResult k403_1856_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->pp_reduce || !g_core->pp_restore
        || !g_core->deal_pink_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->plugin_storage.count(k1856WireSkill + owner)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(k1856WireSkill + owner, 1);
    const HostRow row = host_row(ctx, a, 1856);
    const int pp_cut = db_at(row, 0, 1);                // db[0]，兜底字面 1
    const int drain_denom = db_at(row, 1, 3);           // db[1]，兜底 tips 字面 1/3
    const int pp_up = db_at(row, 2, 1);                 // db[2]，兜底字面 1
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pp_cut, drain_denom, pp_up](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->pp_reduce || !g_core->pp_restore
                || !g_core->deal_pink_damage || !g_core->heal_amount) {
                return;
            }
            if (ev.actor != owner && ev.actor != 1 - owner) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, 1856)) {
                return;
            }
            // 类型 = ws.skill_type_view（物化三类分型 0物理/1特殊/2属性——记档；当次
            // 用技在 SKILL_USED 发出前已物化，battleFsm.cpp:530 锚）。
            const int type = c->ws.skill_type_view[ev.actor];
            if (ev.actor == 1 - owner) {
                // ── 对手支：连续同型 → 减对手全技 PP db[0]（原语：钳 0/无限槽跳过）──
                const int key = k1856OppLast + owner;
                const int last = read_int(c, key);
                const bool has_last = c->plugin_storage.count(key) > 0;
                if (has_last && last >= 0 && last == type && pp_cut > 0) {
                    (void)g_core->pp_reduce(c, 1 - owner, pp_cut);
                    write_int(c, k1856Cut + owner, read_int(c, k1856Cut + owner) + 1);
                }
                write_int(c, key, type);   // 账本无条件更新（异型断连，K240 口径）
                return;
            }
            // ── 自身支：连续同型 → 吸取 maxHP/db[1]（不越存量）+ 自身全技 PP+db[2] ──
            const int key = k1856OwnLast + owner;
            const int last = read_int(c, key);
            const bool has_last = c->plugin_storage.count(key) > 0;
            if (has_last && last >= 0 && last == type) {
                const int opp = 1 - owner;
                ElfPet& foe = c->getPet(opp);
                if (drain_denom > 0 && foe.hp > 0) {
                    int drain = resolve_pet_max_hp(foe) / drain_denom;
                    if (drain > foe.hp) {
                        drain = foe.hp;             // 吸取不越过存量（K302/K338 口径）
                    }
                    if (drain > 0) {
                        (void)g_core->deal_pink_damage(c, opp, drain, DamageKind::FIXED,
                                                       owner);
                        (void)g_core->heal_amount(c, owner, drain);
                        write_int(c, k1856Drain + owner,
                                  read_int(c, k1856Drain + owner) + 1);
                    }
                }
                if (pp_up > 0) {
                    (void)g_core->pp_restore(c, owner, pp_up, /*only_empty=*/false);
                }
            }
            write_int(c, key, type);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（1772/1680/1856，只增不改）────
// 三行 tips 尾注全部全句尾（boss无效）→ 全节点 boss_invalid=true（K400 ⑨惯例）。
SOUL_PROGRAM(1772, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k403_1772_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k403_1772_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});

SOUL_PROGRAM(1680, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k403_1680_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k403_1680_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_END, &k403_1680_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});

SOUL_PROGRAM(1856, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k403_1856_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/true)});
