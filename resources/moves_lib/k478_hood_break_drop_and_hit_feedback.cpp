// K478 · 护罩消散降属 + 受击反馈两族 —— 1446×5 + 658×5（2026-11-18，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k478，分支 feat/k478，基点 7968ed04
// 含 K461-K477 主线实现；场景 test/scenario/scenario_8650_k478.cpp，号段 8650-8659）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-18）══
//   1446（args_num=2）"获得{0}点护罩，护罩消失时使对手全属性-{1}"
//   658 （args_num=1）"下次受到攻击伤害后于对手出手流程结束反馈伤害值{0}%的百分比伤害"
//
// ══ 载体切片（unity 库 side_effect.arg_count 顺序切分，2026-11-18 全载体自洽零剩余；
//   实现读行参不硬编码）══
//   1446 [{0}=护罩点数, {1}=全属性降级数]（5 载体；{0}∈{300,400,500}、{1}∈{1}）：
//     26581 永恒咒印 [885,1239,1446]→arg[1,1,2,300,1] 切 885=[1]|1239=[1,2]|**1446=[300,1]**（cat4）、
//     27363 花灵庇护 [1446,191,1235]→arg[500,1,4,3,80,1] 切 **1446=[500,1]**|191=[4]|1235=[3,80,1]、
//     28258 川岳凝合 [191,1753,1446]→arg[4,400,3,400,1] 切 191=[4]|1753=[400,3]|**1446=[400,1]**、
//     36314 煌龙之狩 [5,5,1446,1461]→arg[0,100,-2,2,100,-2,300,1,3] 切 5=[0,100,-2]|5=[2,100,-2]|
//       **1446=[300,1]**|1461=[3]（cat2 攻击载体，power110）、
//     36318 煌龙之噬 [5,5,1446,1461]→arg[4,100,-2,5,100,-2,300,1,3] 切 5=[4,100,-2]|5=[5,100,-2]|
//       **1446=[300,1]**|1461=[3]（cat2）。
//   658 [{0}=反馈百分比]（5 载体；{0}∈{80,100}）：
//     19314 剑挥四方 [658]→[100]（cat2 特攻120）、
//     23514 万夫不当 [658]→[100]（cat4，priority2、max_pp=1——单用 stance）、
//     23864 冰霜降落 [658,656]→arg[80,50,2] 切 **658=[80]**|656=[50,2]、
//     24328 森林伪装 [658,48,187,694]→arg[100,4,3,3,2] 切 **658=[100]**|48=[4]|187=[3,3]|694=[2]、
//     24740 龙威驰骋 [47,658,165]→arg[4,100,3,2] 切 47=[4]|**658=[100]**|165=[3,2]。
//   4399 载体原文（pets_full.jsonl 现查）：26581"为自己附加一个护罩"、27363"使自身获得
//     1 个护罩"、28258"为自身附加一些护盾和护罩"（1753 护盾+1446 护罩同链成对）；24740
//     "将**下一次**受到的伤害反馈给对手"、24328"将伤害反馈给对手"、23864"有概率将伤害
//     反馈给对手"（"有概率"指链上 656 的 50% PP 归零，658 本体无概率措辞）。
//
// ══ 1446 实现口径（套路目录 §零决策树①：同形套路照抄先例）══
//   护罩面 = hood_bank_.add(priority=0, {0}, source_id=1446, duration=0) 落**自身**
//     （"获得"=self；J47 1753 盾直系 + K472 1938 盾形先例——hood_bank_ 粉伤槽，
//     battleContext.cpp:336 HOOD 相消费、破罩 emit EVENT_SHIELD_BROKEN）。
//   "护罩消失时"三分辨裁定（板行指令：被打穿/到期/离场逐分辨，不猜）：
//     ① 被打穿 = **触发**。粉伤吸收把 1446 源护罩扣空 → EVENT_SHIELD_BROKEN
//       （battleContext.cpp:338-344 broken>0 emit）→ watcher 落子句。
//     ② 到期 = **结构不可达**：add duration=0（破盾前持续；J47 1753/K472 1938 同款），
//       且 tick_duration 无任何 FSM 调用方（K472 头注实证）→ 模板无持续字样、
//       官方无回合窗证据，无"到期"语义可辨。
//     ③ 离场 = **不触发**。护罩槽挂 ElfPet 本体、perform_switch 不清 bank
//       （battleFsm.cpp:580 全文无 bank 清理）→ 换宠/死亡护罩随宠走、"消失"不发生；
//       回到场上护罩仍在，被打穿照常触发。arm 旗 slot 戳（armed_slot=获罩时
//       on_stage）在触发时校验：不同宠（同侧换宠）在场时收到的破盾事件跳过不消费
//       （旗保留给原宠回场）——键侧防串（K463 epoch 作废同源思想，按需弱化：
//       过期旗惰性滞留无行为面，同 1753 单旗口径）。
//   触发门 = armed 旗 + hood_bank_.quantity_of(1446)==0（**池语义**：本源护罩全空
//     才算"护罩消失"；事件无 bank/source 归属（event_center.h BattleEvent 无此字段，
//     1753 头注同款记档），池读数是唯一可辨面）——半吸收（护罩仍存）不触发、
//     同源重施放叠加后任一条被扣空不触发（最后一条扣空才触发）。
//   ⚠️ 静默清除边界（记档）：消盾类技能（j_clear_shields_pink/batch_c 等）直调
//     hood_bank_.clear() 不 emit 事件——"护罩消失"当场不触发；延迟到本侧下一破盾
//     事件经池读数补触发（旗仍在+池已空）。事件面缺"罩被清"信号，1753 同款
//     "事件无归属待扩展"记档，不私造信号。
//   降属 = 对手（触发时在场对手=1-self）**六槽逐项 stat_drop**（749"全属性-n 含命中"
//     同一直系口径：各查免弱 STAT_DROP、各钳 -6、AT_FLOOR 合法通过态）；
//     全属性语义 = moves_lib/common.h all_stats_up 同款六槽（用户 2026-09-18 口径：
//     能力提升官方就是 6 种含命中，不含体力）。
//
// ══ 658 实现口径 ════════════════════════════════════════════════════════
//   一次性受击哨兵（"下次"=消费即失效）：
//     SKILL_EFFECT（HIT 分支 default：miss/被盔趟不进本函数=不武装——K470 674 同款
//     引擎结构性）置 armed 旗 + slot 戳 + 注册 EVENT_TAKE_DAMAGE watcher
//     （TEAM + duration=0 永久 + once=false + valid_id 显式传）。
//     ⚠️ **once=false + 回调内先过滤后消费**（与 634 once=true 的分岔点）：哨兵须
//       跨回合存活（"下次"可能在下回合），而 once=true 任何投递即焚——载体自身
//       链上前置攻击伤害（19314 cat2 剑挥四方自己打的 TAKE_DAMAGE target=opp）、
//       链外任意粉伤/真伤投递都会误焚。once=false 永久 watcher + armed 旗门 =
//       过滤后才消费，inert 旗滞留无行为面（1753 永久 watcher 同口径）。
//     受击过滤四重（K470 634 直系）：ev.target==self（我受击）+ ev.actor==opp
//       （攻击方是对手）+ ev.damage_kind==NORMAL（"攻击伤害"=红伤；E12 反伤族
//       "只认 NORMAL"先例——粉/真/属性直伤不算）+ ev.state ∈ {FIRST/SECOND
//       ATTACK_DAMAGE}（攻击伤害时点双保险）。
//     基数 = **ev.amount**（TAKE_DAMAGE 载荷=护盾吸收+超杀钳制后实际伤害，K463/K470
//       引擎事实入档同款；护盾全吸收不 emit → 不消耗哨兵）。slot 戳校验同 1446
//       （换宠后他人受击不消费、旗保留）。
//   反馈时点 = **对手出手流程结束**（板行指令 K459/K470 口径甄别）：
//     · 消费（ATTACK_DAMAGE 后 drain）→ 当场注册 AFTER_ACTION 桥（owner=对手侧，
//       BATTLE_FIRST/SECOND_AFTER_ACTION 双注册——桶只跑行动方，实际只命中行动侧
//       一份，k470_register_after_action_once 同式）。K1 晚 drain 已改为逐 State
//       桶后统一 drain（battleFsm.cpp:1320-1322），TAKE_DAMAGE 回调在攻击方
//       ATTACK_DAMAGE 桶后即跑，早于 AFTER_ACTION 桶 → 必须桥到 AFTER_ACTION，
//       直发会提前两个时点（"出手流程结束"甄别=K470 674 同判据链 1307/1218/
//       984/2440/523）。
//     · 桥窗 = **显式 start=当前回合、duration=2**（直构 ContinuousEffect 绕
//       round_effect_start_round 折算）：覆盖"对手本回合 AFTER_ACTION 已过"
//       （红伤弹伤族在己方行动中打到我——对手先手已行动）的下一回合补发，同回合
//       主路径（对手后手）照常当回合 AFTER_ACTION 发。rounds=1 + InRounds 折算在
//       该边角会把窗顺延丢拍，故直构（duration=2 自起点起 [R,R+1]）。
//     · 桥 tick 一次性（pending 槽清账即空转）+ 双存活门（self hp>0/opp hp>0，
//       K470/K441 口径——同归窗口/对手已倒不反馈）。
//   反馈伤害 = deal_pink_damage(FIXED)（**百分比伤害**=粉伤族 FIXED 档；K443 929
//     反弹直系通道：吃护罩/抗性/免粉，emit 对手侧 TAKE_PINK/TAKE_DAMAGE 事件
//     target=opp——自身哨兵 filter ev.target==self 不自激）。反馈量 = base×{0}/100
//     （int 直除，634 吸血同式；{0}∈{80,100} 无小数舍入面）。
//   未触发滞留（记档）：哨兵永不消耗时旗+永久 watcher 惰性滞留（1753 同口径）；
//     桥已注册但对手当回合+下回合均未再走 AFTER_ACTION 桶（如被控 NONE）→
//     pending 滞留至窗过期，反馈作废（窗 [R,R+1] 显式兜一圈——比 rounds=1 少一条
//     丢拍路径，其余按引擎流程面如实记档）。
//   多哨兵并发（记档）：同侧单旗单 pending（1753 单旗/K470 单槽同族）；双载体双
//     消费时后者覆盖 pending（桥同键覆盖）——载体无双哨兵实测面（万夫不当
//     max_pp=1），不立变异。
//
// ══ 审计槽（K440/K462/K470 布局同构：10^9 + id×10^4 + owner×16 + slot）══
//   1446（1014460000）：slot 0 armed 旗 / 1 armed on_stage slot 戳 / 2 护罩点数行参 /
//     3 降级数行参 / 4 触发计数（子句实际执行次数）/ 5 触发时回合数 / 8 累计使用计数 /
//     9 跳过原因（0=无/1=行参关闸/2=缺原语/3=旗过期跳过[slot 戳不符，不清旗]/
//     4=罩槽满落空不武装）
//   658（1000658000）：slot 0 pct 行参 / 1 armed 旗 / 2 armed slot 戳 / 3 pending 基数
//     （ev.amount；-1=无 pending）/ 4 反馈量计算值 / 5 桥 fire 计数 / 6 消费计数
//     （watcher 实际消费哨兵次数——一次性判别面）/ 8 累计使用计数 /
//     9 跳过原因（0=无/1=行参关闸/2=对手已倒不武装/3=缺原语/4=slot 戳不符跳过
//     [不清旗]/5=self 已倒不反馈/6=对手已倒不反馈/7=缺 pending 空转）
//   新增键 1014460000/1000658000 全库 grep 零命中（2026-11-18 现查）+
//   tools/check_storage_keys.py 过门复扫；soul 空间 1446/658 均未注册（registry
//   soul=415 零命中，两族键段无共存拆分需求——预检 soul 同号甄别结论）。
//
// ══ 动工前置三查（2026-11-18，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep：1446/658 裸 id 词界全扫 src/ include/ 零命中（无路由无硬编码）。
//   ② effect_info 两行在列（args_num 2/1=side_effect.arg_count 全载体自洽）；
//     custom_effect_overrides/programs 未查表有声明——真缺口。
//     ⚠️ boss_invalid_effects.json 1446 在册（官方 tips 尾注"boss 无效"机器口径，
//     658 不在册）——引擎 DisabledEffects 装配期整体摘除，插件侧零工作量（记档）。
//   ③ 全库并集无注册：resources/ test/ SKILL_EFFECT(1446|658 零命中；跨 worktree
//     （k479/k480/endgame-walk/web-replay/agents）同款零命中——真缺口。
//
// ══ 确认无拆分迁移与退役记录（板行指令）：tasklog/看板/06-交接 grep 1446/658
//   无"拆分/迁移/退役"记录；boss_invalid 1446 在册为数据面口径非退役。══

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1446AuditBase = 1014460000;   // 10^9 + 1446×10^4
constexpr int k658AuditBase  = 1000658000;   // 10^9 + 658×10^4
constexpr int kSourceHood = 1446;            // 护罩 source_id（池读数定位用）

