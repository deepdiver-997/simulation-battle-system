// K402 先制累计/未击败响应族 —— effect 1475/1561/1630（3 宏 / 6 行，双行档位差）
//（看板 K402「先制累计/未击败响应族 2026-11-02 编排挖掘」，a01-knight6，
//  worktree ../agents/k402；号段 7580-7589；通道=K394 ③/K395 881 FMR 先制窗 +
//  K394 ④ HEAL_BLOCK 票 + K4/K196 stat_change + K377 高体门 + K188 死亡账本 +
//  灵巢 4599 击败转移/下只登场消费 + K395 记档「永久累计=离场保留」口径）
//
// 行清单（tools/soul_effects_dump.py 1475 1561 1630，2026-11-02）：
//   1475 凌波清芙 3888 两行（row1157 本体 / row1561 伙伴「并蒂双莲」，kind 同 [1,4,10]，
//   **args 档位差**常规行≠宏——两行 8 参同构，逐位与 tips 数值子句 1:1 对齐）：
//     row1157 args[1,50,3,50,3,400,400,1]：「当回合攻击未击败对手则下回合自身所有技能
//       先制+1且攻击造成的伤害提升50%，当回合攻击击败对手则{吸取}对手下只登场精灵
//       最大{体力}的1/3且50%令对手睡眠；|死亡后恢复己方所有不在场且存活精灵最大体力
//       的1/3，同时使己方下只精灵登场后获得神圣之荷的庇护：400点护盾和400点护罩
//       （boss无效）」
//     row1561 伙伴行 args[1,100,3,100,3,400,400,1]：50/100 两档（先制+1 两行同量）。
//     → db[0]=先制量（1，"先制+1"互锚）、db[1]=未击败伤害提升%（50/100）、
//       db[2]=吸取分母（3，"1/3"互锚）、db[3]=击败睡眠%（50/100）、db[4]=死亡恢复
//       分母（3）、db[5]=神圣之荷护盾（400）、db[6]=护罩（400）、db[7]=1 旗不消费
//       （boss 尾注按全句尾注读，见记档⑤）。
//   1561 极影魔兽 3961 两行（row1226 本体 / row1271 伙伴「魔界凶兽」，kind 同 [1,9]，
//   args 档位差仅 db[5]）：
//     row1226 args[300,1,3,300,30,90,1]：「自身先出手时令对手当回合无法通过自身技能
//       恢复体力；每次受到的攻击伤害高于300则自身从下回合起攻击技能先制+1，最高累计
//       为先制+3；每次受到的攻击伤害低于300则自身从下回合起造成的攻击伤害提升30%，
//       最高累计提升90%（boss无效，伤害效果持续到被击败或切换下场）」
//     row1271 伙伴行 args[300,1,3,300,30,120,1]：增伤上限 90/120。
//     → db[0]=高伤阈值（300）、db[1]=先制每层量（1）、db[2]=先制累计上限（3）、
//       db[3]=低伤阈值（300）、db[4]=增伤每层%（30）、db[5]=增伤上限%（90/120）、
//       db[6]=1 旗不消费。
//   1630 天玄界神 4037 两行（row1292 本体 / row1541 伙伴「六界神谕」，kind 同 [1,9,11]，
//   args 档位差仅 db[4]）：
//     row1292 args[1,1,3,40,10,100,1]：「回合开始时，若自身体力高于对手则令自身全属性+1，
//       每次触发该效果后则当回合自身攻击先制+1，最多+3（先制效果切换下场后重新叠加）；
//       本场战斗中自身攻击先出手时掌控啸风：40%的概率造成的攻击伤害翻倍，每次触发则
//       概率增加10%，最高概率100%（boss无效）」
//     row1541 伙伴行 args[1,1,3,40,20,100,1]：爬坡步 10/20。
//     → db[0]=全属性增量（1）、db[1]=先制每层量（1）、db[2]=先制上限（3）、
//       db[3]=啸风基础概率%（40）、db[4]=爬坡步%（10/20）、db[5]=概率上限%（100）、
//       db[6]=1 旗不消费。
//
// ⚠️ 行判别（K400 1237「args 档位差」同判）：三宏两行均同宠同 kind 同参数个数，
//   唯一运行期差异=行参数值（50/100、90/120、10/20）——纯 args 驱动，无需行别通道，
//   实现零硬编码行值（含先制量 db[0]/db[1]=1 也走 args）。
//
// ⚠️ 口径记档（majority reading，均按先例）：
//   ①【1475 击败支两子句同靶】"吸取对手下只登场精灵最大体力的1/3 且 50%令对手睡眠"
//     ——睡眠"对手"按击败时点无活体可睡，两子句同指**对手下只登场精灵**（登场即
//     吸取+掷睡，反下场惩罚整句；灵巢 4599 击败转移同族句式）。
//   ②【1475 未击败支门】"当回合攻击未击败"= executed_skill_this_round +
//     executed_skill_is_attack（速查 §四点五判据；miss 也算使用）且对手在场活体
//     （lingchao 4599 not_defeat 同门：对手已死但非我所杀 → 两支都不结）。
//   ③【1475 吸取/神圣之荷跨宠账】= side 级 plugin_storage 待继承账（k188 983 同构：
//     宿主死亡/击败后接班精灵必然是"下一只"），消费=自删式 EVENT_ENTER_STAGE watcher
//     （once=true 广播投递会被对侧登场事件烧毁——event_center.h deliver 无 owner 过滤、
//     once 无条件移除，咤克斯 2386 自删先例；重复击败覆盖=接最新一笔，lingchao 同口径）。
//     消费回调不做 onstage 守卫（账是整队的，宿主可先死；lingchao 转移回调同形）。
//   ④【1561 永久累计 vs 切换重置的不对称】先制累计=「永久累计」plugin_storage 账本
//     离场保留（板行处方 K395 记档口径，行文无清除子句）；增伤累计按行文尾注"伤害效果
//     持续到被击败或切换下场" → enter 节点清账（再入场重累计；复活不换场的极端面记档
//     不另设清除点）。两账均"从下回合起"由时点天然成立：受击在攻击相（晚于当回合 FMR）
//     → 次回合 FMR 才首次消费。
//   ⑤【boss 尾注】三行 (boss无效) 均为 tips 末位全句尾注 → 全节点 boss_invalid=true
//     （K400 ⑨/K399 boss 口径；K349/K394"最近邻子句"反例知悉不采——本单三行尾注都
//     收束全串，与 K400/K399 行形一致）。1561 尾注括号内"伤害效果持续到…"子句按 ④
//     只约束增伤账（先制账行文无清除子句）。
//   ⑥【1561 封回血"先出手"判据】= 条件票回调现场读 ws.preemptive_right
//     （!is_second_mover(owner)，K395 881 [M3]/K12 310 同判据；heal 时点=行动相，
//     先手权已结算）。"技能恢复"vs"一切恢复"插件面无区分通道 → 按 heal 原语口径
//     整面封锁（K394 ④ 记档延续）；ROUND_START 段回血早于 FMR 会读到上回合 stale
//     先手权——引擎时序边，技能回血全部发生在行动相不受影响（记档）。
//   ⑦【1561 受击阈值门】"每次受到的攻击伤害">/< 300 = EVENT_TAKE_DAMAGE 两攻击伤害
//     时点过滤（K400 ④/K241 同门，ev.amount 实扣值；恰 300 双分支都不结——行文严格
//     不等号；攻击中附带粉同态计入，同门记档）。
//   ⑧【1630"全属性+1"槽集】= stat_change 六槽循环含命中（k242/k893 → K399
//     drop_all_stats 口径，板行"六槽循环"同判）。
//   ⑨【1630 先制累计读法】"每次触发该效果后则当回合自身攻击先制+1，最多+3（切换
//     下场后重新叠加）"= 触发轮施加量=累计触发层数（min(层数,db[2])，1/2/3/3…），
//     仅触发轮施加（"当回合"字面），层账 enter 清（"重新叠加"）——板行"每次触发累计
//     max+3（切换下场重置=STAGE scope 账本）"同读；先制通道=FMR 桶 memset 后写
//     ws.preemptive_level（K394 ③/K395 881 同构；行文"攻击先制"按板行处方落 side 级
//     pl 槽，无按技型先制通道，记档）。
//   ⑩【1630 啸风"每次触发"】= 掷点**成功**（伤害翻倍落地）才爬坡（K400 1237 疲惫爬坡
//     触发语义同源：触发=效果发生）；概率账"本场战斗中"= 战斗级 plugin_storage，
//     不随切换清（行文与 ⑨ 的切换重置明示只属先制效果）。翻倍=×2 固定（行文无幅度参），
//     消费=DETECT 票 ×flag/100（K400 1151 ⑥ 同位），SKILL_USED 入口清残旗+ROUND_END
//     兜底清（miss 残留防泄漏，同 ⑥）。
//   ⑪ 概率节点全部 K662 式审计槽（掷前写申报概率）+落地计数（统计带侧证）；
//     1475 睡眠走申报通道（免睡/抗性按内核语义），1475 死亡恢复场下直写（场下无恢复
//     原语，tianqi_dijun 3550 口径；跳过倒地/消逝）。
//
// ⚠️ 三查记录（a01-knight6 2026-11-02，worktree 内复核）：
//   ①内核 grep -w 1475/1561/1630 零命中（无魂印行为硬编码）；
//   ②effect_icon kind 已核（dump kind[1,4,10]/[1,9]/[1,9,11]）；effect_info 三 id 均
//     为技能空间异文模板（魂印语义权威=effect_icon tips，SOP §六.1）；
//   ③registry soul=369 无三 id；soul_lib 全库无 SOUL_PROGRAM(1475/1561/1630)、无未合并
//     分支实现（K400/K399 为并行族批、不涉三 id）——真缺口。
// 存储键段：1004750000+owner*16+槽（1475，槽 0..10）/ 1005610000…（1561，槽 0..3）/
//   1006300000…（1630，槽 0..6），grep 全库无撞；plugin_storage 键 int 空间。
// 注册：SOUL_PROGRAM ×3（1475/1561/1630 入账，只增不改），soul 369→372。
// 场景回归：test/scenario/scenario_7580_undefeat_1475.cpp /
//   scenario_7583_healblock_stack_1561.cpp / scenario_7586_rs_fullstat_1630.cpp
//   （号段 7580-7589）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark1475 = 1475;
constexpr int kMark1561 = 1561;
constexpr int kMark1630 = 1630;

constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);  // 8 睡眠
constexpr int kCombatStatCount = 6;  // 六槽含命中（k242/k893 → K399 口径）
constexpr int kOwnerStride = 16;     // 每 owner 键段步长（槽 0..15）

// ── 存储键段（base + owner*16 + 槽；全库 grep 无撞）─────────────────────────
constexpr int k1475Base = 1004750000;
constexpr int k1475RoundAStamp = 0;   // 未击败支 pending 戳（=授予回合+1；0=无）
constexpr int k1475DrainDenom = 1;    // 击败支待继承：吸取分母
constexpr int k1475SleepPct = 2;      // 击败支待继承：睡眠申报概率
constexpr int k1475DrainWatchArmed = 3;   // 对手登场消费 watcher 已挂旗
constexpr int k1475DrainWatchId = 4;      // watcher id（自删用）
constexpr int k1475BeqShield = 5;     // 死亡支待继承：护盾值
constexpr int k1475BeqCover = 6;      // 死亡支待继承：护罩值
constexpr int k1475BeqWatchArmed = 7;     // 己方登场消费 watcher 已挂旗
constexpr int k1475BeqWatchId = 8;        // watcher id（自删用）
constexpr int k1475SleepAudit = 9;    // 睡眠申报概率（审计槽）
constexpr int k1475DeathEpoch = 10;   // 死亡 epoch（同尸防重，k188 口径）
constexpr int k1475SleepLand = 11;    // 睡眠落地计数（统计带侧证）

