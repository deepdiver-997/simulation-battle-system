#include <utils/sbs_rand.h>
// 克雷弗德 4644 技能组（协议四技 + 超越式/隔断式/歼灭式/调和式 + 通用模板族）
//
// 官方参数（moves.side_effect / effect_info，2026-09-20 查库）：
//   46   抵挡下{0}次对手的攻击            —— 隔断式·建构律(28724) [1]
//   1672 出手时若自身未满体力则吸取对手{0}点体力 —— 隔断式 [300]
//   2149 构造I：先出手时消除对手回合类效果 —— 协议I·激(37354)
//   954  若当前体力低于对手则先制+1        —— 协议I·激（选择期，core 路由）
//   1398 消除对手能力提升，成功则{0}%令对手{1}，未触发则{2}%令对手{3} —— 协议I [100,0,100,7]
//   1876 反馈自身能力下降，成功则令对手下{0}次使用的技能无效，失败则解除自身下降 —— 协议I [1]
//   2150 构造II：先出手时令对手下次攻击造成的伤害减少50% —— 协议II·限(28725)
//   1400 {0}回合内若自身回合类效果被消除则{1}%令对手{2} —— 协议II [3,100,0]
//   1475 {0}回合内自身受到的固定伤害和百分比伤害减少{1}% —— 协议II [3,60]
//   2151 构造III：先出手时此技能不会消耗PP值 —— 协议III·维(28726)
//   1397 全属性+{0}，自身体力低于1/{1}时强化效果翻倍 —— 协议III [1,2]
//   1892 {0}回合内每回合使用技能吸取对手{1}点体力，低于1/{2}翻倍，未受到固伤则+{3}真伤 —— [4,300,2,240]
//   1926 下{0}次攻击技能无视攻击免疫效果 —— 协议III [2]
//   937  命中后{0}%使对手{1}，未触发则对手下{2}回合先制-{3} —— 协议III [100,7,2,2]
//   2152 构造IV：先出手时造成的攻击伤害提升50% —— 协议IV·解(37357)
//   1747 消除对手回合类效果，成功则自身免疫下{0}次异常 —— 协议IV [1]
//   1600 {0}%令对手{1}，未触发则自身下{2}回合必暴，触发后{3}回合内对手主动切换则{4}%令登场者{5} —— [100,0,2,2,100,0]
//   1323 造成伤害的{0}%恢复自身体力，暴击则附加恢复量等量的百分比伤害 —— 协议IV [100]（AFTER_ACTION）
//   2153 此技能拥有本次在场生效过的所有构造效果（取消先出手条件）—— 超越式·变革律(37358)
//   2155 德莱赫默构造数低于2时先制-1并吸取对手最大体力1/3；≥2时无视自身下降与对手提升 —— 超越式
//   1156 击败对手则恢复自身全部体力和所有技能PP值 —— 超越式（BATTLE_AFTER_DEFEATING_OPPONENT）
//   1563 损失自身{0}体力给对手{1}固伤，不足{2}则全损且翻倍 —— 歼灭式·崩解律(37355) [150,300,150]
//   1122 使对手全属性-{0}，未触发则对手{1} —— 歼灭式 [1,0]
//   1444 威力随自身体力变化，越少越大 —— 歼灭式（线性口径：威力×(1+已损失体力比例)，用户 2026-09-20）
//   1419 消除双方能力提升，任一方成功则自身免疫下{0}次异常+对手随机{1}个技能PP归零 —— 调和式·进化律(37356) [1,2]
//   1574 附加自身攻速总和{0}%的百分比伤害，每次+{1}%，最高{2}% —— 调和式 [10,15,40]
//
// 关键口径（用户 2026-09-20 拍板）：
//   ★ 构造点亮 = 携带构造效果的技能**命中且效果生效**（SKILL_EFFECT HIT 分支天然满足）；
//     每次在场首次点亮授予德莱赫默，重复点亮无额外效果。
//   ★ 次数类减伤（抵挡/对手下次攻击-50%）**不响应属性技能**（官方已修复 B 站实测的
//     "被属性消耗"坑）：-50% 的撤只认 allowAttackDamagePipeline 的攻击（soul 侧 ATTACK_DAMAGE 节点）；
//     46 的抵挡走 DAMAGE 免伤票（属性直伤天然不消耗，skip_routine_block）。
//   ★ 954/2155 的先制部分在**选择期**生效：954 走 core 选择期路由（2000/610 同款）；
//     2155 的先制-1 由魂印侧 MOVE_RIGHT 节点处理（构造数只有魂印侧/发布槽知道）。
//   ★ 2155 的"无视自身下降/对手提升"= 伤害计算视图钳制（魂印侧 SKILL_EFFECT 钳、AFTER_ACTION 还原）。
//   ★ 1876 反馈：成功 = 镜像下降至少一项落到对手 + 解除自身；失败（免弱/无下降可反馈）
//     = 只解除自身下降，不封技能。
#include "common.h"
#include "../crawford_runtime.h"

