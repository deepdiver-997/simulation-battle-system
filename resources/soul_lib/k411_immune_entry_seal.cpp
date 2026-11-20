// K411 受击免疫/登场封属族 —— effect 1480/1528（2 宏 / 4 行）
//（看板 K411「受击免疫/登场封属族 2026-11-04」，a01-k411，worktree ../agents/k411；
//  号段 7710-7719）
//
// 行清单（tools/soul_effects_dump.py 1480 1528，2026-11-04）：
//   1480 伽玛·夜煞 3893 row1162 args[40,40,1,1,1]（kind[6,12,14]）：
//     「受到攻击时有40%的概率免疫当回合受到的攻击伤害，未触发则战斗阶段结束时附加
//     自身攻击值40%的百分比；当回合自身受到的攻击伤害则战斗阶段结束时令对手全属性
//     -1，当回合自身未受到的攻击伤害则战斗阶段结束时令自身全属性+1（boss无效）」
//     row1962 伙伴行 args[50,70,1,1,1]（come=千机再构伙伴效果）：40→50 / 40→70。
//   1528 墨影星魂 3932 row1197 args[2,10,50,300,1,100,300,1]（kind[1,5,13]）：
//     「每次登场后对手当前在场精灵2回合内属性技能失效且令己方月照星魂在本场战斗中
//     造成的伤害提升10%（最高50%）；|回合开始时，若对手体力低于300则当回合自身攻击
//     技能先制+1且100%打出致命一击，若对手体力高于300则当回合自身攻击命中后{吸取}
//     对手300点{体力}（boss无效）」
//     row1436 伙伴行 args[2,10,50,300,2,100,350,1]（come=无关风月伙伴效果）：
//     先制+1→+2 / 吸取300→350。
//
// 行参解码（db_at 原序读 owned_int_args；两行互锚逐位与 tips 自洽）：
//   1480：db[0]=免疫概率%（40/50）、db[1]=未触发补偿比%（自身攻击值，40/70）、
//         db[2..4]="1 1 1" 三旗不消费——两分支子句（对手-1/自身+1）是行文固定结构
//         无关断开关可锚定（majority reading，记档）；boss 尾注旗落注册面
//         boss_invalid=true（K396 四行同纪律）。
//   1528：db[0]=封属回合数（2）、db[1]=月照增伤幅度%（10）、db[2]=月照增伤上限%
//         （50）、db[3]=体力阈值（300）、db[4]=先制+N（1/2）、db[5]=必暴%（100）、
//         db[6]=吸取量（300/350）、db[7]=boss 尾注旗（不消费，落注册面）。
//
// ⚠️ 口径记档（majority reading，均按先例）：
//   ① 1480 免疫 = BLOCK 类概率票（K 守护族 150 暴击免疫直系同位：DamagePhase::BLOCK
//     桶 roll_percent → 全字段清零）+ K662 式审计槽（掷前写申报值、掷后写结果值，
//     场景无随机精确断言申报 + 统计带侧证概率落地）。逐笔判定：每笔攻击红伤独立
//     掷（150 同构；连击各段独立），「当回合」修饰由票常驻自然满足。
//   ② 1480 未触发补偿 = 逐笔记账（本回合掷败计数 N）→ 战阶末附加 N 笔
//     自身攻击值×db[1]% 的 PERCENT_VALUE 粉伤（「百分比伤害」字面→PERCENT_VALUE，
//     K399 1020① 同 kind；「自身攻击值」= numericalBase 物攻原始面板，K243 1522
//     atk_panel 同源）。每笔掷败对应一笔补偿（逐次 majority，记档）。
//   ③ 1480 属性分支 = 受击旗（1052 形制）：TAKE_DAMAGE watcher 只认被对手打的
//     **红伤**（ev.damage_kind==NORMAL，1843 受击计数同门——「攻击伤害」字面，
//     与 1040「受到攻击时」红粉不滤口径的区别在此，按文本强度取红伤滤，记档）。
//     免疫落地笔 amount=0 不置旗 → 「未受到攻击伤害」支自洽（自身+1）。
//     战阶末：旗在→对手六槽 stat_drop（1210 drop_all_six 同原语，查免弱）；
//     旗不在→自身六槽 stat_change（自增益不查免弱）。旗每回合 ROUND_END 消费。
//   ④ 1528 封属 = 登场一次性窗（非 1470① 每回合重授——「2回合内」固定窗）：
//     seal_skill(attribute=true, count=0 不限次数, duration=db[0]=2, 固定句柄
//     effect_id=kMarkId 重复授予=刷新, penetrable=true, ON_STAGE, hit_invalid=
//     false)。tick_rounds 在回合末（battleFsm:2169）→ 登场授 rounds=2 覆盖登场
//     当回合+下回合，恰「2回合内」。「对手当前在场精灵」按 1843/1470① 先例落
//     **侧别**票（引擎票面无个体维度；换宠延续性不建模，记档）。
//   ⑤ 1528 月照增伤 = roster/species 查询（ElfPet::id==3866 遍历己方六格，通道
//     点名可达）→ 侧别账本叠层（墨影每次登场+1 层，上限 db[2]/db[1]=5 层）；
//     AMP 票实时读账本（203 AMP 同位），回调内 getPet(attackerId).id==3866 判
//     「月照星魂造成伤害」（attackerId 是侧别，攻击者必在场 → getPet 即其本尊）。
//     己方无月照星魂时叠层空转（字面「令己方月照星魂…提升」无对象不落地），记档。
//   ⑥ 1528 回合开始双支 = 1264 孙权直系同构（ROUND_START 采样→账本显式写/清）：
//     对手 hp<db[3]（严格）→ 先制必暴支；hp>db[3]（严格）→ 吸取支；==db[3] 字面
//     空缺（「低于」「高于」都闭）→ 双支不触发（记档，majority 按字面）。
//     先制：FMR 桶 memset 后写 ws.preemptive_level[owner]+=db[4]，消费即清旗
//     （K394 ⑤/K399 1264 直系）；必暴：ROUND_START 直接 ws.must_crit[owner]=true
//     （ws 每回合 reset → 天然当回合有效，227 先例；db[5] 按概率 roll，100 恒真）。
//     吸取：DETECT 票伤害时点读高体旗 → 对手掉 db[6] FIXED 粉伤 + 自身恢复
//     **实际**扣除量（heal_amount 走原语吃封回血钳制；「吸取」= deal+heal 实量
//     口径，K228 heal_drain 同源；CoreApi 未暴露 drain_hp_amount → 组合落法）。
//   ⑦ 概率/账本全走行参（args 驱动），兜底 tips 字面；0 档=关闸直读不回落（K396
//     纪律）。
//
// ⚠️ 本单实测新坑四条（K 系新单必读，2026-11-04）：
//   a) 「每次登场」必须落 **early** ENTER 节点 + epoch 幂等守卫（armed_this_epoch，
//     pipeline_valid_id 只在 invalidate_on_stage_effects 即换宠时递增）——非 early 的
//     ENTER 桶节点只在开局 init_battle 的 execute_registered_actions 跑一次，换宠
//     登场走 activate_soul_mark 只直调 early 节点（soul_mark_manager.cpp:436）。
//   b) 「本场」buff 类票必须 register_default_damage_effect（TEAM 绑定）——
//     register_damage_effect 是 ON_STAGE 绑定，挂印者换下即整票作废（T7716 实测
//     月照增伤 raw==amp 复现后改 TEAM 面 + 回调 species 精判，误伤面为空）。
//   c) SEAL 票生命周期 = tick 递减模型（tick_rounds 每回合末递减删票），响应循环
//     （rule_center.h notify）**无窗过期检查**——has_seal_kind 的 window_expired
//     绝对窗判定与实际响应不一致（register_round 恒 0 → 审计查询恒报过期），
//     行为断言别用 has_seal_kind。登场授 rounds=N 覆盖登场当回合起 N 回合 ✓。
//   d) 场景 CHECK_EQ 失败分支对 a **再求值一次**（double-eval）——带 drive 副作用的
//     表达式必须先落变量再断言（T7715 首版踩坑；harness REQUIRE 注同源）。
//     变异部署：python 写文件 + touch 同秒 → mtime 不重编 → deploy 旧 dylib 假绿
//     （mutant2 首跑 rc=0 假象实证）——mutate 与 touch 之间必须 sleep 1 跨秒。
//
// 三查记录（a01-k411 2026-11-04，worktree 内复核）：
//   ①内核 grep -w 1480/1528 于 src/ 零命中（无魂印行为硬编码）；
//   ②effect_info 1480=「附加{0}点固定伤害，每次使用额外附加{1}点，最高{2}点」/
//     1528=「附加{0}点真实伤害」——均技能空间模板、与魂印 tips 全异文，effect_icon
//     tips 为权威（SOP §六.1）；
//   ③registry：soul_lib 全库 grep 无 SOUL_PROGRAM(1480/1528)——真缺口。
// 键段：990411000-990411700（check_storage_keys.py 全库扫描无撞）。
// 先例：k_guard_family 150（BLOCK 概率票直系）、k_prob_compensate（概率+补偿双节点
//   /审计槽/0 档纪律）、k301 1052（受击旗+ROUND_END 清旗）、k_anomaly_threshold_1210
//   （六槽 stat_drop drop_all_six）、k399 1264/1470①（回合开始账本/FMR/DETECT 吸取/
//   seal_skill 同式）、k_hit_suppress_cond 1843（TAKE_DAMAGE 红伤过滤+duration=2 窗）、
//   k_panel_family 203（AMP 票实时读）、k243（atk_panel 攻击值）、K662（审计槽）。
// 场景回归：test/scenario/scenario_7710_immune_entry_seal.cpp（号段 7710-7719）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── 存储槽键段（tools/check_storage_keys.py 已核无撞；plugin_storage 键 = int）────
constexpr int kWired1480 = 990411000;     // +owner：BLOCK 票/watcher 已挂旗标
constexpr int kAudit1480Pct = 990411100;  // +owner：本次免疫申报概率（掷前写，K662）
constexpr int k1480FailCnt = 990411200;   // +owner：本回合免疫掷败计数（逐笔补偿）
constexpr int k1480HitFlag = 990411300;   // +owner：本回合受击旗（1/0）
constexpr int kWired1528 = 990411400;     // +owner：1528 AMP/DETECT 票已挂旗标
constexpr int k1528AmpStacks = 990411500; // +owner：月照星魂增伤层数（本场累计）
constexpr int k1528LowFlag = 990411600;   // +owner：对手低体旗（先制必暴支）
constexpr int k1528HighFlag = 990411700;  // +owner：对手高体旗（吸取支）

