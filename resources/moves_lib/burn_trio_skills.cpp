#include <utils/sbs_rand.h>
// 烧伤三精灵技能组（2026-09-20）：希拓 4622 / 绮影罗心·织焰 4696 / 永烬劫炎·上古炎兽 4875
//
// 官方参数（moves.side_effect / effect_info，2026-09-20 查库；args 按各效果 args_num 顺序切分，
// SKILL_EFFECT 直挂的效果 [0]=owner、[1]=target 由引擎绑定、官方参数从 [2] 起；
// register_round_effect 系窗口 tick 的 int_args = 调用方自带的整包，[0] 自带 owner）：
//   ── 希拓 4622 ──
//   136  若Miss则自己恢复1/{0}体力 ................................ 灵魂之歌(22410) [4]（无效分支）
//   533  消除双方能力提升、下降状态 .............................. 心焰(34409) []
//   164  {0}回合内若受到攻击则有{1}%概率令对手{2} ................ 剑魄共鸣(28658) [4,100,2]
//   986  先出手时造成的攻击伤害额外提升{0}% ...................... 秘裁斩(37219) [30]
//   1187 {0}%令对手{1}，未触发则消除对手回合类效果 .............. 秘纹护体(28659) [100,2]
//   837  吸取对手{0}点固定体力，每次使用额外附加{1}点，最高{2}点 .. 秘纹护体 [300,100,500]
//   2059 自身免疫下{0}次受到的异常状态，每次免疫成功吸取对手{1}点  秘纹护体 [2,200]
//   786  随机附加{0}种异常状态 .................................. 秘符斩(37220) [1]
//   993  全属性+{0}，对手处于{1}状态时强化效果翻倍 .............. 剑意流转(28660) [1,2]
//   881  若自身处于能力下降状态则技能先制+3 .................... 神火剑阵(37221) []（选择期路由）
//   733  反转自身能力下降状态，反转成功则对手{0} ................ 神火剑阵 [20]
//   1604 {0}%恢复自身所有PP值{1}点 .............................. 神火剑阵 [100,2]
//   （2000/1287/2126 已有：烬灭神咒剑 37218；2/53 在本文件补实现）
//   ── 绮影罗心·织焰 4696 ──
//   636  消除{0}状态，消除成功则令对手{1} ........................ 逐月(37679) [0,2]
//   1148 随机吸取对手{0}-{1}点体力 .............................. 逐月 [150,250]
//   108  {0}回合内受到物理攻击时，有{1}%概率将对手烧伤 .......... 神火罩(21531) [3,100]
//   96   对手处于烧伤状态时，威力翻倍 .......................... 破魂枪(12935) []（★有效烧伤检测）
//   663  {0}回合内若对手使用攻击技能则{1}%使对手{2} .............. 祸尽狐言(28904) [3,100,2]
//   800  {0}回合内若对手使用属性技能，则下{1}回合自身受到的伤害不超过{2} 祸尽狐言 [3,2,300]
//   1168 全属性+{0}，双方任意一方处于异常状态则强化效果翻倍 ...... 和气生财(28905) [1]
//   615  {0}回合内使用技能恢复{1}点体力，体力低于1/{2}时恢复至满 .. 和气生财 [4,300,2]
//   1032 消除对手回合类效果，消除成功则下{0}回合令对手使用的攻击技能无效 乱云烟锁烬(37682·五) [1]
//   1845 对手每损失{0}点体力则额外附加{1}点固定伤害 .............. 乱云烟锁烬 [2,1]
//   1853 获得{0}点护罩，护罩消失时附加对手最大体力1/{1}的百分比伤害 乱云烟锁烬 [300,3]
//   1355 若打出致命一击则自身免疫下{0}次受到的异常状态 ........... 嚣尘两袖却(37692) [1]
//   2054 下{0}回合攻击技能打出致命一击后自身免疫下{1}次受到的异常  嚣尘两袖却 [2,1]
//   1115 解除自身能力下降状态，解除成功则令对手下{0}次使用的攻击技能无效 酣胆素心(28910) [1]
//   1378 全属性+{0}，{1}%概率强化效果翻倍，未触发则{2}回合内每回合{3}(六槽增量) 酣胆素心 [1,30,2,1,0,0,0,1,0]
//   619  下{0}回合令对手所有技能先制-{1} ........................ 酣胆素心 [2,2]
//   （776/916/936/1122/48/3/426/77/85/843 已有；其中 **916 走"文本解析器"路径**——
//     模板 "命中后{0}%使对手{1}，若未触发则恢复自身全部体力" 被 effect_unit_parser 支持，
//     不注册 C++ 函数（test/test.cpp 的冒烟用例以它为解析器样本守护，别抢注册））
//   ── 永烬劫炎·上古炎兽 4875 ──
//   1606 反转自身能力下降状态，反转成功则吸取对手最大体力的1/{0} .. 大赤殓(38282) [3]
//   1061 反转对手能力提升状态，成功则自身附加相同提升，失败则消除 .. 大赤殓 []
//   2240 解除自身所处的异常状态，解除成功则本次攻击威力提升{0}% ... 大赤殓/浩瀚一竿凭(37693) [100]
//   2418 技能无效时，获得等同于对手最大体力1/3的护盾、护罩 ........ 大赤殓 []（无效分支）
//   1605 {0}%令对手{1} .......................................... 燔薪照日(29275) [100,2]
//   783  {0}回合内自身能力提升状态被消除或吸取时附加对手最大体力1/{1}的百分比伤害 燔薪照日 [3,2]
//   1676 {0}回合内每回合使用技能{1}%令对手{2}，未触发则令自身全属性+{3} 燔薪照日 [3,100,36,1]（36=沸涌）
//   1674 {0}回合内每回合使用技能吸取对手最大体力的1/{1}，若对手未受到百分比伤害则额外附加{2}点真实伤害 荒烬涂天(29276) [4,3,280]
//   2198 双方任意一方处于护盾状态时{0}%令对手{1} ................. 荒烬涂天 [100,2]
//   2199 双方任意一方处于护罩状态时{0}%令对手{1} ................. 荒烬涂天 [100,22]
//   909  先出手时{0}%使对手{1}，未触发则附加{2}点固定伤害 ........ 忷魔涸焦血(38283) [100,2,280]
//   919  后出手时令对手下{0}回合属性技能无效 .................... 忷魔涸焦血 [2]
//   2105 未击败对手则令对方场下阵亡的首位精灵消逝 ................ 忷魔涸焦血 []
//   1959 消除对手回合类效果，消除成功{0}%令对手{1}，未触发则造成的攻击伤害翻倍 鏖火证阳炎(38284·五) [100,2]
//   2419 解除双方所处的异常状态，触发成功对手2回合内体力恢复效果减少其中烧伤以外异常状态个数的比例，且为自身附加回合数等于其中(对手)烧伤回合数合计的神惧劫炎 鏖火证阳炎 []
//   572  攻击对手时若对手处于{0}状态，则{1}%使对手{2} ............ 鏖火证阳炎 [2,100,22]（条件式：看虚拟烧伤）
//   1820 造成的攻击伤害若低于{0}则附加给对手{1}点真实伤害 ........ 鏖火证阳炎 [100,200]
//   （191/1014/843 已有；1268 在本文件补实现）
//
// 关键口径（2026-09-20）：
//   ★ 涉及"处于烧伤"的判据按**描述措辞**分两类（语料《机制解析—永烬劫炎·上古炎兽》）：
//     ① **检测/条件式**（"处于烧伤状态则…"/"若对手处于烧伤则…"）→ is_burn_effective
//        （虚拟烧伤也算）：96 破魂枪威力翻倍、993 剑意流转翻倍、希拓/织焰魂印、
//        **572 焚烬**（文章明确"配合魂印的视为处于烧伤就可以触发"）；
//     ② **状态转化式**（"将对手所处的烧伤转化为…"，影中艳 4671 族）→ is_real_burn
//        （槽里得真有烧伤可转）。
//     ③ 虚拟烧伤还有**伴生效果**（1/8 真伤 + 威力减半），已接进引擎 battleFsm，不在本文件。
//   ★ 2059 的"每次免疫成功"走 EVENT_ANOMALY_IMMUNED（本线新增事件）。
//   ★ 2105 的"未击败"在效果时点判对手在场宠是否还活着（1307 同款），消逝首位 = 槽序
//     最小的、已阵亡且未被消逝的场下宠。
//   ★ 2419 的两个"其中"取对手侧读法：减免比例 = 对手被解除异常中非烧伤占比；神惧劫炎
//     回合 = 对手被解除烧伤的剩余回合合计（口语歧义记档，待实测翻案只需改一处）。
//   ★ "先/后出手"判据：SKILL_EFFECT 时点直接用 acting_side_is_first（currentState 在
//     FIRST/SECOND 段）；AFTER_ACTION/ATTACK_DAMAGE 窗口 tick 用 preemptive_right 推
//     本桶行动方（与 battleFsm resolve_first_mover_id 同源）。
//   ★ 919 守卫反置修正（2026-10-01，小修批 X1）：落地时守卫写成了"先手才挂"
//     （!acting_side_is_first 早退 = 先手继续），与官方"**后出手**时令对手下{0}回合
//     属性技能无效"相反，40 载体全受影响（R1 夜跑 DB 核对时留票）。判据一刀翻转为
//     acting_side_is_first 早退（同库 d5_D2.cpp 明写"919 同款判据"的极性即正）。
//     场景 880 双向断言：后手才挂、先手不挂。
#include "common.h"
#include <effects/burn_perception.h>
#include "../burn_trio_runtime.h"