namespace {

// ⚠️ 与 soul_lib/crawford_deleermo_4644.cpp 同一注册键（effect_icon.effect_id = 2231）。
constexpr int kMark = 2231;
constexpr int kFifthSkillId = 37358;   // 超越式·变革律（魂印侧先制-1/视图钳制的判据）
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 麻痹 0

// ── 构造点亮（每次在场首次；bit0=I..bit3=IV）——SKILL_EFFECT HIT 分支即"命中且生效"──
// 位图存 ElfPet::on_stage_storage（"本次上场"槽，换宠由引擎清空 = 下场重置）。
void light_construct(BattleContext* ctx, int owner, int bit) {
    ElfPet& host = ctx->getPet(owner);
    const int cur = crawford_constructs(host);
    if (cur & bit) {
        return;
    }
    crawford_set_constructs(host, cur | bit);
}

// ── 构造载荷（2149~2152 的"构造效果"本体；2153 免条件复用）──
// 构造I：消除对手回合类效果（arg 条件"先出手"由调用方判，2153 无条件直调）。
void payload_break_rounds(BattleContext* ctx, int owner) {
    if (g_core && g_core->break_round_effects) {
        g_core->break_round_effects(ctx, 1 - owner);
    }
}

// 构造II：对手下次攻击伤害 -50%（持续到对手下一次攻击管线结算；属性技能不消耗）。
// ws 槽位先抢空槽（chaos_sorensen 同款），槽位经发布槽告知 soul 侧（撤除时清同一槽）。
// ⚠️ **幂等**（用户 2026-09-20：与对应技能获得的是同一来源，超越式复读/重复使用
// **覆盖不叠加**）——已 pending 就不抢第二笔，对手那一刀只吃一个 -50%。
void payload_opp_next_attack_reduce(BattleContext* ctx, int owner) {
    if (crawford_pub_opp_reduce(ctx->plugin_storage, owner)) {
        return;
    }
    // ⚠️ 减伤桶按**防守方**索引（对手打我时 defender=我）→ 写**自己**的桶。
    int slot = -1;
    int(&slots)[4] = ctx->ws.damage_reduce_add[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = 50;
            slot = i;
            break;
        }
    }
    crawford_pub_opp_reduce(ctx->plugin_storage, owner, true);
    if (slot >= 0) {
        ctx->plugin_storage[kCrawfordPubOppReduceBase + 100 + owner] = slot;
    }
}

// 构造III：本技能不消耗 PP（AFTER_ACTION 的 consume_selected_skill_pp 读该旗短路）。
void payload_pp_free(BattleContext* ctx, int owner) {
    ctx->ws.skill_pp_cost_consumed[owner] = true;
}

// 构造IV：造成的攻击伤害 +50%（通用增伤·加算；SKILL_EFFECT 写视图 → ATTACK_DAMAGE 生效）。
void payload_damage_amp(BattleContext* ctx, int owner) {
    ctx->ws.damage_add_pct[owner] += 50.0f;
}

// ════════════════════════ 通用模板族 ════════════════════════

// ── 46 —— 抵挡下{0}次对手的攻击 ──
// 免伤票（IMMUNE/DAMAGE，counts 型）：伤害管线 BLOCK 阶段消费——只响应**攻击红伤**，
// 属性直伤（skip_routine_block）与属性技能天然不消耗（用户 2026-09-20 口径）。
EffectResult effect_46_block_attacks(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int counts = args.int_args[2];
    if (owner < 0 || owner > 1 || counts <= 0) {
        return EffectResult::kOk;
    }
    ctx->rule_center_.grant_immune(
        owner, static_cast<int>(ImmunityType::DAMAGE), ~0ULL, /*anomaly_mask=*/0,
        /*duration_rounds=*/0, ctx->roundCount, /*source_id=*/0, /*soul=*/false,
        EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/counts,
        /*source_effect_id=*/46);
    return EffectResult::kOk;
}

