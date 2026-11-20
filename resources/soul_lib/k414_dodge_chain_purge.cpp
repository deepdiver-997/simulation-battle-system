// K414 闪避触发链/业火涤尘族 —— effect 791/821/1195（3 宏 / 6 行，行≠宏 双行档位差）
//（看板 K414「闪避涤尘族 2026-11-04」，a01-k414，worktree ../agents/k414；
//  号段 7740-7749；S5 调研二期 族 B/C 混合批次）
//
// 行清单（tools/soul_effects_dump.py 791 821 1195，2026-11-04，dump 全文为权威，
// 板行截断处以 dump 对参）：
//   791 row484 王·雷伊 3142 args[1,2,1] kind[11,12]：
//     「每回合开始和结束时吸取对手能力提升状态；回合开始时对手所有回合类效果的回合数
//      变为1回合；回合开始时若自身体力高于对手则战斗阶段结束时自身攻击+2，回合开始
//      时若自身体力低于对手，则当回合自身回合类效果无法被消除（BOSS无效）」
//   791 row527 王·雷伊 3142 args[1,2,1,1]（雷电传承魂印进化行——扩展档多三项：
//     「…自身攻击+2、速度+1、命中+1…」，其余同文）
//   821 row516 王·布莱克 3190 args[70,50,25,1] kind[1,10,17]：
//     「受到攻击时有70%的概率闪避对手攻击，触发闪避则下回合自身攻击有50%的概率使
//      对手害怕，同时无视对手双防强化状态；攻击附加自身攻击值25%的固定伤害（BOSS无效）」
//   821 row531 王·布莱克 3190 args[70,65,30,1]（暗影传承魂印进化行：害怕65%/固伤30%）
//   1195 row907 周瑜 3617 args[1,100,5,50,50,1] kind[3,8,10]：
//     「首次登场下1次技能100%令对手进入焚烬状态；登场后每5回合触发一次业火涤尘，
//      消除双方能力提升状态和回合类效果，消除任意一项成功则令对手焚烬；回合开始时
//      若自身体力高于对手则造成的攻击伤害提高50%，自身体力低于对手则受到的攻击伤害
//      减少50%（boss无效）」
//   1195 row938 周瑜 3617 args[1,100,5,80,60,1]（郎才女貌伙伴行：增伤80%/减伤60%）
//
// 行参解码（尾位旗按 K372 纪律不消费；db_at 原序直读、禁读 a.row——K293 记档）：
//   791: db[0]=主支旗（两行恒 1，行文恒有——无锚不消费，记档）；db[1]=攻击提升量 2
//        （文字锚「攻击+2」，两行同值）；db[2]=低体保护支旗（两行恒 1——该支内核缺口
//        记档 M2，不消费）；db[3]=进化行扩展档旗（仅 row527=1——速度+1/命中+1 的
//        行≠宏锚，row484 缺位=无扩展）。
//   821: db[0]=闪避率%（70，两行同值）；db[1]=下回合攻击害怕概率%（50/65）；
//        db[2]=攻击附加自身攻击值固伤比%（25/30）；db[3]=尾注旗不消费。
//   1195: db[0]=首次登场次数旗（两行恒 1——「首次」语义锚，实现按 K401 888 严格一次
//         门，数值不消费记档）；db[1]=首登焚烬申报概率%（恒 100）；db[2]=业火涤尘
//         周期（每 db[2] 回合，两行恒 5）；db[3]=高体增伤%（50/80）；db[4]=低体减伤%
//         （50/60）；db[5]=尾注旗不消费。
//
// 口径记档（majority reading 裁决，均按先例；口径歧义按派发指令 majority 实现并记档）：
//   M1【791② 不可达记档】"对手所有回合类效果的回合数变为1回合"：通道不可达——
//     ContinuousEffect::registered_round_/duration_rounds_ 为 protected 仅 const
//     getter（continuousEffect.h:95-99），唯一变原语 force_expire=「立即消除」非
//     「钳 1」语义；TimedBucket::at 是查询面（soul_lib 全库零直改先例，grep
//     registered_round_/getDurationRounds 无命中）；battle_primitives.h 无 set/clamp
//     回合数原语 → 本子句跳过实现，留待内核立项。
//   M2【791③低体支 不可达记档】"当回合自身回合类效果无法被消除"：内核缺口箱 P2
//     仍开放（调研二期 §六：break_round_effects 增 exempt 参数/旗标未立项；证据行
//     本单 791 row484/527）→ 跳过实现。
//   M3【boss 尾注最近邻归属】(BOSS无效) 只门它紧贴的；分段（majority：K401 888
//     "紧贴③句尾→仅③" + K408 1805"尾注紧贴尾支句尾"口径；派发指令"按 dump 行文
//     子句归属判"）——791 尾注在第三分段句尾 → 仅③支节点 boss_invalid=true（①吸取
//     支 boss 局照常，K408 1668"主支无尾注 boss 局照常"同形，M11）；821 尾注在第二
//     分段句尾 → 仅④固伤 DETECT 票回调手动门（watcher/票不随节点 boss_invalid 走，
//     hamo_3809_ao 口径）；1195 尾注在第三分段句尾 → 仅③增减伤 RS 节点 boss_invalid。
//     （K395/K396 "尾注全句→全节点"读法与本三形不符——多分段+尾注贴尾段，按更新的
//     最近邻口径落，分歧记档。）
//   M4【821 无视双防强化 可达】ws.boost_pierced（E11 旗标，battleWorkspace.h:241：
//     命中前桶写入、ATTACK_DAMAGE 消费、行动末内核清；语义=本次命中）——本实现走
//     FIRST/SECOND_BEFORE_SKILL_HIT 双桶 tick（k412 2514 pierce tick 直系）。
//   M5【821 闪避触发检测】"触发闪避"= EVENT_SKILL_INVALID（Skills::execute emit，
//     actor=攻击方）+ 攻击技过滤（k_spacetime_leap 1673 过滤形：roundChoice 取技能
//     type≠Attribute）；SKILL_INVALID 亦覆盖"技能无效"（被封/盔）非纯 miss（K256
//     记档同款）——本宏 70% dodge_rate 是场景纯链下唯一 miss 源，二者重合。
//   M6【821 无视强化窗界】"下回合"= 绝对窗 [触发回合+1, 触发回合+1]（888 补偿窗
//     戳记同形）；"同时无视"挂"下回合自身攻击"整句 → 窗内每次出手都 pierce（非仅
//     害怕落地的出手），害怕逐攻击掷（majority 直读，记档）；窗长 1 回合不过期擦除
//     问题（比较窗界即天然过期）。
//   M7【1195① 首登语义】"首次登场"= K401 888 严格一次（重登场不重授；enter 桶
//     epoch 守卫防开局双触发，k_guard/k401 记档）；"下1次技能"= pending 无过期
//     （K395 881 记档口径），own 任意技能即耗——行文无"攻击"限定，**不做**
//     used_attribute 过滤（888③ 攻击技门是其行文"自身攻击技能"字面，此处理由相反，
//     记档）；申报制：pending 无论落地与否即耗（apply_anomaly_checked 100% 短路掷点
//     仍吃免疫/抗性，K2 口径）。
//   M8【1195② 周期锚】"登场后每5回合"= 以最近一次登场回合为锚，RS 判
//     (roundCount-entry)%db[2]==0 且 roundCount>entry（登场回合本身不触发；重登场
//     重锚记档）；"消除任意一项成功"= clear_stat_boosts 返回>0 或 break_round_effects
//     返回 SUCCESS 任一即焚烬（双向四清：自身/对手 × 能力提升/回合类，K400 1151
//     消强直执行形；break 也清己方——行文"双方…回合类效果"字面）。
//   M9【攻击值】= numericalBase[PHYSICAL_ATTACK]（k243 "攻击值"=数值面板直系）。
//   M10【791③快照-消费】RS 采样体力对照（K377 分支门）→ 战斗阶段结束（=RE，
//     K302/K408 定例）按 stamp 消费（K405 戳>0 守卫+过期擦：缺失键 0 撞首回合）。
//   M11【791 吸取无 boss 门】(BOSS无效) 不在①分段 → boss 局照常吸取。
//
// 三查记录（a01-k414 2026-11-04，worktree 内复核）：
//   ①内核 grep -w 791/821/1195：仅 src/entities/skills.cpp:685/691 命中 821 = **技能
//     空间**免断模板路由（元神神之宣告 27021），非魂印硬编码——soul 空间三 id 零命中 ✓
//   ②effect_info 791/821/1195 均为技能空间异文模板（791 恢复体力回倍/821 回合类免断/
//     1195 受击异常未触发固伤），与魂印 tips 全异文——SOP §六.1：effect_icon tips
//     为权威 ✓
//   ③registry：soul_lib 全库无 SOUL_PROGRAM(791/821/1195)——真缺口 ✓；场景号段
//     7740-7749 全分支无撞；存储键段 99791xxxx/99821xxxx/991195xxx 全库无撞 ✓
// 注册：SOUL_PROGRAM ×3（791/821/1195 入账，只增不改）。
// 场景回归：test/scenario/scenario_7740_wleiyi_791.cpp / scenario_7743_wblaik_821.cpp /
//   scenario_7746_zhouyu_1195.cpp（号段 7740-7749）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