constexpr int kMark1480 = 1480;
constexpr int kMark1528 = 1528;
constexpr int kMoonlightSpeciesId = 3866;   // 月照星魂 species（monsters 表实测）
constexpr int kCombatStatSlots = 6;         // 全属性 = 六项能力等级（1210 口径）

// 「自身攻击值」= 物攻原始面板（K243 1522 同源）。
int atk_panel(const ElfPet& pet) {
    return pet.numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK];
}

// ── 宿主行参数（db_at 原序读 owned_int_args——K367/K396 同款；禁读 a.row）──────
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

// 侧别账本（plugin_storage int 槽；k399 set_flag/flag_of 私副本同形）。
int flag_of(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return 0;
    }
    return any_int_or(it->second, 0);
}

void set_flag(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

void clear_flag(BattleContext* ctx, int key) {
    ctx->plugin_storage.erase(key);
}

// 对手全属性-1（六槽逐个 stat_drop，查免弱；1210 drop_all_six 同原语。amount 必须
// 为正——amount<=0 = INVALID_PARAM 静默无效）。
void drop_all_six(BattleContext* ctx, int target, int actor) {
    if (!ctx || !g_core || !g_core->stat_drop) {
        return;
    }
    for (int i = 0; i < kCombatStatSlots; ++i) {
        (void)g_core->stat_drop(ctx, target, i, /*amount=*/1, actor);
    }
}

// 自身全属性+1（六槽逐个 stat_change，自增益通道不查免弱；4797 ④同口径）。
void raise_all_six(BattleContext* ctx, int target) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return;
    }
    for (int i = 0; i < kCombatStatSlots; ++i) {
        (void)g_core->stat_change(ctx, target, i, /*delta=*/1);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// effect 1480 伽玛·夜煞：受击 db[0]% 免疫当回合攻击伤害（BLOCK 票），掷败逐笔记账
// → 战阶末补偿 db[1]% 攻击值粉伤；受击旗分支 → 对手全-1 / 自身全+1
// ═══════════════════════════════════════════════════════════════════════════

// epoch 幂等守卫（k_guard_family armed_this_epoch 的 plugin_storage 版）：同一登场
// epoch 只过一次——开局 early 双触发（activate+ENTER 桶）防重；换宠 bump epoch 后
// 重跑（ON_STAGE 票已随下场 invalidate，重挂恢复）。
bool armed_this_epoch(BattleContext* ctx, int owner, int key) {
    const auto it = ctx->plugin_storage.find(key);
    const int cur = ctx->pipeline_valid_id[owner];
    if (it != ctx->plugin_storage.end() && any_int_or(it->second, -1) == cur) {
        return true;
    }
    ctx->plugin_storage[key] = cur;
    return false;
}

EffectResult k1480_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, kMark1480)) {
        return EffectResult::kOk;
    }
    // 受击旗 watcher（TEAM、wired-once：事件面不随换宠清，重挂会重复）。
    const int watcher_key = kWired1480 + owner;
    if (!ctx->plugin_storage.count(watcher_key)) {
        ctx->plugin_storage.emplace(watcher_key, 1);
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                if (!c) {
                    return;
                }
                if (ev.target != owner || ev.actor == owner || ev.amount <= 0) {
                    return;   // 只认"被对手打"且真掉血（1052 同门）
                }
                if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                    return;   // 「攻击伤害」= 红伤（粉伤/固伤/真实伤不置旗）
                }
                if (!onstage_has_soul_mark(c, owner, kMark1480)) {
                    return;
                }
                c->plugin_storage[k1480HitFlag + owner] = 1;
            },
            WatcherScope::TEAM);
    }
    // BLOCK 票（ON_STAGE 面：下场随 invalidate 清、再登场 epoch 变 → 此处重挂）。
    if (armed_this_epoch(ctx, owner, kWired1480 * 2 + owner)) {
        return EffectResult::kOk;   // 本登场 epoch 已挂
    }
    // 行参 arm 时捕获（单行族一场一宿主，K396 同窗同款）。0 档=关闸直读。
    const HostRow row = host_row(ctx, a, kMark1480);
    const int immune_pct = db_at(row, 0, 40);   // db[0]，兜底 tips 字面 40

    // BLOCK 票：受攻击红伤时掷 db[0]%，成功全字段清零（150 号同位）；掷败逐笔记账。
    ctx->register_damage_effect(
        DamagePhase::BLOCK, owner, DamageEffectCategory::BLOCK,
        [owner, immune_pct](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core) {
                return;
            }
            if (c->is_boss_challenge) {
                return;   // boss 尾注（双保险；节点面 boss_invalid 已挡 arm）
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;   // 只免攻击红伤（「攻击伤害」字面；150 同门）
            }
            if (!onstage_has_soul_mark(c, bucket_owner, kMark1480)) {
                return;
            }
            c->plugin_storage[kAudit1480Pct + owner] = immune_pct;   // 审计槽：掷前写
            if (!roll_percent(immune_pct)) {
                // 未触发：逐笔记账 → 战阶末补偿（majority 逐次，记档②）
                const int cnt_key = k1480FailCnt + owner;
                c->plugin_storage[cnt_key] = flag_of(c, cnt_key) + 1;
                return;
            }
            // 免疫落地：全字段清零（K 守护族 150 同位）
            d.base = 0;
            d.afterAdd = 0;
            d.afterMul = 0;
            d.final = 0;
            d.addPct = 0;
            d.mulCoef = 0.0;
        });
    return EffectResult::kOk;
}