constexpr int k1561Base = 1005610000;
constexpr int k1561PlLedger = 0;      // 先制累计层（永久，离场保留）
constexpr int k1561AmpLedger = 1;     // 增伤累计%（切换下场/被击败重置）
constexpr int k1561Wired = 2;         // TAKE_DAMAGE watcher 已挂旗
constexpr int k1561HealTicketSid = 3; // 封回血票句柄（重授刷新）

constexpr int k1630Base = 1006300000;
constexpr int k1630PlLedger = 0;      // 先制累计层（STAGE 账，enter 清）
constexpr int k1630TriggerRound = 1;  // 本回合触发戳（=roundCount+1，0=未触发——防首回合 0 撞车，K394 T2 口径）
constexpr int k1630WindPct = 2;       // 啸风现行概率%（战斗级账，0=未记）
constexpr int k1630DblFlag = 3;       // 本攻击翻倍旗（200=×2，0=无）
constexpr int k1630Wired = 4;         // watcher/DETECT 票已挂旗
constexpr int k1630WindAudit = 5;     // 啸风申报概率（审计槽）
constexpr int k1630WindLand = 6;      // 翻倍落地计数（统计带侧证）

// ── 共用小件（HostRow/db_at 原序读 owned_int_args，K400/K399 定例）──────────

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

int read_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

void write_int(BattleContext* ctx, int key, int v) {
    ctx->plugin_storage[key] = v;
}

