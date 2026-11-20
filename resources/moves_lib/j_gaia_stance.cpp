// J11 · 武道衍化姿态机族 —— 盖亚 4766/4758 专属（2026-10-01，worktree ../agents/j11）
//
// 官方机制（4399 魂印原文/盘点 §3.2）：盖亚自带"衍化轨迹"，**正转/逆转两档**，
// 技能切档并按档位改效果。本文件只落 **技能侧 7 宏**（+同 helper 顺带 779）：
//
//   2294（args_num=0）"衍化自身的武道"        → 姿态取反（正转↔逆转）+ 衍化强化
//   2292（1 参）"改为逆转并把正转期间抵挡的
//             伤害以固伤反馈，上限自身最大
//             体力的{0}%"                    → 姿态=逆转 + 读账反爆 + 清账
//   2293（1 参）"复原自身的武道衍化轨迹，
//             下{0}次衍化时全属性+1"          → 姿态=正转 + 强化次数={0}
//   2291（3 参）"正转时{0}回合对手无法通过
//             技能恢复体力；逆转时{1}回合
//             对手造成的攻击伤害减少{2}%"     → 按姿态分支
//   2298（0 参）"自身不处于能力提升状态时
//             先制+2"                        → 选择期条件先制
//   2300（2 参）"攻击伤害被限制时{0}%令
//             对手{1}"                        → ATTACK_BLOCKED watcher + 申报
//   2301（1 参）"{0}回合内自身免疫对手的攻击" → DAMAGE 免疫窗（570 票系窗口型）
//   779（0 参）"若对手处于能力提升状态则
//             先制+2"（37911 链上，盘点与 2298
//             并列同缺口）                    → 选择期条件先制（与 2298 同路由）
//   （2299"消回成功→攻击技 PP 归 0"已由 J1 j_purge_round.cpp 落地，本文件不重复注册。）
//
// ── 设计（2026-10-01 用户裁定：插件线，不改内核）──
// · **姿态/账本/强化次数/武装旗宿主（2026-11-10 K449 迁槽）**：ctx->plugin_storage
//   (battle 全局，逐 side) → **getPet(side).on_stage_storage**（"本次上场"槽，
//   perform_switch 引擎自动清=battleFsm.cpp:609）——下场重置语义落地；armed 旗
//   随槽清 = 换宠再登场自动重武装 CAP/BLOCK 记账观察者（修 J11 潜伏缺陷：旧旗
//   换宠不清而观察者已随 pipeline epoch 作废，再登场挡伤永不入账）。键段与
//   存取器见 ../gaia_wudao_runtime.h（与 K449 soul 2377 三道轮回机共享）。
//   例外：2300 watcher 句柄留 plugin_storage——该 watcher 是 WatcherScope::TEAM
//   （切换保留），句柄随槽清会丢旧句柄无法换绑 → 重复用技叠加（头内详注）。
// · **挡伤记账 = CAP 快照 + BLOCK 确认两节点配对**：裁定原文写 EVENT_ATTACK_BLOCKED
//   watcher，但该事件 emit 于 final 已归零**之后**且 amount 恒 0
//   （battleFsm.cpp apply_resolved_damage）——金额拿不到，记账不成立。改走设计定档
//   §2.2 本就点名的 2292 接入形状（816/2360 BLOCK 相位先例）的**观察者变体**：
//   CAP 相位（锁伤票后注册→快照即"进入 BLOCK 前的值"，锁到 0 的"限制"不记账——
//   与 2300"被限制"口径正好分流）记 pending，BLOCK 相位（默认挡票归零后）确认
//   final<=0 且 pending>0 → 记账。观察者节点挂 AMP 类别（不吃"挡伤失效"抑制——
//   票被抑制时 final>0、确认自然不中，行为自洽）。
// · **2292 反爆 = 读账发固伤**，min(账, 自身最大体力×{0}%)，g_core->fixed_damage。
// · **2298/779 条件先制 = 选择期路由（skills.cpp 选择期表 → on_selected 注册
//   MOVE_RIGHT 桶）**：天生先制同族——点击即判定、先手权比较前生效、被控回合照跑
//   （2026-10-02 改版：原"用技后注册 MOVE_RIGHT 节点"首回合吃不到加成，引擎本有
//   选择期路由正位，用户指正后迁入；效果来源=技能同源去重，直接写先制等级）。
// · 2291 逆转窗 = ws.damage_add_pct[opp] 负值（AMP 加算通道，负=减伤；ws 每回合
//   reset → 当回合直写 + ROUND_START tick 重写，1111 兜底窗口同款）。
// · 2293 的"复原"不算衍化（不消费强化次数）；2292 改轨迹**算**衍化（消费）——
//   官方语序"下 N 次**衍化**时"，改轨迹/取反均为衍化，记档。
//
// 前置三查（2026-10-01）：①内核 grep 2291/2292/2293/2294/2298/2300/2301/779 无行为
//   硬编码（挡伤票据定档 §一 曾记 2292"缺武道衍化状态机"跳过）②effect_meta 无该批
//   id 声明 ③moves_lib 无既有注册（2299 除外——J1 已落）。载体核对：37909/37910/
//   37911/29045/29046 五技切片逐条吻合（2292=[150]、2291=[2,2,50]、2301=[2]、
//   2300=[100,6]，与盘点口径一致）；2301 无 379xx 载体、由第五技 29045 战·神罡不灭
//   携带。全族载体 5 技（盖亚专属，无皮肤污染）。
// 先例：816/2360（BLOCK 相位金额）、994（storage 记账/同键替换）、843/499（MOVE_RIGHT
//   先制 tick）、1111（ws 重写窗口）、1449（HEAL_BLOCK 票）、570/2096（DAMAGE 免疫窗）、
//   1293（watcher 自删换绑）、J3（申报制 apply_anomaly_checked）。
// 场景回归：test/scenario/scenario_963_gaia_stance.cpp（960-969 号段）；
// 下场语义/三道轮回联动：test/scenario/scenario_8370_gaia_wudao_2377.cpp（8370 号段）。