// ── 1672 —— 出手时若自身未满体力则吸取对手{0}点体力 ──
EffectResult effect_1672_drain_flat(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp >= max_hp_of(ctx, owner)) {
        return EffectResult::kOk;   // 满体力不吸取
    }
    ElfPet& opp = ctx->getPet(1 - owner);
    if (opp.hp <= 0) {
        return EffectResult::kOk;
    }
    const int drained = amount > opp.hp ? opp.hp : amount;
    opp.hp -= drained;
    g_core->heal_amount(ctx, owner, drained);
    return EffectResult::kOk;
}

// ── 954 —— 若当前体力低于对手则先制+1（选择期；core 侧路由到 selection_effects_）──
EffectResult effect_954_low_hp_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp < ctx->getPet(target).hp) {
        ctx->ws.preemptive_level[owner] += 1;
    }
    return EffectResult::kOk;
}

// ── 1398 —— 消强成功{0}%令对手{1}，未触发{2}%令对手{3} ──
EffectResult effect_1398_clear_boost_or_status(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int c1 = args.int_args[2];
    const int s1 = args.int_args[3];
    const int c2 = args.int_args[4];
    const int s2 = args.int_args[5];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const bool cleared = g_core->clear_stat_boosts && g_core->clear_stat_boosts(ctx, target) > 0;
    if (!g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    if (cleared && roll_percent(c1)) {
        g_core->apply_anomaly(ctx, target, s1, /*duration=*/-1, owner);
    } else if (roll_percent(c2)) {
        g_core->apply_anomaly(ctx, target, s2, /*duration=*/-1, owner);
    }
    return EffectResult::kOk;
}

// ── 1876 —— 反馈自身能力下降：成功 = 镜像落到对手 + 解除自身 + 封对手下{0}次技能；失败 = 只解除 ──
EffectResult effect_1876_reflect_drops(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int times = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || times <= 0 || !g_core) {
        return EffectResult::kOk;
    }
    // 先拍快照再动手：clear_stat_drops 会把负等级清零。
    int neg_slots[6];
    int neg_count = 0;
    for (int i = 0; i < 6; ++i) {
        if (ctx->ability_levels[owner][i] < 0) {
            neg_slots[neg_count++] = i;
        }
    }
    if (neg_count == 0) {
        return EffectResult::kOk;   // 无下降可反馈 → 反馈失败（也无下降可解除）
    }
    if (!g_core->stat_drop || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    bool mirrored = false;
    for (int i = 0; i < neg_count; ++i) {
        const StatDropResult r = g_core->stat_drop(ctx, target, neg_slots[i],
                                                   -ctx->ability_levels[owner][neg_slots[i]],
                                                   /*actor=*/owner);
        // 2026-10-25 口径：SUCCESS 才算反馈落地；AT_FLOOR（对手已 -6 钳底）与 IMMUNE
        // 都算未触发 → 不封技（官方实测灼眼焰华 2161 同族：对手 -6 也算未触发）。
        if (r == StatDropResult::SUCCESS) {
            mirrored = true;
        }
    }
    g_core->clear_stat_drops(ctx, owner);
    if (mirrored && g_core->seal_skill) {
        // "下{0}次使用的技能无效"：攻击/属性都封（SEAL_ALL），次数型。
        g_core->seal_skill(ctx, /*source=*/owner, /*target=*/target, /*effect_id=*/1876,
                           /*attribute=*/true, /*attack=*/true, /*count=*/times,
                           /*duration_rounds=*/0, /*penetrable=*/true, /*source_slot=*/-1,
                           EffectScope::ON_STAGE, /*hit_invalid=*/false,
                           /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

// ── 1400 —— {0}回合内若自身回合类效果被消除则{1}%令对手{2} ──
EffectResult effect_1400_break_retaliate(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int status = args.int_args[4];
    if (owner < 0 || owner > 1 || rounds <= 0 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    CrawfordMovesState& st = crawford_moves_rt(ctx->plugin_storage, owner);
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds);
    const int until = start + rounds - 1;
    if (!st.break_watch_up) {
        st.break_watch_up = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_BREAK, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, chance, status](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.target != owner) {
                    return;   // 只响应"自身的回合类效果被消除"
                }
                CrawfordMovesState& s = crawford_moves_rt(c->plugin_storage, owner);
                if (c->roundCount > s.break_watch_until) {
                    return;   // 窗口外
                }
                if (g_core && g_core->apply_anomaly && roll_percent(chance)) {
                    g_core->apply_anomaly(c, 1 - owner, status, -1, owner);
                }
            },
            WatcherScope::TEAM);
    }
    st.break_watch_until = until;   // 重复施放刷新窗口
    return EffectResult::kOk;
}

// ── 1475 —— {0}回合内受到的固伤和百分比伤害-{1}%（粉伤管线 REDUCE 阶段，窗口型）──
EffectResult effect_1475_pink_reduce(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    CrawfordMovesState& st = crawford_moves_rt(ctx->plugin_storage, owner);
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds);
    if (!st.pink_1475_up) {
        st.pink_1475_up = true;
        ctx->register_pink_damage_effect(
            PinkDamagePhase::REDUCE_EXTRA, owner,
            [owner, pct](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner || c->resolvedPink.target != owner) {
                    return;
                }
                CrawfordMovesState& s = crawford_moves_rt(c->plugin_storage, owner);
                if (c->roundCount > s.pink_1475_until || c->resolvedPink.final <= 0) {
                    return;
                }
                c->resolvedPink.final -= c->resolvedPink.final * pct / 100;
            },
            EffectScope::TEAM);
    }
    st.pink_1475_until = start + rounds - 1;
    return EffectResult::kOk;
}

