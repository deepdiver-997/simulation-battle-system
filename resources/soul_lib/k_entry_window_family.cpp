// K401 登场/首次 N 次窗口族 —— effect 720/888/2087（3 宏 / 6 行，2026-11-02 编排挖掘）
//（看板 K401「登场/首次 N 次窗口族」，a01-knight5，worktree ~/.agent-workspaces/.../agents/k401；
//  号段 7570-7579；通道=K394 ⑤ shield_bank_ add + K8 203 roster/epoch arm 先例 +
//  K163 221/270 grant_immunity 直系 + K190 免疫弹控配对 + K1/K8 REDUCE_PCT/REDUCE_EXTRA
//  + K12 AFTER_DEFEATED 击败归因 + K2 358 SKILL_USED 窗 + K400 审计槽惯例）
//
// 行清单（tools/soul_effects_dump.py 720 888 2087，2026-11-02）：
//   720 乔特鲁德 3036 两行（**args 计数互异** → 行判别锚，同 K400 行判别 B）：
//     row398 本体 args[100,100,600,10,50] kind[1,2]：
//       ①"登场时获得一个可抵御100点攻击伤害的护盾,背包内每存活一只精灵(不包括自己),
//         则护盾值增加100点,最高600点" ②"登场时背包内每阵亡一只精灵,造成的伤害提升10%,
//         最高50%"
//     row399 进化行（乔特鲁德龙族羁绊魂印进化）args[100,100,600,10,50,20,20] kind[1,2,3]：
//       ①②同文 + ③"最大体力提升20%,受到的伤害减少20%"
//     → db[0]=盾基座(100)、db[1]=每存活加值(100)、db[2]=盾上限(600)、db[3]=每阵亡增伤(10)、
//       db[4]=增伤上限(50)、db[5]=最大体力提升%(20，仅进化行)、db[6]=受伤减%(20，仅进化行)。
//   888 灵尊·斯凯尔 3276 两行（同文，**db[3] 档位差** 50/60 常规行≠宏）：
//     row603 args[3,3,2,50,1] kind[7,8,10] / row608 进化行 args[3,3,2,60,1] kind[7,8,10]：
//       ①"首次登场免疫并反弹下3次受到的异常状态" ②"首次登场3回合内免疫能力下降状态"
//       ③"自身攻击技能使对手随机进入烧伤、冻伤、中毒中的一种异常状态，未触发则消除对手
//         回合类效果且下2回合自身攻击有50%/60%的概率使对手随机进入害怕、麻痹、疲惫中的一
//         种异常状态（BOSS无效）"（4399✓3276 原文一致）
//     → db[0]=免疫反弹次数(3)、db[1]=免降窗回合(3)、db[2]=补偿窗回合(2)、db[3]=补偿概率
//       (50/60)、db[4]=1 旗不消费（K1 纪律；boss 尾注按行文归属仅③，见下）。
//   2087 秘尔威缇 4486 两行（**db[2] 档位差** 3/1）：
//     row1698 args[1,30,3,1] kind[7,3,5]：「每次登场时触发岳灵涤荡：1回合内免疫所受到的
//       异常状态且受到的攻击伤害、固定伤害、百分比伤害减少30%；击败对手后恢复自身最大
//       体力的1/3并再次触发岳灵涤荡（boss无效）」
//     row1817 伙伴行（山祇之息）args[1,30,1,1] kind[5,3]：同文，回体=「恢复自身全部体力」。
//     → db[0]=涤荡窗回合(1)、db[1]=三通道减伤%(30)、db[2]=击败回体分母(3=1/3 / 1=全部，
//       "1/3"与"全部"互锚）、db[3]=1 旗不消费（boss 尾注按行文归属仅②）。
//
// ⚠️ 口径记档（majority reading，均按先例）：
//   【720①】护盾走 ShieldBank::kSoulShieldPriority（K394 ⑤/K173/K374 同款），source_id=720；
//     同魂印多次登场按 K394 约定 refresh_source（不并排叠条）；存活数 = 扫 6 槽
//     （跳 is_vanished=空槽/消逝、跳自身槽，hp>0 计）——与 count_dead(ROSTER) 同口径
//     （阵亡 hp<=0 不计存活）。盾只吃红伤（ShieldBank 语义，"可抵御…攻击伤害"字面）。
//   【720②】"造成的伤害提升"= 红伤 AMP 零状态票实时读 storage（K8 203/K12 319 同款
//     majority；行文无"所有伤害"限定词不扩粉伤）；登场时点 count_dead(ROSTER) 求值后
//     存账（"登场时"=一次性快照，重登场重算覆盖）。
//   【720③】"最大体力提升20%"= SOUL_PRE_BATTLE(STAT_MODIFY) 面板补丁（K8 247/137 +
//     suit.cpp ④范式：base*(100+pct)/100 向下取整、base/properties 同步加——hp 是
//     properties[HP] 引用自动跟随，开局满血=提升后上限）；"受到的伤害减少20%"= 红伤
//     REDUCE_PCT（K8 203 受伤减同款仅红伤）。
//   【888①】"免疫并反弹下3次"= 次数型 ANOMALY 免疫票（grant_immunity counts=db[0]、
//     duration=0=不随回合过期，window_expired 判据实证；consume_immune 扣到 0 注销）+
//     反弹走 EVENT_ANOMALY_IMMUNED watcher（battle_primitives [3]/[5] 免疫命中事件，
//     "「每次免疫成功…」族检测点"钦定注释点名 2059 同构）——次数型回弹票 grant_reflect
//     不支持（rule_center.h "counts 可后续扩"），故插件侧自记反弹余量计数器（初值=db[0]，
//     每次免疫成功反弹一次并递减）。反弹施加 = apply_anomaly(Modern, actor=自身)（内核
//     内置弹控同形：reflect_depth=1 由原语自理、chance 不重掷）；事件不带原时长 → 按
//     random_anomaly_duration()=2 口径（记档）。票 scope=ON_STAGE：切走即失效（K163 221
//     免疫窗同形），重登场**不**重授（"首次"严格一次）、反弹计数器随之清账（K7 显式清账
//     教训——防"票已死计数还在"的假反弹）。
//   【888②】STAT_DROP 免疫窗 rounds=db[1]（K163 270 直系），同"首次"门。
//   【888③】主支 = 每次自身攻击技（EVENT_SKILL_USED own+!used_attribute，E13 使用即发
//     miss 也算，K396 ③ 定例）→ 池 {烧伤2,冻伤5,中毒1} sbs_rand()%3 等概率挑一 →
//     apply_anomaly_checked(pct=100 短路不耗 rand, K2 口径)；未落地（!anomaly_landed，
//     K238/K400 落地集）→ break_round_effects(对手)（3090 回合型载体可断面）+ 武装补偿窗
//     [roundCount+1, roundCount+db[2]]（"下2回合"绝对窗，戳记账本）。窗内攻击 → 池
//     {害怕6,麻痹0,疲惫7} 挑一 → apply_anomaly_checked(db[3])（K662 审计槽掷前写）。
//     窗与主支并存不互斥（行文"且"连接，majority 直读；记档）。
//   【888 boss 尾注】"（BOSS无效）"紧贴③句尾 → 仅③ watcher 回调手动门 is_boss_challenge
//     （hamo_3809_ao：watcher 不随节点 boss_invalid 走）；①②无尾注照常（boss 局也免疫）。
//   【2087①】"每次登场时触发岳灵涤荡"：enter 节点每次登场重授（免异常票 rounds=db[0]
//     同句柄刷新 + 涤荡戳=roundCount+1，K394 戳位+1 偏移防 0 撞车）；三通道减伤=管线
//     零状态票（攻伤=REDUCE_PCT、固伤=粉管 REDUCE_EXTRA·FIXED、百分比=·PERCENT
//     ——PinkDamageTier 二值枚举，PERCENT_VALUE 归 PERCENT 档，pink_damage_pipeline.h
//     注释"含 PERCENT_VALUE"）回调实时查涤荡戳活跃；票 ON_STAGE 随切换作废、epoch 守卫
//     每次登场重挂（K1 arm_percent_mitigate 同款）。真伤/属性直伤行文未列 → 不减（记档）。
//   【2087②】"击败对手后恢复…"= BATTLE_AFTER_DEFEATED 节点 + find_defeated_foe_slot
//     归因门（K12 319 直系；AFTER_DEFEATING_OPPONENT 跳过态记档延续）→ heal_amount
//     (max/db[2])（db[2]=3→1/3、=1→全部；吃封回血）+ 再次触发涤荡（重授免异常票+刷戳）。
//     节点 boss_invalid=true（尾注最近邻归属，K394 口径）；自身体力<=0 不回（记档：
//     同归于尽时无"恢复自身"主体）。
//     ⚠️ 归因口径记档：last_damage_actor 只由红伤落血/force_hp_to_zero 写（run_pink_damage
//     落血点不写，battle_primitives.cpp:1094 注释明示）→ 粉伤击杀的归因读默认值/旧值，
//     "击败"按红伤/秒杀口径（find_defeated_foe_slot 先例同限；majority reading）。
//   【概率单位】db[3]=百分比（50/60 与 tips "50%/60% 的概率"互锚，两行档位差即此位）。
//
// ⚠️ 三查记录（a01-knight5 2026-11-02，worktree 内复核）：
//   ①内核 grep -w 720/888/2087：唯一命中 src/entities/skills.cpp:282 case 2087 =
//     **技能空间**"空元之诗·镀"（INHERENT 固有效果分类，与魂印 2087 分属两注册空间，
//     1004 双空间常识）——魂印行为零硬编码；
//   ②effect_info 720/888/2087 均为技能空间异文模板（720 先手闪避/888 体低削最大HP/
//     2087 空元之诗），effect_icon.tips 为魂印权威（SOP §六.1）；
//   ③registry soul=366 无三 id；全 feat 分支 git grep 无 SOUL_PROGRAM(720/888/2087)；
//     场景号段 7570-7579 全分支无撞；存储键段 997200000/998880000/992087000 无撞
//     （tools/check_storage_keys.py 全绿）；免疫票句柄 8881/8882 无撞（grep 全库）。
// 注册：SOUL_PROGRAM ×3（720/888/2087 入账，只增不改）+ SOUL_PRE_BATTLE ×1（720
//   面板补丁；K8 247 同注册面，不占 SOUL_PROGRAM 数）。soul 366→369。
// 场景回归：test/scenario/scenario_7570_entry_shield_720.cpp /
//   scenario_7573_skylord_888.cpp / scenario_7576_yuelin_2087.cpp（号段 7570-7579）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

