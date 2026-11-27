#include <utils/sbs_rand.h>
// F 组收官 · F10 misc 散族 —— 498/716/672/671/1117/923/762/952/1075/132（2026-09-29，worktree ../agents/f10）
//
// 看板 F10 行（11 效果）；本文件实现 10 个，**484 跳过**（见文末记档）。
// 每个效果"就近挂靠"既有范式（先例文件注在各自小节），全部纯插件可达、不改内核。
//
// 载体切片（DB moves.side_effect/side_effect_arg，side_effect.arg_count 切分，2026-09-29 查库；
// 4399 原文 reference/scrape_4399/out/pets_full.jsonl 交叉验证——每条都逐字吻合）：
//   498×33  狂雷惊爆闪 15517=[5,8]（伊森特："使用后5回合内致命一击几率提升1/8"）、
//           怨之潮 15914=[3,8]、拉朽震天箭 16562=[3,4]（"3回合致命几率上升1/4"）、弑君之芍 19718=[5,4]
//   716×31  圣灵之光 24190=[3,1]（北冥·玄武："3回合内对手使用攻击技能，则使对手随机1个技能的
//           PP值归零"）、天地玄黄 23869=[3,2]（"随机2个"）
//   672×32  一锤定江山 16586=[672]（大锤："当回合击败对手则恢复全部体力"；args_num=0 纯标记）
//   671×30  圣虎凌天爪 16580=[50]（天尊·白虎："若对手处于异常状态则恢复造成伤害的50%的体力"）、
//           雪岭凝霜刃 31480=[100]
//   1117×31 丰收之舞 26445=[]（args_num=0 纯标记；生命永恒 25861 同款："自身体力低于1/2时先制+2"）
//   923×31  停梅抒意 25108=[3,200]（金笔书圣："3回合内，先出手时额外吸收对手200点体力"）、
//           菊韵冷艳 26698=[3,300]
//   762×35  虫族领地 26166=[3,50]（艾莉安娜："3回合内每回合恢复自身已损失体力的50%"）、
//           骤雨天罚 24252=[2,50]（"恢复自身已损失HP的50%"）
//   952×29  狂风怒战 25023=[1]（"全属性+1,若对手处于能力下降状态则效果翻倍"）、坠天断魂曲 24921=[1]
//   1075×30 寰宇守望 34981=[3,3]（切片自查：926.arg_count=2 → 1075=[3,3]，回 1/3、
//           低于 1/3 回满；链 926"反转自身能力下降"无下降则 no-op）、纯净魂心 25211=[2,2]
//           （弥卡尔："恢复1/2最大体力，低于1/2则直接回满"——[2,2] 两分支都落在回满）
//   132×30  爆炎喷射 12371=[]（皮特萨罗："若当前HP低于对手，则技能威力翻倍"；args_num=0 纯标记）
//   484×30  月牙之刃 14226=[5,1,10]（鲁伊斯："连续攻击对手5次，每次使用攻击次数+1，最大攻击
//           次数10次"）——**跳过，见文末记档**
//
// 动工前三查（2026-09-29）：①内核 grep 无 498/716/672/671/1117/923/762/952/1075/132 硬编码
// （case 分支零命中；484 在 effect_meta.cpp 明确标注"动态连击…（未实现）"）②effect_meta 无
// 牵连分类（484 的 Combo 是纯元数据标注，loadSkills 不消费）③moves_lib 无既有注册
// （SKILL_EFFECT/CARRY_EFFECT 这十个 id 零命中）——真缺口。
//
// plugin_storage 键段（99055 段，grep 全库无撞；场景 700 只读审计）：
//   990551100 + owner   1117 watcher 已挂旗标（防重挂，482 同款）
//
// 场景回归：test/scenario/scenario_700_misc_family.cpp（700 号段，一文件覆盖十效果）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <numeric>
#include <vector>