// ── 1397 —— 全属性+{0}，自身体力低于 1/{1} 时翻倍 ──
EffectResult effect_1397_all_stats(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int delta = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || delta <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    int boost = delta;
    if (ctx->getPet(owner).hp * denom < max_hp_of(ctx, owner)) {
        boost = delta * 2;   // 严格低于 1/denom
    }
    all_stats_up(ctx, owner, boost);
    return EffectResult::kOk;
}

// ── 1892 —— {0}回合内每回合使用技能吸取{1}点体力（低于1/{2}翻倍；未受到固伤+{3}真伤）──
// Tick Args = [owner, target, amount, denom, true_dmg]（window 体显式重排后传入）。
// "未受到固伤"= 结果口径：吸取前后体力没变（护罩全吸/免粉/锁血）即算，见 tick 内注释。
EffectResult effect_1892_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 5 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    const int denom = args.int_args[3];
    const int true_dmg = args.int_args[4];
    if (!used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;   // 被控/嗑药回合不结算（"每回合使用技能"）
    }
    int drain = amount;
    if (ctx->getPet(owner).hp * denom < max_hp_of(ctx, owner)) {
        drain *= 2;   // 自身体力低于 1/denom → 吸取翻倍
    }
    if (drain <= 0) {
        return EffectResult::kOk;
    }
    // "未受到固伤"按**结果口径**判（用户 2026-09-20 实测口径）：只看吸取前后**体力有没有变化**
    // ——护罩把吸取全吸掉（护罩照扣）、免粉、锁血留 1 点，体力没变都算"未受到" → +真伤；
    // 回血同样按实际落到体力的量（不再读 resolvedPink.final 那个结算视图：锁血/护罩场合
    // 它 > 0 而体力没动，照它回血就是凭空回血）。
    const int hp_before = ctx->getPet(target).hp;
    g_core->deal_pink_damage(ctx, target, drain, DamageKind::FIXED, owner);
    const int received = hp_before - ctx->getPet(target).hp;
    if (received > 0 && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, received);   // 吸取 = 按体力实际变化回
    }
    if (received <= 0 && true_dmg > 0 && g_core->deal_true_damage) {
        g_core->deal_true_damage(ctx, target, true_dmg, owner);   // 未受到固伤 → +真伤补偿（用户口径⑤）
    }
    return EffectResult::kOk;
}