namespace {

constexpr int kBurnId = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kBoilId = static_cast<int>(AbnormalStatusId::Boil);

// 场下/在场通用最大体力（与 soul_lib 的 resolve_pet_max_hp 同义；跨 .so 不共享，本地一份，
// tianqi_dijun_skills.cpp 的 pet_max_hp 同款）。
inline int pet_max_hp_of(const ElfPet& pet) {
    int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = pet.numericalProperties[NumericalPropertyIndex::HP];
    }
    if (max_hp <= 0) {
        max_hp = pet.hp;
    }
    return max_hp;
}

// 目标槽里是否真的有某异常（真状态；转化类门槛用）。
inline bool has_real_status(const BattleContext* ctx, int side, int status_id) {
    if (!ctx || side < 0 || side > 1 || status_id < 0
        || status_id > kOfficialAbnormalStatusMaxId) {
        return false;
    }
    return ctx->roundCount < ctx->abnormal_status_end_round[side][status_id];
}

// 现代施加的"落地"判定：SUCCESS / 替换控场 / 同种延长都算触发；CONVERTED（被转化成
// 别的异常）与各挡下属"未触发"。
bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// AFTER_ACTION 窗口 tick 里判"本桶行动方"（preemptive_right 推，与 FSM 同源；
// 只读 preemptive_right/currentState，const 安全）。
inline int acting_side_of_bucket(const BattleContext* ctx) {
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    return ctx->currentState == State::BATTLE_SECOND_AFTER_ACTION ? 1 - first : first;
}

bool either_side_has_any_anomaly(BattleContext* ctx) {
    if (!ctx) {
        return false;
    }
    for (int side = 0; side < 2; ++side) {
        for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
            if (ctx->roundCount < ctx->abnormal_status_end_round[side][id]) {
                return true;
            }
        }
    }
    return false;
}

// ════════════════════════════ 希拓 4622 ════════════════════════════

// ── 136 —— 若Miss则自己恢复1/{0}体力（SKILL_INVALID 分支；盔挡下同算，口径同 2126）──
EffectResult effect_136_miss_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    const int max_hp = pet_max_hp_of(self);
    if (max_hp - self.hp > 0) {
        g_core->heal_amount(ctx, owner, max_hp / denom);
    }
    return EffectResult::kOk;
}

// ── 533 —— 消除双方能力提升、下降状态 ──
EffectResult effect_533_clear_both_stats(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    for (int side = 0; side < 2; ++side) {
        if (g_core->clear_stat_boosts) {
            g_core->clear_stat_boosts(ctx, side);
        }
        if (g_core->clear_stat_drops) {
            g_core->clear_stat_drops(ctx, side);
        }
    }
    return EffectResult::kOk;
}

// ── 164 —— {0}回合内若受到攻击则有{1}%概率令对手{2}（受击监听，时长 = 窗口）──
EffectResult effect_164_take_hit_status(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int status = args.int_args[4];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, rounds,
        /*once=*/false,
        [owner, chance, status](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || !roll_skill_percent(*c, 164, 0, owner, 1 - owner, chance)) {
                return;
            }
            if (g_core && g_core->apply_anomaly) {
                g_core->apply_anomaly(c, 1 - owner, status, random_anomaly_duration(), owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 986 —— 先出手时造成的攻击伤害额外提升{0}%（"额外"= 非通用乘法通道 AMP_EXTRA）──
EffectResult effect_986_first_amp_extra(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || pct == 0 || !acting_side_is_first(ctx)) {
        return EffectResult::kOk;
    }
    int(&slots)[4] = ctx->ws.damage_add_extra_mul[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = pct;
            break;
        }
    }
    return EffectResult::kOk;
}

// ── 1187 —— {0}%令对手{1}，未触发则消除对手回合类效果 ──
EffectResult effect_1187_status_or_break(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int status = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    bool landed = false;
    if (roll_skill_percent(*ctx, 1187, 0, owner, target, chance)) {
        landed = anomaly_landed(g_core->apply_anomaly(ctx, target, status,
                                                      random_anomaly_duration(), owner));
    }
    if (!landed && g_core->break_round_effects) {
        g_core->break_round_effects(ctx, target);
    }
    return EffectResult::kOk;
}

// ── 837 —— 吸取对手{0}点固定体力，每次使用额外附加{1}点，最高{2}点 ──
EffectResult effect_837_growing_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int base = args.int_args[2];
    const int growth = args.int_args[3];
    const int cap = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || base <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    ElfPet& foe = ctx->getPet(target);
    if (foe.hp <= 0) {
        return EffectResult::kOk;
    }
    // 递增档存 soulmark_storage（下场保留）：首次 0、每用一次 +growth（封顶 cap-base）。
    const int bonus = miwen_drain_bonus(self);
    miwen_set_drain_bonus(self, std::min(bonus + growth, std::max(0, cap - base)));
    const int amount = std::min(base + bonus, cap);
    const int drained = std::min(amount, foe.hp);
    foe.hp -= drained;
    g_core->heal_amount(ctx, owner, drained);
    return EffectResult::kOk;
}