// ── 存储槽键段（+owner；tools/check_storage_keys.py 已核全库无撞）──────────────
constexpr int kBase791 = 997910000;
constexpr int kWire791 = kBase791 + 0;      // enter 已接线旗（防再登场双挂）
constexpr int kStealRS791 = kBase791 + 10;  // RS 吸取搬走项数累计（侧证）
constexpr int kStealRE791 = kBase791 + 20;  // RE 吸取搬走项数累计（侧证）
constexpr int kBranch791 = kBase791 + 30;   // RS 体力对照分支（1 高体 / 2 低体）
constexpr int kBranchRound791 = kBase791 + 40;  // 分支采样回合戳（**+1 偏移编码**
//   ——roundCount 从 0 起步，0 是合法回合号；stamp>0 守卫下直存 0 会被当"未武装"
//   踢掉，K394 ⑤"戳位+1 偏移防首回合 0 撞车"同款）

constexpr int kBase821 = 998210000;
constexpr int kWire821 = kBase821 + 0;      // enter 已接线旗（watcher+票一场一次）
constexpr int kWinFrom821 = kBase821 + 10;  // 害怕/穿防窗起（roundCount；0=未武装）
constexpr int kWinTo821 = kBase821 + 20;    // 害怕/穿防窗止（inclusive）
constexpr int kAuditFear821 = kBase821 + 30;   // 本次害怕申报概率（K662 审计槽）
constexpr int kFearLand821 = kBase821 + 40;    // 害怕落地计数（统计侧证）
constexpr int kDodgeCnt821 = kBase821 + 50;    // 闪避触发计数（侧证）
constexpr int kAttachCnt821 = kBase821 + 60;   // 固伤附加计数（侧证）

