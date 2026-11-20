#include <utils/sbs_rand.h>
// 启灵元神 3986 技能组（moves_lib 侧；魂印在 soul_lib/qiling_1581.cpp）
//
// 神游·众生归渺（33894，特殊 160，必中）——2026-09-21 已落地：
//
// 官方 effect（DB effect_info + 4399 图鉴 907369；4399 给出了"大量真实伤害"的
// **内置公式**——这正是本文件的落点依据）：
//   · 1208「反转自身能力下降状态，反转成功则令对手下{0}次使用的攻击技能无效」
//     —— **已由天启帝君线实现**（tianqi_dijun_skills.cpp 的 SKILL_EFFECT(1208) 全局
//     注册，同 id 效果跨精灵共享），本文件不重复注册。
//   · 1533「造成的攻击伤害低于{0}时附加大量真实伤害」
//     —— 4399：「内置公式：真实伤害 =（350 − 造成伤害）× 1.2」。args=[350]。
//       读**裸伤台账**（ws.raw_attack_damage，公式值——"挡伤/锁伤改变不了它"口径，
//       雷解 42 同款读法）；时点 AFTER_ACTION（伤害落地之后才有"造成的伤害"可读）。
//   · 1534「100%令对手随机1个被放逐的精灵所有技能PP值-1，若对手不存在被放逐的精灵
//     则令对手当前精灵所有技能PP值-1」
//     —— 被放逐位图由 [神殇]（soul_lib/qiling_1581.cpp）每回合 ROUND_START 重掷并
//       发布到 plugin_storage（resources/qiling_runtime.h 的 int 槽，跨 dylib 约定）。
//
// ⚠️ PP-1 口径：每个技能 -1、地板 0；无限 PP（pp<0）不参与（吸取/扣减族既有约定）。
//
// ── 2026-09-22 批次A追加：技能组收尾（其余四技，DB moves + 4399 图鉴 907369）──
//   神游·浩渺星河（33893 特攻150 必中）：1532 + 984
//   真神信仰      （33891 特攻130）    ：600
//   神之佑        （27019 属性）       ：847 + 1346
//   神之束缚      （27020 属性 必中）  ：1440（936/1531/843 已有，全局复用）
//   神之宣告      （27021 属性 必中）  ：1536 + 971（1475/821 已有，全局复用）
//   神印联动读数：1532/1536 读 plugin_storage 的**神印层数发布槽**
//   （qiling_runtime.h 15830000+side，魂印侧 EVENT_SWAP 挂层时发布）——层数本体在
//   受害者 soulmark_storage[1581]（soul_lib 私有结构），跨 dylib 直接 any_cast 必炸。
#include "common.h"
#include "../qiling_runtime.h"

namespace {

// ── 1533 攻击伤害低于阈值 → 附加（350 − 伤害）× 1.2 的真实伤害 [AFTER_ACTION] ──
EffectResult effect_1533_low_damage_true(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    if (self < 0 || self > 1 || target < 0 || target > 1 || threshold <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).hp <= 0) {
        return EffectResult::kOk;   // 已被打死：不再补真伤（避免多出一条归因）
    }
    const int dealt = ctx->ws.raw_attack_damage[self];
    if (dealt >= threshold) {
        return EffectResult::kOk;   // "低于{0}时"——达到阈值不触发
    }
    const int extra = (threshold - dealt) * 12 / 10;   // （350 − 造成伤害）× 1.2
    if (extra > 0 && g_core->deal_true_damage) {
        g_core->deal_true_damage(ctx, target, extra, self);
    }
    return EffectResult::kOk;
}

// ── 1534 打被放逐精灵的 PP（无被放逐 → 打当前精灵）[SKILL_EFFECT] ──
EffectResult effect_1534_exile_pp_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const int mask = qiling::exile_mask(ctx->plugin_storage, target);
    int chosen = -1;
    if (mask != 0) {
        int slots[6];
        int n = 0;
        for (int slot = 0; slot < 6; ++slot) {
            if (mask & (1 << slot)) {
                slots[n++] = slot;
            }
        }
        if (n > 0) {
            chosen = slots[sbs_rand() % n];   // 随机 1 个被放逐的精灵
        }
    }
    if (chosen < 0) {
        chosen = ctx->on_stage[target];   // 没有被放逐的 → 打当前精灵
    }
    for (Skills& sk : ctx->seerRobot[target].elfPets[chosen].skills) {
        if (sk.pp > 0) {
            sk.pp -= 1;
        }
    }
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册）──
SKILL_EFFECT(1533, effect_1533_low_damage_true);
SKILL_EFFECT(1534, effect_1534_exile_pp_drain);