// ── 2059 —— 免疫下{0}次异常，每次免疫成功吸取对手{1}点体力 ──
// 免疫成功 = EVENT_ANOMALY_IMMUNED（本线新增事件）。剩余次数自记账（plugin_storage）：
// 事件不带"是哪张票挡的"，同侧若还有**别的**异常免疫来源先挡，本次也会记账消耗——
// 极小偏差记档（2059 的票是 Modern 次免，与其并存的同类票场景罕见）。
//
// ⚠️ **不可传承**（用户 2026-09-20 拍板）：秘纹护体的次免**不会**传承给下一只上场精灵。
//    · 票的生命周期 = `EffectScope::ON_STAGE` + `source_slot=-1` → 宿主一侧**任何**换宠
//      都会 `RuleCenter::clear_on_stage` 清掉它（下场即失效，重上场要重新使用技能）；
//    · **覆盖键独占**：`source_effect_id=2059` —— RuleCenter 的覆盖键 =
//      (source_owner, **source_effect_id**, subtype)，所以本票与"可传承的可视化次免"
//      （将来实现时会有自己的 effect_id）**不会互相覆盖**，也不会被它清掉。
//    · ⚠️ 引擎目前**没有**"可传承次免"的表示（docs/02-效果系统 官方机制理解与引擎缺口对照.md
//      §二 idx=375：可视化次免默认可传承给下只精灵）——该缺口落地时，**本票必须保持
//      不可传承**（on_stage 语义），别被顺手改成可传承。
EffectResult effect_2059_immune_then_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int counts = args.int_args[2];
    const int drain = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || counts <= 0) {
        return EffectResult::kOk;
    }
    ctx->rule_center_.grant_immune(
        owner, static_cast<int>(ImmunityType::ANOMALY), coverage_all(), /*anomaly_mask=*/0,
        /*duration_rounds=*/0, ctx->roundCount, /*source_id=*/0, /*soul=*/false,
        EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/counts,
        /*source_effect_id=*/2059);
    // 记账（剩余次数）挂**宿主 pet 的 on_stage_storage** —— 与票同生命周期：换宠时票被
    // `clear_on_stage` 清掉、记账也随 pet 的 on_stage_storage 一起清零，于是"票没了但
    // 残留记账继续吃别人免疫票的事件"这条错路不会发生（重新上场再用技能才重新记账）。
    miwen_set_immune_remain(ctx->getPet(owner), counts);
    // 监听只挂一次（per side，plugin_storage 旗；WatcherScope::TEAM 跨切换存活）——
    // 每次使用都挂会累积监听器；这里由记账值驱动，记账为 0 时自然 no-op。
    const int watcher_key = 205901 + owner;
    if (ctx->plugin_storage.count(watcher_key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[watcher_key] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_ANOMALY_IMMUNED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, target, drain](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner) {
                return;
            }
            ElfPet& host = c->getPet(owner);
            const int remain = miwen_immune_remain(host);
            if (remain <= 0) {
                return;   // 无票（未使用过 / 已扣完 / 已下场被清）
            }
            miwen_set_immune_remain(host, remain - 1);
            ElfPet& foe = c->getPet(target);
            const int drained = std::min(drain, foe.hp);
            if (drained > 0) {
                foe.hp -= drained;
                if (g_core && g_core->heal_amount) {
                    g_core->heal_amount(c, owner, drained);
                }
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 786 —— 随机附加{0}种异常状态（池解析 ⊕ ANOMALY_POOL_MOD 票，原语内做）──
EffectResult effect_786_random_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->attach_random_anomalies) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int count = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    g_core->attach_random_anomalies(ctx, target, anomaly_pool_all(), count,
                                    /*probability_pct=*/100, owner,
                                    ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 993 —— 全属性+{0}，对手处于{1}状态时强化效果翻倍（烧伤检测 = 有效烧伤）──
EffectResult effect_993_all_stats_burn_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int delta = args.int_args[2];
    const int status = args.int_args[3];
    if (owner < 0 || owner > 1 || delta == 0) {
        return EffectResult::kOk;
    }
    all_stats_up(ctx, owner, delta);
    if (status == kBurnId && is_burn_effective(ctx, 1 - owner)) {
        all_stats_up(ctx, owner, delta);
    }
    return EffectResult::kOk;
}

// ── 881 —— 若自身处于能力下降状态则技能先制+3（选择期路由到 MOVE_RIGHT 桶）──
EffectResult effect_881_weak_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] < 0) {
            ctx->ws.preemptive_level[owner] += 3;
            return EffectResult::kOk;
        }
    }
    return EffectResult::kOk;
}

// ── 733 —— 反转自身能力下降状态，反转成功则对手{0} ──
EffectResult effect_733_reversal_then_status(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_reversal
        || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int status = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, owner) == StatReversalResult::REVERSED) {
        g_core->apply_anomaly(ctx, target, status, random_anomaly_duration(), owner);
    }
    return EffectResult::kOk;
}

// ── 1604 —— {0}%恢复自身所有PP值{1}点 ──
EffectResult effect_1604_restore_pp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int chance = args.int_args[2];
    const int amount = args.int_args[3];
    if (owner < 0 || owner > 1 || !roll_skill_percent(*ctx, 1604, 0, owner, owner, chance)) {
        return EffectResult::kOk;
    }
    for (Skills& s : ctx->getPet(owner).skills) {
        if (s.pp >= 0) {
            s.pp = std::min(s.maxPP, s.pp + amount);
        }
    }
    return EffectResult::kOk;
}

// ════════════════════════════ 绮影罗心·织焰 4696 ════════════════════════════

// ── 636 —— 消除{0}状态，消除成功则令对手{1}（{0}=0 → 能力提升状态）──
EffectResult effect_636_clear_boost_then_status(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int status = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts && g_core->clear_stat_boosts(ctx, target) > 0) {
        g_core->apply_anomaly(ctx, target, status, random_anomaly_duration(), owner);
    }
    return EffectResult::kOk;
}

// ── 1148 —— 随机吸取对手{0}-{1}点体力 ──
EffectResult effect_1148_random_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int low = args.int_args[2];
    const int high = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || low <= 0 || high < low) {
        return EffectResult::kOk;
    }
    ElfPet& foe = ctx->getPet(target);
    if (foe.hp <= 0) {
        return EffectResult::kOk;
    }
    const int amount = choose_skill_range(*ctx, 1148, 0, owner, target, low, high, ChanceSingleton::SkipDraw);
    const int drained = std::min(amount, foe.hp);
    foe.hp -= drained;
    g_core->heal_amount(ctx, owner, drained);
    return EffectResult::kOk;
}