constexpr int kBase1195 = 991195000;
constexpr int kFirst1195 = kBase1195 + 0;      // 首次登场已触发旗（严格一次）
constexpr int kWire1195 = kBase1195 + 10;      // SKILL_USED watcher 已接线旗
constexpr int kEntryRound1195 = kBase1195 + 20;    // 最近登场回合锚（**+1 偏移编码**
//   ——0=未登场哨兵与合法的首回合登场(0)撞号，故存 roundCount+1；K394 ⑤同款）
constexpr int kPendingBurn1195 = kBase1195 + 30;   // 首登焚烬 pending 旗（1=在等）
constexpr int kAuditFirst1195 = kBase1195 + 40;    // 首登焚烬申报概率（恒 100）
constexpr int kAuditPurge1195 = kBase1195 + 50;    // 涤尘焚烬申报概率（恒 100）
constexpr int kPurgeCnt1195 = kBase1195 + 60;      // 业火涤尘触发计数（侧证）
constexpr int kPurgeBurnLand1195 = kBase1195 + 70; // 涤尘焚烬落地计数（侧证）

constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);  // 22
constexpr int kFearCode = static_cast<int>(AbnormalStatusId::Fear);          // 6
constexpr int kAtkSlot = 0;           // kAbilityLevelIndexAttack
constexpr int kSpeedSlot = 4;         // kAbilityLevelIndexSpeed
constexpr int kHitSlot = 5;           // kAbilityLevelIndexHit

