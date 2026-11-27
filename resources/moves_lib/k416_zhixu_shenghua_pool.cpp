// K416 逐宠定制池首批 —— 至序圣华 4321 专属效果族（8 宏落地 + 1 内核门槛记档）
// （看板 K416，a01-k416 2026-11-05，worktree ~/.agent-workspaces/workspaces/sbs-local/k416，
//  分支 feat/k416；场景号段 7900-7909，场景 test/scenario/scenario_7900_zhixu_shenghua_pool.cpp。
//  选载体依据：coverage_recompute 2026-11-05 23:21 快照（链级可用率 92.72% 非空链口径，
//  registry moves=1413）逐宠聚合 4321 链级可用率 56.25%（9/16），缺口连号段 1757-1762
//  为年费精灵专属定制资源族（圣剑/处决印记），未被 J/K 批产覆盖。）
//
// 行清单（moves 切片 side_effect.arg_count 优先；全部 self-consistent 逐位核对）：
//   519  [2] "{0}回合内自身每处于一种能力提升或下降状态时都会恢复{1}点体力"
//        载体：秩序重建 27838[5,100]（4321）/ 圣光守护 22588[5,40]（黄金天马 1960 系）/
//        春台沐风 28638[3,100]（炽寒妖兽 3884）/ 浮游荧 29282[3,50]（呱太郎 3922）
//   1720 [1] "对手存在护盾则附加对手护盾值{0}%的真实伤害"
//        载体：原初圣印 35615[40]（4321）/ 罹罪血龙吟 35321[50]（库莱姆斯 4264）/
//        霜·凌天寂帝枪 36628[60]（潜隐灵蛇 4146）
//   1476 [2] "使对手全属性-{0}，自身体力低于1/{1}时弱化效果翻倍"
//        载体：剑决浮云 35618[1,2]（4321）/ 料峭之境 26775 / 树蛙粘液 26874 / 覆羽而行
//        26913 / 不洁之夜 28218 / 海天相映 28749（全部 [1,2]）
//   1757 [2] "免疫下{0}次自身受到的异常状态，每次免疫成功则在回合结束时附加{1}点真实伤害"
//        载体：天命已拟 27840[2,150]（4321 专属）
//   1758 [3] "{0}回合内对手使用攻击技能则使对手随机{1}个技能的PP值归零且为己方场上附加{2}柄圣剑"
//        载体：天命已拟 27840[4,2,1]（4321 专属）
//   1759 [2] "{0}回合内对手使用属性技能则被处决精灵所有技能PP值-{1}"
//        载体：天命已拟 27840[4,1]（4321 专属）
//   1760 [1] "全属性+{0}，对手存在处决印记则强化效果翻倍"
//        载体：众生之道 27841[1]（4321）/ 光辉意志 28029[1]（云晔 3937）
//   1762 [3] "出手时若自身体力低于对手则{0}%令对手{1}，未触发则令自身下回合所有技能先制+{2}"
//        载体：圣御·征伐四方 35619[100,7,2]（4321，{1}=7 疲惫）/ 龙契·惘 28935[100,23,1]
//        （龙琰 4712，{1}=23 诅咒）
//
// 动工前置三查 + 第四查（2026-11-05）：
//   ① 内核 grep 8 id：仅注释数字巧合（519=取整算例、1760=LIKE 误中注），零行为硬编码；
//   ② effect_info 模板在列、custom_effect_programs 6 行（179/700/1083/1257/1248/456）
//     零命中、custom_effect_overrides 0 行；
//   ③ 全库并集无注册：registry moves=1413 无 8 id；feat 分支（r4/r5/s24/s25/s27/s4/
//     windows-*）仅命中 resources/boss_invalid_effects.json 与场景号段注释——
//     其中 1757/1758/1760 的 boss 无效记录来自**魂印空间** effect_icon 同号行
//     （查梅洛 4167 等 tips 带"（boss无效）"），SKILL_EFFECT 与 SOUL_MARK 两注册空间
//     互不相干（速查 §二"effect 1004 ≠ 魂印 1004"口径），本文件 8 行的 4399 技能文本
//     均无 boss 尾注 → boss_invalid 全 false；
//   ④ parser 覆盖面（EffectUnit 兜底，废弃冻结但仍在树）：519/1720/1476/1757/1758/
//     1759/1760 逐条对 parse_status_inflict_primary 匹配语义 = NONE 安全跳过（无
//     "令/使对手{"主句、无回合窗误接）；**1762 = PARTIAL·前文条件+兜底双丢失**——
//     "出手时若自身体力低于对手则{0}%令对手{1}…" 的主句 "{0}%令对手{1}" 被 find()
//     裸命中（前缀 HP 门被静默丢弃、未触发兜底不接）→ 本宏落地前 圣御·征伐四方/
//     龙契·惘 每次使用都被 parser 寄生为无条件 100% 疲惫/诅咒（2 回合硬编码）。
//     本实现注册后 clone_effect 先命中、parser 通道对该 id 关闭 = 寄生修复
//     （S22 §3.4 危险面 -1 模板）。场景 7900 有门关→无异常的对照断言钉住此修复。
//
// 内核门槛记档（不产码）：1634 "自身体力低于250时必定先手"（先驱 35610/灵能眩光击
//   34692/断妄之律 36544）= 条件先制族，正解 = src/entities/skills.cpp 的
//   selection_effects_ 硬编码路由表加行（J115/J66 定版口径）——内核 src/ 本单不可碰，
//   按派发纪律记档跳过，留内核微任务。
//
// 圣剑/处决印记资源建模（§六 资源计数，nutao 4690000/diwang 4691000/gelaorui 4692000/
//   monika 4693000 顺延）：
//   - 圣剑 plugin_storage[4694000+side]：本单 1758 是全库**第一个写点**（soul_lib/
//     j_defeat_family.cpp 记档"缺主魂印 1897『附加1柄圣剑』的写点"即此）；消费端
//     （圣剑重铸：魂印 1897/1972）未实现，本单只产不消、只增不减。
//   - 处决印记 plugin_storage[4694010+side*6+slot]：生产端 = 至序圣华魂印 1897
//     "登场时对对手背包当前体力最高者附加1层"（未实现，层恒 0）→ 1759/1760 的查询
//     现阶段对真实对局恒走"无印记"分支；场景以白盒直写印记槽验证查询路径（烙印槽
//     布局是本文件实现细节，魂印任务落地时以本段为准对接）。与 j_prob_anomaly_
//     fallback.cpp 1824 记档（"印记原语落地后此处接 counts 查询"）同因同解。
//
// 口径记档：
//   - 519 tick 挂 AFTER_ACTION 双桶（57 形：恰好一桶/回合、出招回合即首 tick），
//     无 used_skill 门（官方行文无"使用技能"字样）；被控回合该方桶不跑 = 不回（桶
//     语义边界，K415 1930 勘误同源）；状态计数 = ability_levels 六槽（含命中，不含
//     体力——common.h all_stats_up 用户口径）非 0 槽位数，回量 = 槽数×{1}。
//   - 1757 载荷"回合结束时"= 真挂 ROUND_END 消费者（免疫事件 watcher 只累计待附加
//     台账 + 每回合注册一次消费节点，节点读台账、清台账、deal_true_damage 给施加方）；
//     载体只有 4321 专属一行，{1} 恒 150，但台账仍按次写入（重复施放读最新行参）。
//   - 1758 watcher = j_attack_cond_window j5_arm 范式（EVENT_SKILL_USED + 窗口自检 +
//     executed 判据：ev.used_attribute=false=攻击技）；随机 N 技 PP 归零 = k398
//     pp_zero_random_slots guard 循环直系。
//   - 1759 "被处决精灵" PP 操控走 pp_reduce（原语按 side 生效于场上宠）→ 印记持有者
//     在背包时无可作用面（PP 无场下通道），仅当被处决精灵在场上才削减——按字面落地。
//   - 1762 门闭（自身体力不低于对手）→ 整个条件子句跳过（无掷骰无兜底）；门开 →
//     apply_anomaly_checked 申报制（J3 范式），未落地 → FMR pending（register_
//     next_rounds_effect + effect_move_right_priority_bonus，868/1123 moves 侧同款）。
//     100% 档原语短路不耗 rand（K176 口径）。
//   - 1720 护盾量 = shield_bank_.total()（SKILL_EFFECT 时点=本次命中前护盾余量），
//     附加量 = total×{0}/100 向下取整、0 不发。
//
// 存储键段（新增前已 grep 全库无撞；tools/check_storage_keys.py 门禁兜底）：
//   - 审计槽 10<id>000x 段（7690 同款）：1005190001 / 1017200001 / 1014760001 /
//     1017570001(剩余次数) / 1017570002(ROUND_END 实发) / 1017580001(圣剑累计) /
//     1017580002(本次归零技数) / 1017590001 / 1017600001 / 1017620001(落地) /
//     1017620002(兜底先制量)
//   - on_stage_storage[17570]：1757 次免剩余（与票同生命周期，2109 4602 同款）
//   - plugin_storage 994700xx 段：+side watcher 旗(0)/1757 待附加台账(10)/1757 消费
//     者回合戳(20)/1757 当前行参伤害(30)
//   - plugin_storage 4694xxx 段：圣剑 4694000+side；处决印记 4694010+side*6+slot

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <functional>
#include <vector>

