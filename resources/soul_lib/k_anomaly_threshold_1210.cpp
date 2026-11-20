// K366 概率异常+低伤免疫阈值族 —— effect 1210（1 宏 3 行）
//
// 行清单（tools/soul_effects_dump.py 1210，2026-10-24）：
//   1210 row924（3632 风暴女神·斯蒂亚）args[40,6,300,1,1,40,1]：
//     「自身所有技能有40%的概率令对手害怕；免疫低于300的攻击伤害，若对手造成的
//      攻击伤害高于300则使对手全属性-1且下1次自身受到的伤害减少40%（boss无效）」
//   1210 row1476（3632 X之战意伙伴效果行）args[50,6,300,1,1,50,1]：同构互锚 50/50 档。
//   1210 row1812（4607 小灯龙）args[100,2,0,1,0,0,1]：
//     「自身的所有技能额外有100%的概率令对手烧伤；对手使用攻击技能额外有100%
//      的概率全属性-1（boss无效）」——阈值/减免段关（db[2]=0、db[4]=db[5]=0）。
//
// 行参解码（三行互锚自证；⚠️ 行≠模板：effect_info 1210 = 技能空间同号模板
//   「{0}%使对手{1}，自身体力低于对手时概率翻倍」args_num=2，异文，tips 权威）：
//   db[0]=异常概率%（40/50/100）      db[1]=异常码（6害怕/2烧伤，官方异常码序）
//   db[2]=伤害阈值（免疫<阈值；>阈值触发反馈；0=段关）
//   db[3]=全属性-1反馈旗（1=开）      db[4]=下次受伤减免旗（1=开，row1812=0）
//   db[5]=减免百分比（40/50/0）       db[6]=boss无效旗（PvP 引擎按 S1 §七忽略）
//
// 落法（工单处方逐子句落地；管线相位序 GUARD_DETECT→…→REDUCE_PCT→…→BLOCK→DETECT）：
//   ①「所有技能 P% 异常」= EVENT_SKILL_USED watcher（E13"使用即触发"锚，K2/k_attack_prob
//      直系去掉攻击门——所有技能含属性技/miss）→ apply_anomaly_checked 概率申报通道
//      （内核单掷、亮节管辖；100% 档短路不耗 rand）。审计槽 990366800+owner*2=申报概率、
//      990366801+owner*2=ApplyAnomalyResult（F9 782 审计槽技巧，场景无随机精确计数）。
//   ②「免疫 < db[2]」= BLOCK 相位归零门（⚠️ 工单处方原文"DETECT 阈值门"，动工时按子句
//      直系改挂 BLOCK：与 608"免疫低于300的攻击伤害"同款子句同相位（k180 直系），
//      DETECT 相位在其后、归零等价但类别语义更纯（挡伤失效正确废免伤票）；归零后
//      DETECT 回调 final<=0 早退 → 减免旗不被免伤消耗，"下1次**受到的**伤害"口径更忠实）。
//   ③「> db[2]」= DETECT 相位分叉（k189 1016 直系）：对手全属性-1（六槽 stat_drop，
//      查免弱，k_open_combo_defdrop 263 同原语）+ 置下次受伤减免旗。
//   ③'「下1次受伤 -db[5]%」= REDUCE_PCT/MITIGATE 旗消费（k_hit_dodge_aura 725 直系：
//      final*(100-pct)/100，一次性，减伤不可被挡伤失效无视）。
//   ③'' row1812 降攻门（db[2]==0）：BATTLE_ROUND_END 节点 + §四点五 executed_* 私副本
//      （对手本回合成功执行过攻击技 → 六槽-1；属性技/NONE/被控不触发）。
//   行参读取：db_at 读宿主 owned_int_args 原序（方案 B，K257 起夜跑口径；禁 a.row）。
//
// 三查记录（2026-10-24，worktree ../agents/k366）：
//   ①内核 grep 1210 无魂印行为硬编码；②effect_icon kind=[6,10,12]/[11]、tips 为权威文
//   （dump 原文已核，见上）；③registry soul 集合无 1210、soul_lib 全库 grep 无
//   SOUL_PROGRAM(1210)——真缺口。⚠️ 技能空间 moves_lib 已有 1210 注册
//   （j_prob_anomaly_low_hp，官方模板语义）+ 其审计槽 990121000 段——两个注册空间
//   不相干（K361 819 同款撞号甄别），本文件键段换 990366000（grep 无撞）。
// 先例：k_attack_prob（E13 锚+apply_anomaly_checked+审计 dedup）、k180 608（免疫低伤
//   BLOCK 归零形）、k189 1016（DETECT 阈值分叉+旗标形）、k_hit_dodge_aura 725
//   （REDUCE_PCT/MITIGATE 减免）、k_open_combo_defdrop 263（stat_drop 六槽降）、
//   k306 765（ROUND_END+executed_* 私副本）。
// 场景回归：test/scenario/scenario_7110_anomaly_threshold_1210.cpp（号段 7110-7119）。

#include "common.h"

#include <entities/skills.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1210;
constexpr int kWiredBase = 990366000;    // 接线去重：+宏*10+owner（grep 无撞）
constexpr int kReduceFlagBase = 990366500;  // 下次受伤减免待消费旗：+owner
constexpr int kAuditPctBase = 990366800;    // 审计槽：+owner*2=申报概率；+1=结果

// ── 宿主行参数（方案 B：db_at 读 owned_int_args 原序，与真实对局一致）────────
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