EffectResult effect_1892_window(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 显式重排 tick 参数（窗口体自己的 [2]=rounds 不进 tick）。
    const std::vector<int> tick_args{owner, args.int_args[1], args.int_args[3],
                                     args.int_args[4], args.int_args[5]};
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, owner, 1892, rounds,
                          tick_args, &effect_1892_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, owner, 1892, rounds,
                          tick_args, &effect_1892_tick);
    return EffectResult::kOk;
}

// ── 1926 —— 下{0}次攻击技能无视攻击免疫效果（PENETRATE_ATTACK，次数型）──
EffectResult effect_1926_penetrate(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int counts = args.int_args[2];
    if (owner < 0 || owner > 1 || counts <= 0) {
        return EffectResult::kOk;
    }
    ctx->rule_center_.grant_penetrate(
        owner, RuleCategory::PENETRATE_ATTACK, /*target=*/owner, /*level=*/1,
        /*ignore_attack_immunity=*/true, /*ignore_damage_limit=*/false,
        /*counts=*/counts, /*rounds=*/0, ctx->roundCount,
        EffectScope::ON_STAGE, /*source_effect_id=*/1926);
    return EffectResult::kOk;
}

// ── 937 —— 命中后{0}%令对手{1}，未触发则对手下{2}回合先制-{3} ──
EffectResult effect_937_prio_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int target = args.int_args[1];
    const int drop = args.int_args[2];
    if (target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[target] -= drop;
    return EffectResult::kOk;
}

EffectResult effect_937_hit_status(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int status = args.int_args[3];
    const int rounds = args.int_args[4];
    const int prio_drop = args.int_args[5];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    if (roll_percent(chance)) {
        g_core->apply_anomaly(ctx, target, status, /*duration=*/-1, owner);
        return EffectResult::kOk;
    }
    if (rounds > 0 && prio_drop != 0) {
        // "下{2}回合"= NextRounds 窗口（本回合不算）；tick 在 MOVE_RIGHT（memset 后）扣先制。
        const std::vector<int> tick_args{owner, target, prio_drop};
        register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 937, rounds,
                                    tick_args, &effect_937_prio_tick);
    }
    return EffectResult::kOk;
}

// ── 1747 —— 消除对手回合类效果成功则自身免疫下{0}次异常 ──
EffectResult effect_1747_break_then_immune(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int counts = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || counts <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core->break_round_effects || g_core->break_round_effects(ctx, target) != BreakResult::SUCCESS) {
        return EffectResult::kOk;
    }
    ctx->rule_center_.grant_immune(
        owner, static_cast<int>(ImmunityType::ANOMALY), ~0ULL, /*anomaly_mask=*/0,
        /*duration_rounds=*/0, ctx->roundCount, /*source_id=*/0, /*soul=*/false,
        EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/counts,
        /*source_effect_id=*/1747);
    return EffectResult::kOk;
}

// ── 1600 —— {0}%令对手{1}；未触发则自身下{2}回合必暴；触发后{3}回合内对手主动切换则{4}%令登场者{5} ──
EffectResult effect_1600_paralysis_chain(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 8) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int status = args.int_args[3];
    const int crit_rounds = args.int_args[4];
    const int switch_rounds = args.int_args[5];
    const int sw_chance = args.int_args[6];
    const int sw_status = args.int_args[7];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly(ctx, target, status, /*duration=*/-1, owner);
    const bool landed = r == ApplyAnomalyResult::SUCCESS
                     || r == ApplyAnomalyResult::REPLACED_EXISTING
                     || r == ApplyAnomalyResult::DURATION_EXTENDED
                     || r == ApplyAnomalyResult::CONVERTED;
    if (!landed) {
        // 未触发 → 自身下 crit_rounds 回合攻击必定致命一击（NextRounds 窗口，58 同款 must_crit）。
        if (crit_rounds > 0) {
            const std::vector<int> tick_args{owner, target};
            register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1600, crit_rounds,
                                        tick_args,
                                        [](BattleContext* c, const EffectArgs& a) -> EffectResult {
                                            if (!c || !a.int_args || a.int_count < 1) {
                                                return EffectResult::kOk;
                                            }
                                            c->ws.must_crit[a.int_args[0]] = true;
                                            return EffectResult::kOk;
                                        });
        }
        return EffectResult::kOk;
    }
    // 触发 → switch_rounds 回合内对手**主动切换**（EVENT_SWAP 只在主动中切 emit，死亡换宠不发）
    // 则登场精灵 {4}% 进入 {5}。窗口走 NextRounds 起算；监听器只挂一次、窗口刷新。
    CrawfordMovesState& st = crawford_moves_rt(ctx->plugin_storage, owner);
    if (!st.swap_watch_up) {
        st.swap_watch_up = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_SWAP, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, sw_chance, sw_status](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor < 0 || ev.actor > 1 || !g_core || !g_core->apply_anomaly) {
                    return;
                }
                CrawfordMovesState& s = crawford_moves_rt(c->plugin_storage, owner);
                if (c->roundCount > s.swap_watch_until) {
                    return;
                }
                if (roll_percent(sw_chance)) {
                    // drain 时 on_stage 已是新精灵（battleFsm EVENT_SWAP 注释）
                    g_core->apply_anomaly(c, ev.actor, sw_status, /*duration=*/-1, owner);
                }
            },
            WatcherScope::TEAM);
    }
    const int start =
        ctx->round_effect_start_round(owner, switch_rounds, EffectWindowKind::NextRounds);
    st.swap_watch_until = start + switch_rounds - 1;
    return EffectResult::kOk;
}