namespace {

// ── 审计槽 ──────────────────────────────────────────────────────────────
constexpr int k519TickAudit     = 1005190001;
constexpr int k1720DmgAudit     = 1017200001;
constexpr int k1476DropAudit    = 1014760001;
constexpr int k1757RemainAudit  = 1017570001;
constexpr int k1757PayloadAudit = 1017570002;
constexpr int k1758SwordAudit   = 1017580001;
constexpr int k1758PpAudit      = 1017580002;
constexpr int k1759PpAudit      = 1017590001;
constexpr int k1760UpAudit      = 1017600001;
constexpr int k1762LandAudit    = 1017620001;
constexpr int k1762PrioAudit    = 1017620002;

// ── 功能槽（键段声明见文件头）───────────────────────────────────────────
constexpr int k1757RemainKey  = 17570;      // on_stage_storage：次免剩余
constexpr int k416WatchBase   = 99470000;   // +side：1757 watcher 只挂一次旗
constexpr int k416PendingBase = 99470010;   // +side：1757 本回合待附加真实伤害台账
constexpr int k416StampBase   = 99470020;   // +side：1757 消费者当回合已注册戳
constexpr int k416DmgBase     = 99470030;   // +side：1757 当前行参单次附加量
constexpr int k416SwordBase   = 4694000;    // +side：圣剑数（只增不减，消费端=魂印1897 未实现）
constexpr int k416MarkBase    = 4694010;    // +side*6+slot：处决印记层数（生产端=魂印1897 未实现）

// 处决印记层数（side,slot 的场上/背包统一读口；无印记 = 0）。
int mark_layers(const BattleContext* ctx, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6) {
        return 0;
    }
    const auto it = ctx->plugin_storage.find(k416MarkBase + side * 6 + slot);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

int sword_count(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return 0;
    }
    const auto it = ctx->plugin_storage.find(k416SwordBase + side);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

int on_stage_slot(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return -1;
    }
    const int slot = ctx->on_stage[side];
    return (slot >= 0 && slot < 6) ? slot : -1;
}