// ── 宿主行参数（db_at 原序读 owned_int_args——K397/K408 同款私副本；禁读 a.row）──
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

void write_int(BattleContext* ctx, int key, int v) {
    ctx->plugin_storage[key] = v;
}

int read_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

// K238/K400 落地集（"未触发"只认非这些档）。
bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::CONVERTED;
}

// 体力对照分支（K377 1354 同款）：1=自身高于对手 / 2=低于 / 0=等号（无分支）。
int hp_branch(BattleContext* ctx, int owner) {
    const ElfPet& self = ctx->getPet(owner);
    const ElfPet& foe = ctx->getPet(1 - owner);
    if (self.hp > foe.hp) {
        return 1;
    }
    if (self.hp < foe.hp) {
        return 2;
    }
    return 0;
}

// 攻击值（M9：k243 直系——数值面板物攻）。
int atk_panel(const ElfPet& pet) {
    return pet.numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK];
}

// ═══════════════════════════════════════════════════════════════════════════
// effect 791 王·雷伊：①每回合开始和结束时吸取对手能力提升状态（boss 局照常，M11）；
//   ②回合数钳 1 不可达（M1 记档）；③RS 高体采样 → RE 攻击+2[扩展档速+1命中+1]
//   （boss_invalid，最近邻 M3）；③低体保护支缺口记档（M2）
// ═══════════════════════════════════════════════════════════════════════════

// enter：wired 防重（一场一次；①③ 都无 enter 级状态，节点直接干）。
EffectResult k791_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->transfer_stat_boosts || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(kBranch791 + owner);       // 跨局/换宠残留清账
    ctx->plugin_storage.erase(kBranchRound791 + owner);  //（中途上场当回合无采样，
    return EffectResult::kOk;                            //  k390 同款记档）
}

// ROUND_START·①：吸取对手提升（transfer：对手正等级整体搬到自身；免消除强化挡下
// 返回 0 且对手保有提升——K408 1668 ①同款原语读法）。
EffectResult k791_round_start_steal(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 791)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 对手已倒地不结算（K401 口径）
    }
    const int moved = g_core->transfer_stat_boosts(ctx, opp, owner);
    if (moved > 0) {
        write_int(ctx, kStealRS791 + owner, read_int(ctx, kStealRS791 + owner) + moved);
    }
    return EffectResult::kOk;
}

// ROUND_START·③：体力对照采样（高体/低体；等号=无分支不写戳——K397 hp_branch 门）。
EffectResult k791_round_start_branch(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, 791)) {
        return EffectResult::kOk;
    }
    const int branch = hp_branch(ctx, owner);
    write_int(ctx, kBranch791 + owner, branch);
    write_int(ctx, kBranchRound791 + owner, branch > 0 ? ctx->roundCount + 1 : 0);
    return EffectResult::kOk;
}