namespace {

// ── AFTER_ACTION 桶的当前行动方（f_onhit_family.cpp 同款小工具，插件各自持私有副本）──
int bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

// ── 桥接节点注册（f_damage_threshold.cpp register_once_this_round 同款，本地私有副本）──
// FIRST/SECOND_AFTER_ACTION 双侧各一条 duration=1，只有行动方那条跑 → 恰好一次。
// ⚠️ ints 按值逐桶拷贝（场景 640 抓过循环 move 搬空 args 的坑）。
void register_after_action_bridge(BattleContext* ctx, int owner, int effect_id,
                                  const std::vector<int>& ints, EffectFn fn) {
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(effect_id, 0, owner, -1, EffectArgs(ints), fn);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = effect_id;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
}

// ── 选中技能的效果记录查询（f_ramp_family.cpp 同款私有副本，返回指针以便复核存在性）──
bool selected_skill_has_effect(BattleContext* ctx, int owner, int effect_id) {
    constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL
    if (!ctx || owner < 0 || owner > 1 || ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return false;
    }
    const int slot = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (slot < 0 || slot >= static_cast<int>(skills.size())) {
        return false;
    }
    for (const official_data::SkillEffectRecord& rec :
         skills[static_cast<std::size_t>(slot)].rawEffectRecords) {
        if (rec.effect_id == effect_id) {
            return true;
        }
    }
    return false;
}

// ════════════════════════════════════════════════════════════════════
// 498 —— {0}回合内致命一击概率上升1/{1}
// 挂靠：effect 32 蓄气族（round_window.cpp effect_32_crit_rate_up）同款 crit_rate_add
// 加算通道：+1/{1} = +100/{1} 个百分点。32 的字面是"使用后{0}回合"，498 是"{0}回合内"
// ——同族窗口语义，实现形态照抄 32：当回合立即写 + ROUND_START 窗口 tick 逐回合重写
// （ws 每回合 reset；后手顺延由 register_round_effect 的 round_effect_start_round 折算）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_498_crit_add_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.crit_rate_add[owner] = 100.0f / static_cast<float>(denom);
    return EffectResult::kOk;
}

EffectResult effect_498_crit_rate_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 498, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 498, args,
                     "498 布局=[owner,target,rounds,denom]（int_count>=4）");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && rounds > 0 && denom > 0, 498, args,
                     "需要 rounds>0 且 denom>0");
    ctx->ws.crit_rate_add[owner] = 100.0f / static_cast<float>(denom);   // 当回合立即生效
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 498, rounds,
                          {owner, 1 - owner, denom}, &effect_498_crit_add_tick);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 716 —— {0}回合内对手使用攻击技能，则使对手随机{1}个技能的PP值归零
// 挂靠：受击 watcher（F6 817 同款 EVENT_TAKE_DAMAGE + damage_kind==NORMAL 红伤判据；
// 官方字面省略"自身"，716 全部载体都是属性技——窗口挂在自身上，对手的攻击技能打过来
// 即触发）。清 PP 走 pp_zero_slot 原语（"随机{0}项技能 PP 归零"族专用槽，emit
// EVENT_PP_REDUCED；直写 pp 会绕过事件）。随机 N 项 = 部分 Fisher-Yates 抽不重复槽位
//（对手在场宠的技能槽数 < {1} 时取全部）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_716_pp_zero_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 716, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 716, args,
                     "716 布局=[owner,target,rounds,count]（int_count>=4）");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int count = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && rounds > 0 && count > 0, 716, args,
                     "需要 rounds>0 且 count>0");
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, count](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner) {
                return;
            }
            // "使用攻击技能" = 红伤（E12 口径；粉伤 tick/真伤/属性直伤不算）。
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;
            }
            const int attacker = ev.actor;
            if (attacker < 0 || attacker > 1 || attacker == owner) {
                return;
            }
            if (!g_core || !g_core->pp_zero_slot) {
                return;
            }
            const int slots = static_cast<int>(c->getPet(attacker).skills.size());
            if (slots <= 0) {
                return;
            }
            std::vector<int> idx(static_cast<std::size_t>(slots));
            std::iota(idx.begin(), idx.end(), 0);
            const int n = std::min(count, slots);
            for (int i = 0; i < n; ++i) {
                const int j = choose_skill_range(*c, 716, 0, owner, attacker, i, slots - 1, ChanceSingleton::SkipDraw);
                std::swap(idx[static_cast<std::size_t>(i)],
                          idx[static_cast<std::size_t>(j)]);
                (void)g_core->pp_zero_slot(c, attacker, idx[static_cast<std::size_t>(i)],
                                           owner);
            }
        },
        WatcherScope::TEAM);   // 跨回合 watcher 必须 TEAM（常识速查 §五）
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 672 —— 当回合击败对手则恢复自身全部体力
// 挂靠："击败与否在伤害落地后才可知" → AFTER_ACTION 桥（F4 1106/F6 897 同款双桶
// duration=1 探针）；"未击败"判据取反 = 对手在场宠 hp<=0（897/1218/1307 同款，
// AFTER_ACTION 时点死亡宠物尚未换下）。恢复全额 = heal_amount(max_hp - hp)（heal_impl
// 通道：封回血/恢复修正生效；同归于尽时自身 hp<=0 不结算）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_672_full_heal_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (bucket_acting_side(ctx) != owner) {
        return EffectResult::kOk;   // 只判"自己这次行动"的收尾
    }
    if (ctx->getPet(1 - owner).hp > 0) {
        return EffectResult::kOk;   // 未击败（897 同判据取反）
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;   // 同归于尽：自身不在场，无从恢复
    }
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    (void)g_core->heal_amount(ctx, owner, max_hp_of(ctx, owner) - ctx->getPet(owner).hp);
    return EffectResult::kOk;
}