// ═══════════════ 519 状态回血窗 ═══════════════
// tick：AFTER_ACTION 双桶（57 形），回量 = 非零能力槽数（六槽含命中）× {1}。
EffectResult k416_519_tick(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 519, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 519, args,
                     "519 tick 布局=[owner,opp,每次回复量]");
    const int owner = args.int_args[0];
    const int heal = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && heal > 0, 519, args, "需要 owner∈{0,1} 且回复量>0");
    int states = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] != 0) {
            ++states;
        }
    }
    if (states <= 0) {
        return EffectResult::kOk;   // "每处于一种…时"——一种都没有不回
    }
    const int amount = states * heal;
    if (g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, amount);
    }
    ctx->plugin_storage[k519TickAudit] = amount;
    return EffectResult::kOk;
}

EffectResult effect_519_state_heal_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 519, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 519, args,
                     "519 布局=[owner,target,回合,每次回复量]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int heal = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && rounds > 0 && heal > 0, 519, args,
                     "需要 owner∈{0,1} 且 回合/回复量>0");
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 519, rounds, {owner, 1 - owner, heal},
                              &k416_519_tick);
    }
    return EffectResult::kOk;
}

// ═══════════════ 1720 护盾转真伤 ═══════════════
EffectResult effect_1720_shield_true_damage(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1720, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1720, args,
                     "1720 布局=[owner,target,百分比]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     1720, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(pct > 0, 1720, args, "需要 百分比>0");
    if (!g_core || !g_core->deal_true_damage) {
        return EffectResult::kOk;
    }
    const int shield = ctx->getPet(target).shield_bank_.total();
    if (shield <= 0) {
        return EffectResult::kOk;   // "对手存在护盾则"——门
    }
    const int amount = shield * pct / 100;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    g_core->deal_true_damage(ctx, target, amount, owner);
    ctx->plugin_storage[k1720DmgAudit] = amount;
    return EffectResult::kOk;
}

