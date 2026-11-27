// K397 体力对照双分支族首批 —— effect 1139/897/947/1149（4 宏 / 4 行，
// 族 C 池 175 行/170 宏首刀；号段 7510-7519，worktree ../agents/k397，
// a01-knight 2026-11-02）
//
// 行清单（tools/soul_effects_dump.py 1139 897 947 1149，2026-11-02）：
//   1139 无妙 3551 row854 args[50,50,3,1]（kind[1,3,15]）：「回合开始时若自身体力高于
//     对手则造成的攻击伤害提升50%，体力低于对手则受到的攻击伤害减少50%；自身死亡后
//     消除对手能力提升状态且对手随机3个技能的PP值归零(BOSS无效)」
//   897 戴斯威尔 3283 row613 args[50,1]（kind[13,17]）：「回合开始时若自身体力低于对手
//     则当回合自身有50%的概率闪避对手攻击；回合开始时若自身体力高于对手则当回合对手
//     属性技能无效（BOSS无效）」
//   947 芊芊 3341 row664 args[100,1]（kind[1,7]）：「回合开始时，若自身体力高于对手则
//     当回合自身攻击造成的伤害翻倍；战斗阶段结束时，若自身体力低于对手则下回合自身
//     免疫所有异常状态（BOSS无效）」
//   1149 黑桃骑士 3559 row861 args[40,60,50,1]（kind[10,17]）：「回合开始时若自身体力
//     高于对手则40%的概率闪避对手攻击，自身体力低于对手时概率变为60%；使用攻击技能时
//     50%的概率令对手害怕（boss无效）」
//   本批 4 宏各只此 1 行（纯链 dump 实证）——行参驱动断言用「精确倍率/精确档位」钉死
//   args 消费（照抄任一行数值硬编码即红）。
//
// ⚠️ 行≠模板：effect_info 四 id 全为技能空间异文模板（1139 消回合+恢复 / 897 未击败
//    减伤 / 947 吸取 / 1149 反馈）——tips 权威（K377 1354/K391 1497 同款记档）。
//
// 行参解码（db_at 原序读 owned_int_args——方案 B 禁读 a.row）：
//   1139：db[0]=高体增伤%（50）、db[1]=低体受攻减伤%（50）、db[2]=死亡后对手随机N技
//         PP归零（3）、db[3]=旗——逐位与 tips 互锚自洽。
//   897 ：db[0]=低体闪避%（50）、db[1]=旗。
//   947 ：db[0]=高体增伤%（100 → (100+100)/100=×2 即"翻倍"）、db[1]=旗。
//   1149：db[0]=高体闪避%（40）、db[1]=低体闪避%（60）、db[2]=攻技害怕%（50）、db[3]=旗。
//
// 落法（通道=K377 1354 体力对照双分支直系 + K391 1497 变档；板行处方逐子句）：
//   1139  ROUND_START：hp>foe → ws.damage_add_pct[owner]+=db[0]（K377 LOW 补偿同款
//         ws 增伤通道，ws 每回合 reset → 每回合重写）；hp<foe →
//         ws.damage_reduce_add[owner][0]+=db[1]（hundun_diwang 2411 自减伤槽 0 直系，
//         默认 REDUCE_PCT/MITIGATE 安装面消费、只红伤）；等号 → 无分支（K377
//         严格档口径）。
//         死亡子句：FIRST/SECOND_MOVER_DEATH 双桶（K11 324 同款；⚠️ 324 行文带
//         "被对手击败"归因门、本行"自身死亡后"无归因限定 → 不做 last_damage_actor
//         门，任何死亡路径触发，记档）宿主已倒 → clear_stat_boosts(对手)（只清提升，
//         "能力提升状态"字面，不碰下降）+ pp_zero_slot 随机槽循环凑满 db[2] 个
//         （K230 1562 同款 guard 循环，原语 emit EVENT_PP_REDUCED）。
//   897   ROUND_START：hp<foe → ws.dodge_rate=db[0]/100（K377 LOW 支/K5 闪避通道，
//         赋值形）；hp>foe → seal_skill(对手, attribute=true, count=0 不限次数,
//         duration=1 每回合窗, 固定句柄刷新)——「属性技能无效」走 SEAL_ATTRIBUTE
//         非 hit_invalid（K311 323 口径：官方"无效"= ②被无效、触发技能无效补偿）；
//         条件持续语义 → ROUND_START 每回合重授（K311 每回合窗同款）。
//   947   ROUND_START：hp>foe → ws.damage_add_pct[owner]+=db[0]（"伤害翻倍"=+100%，
//         与"威力翻倍"（power_view ×2，K9 242）不同通道——字面是伤害倍率，走增伤
//         乘区 final×(100+100)/100，记档辨析）；ROUND_END：hp<foe（当次判，K190 口径
//         ——字面无"回合开始时"锚）→ grant_immunity(ANOMALY, coverage_all, mask=0
//         全异常, duration=2, 固定句柄刷新, soul_immunity=true, ON_STAGE)。
//         ⚠️ duration=2 而非 1：IMMUNE 是纯查询票（免 tick，绝对窗
//         window_expired=current-register>=rounds）——ROUND_END 落票 register_round
//         =本回合，duration=1 在下回合入口即判过期；=2 恰覆盖"下回合"一回合
//         （221 出战首回合 duration=1 的 enter 落票同理差一位）。条件持续：低体
//         期间每回合 ROUND_END 重授刷新（同句柄覆盖），低体解除后自然过期。
//   1149  ROUND_START：hp>foe → ws.dodge_rate=db[0]/100；hp<foe → =db[1]/100（双
//         闪避档位差 args 驱动）；等号 → 无分支。
//         害怕子句：enter(early) 武装 EVENT_SKILL_USED watcher（own+!used_attribute，
//         K164 116 同窗同门；miss 也算使用）→ apply_anomaly_checked(害怕6,
//         random_anomaly_duration, owner, db[2], SoulMark)——概率申报给原语内核单掷
//         （K164 纪律：禁本地 roll 双掷）。无体力对照门（"使用攻击技能时"无条件）。
//   boss 口径：四行 tips 尾注（BOSS无效）→ 全节点 boss_invalid=true；1139 自减伤走
//         ws 槽（由 ROUND_START 节点写，boss 局节点不跑即不写，天然无票残留）、死亡
//         子句/免疫/封属/害怕全在节点内 → 无需票内手动门（K391 管线票双保险此处不
//         适用，本单无 enter 早挂管线票）。
//
// 三查记录（a01-knight 2026-11-02 本 worktree 复核）：
//   ①内核 grep \b1139\b/\b897\b/\b947\b/\b1149\b 零命中（无魂印行为硬编码）；
//   ②effect_icon kind 已核（dump kind[1,3,15]/[13,17]/[1,7]/[10,17]）；effect_info
//     四 id 异文、tips 权威；
//   ③registry soul 基线 346 无四 id、soul_lib 全库无 SOUL_PROGRAM/SOUL_MARK 四 id、
//     无未合并分支（git branch 仅本 feat/k397）——真缺口。4399 旁证：行文本即
//     soul_seal 权威文（dump come 空，无活动变体）。
// 先例：k377_hpcrit_dodge_branch 1354（体力对照双分支 ROUND_START 定分支+ws 写入+
//     等号严格档，直系）、k391_order_fixed_seal_1497（变档+enter 臂）、k311 烧伤门
//     （SEAL_ATTRIBUTE 属性技无效每回合重授窗）、k343（seal_skill 固定句柄）、
//     k11 k_death_revenge_family 324（MOVER_DEATH 双桶+宿主已倒门）、k230 1562
//     （pp_zero_slot guard 循环）、k164（EVENT_SKILL_USED 害怕原语申报）、
//     hundun_diwang 2411（ws.damage_reduce_add[owner][0] 自减伤直写）、
//     k275/221（ANOMALY 全 mask 免疫票+绝对窗语义）。
// 场景回归：test/scenario/scenario_7510_hp_compare_dual_k397.cpp（号段 7510-7519）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark1139 = 1139;
constexpr int kMark897 = 897;
constexpr int kMark947 = 947;
constexpr int kMark1149 = 1149;

