// K392 锁伤族 CAP 二批 —— effect 928/1191/1938（3 宏 / 3 行）
//（2026-10-26，worktree ../agents/k392；K1 k_guard_family 766 的 CAP 阶段复用）
//
// 行清单（tools/soul_effects_dump.py 928 1191 1938，2026-10-26）：
//   928  茉蕊儿 3322 args[2,500,1,1]（kind[4,7]）：
//     「回合开始时，若对手使用攻击技能则当回合自身受到的攻击伤害不超过自身最大体力的一半，
//       若对手使用属性技能则当回合免疫异常状态且回合结束后解除自身能力下降状态（BOSS无效）；
//       自身死亡后则下只登场精灵获得500点护盾且免疫1次异常状态」
//     → db[0]=2（锁伤分母 1/2；500/1/1 属死亡继承子句，工单行外→记档跳过）。
//   1191 芳馨·茉蕊儿 3613 args[2,1,500,500,1]（kind[4,7,15]）：
//     「回合开始时，若当回合对手选择使用攻击技能，则使对手随机1个技能PP值归零且当回合
//       自身受到的攻击伤害不超过最大体力的1/2；|（属性技分支免疫异常+免降 / 死亡继承
//       500护盾+500护罩+免1次异常——均在工单行外→记档跳过）」
//     → db[0]=2（锁伤分母）、db[1]=1（PP 归零技数）。
//   1938 狂沙之主·萨特 4354 args[1,2,2,2,2,1,1]（kind[7,8,9]）：
//     「回合开始时若自身体力高于最大体力的1/2，则当回合自身受到的攻击伤害不超过自身
//       最大体力的1/2，战斗阶段结束时若自身体力低于最大体力的1/2则下回合所有技能先制+2
//       |（同型免异常/黯天之尘等第二句——工单行外→记档跳过）」
//     → db[1]=2（体力门分母）、db[2]=2（锁伤分母）。
//
// ⚠️ 三查记录（a01-kcap 2026-10-26）：①内核 grep 928/1191/1938 无魂印行为硬编码
//   （rule_center.h:320 的 928 是官方技能 id 年代例证注释，非行为）②全库并集无同 id
//   SOUL_MARK/SOUL_PROGRAM 注册 ③worktree 内无未合并同族分支（feat/k393 为别单）。
//
// 「对手使用攻击技能」判定时点（本单核心口径，工单行钦定）：
//   · 928/1191 的 **CAP 子句 = 回调内实时判**：伤害落地时读 ws.skill_type_view[attacker]
//     （实际物化类型，速查 §四点五 executed_skill_is_attack 判据）——攻击伤害只出自攻击技
//     执行中（view 恒=攻击类型），属性技执行中的属性直伤 view=Attribute → 不锁，恰与
//     「对手用属性技走免异常分支、不走锁伤分支」的官方分支语义一致。回合开始的
//     roundChoice（选择期静态判据）对 CAP 不可用：出手顺序未定，若对手后手属性技而
//     我方先受伤，选择期判据会把攻击伤害错误锁住/放行——实时判天然免疫这个时序问题。
//   · 1191 的 **PP 归零子句 = ROUND_START 选择期判**（官方明文「选择使用」）：BATTLE_
//     ROUND_START 晚于 CHOOSE（battleContext.h:612 注释，roundChoice 已写入且被
//     resetWorkspace 显式保留）→ 读 roundChoice 静态类型（selected_skill_is_attack
//     判据私副本）。引擎 PP 无执行期复核（consume_selected_skill_pp 只扣不查可用，
//     battleFsm.cpp:461）→ 归零不取消当回合执行，与官方「选择已做出」一致。
//   · 928 的 **免异常子句 = ROUND_START 挂票 + 施加时点现场判**（K171 285 满血免异常
//     同款 condition 形态）：票 duration=1 每回合重授，condition 读对手 roundChoice
//     选的是属性技才响应——对手选攻击技/切宠/被控 NONE 都不响应。
//   · 1938 的 **体力门 = ROUND_START 评一次锁当回合**（K1 4836 观星派派同款 rt 旗，
//     非受伤当下实时比——「回合开始时若…则当回合…」的官方 latch 语义）。
//
// 697「无视伤害限制」凭证消费：与 K1 766 相同——CAP 回调内查
//   ws.attack_credential[attacker].ignore_damage_limit，持凭证直接放行（穿锁伤）。
//
// ⚠️ boss_invalid 标注（按「带尾注的子句」单位，plugin_interface.h SoulMarkNodeRef 注）：
//   928 免异常子句带「（BOSS无效）」尾注 → 免疫节点 boss_invalid=true；928 CAP 子句、
//   1191/1938 全部子句行文无尾注 → 不标。K1 766 同族先例对「boss无效」整体忽略
//   （PvP 引擎 S1 §七口径）与本案不冲突：那是尾注覆盖整句的 766；928 的尾注明确挂在
//   第二分支句尾。
//
// ── 记档：机械领域族 1780/1935（effect_des id=126 场景，只调研不实现）──
//   行 dump（1780 row1408 / 1935 row1580，同为贝狄薇尔 4184，1935 为伙伴效果进化行）：
//   「自身先出手前令对手所有技能PP值-1(-2)，后出手前令对手随机1个技能PP值归零；|自身
//   登场时若对手体力值＞500则激活**机械领域**（持续3回合），若≤500则激活次元领域；
//   主动切换登场则额外令对手当回合技能PP消耗5(20)倍；领域结束当回合战斗阶段结束时
//   自身获得500点护盾和500点护罩（boss无效）|机械领域：对手造成的攻击伤害不超过自身
//   最大体力的60%(40%)；战斗阶段结束时若当回合受到了攻击伤害则下回合所有技能先制
//   +1(+2)|次元领域：对手无法通过技能恢复体力；战斗阶段结束时若受攻击则吸取对手
//   最大体力的1/3」。实现缺口：①登场合激活 + 3 回合领域窗（多分支领域态机，需
//   enter/exit 生命周期 + 领域种类旗）；②「对手造成的攻击伤害≤maxHP%」CAP 可直接
//   复用本单 CAP 形态（分母 100/60、100/40，条件=机械领域窗内）；③先/后手分支的
//   PP 扣减与归零（K6 1576/K7 1639 已有 PP 原语可复用）；④PP 消耗倍率改写
//   （ws.skill_pp_cost_multiplier 写点需查——字段已存在）；⑤封回血（HEAL_BLOCK
//   免疫票，J 系有先例）；⑥护盾/护罩授予 + 领域结束回血结算点。工作量约 3-4 个
//   普通单，留待下轮立单（本单按工单行只记档）。
//
// 先例：K1 k_guard_family 766（CAP 阶段/EpochGuard/697 凭证/host_row/db_at）、
//   K6 k_hit_suppress_cond（executed_is_attack 私副本/SOUL_PROGRAM 声明节点形态/
//   PP 归零）、K7 k_spacetime_leap 1639（ROUND_START 随机 PP 归零）、
//   K171 k_fullhp_immune_family 285（condition 免疫票 duration=1 每回合重授）、
//   K275 k275_immune_all_anomaly 644（grant_immunity 全参形）。
// 场景回归：test/scenario/scenario_7450_damage_cap2.cpp（号段 7450-7459）。