// ═══════════════ 1476 全属性弱化 + 低体翻倍 ═══════════════
EffectResult effect_1476_all_drop_lowhp_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1476, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1476, args,
                     "1476 布局=[owner,target,降幅,低体分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int drop = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     1476, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(drop > 0 && denom > 0, 1476, args, "需要 降幅/分母>0");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    // "自身体力低于1/{1}"——严格档（K377）；整型交叉相乘免截断。
    const bool low = ctx->getPet(owner).hp * denom < max_hp_of(ctx, owner);
    const int per = low ? drop * 2 : drop;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, target, i, per, owner);
    }
    ctx->plugin_storage[k1476DropAudit] = per;
    return EffectResult::kOk;
}

// ═══════════════ 1757 次免票 + 免疫成功回合末真伤 ═══════════════
// ROUND_END 消费者：读台账 → deal_true_damage(施加方) → 清台账。
EffectResult k416_1757_round_end(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1757, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1757, args,
                     "1757 round_end 布局=[owner,opp]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1757, args,
                     "需要 owner/opp ∈ {0,1}");
    const auto it = ctx->plugin_storage.find(k416PendingBase + owner);
    if (it == ctx->plugin_storage.end()) {
        return EffectResult::kOk;
    }
    const int amount = any_int_or(it->second, 0);
    ctx->plugin_storage.erase(it);
    if (amount <= 0 || !g_core || !g_core->deal_true_damage) {
        return EffectResult::kOk;
    }
    g_core->deal_true_damage(ctx, opp, amount, owner);
    ctx->plugin_storage[k1757PayloadAudit] = amount;
    return EffectResult::kOk;
}