// ── 存储键段（+owner；tools/check_storage_keys.py 已核无撞）──────────────────
constexpr int k720AmpDead = 997200000;    // ② 阵亡增伤 %（enter 重算覆盖）
constexpr int k888First = 998880000;      // 首次登场已触发旗
constexpr int k888ReflLeft = 998880010;   // ① 反弹余量计数
constexpr int k888WireSkill = 998880020;  // ③ SKILL_USED watcher 已挂旗
constexpr int k888WinFrom = 998880030;    // 补偿窗起（roundCount，0=未武装）
constexpr int k888WinTo = 998880040;      // 补偿窗止（inclusive）
constexpr int k888AuditMain = 998880050;  // 主支申报概率（审计槽，恒 100）
constexpr int k888AuditComp = 998880060;  // 补偿窗申报概率（审计槽）
constexpr int k888MainLand = 998880070;   // 主支落地计数（统计侧证）
constexpr int k888CompLand = 998880080;   // 补偿落地计数（统计侧证）
constexpr int k888BreakCnt = 998880090;   // 未触发→断回合计数（侧证）

constexpr int k2087Purify = 992087000;     // 涤荡戳（=roundCount+1 活跃，0=无）
constexpr int k2087PurifyCnt = 992087010;  // 涤荡触发计数（侧证）
constexpr int k2087HealCnt = 992087020;    // 击败回体次数（侧证）