// ── 108 —— {0}回合内受到物理攻击时，有{1}%概率将对手烧伤 ──
// "物理攻击"取红伤口径：只在攻击伤害时点（FIRST/SECOND_ATTACK_DAMAGE）响应——
// 属性直伤/粉伤/真伤不算（事件 state 记 emit 当时点）。
EffectResult effect_108_hit_by_attack_burn(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, rounds,
        /*once=*/false,
        [owner, chance](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner) {
                return;
            }
            const bool red_attack =
                ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
            if (!red_attack || !roll_skill_percent(*c, 108, 0, owner, 1 - owner, chance)) {
                return;
            }
            if (g_core && g_core->apply_anomaly) {
                g_core->apply_anomaly(c, 1 - owner, kBurnId, random_anomaly_duration(), owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 96 —— 对手处于烧伤状态时，威力翻倍（★有效烧伤检测：虚拟烧伤也算）──
// "威力翻倍"是**变威力**族 → 与 40/30（先手/后手威力2倍）同款：改**视图威力**（`=` 打
// rewritten 标记，ATTACK_DAMAGE 阶段按新威力重算）。⚠️ 与烧伤减半的次序：引擎先按面板
// materialize(power/2)、本效果再 ×2 → 乘法可交换，(100×2)/2 == (100/2)×2 = 100，无歧义。
EffectResult effect_96_burn_power_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!is_burn_effective(ctx, 1 - owner)) {
        return EffectResult::kOk;
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * 2;
    }
    return EffectResult::kOk;
}

// ── 663 —— {0}回合内若对手使用攻击技能则{1}%使对手{2}（窗口 tick：args=[owner,chance,status]）──
EffectResult effect_663_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int chance = args.int_args[1];
    const int status = args.int_args[2];
    const int opp = 1 - owner;
    if (acting_side_of_bucket(ctx) != opp || !executed_skill_is_attack(ctx, opp)
        || !roll_skill_percent(*ctx, 663, 0, owner, opp, chance)) {
        return EffectResult::kOk;
    }
    g_core->apply_anomaly(ctx, opp, status, random_anomaly_duration(), owner);
    return EffectResult::kOk;
}

EffectResult effect_663_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> tick_args{owner, args.int_args[3], args.int_args[4]};
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, owner, 663, rounds, tick_args,
                          &effect_663_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, owner, 663, rounds, tick_args,
                          &effect_663_tick);
    return EffectResult::kOk;
}

// ── 800 —— {0}回合内若对手使用属性技能，则下{1}回合自身受到的伤害不超过{2}（窗口 tick）──
EffectResult effect_800_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int next_rounds = args.int_args[1];
    const int cap = args.int_args[2];
    const int opp = 1 - owner;
    if (acting_side_of_bucket(ctx) != opp || next_rounds <= 0 || cap <= 0
        || !executed_skill_this_round(ctx, opp)) {
        return EffectResult::kOk;
    }
    if (executed_skill_is_attack(ctx, opp)) {
        return EffectResult::kOk;   // 本次用的不是属性技能（实际物化类型，判据统一 2026-10-03）
    }
    // 下{1}回合 CAP（显式绝对窗口；重复触发叠同款条目，效果幂等）。
    const int start = ctx->roundCount + 1;
    const int expire = start + next_rounds;
    ctx->register_damage_effect(
        DamagePhase::CAP, owner, DamageEffectCategory::MITIGATE,
        [owner, start, expire, cap](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            const int attacker = d.attackerId;
            if (attacker >= 0 && attacker <= 1
                && c->ws.attack_credential[attacker].ignore_damage_limit) {
                return;   // 697「无视伤害限制」凭证穿透（125 同款）
            }
            if (d.final > cap) {
                d.final = cap;
            }
        });
    return EffectResult::kOk;
}

EffectResult effect_800_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> tick_args{owner, args.int_args[3], args.int_args[4]};
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, owner, 800, rounds, tick_args,
                          &effect_800_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, owner, 800, rounds, tick_args,
                          &effect_800_tick);
    return EffectResult::kOk;
}

// ── 1168 —— 全属性+{0}，双方任意一方处于异常状态则强化效果翻倍 ──
EffectResult effect_1168_all_stats_anomaly_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int delta = args.int_args[2];
    if (owner < 0 || owner > 1 || delta == 0) {
        return EffectResult::kOk;
    }
    all_stats_up(ctx, owner, delta);
    if (either_side_has_any_anomaly(ctx)) {
        all_stats_up(ctx, owner, delta);
    }
    return EffectResult::kOk;
}

// ── 615 —— {0}回合内使用技能恢复{1}点体力，体力低于1/{2}时恢复至满（窗口 tick）──
EffectResult effect_615_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int amount = args.int_args[1];
    const int denom = args.int_args[2];
    if (acting_side_of_bucket(ctx) != owner || !used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    const int max_hp = pet_max_hp_of(self);
    const int lost = max_hp - self.hp;
    if (lost <= 0) {
        return EffectResult::kOk;
    }
    if (self.hp * denom < max_hp) {
        g_core->heal_amount(ctx, owner, lost);   // 低于 1/{denom}：恢复至满
    } else {
        g_core->heal_amount(ctx, owner, std::min(amount, lost));
    }
    return EffectResult::kOk;
}

EffectResult effect_615_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> tick_args{owner, args.int_args[3], args.int_args[4]};
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, owner, 615, rounds, tick_args,
                          &effect_615_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, owner, 615, rounds, tick_args,
                          &effect_615_tick);
    return EffectResult::kOk;
}