EffectResult effect_1757_immune_true_payload(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1757, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1757, args,
                     "1757 布局=[owner,target,次数,附加真伤]");
    const int owner = args.int_args[0];
    const int counts = args.int_args[2];
    const int dmg = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && counts > 0 && dmg > 0, 1757, args,
                     "需要 owner∈{0,1} 且 次数/真伤>0");
    // 次数票（Modern 次免，ON_STAGE）+ 剩余记账（票同生命周期，2109 同款）。
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/1757 * 10 + 1,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/counts,
                        /*source_effect_id=*/1757);
    ctx->getPet(owner).on_stage_storage[k1757RemainKey] = counts;
    ctx->plugin_storage[k1757RemainAudit] = counts;
    ctx->plugin_storage[k416DmgBase + owner] = dmg;   // 重复施放读最新行参
    // 监听只挂一次（per side，plugin_storage 旗；由剩余记账驱动 no-op）。
    if (ctx->plugin_storage.count(k416WatchBase + owner)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k416WatchBase + owner] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_ANOMALY_IMMUNED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner) {
                return;
            }
            ElfPet& host = c->getPet(owner);
            const auto it = host.on_stage_storage.find(k1757RemainKey);
            const int remain = it == host.on_stage_storage.end() ? 0 : any_int_or(it->second, 0);
            if (remain <= 0) {
                return;   // 无票（未使用过 / 已扣完 / 已下场被清）
            }
            host.on_stage_storage[k1757RemainKey] = remain - 1;
            // 待附加台账累计（同回合多次免疫合并，回合末一次附加）。
            const auto dit = c->plugin_storage.find(k416DmgBase + owner);
            const int dmg = dit == c->plugin_storage.end() ? 0 : any_int_or(dit->second, 0);
            if (dmg <= 0) {
                return;
            }
            const auto pit = c->plugin_storage.find(k416PendingBase + owner);
            const int pending = pit == c->plugin_storage.end() ? 0 : any_int_or(pit->second, 0);
            c->plugin_storage[k416PendingBase + owner] = pending + dmg;
            // ROUND_END 消费者每回合只注册一次（回合戳）。
            const auto sit = c->plugin_storage.find(k416StampBase + owner);
            const int stamp = sit == c->plugin_storage.end() ? -1 : any_int_or(sit->second, -1);
            if (stamp != c->roundCount) {
                c->plugin_storage[k416StampBase + owner] = c->roundCount;
                register_round_effect(c, State::BATTLE_ROUND_END, owner, 1757, 1,
                                      {owner, 1 - owner}, &k416_1757_round_end);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ═══════════════ 1758/1759 条件窗（j5_arm 范式直系）═══════════════
// EVENT_SKILL_USED watcher：窗口 [start, start+rounds)（InRounds），触发动作由 want_attack 定。
void k416_arm_attr_window(BattleContext* ctx, int owner, int rounds, bool want_attack,
                          std::function<void(BattleContext*)> act) {
    if (!ctx || owner < 0 || owner > 1 || rounds <= 0 || !act) {
        return;
    }
    const int opp = 1 - owner;
    const int start =
        ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, opp, start, rounds, want_attack, act = std::move(act)](BattleContext* c,
                                                                        const BattleEvent& ev) {
            if (!c || ev.actor != opp || (ev.used_attribute != 0) == want_attack) {
                return;   // 只认对手的（攻/属）技能：want_attack=true→攻击技(非属性)、
                          // false→属性技（miss/被盔也算使用，EVENT_SKILL_USED 使用即发）
            }
            if (c->roundCount < start || c->roundCount >= start + rounds) {
                return;   // 窗口外
            }
            act(c);
        },
        WatcherScope::TEAM);
}

// 随机槽 PP 归零 ×count —— **不重复**取槽（Fisher-Yates，446 同式；官方"随机{1}个
// 技能"= {1} 个不同技能，k398 guard 循环的同槽重掷会以幂等成功顶掉配额，其载体
// 全是 count=1 不受影响，本宏 count=2 必须去重）。已 0 槽原语幂等成功照计。
void k416_pp_zero_random_slots(BattleContext* ctx, int target, int actor, int count) {
    if (!ctx || !g_core || !g_core->pp_zero_slot || count <= 0) {
        return;
    }
    int slots[5] = {0, 1, 2, 3, 4};
    const int n = static_cast<int>(ctx->getPet(target).skills.size());
    const int total = n < 5 ? n : 5;
    for (int i = 0; i < total && i < count; ++i) {
        const int j = i + (ctx->rng().next() % (total - i));
        const int tmp = slots[i];
        slots[i] = slots[j];
        slots[j] = tmp;
        (void)g_core->pp_zero_slot(ctx, target, slots[i], actor);
    }
}

// 1758：{0}回合内对手使用攻击技能 → 对手随机{1}技 PP 归零 + 己方场上附加{2}柄圣剑。
EffectResult effect_1758_attack_window_pp_sword(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1758, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1758, args,
                     "1758 布局=[owner,target,回合,归零技数,圣剑数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pp_count = args.int_args[3];
    const int swords = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1758,
                     args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(rounds > 0 && pp_count > 0 && swords > 0, 1758, args,
                     "需要 回合/技数/圣剑>0");
    k416_arm_attr_window(ctx, owner, rounds, /*want_attack=*/true,
                         [owner, opp, pp_count, swords](BattleContext* c) {
                             k416_pp_zero_random_slots(c, opp, owner, pp_count);
                             c->plugin_storage[k1758PpAudit] = pp_count;
                             const int now = sword_count(c, owner) + swords;
                             c->plugin_storage[k416SwordBase + owner] = now;
                             c->plugin_storage[k1758SwordAudit] = now;
                         });
    return EffectResult::kOk;
}