// 免疫票句柄（roster_field 稳定句柄形 mark*10+n：同句柄重授=刷新非追加）。
constexpr int k888ImmuneAnomaly = 888 * 10 + 1;  // ① 次数型异常免疫+反弹
constexpr int k888ImmuneDrop = 888 * 10 + 2;     // ② 免降窗
constexpr int k2087ImmuneAnomaly = 2087 * 10 + 1;  // 涤荡免异常窗

// 720 护盾在 ShieldBank 的 source_id（同魂印唯一盾位，K394 约定）。
constexpr int k720ShieldSource = 720;

// ── 宿主行参数（方案 B：db_at 读 owned_int_args **原序**，K190/K400 定例）─────

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

// epoch 守卫（k_guard_family 私副本：管线票每次登场重挂、同 epoch 防重）。
struct EpochGuard {
    int armed_epoch = -1;
};

bool already_armed_this_epoch(BattleContext* ctx, int owner, int storage_key) {
    ElfPet& pet = ctx->getPet(owner);
    if (!pet.soulmark_storage.count(storage_key)) {
        pet.soulmark_storage[storage_key] = EpochGuard{};
    }
    EpochGuard* guard = std::any_cast<EpochGuard>(&pet.soulmark_storage[storage_key]);
    if (!guard) {
        return true;   // 键位被异类型占用 → 视为已武装（共享键空间，不抛）
    }
    if (guard->armed_epoch == ctx->pipeline_valid_id[owner]) {
        return true;
    }
    guard->armed_epoch = ctx->pipeline_valid_id[owner];
    return false;
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

// ═════════════════════════════════════════════════════════════════════════════
// effect 720 乔特鲁德：登场护盾（存活数）+ 阵亡增伤（+进化行 maxHP/受伤减）
// ═════════════════════════════════════════════════════════════════════════════

// 背包存活数（不含自身）：扫 6 槽，跳空槽/消逝（is_vanished）与自身槽，hp>0 计。
// 与 count_dead(ROSTER) 口径互补（阵亡 hp<=0 不计存活、已消逝不占背包）。
int alive_backpack_excluding_self(BattleContext* ctx, int owner) {
    const int self_slot = ctx->on_stage[owner];
    int n = 0;
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == self_slot || ctx->is_vanished(owner, slot)) {
            continue;
        }
        if (ctx->seerRobot[owner].elfPets[slot].hp > 0) {
            ++n;
        }
    }
    return n;
}