#include "common.h"

#include "../gaia_wudao_runtime.h"

#include <memory>

namespace {

// ── 键段（2026-11-10 K449 迁槽）：五键族宿主 getPet(side).on_stage_storage
// （键常量与 inline 存取器集中在 ../gaia_wudao_runtime.h——跨 dylib 与 soul 2377
// 共享姿态槽；本文件不再直接索引存储）。例外见下：2300 watcher 句柄留
// plugin_storage（TEAM watcher 切换保留，句柄随槽清会丢旧句柄无法换绑）。──
constexpr int kWatcher2300 = kGaiaW2300HandleKey;   // plugin_storage 句柄槽（换绑用）

// storage 整数读写：on_stage_storage per-pet 槽（gaia_pet_int 集中在共享头；
// 类型不符按缺失回退，绝不抛——any_cast 治理纪律同旧 j11_stor）。
inline int j11_stor(const BattleContext* ctx, int side, int key) {
    return gaia_pet_int(ctx, side, key);
}

inline int j11_stance(const BattleContext* ctx, int side) {
    return gaia_stance(ctx, side);
}

// ── 挡伤记账（CAP 快照 + BLOCK 确认；只武装一次，正转才入账）──
// K449 迁槽：armed 旗进 on_stage_storage——换宠由引擎清槽 → 再登场首次宏触发
// 到这里时旗=0 重武装（修"旗在观察者亡"潜伏缺陷）；同次在场期内旗=1 幂等。
void j11_ensure_accounting(BattleContext* ctx, int self) {
    if (!ctx || j11_stor(ctx, self, kGaiaArmedKey) != 0) {
        return;
    }
    gaia_pet_set(ctx, self, kGaiaArmedKey, 1);
    auto pending = std::make_shared<int>(0);
    // CAP 相位观察者：默认锁伤票（init 安装，先跑）之后注册 → 快照 = 进入 BLOCK 前的值。
    // 属性直伤（skip_routine_block）与粉伤（不走本管线）不入账。
    ctx->register_damage_effect(
        DamagePhase::CAP, self, DamageEffectCategory::AMP,
        [self, pending](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            *pending = (d.defenderId == self && d.isRed && !d.skip_routine_block
                        && d.final > 0)
                           ? d.final
                           : 0;
        });
    // BLOCK 相位确认：默认挡票归零后跑 → final<=0 且进门前 >0 ⇒ 这一下被"抵挡"了。
    ctx->register_damage_effect(
        DamagePhase::BLOCK, self, DamageEffectCategory::AMP,
        [self, pending](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId == self && d.isRed && !d.skip_routine_block && *pending > 0
                && d.final <= 0 && j11_stance(c, self) == 0) {
                gaia_pet_set(c, self, kGaiaAccountKey,
                             j11_stor(c, self, kGaiaAccountKey) + *pending);  // 正转期间入账
            }
            *pending = 0;
        });
}