// ROUND_END·①：第二次吸取（"每回合开始和结束时"= RS+RE 双时点双桶；RE 独立侧证
// 计数——体例同 K408 1668 双账本）。
EffectResult k791_round_end_steal(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 791)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;
    }
    const int moved = g_core->transfer_stat_boosts(ctx, opp, owner);
    if (moved > 0) {
        write_int(ctx, kStealRE791 + owner, read_int(ctx, kStealRE791 + owner) + moved);
    }
    return EffectResult::kOk;
}

// ROUND_END·③：高体 → 攻击+db[1]（进化行 db[3]>0 → 另速度+1、命中+1）。按戳消费
//（M10：戳>0 且等于本回合才消费——K405 过期擦守卫）。stat_change 原语（自身增益、
// 不查免弱但查 STAT_BOOST 封锁——battle_primitives.h 契约）。
EffectResult k791_round_end_boost(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 791)) {
        return EffectResult::kOk;
    }
    const int stamp = read_int(ctx, kBranchRound791 + owner);
    if (stamp <= 0 || stamp != ctx->roundCount + 1) {
        return EffectResult::kOk;   // 本回合无有效采样（+1 偏移解码，M10/K394 防撞）
    }
    const int branch = read_int(ctx, kBranch791 + owner);
    if (branch != 1) {
        return EffectResult::kOk;   // 低体/等号：无 boost 支（低体支走 M2 记档缺口）
    }
    const HostRow row = host_row(ctx, a, 791);
    const int atk_up = db_at(row, 1, 2);   // db[1]，兜底 tips 字面 2
    if (atk_up > 0) {
        (void)g_core->stat_change(ctx, owner, kAtkSlot, atk_up);
    }
    if (db_at(row, 3, 0) > 0) {            // db[3]：进化行扩展档（行≠宏锚）
        (void)g_core->stat_change(ctx, owner, kSpeedSlot, 1);
        (void)g_core->stat_change(ctx, owner, kHitSlot, 1);
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// effect 821 王·布莱克：①70% 闪避（ws.dodge_rate K5/K397 通道）+ 触发侦测
//   （SKILL_INVALID+攻技过滤，M5）→ 下回合害怕窗（逐攻击掷，审计槽）+穿防
//   tick（boost_pierced，M4/M6）——①段无尾注 boss 局照常；④攻击附加攻击值固伤
//   （DETECT 票，boss 局回调手动门，M3）
// ═══════════════════════════════════════════════════════════════════════════

// enter：接线三件套（一场一次，TEAM watcher 防堆叠；K386 记档）：
//   a) EVENT_SKILL_INVALID：对手攻击技未命中（=本宏闪避触发，M5）→ 武装下回合窗；
//   b) EVENT_SKILL_USED：own 攻技 + 窗内 → 害怕申报（掷前写审计槽，K662）；
//   c) DETECT 票：own 红伤命中 → 附加 攻击值×db[2]% 固伤（boss 局手动门，M3）。
EffectResult k821_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int wired = kWire821 + owner;
    if (ctx->plugin_storage.count(wired)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(wired, 1);
    const HostRow row = host_row(ctx, a, 821);
    const int fear_pct = db_at(row, 1, 50);   // db[1]，兜底本体字面 50
    const int fixed_pct = db_at(row, 2, 25);  // db[2]，兜底本体字面 25

    // a) 闪避触发侦测 → 武装下回合窗（M5/M6）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != 1 - owner) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, 821)) {
                return;
            }
            // 攻击技过滤（1673 形）：roundChoice 取对手本回合技能，属性技不算
            //（"受到攻击"字面——属性技无伤害阶段、不构成"攻击"，速查 §四点五）。
            const int idx = c->roundChoice[1 - owner][1];
            const auto& skills = c->getPet(1 - owner).skills;
            if (idx < 0 || idx >= static_cast<int>(skills.size())
                || skills[static_cast<std::size_t>(idx)].type == SkillType::Attribute) {
                return;
            }
            const int win = c->roundCount + 1;
            write_int(c, kWinFrom821 + owner, win);
            write_int(c, kWinTo821 + owner, win);
            write_int(c, kDodgeCnt821 + owner, read_int(c, kDodgeCnt821 + owner) + 1);
        },
        WatcherScope::TEAM);

    // b) 窗内害怕（逐攻击掷；miss 也算使用——K396 ③/K397 1149 ③ 定例）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, fear_pct](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // own + 攻击技（"自身攻击"字面，K1149 ③ 同门）
            }
            if (!onstage_has_soul_mark(c, owner, 821)) {
                return;
            }
            const int from = read_int(c, kWinFrom821 + owner);
            const int to = read_int(c, kWinTo821 + owner);
            if (from <= 0 || to < from || c->roundCount < from || c->roundCount > to) {
                return;   // 窗外（窗长 1 回合，比较即过期——M6）
            }
            const int opp = 1 - owner;
            if (c->getPet(opp).hp <= 0) {
                return;   // 对手已倒地不施加（K401 口径）
            }
            c->plugin_storage[kAuditFear821 + owner] = fear_pct;   // 掷前写申报值
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, opp, kFearCode, random_anomaly_duration(), owner, fear_pct,
                ChanceSource::SoulMark);
            if (anomaly_landed(r)) {
                write_int(c, kFearLand821 + owner,
                          read_int(c, kFearLand821 + owner) + 1);
            }
        },
        WatcherScope::TEAM);

    // c) ④固伤附加（DETECT 票，K390 1523 接线形；boss 局手动门，M3）。
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, fixed_pct](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            if (c->is_boss_challenge) {
                return;   // ④（BOSS无效）：boss 局整支不执行（watcher/票不随节点走）
            }
            if (!onstage_has_soul_mark(c, owner, 821)) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (!d.isRed || d.final <= 0 || d.attackerId != owner) {
                return;   // own 红伤命中才附加（"攻击附加"字面；miss 不跑管线天然无）
            }
            const ElfPet& self = c->getPet(owner);
            const int attach = atk_panel(self) * fixed_pct / 100;   // M9
            if (attach <= 0) {
                return;   // 取整归零无从附加（K390 口径）
            }
            (void)g_core->deal_pink_damage(c, 1 - owner, attach, DamageKind::FIXED,
                                           owner);
            write_int(c, kAttachCnt821 + owner, read_int(c, kAttachCnt821 + owner) + 1);
        });
    return EffectResult::kOk;
}