// ── 1032 —— 消除对手回合类效果，消除成功则下{0}回合令对手使用的攻击技能无效 ──
EffectResult effect_1032_break_then_seal_attack(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (!ctx->rule_center_.has_round_type(target)) {
        return EffectResult::kOk;   // 消除没成功 → 不封
    }
    if (g_core->break_round_effects) {
        g_core->break_round_effects(ctx, target);
    }
    if (g_core->seal_skill) {
        g_core->seal_skill(ctx, /*source=*/owner, /*target=*/target, /*effect_id=*/1032,
                           /*attribute=*/false, /*attack=*/true, /*count=*/0,
                           /*duration_rounds=*/rounds, /*penetrable=*/true,
                           /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                           /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

// ── 1845 —— 对手每损失{0}点体力则额外附加{1}点固定伤害（读管线后最终伤害，1306 同款）──
EffectResult effect_1845_loss_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int per = args.int_args[2];
    const int extra = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || per <= 0) {
        return EffectResult::kOk;
    }
    const int dealt = ctx->resolvedDamage.final;
    if (dealt <= 0) {
        return EffectResult::kOk;
    }
    const int amount = dealt / per * extra;
    if (amount > 0) {
        g_core->fixed_damage(ctx, target, amount);
    }
    return EffectResult::kOk;
}

// ── 1853 —— 获得{0}点护罩，护罩消失时附加对手最大体力1/{1}的百分比伤害 ──
EffectResult effect_1853_hood_with_burst(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int value = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || value <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    ctx->getPet(owner).hood_bank_.add(/*priority=*/0, value, /*source_id=*/1853,
                                      /*duration=*/0);
    // 爆点旗（+owner）：施放时清 0、破罩时置 1——同场重复施放重新武装。
    // 监听只挂一次；命中条件 = 本 owner 的 1853 罩已不在 + 旗在 0。
    const int watcher_key = kZhiyanHoodWatcherKey + owner;
    const int fired_key = kZhiyanHoodBreakFiredKey + owner;
    ctx->plugin_storage[fired_key] = 0;
    if (!ctx->plugin_storage.count(watcher_key)) {
        ctx->plugin_storage[watcher_key] = 1;
        ctx->event_center_.register_watcher(
            EventType::EVENT_SHIELD_BROKEN, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, target, denom, fired_key](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.target != owner) {
                    return;
                }
                auto it = c->plugin_storage.find(fired_key);
                if (it == c->plugin_storage.end() || any_int_or(it->second) != 0) {
                    return;   // 没有在场的 1853 罩 / 已爆过
                }
                if (c->getPet(owner).hood_bank_.quantity_of(1853) > 0) {
                    return;   // 破的是别人的罩，1853 的罩还在
                }
                it->second = 1;
                if (g_core && g_core->deal_pink_damage) {
                    g_core->deal_pink_damage(c, target,
                                             pet_max_hp_of(c->getPet(target)) / denom,
                                             DamageKind::PERCENT, owner);
                }
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 1355 —— 若打出致命一击则自身免疫下{0}次受到的异常状态 ──
EffectResult effect_1355_crit_immune(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int counts = args.int_args[2];
    if (owner < 0 || owner > 1 || counts <= 0) {
        return EffectResult::kOk;
    }
    DamageSnapshot& d = ctx->resolvedDamage;
    if (!d.isRed || !d.isCrit) {
        return EffectResult::kOk;
    }
    ctx->rule_center_.grant_immune(
        owner, static_cast<int>(ImmunityType::ANOMALY), coverage_all(), /*anomaly_mask=*/0,
        /*duration_rounds=*/0, ctx->roundCount, /*source_id=*/0, /*soul=*/false,
        EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/counts,
        /*source_effect_id=*/1355);
    return EffectResult::kOk;
}

// ── 2054 —— 下{0}回合攻击技能打出致命一击后自身免疫下{1}次受到的异常（窗口 tick）──
EffectResult effect_2054_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int counts = args.int_args[1];
    if (owner < 0 || owner > 1 || counts <= 0) {
        return EffectResult::kOk;
    }
    DamageSnapshot& d = ctx->resolvedDamage;
    if (d.attackerId != owner || !d.isRed || !d.isCrit) {
        return EffectResult::kOk;
    }
    ctx->rule_center_.grant_immune(
        owner, static_cast<int>(ImmunityType::ANOMALY), coverage_all(), /*anomaly_mask=*/0,
        /*duration_rounds=*/0, ctx->roundCount, /*source_id=*/0, /*soul=*/false,
        EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/counts,
        /*source_effect_id=*/2054);
    return EffectResult::kOk;
}

EffectResult effect_2054_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int counts = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> tick_args{owner, counts};
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_ATTACK_DAMAGE, owner, 2054, rounds,
                                tick_args, &effect_2054_tick);
    register_next_rounds_effect(ctx, State::BATTLE_SECOND_ATTACK_DAMAGE, owner, 2054, rounds,
                                tick_args, &effect_2054_tick);
    return EffectResult::kOk;
}

// ── 1115 —— 解除自身能力下降状态，解除成功则令对手下{0}次使用的攻击技能无效 ──
EffectResult effect_1115_clear_drops_then_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int counts = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || counts <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core->clear_stat_drops || g_core->clear_stat_drops(ctx, owner) <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->seal_skill) {
        g_core->seal_skill(ctx, /*source=*/owner, /*target=*/target, /*effect_id=*/1115,
                           /*attribute=*/false, /*attack=*/true, /*count=*/counts,
                           /*duration_rounds=*/0, /*penetrable=*/true,
                           /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                           /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

// ── 1378 —— 全属性+{0}，{1}%概率翻倍，未触发则{2}回合内每回合按六槽增量补（窗口 tick）──
// tick args=[owner, d0..d5]（六项能力等级增量，index = kAbilityLevelIndex）。
EffectResult effect_1378_deltas_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 7 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[1 + i];
        if (delta != 0) {
            g_core->stat_change(ctx, owner, i, delta);
        }
    }
    return EffectResult::kOk;
}

EffectResult effect_1378_all_stats_chance_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 11) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int delta = args.int_args[2];
    const int chance = args.int_args[3];
    const int rounds = args.int_args[4];
    if (owner < 0 || owner > 1 || delta == 0 || rounds <= 0) {
        return EffectResult::kOk;
    }
    all_stats_up(ctx, owner, delta);
    if (roll_skill_percent(*ctx, 1378, 0, owner, owner, chance)) {
        all_stats_up(ctx, owner, delta);
        return EffectResult::kOk;
    }
    const std::vector<int> tick_args{owner, args.int_args[5], args.int_args[6], args.int_args[7],
                                     args.int_args[8], args.int_args[9], args.int_args[10]};
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1378, rounds, tick_args,
                                &effect_1378_deltas_tick);
    return EffectResult::kOk;
}

// ── 619 —— 下{0}回合令对手所有技能先制-{1}（NextRounds，MOVE_RIGHT 桶压对手先制）──
// tick args=[owner, down, start, expire]。
EffectResult effect_619_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int down = args.int_args[1];
    const int start = args.int_args[2];
    const int expire = args.int_args[3];
    if (owner < 0 || owner > 1 || down == 0) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount < start || ctx->roundCount >= expire) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[1 - owner] -= down;
    return EffectResult::kOk;
}

EffectResult effect_619_opp_priority_down(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int down = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || down == 0) {
        return EffectResult::kOk;
    }
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::NextRounds);
    const std::vector<int> tick_args{owner, down, start, start + rounds};
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 619, rounds,
                                tick_args, &effect_619_tick);
    return EffectResult::kOk;
}

// ════════════════════════════ 永烬劫炎·上古炎兽 4875 ════════════════════════════

// ── 1606 —— 反转自身能力下降状态，反转成功则吸取对手最大体力的1/{0} ──
EffectResult effect_1606_reversal_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_reversal
        || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, owner) == StatReversalResult::REVERSED) {
        g_core->drain_hp(ctx, owner, target, denom);
    }
    return EffectResult::kOk;
}

// ── 1061 —— 反转对手能力提升：成功则自身附加相同提升，失败则消除对手能力提升 ──
EffectResult effect_1061_boost_reversal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->stat_boost_reversal) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    int boosted[6]{};
    for (int i = 0; i < 6; ++i) {
        boosted[i] = std::max(0, ctx->ability_levels[target][i]);
    }
    const StatReversalResult r = g_core->stat_boost_reversal(ctx, target);
    if (r == StatReversalResult::REVERSED && g_core->stat_change) {
        for (int i = 0; i < 6; ++i) {
            if (boosted[i] > 0) {
                g_core->stat_change(ctx, owner, i, boosted[i]);
            }
        }
        return EffectResult::kOk;
    }
    // 反转失败（免弱挡下 / 无提升可反）→ 消除对手能力提升（查 STAT_CLEAR 的真原语）。
    if (g_core->clear_stat_boosts) {
        g_core->clear_stat_boosts(ctx, target);
    }
    return EffectResult::kOk;
}