constexpr int k897SealHandle = 89701;    // 固定句柄：每回合重授=刷新非追加（K311 同款）
constexpr int k947ImmuneHandle = 94701;  // 固定句柄：低体期间重授=刷新非追加
constexpr int k1149Wired = 990397200;    // +owner：enter 臂防重旗
constexpr int kFearCode = static_cast<int>(AbnormalStatusId::Fear);   // 6

// ── 宿主行参数（db_at 原序读 owned_int_args，K364/K377 HostRow 同款）────────────
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

int flag_of(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

// 回合开始体力对照（严格档：等号无分支，K377 口径；双方存活门）。
// 返回 1=高体 / 2=低体 / 0=等号或不可判。
int hp_branch(BattleContext* ctx, int owner) {
    const ElfPet& self = ctx->getPet(owner);
    const ElfPet& foe = ctx->getPet(1 - owner);
    if (self.hp <= 0 || foe.hp <= 0) {
        return 0;
    }
    if (self.hp > foe.hp) {
        return 1;
    }
    if (self.hp < foe.hp) {
        return 2;
    }
    return 0;
}

// ════════════════════ 1139 无妙 ════════════════════

// ROUND_START：定分支 + 高体增伤 / 低体自减伤（ws 槽，默认安装面消费）。
EffectResult k397_1139_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMark1139)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1139);
    const int amp_pct = db_at(row, 0, 50);
    const int reduce_pct = db_at(row, 1, 50);
    const int branch = hp_branch(ctx, a.owner);
    if (branch == 1 && amp_pct > 0) {
        ctx->ws.damage_add_pct[a.owner] += static_cast<float>(amp_pct);
    } else if (branch == 2 && reduce_pct > 0) {
        ctx->ws.damage_reduce_add[a.owner][0] += reduce_pct;
    }
    return EffectResult::kOk;
}