EffectResult k720_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const HostRow row = host_row(ctx, a, 720);
    if (!row.valid) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;   // 已倒地不授（记档）
    }

    // ① 登场护盾 = min(db[2], db[0] + db[1]×背包存活数(不含自身))。
    const int base = db_at(row, 0, 100);
    const int per_alive = db_at(row, 1, 100);
    const int cap = db_at(row, 2, 600);
    if (base > 0) {
        const int alive = alive_backpack_excluding_self(ctx, owner);
        int shield = base + per_alive * alive;
        if (cap > 0 && shield > cap) {
            shield = cap;
        }
        if (self.shield_bank_.quantity_of(k720ShieldSource) > 0) {
            self.shield_bank_.refresh_source(k720ShieldSource, shield);   // 同盾位刷新
        } else {
            self.shield_bank_.add(ShieldBank::kSoulShieldPriority, shield,
                                  k720ShieldSource, /*duration=*/0);
        }
    }

    // ② 登场时阵亡增伤 = min(db[4], db[3]×count_dead(ROSTER))，存账供 AMP 票实时读。
    const int per_dead = db_at(row, 3, 10);
    const int amp_cap = db_at(row, 4, 50);
    if (per_dead > 0 && g_core->count_dead) {
        const int dead = g_core->count_dead(ctx, owner, DeadScope::ROSTER);
        int amp = per_dead * dead;
        if (amp_cap > 0 && amp > amp_cap) {
            amp = amp_cap;
        }
        write_int(ctx, k720AmpDead + owner, amp);
    }

    // ② AMP 票（红伤攻侧零状态票实时读账）+ ③ REDUCE_PCT 票：epoch 守卫每次登场重挂。
    if (!already_armed_this_epoch(ctx, owner, 720 * 100 + 9)) {
        ctx->register_damage_effect(
            DamagePhase::AMP, owner, DamageEffectCategory::AMP,
            [owner](BattleContext* c, int bucket_owner) {
                DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != bucket_owner || !d.isRed || d.final <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(c, bucket_owner, 720)) {
                    return;
                }
                const int amp = read_int(c, k720AmpDead + bucket_owner);
                if (amp <= 0) {
                    return;
                }
                d.addPct += amp;
                d.final = d.final * (100 + amp) / 100;
            });
        const int reduce_pct = db_at(row, 6, 0);   // ③ 仅进化行有 db[6]
        if (reduce_pct > 0) {
            ctx->register_damage_effect(
                DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
                [reduce_pct](BattleContext* c, int bucket_owner) {
                    DamageSnapshot& d = c->resolvedDamage;
                    if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                        return;
                    }
                    if (!onstage_has_soul_mark(c, bucket_owner, 720)) {
                        return;
                    }
                    d.final = d.final * (100 - reduce_pct) / 100;
                });
        }
    }
    return EffectResult::kOk;
}