// ════════════════════════════════════════════════════════════════════
// 2026-09-22 批次A：技能组收尾（八个效果，样板的出处见各节注释）
// ════════════════════════════════════════════════════════════════════

// ── 1532 神游·浩渺星河「对手不存在神印则攻击必定打出致命一击，对手每存在1层神印
//       则100%使对手随机1项技能PP值归零」（args_num=0）──
// ★ 运行时点 = **MOVE_RIGHT**（选择期路由，见 skills.cpp 的 selection_effects_ 表）：
//   必暴要写 ws.must_crit，而暴击掷点在 query_usage ①.5（ON_SKILL_HIT）——执行期桶
//   （SKILL_EFFECT 及以后）写必暴只惠及下次攻击（1315 的"本次消费"注释按状态序推演
//   不成立，且无场景断言过；场景 112 A3 锁定本次生效）。MOVE_RIGHT 桶执行时本轮
//   所有提交均已应用 → 神印发布槽为新鲜值。
// ★ PP 归零半边**自延后**到自身行动期：先/后手各挂一格（1531 同款去重形态），只有
//   自己那格会以自己为 mover 执行 → 官方"命中后归零"序（对手先手时其 PP 消耗在前），
//   且 miss 时 SKILL_EFFECT 桶整段不跑 → 归零不生效（命中挂靠语义）。
//   归零按 1419 同款（crawford_skills.cpp）：候选 = pp>0 的技能、不重复挑、直写 0。
// ★ "神印层数"读发布槽（qiling_runtime.h 15830000+target；0 = 不存在神印）。
EffectResult effect_1532_pp_zero_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    int stacks = qiling::shenyin_stacks(ctx->plugin_storage, target);
    ElfPet& opp = ctx->getPet(target);
    std::vector<int> candidates;
    for (std::size_t i = 0; i < opp.skills.size(); ++i) {
        if (opp.skills[i].pp > 0) {
            candidates.push_back(static_cast<int>(i));
        }
    }
    for (int n = 0; n < stacks && !candidates.empty(); ++n) {
        const int pick = sbs_rand() % candidates.size();
        opp.skills[static_cast<std::size_t>(candidates[pick])].pp = 0;
        candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(pick));
    }
    return EffectResult::kOk;
}

EffectResult effect_1532_shenyin_crit_or_pp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (qiling::shenyin_stacks(ctx->plugin_storage, target) > 0) {
        // 有神印 → 不必暴；PP 归零延到自己行动期（先/后手各挂一格，本回合一次性）
        for (const State st : {State::BATTLE_FIRST_SKILL_EFFECT,
                               State::BATTLE_SECOND_SKILL_EFFECT}) {
            Effect e(1532, 0, 0, 0, EffectArgs(std::vector<int>{self, target}),
                     &effect_1532_pp_zero_tick);
            auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/1,
                                                         ctx->roundCount);
            ce->source_id_ = 1532;
            ctx->register_skill_effect(st, self, std::move(ce));
        }
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[self] = true;   // 不存在神印 → 本次攻击必定致命一击
    return EffectResult::kOk;
}

// ── 1536 神之宣告「全属性+{0}，对手存在神印时强化效果翻倍」（args=[1]）
//       [SKILL_EFFECT；1397 全属性+条件翻倍同形，crawford_skills.cpp] ──
// "存在神印" = 对手（在场）身上层数 > 0（发布槽；1532 同一读口）。
EffectResult effect_1536_all_stats_shenyin_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int delta = args.int_args[2];
    if (self < 0 || self > 1 || target < 0 || target > 1 || delta <= 0) {
        return EffectResult::kOk;
    }
    int boost = delta;
    if (qiling::shenyin_stacks(ctx->plugin_storage, target) > 0) {
        boost = delta * 2;   // 对手存在神印 → 翻倍
    }
    all_stats_up(ctx, self, boost);
    return EffectResult::kOk;
}