// ROUND_START：70% 闪避率每回合重写（ws.reset 每回合清 → 窗式条款每回合无脑重写，
// 2492 形；"受到攻击时"无回合限定 → 在场即常驻）。
EffectResult k821_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, 821)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 821);
    const int dodge_pct = db_at(row, 0, 70);   // db[0]，兜底 tips 字面 70
    ctx->ws.dodge_rate[a.owner] = dodge_pct / 100.0f;   // K5/K397 通道
    return EffectResult::kOk;
}

// 命中前双桶 tick：窗内出手 → 穿防旗（M4/M6；2514 pierce tick 直系——先于同行动
// ATTACK_DAMAGE 消费，行动末随内核清；"本次命中"语义）。
EffectResult k821_pierce_tick(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, 821)) {
        return EffectResult::kOk;
    }
    const int from = read_int(ctx, kWinFrom821 + owner);
    const int to = read_int(ctx, kWinTo821 + owner);
    if (from <= 0 || to < from || ctx->roundCount < from || ctx->roundCount > to) {
        return EffectResult::kOk;
    }
    ctx->ws.boost_pierced[1 - owner] = true;
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// effect 1195 周瑜：①首登焚烬 pending（100% 申报，boss 局照常——M3/M7）；
//   ②业火涤尘每 db[2] 回合：双向四清，任一成功 → 对手焚烬（boss 局照常）；
//   ③RS 体力对照增伤/减伤（K397 1139 直系镜像，boss_invalid——最近邻 M3）
// ═══════════════════════════════════════════════════════════════════════════