// 战阶末：①掷败逐笔补偿（N 笔 自身攻击值×db[1]% PERCENT_VALUE）；②受击旗分支
// 对手全-1 / 自身全+1；无论分支清账（下回合重计）。
EffectResult k1480_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark1480)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const ElfPet& self = ctx->getPet(owner);

    // ① 未触发补偿（逐笔）
    const int fail_key = k1480FailCnt + owner;
    const int fails = flag_of(ctx, fail_key);
    ctx->plugin_storage.erase(fail_key);
    if (fails > 0 && self.hp > 0) {
        const HostRow row = host_row(ctx, a, kMark1480);
        const int pct = db_at(row, 1, 40);   // db[1]，兜底 tips 字面 40
        const int attach = atk_panel(self) * pct / 100;
        if (attach > 0) {
            for (int i = 0; i < fails; ++i) {
                if (ctx->getPet(opp).hp > 0) {
                    (void)g_core->deal_pink_damage(ctx, opp, attach,
                                                   DamageKind::PERCENT_VALUE, owner);
                }
            }
        }
    }

    // ② 受击旗分支（1052 形制：无论分支，回合末清旗）
    const int hit_key = k1480HitFlag + owner;
    const bool hit = flag_of(ctx, hit_key) == 1;
    ctx->plugin_storage.erase(hit_key);
    if (hit) {
        drop_all_six(ctx, opp, owner);   // 对手全属性-1（查免弱）
    } else {
        raise_all_six(ctx, owner);       // 自身全属性+1（自增益）
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// effect 1528 墨影星魂：登场封属 db[0] 回合窗 + 己方月照星魂本场增伤叠层；
// 回合开始按对手体力 db[3] 阈值双支（先制+必暴 / 命中后吸取）
// ═══════════════════════════════════════════════════════════════════════════

// 己方 roster 是否有月照星魂（species 3866；ElfPet::id=species id）。
bool roster_has_moonlight(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    for (const ElfPet& pet : ctx->seerRobot[owner].elfPets) {
        if (pet.id == kMoonlightSpeciesId) {
            return true;
        }
    }
    return false;
}

// enter(early)：每次登场执行——①接线 AMP/DETECT 票（TEAM 面，wired-once 一次性）；
// ②封属 db[0] 回合窗；③月照星魂在队 → 叠层+1。
// ⚠️ 「每次登场」必须落 early 节点：换宠登场走 activate_soul_mark（只执行 early 节点
//    + on_enter，soul_mark_manager.cpp:436），非 early 的 ENTER 桶节点只在开局跑
//    （init_battle 的 execute_registered_actions）——2026-11-04 场景 T7716 实测换宠
//    再登场不叠层后修正。开场 early 双触发（activate + ENTER 桶各一次）用 epoch
//    幂等守卫去重；换宠 bump epoch → 重跑登场段。
EffectResult k1528_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1528)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1528);

    // ① 票接线（TEAM 注册面不被清，wired-once 一次性；boss 局节点不跑天然不挂）。
    const int key = kWired1528 + owner;
    if (!ctx->plugin_storage.count(key)) {
        ctx->plugin_storage.emplace(key, 1);
        const int amp_step = db_at(row, 1, 10);   // db[1]，兜底 tips 字面 10
        const int drain_amount = db_at(row, 6, 300);  // db[6]，兜底 row1197 字面 300

        // 月照星魂增伤 AMP 票：实时读叠层账本（203 AMP 同位）。⚠️ 必须走 **default/TEAM**
        //    注册面：增伤是月照的整场 buff（墨影下场后仍生效），ON_STAGE 注册面会在墨影
        //    换下时把票清掉（2026-11-04 场景 T7716 实测 raw==amp 复现）——
        //    register_default_damage_effect = TEAM 常驻不受切换作废（battleContext.h:1496
        //    注；1264 DETECT 票同款）。回调按 species 精确判攻击者，误伤面为空。
        ctx->register_default_damage_effect(
            DamagePhase::AMP, owner, DamageEffectCategory::AMP,
            [owner, amp_step](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner || !g_core) {
                    return;
                }
                if (c->is_boss_challenge) {
                    return;
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != bucket_owner || !d.isRed || d.final <= 0) {
                    return;
                }
                // ⚠️ 不设墨影 onstage 门：「本场战斗中造成的伤害提升」是月照的整场
                //    buff（墨影下场后月照攻击仍享），账本已记录（记档⑤）。
                // 「月照星魂造成伤害」：攻击者必在场 → getPet(侧) 即本尊（记档⑤）
                if (c->getPet(bucket_owner).id != kMoonlightSpeciesId) {
                    return;
                }
                const int stacks = flag_of(c, k1528AmpStacks + owner);
                const int cur_pct = stacks * amp_step;
                if (cur_pct <= 0) {
                    return;
                }
                d.addPct += cur_pct;
                d.final = d.final * (100 + cur_pct) / 100;
            });

        // 吸取 DETECT 票：高体旗开 → 攻击命中后吸取 db[6]（deal FIXED + heal 实量）。
        ctx->register_default_damage_effect(
            DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
            [owner, drain_amount](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner || !g_core || !g_core->deal_pink_damage
                    || !g_core->heal_amount) {
                    return;
                }
                if (c->is_boss_challenge) {
                    return;
                }
                const DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                    return;   // 只随自己的攻击红伤附加（1264 同门）
                }
                if (!onstage_has_soul_mark(c, owner, kMark1528)) {
                    return;
                }
                if (flag_of(c, k1528HighFlag + owner) != 1 || drain_amount <= 0) {
                    return;
                }
                const int opp = c->opponent(owner);
                if (c->getPet(opp).hp <= 0) {
                    return;
                }
                // 「吸取对手300点体力」：对手掉 amount、自身恢复**实际**扣除量
                // （heal 原语吃封回血钳制；K228 实量口径）。
                const int foe_hp_before = c->getPet(opp).hp;
                (void)g_core->deal_pink_damage(c, opp, drain_amount, DamageKind::FIXED,
                                               owner);
                const int drained = foe_hp_before - c->getPet(opp).hp;
                if (drained > 0) {
                    (void)g_core->heal_amount(c, owner, drained);
                }
            });
    }

    // ②③ 登场段：epoch 幂等（开局 early 双触发防重；换宠 bump epoch 重跑）。
    if (armed_this_epoch(ctx, owner, kWired1528 * 2 + owner)) {
        return EffectResult::kOk;
    }
    const int seal_rounds = db_at(row, 0, 2);   // db[0]，兜底 tips 字面 2
    const int amp_step2 = db_at(row, 1, 10);
    const int amp_cap = db_at(row, 2, 50);      // db[2]，兜底 tips 字面 50

    // ② 封属窗：属性技失效 = SEAL_ATTRIBUTE（K311/K399-1470① 口径）；count=0
    //    不限次数、duration=db[0] 固定窗、固定句柄=刷新、tick 回合末 → 覆盖登场
    //    当回合+下回合（记档④）。
    if (seal_rounds > 0) {
        (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/1 - owner,
                                 /*effect_id=*/kMark1528,
                                 /*attribute=*/true, /*attack=*/false,
                                 /*count=*/0, /*duration_rounds=*/seal_rounds,
                                 /*penetrable=*/true, /*source_slot=*/-1,
                                 EffectScope::ON_STAGE, /*hit_invalid=*/false,
                                 /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    }

    // ③ 月照星魂叠层：己方 roster 有 3866 才叠（无对象空转，记档⑤）；上限
    //    amp_cap/amp_step 层（db 驱动；50/10=5 层封顶）。零步/零上限 = 关闸不叠。
    if (amp_step2 > 0 && amp_cap > 0 && roster_has_moonlight(ctx, owner)) {
        const int max_stacks = amp_cap / amp_step2;
        const int cur = flag_of(ctx, k1528AmpStacks + owner);
        if (cur < max_stacks) {
            ctx->plugin_storage[k1528AmpStacks + owner] = cur + 1;
        }
    }
    return EffectResult::kOk;
}