inline int audit_key(int base, int owner, int slot) { return base + owner * 16 + slot; }

int storage_int_local(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

void set_storage_int_local(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

void erase_storage_local(BattleContext* ctx, int key) { ctx->plugin_storage.erase(key); }

void bump_use_count(BattleContext* ctx, int base, int owner) {
    set_storage_int_local(ctx, audit_key(base, owner, 8),
                          storage_int_local(ctx, audit_key(base, owner, 8), 0) + 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// 1446 —— 获得{0}点护罩，护罩消失时使对手全属性-{1}
// 行参布局 [owner, target, hood, drop]（args_num=2，loader 注入 owner/target 前缀）。
// 护罩落自身 hood_bank_（source_id=1446、duration=0 常驻）；armed 旗 + slot 戳；
// EVENT_SHIELD_BROKEN watcher（TEAM/永久/once=false）：旗+池空双门 → 对手六槽
// stat_drop（749"全属性含命中"直系）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1446_hood_break_all_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1446, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1446, args,
                     "1446 布局=[owner,target,罩点,降级]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int hood = args.int_args[2];
    const int drop = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1446,
                     args, "需要 owner/target ∈ {0,1}");
    set_storage_int_local(ctx, audit_key(k1446AuditBase, self, 2), hood);
    set_storage_int_local(ctx, audit_key(k1446AuditBase, self, 3), drop);
    bump_use_count(ctx, k1446AuditBase, self);
    if (hood <= 0 || drop <= 0) {
        // 行参在位即消费：显式 0/负 = 关闸档，不挂罩不武装（全载体 {0}>0、{1}=1）。
        set_storage_int_local(ctx, audit_key(k1446AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->stat_drop) {
        set_storage_int_local(ctx, audit_key(k1446AuditBase, self, 9), 2);
        return EffectResult::kOk;
    }
    // 护罩落自身（"获得"）：普通罩 priority=0、source_id=效果 id、duration=0 常驻
    // （J47 1753 盾直系；K472 1938 同形）。重施放 plain add 叠加（池语义）。
    const int before = ctx->getPet(self).hood_bank_.quantity_of(kSourceHood);
    ctx->getPet(self).hood_bank_.add(/*priority=*/0, hood, /*source_id=*/kSourceHood,
                                     /*duration=*/0);
    const int after = ctx->getPet(self).hood_bank_.quantity_of(kSourceHood);
    if (after == before) {
        // 罩槽满（kMaxShields=8）静默丢弃 → 不武装（旗不置，破罩无从谈起）。
        set_storage_int_local(ctx, audit_key(k1446AuditBase, self, 9), 4);
        return EffectResult::kOk;
    }
    // armed 旗 + slot 戳（获罩时在场合位；触发时校验——换宠他人受击事件不触发）。
    set_storage_int_local(ctx, audit_key(k1446AuditBase, self, 0), 1);
    set_storage_int_local(ctx, audit_key(k1446AuditBase, self, 1), ctx->on_stage[self]);
    set_storage_int_local(ctx, audit_key(k1446AuditBase, self, 4), 0);   // 触发计数清账
    set_storage_int_local(ctx, audit_key(k1446AuditBase, self, 5), 0);
    set_storage_int_local(ctx, audit_key(k1446AuditBase, self, 9), 0);

    auto on_shield_broken = [self](BattleContext* c, const BattleEvent& ev) {
        if (!c || ev.target != self || !g_core || !g_core->stat_drop) {
            return;
        }
        const int armed_key = audit_key(k1446AuditBase, self, 0);
        if (storage_int_local(c, armed_key, 0) != 1) {
            return;   // 旗未置/已消费（一次性）
        }
        // slot 戳校验：当前在场不是获罩那只 → 他人侧破盾事件跳过，旗保留（原宠
        // 回场后护罩仍在，可照常触发）。过期旗惰性滞留无行为面（1753 单旗口径）。
        if (c->on_stage[self] != storage_int_local(c, audit_key(k1446AuditBase, self, 1), -1)) {
            set_storage_int_local(c, audit_key(k1446AuditBase, self, 9), 3);
            return;
        }
        // 池门：1446 源护罩全空才算"护罩消失"（半吸收/叠加部分破碎不触发）。
        if (c->getPet(self).hood_bank_.quantity_of(kSourceHood) != 0) {
            return;
        }
        erase_storage_local(c, armed_key);   // 消费即失效（一次性）
        const int opp = 1 - self;
        set_storage_int_local(c, audit_key(k1446AuditBase, self, 4),
                              storage_int_local(c, audit_key(k1446AuditBase, self, 4), 0) + 1);
        set_storage_int_local(c, audit_key(k1446AuditBase, self, 5), c->roundCount);
        if (c->getPet(opp).hp <= 0) {
            return;   // 对手已倒：降属无对象（存活门，K470 口径）
        }
        const int drop = storage_int_local(c, audit_key(k1446AuditBase, self, 3), 0);
        // "全属性-{1}"含命中：六槽逐项 stat_drop（各查免弱、各钳 -6；749 直系）。
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            (void)g_core->stat_drop(c, opp, i, drop, self);
        }
    };
    ctx->event_center_.register_watcher(
        EventType::EVENT_SHIELD_BROKEN, self, ctx->roundCount,
        /*duration_rounds=*/0,   // 永久（armed 旗门控，inert 滞留同 1753）
        /*once=*/false, std::move(on_shield_broken),
        WatcherScope::TEAM,
        /*valid_id=*/ctx->watcher_valid_id[self]);   // 显式传（TEAM 不校验，K453 口径）
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 658 反馈桥 tick：payload=[self, opp, pct]。
// pending 槽读基数 → 双存活门 → deal_pink_damage(FIXED, actor=self) → 清账。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult k478_658_feedback_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    const int pending_key = audit_key(k658AuditBase, self, 3);
    const int base = storage_int_local(ctx, pending_key, -1);
    if (base < 0 || !g_core || !g_core->deal_pink_damage) {
        set_storage_int_local(ctx, audit_key(k658AuditBase, self, 9), 7);   // 空转（窗内重复桶）
        return EffectResult::kOk;
    }
    erase_storage_local(ctx, pending_key);   // 一次性：fire 即清（窗内第二桶空转）
    if (ctx->getPet(self).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k658AuditBase, self, 9), 5);
        return EffectResult::kOk;   // self 已倒（同归窗口）：反馈无主体
    }
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k658AuditBase, self, 9), 6);
        return EffectResult::kOk;   // 对手已倒：反馈无对象（K422/K441 口径）
    }
    const int reflect = base * pct / 100;   // int 直除（634 吸血同式；{0}∈{80,100}）
    set_storage_int_local(ctx, audit_key(k658AuditBase, self, 4), reflect);
    set_storage_int_local(ctx, audit_key(k658AuditBase, self, 5),
                          storage_int_local(ctx, audit_key(k658AuditBase, self, 5), 0) + 1);
    if (reflect > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, reflect, DamageKind::FIXED, self);
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 658 —— 下次受到攻击伤害后于对手出手流程结束反馈伤害值{0}%的百分比伤害
// 行参布局 [owner, target, pct]（args_num=1，loader 注入 owner/target 前缀）。
// SKILL_EFFECT 武装哨兵（armed 旗 + slot 戳 + TEAM 永久 TAKE_DAMAGE watcher，
// once=false 过滤后消费）；消费时当场注册对手侧 AFTER_ACTION 桥（显式 start=R、
// duration=2 直构——绕 InRounds 后手顺延折算），tick 反馈 deal_pink_damage(FIXED)。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_658_hit_feedback_stance(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 658, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 658, args,
                     "658 布局=[owner,target,pct]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 658,
                     args, "需要 owner/target ∈ {0,1}");
    set_storage_int_local(ctx, audit_key(k658AuditBase, self, 0), pct);
    bump_use_count(ctx, k658AuditBase, self);
    if (pct <= 0) {
        // 行参在位即消费：显式 0/负 = 关闸档（全载体 {0}∈{80,100}，防御档）。
        set_storage_int_local(ctx, audit_key(k658AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k658AuditBase, self, 9), 2);
        return EffectResult::kOk;   // 对手已倒：stance 无判定意义（K438/K470 口径）
    }
    if (!g_core || !g_core->deal_pink_damage) {
        set_storage_int_local(ctx, audit_key(k658AuditBase, self, 9), 3);
        return EffectResult::kOk;
    }
    // armed 旗 + slot 戳 + pending 清账。
    set_storage_int_local(ctx, audit_key(k658AuditBase, self, 1), 1);
    set_storage_int_local(ctx, audit_key(k658AuditBase, self, 2), ctx->on_stage[self]);
    set_storage_int_local(ctx, audit_key(k658AuditBase, self, 3), -1);   // 无 pending
    set_storage_int_local(ctx, audit_key(k658AuditBase, self, 4), 0);
    set_storage_int_local(ctx, audit_key(k658AuditBase, self, 6), 0);   // 消费计数清账
    set_storage_int_local(ctx, audit_key(k658AuditBase, self, 9), 0);

    auto on_take_damage = [self, opp, pct](BattleContext* c, const BattleEvent& ev) {
        if (!c) {
            return;
        }
        // 受击四重过滤（K470 634 直系）：我受击 + 攻击方是对手 + 红伤 + 攻击伤害时点。
        if (ev.target != self || ev.actor != opp) {
            return;
        }
        if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
            return;
        }
        if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
            && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
            return;
        }
        const int armed_key = audit_key(k658AuditBase, self, 1);
        if (storage_int_local(c, armed_key, 0) != 1) {
            return;   // 旗未置/已消费（一次性；inert watcher 滞留同 1753）
        }
        // slot 戳校验：当前在场的不是武装那只 → 不消费、旗保留（原宠回场后可触发）。
        if (c->on_stage[self] != storage_int_local(c, audit_key(k658AuditBase, self, 2), -1)) {
            set_storage_int_local(c, audit_key(k658AuditBase, self, 9), 4);
            return;
        }
        const int base = ev.amount;   // 实扣口径（护盾吸收+超杀钳制后，K463/K470 引擎事实）
        if (base <= 0) {
            return;
        }
        // 消费即失效（"下次"语义）：清旗、记 pending、当场注册对手侧 AFTER_ACTION 桥。
        erase_storage_local(c, armed_key);
        set_storage_int_local(c, audit_key(k658AuditBase, self, 3), base);
        set_storage_int_local(c, audit_key(k658AuditBase, self, 6),
                              storage_int_local(c, audit_key(k658AuditBase, self, 6), 0) + 1);
        // 桥：owner=对手侧、双 AFTER_ACTION 注册（桶只跑行动方）；显式 start=当前回合、
        // duration=2 直构（窗 [R,R+1]——绕 InRounds 后手顺延折算，红伤弹伤边角不丢拍）。
        for (const State st :
             {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
            Effect e(658, 0, opp, /*rounds=*/2, EffectArgs({self, opp, pct}),
                     &k478_658_feedback_tick);
            auto ce = std::make_unique<ContinuousEffect>(e, st, opp, /*duration=*/2,
                                                         /*registeredRound=*/c->roundCount);
            ce->source_id_ = 658;
            c->register_skill_effect(st, opp, std::move(ce));
        }
    };
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount,
        /*duration_rounds=*/0,   // 永久（哨兵跨回合存活；armed 旗门控）
        /*once=*/false, std::move(on_take_damage),
        WatcherScope::TEAM,
        /*valid_id=*/ctx->watcher_valid_id[self]);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(1446, effect_1446_hood_break_all_drop)
SKILL_EFFECT(658, effect_658_hit_feedback_stance)