// ③ 进化行"最大体力提升20%"：战前面板补丁（K8 247/suit ④范式——先点数后百分比、
// 提升向下取整、base/properties 同步加，hp 引用自动跟随=开局满血）。
void k720_pre_battle(BattleContext* ctx, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot > 5) {
        return;
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    const std::vector<int>& db = pet.soulMark.args.owned_int_args;
    if (db.size() < 7 || db[5] <= 0) {
        return;   // row398（5 参）无此子句
    }
    const int pct = db[5];
    const auto idx = NumericalPropertyIndex::HP;
    const int after = pet.numericalBase[idx] * (100 + pct) / 100;   // 提升向下取整
    const int gained = after - pet.numericalBase[idx];
    if (gained <= 0) {
        return;
    }
    pet.numericalBase[idx] += gained;
    pet.numericalProperties[idx] += gained;   // hp 槽引用自动跟随（手动再加会翻倍）
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 888 灵尊·斯凯尔：首次登场免疫反弹异常（3 次型）+ 免降窗 + 攻击异常/补偿链
// ═════════════════════════════════════════════════════════════════════════════

constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);        // 2
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);  // 5
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);    // 1
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);        // 6
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);  // 7

// ① 反弹：免疫成功（内核 [3]/[5] 命中即发）→ 余量>0 → 同异常弹回施加方。
// 注意只在挂 888 的宿主在场时生效（onstage 守卫）；弹回施加走 Modern 通道
// apply_anomaly（内核内置弹控同形：depth=1 防乒乓由原语自理）。
void arm_888_reflect_watcher(BattleContext* ctx, int owner) {
    if (!ctx || !g_core || !g_core->apply_anomaly) {
        return;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_ANOMALY_IMMUNED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly) {
                return;
            }
            if (ev.target != owner || ev.actor < 0 || ev.actor > 1 || ev.actor == owner) {
                return;   // 只弹"别人对我就地免疫成功"的异常
            }
            if (!onstage_has_soul_mark(c, owner, 888)) {
                return;
            }
            const int left = read_int(c, k888ReflLeft + owner);
            if (left <= 0) {
                return;   // 3 次反弹已用尽
            }
            write_int(c, k888ReflLeft + owner, left - 1);
            (void)g_core->apply_anomaly(c, ev.actor, ev.amount, random_anomaly_duration(),
                                        owner);
        },
        WatcherScope::TEAM);
}