// 1759：{0}回合内对手使用属性技能 → 被处决精灵（场上持有印记者）全技能 PP-{1}。
EffectResult effect_1759_attr_window_marked_pp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1759, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1759, args,
                     "1759 布局=[owner,target,回合,PP削减]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pp_down = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1759,
                     args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(rounds > 0 && pp_down > 0, 1759, args, "需要 回合/削减>0");
    k416_arm_attr_window(ctx, owner, rounds, /*want_attack=*/false,
                         [owner, opp, pp_down](BattleContext* c) {
                             const int slot = on_stage_slot(c, opp);
                             if (slot < 0 || mark_layers(c, opp, slot) <= 0) {
                                 return;   // 场上对手未被处决 → 无被处决精灵（印记生产端=魂印1897 未实现）
                             }
                             if (g_core && g_core->pp_reduce) {
                                 g_core->pp_reduce(c, opp, pp_down);
                                 c->plugin_storage[k1759PpAudit] = pp_down;
                             }
                         });
    return EffectResult::kOk;
}

// ═══════════════ 1760 全属性提升 + 印记翻倍 ═══════════════
EffectResult effect_1760_all_up_mark_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1760, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1760, args,
                     "1760 布局=[owner,target,提升]");
    const int owner = args.int_args[0];
    const int up = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && up > 0, 1760, args, "需要 owner∈{0,1} 且提升>0");
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const int slot = on_stage_slot(ctx, opp);
    const bool marked = mark_layers(ctx, opp, slot) > 0;   // 对手当前精灵存在处决印记
    const int delta = marked ? up * 2 : up;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_change(ctx, owner, i, delta);   // 自升走 stat_change（不吃免弱）
    }
    ctx->plugin_storage[k1760UpAudit] = delta;
    return EffectResult::kOk;
}

// ═══════════════ 1762 低体门异常 + 未触发先制兜底 ═══════════════
// （本宏注册同时消灭 parser 寄生：此前"{0}%令对手{1}"主句被裸解析为无条件 100% 施加。）
EffectResult effect_1762_lowhp_anomaly_fallback_prio(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1762, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1762, args,
                     "1762 布局=[owner,target,概率,异常码,先制量]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int prio = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     1762, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(pct > 0 && prio > 0, 1762, args, "需要 概率/先制量>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1762, args, "异常码越界（≤43）");
    // 门闭（自身体力不低于对手）→ 整个条件子句跳过（无掷骰无兜底）。
    if (ctx->getPet(owner).hp >= ctx->getPet(target).hp) {
        return EffectResult::kOk;
    }
    bool landed = false;
    if (g_core && g_core->apply_anomaly_checked) {
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            ctx, target, anomaly, random_anomaly_duration(), owner, pct, ChanceSource::Skill);
        landed = r == ApplyAnomalyResult::SUCCESS
              || r == ApplyAnomalyResult::REPLACED_EXISTING
              || r == ApplyAnomalyResult::DURATION_EXTENDED;
    }
    ctx->plugin_storage[k1762LandAudit] = landed ? 1 : 0;
    if (!landed) {
        // "未触发则令自身下回合所有技能先制+{2}"——FMR pending（NextRounds 单回合窗）。
        register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1762, 1,
                                    {owner, target, prio}, &effect_move_right_priority_bonus);
        ctx->plugin_storage[k1762PrioAudit] = prio;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(519, effect_519_state_heal_window)
SKILL_EFFECT(1720, effect_1720_shield_true_damage)
SKILL_EFFECT(1476, effect_1476_all_drop_lowhp_double)
SKILL_EFFECT(1757, effect_1757_immune_true_payload)
SKILL_EFFECT(1758, effect_1758_attack_window_pp_sword)
SKILL_EFFECT(1759, effect_1759_attr_window_marked_pp)
SKILL_EFFECT(1760, effect_1760_all_up_mark_double)
SKILL_EFFECT(1762, effect_1762_lowhp_anomaly_fallback_prio)