EffectResult effect_672_defeat_full_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 672, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 672, args,
                     "672 布局=[owner,target]（args_num=0 纯标记）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 672, args, "owner 越界");
    register_after_action_bridge(ctx, owner, 672, {owner, 1 - owner},
                                 &effect_672_full_heal_tick);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 671 —— 若对手处于异常状态则恢复造成伤害的{0}%的体力
// 挂靠：AFTER_ACTION 桥读 resolvedDamage.final（F4 1106 同款：SKILL_EFFECT 桶在
// ATTACK_DAMAGE 之前、读不到 final）+ 身份门 attackerId==owner（防属性技读到对手先攻
// 残留快照）。异常门槛 = 对手任意异常遍历 0..43（F3 1214 同款 has_active_anomaly，
// 自动吃"视为不处于异常"虚拟视图）。恢复 = heal_amount(final×{0}/100)。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_671_damage_drain_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || opp == owner || pct <= 0) {
        return EffectResult::kOk;
    }
    if (bucket_acting_side(ctx) != owner) {
        return EffectResult::kOk;
    }
    if (ctx->resolvedDamage.attackerId != owner) {
        return EffectResult::kOk;   // 身份门：只认自己打的那笔（F4 口径）
    }
    const int final_damage = ctx->resolvedDamage.final;
    if (final_damage <= 0) {
        return EffectResult::kOk;   // 未造成伤害（属性技/被全挡）：无伤可折算
    }
    if (!g_core || !g_core->has_active_anomaly || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    bool foe_abnormal = false;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, opp, id)) {
            foe_abnormal = true;
            break;
        }
    }
    if (!foe_abnormal) {
        return EffectResult::kOk;   // 门槛：对手不处于任何异常 → 不恢复
    }
    (void)g_core->heal_amount(ctx, owner, final_damage * pct / 100);
    return EffectResult::kOk;
}

EffectResult effect_671_anomaly_damage_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 671, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 671, args,
                     "671 布局=[owner,target,pct]（int_count>=3）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 671, args, "owner 越界");
    register_after_action_bridge(ctx, owner, 671,
                                 {owner, 1 - owner, args.int_args[2]},
                                 &effect_671_damage_drain_tick);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 1117 —— 若自身体力低于最大体力的1/2则先制+2
// 挂靠：CARRY 常驻 TEAM MOVE_RIGHT watcher（F9 482 模板逐字改——先制在选择期/MOVE_RIGHT
// 判定，SKILL_EFFECT 时点太晚；内核 selection 路由是硬编码清单，改内核被禁）。
// tick 读选中技能的 1117 记录（纯标记、无参数）→ hp×2 < max_hp（"低于1/2"严格）→
// preemptive_level += 2。全 31 载体通用、当回合生效（先制比较在两个 MOVE_RIGHT 桶
// 跑完之后）。BATTLE_FIRST_ACTION_START 桶只对先手方且出手时执行（battleFsm
// execute_registered_actions 守门）——本效果在 MOVE_RIGHT 桶、先于该守门，符合官方
// "条件满足即可拿先手"的语感（嗑药回合先手权照判，只是没有后续行动）。
// ════════════════════════════════════════════════════════════════════
constexpr int k1117ArmKeyBase = 990551100;   // +owner：watcher 已挂旗标（防重挂）

EffectResult effect_1117_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!selected_skill_has_effect(ctx, owner, 1117)) {
        return EffectResult::kOk;   // 非载体技：无先制可加
    }
    const int max_hp = max_hp_of(ctx, owner);
    if (ctx->getPet(owner).hp * 2 >= max_hp) {
        return EffectResult::kOk;   // 未"低于最大体力的1/2"（严格）
    }
    ctx->ws.preemptive_level[owner] += 2;
    return EffectResult::kOk;
}