EffectResult k888_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const HostRow row = host_row(ctx, a, 888);
    if (!row.valid) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    // ⚠️ epoch 守卫：开局 ENTER_EXIT 桶双触发（k_guard 记档），第二趟若落进"重登场"
    // 分支会把刚写的反弹计数器清掉——同 epoch 只处理一次，真重登场（epoch 已递增）
    // 才走清账分支。
    if (already_armed_this_epoch(ctx, owner, 888 * 100 + 9)) {
        return EffectResult::kOk;
    }
    const int reflect_counts = db_at(row, 0, 3);
    const int drop_rounds = db_at(row, 1, 3);

    if (!ctx->plugin_storage.count(k888First + owner)) {
        // ── 首次登场：①次数型免疫票 + ②免降窗 + 反弹计数器 + ③watcher 接线 ──
        ctx->plugin_storage.emplace(k888First + owner, 1);
        if (reflect_counts > 0) {
            ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                                /*anomaly_mask=*/0, /*duration_rounds=*/0,
                                /*source_id=*/k888ImmuneAnomaly, /*soul_immunity=*/false,
                                EffectScope::ON_STAGE, /*counts=*/reflect_counts,
                                /*source_effect_id=*/888);
            write_int(ctx, k888ReflLeft + owner, reflect_counts);
        }
        if (drop_rounds > 0) {
            ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                                /*anomaly_mask=*/0, /*duration_rounds=*/drop_rounds,
                                /*source_id=*/k888ImmuneDrop, /*soul_immunity=*/false,
                                EffectScope::ON_STAGE, /*counts=*/0,
                                /*source_effect_id=*/888);
        }
        // ③ 攻击异常/补偿链 watcher（一场一次；boss 门在回调内手动判——watcher 不随
        // 节点 boss_invalid 走，hamo_3809_ao 口径；①②无 boss 尾注不受门限）。
        if (!ctx->plugin_storage.count(k888WireSkill + owner)) {
            ctx->plugin_storage.emplace(k888WireSkill + owner, 1);
            const int comp_pct = db_at(row, 3, 50);
            const int comp_rounds = db_at(row, 2, 2);
            ctx->event_center_.register_watcher(
                EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
                /*once=*/false,
                [owner, comp_pct, comp_rounds](BattleContext* c, const BattleEvent& ev) {
                    if (!c || !g_core || !g_core->apply_anomaly_checked
                        || !g_core->break_round_effects) {
                        return;
                    }
                    if (c->is_boss_challenge) {
                        return;   // ③（BOSS无效）：boss 局整链不执行
                    }
                    if (ev.actor != owner || ev.used_attribute) {
                        return;   // 攻击技窗（K396 ③：E13 使用即发，miss 也算）
                    }
                    if (!onstage_has_soul_mark(c, owner, 888)) {
                        return;
                    }
                    const int opp = 1 - owner;
                    if (c->getPet(opp).hp <= 0) {
                        return;   // 对手已倒地不施加
                    }
                    // 补偿窗支（先行——"下2回合自身攻击有 N% 几率"，与主支并存不互斥）。
                    const int win_from = read_int(c, k888WinFrom + owner);
                    const int win_to = read_int(c, k888WinTo + owner);
                    if (win_from > 0 && win_to >= win_from
                        && c->roundCount >= win_from && c->roundCount <= win_to) {
                        static const int kCompPool[3] = {kFear, kParalysis, kFatigue};
                        const int anomaly = kCompPool[sbs_rand() % 3];
                        write_int(c, k888AuditComp + owner, comp_pct);
                        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                            c, opp, anomaly, random_anomaly_duration(), owner, comp_pct,
                            ChanceSource::SoulMark);
                        if (anomaly_landed(r)) {
                            write_int(c, k888CompLand + owner,
                                      read_int(c, k888CompLand + owner) + 1);
                        }
                    }
                    // 主支：池 {烧伤,冻伤,中毒} 等概率挑一，100% 申报（短路不耗 rand）。
                    static const int kMainPool[3] = {kBurn, kFrostbite, kPoison};
                    const int anomaly = kMainPool[sbs_rand() % 3];
                    write_int(c, k888AuditMain + owner, 100);
                    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                        c, opp, anomaly, random_anomaly_duration(), owner, /*chance_pct=*/100,
                        ChanceSource::SoulMark);
                    if (anomaly_landed(r)) {
                        write_int(c, k888MainLand + owner,
                                  read_int(c, k888MainLand + owner) + 1);
                        return;
                    }
                    // 未触发：消除对手回合类效果 + 武装补偿窗 [R+1, R+db[2]]。
                    (void)g_core->break_round_effects(c, opp);
                    write_int(c, k888BreakCnt + owner, read_int(c, k888BreakCnt + owner) + 1);
                    write_int(c, k888WinFrom + owner, c->roundCount + 1);
                    write_int(c, k888WinTo + owner, c->roundCount + comp_rounds);
                },
                WatcherScope::TEAM);
        }
        // ① 反弹 watcher（一场一次，随首次登场接线；余量计数器驱动）。
        arm_888_reflect_watcher(ctx, owner);
        return EffectResult::kOk;
    }

    // 重登场：首次旗已在 → 不重授（"首次"严格一次）；反弹计数器显式清账
    //（ON_STAGE 免疫票已随切走作废——防"票死计数活"的假反弹，K7 清账教训）。
    ctx->plugin_storage.erase(k888ReflLeft + owner);
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 2087 秘尔威缇：每次登场岳灵涤荡（免异常+三通道减伤 1 回合）+ 击败回体再触发
// ═════════════════════════════════════════════════════════════════════════════

// 涤荡活跃：戳 == roundCount+1（戳位 +1 偏移防首回合 0 撞车，K394 教训）。
bool purify_active(BattleContext* ctx, int owner) {
    return read_int(ctx, k2087Purify + owner) == ctx->roundCount + 1;
}

// 触发一次岳灵涤荡：免异常窗（同句柄刷新）+ 涤荡戳。
void trigger_purify(BattleContext* ctx, int owner, int rounds) {
    if (rounds <= 0) {
        return;
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/rounds,
                        /*source_id=*/k2087ImmuneAnomaly, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/2087);
    write_int(ctx, k2087Purify + owner, ctx->roundCount + 1);
    write_int(ctx, k2087PurifyCnt + owner, read_int(ctx, k2087PurifyCnt + owner) + 1);
}