// 死亡子句（FIRST/SECOND_MOVER_DEATH 双桶）：宿主已倒 → 清对手强化 + 对手随机
// db[2] 技 PP 归零。无归因门（行文"自身死亡后"无"被击败"限定，头注记档）。
EffectResult k397_1139_death(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMark1139)) {
        return EffectResult::kOk;
    }
    const int slot = ctx->on_stage[a.owner];
    if (ctx->seerRobot[a.owner].elfPets[slot].hp > 0) {
        return EffectResult::kOk;   // 宿主未倒（对手侧死亡时本侧桶空跑）
    }
    const int opp = 1 - a.owner;
    if (g_core && g_core->clear_stat_boosts) {
        g_core->clear_stat_boosts(ctx, opp);   // 只清"能力提升状态"（字面，不碰下降）
    }
    if (g_core && g_core->pp_zero_slot) {
        const HostRow row = host_row(ctx, a, kMark1139);
        const int count = db_at(row, 2, 3);
        // ⚠️ 本地去重：「随机 N 个技能」字面 = N 个**不同**技能；pp_zero_slot 对已零
        // 槽返回幂等 SUCCESS（battle_primitives.cpp"已是 0：幂等成功"），不去重的话
        // 重复命中同槽会把 done 虚增至 count 而实际归零不足 N 槽。
        bool picked[5] = {false, false, false, false, false};
        int done = 0;
        int guard = 0;
        while (done < count && guard < 100) {
            ++guard;
            const int sk_slot = choose_soul_range(*ctx, kMark1139, 0, a.owner, opp, 0, 4);
            if (picked[sk_slot]) {
                continue;   // 已试过（含无限 PP 槽/已零槽）不重复计
            }
            picked[sk_slot] = true;
            if (g_core->pp_zero_slot(ctx, opp, sk_slot, a.owner) ==
                PpReduceResult::SUCCESS) {
                ++done;
            }
        }
    }
    return EffectResult::kOk;
}

// ════════════════════ 897 戴斯威尔 ════════════════════