#include <utils/sbs_rand.h>

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <entities/skills.h>

namespace {

// ── 宿主行参数视图（"行≠宏"：全量 DB args + monster_id，K1/K7 同款私副本）────────
// ⚠️ 只读宿主 pet.soulMark.args.owned_int_args 原序（bind 前 DB 原样），禁 a.row
//    （bind 丢前两位 → 真实对局常空，见《魂印行参读取路径缺陷-2026-10-21》）。

struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const EffectArgs& args, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);   // 场景兜底（mozun 同款）
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

// ── arm 幂等守卫（每 epoch 一次，K1 EpochGuard 同款）──────────────────────────
// 开局 early 的 ENTER_EXIT_STAGE 节点跑两次（activate + init_battle 桶）→ 不设防
// 则管线同 epoch 注册两条（锁伤重复，虽然 min 语义下幂等，仍按纪律防重）。

bool already_armed_this_epoch(BattleContext* ctx, int owner, int storage_key) {
    ElfPet& pet = ctx->getPet(owner);
    if (!pet.soulmark_storage.count(storage_key)) {
        pet.soulmark_storage[storage_key] = 0;
    }
    int& armed_epoch = std::any_cast<int&>(pet.soulmark_storage[storage_key]);
    if (armed_epoch == ctx->pipeline_valid_id[owner]) {
        return true;
    }
    armed_epoch = ctx->pipeline_valid_id[owner];
    return false;
}

// ── 判据统一 §四点五 私副本（soul 侧不链 moves_lib/common.h，K6 同款）──────────

// 执行期：该方当前物化技能类型是否攻击技（skill_type_view；ws 每回合 reset 无跨回
// 合残留，回调时点读到的就是本次执行技能）。
bool executed_is_attack(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    return ctx->ws.skill_type_view[side] != static_cast<int>(SkillType::Attribute);
}