void carry_1117_arm_priority_watcher(BattleContext* ctx, int owner, int skill_slot,
                                     int effect_id) {
    (void)skill_slot;
    (void)effect_id;
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    const int key = k1117ArmKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return;   // 重入幂等（roster 多携带者/复扫防双挂）
    }
    ctx->plugin_storage[key] = 1;
    Effect e(1117, 0, owner, -1, EffectArgs(std::vector<int>{owner}),
             &effect_1117_priority_tick);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_MOVE_RIGHT, owner,
                                                 /*duration=*/-1, ctx->roundCount);
    ce->source_id_ = 1117;
    ce->scope_ = EffectScope::TEAM;   // 活过换宠；duration<0 常驻、不可被断回合终结
    ctx->register_skill_effect(State::BATTLE_FIRST_MOVE_RIGHT, owner, std::move(ce));
}

// ════════════════════════════════════════════════════════════════════
// 923 —— {0}回合内每回合若先出手则吸取对手{1}点体力
// 挂靠：窗口 tick 挂 **BATTLE_FIRST_ACTION_START** 桶——battleFsm 该桶只对
// resolve_first_mover_id 且 roundChoice==SELECT_SKILL 的一方执行（execute_registered_actions
// 守门），桶跑 = "本回合先出手且真的出了招"，零额外判据。窗口 InRounds
//（register_round_effect 折算：后手顺延 +1）；
// 用技当回合 R 的 FIRST_ACTION_START 桶在 SKILL_EFFECT 之前已跑过 → 首跳补当回合：
// acting_side_is_first（F5 判据，SKILL_EFFECT 时先手权已定）成立才吸。合计恰 {0} 次
// 机会（先手：R 首跳 + R+1..R+{0}-1 tick；后手：R+1..R+{0} tick，R 本来就没先手）。
// 吸取 = deal_pink_damage(FIXED) + heal_amount 等量（F3 1214 同款双原语形态；
// drain_hp 原语是 max_hp 分数档、固定点数版未暴露）。目标已亡不结算。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_923_drain_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || opp == owner || amount <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 目标已阵亡（837/1148 同款）
    }
    if (!g_core || !g_core->deal_pink_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, opp, amount, DamageKind::FIXED, owner);
    (void)g_core->heal_amount(ctx, owner, amount);
    return EffectResult::kOk;
}

EffectResult effect_923_first_strike_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 923, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 923, args,
                     "923 布局=[owner,target,rounds,amount]（int_count>=4）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner
                         && rounds > 0 && amount > 0, 923, args,
                     "需要 owner/target ∈ {0,1} 且 rounds>0 且 amount>0");
    // 首跳：用技当回合已先出手（R 的 FIRST_ACTION_START 桶在此之前已跑过）。
    if (acting_side_is_first(ctx)) {
        effect_923_drain_tick(ctx, EffectArgs(std::vector<int>{owner, opp, amount}));
    }
    // 后续回合：桶语义自带"先手且出手"过滤（见小节注释）。
    register_round_effect(ctx, State::BATTLE_FIRST_ACTION_START, owner, 923, rounds,
                          {owner, opp, amount}, &effect_923_drain_tick);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 762 —— {0}回合内每回合恢复自身已损失体力的{1}%
// 挂靠：tick = 回合扣减点（F7 76 dot 同款"每回合结束后"时点——762 官方字面**无**
// "使用技能"限定（707 有、762 无），所以不做 used_skill 门、也不用 AFTER_ACTION 桶）；
// 窗口 InRounds（先手当回合即结算第 1 次、后手 round_effect_start_round 顺延）。
// 已损失体力 = max_hp - hp（F8 455 同口径）；恢复走 heal_amount（封回血/修正生效）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_762_regen_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;   // 已阵亡不结算
    }
    const int lost = max_hp_of(ctx, owner) - ctx->getPet(owner).hp;
    if (lost <= 0 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    (void)g_core->heal_amount(ctx, owner, lost * pct / 100);
    return EffectResult::kOk;
}

EffectResult effect_762_regen_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 762, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 762, args,
                     "762 布局=[owner,target,rounds,pct]（int_count>=4）");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && rounds > 0 && pct > 0, 762, args,
                     "需要 rounds>0 且 pct>0");
    register_round_effect(ctx, State::BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS, owner, 762,
                          rounds, {owner, 1 - owner, pct}, &effect_762_regen_tick);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 952 —— 全属性+{0}，若对手处于能力下降状态则效果翻倍