// ── 2294 —— 衍化自身的武道（2026-11-10 des 449 校正）──
// 旧实现=姿态取反（正转↔逆转），依据只是旧 963 断言锁死——与魂印触发衍化
// （推进道位）拆成互不兼容两动作，被主会话实锤为存量缺陷。现行 effect_des 449：
// 「武道的衍化遵循精灵所声明的衍化轨迹，当武道没有可衍化的对象时，武道的衍化
// 改为关闭」→「衍化」= 沿声明轨迹推进一道（+消费 2293 次数 + 走完轨迹回满 PP +
// 无后继衍化关闭），与魂印触发衍化共用 gaia_wudao_evolve（同一动作单一实现）。
// 方向由 2292（改为逆转）/2293（复原）管理，2294 不再触碰姿态。
EffectResult effect_2294_evolve(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2294, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2294, args,
                     "2294 布局=[owner,target]");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 2294, args, "需要 owner ∈ {0,1}");
    j11_ensure_accounting(ctx, self);
    (void)gaia_wudao_evolve(ctx, self,
                            g_core ? g_core->stat_change : nullptr,
                            g_core ? g_core->pp_restore : nullptr);
    return EffectResult::kOk;
}

// ── 2292 —— 改逆转 + 把正转期间抵挡的伤害以固伤反馈，上限自身最大体力{0}% ──
EffectResult effect_2292_invert_reflect(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2292, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2292, args,
                     "2292 布局=[owner,target,上限%]");
    const int self = args.int_args[0];
    const int opp = 1 - self;
    const int cap_pct = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && cap_pct > 0, 2292, args, "需要 上限%>0");
    j11_ensure_accounting(ctx, self);
    // 改为逆转：轨迹方向切到逆转序（[0,2,1,0]），道位值不变、轨迹位置按道位值在
    // 新序中的首次出现重映射（建模决策记档：官方未写改向时轨迹位置的换算——
    // 「改为逆转」字面无衍化语义 → 不消费 2293 次数，旧「改轨迹算衍化」记档随
    // des 449 模型废除）。
    gaia_pet_set(ctx, self, kGaiaStanceKey, 1);
    {
        const int way_now = gaia_way(ctx, self);
        const int rev[4] = {0, 2, 1, 0};
        int mapped = 0;
        for (int i = 0; i < 4; ++i) {
            if (rev[i] == way_now) {
                mapped = i;
                break;
            }
        }
        gaia_pet_set(ctx, self, kGaiaTrajPosKey, mapped);
    }
    const int account = j11_stor(ctx, self, kGaiaAccountKey);
    gaia_pet_set(ctx, self, kGaiaAccountKey, 0);   // 读账即清
    if (account <= 0 || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int cap = max_hp_of(ctx, self) * cap_pct / 100;
    const int reflect = account < cap ? account : cap;
    if (reflect > 0) {
        (void)g_core->fixed_damage(ctx, opp, reflect);
    }
    return EffectResult::kOk;
}

// ── 2293 —— 复原轨迹（正转）+ 下{0}次衍化时全属性+1 ──
EffectResult effect_2293_reset(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2293, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2293, args,
                     "2293 布局=[owner,target,次数]");
    const int self = args.int_args[0];
    const int times = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && times > 0, 2293, args, "需要 次数>0");
    j11_ensure_accounting(ctx, self);
    // 复原自身的武道衍化轨迹：方向回正转 + **轨迹重启**（位置回起点、道位回血道、
    // 衍化关闭态解除——des 449「无后继→衍化关闭」的逆操作）；不算衍化，不消费次数。
    gaia_pet_set(ctx, self, kGaiaStanceKey, 0);
    gaia_pet_set(ctx, self, kGaiaTrajPosKey, 0);
    gaia_pet_set(ctx, self, kGaiaWayKey, 0);
    gaia_pet_set(ctx, self, kGaiaEvolveKey, times);
    return EffectResult::kOk;
}

// 2291 逆转窗 tick：每回合把对手攻击伤害 -{2}% 重写回 AMP 加算通道（ws 每回合 reset）。
EffectResult j11_invert_amp_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    if (opp < 0 || opp > 1 || pct >= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[opp] += pct;
    return EffectResult::kOk;
}