// ── 1323 —— 造成伤害{0}%恢复自身体力；暴击则附加恢复量等量的百分比伤害（AFTER_ACTION）──
EffectResult effect_1323_damage_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 3 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int dmg = ctx->resolvedDamage.final;
    if (dmg <= 0) {
        return EffectResult::kOk;
    }
    const int before = ctx->getPet(owner).hp;
    g_core->heal_amount(ctx, owner, dmg * pct / 100);
    const int healed = ctx->getPet(owner).hp - before;
    if (ctx->resolvedDamage.isCrit && healed > 0 && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, target, healed, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1156 —— 击败对手则恢复自身全部体力和所有技能PP值 ──
// ⚠️ AFTER_DEFEATING_OPPONENT 状态**每回合都进**（无死亡也走），1960 式"挂状态桶"会让
// 满血恢复无门控地每回合触发 → 这里挂 **EVENT_DEATH**：每次使用挂一个 once 监听，
// 对手死亡（ev.target==对手）→ 满血 + 全 PP，自删。
// 已知边角：①使用后若对手由**其他原因**死亡（残留监听触发）也算"击败"——归因粒度所限，
// 与官方语义偏差极小；②吸取粉伤路径不写 last_damage_actor（死亡归因缺失），故**不判
// ev.actor**——超越式的击杀大量来自 2155 的吸取，按 actor 过滤会漏掉最常见的击杀形态。
EffectResult effect_1156_full_restore(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/true,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != 1 - owner) {
                return;
            }
            ElfPet& self = c->getPet(owner);
            const int max_hp = std::max(1, self.numericalBase[NumericalPropertyIndex::HP]);
            if (self.hp < max_hp) {
                g_core->heal_amount(c, owner, max_hp - self.hp);
            }
            for (Skills& s : self.skills) {
                if (s.pp >= 0) {
                    s.pp = s.maxPP;
                }
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1563 —— 损失自身{0}体力给对手{1}固伤；不足{2}则全损且固伤翻倍 ──
EffectResult effect_1563_lose_hp_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 5 || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int loss = args.int_args[2];
    const int fixed = args.int_args[3];
    const int thresh = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp < thresh) {
        const int dmg = fixed * 2;
        self.hp = 0;   // 损失全部体力
        g_core->fixed_damage(ctx, target, dmg);
    } else {
        self.hp -= loss;
        g_core->fixed_damage(ctx, target, fixed);
    }
    return EffectResult::kOk;
}

// ── 1122 —— 使对手全属性-{0}，未触发（全六槽未落地：免弱挡/已 -6 钳底）则对手{1} ──
// 2026-10-25 口径：SUCCESS 才算落地，AT_FLOOR（已 -6）与 IMMUNE 都计入"未触发"。
EffectResult effect_1122_all_drop_or_status(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 4 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    const int status = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    int blocked = 0;
    for (int i = 0; i < 6; ++i) {
        if (g_core->stat_drop(ctx, target, i, amount, /*actor=*/owner) != StatDropResult::SUCCESS) {
            ++blocked;
        }
    }
    if (blocked == 6 && g_core->apply_anomaly) {
        g_core->apply_anomaly(ctx, target, status, /*duration=*/-1, owner);
    }
    return EffectResult::kOk;
}