// 挂靠：all_stats_up（common.h 助手，六槽含命中，F5 1344 同款）+ 对手负等级遍历门槛
//（F8 1244 的 foe_dropped 同判据：ability_levels 六槽任一 < 0）→ 再写一次。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_952_all_stats_double_if_foe_dropped(BattleContext* ctx,
                                                        const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 952, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 952, args,
                     "952 布局=[owner,target,up]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int up = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 952,
                     args, "owner/target 越界");
    if (up <= 0) {
        return EffectResult::kOk;
    }
    all_stats_up(ctx, owner, up);
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            all_stats_up(ctx, owner, up);   // 对手处于能力下降状态：效果翻倍
            break;
        }
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 1075 —— 恢复自身最大体力的1/{0}，自身体力低于1/{1}时回满
// 挂靠：heal_max_hp_fraction（common.h 助手）+ 低血门槛分支（707 的 low 判定同式：
// hp < max_hp/{1} 严格）。"回满" = heal_amount(max_hp - hp)。低血分支优先判定。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_1075_heal_fraction_or_full(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1075, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1075, args,
                     "1075 布局=[owner,target,denom,low_denom]（int_count>=4）");
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    const int low_denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && denom > 0 && low_denom > 0, 1075, args,
                     "需要 denom>0 且 low_denom>0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, owner);
    if (ctx->getPet(owner).hp < max_hp / low_denom) {
        (void)g_core->heal_amount(ctx, owner, max_hp - ctx->getPet(owner).hp);   // 回满
        return EffectResult::kOk;
    }
    (void)heal_max_hp_fraction(ctx, owner, denom);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 132 —— 若当前HP低于对手，则技能威力翻倍
// 挂靠：SKILL_EFFECT 时点物化威力视图（F9 1443 同路：view 置 rewritten → 标准变威力
// 重算管线；SKILL_EFFECT 桶在 ATTACK_DAMAGE 红伤之前，"当前HP"即出手时血量）。
// hp 比较 = hp[owner] < hp[opp]（严格）。args_num=0 纯标记，载体全是攻击技。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_132_power_double_lower_hp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 132, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 132, args,
                     "132 布局=[owner,target]（args_num=0 纯标记）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 132,
                     args, "owner/target 越界");
    if (ctx->getPet(owner).hp >= ctx->getPet(opp).hp) {
        return EffectResult::kOk;   // 未"低于对手"（严格）
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * 2;   // 置 rewritten → 标准变威力重算管线（1443 同路）
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(498, effect_498_crit_rate_up);
SKILL_EFFECT(716, effect_716_pp_zero_window);
SKILL_EFFECT(672, effect_672_defeat_full_heal);
SKILL_EFFECT(671, effect_671_anomaly_damage_drain);
SKILL_EFFECT(923, effect_923_first_strike_drain);
SKILL_EFFECT(762, effect_762_regen_window);
SKILL_EFFECT(952, effect_952_all_stats_double_if_foe_dropped);
SKILL_EFFECT(1075, effect_1075_heal_fraction_or_full);
SKILL_EFFECT(132, effect_132_power_double_lower_hp);
CARRY_EFFECT(1117, carry_1117_arm_priority_watcher);

// ════════════════════════════════════════════════════════════════════
// 【跳过记档】484 —— 连击{0}次，每次命中后连击数+{1}，最高连击{2}次（30 技在用）
//
// 为什么跳过：官方语义是**动态连击**——攻击段数随命中逐段增长（月牙之刃 14226=[5,1,10]：
// "连续攻击对手5次，每次使用攻击次数+1，最大攻击次数10次"），需要"每段结算后修改
// 本技能剩余段数"的内核原语。内核现状（skills.cpp combo_arg_indices 注释 + effect_meta
// Combo 分类清单）明确把 484 列入"**未覆盖：动态加数 484/1108/1795/1863/1930（基数固定、
// 靠计数器加）**"——静态区间档（combo_min/combo_max，loadSkills 一次性读取）表达不了
// "命中后加段"，插件侧无可写挂点（Skills 执行循环的段数在 execute 时已固化，
// SKILL_EFFECT/MOVE_RIGHT 时点改 combo 字段要么太晚要么不逐段生效）。禁改内核 →
// 按工单预案跳过并记档。将来落地路径：内核把 484 纳入 combo 执行循环的"逐段回调"
// （或 custom_effect_overrides(override_type='combo') 认证数据层开关 + 段数原语），
// 本文件补一个薄 handler 即可（切片 [base,step,cap] 已在 moves 表侧备好）。
// 30 个载体不受影响照常出招（连击退化为单体一次攻击），registry 不计本效果。
// ════════════════════════════════════════════════════════════════════