// ── 2240 —— 解除自身所处的异常状态，解除成功则本次攻击威力提升{0}% ──
EffectResult effect_2240_dispel_amp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->dispel_active_anomalies) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || pct == 0) {
        return EffectResult::kOk;
    }
    if (g_core->dispel_active_anomalies(ctx, owner) > 0) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);
    }
    return EffectResult::kOk;
}

// ── 2418 —— 技能无效时，获得等同于对手最大体力1/3的护盾、护罩（SKILL_INVALID 分支）──
EffectResult effect_2418_invalid_shield_hood(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int value = pet_max_hp_of(ctx->getPet(1 - owner)) / 3;
    if (value <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    self.shield_bank_.add(/*priority=*/0, value, /*source_id=*/2418, /*duration=*/0);
    self.hood_bank_.add(/*priority=*/0, value, /*source_id=*/2418, /*duration=*/0);
    return EffectResult::kOk;
}

// ── 1605 —— {0}%令对手{1} ──
EffectResult effect_1605_chance_status(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int status = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1
        || !roll_skill_percent(*ctx, 1605, 0, owner, target, chance)) {
        return EffectResult::kOk;
    }
    g_core->apply_anomaly(ctx, target, status, random_anomaly_duration(), owner);
    return EffectResult::kOk;
}

// ── 783 —— {0}回合内自身能力提升状态被消除或吸取时附加对手最大体力1/{1}的百分比伤害 ──
// 检测点 = EVENT_STAT_CHANGED（所有改等级路径的唯一落点）：本方正等级被压低即命中
// （消除/吸取/反转全走 write_ability_level）。
EffectResult effect_783_boost_lost_pink(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_CHANGED, owner, ctx->roundCount, rounds,
        /*once=*/false,
        [owner, target, denom](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner) {
                return;
            }
            const int before = ev.level_after - ev.level_delta;
            if (before <= 0 || ev.level_after >= before) {
                return;   // 只认"正等级被压低"
            }
            if (g_core && g_core->deal_pink_damage) {
                g_core->deal_pink_damage(c, target,
                                         pet_max_hp_of(c->getPet(target)) / denom,
                                         DamageKind::PERCENT, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1676 —— {0}回合内每回合使用技能{1}%令对手{2}，未触发则令自身全属性+{3}（窗口 tick）──
EffectResult effect_1676_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int chance = args.int_args[1];
    const int status = args.int_args[2];
    const int up = args.int_args[3];
    if (acting_side_of_bucket(ctx) != owner || !used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;
    }
    bool landed = false;
    if (roll_skill_percent(*ctx, 1676, 0, owner, 1 - owner, chance)) {
        landed = anomaly_landed(g_core->apply_anomaly(ctx, 1 - owner, status,
                                                      random_anomaly_duration(), owner));
    }
    if (!landed && up != 0) {
        all_stats_up(ctx, owner, up);
    }
    return EffectResult::kOk;
}

EffectResult effect_1676_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> tick_args{owner, args.int_args[3], args.int_args[4], args.int_args[5]};
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, owner, 1676, rounds, tick_args,
                          &effect_1676_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, owner, 1676, rounds, tick_args,
                          &effect_1676_tick);
    return EffectResult::kOk;
}

// ── 1674 —— {0}回合内每回合使用技能吸取对手最大体力的1/{1}，未受伤则+{2}真伤（窗口 tick）──
EffectResult effect_1674_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->drain_hp
        || !g_core->deal_true_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int denom = args.int_args[1];
    const int true_amount = args.int_args[2];
    const int opp = 1 - owner;
    if (acting_side_of_bucket(ctx) != owner || !used_skill_this_round(ctx, owner) || denom <= 0) {
        return EffectResult::kOk;
    }
    const int before = ctx->getPet(opp).hp;
    g_core->drain_hp(ctx, owner, opp, denom);
    if (ctx->getPet(opp).hp == before) {
        g_core->deal_true_damage(ctx, opp, true_amount, owner);   // 一点没吸进去 → 真伤补刀
    }
    return EffectResult::kOk;
}

EffectResult effect_1674_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> tick_args{owner, args.int_args[3], args.int_args[4]};
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, owner, 1674, rounds, tick_args,
                          &effect_1674_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, owner, 1674, rounds, tick_args,
                          &effect_1674_tick);
    return EffectResult::kOk;
}

// ── 2198 —— 双方任意一方处于护盾状态时{0}%令对手{1} ──
EffectResult effect_2198_shield_status(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int status = args.int_args[3];
    // Keep the legacy roll before the shield check.
    if (owner < 0 || owner > 1 || target < 0 || target > 1
        || !roll_skill_percent(*ctx, 2198, 0, owner, target, chance)) {
        return EffectResult::kOk;
    }
    const bool any_shield = !ctx->getPet(0).shield_bank_.empty()
                         || !ctx->getPet(1).shield_bank_.empty();
    if (!any_shield) {
        return EffectResult::kOk;
    }
    g_core->apply_anomaly(ctx, target, status, random_anomaly_duration(), owner);
    return EffectResult::kOk;
}

// ── 2199 —— 双方任意一方处于护罩状态时{0}%令对手{1}（{1}=22 焚烬，直接施加非转化）──
EffectResult effect_2199_hood_status(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int status = args.int_args[3];
    // Keep the legacy roll before the hood check.
    if (owner < 0 || owner > 1 || target < 0 || target > 1
        || !roll_skill_percent(*ctx, 2199, 0, owner, target, chance)) {
        return EffectResult::kOk;
    }
    const bool any_hood = !ctx->getPet(0).hood_bank_.empty()
                       || !ctx->getPet(1).hood_bank_.empty();
    if (!any_hood) {
        return EffectResult::kOk;
    }
    g_core->apply_anomaly(ctx, target, status, random_anomaly_duration(), owner);
    return EffectResult::kOk;
}

// ── 909 —— 先出手时{0}%使对手{1}，未触发则附加{2}点固定伤害 ──
EffectResult effect_909_first_status_or_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int status = args.int_args[3];
    const int fixed = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || !acting_side_is_first(ctx)) {
        return EffectResult::kOk;
    }
    bool landed = false;
    if (roll_skill_percent(*ctx, 909, 0, owner, target, chance) && g_core->apply_anomaly) {
        landed = anomaly_landed(g_core->apply_anomaly(ctx, target, status,
                                                      random_anomaly_duration(), owner));
    }
    if (!landed && fixed > 0 && g_core->fixed_damage) {
        g_core->fixed_damage(ctx, target, fixed);
    }
    return EffectResult::kOk;
}

// ── 919 —— 后出手时令对手下{0}回合属性技能无效 ──
EffectResult effect_919_second_seal_attribute(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || rounds <= 0
        || acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 本方是先手 → 不是"后出手"（2026-10-01 修正反置守卫）
    }
    g_core->seal_skill(ctx, /*source=*/owner, /*target=*/target, /*effect_id=*/919,
                       /*attribute=*/true, /*attack=*/false, /*count=*/0,
                       /*duration_rounds=*/rounds, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 2105 —— 未击败对手则令对方场下阵亡的首位精灵消逝 ──
EffectResult effect_2105_vanish_first_dead(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->vanish_spirit) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).hp > 0) {
        return EffectResult::kOk;   // 击败了对手 → 不消逝（1307 同款时点判活）
    }
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == ctx->on_stage[target]) {
            continue;
        }
        const ElfPet& pet = ctx->seerRobot[target].elfPets[slot];
        if (pet.hp <= 0 && pet.numericalBase[NumericalPropertyIndex::HP] > 0) {
            g_core->vanish_spirit(ctx, target, slot, owner);
            return EffectResult::kOk;   // 首位（槽序最小）
        }
    }
    return EffectResult::kOk;
}