// ── 600 真神信仰「若对手是{0}则造成伤害提升{1}%，若对手不是{0}，则有{2}%概率使
//       对手{3}」（args=[1, 100, 50, 7]；4399：雄性→增伤100%，非雄性→50%疲惫）
//       [SKILL_EFFECT] ──
// · {0} = DB monsters.gender 码（1=雄性；码表见 pet_factory 的 map_gender 注释——
//   2026-09-22 纠偏前该映射把 0/1 对调，本效果是 gender 的首个读者）。
// · 增伤写 ws.damage_add_pct（AMP 阶段读，mistike 612 同款）；疲惫走现代通道
//   apply_anomaly（936/22 同款），时长用随机档（官方模板未给回合数）。
EffectResult effect_600_gender_damage_or_fatigue(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int gender_code = args.int_args[2];
    const int dmg_pct = args.int_args[3];
    const int chance = args.int_args[4];
    const int status = args.int_args[5];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const Gender want = gender_code == 1   ? Gender::MALE
                        : gender_code == 2 ? Gender::FEMALE
                                           : Gender::NONE;
    if (ctx->getPet(target).gender == want) {
        ctx->ws.damage_add_pct[self] += static_cast<float>(dmg_pct);
        return EffectResult::kOk;
    }
    if (roll_percent(chance) && g_core->apply_anomaly && is_valid_abnormal_status_id(status)) {
        g_core->apply_anomaly(ctx, target, status, random_anomaly_duration(), self);
    }
    return EffectResult::kOk;
}

// ── 847 神之佑「为自身附加{0}点护盾」（args=[300]）[SKILL_EFFECT] ──
// 1545 同款落法（artemis_skills.cpp）：ShieldBank、魂印优先级、duration=0
// （官方未写持续 → 破盾前持续）。
EffectResult effect_847_gain_shield(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int amount = args.int_args[2];
    if (self < 0 || self > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    ctx->getPet(self).shield_bank_.add(ShieldBank::kSoulShieldPriority, amount,
                                       /*source_id=*/847, /*duration=*/0);
    return EffectResult::kOk;
}

// ── 1346 神之佑「{0}回合内每回合{1}%令自身全属性+{2}，未触发则令对手全属性-{3}」
//       （args=[3, 50, 1, 1]）[窗口 tick @ROUND_START] ──
// "每回合"（不带"使用技能"）→ ROUND_START tick（448 同款，kongyuan_skills.cpp）；
// 掷点失败分支 = 全属性弱化：逐项 stat_drop（各自查免弱/ATTACH_BAN、钳 -6，
// chaos_sorensen 的 all_stats_down 同款）。
EffectResult effect_1346_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int up = args.int_args[3];
    const int down = args.int_args[4];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (roll_percent(chance)) {
        all_stats_up(ctx, self, up);
    } else if (g_core->stat_drop && down > 0) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            g_core->stat_drop(ctx, target, i, down, /*actor=*/self);
        }
    }
    return EffectResult::kOk;
}

EffectResult effect_1346_roundly_stats(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int up = args.int_args[4];
    const int down = args.int_args[5];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 1346, rounds,
                          {self, 1 - self, chance, up, down}, &effect_1346_tick);
    return EffectResult::kOk;
}

// ── 1440 神之束缚「免疫下{0}次自身受到的异常状态」（args=[2]）[SKILL_EFFECT] ──
// 次数型异常免疫：duration_rounds=0 + counts（2087「镀」同款，kongyuan_zhishi.cpp；
// 命中一次扣一次、扣到 0 注销）。
EffectResult effect_1440_count_anomaly_immune(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int counts = args.int_args[2];
    if (self < 0 || self > 1 || counts <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(self, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/1440,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/counts, /*source_effect_id=*/1440);
    return EffectResult::kOk;
}

// ── 971 神之宣告「{0}回合内每回合使用技能吸取对手最大体力的1/{1}，吸取体力时若
//       自身体力低于最大体力的1/{2}则吸取效果翻倍」（args=[4, 3, 2]）
//       [窗口 tick @FIRST/SECOND_SKILL_EFFECT] ──
// "每回合**使用技能**" → 先/后手各一格 tick（1531/2077 同款，
// signature_true_damage.cpp）；吸取按**体力实际变化**回（1892 口径，
// 2026-09-20 用户拍板——护罩全吸/锁血场合 resolvedPink 会凭空回血）。
EffectResult effect_971_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core
        || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    const int low_denom = args.int_args[3];
    if (self < 0 || self > 1 || target < 0 || target > 1 || denom <= 0 || low_denom <= 0) {
        return EffectResult::kOk;
    }
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;   // 被控/嗑药回合不结算（"每回合使用技能"）
    }
    int drain = max_hp_of(ctx, target) / denom;   // 吸取对手最大体力的 1/{1}
    if (drain <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(self).hp * low_denom < max_hp_of(ctx, self)) {
        drain *= 2;   // 自身体力低于 1/{2} → 吸取翻倍
    }
    const int hp_before = ctx->getPet(target).hp;
    g_core->deal_pink_damage(ctx, target, drain, DamageKind::FIXED, self);
    const int received = hp_before - ctx->getPet(target).hp;
    if (received > 0 && g_core->heal_amount) {
        g_core->heal_amount(ctx, self, received);
    }
    return EffectResult::kOk;
}