// ── 1444 —— 威力随自身体力变化（线性：威力 ×(1+已损失体力比例)，满血 130 / 空血 260）──
// SKILL_EFFECT 写威力视图 → ATTACK_DAMAGE 的"变威力"检查点按新威力重算（2501 同路）。
EffectResult effect_1444_power_by_hp(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= static_cast<int>(ctx->getPet(owner).skills.size())) {
        return EffectResult::kOk;
    }
    const int base = ctx->getPet(owner).skills[static_cast<std::size_t>(idx)].power;
    const int max_hp = max_hp_of(ctx, owner);
    const int lost = max_hp - ctx->getPet(owner).hp;
    if (base <= 0 || lost <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.skill_power_view[owner] += base * lost / max_hp;
    return EffectResult::kOk;
}

// ── 1419 —— 消除双方能力提升，任一方成功则自身免疫下{0}次异常 + 对手随机{1}个技能PP归零 ──
EffectResult effect_1419_clear_both(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int immune_counts = args.int_args[2];
    const int pp_count = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const int a = g_core->clear_stat_boosts ? g_core->clear_stat_boosts(ctx, owner) : 0;
    const int b = g_core->clear_stat_boosts ? g_core->clear_stat_boosts(ctx, target) : 0;
    if (a <= 0 && b <= 0) {
        return EffectResult::kOk;   // 双方都没消掉 → 无后续
    }
    if (immune_counts > 0) {
        ctx->rule_center_.grant_immune(
            owner, static_cast<int>(ImmunityType::ANOMALY), ~0ULL, /*anomaly_mask=*/0,
            /*duration_rounds=*/0, ctx->roundCount, /*source_id=*/0, /*soul=*/false,
            EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/immune_counts,
            /*source_effect_id=*/1419);
    }
    if (pp_count > 0 && g_core->pp_reduce) {
        // 随机 {1} 个技能 PP 归零（不重复挑；pp==-1 无限 PP 不选）。
        ElfPet& opp = ctx->getPet(target);
        std::vector<int> candidates;
        for (std::size_t i = 0; i < opp.skills.size(); ++i) {
            if (opp.skills[i].pp > 0) {
                candidates.push_back(static_cast<int>(i));
            }
        }
        for (int n = 0; n < pp_count && !candidates.empty(); ++n) {
            const int pick = sbs_rand() % candidates.size();
            opp.skills[static_cast<std::size_t>(candidates[pick])].pp = 0;
            candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(pick));
        }
    }
    return EffectResult::kOk;
}

// ── 1574 —— 附加自身攻速总和{0}%的百分比伤害，每次使用+{1}%，最高{2}% ──
EffectResult effect_1574_scaling_pink(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 5 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int step = args.int_args[3];
    const int cap = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    CrawfordMovesState& st = crawford_moves_rt(ctx->plugin_storage, owner);
    const int atk = ctx->ws.battle_attrs[owner][NumericalPropertyIndex::PHYSICAL_ATTACK];
    const int spd = ctx->ws.battle_attrs[owner][NumericalPropertyIndex::SPEED];
    const int amount = (atk + spd) * st.tune_1574_pct / 100;
    if (amount > 0) {
        g_core->deal_pink_damage(ctx, target, amount, DamageKind::PERCENT_VALUE, owner);
    }
    if (st.tune_1574_pct < cap) {
        st.tune_1574_pct = st.tune_1574_pct + step > cap ? cap : st.tune_1574_pct + step;
    }
    return EffectResult::kOk;
}

// ════════════════════════ 克雷弗德专属构造族 ════════════════════════

// ── 2149 —— 构造I：先出手时消除对手回合类效果 ──
EffectResult effect_2149_construct1(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    light_construct(ctx, owner, 1);
    if (!ctx->is_second_mover(owner)) {
        payload_break_rounds(ctx, owner);
    }
    return EffectResult::kOk;
}

// ── 2150 —— 构造II：先出手时令对手下次攻击造成的伤害减少50% ──
EffectResult effect_2150_construct2(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    light_construct(ctx, owner, 2);
    if (!ctx->is_second_mover(owner)) {
        payload_opp_next_attack_reduce(ctx, owner);
    }
    return EffectResult::kOk;
}