// ── 1959 —— 消除对手回合类效果，消除成功{0}%令对手{1}，未触发则造成的攻击伤害翻倍 ──
EffectResult effect_1959_break_burn_or_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int status = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const bool had_round = ctx->rule_center_.has_round_type(target);
    if (g_core->break_round_effects) {
        g_core->break_round_effects(ctx, target);
    }
    if (had_round) {
        bool landed = false;
        if (roll_skill_percent(*ctx, 1959, 0, owner, target, chance) && g_core->apply_anomaly) {
            landed = anomaly_landed(g_core->apply_anomaly(ctx, target, status,
                                                          random_anomaly_duration(), owner));
        }
        // "未触发则翻倍"只对"消除成功但异常没中"生效；消除本身没成功不翻倍。
        if (!landed) {
            ctx->ws.damage_add_pct[owner] += 100.0f;
        }
    }
    return EffectResult::kOk;
}

// ── 2419 —— 解除双方异常；对手侧触发减免窗口 + 自身按对手被解除烧伤合计得神惧劫炎 ──
// 减免窗口 = heal_mod_pct（per-side 单槽）：立即写入（覆盖当前回合），再以 ROUND_START
// tick 重写、OLD_ROUND_END tick 到期清零。tick args=[target, pct, expire]。
EffectResult effect_2419_healmod_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int target = args.int_args[0];
    const int pct = args.int_args[1];
    const int expire = args.int_args[2];
    if (target < 0 || target > 1 || ctx->roundCount >= expire) {
        return EffectResult::kOk;
    }
    ctx->heal_mod_pct[target] = -pct;
    return EffectResult::kOk;
}

EffectResult effect_2419_healmod_clear(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int target = args.int_args[0];
    const int expire = args.int_args[2];
    if (target < 0 || target > 1 || ctx->roundCount + 1 < expire) {
        return EffectResult::kOk;   // 还没到最后一个窗口回合
    }
    ctx->heal_mod_pct[target] = 0;
    return EffectResult::kOk;
}

EffectResult effect_2419_dispel_both_yanshou(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->dispel_active_anomalies) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // 先盘点对手侧（剩余回合数），再解除双方。
    int opp_total = 0;
    int opp_non_burn = 0;
    int opp_burn_rounds = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (!has_real_status(ctx, target, id)) {
            continue;
        }
        const int remaining = ctx->abnormal_status_end_round[target][id] - ctx->roundCount;
        ++opp_total;
        if (id == kBurnId) {
            opp_burn_rounds += remaining;
        } else {
            ++opp_non_burn;
        }
    }
    (void)g_core->dispel_active_anomalies(ctx, owner);
    const int opp_dispelled = g_core->dispel_active_anomalies(ctx, target);
    if (opp_dispelled <= 0) {
        return EffectResult::kOk;   // 对手侧没解除到 → "触发成功"不成立
    }
    // 减免比例 = 非烧伤个数 / 总个数（对手 2 回合内体力恢复效果减少）。
    if (opp_non_burn > 0) {
        const int pct = 100 * opp_non_burn / opp_total;
        const int start = ctx->round_effect_start_round(owner, 2, EffectWindowKind::InRounds);
        const int expire = start + 2;
        const std::vector<int> tick_args{target, pct, expire};
        register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 2419, 2, tick_args,
                              &effect_2419_healmod_tick);
        // 回合结束清零槽（独立 effect_id 占桶；expire 判定内才清，不干扰别家 heal_mod）。
        register_round_effect(ctx, State::BATTLE_ROUND_REDUCTION_NEW_ROUND_END, owner, 241902, 2, tick_args,
                              &effect_2419_healmod_clear);
    }
    // 神惧劫炎：回合数 = 对手被解除烧伤的剩余回合合计（"其中"取对手侧读法，记档待实测）。
    if (opp_burn_rounds > 0) {
        ElfPet& host = ctx->getPet(owner);
        yanshou_gain_shenju(host, opp_burn_rounds);
    }
    return EffectResult::kOk;
}

// ── 572 —— 攻击对手时若对手处于{0}状态，则{1}%使对手{2}（条件式；烧伤看虚拟）──
// 官方依据（语料《机制解析—永烬劫炎·上古炎兽》）：「这个"攻击处于烧伤的对手，令对手焚烬"，
// 这个效果在**解控之后**，所以正常手段是拿不到的，但是**配合魂印的视为处于烧伤就可以触发**」
// → 本条是**条件式**"若处于烧伤则…"（§2）→ 走 is_burn_all（虚拟烧伤也算）。
// 与之相区别的**状态转化**族（影中艳「将对手所处的烧伤转化为焚烬」，库拉塔克 4671）
// 要槽里真有烧伤可转 → 那种走 is_real_burn（见 burn_perception.h 的三类分工）。
EffectResult effect_572_burn_to_incinerate(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int src_status = args.int_args[2];
    const int chance = args.int_args[3];
    const int dst_status = args.int_args[4];
    // Keep the legacy roll before the status check.
    if (owner < 0 || owner > 1 || target < 0 || target > 1
        || !roll_skill_percent(*ctx, 572, 0, owner, target, chance)) {
        return EffectResult::kOk;
    }
    const bool condition_met = (src_status == kBurnId)
        ? is_burn_all(ctx, target)
        : has_real_status(ctx, target, src_status);
    if (!condition_met) {
        return EffectResult::kOk;
    }
    g_core->apply_anomaly(ctx, target, dst_status, random_anomaly_duration(), owner);
    return EffectResult::kOk;
}

// ── 1820 —— 造成的攻击伤害若低于{0}则附加给对手{1}点真实伤害 ──
EffectResult effect_1820_low_damage_true(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->deal_true_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int amount = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const DamageSnapshot& d = ctx->resolvedDamage;
    if (!d.isRed || d.final <= 0 || d.final >= threshold) {
        return EffectResult::kOk;
    }
    g_core->deal_true_damage(ctx, target, amount, owner);
    return EffectResult::kOk;
}

// ════════════════════════ 通用模板族（本三精灵技能引用；非专属但缺实现）════════════════════════

// ── 2 —— 对方体力小于1/2时威力加倍（炎王击 10251；变威力）──
// "威力加倍" = 变威力族，与 40/30 同款改**视图威力**（`=` 打 rewritten → 伤害重算）。
EffectResult effect_2_low_hp_power_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    ElfPet& foe = ctx->getPet(target);
    if (foe.hp * 2 >= pet_max_hp_of(foe)) {
        return EffectResult::kOk;   // 对方体力不低于 1/2
    }
    auto& view = ctx->ws.skill_power_view[self];
    if (view.value > 0) {
        view = view.value * 2;
    }
    return EffectResult::kOk;
}