void erase_int(BattleContext* ctx, int key) {
    ctx->plugin_storage.erase(key);
}

// const 版在场守卫（封回血票条件回调语境，rule_center 传 const BattleContext*）。
bool host_onstage_const(const BattleContext* c, int owner, int mark_id) {
    if (!c || owner < 0 || owner > 1 || c->on_stage[owner] < 0 || c->on_stage[owner] > 5) {
        return false;
    }
    return c->seerRobot[owner].elfPets[c->on_stage[owner]].soulMark.id == mark_id;
}

// K238 落地集（异常申报"已落地"档；免睡/抗性等非掷败档不算未触发）。
bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::CONVERTED;
}

// 执行判据私副本（速查 §四点五；soul_lib 各文件自治同款）。
bool executed_skill_this_round(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->skill_use_seq[owner] > ctx->ws.skill_use_seq_round_start[owner];
}

bool executed_skill_is_attack(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->ws.skill_type_view[owner] != static_cast<int>(SkillType::Attribute);
}

// 攻击伤害事件时点（K400/K395 同款私副本）。
bool attack_damage_event(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

// 自删式一次性登场 watcher 装配（event_center deliver 广播投递、once=true 会被
// 对侧事件烧毁 → 回调内过门后按 id 自删，咤克斯 2386 先例）。
// armed_flag/watch_id 为 storage 槽键（已含 owner 偏移）；on_enter 返回 false=无待
// 继承账（不算触发，watcher 留任）。消费回调不做 onstage 守卫（跨宠待继承账，记档③）。
template <typename Fn>
void arm_enter_watcher(BattleContext* ctx, int armed_flag, int watch_id_key, Fn&& on_enter) {
    if (read_int(ctx, armed_flag) == 1) {
        return;   // 已挂（待继承账未被消费前不重挂）
    }
    const int wid = ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, /*owner=*/0, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [armed_flag, watch_id_key,
         fn = std::forward<Fn>(on_enter)](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor < 0 || ev.actor > 1) {
                return;
            }
            if (!fn(c, ev.actor)) {
                return;   // 该侧无待继承账 → 不算触发
            }
            // 消费完成：自删 + 清挂旗（下次击败/死亡重新装配）。
            c->event_center_.remove_watcher(read_int(c, watch_id_key));
            erase_int(c, armed_flag);
            erase_int(c, watch_id_key);
        },
        WatcherScope::TEAM);
    write_int(ctx, armed_flag, 1);
    write_int(ctx, watch_id_key, wid);
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1475 凌波清芙：未击败→下回合先制+1/增伤 P%；击败→吸取对手下只登场精灵
//   1/3 maxHP + P% 睡眠；死亡→场下回复 1/3 + 己方下只登场获 400 盾/400 罩
// ═════════════════════════════════════════════════════════════════════════════

// enter：跨局/换宠残留清账（只清未击败支回合戳——吸取/神圣之荷是跨宠待继承账，
// 接班宠 enter 不得清，否则顺序早于 ENTER_STAGE 事件会吞账，记档③）。
EffectResult k402_1475_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    erase_int(ctx, k1475Base + a.owner * kOwnerStride + k1475RoundAStamp);
    return EffectResult::kOk;
}