// enter（epoch 守卫防开局双触发，k401 888 直系）：
//   首次登场 → 首登旗 + 武装焚烬 pending（严格一次，重登场不重授）；
//   每次登场 → 重锚涤尘周期（M8 记档）+ 接线 SKILL_USED watcher（一场一次）。
EffectResult k1195_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    // epoch 守卫：开局 ENTER_EXIT 桶双触发（k401/k_guard 记档）——同 epoch 只处理一次。
    const int epoch_key = kBase1195 + 90;
    const auto git = ctx->plugin_storage.find(epoch_key);
    if (git != ctx->plugin_storage.end()
        && any_int_or(git->second, -1) == ctx->round_effect_valid_id[owner]) {
        return EffectResult::kOk;
    }
    write_int(ctx, epoch_key, ctx->round_effect_valid_id[owner]);

    const HostRow row = host_row(ctx, a, 1195);
    // ① 首次登场：武装焚烬 pending（严格一次）。
    if (!ctx->plugin_storage.count(kFirst1195 + owner)) {
        ctx->plugin_storage.emplace(kFirst1195 + owner, 1);
        if (db_at(row, 1, 100) > 0) {
            write_int(ctx, kPendingBurn1195 + owner, 1);
        }
    }
    // ② 周期锚：每次登场重锚（M8；+1 偏移——0 是合法登场回合）。
    write_int(ctx, kEntryRound1195 + owner, ctx->roundCount + 1);
    // watcher 接线（一场一次；①支无尾注 → 回调无 boss 门）。
    if (!ctx->plugin_storage.count(kWire1195 + owner)) {
        ctx->plugin_storage.emplace(kWire1195 + owner, 1);
        const int burn_pct = db_at(row, 1, 100);   // db[1]，兜底 tips 字面 100
        ctx->event_center_.register_watcher(
            EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, burn_pct](BattleContext* c, const BattleEvent& ev) {
                if (!c || !g_core || !g_core->apply_anomaly_checked) {
                    return;
                }
                if (ev.actor != owner) {
                    return;   // "下1次技能"无攻击限定 → 不滤属性技（M7 记档）
                }
                if (!onstage_has_soul_mark(c, owner, 1195)) {
                    return;
                }
                const auto pit = c->plugin_storage.find(kPendingBurn1195 + owner);
                if (pit == c->plugin_storage.end()
                    || any_int_or(pit->second, 0) != 1) {
                    return;   // pending 未武装/已耗
                }
                c->plugin_storage.erase(pit);   // 申报制：无论落地与否即耗（M7）
                const int opp = 1 - owner;
                if (c->getPet(opp).hp <= 0) {
                    return;   // 对手已倒地不施加（K401 口径）
                }
                c->plugin_storage[kAuditFirst1195 + owner] = burn_pct;  // 掷前申报
                (void)g_core->apply_anomaly_checked(c, opp, kIncinerate,
                                                    random_anomaly_duration(), owner,
                                                    burn_pct, ChanceSource::SoulMark);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ROUND_START·②：业火涤尘（每 db[2] 回合；双向四清，任一成功 → 焚烬申报）。
EffectResult k1195_purge(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->clear_stat_boosts || !g_core->break_round_effects
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1195)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 1195);
    const int period = db_at(row, 2, 5);   // db[2]，兜底 tips 字面 5
    if (period <= 0) {
        return EffectResult::kOk;
    }
    const int entry_stored = read_int(ctx, kEntryRound1195 + owner);
    const int entry = entry_stored - 1;   // +1 偏移解码（0=未登场哨兵）
    if (entry_stored <= 0 || ctx->roundCount <= entry
        || (ctx->roundCount - entry) % period != 0) {
        return EffectResult::kOk;   // 未登场/登场当回合/未到周期（M8）
    }
    const int opp = 1 - owner;
    // 双向四清（消除不掷骰，K400 1151 直执行形）：任一成功即焚烬（M8 判据）。
    bool any_success = false;
    const int own_cleared = g_core->clear_stat_boosts(ctx, owner);
    const int opp_cleared = g_core->clear_stat_boosts(ctx, opp);
    if (own_cleared > 0 || opp_cleared > 0) {
        any_success = true;
    }
    const BreakResult own_break = g_core->break_round_effects(ctx, owner);
    const BreakResult opp_break = g_core->break_round_effects(ctx, opp);
    if (own_break == BreakResult::SUCCESS || opp_break == BreakResult::SUCCESS) {
        any_success = true;
    }
    write_int(ctx, kPurgeCnt1195 + owner, read_int(ctx, kPurgeCnt1195 + owner) + 1);
    if (!any_success) {
        return EffectResult::kOk;   // 消除任意一项成功才焚烬（行文字面）
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 对手已倒地不施加
    }
    ctx->plugin_storage[kAuditPurge1195 + owner] = 100;   // 掷前申报（恒 100）
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, kIncinerate, random_anomaly_duration(), owner, 100,
        ChanceSource::SoulMark);
    if (anomaly_landed(r)) {
        write_int(ctx, kPurgeBurnLand1195 + owner,
                  read_int(ctx, kPurgeBurnLand1195 + owner) + 1);
    }
    return EffectResult::kOk;
}