EffectResult k2087_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const HostRow row = host_row(ctx, a, 2087);
    if (!row.valid) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    const int window_rounds = db_at(row, 0, 1);
    const int mitigate_pct = db_at(row, 1, 30);

    // ⚠️ epoch 守卫：开局双触发会把涤荡计数翻倍——同 epoch 只触发一次（涤荡重授权
    // 交由 ② 的"再次触发"与真重登场走本节点新 epoch）。
    if (!already_armed_this_epoch(ctx, owner, 2087 * 100 + 9)) {
        // ① 每次登场触发岳灵涤荡（重登场重授=刷新）。
        trigger_purify(ctx, owner, window_rounds);

        // 三通道减伤票：随本 epoch 挂载（ON_STAGE 票随切换作废，K1 同款）。
        // 攻击伤害（红伤）REDUCE_PCT。
        ctx->register_damage_effect(
            DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
            [owner, mitigate_pct](BattleContext* c, int bucket_owner) {
                DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(c, bucket_owner, 2087) || !purify_active(c, bucket_owner)) {
                    return;
                }
                d.final = d.final * (100 - mitigate_pct) / 100;
            });
        // 固定伤害 / 百分比伤害：粉伤管线 REDUCE_EXTRA（百分比免减，乘法）。
        // PinkDamageTier 二值（FIXED/PERCENT——PERCENT_VALUE 归 PERCENT 档）。
        ctx->register_pink_damage_effect(
            PinkDamagePhase::REDUCE_EXTRA, owner,
            [owner, mitigate_pct](BattleContext* c, int bucket_owner) {
                PinkDamageResolved& p = c->resolvedPink;
                if (p.target != bucket_owner || p.tier != PinkDamageTier::FIXED
                    || p.final <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(c, bucket_owner, 2087) || !purify_active(c, bucket_owner)) {
                    return;
                }
                p.final = p.final * (100 - mitigate_pct) / 100;
            });
        ctx->register_pink_damage_effect(
            PinkDamagePhase::REDUCE_EXTRA, owner,
            [owner, mitigate_pct](BattleContext* c, int bucket_owner) {
                PinkDamageResolved& p = c->resolvedPink;
                if (p.target != bucket_owner || p.tier != PinkDamageTier::PERCENT
                    || p.final <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(c, bucket_owner, 2087) || !purify_active(c, bucket_owner)) {
                    return;
                }
                p.final = p.final * (100 - mitigate_pct) / 100;
            });
    }
    return EffectResult::kOk;
}

// ② 击败对手 → 恢复 maxHP/db[2] + 再次触发涤荡（boss_invalid 节点）。
EffectResult k2087_after_defeated(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 2087)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;   // 同归于尽无"恢复自身"主体（记档）
    }
    if (find_defeated_foe_slot(ctx, owner) < 0) {
        return EffectResult::kOk;   // 归因门：对手在场宠未倒地 / 最后一击非本方
    }
    const HostRow row = host_row(ctx, a, 2087);
    const int denom = db_at(row, 2, 3);
    if (denom <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->heal_amount(ctx, owner, max_hp / denom);
    write_int(ctx, k2087HealCnt + owner, read_int(ctx, k2087HealCnt + owner) + 1);
    // 再次触发岳灵涤荡（免异常票同句柄刷新 + 戳重写）。
    trigger_purify(ctx, owner, db_at(row, 0, 1));
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3 / SOUL_PRE_BATTLE +1（只增不改）──
// 720 = early ENTER arm（K8 203 同形：开局双触发由 epoch 盾刷新/epoch 守卫吸收）；
// 888 = ENTER（首次门在节点内）；2087 = ENTER + AFTER_DEFEATED（② boss_invalid）。
SOUL_PROGRAM(720, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k720_enter,
                    /*once=*/false, /*early=*/true)});
SOUL_PRE_BATTLE(720, PreBattlePhase::STAT_MODIFY, k720_pre_battle);
SOUL_PROGRAM(888, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k888_enter,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(2087, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k2087_enter,
                     /*once=*/false, /*early=*/true),
                    SOUL_NODE(State::BATTLE_AFTER_DEFEATED, &k2087_after_defeated,
                     /*once=*/false, /*early=*/false, SoulScope::STAGE, /*boss_invalid=*/true)});