// DEFEAT_STATUS（未击败/击败时点，lingchao 4599 同桶）：两支互斥。
EffectResult k402_1475_defeat_status(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, kMark1475)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int foe = 1 - owner;
    const int foe_slot = ctx->on_stage[foe];
    if (foe_slot < 0 || foe_slot > 5) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1475);
    if (find_defeated_foe_slot(ctx, owner) >= 0) {
        // ── 击败支：待继承账（重复击败覆盖=接最新一笔，lingchao 同口径，记档①）──
        const int denom = db_at(row, 2, 3);      // db[2]，兜底字面 1/3
        const int sleep_pct = db_at(row, 3, 50); // db[3]，兜底本体字面 50
        const int base = k1475Base + owner * kOwnerStride;
        write_int(ctx, base + k1475DrainDenom, denom);
        write_int(ctx, base + k1475SleepPct, sleep_pct);
        arm_enter_watcher(ctx, base + k1475DrainWatchArmed, base + k1475DrainWatchId,
                          /*on_enter=*/[base, owner, foe](BattleContext* c,
                                                          int entering_side) -> bool {
                              if (entering_side != foe) {
                                  return false;   // 只接对手侧登场（"对手下只登场精灵"）
                              }
                              const int denom2 = read_int(c, base + k1475DrainDenom);
                              const int sleep_pct2 = read_int(c, base + k1475SleepPct);
                              if (denom2 <= 0 && sleep_pct2 <= 0) {
                                  return false;
                              }
                              // 清账先行（重复进入不再结）。
                              erase_int(c, base + k1475DrainDenom);
                              erase_int(c, base + k1475SleepPct);
                              // 吸取：对手登场精灵掉 maxHP/分母（FIXED）+ 自身等量回复
                              //（drain_hp 原语；"下只登场精灵"=登场地槽位现值）。
                              if (denom2 > 0 && g_core && g_core->drain_hp) {
                                  (void)g_core->drain_hp(c, owner, foe, denom2);
                              }
                              // 睡眠：申报通道（不预掷，K193/K400 同款），掷前写审计槽。
                              if (sleep_pct2 > 0 && g_core && g_core->apply_anomaly_checked) {
                                  write_int(c, base + k1475SleepAudit, sleep_pct2);
                                  const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                                      c, foe, kSleep, random_anomaly_duration(), owner,
                                      sleep_pct2, ChanceSource::SoulMark);
                                  if (anomaly_landed(r)) {
                                      write_int(c, base + k1475SleepLand,
                                                read_int(c, base + k1475SleepLand)
                                                    + 1);   // 落地计数（统计带侧证）
                                  }
                              }
                              return true;
                          });
        return EffectResult::kOk;
    }
    // ── 未击败支：本回合攻击过且对手仍是活体（记档②）→ 下回合先制+1/增伤 P% ──
    if (ctx->seerRobot[foe].elfPets[foe_slot].hp <= 0) {
        return EffectResult::kOk;   // 对手已死但非我所杀 → 两支都不结（lingchao 同门）
    }
    if (!executed_skill_this_round(ctx, owner) || !executed_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;   // 未攻击/属性技 → 不结
    }
    const int pl = db_at(row, 0, 1);         // db[0]，兜底字面 +1
    const int amp = db_at(row, 1, 50);       // db[1]，兜底本体字面 50
    if (pl <= 0 && amp <= 0) {
        return EffectResult::kOk;
    }
    write_int(ctx, k1475Base + owner * kOwnerStride + k1475RoundAStamp,
              ctx->roundCount + 1);
    return EffectResult::kOk;
}

// FMR：消费未击败支 pending（memset 后写，K394 ③/K395 881 同构；戳=授予回合+1，
// 只消费"下回合"，宿主缺席则戳过期哑火——K394 ③ 记档口径）。
EffectResult k402_1475_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, kMark1475)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int key = k1475Base + owner * kOwnerStride + k1475RoundAStamp;
    // 消费判据：戳=授予回合+1，须 >0 且 ==当回合（缺失键读 0，首回合 rc=0 时
    // 无守卫会误消费——K394 T2"槽默认 0 撞车"同款坑，实测 d1=136 证实）。
    const int stamp = read_int(ctx, key);
    if (stamp <= 0 || stamp != ctx->roundCount) {
        return EffectResult::kOk;
    }
    erase_int(ctx, key);
    const HostRow row = host_row(ctx, a, kMark1475);
    const int pl = db_at(row, 0, 1);     // db[0]，兜底字面 +1
    const int amp = db_at(row, 1, 50);   // db[1]，兜底本体字面 50
    if (pl > 0) {
        ctx->ws.preemptive_level[owner] += pl;   // "所有技能"——无攻击技门
    }
    if (amp > 0) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(amp);   // ws 钦定通道（K395 M6）
    }
    return EffectResult::kOk;
}