// ── 2291 —— 正转：{0}回合对手无法通过技能恢复体力；逆转：{1}回合对手攻击伤害-{2}% ──
EffectResult effect_2291_stance_conditional(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2291, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 2291, args,
                     "2291 布局=[owner,target,正转回合,逆转回合,逆转减伤%]");
    const int self = args.int_args[0];
    const int opp = 1 - self;
    const int rounds_forward = args.int_args[2];
    const int rounds_reverse = args.int_args[3];
    const int pct_reverse = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 2291, args, "需要 owner ∈ {0,1}");
    j11_ensure_accounting(ctx, self);
    if (j11_stance(ctx, self) == 0) {
        // 正转：HEAL_BLOCK 票挂对手（1449 同款，InRounds {0} 回合）
        ctx->grant_immunity(opp, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                            /*duration_rounds=*/rounds_forward, /*source_id=*/2291,
                            /*soul_immunity=*/false, EffectScope::ON_STAGE,
                            /*counts=*/0, /*source_effect_id=*/2291);
        return EffectResult::kOk;
    }
    // 逆转：对手攻击伤害 -{2}%（当回合直写 + {1} 回合 ROUND_START 重写，1111 窗口法）
    if (rounds_reverse > 0 && pct_reverse > 0) {
        ctx->ws.damage_add_pct[opp] -= pct_reverse;
        register_round_effect(ctx, State::BATTLE_ROUND_START, self, 2291, rounds_reverse,
                              {self, opp, -pct_reverse}, &j11_invert_amp_tick);
    }
    return EffectResult::kOk;
}

// ── 2298 —— 自身不处于能力提升状态时先制+2 ──
// ★ **选择期效果**（skills.cpp 选择期路由表 → on_selected 注册 MOVE_RIGHT 桶）：
//   点击即判定、先手权比较前生效（2000/610/954/881 同族正位）；效果来源=技能
//   （同源去重不叠加），args[0/1] 由 on_selected 绑定 owner/对手，被控回合照跑
//   （束缚的清零点在桶之后，那是官方"先制效果失效"的正主）。
EffectResult effect_2298_cond_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2298, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2298, args,
                     "2298 布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 2298, args, "需要 owner ∈ {0,1}");
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[self][i] > 0) {
            return EffectResult::kOk;   // 处于能力提升状态 → 不加先制
        }
    }
    ctx->ws.preemptive_level[self] += 2;
    return EffectResult::kOk;
}

// ── 779 —— 若对手处于能力提升状态则先制+2（37911 链上，选择期路由同 2298）──
EffectResult effect_779_cond_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 779, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 779, args,
                     "779 布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    const int opp = 1 - self;
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 779, args, "需要 owner ∈ {0,1}");
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] > 0) {
            ctx->ws.preemptive_level[self] += 2;   // 对手有提升 → +2
            return EffectResult::kOk;
        }
    }
    return EffectResult::kOk;
}

// ── 2300 —— 攻击伤害被限制时{0}%令对手{1}（自己的攻击被拦/锁/归零 → 申报）──
EffectResult effect_2300_limited_punish(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2300, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2300, args,
                     "2300 布局=[owner,target,概率,异常码]");
    const int self = args.int_args[0];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 2300, args, "需要 owner ∈ {0,1}");
    // 常驻 watcher：同键换绑（1293 自删思路——storage 存旧句柄，重复用技不叠加）。
    const int handle_key = kWatcher2300 + self;
    // 句柄槽留 plugin_storage（TEAM watcher 切换保留——迁槽特例，见共享头注）。
    const auto hit = ctx->plugin_storage.find(handle_key);
    const int old = hit == ctx->plugin_storage.end() ? 0 : gaia_int_or(hit->second);
    if (old > 0) {
        ctx->event_center_.remove_watcher(old);
    }
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_ATTACK_BLOCKED, self, ctx->roundCount, /*rounds=*/0, /*once=*/false,
        [self, chance, anomaly](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != self) {
                return;   // 只关心"自己的攻击被拦下"（含锁伤/归零/免疫各档）
            }
            const int target = ev.target;
            if (target < 0 || target > 1 || target == self || !g_core
                || !g_core->apply_anomaly_checked || chance <= 0
                || !is_valid_abnormal_status_id(anomaly)) {
                return;
            }
            (void)g_core->apply_anomaly_checked(c, target, anomaly, random_anomaly_duration(),
                                                self, chance, ChanceSource::Skill);
        },
        WatcherScope::TEAM);
    ctx->plugin_storage[handle_key] = watcher_id;
    return EffectResult::kOk;
}

