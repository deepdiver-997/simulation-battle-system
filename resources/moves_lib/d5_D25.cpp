#include <utils/sbs_rand.h>
// D25 · 琉梦 4761 第五技「无觉之长梦 37930」缺失效果 2305、1756、805
//（夜跑 night-2，2026-09-28）
//
// 37930 链 = [2305, 1756, 805]、args [100, 8, 1, 2]：三条全为本文件。
// 4399 图鉴（无觉之长梦）："先制+3；必中；双方的此条效果本场战斗只能生效1次，若己方
// 存在希望之花则消耗自身全部体力并使己方的危机感+1，若己方的希望之花已被摧毁则使自身
// 获得10层魔王咒怨；消除对手回合类效果，消除成功则100%令对手睡眠，未触发或对手回合类
// 效果无法被消除则下1次对手使用的属性技能无效；消除对手能力提升状态，消除成功则令对手
// 随机2项技能PP值归零"。
//
// 2305 "双方的此条效果本场战斗只能生效1次，若己方存在希望之花则消耗自身全部体力并使
//   己方的危机感+1，若己方的希望之花已被摧毁则使自身获得10层魔王咒怨"：
//   - "希望之花存在" = 琉梦魂印 2386 的希望态槽（soulmark_storage[238691]，zha_liumeng
//     魂印侧写入）**存在**（含已成熟）；"已被摧毁" = 槽被抹（秒杀摧毁路径已清槽）；
//   - 消耗自身全部体力 = force_hp_to_zero 自体（死亡漏斗/拦截器/监听全链生效）；
//   - 获得 10 层魔王咒怨 = ensure_curse_wiring + add_curse_stacks_slot(+10)
//     （curse_stack.h 共享层，越 10 层自动授控制类免控票）；
//   - ⛔ 部分跳过（记档待拍板）："使己方的危机感+1"——危机感是琉梦魂印侧
//     LiumengHopeState 结构的私有字段（zha_liumeng_curse.cpp 匿名命名空间，无共享头），
//     技能侧无法读写；外部直写会破坏 any 类型。需魂印侧暴露 +1 接口后回补；
//     本实现落地"消耗体力"半边，"危机感+1"未生效。
//   - "双方只能生效1次"：按持有方各自 1 次（plugin_storage 门，生效=走过判定）。
// 1756（[100, 8, 1]）"消除对手回合类效果，消除成功则{0}%令对手{1}，未触发或对手回合类
//   效果无法被消除则下{2}次对手使用的属性技能无效"：
//   - had_round 快照（消除前）判分支；成功 + roll_percent(100)（短路必发）→ 施加睡眠
//     （8，apply_anomaly 申报 + random_anomaly_duration，1959 同款）；未命中或本就无
//     回合类 → seal_skill 属性单类 count=1（976/1056 次数型同款）。
// 805（[2]）"消除对手能力提升状态，消除成功则令对手随机{0}项技能PP值归零"：
//   - clear_stat_boosts > 0 才触发；随机不重复挑 pp>0 的槽走 pp_zero_slot 原语
//     （583 同款——清 PP 必走原语，直写绕过事件）。
//
// 场景回归：test/scenario/scenario_410_liumeng_4761_2305_1756_805.cpp。

#include "common.h"

#include <effects/curse_stack.h>