// 死亡双桶（k188 983 同构，epoch 防同尸重触发）：场下回复 1/3 + 神圣之荷待继承账。
EffectResult k402_1475_death(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, kMark1475)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int slot = ctx->on_stage[owner];
    if (slot < 0 || slot > 5 || ctx->seerRobot[owner].elfPets[slot].hp > 0) {
        return EffectResult::kOk;   // 宿主未倒（死亡时点桶）
    }
    // 同尸门（k188：MOVER_DEATH 态每回合都走，死亡时 epoch 作防重）。
    const int epoch = ctx->pipeline_valid_id[owner];
    const int epoch_key = k1475Base + owner * kOwnerStride + k1475DeathEpoch;
    if (read_int(ctx, epoch_key) == epoch) {
        return EffectResult::kOk;
    }
    write_int(ctx, epoch_key, epoch);

    const HostRow row = host_row(ctx, a, kMark1475);
    // ① 场下回复：己方所有不在场且存活精灵 maxHP/分母（场下无恢复原语 → 直写钳上限，
    //    tianqi_dijun 3550 口径；跳过消逝）。
    const int heal_denom = db_at(row, 4, 3);   // db[4]，兜底字面 1/3
    if (heal_denom > 0) {
        for (int i = 0; i < 6; ++i) {
            if (i == ctx->on_stage[owner]) {
                continue;
            }
            ElfPet& mate = ctx->seerRobot[owner].elfPets[i];
            if (mate.hp <= 0 || ctx->is_vanished(owner, i)) {
                continue;
            }
            const int cap = resolve_pet_max_hp(mate);
            const int amount = cap / heal_denom;
            if (amount > 0) {
                mate.hp = std::min(cap, mate.hp + amount);
            }
        }
    }
    // ② 神圣之荷待继承账：己方下只登场精灵获得 db[5] 盾 + db[6] 罩（"获得"=加算，
    //    k_dual_domain ⑤ 同原语；boss 门由死亡节点 boss_invalid 承担）。
    const int shield = db_at(row, 5, 400);   // db[5]，兜底字面 400
    const int cover = db_at(row, 6, 400);    // db[6]，兜底字面 400
    if (shield > 0 || cover > 0) {
        const int base = k1475Base + owner * kOwnerStride;
        write_int(ctx, base + k1475BeqShield, shield);
        write_int(ctx, base + k1475BeqCover, cover);
        arm_enter_watcher(ctx, base + k1475BeqWatchArmed, base + k1475BeqWatchId,
                          /*on_enter=*/[base](BattleContext* c, int entering_side) -> bool {
                              const int sh = read_int(c, base + k1475BeqShield);
                              const int cv = read_int(c, base + k1475BeqCover);
                              if (sh <= 0 && cv <= 0) {
                                  return false;
                              }
                              erase_int(c, base + k1475BeqShield);
                              erase_int(c, base + k1475BeqCover);
                              // 登场宠 = 该侧 on_stage 现值（perform_switch ⑥ 事件时点
                              // 新宠已完全就位）："获得"=加算（k_dual_domain ⑤ 同原语）。
                              ElfPet& pet = c->seerRobot[entering_side].elfPets[
                                  c->on_stage[entering_side]];
                              if (sh > 0) {
                                  pet.shield_bank_.add(ShieldBank::kSoulShieldPriority, sh,
                                                       /*source_id=*/kMark1475,
                                                       /*duration=*/0);
                              }
                              if (cv > 0) {
                                  pet.cover += cv;
                              }
                              return true;
                          });
    }
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1561 极影魔兽：先出手→封回血（当回合）；受攻伤>300→先制+1 永久累计 cap3；
//   受攻伤<300→增伤+30% 累计 cap90/120（切换/被击败重置）
// ═════════════════════════════════════════════════════════════════════════════