// 选择期：该方本回合选中的技能（roundChoice 静态 type）是否攻击技。仅选择已做出
// （BATTLE_ROUND_START 及之后、技能尚未执行语境）时调用。
// ⚠️ 直读 seerRobot/on_stage 公共成员（getPet 无 const 重载，condition 回调拿到的是
//    const BattleContext*——common.h damage_guard_suppressed 同款直读纪律）。
bool selected_is_attack(const BattleContext* ctx, int side) {
    constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    if (ctx->roundChoice[side][0] != kActionSelectSkill) {
        return false;
    }
    const int idx = ctx->roundChoice[side][1];
    if (idx < 0 || idx >= static_cast<int>(
            ctx->seerRobot[side].elfPets[ctx->on_stage[side]].skills.size())) {
        return false;
    }
    return ctx->seerRobot[side].elfPets[ctx->on_stage[side]]
        .skills[static_cast<std::size_t>(idx)].type != SkillType::Attribute;
}

// 选择期：对手是否选了属性技（928 免异常分支的条件）。
bool selected_is_attribute(const BattleContext* ctx, int side) {
    constexpr int kActionSelectSkill = 1;
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    if (ctx->roundChoice[side][0] != kActionSelectSkill) {
        return false;
    }
    const int idx = ctx->roundChoice[side][1];
    if (idx < 0 || idx >= static_cast<int>(
            ctx->seerRobot[side].elfPets[ctx->on_stage[side]].skills.size())) {
        return false;
    }
    return ctx->seerRobot[side].elfPets[ctx->on_stage[side]]
        .skills[static_cast<std::size_t>(idx)].type == SkillType::Attribute;
}

// 697「无视伤害限制」凭证（K1 766 首消费同款读法）。
bool attacker_pierces_damage_limit(const BattleContext* c, int attacker_id) {
    return attacker_id >= 0 && attacker_id <= 1
        && c->ws.attack_credential[attacker_id].ignore_damage_limit;
}

// ── 通用 CAP 回调（928/1191 共用形态：对手攻击技实时判）──────────────────────
// cap = 防守方 maxHP / cap_denom（行≠宏：分母来自 db）。只锁红伤（粉伤独立管线，
// 本族行文均「攻击伤害」，K1 766 同口径）。

void register_foe_attack_cap(BattleContext* ctx, int owner, int mark_id, int cap_denom) {
    if (cap_denom <= 0) {
        return;
    }
    ctx->register_damage_effect(
        DamagePhase::CAP, owner, DamageEffectCategory::MITIGATE,
        [mark_id, cap_denom](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, mark_id)) {
                return;
            }
            // 「对手使用攻击技能」实时判：伤害必须来自对手且其当前执行技能为攻击技。
            if (d.attackerId != 1 - bucket_owner || !executed_is_attack(c, d.attackerId)) {
                return;
            }
            const int cap = resolve_pet_max_hp(c->getPet(bucket_owner)) / cap_denom;
            if (d.final <= cap) {
                return;
            }
            if (attacker_pierces_damage_limit(c, d.attackerId)) {
                return;   // 官方 697「无视伤害限制」穿锁伤（K1 766 凭证消费链）
            }
            d.final = cap;
        });
}

// ════════════════════ 928 茉蕊儿 ════════════════════

constexpr int k928ImmuneHandle = 928100;   // 固定句柄：同 source_id 重复授予=刷新

// early arm：挂 CAP（每 epoch 一次）。
EffectResult soulmark_928_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (already_armed_this_epoch(ctx, owner, 928 * 100 + 9)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 928);
    const int cap_denom = db_at(row, 0, 2);
    register_foe_attack_cap(ctx, owner, 928, cap_denom);
    return EffectResult::kOk;
}

// ROUND_START：挂当回合免异常票（condition=对手选了属性技才响应；K171 285 同款
// 「condition 现场判」形态——票常在，分支在施加时点逐次判）。
EffectResult soulmark_928_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 928)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    ctx->grant_immunity(
        owner, ImmunityType::ANOMALY, coverage_all(),
        /*anomaly_mask=*/0,          // 0 = 全异常（不限码）
        /*duration_rounds=*/1,       // 当回合票，每回合重授刷新
        /*source_id=*/k928ImmuneHandle,
        /*soul_immunity=*/true,
        EffectScope::ON_STAGE,       // 换宠即清（免疫随魂印宿主）
        /*counts=*/0,
        /*source_effect_id=*/928,
        ImmunityTier::Modern,        // 2026 现代行，默认档（无主动毒实测证据）
        [opp](const BattleContext* c, int) {
            return selected_is_attribute(c, opp);
        });
    return EffectResult::kOk;
}

// ════════════════════ 1191 芳馨·茉蕊儿 ════════════════════

// early arm：挂 CAP（每 epoch 一次）。
EffectResult soulmark_1191_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (already_armed_this_epoch(ctx, owner, 1191 * 100 + 9)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 1191);
    const int cap_denom = db_at(row, 0, 2);
    register_foe_attack_cap(ctx, owner, 1191, cap_denom);
    return EffectResult::kOk;
}