EffectResult effect_971_round_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int low_denom = args.int_args[4];
    if (self < 0 || self > 1 || rounds <= 0 || denom <= 0 || low_denom <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> tick_args{self, 1 - self, denom, low_denom};
    register_round_effect(ctx, State::BATTLE_FIRST_SKILL_EFFECT, self, 971, rounds,
                          tick_args, &effect_971_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_SKILL_EFFECT, self, 971, rounds,
                          tick_args, &effect_971_tick);
    return EffectResult::kOk;
}

// ── 984 神游·浩渺星河「未击败对手则下回合自身所有技能先制+{0}」（args=[2]）
//       [AFTER_ACTION；1218「未击败对手则下{0}回合自身先制+{1}」同款，
//       kongyuan_skills.cpp——"未击败"在伤害落地后判（1307 归位口径）] ──
EffectResult effect_984_no_ko_next_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int bonus = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || bonus <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 已击败对手 → 不给先制
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, self, 984, /*rounds=*/1,
                                {self, 0, bonus}, &effect_move_right_priority_bonus);
    return EffectResult::kOk;
}

// ── 270210 神之宣告「2回合内自身回合类效果不能被消除」的**元神 n-1 特判**
//       （内部 id = 技能id×10，由 skills.cpp 按 (skill 27021, effect 821) 路由到此；
//         821 通用模板 25+ 技能共享，保持不动）──
// 语料《机制解析—免断》+ 作者顶置：元神的免断被官方特殊处理为 **n-1 回合**——
// 最准确描述是「1回合免断 ➕ 下1次免断」，而不是描述上的 2 回合（官方双标产物；
// "元神2回合免断和圣雷2回合免断比起来，会少1回合"）。
// 落点（两张票并存，各占显式 source_effect_id）：
//   · 窗口票：duration = n-1（本回合、延续至下回合开始的绝对窗口口径）；
//   · 次数票：counts = 1、无时限 =「下1次免断」——消费点在 break_round_effects
//     （窗口票覆盖期间不烧次数票，见 core 侧注释），窗口到期后由它接住下一断。
// 注：821 模板自带的"后出手时回合数+1"骑缝不再叠加——通用模板的尾巴本就是
//     "对手下1次技能出手流程结束后才结束"（变相次数免断，语料同段），特判的
//     次数票已按真形态表达，旧骑缝是早期近似，不重复计入。
EffectResult effect_270210_yuanshen_mianban(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (rounds > 1) {   // n-1 回合窗口（27021 的 n=2 → 1 回合）
        ctx->grant_immunity(self, ImmunityType::BREAK, coverage_all(), /*anomaly_mask=*/0,
                            /*duration_rounds=*/rounds - 1, /*source_id=*/270210,
                            /*soul_immunity=*/false, EffectScope::ON_STAGE,
                            /*counts=*/0, /*source_effect_id=*/270210);
    }
    ctx->grant_immunity(self, ImmunityType::BREAK, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/270211,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/1, /*source_effect_id=*/270211);
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册，追加批次）──
SKILL_EFFECT(1532, effect_1532_shenyin_crit_or_pp);       // 神游·浩渺星河：神印联动
SKILL_EFFECT(1536, effect_1536_all_stats_shenyin_double); // 神之宣告：全属性+神印翻倍
SKILL_EFFECT(600, effect_600_gender_damage_or_fatigue);   // 真神信仰：性别条件增伤/疲惫
SKILL_EFFECT(847, effect_847_gain_shield);                // 神之佑：附加护盾
SKILL_EFFECT(1346, effect_1346_roundly_stats);            // 神之佑：每回合概率全属性
SKILL_EFFECT(1440, effect_1440_count_anomaly_immune);     // 神之束缚：次数型免异常
SKILL_EFFECT(971, effect_971_round_drain);                // 神之宣告：逐回合吸取窗口
SKILL_EFFECT(984, effect_984_no_ko_next_priority);        // 神游·浩渺星河：未击败先制
SKILL_EFFECT(270210, effect_270210_yuanshen_mianban);     // 神之宣告：免断 n-1 元神特判

}  // namespace
