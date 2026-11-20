// K370 免疫降属+条件烧伤+PP分档族 —— effect 1231（1 宏 / 2 行，圣羽天翔·英普瑞斯3657）
//
// 行清单（tools/soul_effects_dump.py 1231，2026-10-24）：
//   row944  args[50,2,2,1,2,2,1]（icon920 本体行）：「天生免疫能力下降状态，回合开始时
//     若对手不处于能力下降状态则当回合所有技能50%令对手烧伤；|若自身体力高于最大体力
//     的1/2则对手每回合所有技能PP减少1点，若自身体力低于最大体力的1/2则对手每回合
//     所有技能PP减少2点（boss无效）」
//   row1478 args[100,2,2,1,2,2,1]（icon1454 伙伴行，come=X之战意伙伴效果）：同文 100% 档。
//   ⚠️ 同宠双行（3657）：pet_factory 多版本行按 icon_id 升序取 .back() —— 实战恒绑
//   row1478（100% 档）；行参驱动实现两行通用。
//
// 行参解码（db_at 原序读 owned_int_args——2026-10-21 方案B，禁读 a.row）：
//   db[0]=烧伤概率%（50/100 随行变，与 tips 50%/100% 互锚）；db[1]=烧伤码（2=烧伤，
//   与 tips/官方异常码表互锚）；db[2]=低体档 PP 消耗（2）；db[3]=高体档 PP 消耗（1）
//   ——db[2]/db[3] 与 tips「高于1/2→PP-1、低于→PP-2」对齐消费。尾段 [2,2,1] 与
//   头段 [码,低,高] 重复、逐位含义不唯一 → 记档不猜（K355 2381 口径：tips 权威）。
//
// 落法：
//   ①天生免疫能力下降 = enter(early) grant_immunity STAT_DROP coverage_all ON_STAGE
//     永久票（K364 639/K355 2381 同通道，Modern 档）。
//   ②条件烧伤 = ROUND_START 查对手能力下降（k253 336 同款判据：任一槽
//     ability_levels[opp][i]<0）→ 写回合键旗标（每回合必写 1/0，k_hit_suppress_cond
//     1843 同形）；enter 常驻 EVENT_SKILL_USED watcher（actor==own 门，**无
//     used_attribute 门**——tips「所有技能」含属性技，K164 116 反差记档）→ 旗标开且
//     持印在场 → apply_anomaly_checked(烧伤2, pct=db[0], SoulMark)（概率申报给原语
//     内核单掷，禁本地双掷；100% 档短路不耗 rand → 场景确定性）。
//   ③PP 分档 = ROUND_START 体力分档：hp×2>max → pp_reduce(对手,db[3])；
//     hp×2<max → pp_reduce(对手,db[2])（先乘后除防截断）；等号档 tips 两分支皆未
//     覆盖 → 无归属不消耗（K347 663/K358 严格档口径）。
//   boss 口径：tips 尾注（boss无效）为整段限定 → 全节点 boss_invalid=true（K355 同形）。
//
// 三查记录（a23-kprod 2026-10-24 本 worktree 复核）：①内核 grep \b1231\b 无魂印行为
//   硬编码；②effect_info 1231 = 技能空间异文模板（「{0}%令对手{1}，未触发则附加对手
//   最大体力1/{2}的百分比伤害」——917 parser 同族），与魂印空间不冲突；③registry soul
//   集合无 1231、soul_lib 全库无 SOUL_PROGRAM(1231)——真缺口。4399✓3657 旁证一致。
// 先例：k364_foe_pp_drain 639（STAT_DROP 永久票+pp_reduce）、k253_statdrop_frostbite
//   336（对手能力下降判据）、k_attack_prob 116（概率异常申报通道）、
//   k_hit_suppress_cond 1843（回合键旗标门）。
// 场景回归：test/scenario/scenario_7150_immune_condburn_pp_1231.cpp（号段 7150-7159）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kMarkId = 1231;
constexpr int kWiredBase = 990370000;   // +owner：watcher 已挂旗标
constexpr int kCondBase = 990370010;    // +roundCount：当回合烧伤门旗（1=开）
constexpr int kBurnCode = static_cast<int>(AbnormalStatusId::Burn);   // 2