// ROUND_START：对手选择期选中攻击技 → 随机 db[1] 个技能 PP 归零（K7 1639 同款：
// 只挑 pp>0 槽、pp=-1 无限 PP 跳过；K6 1314 去重抽取形）。官方「选择使用」=选择期
// 判据；归零不取消当回合执行（引擎 PP 只扣不查可用，文件头记档）。
EffectResult soulmark_1191_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1191)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (!selected_is_attack(ctx, opp)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 1191);
    const int zero_count = db_at(row, 1, 1);
    if (zero_count <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& foe = ctx->getPet(opp);
    std::vector<int> pp_slots;
    for (int i = 0; i < static_cast<int>(foe.skills.size()); ++i) {
        if (foe.skills[static_cast<std::size_t>(i)].pp > 0) {
            pp_slots.push_back(i);
        }
    }
    int done = 0;
    int guard = 0;
    while (done < zero_count && !pp_slots.empty() && guard < 100) {
        ++guard;
        const int pick = static_cast<int>(sbs_rand() % pp_slots.size());
        Skills& sk = foe.skills[static_cast<std::size_t>(pp_slots[static_cast<std::size_t>(pick)])];
        if (sk.pp > 0) {
            sk.pp = 0;
            ++done;
        }
        pp_slots.erase(pp_slots.begin() + pick);   // 去重抽取（同槽不重复归零）
    }
    return EffectResult::kOk;
}

// ════════════════════ 1938 狂沙之主·萨特 ════════════════════

struct Cap2Runtime {
    int armed_epoch = -1;        // arm 每 epoch 一次（开局 early 节点双触发，K1 同款）
    bool round_cap_active = false;   // ROUND_START 评出的本回合锁伤门（latch）
};

Cap2Runtime& cap2_runtime(BattleContext* ctx, int owner) {
    ElfPet& pet = ctx->getPet(owner);
    constexpr int kSlotKey = 1938 * 100 + 1;
    if (!pet.soulmark_storage.count(kSlotKey)) {
        pet.soulmark_storage[kSlotKey] = Cap2Runtime{};
    }
    return std::any_cast<Cap2Runtime&>(pet.soulmark_storage[kSlotKey]);
}

// early arm：挂 CAP（体力门 latch 旗消费，K1 4836 观星派派同款）。
EffectResult soulmark_1938_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    Cap2Runtime& rt = cap2_runtime(ctx, owner);
    if (rt.armed_epoch == ctx->pipeline_valid_id[owner]) {
        return EffectResult::kOk;
    }
    rt.armed_epoch = ctx->pipeline_valid_id[owner];
    const HostRow row = host_row(ctx, args, 1938);
    const int cap_denom = db_at(row, 2, 2);
    if (cap_denom <= 0) {
        return EffectResult::kOk;
    }
    ctx->register_damage_effect(
        DamagePhase::CAP, owner, DamageEffectCategory::MITIGATE,
        [cap_denom](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, 1938)) {
                return;
            }
            const int cap = resolve_pet_max_hp(c->getPet(bucket_owner)) / cap_denom;
            if (d.final <= cap) {
                return;
            }
            if (attacker_pierces_damage_limit(c, d.attackerId)) {
                return;   // 697 凭证穿锁伤（K1 766 同款）
            }
            if (!cap2_runtime(c, bucket_owner).round_cap_active) {
                return;   // 体力门：ROUND_START latch（「回合开始时若高于1/2」）
            }
            d.final = cap;
        });
    return EffectResult::kOk;
}

// ROUND_START：体力门评估（hp*gate_denom > maxHP 即「高于 1/gate_denom」，乘法
// 避整除边界）。
EffectResult soulmark_1938_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1938)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 1938);
    const int gate_denom = db_at(row, 1, 2);
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    Cap2Runtime& rt = cap2_runtime(ctx, owner);
    rt.round_cap_active = self.hp > 0 && gate_denom > 0 && self.hp * gate_denom > max_hp;
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（只增不改）──────────────────
// CAP 走 early=true 的 ENTER_EXIT_STAGE arm 节点（每次登场重挂，管线 epoch 兜底
// 下场作废）；回合内子句走 BATTLE_ROUND_START 声明节点（更新器每回合重注册，
// K6 声明式形态——arm 里手工挂桶会被重建清掉）。
SOUL_PROGRAM(928, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_928_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_928_round_start,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE,
                    /*boss_invalid=*/true)});   // 免异常子句带（BOSS无效）尾注
SOUL_PROGRAM(1191, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1191_arm,
                     /*once=*/false, /*early=*/true),
                    SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1191_round_start)});
SOUL_PROGRAM(1938, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1938_arm,
                     /*once=*/false, /*early=*/true),
                    SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1938_round_start)});