// ── 2151 —— 构造III：先出手时此技能不会消耗PP值 ──
EffectResult effect_2151_construct3(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    light_construct(ctx, owner, 4);
    if (!ctx->is_second_mover(owner)) {
        payload_pp_free(ctx, owner);
    }
    return EffectResult::kOk;
}

// ── 2152 —— 构造IV：先出手时造成的攻击伤害提升50% ──
EffectResult effect_2152_construct4(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    light_construct(ctx, owner, 8);
    if (!ctx->is_second_mover(owner)) {
        payload_damage_amp(ctx, owner);
    }
    return EffectResult::kOk;
}

// ── 2153 —— 超越式：拥有本次在场生效过的所有构造效果（取消"先出手"条件）──
// ★ 只复读**已点亮**的构造（用户 2026-09-20：注意检查对应构造有没有被点亮）。
EffectResult effect_2153_copy_constructs(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int lit = crawford_constructs(ctx->getPet(owner));
    if (lit & 1) {
        payload_break_rounds(ctx, owner);
    }
    if (lit & 2) {
        payload_opp_next_attack_reduce(ctx, owner);
    }
    if (lit & 4) {
        payload_pp_free(ctx, owner);
    }
    if (lit & 8) {
        payload_damage_amp(ctx, owner);
    }
    return EffectResult::kOk;
}

// ── 2154 —— 令德莱赫默进行一次额外行动（幂等声明，FSM 在 EXTRA_ACTION 时点消费）──
EffectResult effect_2154_deleermo_action(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->declare_extra_action(owner);
    return EffectResult::kOk;
}

// ── 2155 —— 超越式条件分叉（执行期半边；先制-1 与视图钳制在魂印侧节点）──
// 构造数<2：吸取对手最大体力的1/3。≥2 的"无视下降/提升"由魂印侧在 SKILL_EFFECT 钳视图。
EffectResult effect_2155_fifth_conditional(BattleContext* ctx, const EffectArgs& args) {
    if (!args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const int count = crawford_count_constructs(crawford_constructs(ctx->getPet(owner)));
    if (count < 2 && g_core && g_core->drain_hp) {
        fprintf(stderr, "[DBG-h1] pre-drain self hp=%d\n", ctx->getPet(owner).hp);
        g_core->drain_hp(ctx, owner, target, /*fraction_denom=*/3);
        fprintf(stderr, "[DBG-h1] post-drain self hp=%d\n", ctx->getPet(owner).hp);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(46, effect_46_block_attacks);
SKILL_EFFECT(1672, effect_1672_drain_flat);
SKILL_EFFECT(954, effect_954_low_hp_priority);
SKILL_EFFECT(1398, effect_1398_clear_boost_or_status);
SKILL_EFFECT(1876, effect_1876_reflect_drops);
SKILL_EFFECT(1400, effect_1400_break_retaliate);
SKILL_EFFECT(1475, effect_1475_pink_reduce);
SKILL_EFFECT(1397, effect_1397_all_stats);
SKILL_EFFECT(1892, effect_1892_window);
SKILL_EFFECT(1926, effect_1926_penetrate);
SKILL_EFFECT(937, effect_937_hit_status);
SKILL_EFFECT(1747, effect_1747_break_then_immune);
SKILL_EFFECT(1600, effect_1600_paralysis_chain);
SKILL_EFFECT(1323, effect_1323_damage_heal);
SKILL_EFFECT(1156, effect_1156_full_restore);
SKILL_EFFECT(1563, effect_1563_lose_hp_fixed);
SKILL_EFFECT(1122, effect_1122_all_drop_or_status);
SKILL_EFFECT(1444, effect_1444_power_by_hp);
SKILL_EFFECT(1419, effect_1419_clear_both);
SKILL_EFFECT(1574, effect_1574_scaling_pink);
SKILL_EFFECT(2149, effect_2149_construct1);
SKILL_EFFECT(2150, effect_2150_construct2);
SKILL_EFFECT(2151, effect_2151_construct3);
SKILL_EFFECT(2152, effect_2152_construct4);
SKILL_EFFECT(2153, effect_2153_copy_constructs);
SKILL_EFFECT(2154, effect_2154_deleermo_action);
SKILL_EFFECT(2155, effect_2155_fifth_conditional);