// enter：增伤账切换重置（记档④）+ 条件封回血票重授（K394 ④ 同通道）+ 受击账本
// watcher 每场一次。
EffectResult k402_1561_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1561Base + owner * kOwnerStride;
    erase_int(ctx, base + k1561AmpLedger);   // 增伤账切换重置（记档④）；先制账保留
    const int sid_key = base + k1561HealTicketSid;
    const int sid = read_int(ctx, sid_key);
    // 条件封回血票（挂对手）：条件=宿主在场 + 本回合自身先出手（heal 时点现读
    // preemptive_right，K395 881 [M3] 判据，记档⑥）。
    const int new_sid = ctx->rule_center_.grant_immune(
        /*owner=*/1 - owner, static_cast<int>(ImmunityType::HEAL_BLOCK), coverage_all(),
        /*anomaly_mask=*/0, /*duration_rounds=*/0, ctx->roundCount, sid,
        /*soul=*/false, EffectScope::TEAM, /*source_slot=*/-1, /*counts=*/0,
        /*source_effect_id=*/kMark1561, static_cast<int>(ImmunityTier::Modern),
        [owner](const BattleContext* c, int) {
            if (!c || !host_onstage_const(c, owner, kMark1561)) {
                return false;
            }
            return !c->is_second_mover(owner);   // "自身先出手"=本回合先手权归自身
        });
    write_int(ctx, sid_key, new_sid);
    // 受击账本 watcher（每场一次，arm 时捕获行参——单行族一场一宿主）。
    if (read_int(ctx, base + k1561Wired) == 1) {
        return EffectResult::kOk;
    }
    write_int(ctx, base + k1561Wired, 1);
    const HostRow row = host_row(ctx, a, kMark1561);
    const int hi = db_at(row, 0, 300);       // db[0]，兜底字面 300
    const int pl = db_at(row, 1, 1);         // db[1]，兜底字面 +1
    const int pl_cap = db_at(row, 2, 3);     // db[2]，兜底字面 3
    const int lo = db_at(row, 3, 300);       // db[3]，兜底字面 300
    const int step = db_at(row, 4, 30);      // db[4]，兜底字面 30
    const int amp_cap = db_at(row, 5, 90);   // db[5]，兜底本体字面 90
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, base, hi, pl, pl_cap, lo, step, amp_cap](BattleContext* c,
                                                         const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.actor == owner || ev.amount <= 0
                || !attack_damage_event(ev)) {
                return;   // 只认"被对手攻击命中"（K400 ④/K241 同门，记档⑦）
            }
            if (!onstage_has_soul_mark(c, owner, kMark1561)) {
                return;
            }
            if (ev.amount > hi && pl > 0) {
                // 高于阈值：先制层 +1 累计封顶（永久账，离场保留，记档④）。
                const int cur = read_int(c, base + k1561PlLedger);
                write_int(c, base + k1561PlLedger,
                          pl_cap > 0 ? std::min(cur + pl, pl_cap) : cur);
            } else if (ev.amount < lo && step > 0) {
                // 低于阈值：增伤 +step% 累计封顶（切换/被击败重置账）。
                const int cur = read_int(c, base + k1561AmpLedger);
                write_int(c, base + k1561AmpLedger,
                          amp_cap > 0 ? std::min(cur + step, amp_cap) : cur);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// FMR：两账消费（memset 后写；账"从下回合起"由受击晚于当回合 FMR 的时点天然成立）。
EffectResult k402_1561_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, kMark1561)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1561Base + owner * kOwnerStride;
    const int pl = read_int(ctx, base + k1561PlLedger);
    if (pl > 0) {
        ctx->ws.preemptive_level[owner] += pl;   // "攻击技能先制"——side 级 pl 槽（记档⑨通道）
    }
    const int amp = read_int(ctx, base + k1561AmpLedger);
    if (amp > 0) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(amp);
    }
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1630 天玄界神：回合开始高体→全属性+1 + 攻击先制+1 累计 cap3（切换重置）；
//   啸风：先出手攻击 P% 翻倍，触发爬坡 cap100（战斗级账）
// ═════════════════════════════════════════════════════════════════════════════

// enter：先制层账/触发戳/翻倍残旗清（"切换下场后重新叠加"，记档⑨）；啸风概率账保留
//（"本场战斗中"）；接线 SKILL_USED 掷点窗 + DETECT 翻倍消费票（每场一次）。
EffectResult k402_1630_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1630Base + owner * kOwnerStride;
    erase_int(ctx, base + k1630PlLedger);
    erase_int(ctx, base + k1630TriggerRound);
    erase_int(ctx, base + k1630DblFlag);
    if (read_int(ctx, base + k1630Wired) == 1) {
        return EffectResult::kOk;
    }
    write_int(ctx, base + k1630Wired, 1);
    const HostRow row = host_row(ctx, a, kMark1630);
    const int base_pct = db_at(row, 3, 40);   // db[3]，兜底字面 40
    const int step = db_at(row, 4, 10);       // db[4]，兜底本体字面 10
    const int cap = db_at(row, 5, 100);       // db[5]，兜底字面 100
    if (base_pct <= 0) {
        return EffectResult::kOk;
    }
    // 啸风掷点窗（E13 使用即发，miss 也算——先例同窗）：先手权已结算（post-FMR，
    // K395 881 [M3] 判据）+ 攻击技。入口清残旗 → 掷前写审计 → 成功置翻倍旗+爬坡。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, base, base_pct, step, cap](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || ev.used_attribute) {
                return;   // own + 攻击技
            }
            if (!onstage_has_soul_mark(c, owner, kMark1630) || c->is_boss_challenge) {
                return;
            }
            if (c->is_second_mover(owner)) {
                return;   // "自身攻击先出手时"（K395 881 [M3] 判据）
            }
            erase_int(c, base + k1630DblFlag);   // 入口清残旗（miss 防泄漏，记档⑩）
            int pct = read_int(c, base + k1630WindPct);
            if (pct <= 0) {
                pct = base_pct;   // 无账本 = 基础档
            }
            write_int(c, base + k1630WindAudit, pct);   // 审计槽：掷前写现行档
            if (!roll_percent(c->rng(), pct)) {
                return;   // 未触发无补偿子句（触发=成功语义，记档⑩）
            }
            write_int(c, base + k1630DblFlag, 200);     // "伤害翻倍"=×2
            write_int(c, base + k1630WindPct,
                      step > 0 ? std::min(pct + step, cap) : pct);   // 触发爬坡封顶
            write_int(c, base + k1630WindLand, read_int(c, base + k1630WindLand) + 1);
        },
        WatcherScope::TEAM);
    // DETECT 翻倍消费票（own 攻击红伤：flag>0 → final×flag/100 → 清旗）。
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, base](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark1630) || c->is_boss_challenge) {
                return;
            }
            const int flag = read_int(c, base + k1630DblFlag);
            if (flag <= 0) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != 1 - owner || !d.isRed
                || d.final <= 0) {
                return;   // 自身攻击红伤才消费
            }
            d.final = d.final * flag / 100;
            erase_int(c, base + k1630DblFlag);   // 首击消费（K400 ⑥ 记档口径）
        });
    return EffectResult::kOk;
}