// ── 2301 —— {0}回合内自身免疫对手的攻击（DAMAGE 免疫窗，570 票系窗口型）──
EffectResult effect_2301_attack_immune_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2301, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2301, args,
                     "2301 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0, 2301, args, "需要 回合>0");
    j11_ensure_accounting(ctx, self);
    ctx->grant_immunity(self, ImmunityType::DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds, /*source_id=*/2301,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/2301);
    return EffectResult::kOk;
}

// ══ 变异测试（6 变异全部真转红，2026-11-10 本 worktree 实测：改→rm .o 强制重编+
//    显式 deploy→单跑 8370 必红（红点全部落在预期断言腿）→字节级还原（cmp+md5 双验
//    858add42bfce995a7f9fcc8ecce9ea0f / soul 2ab2004af4ac97c4b1a40be3cdc1deb3）→
//    rm .o 重编+deploy→回绿 SBS_SCENARIO_CHECKS=84）══
//   MK1 2575 门取反 → T8373a 对手开启旗红（消除反向触发）——门方向锁定；
//   MK2 2575 消除半边空转（旗不清）→ T8373a 开启旗红——「消除武道」引擎落点锁定；
//   MK3 2575 锁行窗 +1 → T8373b r3 解锁腿红——{1} 回合窗精确锁定；
//   MK4 2574 过期轮 +1 且漏对手侧 → T8374 对手侧查询红——双方场地盾+{0}=3 锁定；
//   MK5 2377 抵挡回调删开启旗门（soul 侧）→ T8373a r0 红伤判别式红（hp==max-附加，
//     红伤被清）——「消除武道→抵挡停止」锁定（判别式含双重伤害：附加必落+红伤
//     落地，单断言 hp<max 会被附加伤害淹没——记档防坑）；
//   MK6 2575 门丢不武分支 → T8373c 衰弱腿红——des 569「不武额外视为武道」锁定。
//
// ════════════════════════════════════════════════════════════════════
// K449 续作（2026-11-10）—— 不灭战魂·盖亚 4947 moves 两宏
//（载体：38540 战·混罡乱舞 [697,699,2575,1043,864]|[2,2,2,1,2] → 2575=[2,2]、
//  38541 战·神破天闪 [760,779,1776,2574,1156]|[2,3,3] → 2574=[3]、
//  38538 雷·神天明闪（4946 hide）[760,2573,1885,2574,2591,2017]|[3,100,19,3,1,100]
//  → 2574=[3]——双载体同宏；json_each + arg_count 双表切片核，主会话复核）
// ════════════════════════════════════════════════════════════════════

// ── 2575 —— 自身拥有武道时，消除对手的武道并令对手衰弱{0}回合，
//             未触发则对手{1}回合内无法行动 ──
// 门判据「自身拥有武道」（官方 2575 模板原文主语=**自身**——2547② 的「场上存在
// 武道」才是双方在场检测，两句分开）：自身侧 轨迹武道开启旗（2377）∥ 胜之道
// （soul 2547，预留键）∥ 不武≥1（des 569「不武额外视为武道」；side 级计数
// 990112981+side——2026-11-10 修与 2300 句柄 side1 的键碰撞；soul 2547 ①写——
// ①率查询通道阻塞未实现，计数器现恒 0，场景可播种做确定性腿）。
// 触发：「消除对手的武道」= 清对手在场宠轨迹旗 + 胜之道旗（2377 抵挡观察器以开启旗
//   为门 → 消除后抵挡/衍化停止）+ 「令对手衰弱{0}回合」= apply_anomaly 现代通道
//   全检查链（Weakness=11，弱化类，explicit {0} 回合；2086 apply_anomaly 直系）。
// 未触发：「对手{1}回合内无法行动」= effect 39 族引擎旗（suppress_extra_action，
//   主流程与额外行动一并跳过、回合完成自动清）+ 下 {1} 回合 ROUND_START tick 重锁
//   （NextRounds 窗、条目属对手方——目标换宠即失效，2088「对手切换失效」同族口径
//   记档）。⚠️ 官方现行 BUG「盖亚没有武道时也能触发2回合湮灭效果」（corpus idx488
//   「等修复吧」）**不复刻**——按模板未触发分支语义落地。
EffectResult j2575_lock_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int target = args.int_args[0];
    if (target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    ctx->suppress_extra_action(target);
    return EffectResult::kOk;
}