// ── 53 —— {0}回合攻击伤害是正常状态下的{1}倍（集火 20259）──
// "正常状态下的N倍" = **乘法**通道（非通用增伤）：走 AMP_EXTRA 槽（pct = (N−1)×100，
// 见 effect_set_damage_amp_extra 注释）。窗口沿用 in_rounds 首回合生效套路：
// **立即写当回合 ws**（先手当回合即受益）+ 余下回合由 ROUND_START tick 重写。
EffectResult effect_53_damage_multiplier(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int mult = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || mult <= 1) {
        return EffectResult::kOk;
    }
    const std::vector<int> tick_args{self, 1 - self, (mult - 1) * 100};
    effect_set_damage_amp_extra(ctx, EffectArgs(tick_args));   // 当回合立即生效
    if (rounds > 1) {
        register_round_effect(ctx, State::BATTLE_ROUND_START, self, 53, rounds - 1, tick_args,
                              &effect_set_damage_amp_extra);
    }
    return EffectResult::kOk;
}

// ── 1268 —— 击败对手则下回合开始后随机附加**烧伤/冻伤/中毒**中的{0}种异常（忷魔涸焦血 38283）──
// 结构与盖亚 826（当回合击败对手则下回合开始后随机附加{0}种）同款：MOVER_DEATH 桶探测立旗
// （对手 0 血 + last_damage_actor=自己）+ NextRounds 的 ROUND_START 消费。差别只在**池固定
// 三种**（烧伤 2 / 冻伤 5 / 中毒 1），不是全池。
constexpr int k1268ArmKeyBase = 1268200;   // +owner：击败旗（plugin_storage）

EffectResult effect_1268_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const int slot = ctx->on_stage[opp];
    if (slot < 0 || slot >= 6 || ctx->seerRobot[opp].elfPets[slot].hp > 0) {
        return EffectResult::kOk;
    }
    if (ctx->last_damage_actor[opp][slot] != owner) {
        return EffectResult::kOk;   // 不是自己击败的
    }
    ctx->plugin_storage[k1268ArmKeyBase + owner] = 1;
    return EffectResult::kOk;
}

EffectResult effect_1268_apply(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->attach_random_anomalies) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int count = args.int_args[1];
    if (owner < 0 || owner > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    const auto it = ctx->plugin_storage.find(k1268ArmKeyBase + owner);
    if (it == ctx->plugin_storage.end() || any_int_or(it->second) == 0) {
        return EffectResult::kOk;   // 上回合没击败对手：旗不在
    }
    ctx->plugin_storage.erase(it);
    const std::vector<int> pool{kBurnId, static_cast<int>(AbnormalStatusId::Frostbite),
                               static_cast<int>(AbnormalStatusId::Poison)};
    // 不重复挑选（原语内做）→ "{0}种"在 3 种池里即 3 种全给。
    g_core->attach_random_anomalies(ctx, 1 - owner, pool, count, /*probability_pct=*/100, owner,
                                    ChanceSource::Skill);
    return EffectResult::kOk;
}

EffectResult effect_1268_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int count = args.int_args[2];
    if (owner < 0 || owner > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    // 本回合的死亡探测（两个 MOVER_DEATH 时点都挂——击败发生在哪个出手位都算）
    const std::vector<int> arm_args{owner};
    register_round_effect(ctx, State::BATTLE_FIRST_MOVER_DEATH, owner, 1268, 1, arm_args,
                          &effect_1268_arm);
    register_round_effect(ctx, State::BATTLE_SECOND_MOVER_DEATH, owner, 1268, 1, arm_args,
                          &effect_1268_arm);
    // 下回合开始后消费（NextRounds：本回合不算）
    const std::vector<int> apply_args{owner, count};
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1268, 1, apply_args,
                                &effect_1268_apply);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(136, effect_136_miss_heal);
SKILL_EFFECT(533, effect_533_clear_both_stats);
SKILL_EFFECT(164, effect_164_take_hit_status);
SKILL_EFFECT(986, effect_986_first_amp_extra);
SKILL_EFFECT(1187, effect_1187_status_or_break);
SKILL_EFFECT(837, effect_837_growing_drain);
SKILL_EFFECT(2059, effect_2059_immune_then_drain);
SKILL_EFFECT(786, effect_786_random_anomaly);
SKILL_EFFECT(993, effect_993_all_stats_burn_double);
SKILL_EFFECT(881, effect_881_weak_priority);
SKILL_EFFECT(733, effect_733_reversal_then_status);
SKILL_EFFECT(1604, effect_1604_restore_pp);

SKILL_EFFECT(636, effect_636_clear_boost_then_status);
SKILL_EFFECT(1148, effect_1148_random_drain);
SKILL_EFFECT(108, effect_108_hit_by_attack_burn);
SKILL_EFFECT(96, effect_96_burn_power_double);
SKILL_EFFECT(663, effect_663_window);
SKILL_EFFECT(800, effect_800_window);
SKILL_EFFECT(1168, effect_1168_all_stats_anomaly_double);
SKILL_EFFECT(615, effect_615_window);
SKILL_EFFECT(1032, effect_1032_break_then_seal_attack);
SKILL_EFFECT(1845, effect_1845_loss_fixed);
SKILL_EFFECT(1853, effect_1853_hood_with_burst);
SKILL_EFFECT(1355, effect_1355_crit_immune);
SKILL_EFFECT(2054, effect_2054_window);
SKILL_EFFECT(1115, effect_1115_clear_drops_then_seal);
SKILL_EFFECT(1378, effect_1378_all_stats_chance_double);
SKILL_EFFECT(619, effect_619_opp_priority_down);

SKILL_EFFECT(1606, effect_1606_reversal_drain);
SKILL_EFFECT(1061, effect_1061_boost_reversal);
SKILL_EFFECT(2240, effect_2240_dispel_amp);
SKILL_EFFECT(2418, effect_2418_invalid_shield_hood);
SKILL_EFFECT(1605, effect_1605_chance_status);
SKILL_EFFECT(783, effect_783_boost_lost_pink);
SKILL_EFFECT(1676, effect_1676_window);
SKILL_EFFECT(1674, effect_1674_window);
SKILL_EFFECT(2198, effect_2198_shield_status);
SKILL_EFFECT(2199, effect_2199_hood_status);
SKILL_EFFECT(909, effect_909_first_status_or_fixed);
SKILL_EFFECT(919, effect_919_second_seal_attribute);
SKILL_EFFECT(2105, effect_2105_vanish_first_dead);
SKILL_EFFECT(1959, effect_1959_break_burn_or_double);
SKILL_EFFECT(2419, effect_2419_dispel_both_yanshou);
SKILL_EFFECT(572, effect_572_burn_to_incinerate);
SKILL_EFFECT(1820, effect_1820_low_damage_true);

// 通用模板族（本三精灵技能引用）
SKILL_EFFECT(2, effect_2_low_hp_power_double);       // 炎王击：对方体力<1/2 时威力加倍
SKILL_EFFECT(53, effect_53_damage_multiplier);       // 集火：{0}回合攻击伤害×{1}
SKILL_EFFECT(1268, effect_1268_window);              // 忷魔涸焦血：击败→下回合随机3种异常