// ROUND_START·③：体力对照增伤/减伤（K397 1139 直系镜像；boss_invalid 节点，M3）。
EffectResult k1195_round_start_amp(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, 1195)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 1195);
    const int amp_pct = db_at(row, 3, 50);     // db[3]，兜底本体字面 50
    const int reduce_pct = db_at(row, 4, 50);  // db[4]，兜底本体字面 50
    const int branch = hp_branch(ctx, a.owner);
    if (branch == 1 && amp_pct > 0) {
        ctx->ws.damage_add_pct[a.owner] += static_cast<float>(amp_pct);
    } else if (branch == 2 && reduce_pct > 0) {
        ctx->ws.damage_reduce_add[a.owner][0] += reduce_pct;   // hundun 2411 槽 0 直系
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（791/821/1195，只增不改）────
// boss 尾注最近邻归属（M3）：791 仅③两节点、821 仅④票回调手动门（enter/RS/tick
// 无门——①段无尾注）、1195 仅③ RS 节点（①②无尾注照常）。
SOUL_PROGRAM(791, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k791_enter,
                              /*once=*/false, /*early=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k791_round_start_steal,
                              /*once=*/false, /*early=*/false),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k791_round_start_branch,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_END, &k791_round_end_steal,
                              /*once=*/false, /*early=*/false),
                    SOUL_NODE(State::BATTLE_ROUND_END, &k791_round_end_boost,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});

SOUL_PROGRAM(821, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k821_enter,
                              /*once=*/false, /*early=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k821_round_start,
                              /*once=*/false, /*early=*/false),
                    SOUL_NODE(State::BATTLE_FIRST_BEFORE_SKILL_HIT, &k821_pierce_tick,
                              /*once=*/false, /*early=*/false),
                    SOUL_NODE(State::BATTLE_SECOND_BEFORE_SKILL_HIT, &k821_pierce_tick,
                              /*once=*/false, /*early=*/false)});

SOUL_PROGRAM(1195, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k1195_enter,
                               /*once=*/false, /*early=*/true),
                     SOUL_NODE(State::BATTLE_ROUND_START, &k1195_purge,
                               /*once=*/false, /*early=*/false),
                     SOUL_NODE(State::BATTLE_ROUND_START, &k1195_round_start_amp,
                               /*once=*/false, /*early=*/false, SoulScope::STAGE,
                               /*boss_invalid=*/true)});