EffectResult effect_2575_wudao_purge_weakness(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2575, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2575, args,
                     "2575 布局=[owner,target,衰弱回合,锁行动回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int weak_rounds = args.int_args[2];
    const int lock_rounds = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     2575, args, "需要 owner/target ∈ {0,1} 且异侧");
    if (!gaia_side_has_wudao(ctx, self)) {
        // 未触发：对手 {1} 回合内无法行动（当回合即锁 + 下 {1} 回合重锁）。
        if (lock_rounds > 0) {
            ctx->suppress_extra_action(opp);
            register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, opp, 2575,
                                        lock_rounds, {opp}, &j2575_lock_tick);
        }
        return EffectResult::kOk;
    }
    // 触发：消除对手的武道 + 衰弱 {0} 回合（现代通道全检查链）。
    gaia_pet_set(ctx, opp, kGaiaActiveKey, 0);
    gaia_pet_set(ctx, opp, kGaiaShengKey, 0);
    if (weak_rounds > 0 && g_core && g_core->apply_anomaly) {
        (void)g_core->apply_anomaly(ctx, opp,
                                    static_cast<int>(AbnormalStatusId::Weakness),
                                    weak_rounds, self);
    }
    return EffectResult::kOk;
}

// ── 2574 —— {0}回合内，双方的在场精灵无法成为场下精灵的效果目标 ──
// 场地盾：两侧各记过期轮（plugin_storage +side，**场地级跨换宠**——盾属场地不属
// 精灵）。roundCount < 过期轮 = 活跃（r0 授予、{0}=3 → r0/r1/r2 活跃、r3 过期，
// 与票 tick 窗同构）。消费面 = 场下精灵效果对在场目标施加前的共享查询
// gaia_bench_target_locked（runtime 头；soul 2547 ①率通道阻塞未实现，查询面先行
// 落地供后续挂接）。
// tick：逐轮重写双方 stamp（绝对轮戳——条目死亡（施放方换宠 epoch 作废/被断回合/
// 到期）后戳冻结 ≤ roundCount → gaia_bench_target_locked 自假，离场/断回合/到期
// 三路语义由普通 N 回合窗条目生命周期自动承载）。
// args=[grant_round_1based, N]：当前轮号 ≤ grant+N-1（守窗轮）→ 重写戳=当轮号；
// 当前轮号 = grant+N（首失守轮，NextRounds 第 N 拍恰落在它的 ROUND_START）→ 清零
// （失守轮当场查询即假，不等下一轮）。条目死亡（离场/断回合）→ 停写冻结 → 查询自假。
EffectResult j2574_shield_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int grant = args.int_args[0];
    const int n = args.int_args[1];
    const int cur = ctx->roundCount + 1;          // 当轮绝对轮号（roundCount 滞后一轮）
    const int stamp = (cur <= grant + n - 1) ? cur : 0;
    gaia_side_set(ctx, 0, kGaiaShield2574Key, stamp);
    gaia_side_set(ctx, 1, kGaiaShield2574Key, stamp);
    return EffectResult::kOk;
}

EffectResult effect_2574_bench_target_shield(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2574, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2574, args,
                     "2574 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0, 2574, args, "需要 回合>0");
    // 2291 同款普通窗：当回合直写 stamp + NextRounds {0} 轮 ROUND_START tick 重写
    // （条目属施放方）。消费面（2026-11-10 三次修正）：首个真实消费者已接线 =
    // j_defeat_family.cpp 349 支援回血门（行为正负例 T8374d）；其余场下→场上入口
    // 未接（覆盖矩阵见 K449 完成报告），**不称条款完整**。
    const int grant = ctx->roundCount + 1;        // 当轮绝对轮号（滞后一轮 +1 归位）
    gaia_side_set(ctx, 0, kGaiaShield2574Key, grant);
    gaia_side_set(ctx, 1, kGaiaShield2574Key, grant);
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 2574, rounds,
                                {grant, rounds}, &j2574_shield_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2294, effect_2294_evolve)
SKILL_EFFECT(2292, effect_2292_invert_reflect)
SKILL_EFFECT(2293, effect_2293_reset)
SKILL_EFFECT(2291, effect_2291_stance_conditional)
SKILL_EFFECT(2298, effect_2298_cond_priority)
SKILL_EFFECT(779, effect_779_cond_priority)
SKILL_EFFECT(2300, effect_2300_limited_punish)
SKILL_EFFECT(2301, effect_2301_attack_immune_window)
SKILL_EFFECT(2574, effect_2574_bench_target_shield)
SKILL_EFFECT(2575, effect_2575_wudao_purge_weakness)