// ROUND_START：高体门（K377 口径：现值严格比较）→ 全属性+1 六槽循环 + 层账爬坡 +
// 触发戳（当回合）。
EffectResult k402_1630_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1630Base + owner * kOwnerStride;
    erase_int(ctx, base + k1630TriggerRound);   // 每回合重置触发戳（FMR 消费当回合）
    const ElfPet& self = ctx->getPet(owner);
    const ElfPet& foe = ctx->getPet(1 - owner);
    if (!(self.hp > 0 && foe.hp > 0 && self.hp > foe.hp)) {
        return EffectResult::kOk;   // 高体门不满足（等号不触发，K377 严格档）
    }
    const HostRow row = host_row(ctx, a, kMark1630);
    const int up = db_at(row, 0, 1);          // db[0]，兜底字面 +1
    const int pl = db_at(row, 1, 1);          // db[1]，兜底字面 +1
    const int pl_cap = db_at(row, 2, 3);      // db[2]，兜底字面 3
    if (up > 0) {
        for (int i = 0; i < kCombatStatCount; ++i) {
            (void)g_core->stat_change(ctx, owner, i, up);   // 六槽含命中（记档⑧）
        }
    }
    if (pl > 0) {
        const int cur = read_int(ctx, base + k1630PlLedger);
        write_int(ctx, base + k1630PlLedger,
                  pl_cap > 0 ? std::min(cur + pl, pl_cap) : cur);
        write_int(ctx, base + k1630TriggerRound, ctx->roundCount + 1);
    }
    return EffectResult::kOk;
}

// FMR：触发戳==当回合 → 先制 +层账（memset 后写，K394 ③/K395 881 同构），消费即清戳。
EffectResult k402_1630_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, kMark1630)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1630Base + owner * kOwnerStride;
    if (read_int(ctx, base + k1630TriggerRound) != ctx->roundCount + 1) {
        return EffectResult::kOk;
    }
    erase_int(ctx, base + k1630TriggerRound);
    const int pl = read_int(ctx, base + k1630PlLedger);
    if (pl > 0) {
        ctx->ws.preemptive_level[owner] += pl;
    }
    return EffectResult::kOk;
}

// ROUND_END：翻倍旗兜底清（本攻击 miss 未消费 → 不跨回合残留，记档⑩）。
EffectResult k402_1630_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    erase_int(ctx, k1630Base + a.owner * kOwnerStride + k1630DblFlag);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（1475/1561/1630 入账，只增不改）────
// boss 尾注：三行 (boss无效) 全为 tips 末位全句尾注 → 全节点 boss_invalid=true
//（K400 ⑨/K399 boss 口径，记档⑤）。
SOUL_PROGRAM(kMark1475, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k402_1475_enter,
                               /*once=*/false, /*early=*/true, SoulScope::STAGE,
                               /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_OPPONENT_DEFEAT_STATUS,
                                   &k402_1475_defeat_status,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k402_1475_fmr,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_FIRST_MOVER_DEATH, &k402_1475_death,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_SECOND_MOVER_DEATH, &k402_1475_death,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true)});

SOUL_PROGRAM(kMark1561, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k402_1561_enter,
                                   /*once=*/false, /*early=*/true, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k402_1561_fmr,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true)});

SOUL_PROGRAM(kMark1630, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k402_1630_enter,
                                   /*once=*/false, /*early=*/true, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k402_1630_round_start,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k402_1630_fmr,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_END, &k402_1630_round_end,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true)});