// ROUND_START：低体 → 闪避档；高体 → 对手属性技封属窗（每回合重授）。
EffectResult k397_897_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMark897)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark897);
    const int dodge_pct = db_at(row, 0, 50);
    const int branch = hp_branch(ctx, a.owner);
    if (branch == 2) {
        if (dodge_pct > 0) {
            ctx->ws.dodge_rate[a.owner] = dodge_pct / 100.0f;
        }
    } else if (branch == 1 && g_core && g_core->seal_skill) {
        // 属性技"无效"= SEAL_ATTRIBUTE（K311 口径）；count=0 不限次数、duration=1
        // 每回合窗（条件持续语义）、固定句柄重复授予=刷新（RuleCenter 覆盖键）。
        (void)g_core->seal_skill(ctx, /*source=*/a.owner, /*target=*/1 - a.owner,
                                 /*effect_id=*/k897SealHandle,
                                 /*attribute=*/true, /*attack=*/false,
                                 /*count=*/0, /*duration_rounds=*/1,
                                 /*penetrable=*/true, /*source_slot=*/-1,
                                 EffectScope::ON_STAGE, /*hit_invalid=*/false,
                                 /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

// ════════════════════ 947 芊芊 ════════════════════

// ROUND_START：高体 → 攻击伤害翻倍（增伤 +db[0]%）。
EffectResult k397_947_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMark947)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark947);
    const int amp_pct = db_at(row, 0, 100);
    if (hp_branch(ctx, a.owner) == 1 && amp_pct > 0) {
        ctx->ws.damage_add_pct[a.owner] += static_cast<float>(amp_pct);
    }
    return EffectResult::kOk;
}

// ROUND_END（战斗阶段结束时）：低体（当次判）→ 下回合全异常免疫（duration=2
// 绝对窗恰覆盖下回合一回合，头注记档；低体持续期间每回合重授刷新）。
EffectResult k397_947_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMark947)) {
        return EffectResult::kOk;
    }
    if (hp_branch(ctx, a.owner) != 2) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(a.owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/2,
                        /*source_id=*/k947ImmuneHandle, /*soul_immunity=*/true,
                        EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/kMark947);
    return EffectResult::kOk;
}

// ════════════════════ 1149 黑桃骑士 ════════════════════

// enter(early)：武装 EVENT_SKILL_USED watcher（攻击技 → db[2]% 害怕，无条件门）。
EffectResult k397_1149_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int wired = k1149Wired + owner;
    if (ctx->plugin_storage.count(wired)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(wired, 1);
    const HostRow row = host_row(ctx, a, kMark1149);
    const int fear_pct = db_at(row, 2, 50);
    if (fear_pct <= 0) {
        return EffectResult::kOk;   // 无概率行不武装（K164 纪律）
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, fear_pct](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // own + 攻击技（K164 116 同门；miss 也算使用）
            }
            if (!onstage_has_soul_mark(c, owner, kMark1149)) {
                return;
            }
            (void)g_core->apply_anomaly_checked(c, 1 - owner, kFearCode,
                                                random_anomaly_duration(), owner,
                                                fear_pct, ChanceSource::SoulMark);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ROUND_START：高体 40% / 低体 60% 闪避档（双档 args 驱动）。
EffectResult k397_1149_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMark1149)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1149);
    const int high_pct = db_at(row, 0, 40);
    const int low_pct = db_at(row, 1, 60);
    const int branch = hp_branch(ctx, a.owner);
    if (branch == 1 && high_pct > 0) {
        ctx->ws.dodge_rate[a.owner] = high_pct / 100.0f;
    } else if (branch == 2 && low_pct > 0) {
        ctx->ws.dodge_rate[a.owner] = low_pct / 100.0f;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +4（1139/897/947/1149 入账，
//    只增不改）────────────────────────────────────────────────────────────────────
SOUL_PROGRAM(kMark1139, {SOUL_NODE(State::BATTLE_ROUND_START, &k397_1139_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_FIRST_MOVER_DEATH, &k397_1139_death,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_SECOND_MOVER_DEATH, &k397_1139_death,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});
SOUL_PROGRAM(kMark897, {SOUL_NODE(State::BATTLE_ROUND_START, &k397_897_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});
SOUL_PROGRAM(kMark947, {SOUL_NODE(State::BATTLE_ROUND_START, &k397_947_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                        SOUL_NODE(State::BATTLE_ROUND_END, &k397_947_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});
SOUL_PROGRAM(kMark1149, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k397_1149_enter,
                               /*once=*/false, /*early=*/true, SoulScope::STAGE,
                               /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k397_1149_round_start,
                               /*once=*/false, /*early=*/false, SoulScope::STAGE,
                               /*boss_invalid=*/true)});