bool watcher_wired(BattleContext* ctx, int owner) {
    const int key = kWiredBase + kMarkId * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── §四点五 私副本（soul 侧不链 moves_lib/common.h，k306 同款就地复刻）──────
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

// 对手全属性-1（六槽逐个 stat_drop，查免弱；263 同原语。
// ⚠️ stat_drop 的 amount 是**下降低度**、必须为正（amount<=0 = INVALID_PARAM 静默无效））
void drop_all_six(BattleContext* ctx, int target, int actor) {
    if (!ctx || !g_core || !g_core->stat_drop) {
        return;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, target, i, /*amount=*/1, actor);
    }
}

// ── arm（enter/early，每 epoch 一次，watcher_wired 去重）：四路接线 ──────────
EffectResult k1210_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || watcher_wired(ctx, owner)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int anomaly_pct = db_at(row, 0, -1);
    const int anomaly_code = db_at(row, 1, -1);
    const int threshold = db_at(row, 2, 0);
    const int drop_on = db_at(row, 3, 0);
    const int reduce_on = db_at(row, 4, 0);
    const int reduce_pct = db_at(row, 5, 0);
    const int opp = 1 - owner;

    // ① 所有技能 P% 异常（E13 锚，不限攻击技；概率申报通道单掷）
    if (anomaly_pct > 0 && anomaly_code >= 0) {
        ctx->event_center_.register_watcher(
            EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, anomaly_pct, anomaly_code](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor != owner) {
                    return;   // 自身所有技能（miss/属性技也算"使用"——E13 锚语义）
                }
                if (!onstage_has_soul_mark(c, owner, kMarkId) || !g_core
                    || !g_core->apply_anomaly_checked) {
                    return;
                }
                const ApplyAnomalyResult res = g_core->apply_anomaly_checked(
                    c, 1 - owner, anomaly_code, random_anomaly_duration(), owner,
                    anomaly_pct, ChanceSource::SoulMark);
                // 审计槽（F9 782 技巧）：场景对概率梯度做无随机精确计数
                c->plugin_storage[kAuditPctBase + owner * 2] = anomaly_pct;
                c->plugin_storage[kAuditPctBase + owner * 2 + 1] = static_cast<int>(res);
            },
            WatcherScope::TEAM);
    }

    // ② 免疫 < 阈值：BLOCK 相位归零（608 同款子句同相位；全字段清零）
    if (threshold > 0) {
        ctx->register_default_damage_effect(
            DamagePhase::BLOCK, owner, DamageEffectCategory::BLOCK,
            [owner, threshold](BattleContext* c, int bucket_owner) {
                DamageSnapshot& d = c->resolvedDamage;
                if (bucket_owner != owner || d.defenderId != bucket_owner || !d.isRed
                    || d.final <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                    return;
                }
                if (d.final < threshold) {
                    d.base = 0;
                    d.afterAdd = 0;
                    d.afterMul = 0;
                    d.final = 0;
                    d.addPct = 0;
                    d.mulCoef = 0.0;
                }
            });
    }

    // ③ 高伤分叉：DETECT 相位（k189 直系）——>阈值 → 六槽-1 + 置减免旗
    if (threshold > 0 && (drop_on > 0 || (reduce_on > 0 && reduce_pct > 0))) {
        ctx->register_default_damage_effect(
            DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
            [owner, threshold, drop_on, reduce_on, reduce_pct](BattleContext* c,
                                                               int bucket_owner) {
                DamageSnapshot& d = c->resolvedDamage;
                if (bucket_owner != owner || d.defenderId != bucket_owner || !d.isRed
                    || d.final <= 0) {
                    return;
                }
                if (d.attackerId != 1 - owner) {
                    return;   // 只随"对手造成的攻击伤害"分叉
                }
                if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                    return;
                }
                if (d.final > threshold) {
                    if (drop_on > 0) {
                        drop_all_six(c, 1 - owner, owner);
                    }
                    if (reduce_on > 0 && reduce_pct > 0) {
                        c->plugin_storage[kReduceFlagBase + owner] = 1;
                    }
                }
            });
    }

    // ③' 下次受伤 -pct%：REDUCE_PCT/MITIGATE 旗消费（725 直系；一次性；只红伤）
    if (reduce_on > 0 && reduce_pct > 0) {
        ctx->register_default_damage_effect(
            DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
            [owner, reduce_pct](BattleContext* c, int bucket_owner) {
                DamageSnapshot& d = c->resolvedDamage;
                if (bucket_owner != owner || d.defenderId != bucket_owner || !d.isRed
                    || d.final <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                    return;
                }
                auto it = c->plugin_storage.find(kReduceFlagBase + owner);
                if (it == c->plugin_storage.end() || any_int_or(it->second, 0) != 1) {
                    return;
                }
                c->plugin_storage.erase(it);   // "下1次"：一次性消费
                d.final = d.final * (100 - reduce_pct) / 100;
            });
    }
    return EffectResult::kOk;
}

// ── row1812 降攻门（db[2]==0）：ROUND_END + executed_* 攻击技判据 ────────────
EffectResult k1210_round_end_drop(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    if (db_at(row, 2, 0) > 0) {
        return EffectResult::kOk;   // 阈值行走 DETECT 分叉，不走本门
    }
    if (db_at(row, 3, 0) <= 0) {
        return EffectResult::kOk;   // 反馈段关
    }
    const int opp = 1 - owner;
    if (!executed_skill_this_round(ctx, opp)) {
        return EffectResult::kOk;   // 对手本回合没成功用技能（NONE/被控不算）
    }
    if (!executed_skill_is_attack(ctx, opp)) {
        return EffectResult::kOk;   // 属性技不触发（tips"使用攻击技能"）
    }
    drop_all_six(ctx, opp, owner);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1210 入账，只增不改）──────
SOUL_PROGRAM(kMarkId, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k1210_arm,
                             /*once=*/false, /*early=*/true),
                       SOUL_NODE(State::BATTLE_ROUND_END, &k1210_round_end_drop,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/false)});