// ROUND_START：对手体力阈值采样 → 账本（显式写/清，K229 口径）+ 先制必暴支直写。
EffectResult k1528_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1528)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1528);
    const int threshold = db_at(row, 3, 300);   // db[3]，兜底 tips 字面 300
    const int crit_pct = db_at(row, 5, 100);    // db[5]，兜底 tips 字面 100
    const int opp = 1 - owner;
    const int foe_hp = ctx->getPet(opp).hp;

    const bool low = foe_hp < threshold;    // 严格档（「低于」字面）
    const bool high = foe_hp > threshold;   // 严格档（「高于」字面；==300 双支空缺）
    set_flag(ctx, k1528LowFlag + owner, low ? 1 : 0);
    set_flag(ctx, k1528HighFlag + owner, high ? 1 : 0);

    if (low && crit_pct > 0 && roll_percent(crit_pct)) {
        ctx->ws.must_crit[owner] = true;   // 必暴（ws 每回合 reset → 当回合有效）
    }
    return EffectResult::kOk;
}

// FMR：低体旗 → 当回合攻击技先制 +db[4]（memset 后写，K394 ⑤ 直系），消费即清旗。
EffectResult k1528_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMark1528)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (flag_of(ctx, k1528LowFlag + owner) == 1) {
        const HostRow row = host_row(ctx, a, kMark1528);
        const int pl = db_at(row, 4, 1);   // db[4]，兜底 tips 字面 1
        if (pl > 0) {
            ctx->ws.preemptive_level[owner] += pl;   // 「攻击技能」——引擎先制无类型
                                                     // 维度，全局加（1843 同款近似）
        }
    }
    clear_flag(ctx, k1528LowFlag + owner);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +2（1480/1528，只增不改）────────
// 两 effect 四行 tips 尾注全（boss无效）→ 全节点 boss_invalid=true。
SOUL_PROGRAM(kMark1480,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k1480_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k1480_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});

SOUL_PROGRAM(kMark1528,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k1528_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k1528_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k1528_fmr,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