// ── 宿主行参数（db_at 原序读 owned_int_args，K364 HostRow 同款）────────────────
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

// 对手是否处于能力下降状态（引擎口径 = 任一能力槽为负等级，K253 336 同款）。
bool opp_has_stat_drop(BattleContext* ctx, int opp) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            return true;
        }
    }
    return false;
}

// 回合键旗标（any_int_or 安全读，禁抛式 any_cast——common.h 治理口径）。
void flag_write(BattleContext* ctx, int base, int key, int value) {
    ctx->plugin_storage.insert_or_assign(
        static_cast<unsigned long long>(base + key), std::any{value});
}

int flag_read(const BattleContext* ctx, int base, int key) {
    const auto it = ctx->plugin_storage.find(
        static_cast<unsigned long long>(base + key));
    return it != ctx->plugin_storage.end() ? any_int_or(it->second, 0) : 0;
}

// ── enter(early)：①STAT_DROP 永久票 + ②常驻用技 watcher（一次性挂，旗标门控）──
EffectResult k370_1231_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    // ①天生免疫能力下降（K364 直系）
    ctx->grant_immunity(a.owner, ImmunityType::STAT_DROP, coverage_all(),
                        0, 0, 123101, true, EffectScope::ON_STAGE,
                        0, kMarkId, ImmunityTier::Modern);
    // ②用技 watcher：常驻一条（early 双触发防重），当回合门旗驱动
    const unsigned long long wired = kWiredBase + a.owner;
    if (ctx->plugin_storage.count(wired)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(wired, 1);
    const int owner = a.owner;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != owner) {
                return;   // 只认自己用技（所有技能：属性技也触发，无 used_attribute 门）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            if (flag_read(c, kCondBase, c->roundCount) == 0) {
                return;   // 回合开始时对手处于能力下降 → 门闭
            }
            // 行参概率（50/100）申报给原语，内核单掷
            int slot = c->find_pet_with_soulmark(owner, kMarkId);
            if (slot < 0 || slot > 5) {
                return;
            }
            const std::vector<int>& db =
                c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
            const int pct = !db.empty() ? db[0] : 50;
            if (pct <= 0) {
                return;   // 显式 0 关闸
            }
            (void)g_core->apply_anomaly_checked(c, 1 - owner, kBurnCode,
                                                random_anomaly_duration(), owner,
                                                pct, ChanceSource::SoulMark);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── ROUND_START：②回合门旗写入 + ③PP 分档消耗 ────────────────────────────────
EffectResult k370_1231_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->pp_reduce) {
        return EffectResult::kOk;
    }
    if (a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(a.owner);
    const HostRow row = host_row(ctx, a, kMarkId);
    const int pp_high = db_at(row, 3, 1);   // 高于 1/2 档（tips：PP-1）
    const int pp_low = db_at(row, 2, 2);    // 低于 1/2 档（tips：PP-2）

    // ②门旗：对手不处于能力下降 → 当回合烧伤门开（每回合必写，防残留）
    flag_write(ctx, kCondBase, ctx->roundCount,
               opp_has_stat_drop(ctx, opp) ? 0 : 1);

    // ③PP 分档（先乘后除防截断；等号档无归属不消耗）
    ElfPet& self = ctx->getPet(a.owner);
    if (self.hp > 0) {
        const int max_hp = resolve_pet_max_hp(self);
        if (self.hp * 2 > max_hp && pp_high > 0) {
            (void)g_core->pp_reduce(ctx, opp, pp_high);
        } else if (self.hp * 2 < max_hp && pp_low > 0) {
            (void)g_core->pp_reduce(ctx, opp, pp_low);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1231 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k370_1231_enter,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k370_1231_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