namespace {

constexpr int kLiumengMarkId = 2386;             // 琉梦魂印（希望态宿主）
constexpr int kLiumengHopeSlotKey = 238691;      // 魂印侧希望态槽（2386*100+91）
constexpr int k2305OnceKeyBase = 990230510;      // 2305 本场已生效旗标（owner 侧）

int storage_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// ── 2305 —— 一次性：有希望之花 → 消耗自身体力；已摧毁 → 10 层魔王咒怨 ──
EffectResult effect_2305_once_flower_or_curse(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = k2305OnceKeyBase + owner;
    if (storage_int(ctx, key) != 0) {
        return EffectResult::kOk;   // 本场已生效过（双方各一次，按持有方计）
    }
    // 定位己方琉梦（魂印 2386 载体）：希望态槽挂在其 soulmark_storage
    const int lm_slot = [&]() -> int {
        for (int slot = 0; slot < 6; ++slot) {
            const ElfPet& pet = ctx->seerRobot[owner].elfPets[slot];
            if (pet.soulMark.id == kLiumengMarkId) {
                return slot;
            }
        }
        return -1;
    }();
    if (lm_slot < 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[key] = 1;   // 生效门：判定即消耗（无论走哪个分支）
    auto& storage = ctx->seerRobot[owner].elfPets[lm_slot].soulmark_storage;
    if (storage.count(kLiumengHopeSlotKey)) {
        // 己方存在希望之花 → 消耗自身全部体力（death 漏斗原语，拦截器/监听全链）
        if (g_core->force_hp_to_zero) {
            (void)g_core->force_hp_to_zero(ctx, owner, /*actor=*/owner);
        }
        // ⛔ "使己方的危机感+1"：HopeState 为魂印侧私有结构（无共享头），技能侧不可
        //    读写——外部直写破坏 any 类型。跳过并记档，待魂印侧暴露接口后回补。
        return EffectResult::kOk;
    }
    // 希望之花已被摧毁 → 获得 10 层魔王咒怨（共享层；越 10 层授控制类免控票）
    ensure_curse_wiring(ctx);
    add_curse_stacks_slot(ctx, owner, lm_slot, /*delta=*/10);
    return EffectResult::kOk;
}

// ── 1756 —— 消回合成功{0}%令对手睡眠；未触发/无法消除 → 封对手{2}次属性技 ──
EffectResult effect_1756_break_sleep_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int status = args.int_args[3];
    const int seal_count = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const bool had_round = ctx->rule_center_.has_round_type(target);
    if (had_round && g_core->break_round_effects) {
        g_core->break_round_effects(ctx, target);
        if (roll_percent(chance) && g_core->apply_anomaly) {
            (void)g_core->apply_anomaly(ctx, target, status,
                                        random_anomaly_duration(), owner);
            return EffectResult::kOk;   // 施加成功（100% 短路必发）
        }
    }
    // 未触发 或 对手回合类效果无法被消除（本就没有）→ 封属性技 {2} 次
    if (g_core->seal_skill) {
        g_core->seal_skill(ctx, /*source=*/owner, /*target=*/target, /*effect_id=*/1756,
                           /*attribute=*/true, /*attack=*/false, /*count=*/seal_count,
                           /*duration_rounds=*/0, /*penetrable=*/true,
                           /*source_slot=*/-1, EffectScope::ON_STAGE,
                           /*hit_invalid=*/false, /*chance_pct=*/100,
                           /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

// ── 805 —— 消除对手能力提升状态，消除成功则令对手随机{0}项技能PP值归零 ──
EffectResult effect_805_clear_then_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->clear_stat_boosts
        || !g_core->pp_zero_slot) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int count = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, target) <= 0) {
        return EffectResult::kOk;   // 消除失败（无提升可清）
    }
    ElfPet& opp_pet = ctx->getPet(target);
    std::vector<int> candidates;
    for (int i = 0; i < static_cast<int>(opp_pet.skills.size()); ++i) {
        if (opp_pet.skills[static_cast<std::size_t>(i)].pp > 0) {
            candidates.push_back(i);
        }
    }
    for (int k = 0; k < count && !candidates.empty(); ++k) {
        const int pick = sbs_rand() % static_cast<int>(candidates.size());
        (void)g_core->pp_zero_slot(ctx, target, candidates[static_cast<std::size_t>(pick)],
                                   /*actor=*/owner);
        candidates.erase(candidates.begin() + pick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2305, effect_2305_once_flower_or_curse);
SKILL_EFFECT(1756, effect_1756_break_sleep_seal);
SKILL_EFFECT(805, effect_805_clear_then_pp_zero);
