// J 组 J324 · 击败族九条批产 —— effect_des 187/201/218/273/345/349/362/380/525
//（看板 J324「击败族九条批产 9 宏大单」，a01-j324 2026-11-05，worktree
//  ../agents/j324；号段 7870-7879。通道=每宏注册 EVENT_OPPONENT_DEFEATED 监听
//  （J323 场景 7820 同款探针起步；归因过滤 `actor == 自己 && actor != target`
//  口径见 event_center.h 载荷契约注：actor=-1=无归因致死、actor==target=自毁类）；
//  效果主体按官方文本映射既有原语；registry 只增不改（moves 1412 基线不动——
//  本单走 SOUL_PROGRAM 通道，soul 官方 405 → +5）。）
//
// ── 文本权威与载体三查（2026-11-05 本 worktree 复核）─────────────────────
// 【文本权威】S18 落档的九条全文现查 effect_des（seer_unity.sqlite，kind=1 主词典）：
//     187 永昌玺｜每次在场首次在回合开始达到6方时，开启君临天下；击败对手后方数归0
//     201 凯旋预言｜持有精灵下次击败对手时恢复自身全部体力和所有技能PP值，下场保留
//     218 北冥狂刃｜自身攻击技能必定打出致命一击，且使用后附加上次击败对手的回合
//       所造成攻击伤害100%的真实伤害，下场后保留
//     273 万相乖离｜无为觉者第五技能的首条未取消过的效果中的触发条件永久取消，并
//       提升第五技能50点威力；无为觉者击败对手后，重置万相乖离对第五技能的所有影响
//       并令对方阵亡精灵消逝
//     345 终命之辉｜被支援者击败对手后，自身为其恢复最大体力的30%
//     349 终命之璨｜被支援者击败对手后，自身为其恢复最大体力的35%
//     362 卪｜击败对手后减少自身体力上限的20%，本场战斗受到的攻击伤害减少10%、
//       造成的攻击伤害提升10%
//     380 圣剑重铸｜己方在场精灵所有技能先制+1；己方击败对手时，至序圣华复活；
//       回合开始时，若己方没有圣剑，则圣剑重铸结束
//     525 骑士决斗·落败｜己方危机感+1；对手为艾欧丽娅时，若自身没有可选择使用的
//       第五技能则无法行动；自身击败对手的回合无法触发击败类效果；自身被击败的回合
//       无法触发被击败类效果；自身为己方最后一只精灵时，骑士决斗·落败失效；下场保留
//
// 【载体查】看板行给的 effect_icon 对行查询已执行：187/201/218/273/525 **无行**；
//   345/349/362/380 有行（icon_id 124/128/145/159）但 tips（"每次使用技能有10%的
//   概率提升速度等级1" 等）与 effect_des 同 id 全文**完全互异**——G8 e_reflect_old21
//   考古口径：effect_des / effect_icon / effect_info 是**各自独立命名空间**，四行属
//   effect_icon 空间的另一效果 → 九条在 effect_icon 空间实际**全部无对行**。
//   主魂印反查（tips LIKE + monsters 表）锁定九条宿主（词条=主魂印 tips 内定义的
//   命名状态，哀霆之声同构实证：effect_des 199 全文 vs effect_icon 2099 简版引用）：
//     187 永昌玺   → 始皇帝·嬴政 4468 魂印 2057（"每次自身击败对手后清空"）
//     201 凯旋预言 → 天后·弗丽嘉 4498 魂印 2098（"凯旋预言：下次击败对手时…"）
//     218 北冥狂刃 → 霍光 4515 魂印 2116（"北冥狂刃：霍光持有时…"）
//     273 万相乖离 → 无为觉者 4661 魂印 2260（已实现，见逐条记档）
//     345/349 终命之辉/璨 → 奥菲利娅 4706 魂印 2291（4 层版 30%）/2344（5 层版 35%）
//     362 卪      → 空元行者 4586 魂印 2175（已实现，见逐条记档）
//     380 圣剑重铸 → 至序圣华 4321 魂印 1897 / 圣律剑灵 4400 魂印 1972（圣剑系统）
//     525 骑士决斗·落败 → 星光·艾欧丽娅 4911 魂印 2516（骑士决斗状态机）
//   → 数值锚=官方文本字面（K381 2518「args 无锚数值 → tips 字面 fallback 常量锚」
//     同纪律；本连 tips 锚都缺，降一级用 effect_des 字面），实现零行数值硬编码——
//     行（对行）≠宏：载体行缺失即"无行可依"，按文本驱动，记档。
//
// 【注册键查】SOUL_PROGRAM 键空间 = effect_icon.effect_id（pet_factory.cpp:527
//   "effect_id 是引擎内部效果号，作为 SoulMark.id（插件程序注册键）"实证）。
//   九个 effect_des.id 借键注册前逐一查撞（python 全资源 constexpr+宏扫描 +
//   build/registry.json 对账，2026-11-05）：
//     187/201/218/349/380 → soul 程序表 FREE（187/525 在 moves SKILL_EFFECT 表被
//       batch_b 2894 / round_window 484 占用，但 soul_mark_manager.cpp 撞号注释
//       明说"单效果表与程序表是两张独立的表"，j_dispel_success_cond 655 判例=
//       SKILL_EFFECT 与 SOUL_PROGRAM 同 id 并存先例 → 跨表并存安全）；
//     **345 → SoulMarkProgram 表已被 resources/soul_lib/k259_roll_speed.cpp 占用**
//       （constexpr kMarkId = 345，effect_icon 空间 345=Z 2708 提速魂印）→
//       程序表 map 后到覆盖语义会**改写 k259 存量行为**，违反"只增不改"硬门 →
//       345 记档跳过（恢复方法：k259 迁移后解开本文件尾部 345 注册注释块即可，
//       效果体已按 30% 档写全）。
//   id_collision 按 (Category, id) 记账、白名单挡存量、新撞号判失败——本单新增
//   SoulMarkProgram 注册 5 条逐一核对无同表撞号。
//
// ── 逐条落地/记档（majority reading）─────────────────────────────────
// 【187 落】击败族条款="击败对手后方数归0"。方数计数槽=plugin_storage k187FangBase
//   （键约定记档：主魂印 2057 未来实现攒方时**同键对账**——1方…6方档位读同槽，
//   "永昌玺下场不保留"=战斗级 plugin_storage 正确落点）；本单只做归零写点+审计槽，
//   攒方/君临天下主体=主魂印 2057 范围记档不产码。门=归因过滤+持有者在场（词条
//   "每次在场…"语感）+主魂印尾注"所有特效boss无效"罩词条 → is_boss_challenge
//   双保险（K391 口径）。
// 【201 落】"持有精灵下次击败对手时恢复自身全部体力和所有技能PP值，下场保留"。
//   挂载即武装（armed 旗写持有者 soulmark_storage——天然下场保留，202 迷失预言
//   "下场消失"对照组正说明 201 的旗必须跨下场存活）；击败回调消费：回满
//   （heal_max_hp_fraction denom=1，K99 778 同款）+ 全 PP 满（pp_restore 大数
//   钳 maxPP，battle_primitives.cpp:1906 钳制实证）；消费后写 consumed 旗**不再
//   自动重武装**（主魂印 2098"两种预言都已通过使用技能消耗→为自身附加凯旋预言"
//   =附加式一次性增益，词条自身无"每次行动重新武装"语义，majority reading 记档）。
// 【218 落】词条主体两条：①"攻击技能必暴"=ROUND_START 桶每回合重写
//   ws.must_crit[owner]（j_must_crit_window e_must_crit 同款写点；属性技无伤害
//   不判暴，天然攻击技限定）；②"使用后附加上次击败对手的回合所造成攻击伤害
//   100%的真实伤害"=击败回调把 ws.raw_attack_damage[side]（裸伤台账，
//   battleFsm.cpp:907 写点、每攻击尝试清零——击败时点读到的就是那一击）记入
//   soulmark_storage 账本（"下场后保留"字面=账本必须跨下场存活），ON_SKILL_HIT
//   桶攻击技使用后 deal_true_damage(opp, 账本, owner)（wuxiang_2260 ON_SKILL_HIT
//   先例）。账本不清零（"上次击败"=最近一次击败，新击败覆盖旧值）；首击败当击
//   不附加本次值（时序天然：ON_SKILL_HIT 附加读的是该次击败**之前**的账本）。
//   击败当击对手已倒 → deal_true_damage hp<=0 守卫落空=官方"使用后附加"对已击败
//   对手无落点的自然语义（majority reading 记档）。升级时点管理（北冥之刃→狂刃
//   3 回合，主魂印 2116）记档不产码；词条=狂刃形态效果主体。主魂印尾注"所有特效
//   boss无效"罩词条 → 双保险。
// 【273 跳】查重命中：wuxiang_2260.cpp 已完整实现万相乖离全链（取消进度/威力+50/
//   "击败对手后重置并令对方全部阵亡精灵消逝"=EVENT_DEATH watcher+
//   vanish_dead_spirits(count=0,include_extra=true)，文件头注引 effect_des 273
//   原文）。再注册=重复实现+行为分叉风险。零动作。
// 【345 跳】注册键撞号（SoulMarkProgram 345 被 k259_roll_speed 占用，覆盖=改写
//   存量，见【注册键查】）。效果体已按 30% 档写全（j349_support_heal 复用函数
//   pct/mark 参数化），注册注释块留档，k259 迁移后解注释即插即用。
// 【349 落】"被支援者击败对手后，自身为其恢复最大体力的35%"（主魂印 2344 5 层版
//   档位；2291 4 层版=345 的 30%）。支援者=词条持有者（奥菲利娅），受援者=同队
//   在场精灵。门=归因过滤+持有者**在背包存活**（2291"自身存活于出战背包时…提供
//   下述支援"字面：hp>0 且非在场槽——支援是场下语义，持有者亲自在场时不复读）
//   +主魂印尾注"（boss无效）"罩词条 → 双保险。"恢复最大体力的30%/35%"分母按
//   受益者口径（恢复给受援者 → 分母=受援者 max_hp，heal_max_hp_fraction
//   "恢复自身最大体力"族同款读法，记档）。
// 【362 跳】查重命中：vanish_kongyuan.cpp 空元卪段已完整实现（"击败对手后减少
//   自身体力上限的20%，本场战斗受到的攻击伤害减少10%、造成的攻击伤害提升10%"，
//   使用 5 号位技能击败的门=用户 2026-09-17 口径，文件头注引空元之铭第 5 条原文）。
//   零动作。
// 【380 落】击败族条款="己方击败对手时，至序圣华复活"。回调扫己方 6 槽找
//   soulMark.monster_id==4321 且 hp<=0 且未消逝 → revive_pet 满血复活（hp 数值
//   官方文本无锚；majority reading 按 k_death_revenge 132 复活先例
//   resolve_pet_max_hp 满血口径，记档留主魂印 1897 实现时校正）。**持有者不在场
//   不设门**（条款语义="至序圣华（可能就是持有者本人）死亡后靠队友击败复活"——
//   她死了在场门必 false，复活效果必须在她死亡时仍活着；TEAM watcher 不随死亡
//   拆除，天然满足）。主体（"己方在场精灵所有技能先制+1"/圣剑物件/回合开始
//   结束判定）=主魂印 1897/1972 圣剑系统范围记档不产码（词条文本无圣剑物件
//   的数量/存续建模，缺主魂印"附加1柄圣剑"的写点）。
// 【525 跳】状态机词条，无独立触发时点：①"自身击败对手的回合无法触发击败类
//   效果/被击败的回合无法触发被击败类效果"需要引擎级"击败类效果"注册分类+
//   抑制门——事件中心 deliver 现只有 EVENT_DEATH 倒下方压制位
//   （defeat_effects_suppressed，百罗鬼帝 2085），EVENT_OPPONENT_DEFEATED 无门，
//   加门=改内核 deliver（内核零改动硬门）；②"对手为艾欧丽娅时…无法行动"/
//   "己方最后一只精灵时失效"依赖骑士决斗状态机（主魂印 2516 未实现，
//   "附加骑士决斗/落败仅持续4回合"的状态存续建模缺失）；③"己方危机感+1"触发
//   时点=进入落败状态（状态不存在），且危机感账面是琉梦体系私有状态
//   （zha_liumeng_curse LiumengHopeState.danger，soulmark_storage 私键）——
//   跨魂印写他人私键=键约定脆断。整条记档留主魂印 2516 立单。
// 【boss 尾注】九条 effect_des 词条文本自身**均无尾注** → SOUL_NODE
//   boss_invalid 一律不标（标注单位=带尾注的子句，plugin_interface.h 注）；
//   宿主主魂印 tips 尾注（2057/2116/2291/2344"所有特效boss无效"/"（boss无效）"）
//   罩词条 → 回调内手动 is_boss_challenge 双保险（K391/K409 口径，记档）。
// 【时点语义】"击败对手则…"=击败发生时立即（事件驱动 EVENT_OPPONENT_DEFEATED，
//   死亡漏斗与 EVENT_DEATH 同点成对 emit）——与旧"未击败则"pending 族
//   （j_undefeat_reversal 838 AFTER_ACTION hp>0 门）刻意区分，本族无 pending。
//
// 【通道改隶记档（majority reading，2026-11-05 实测修正）】本文件**落在 soul_lib**
//   而非看板行字面的 moves_lib：SOUL_PROGRAM 注册走 plugin_reg::flush →
//   registry->registerSoulMarkProgram，而 moves dylib 的宿主是 EffectFactory
//   （pet_factory.cpp:275 "EffectFactory::getInstance(\"resources/moves_lib\")"），
//   其 registerSoulMarkProgram 是 **no-op**（effect.cpp:287 "EffectFactory doesn't
//   handle soul marks"）——注册被静默丢弃、watcher 永不挂载（7874 正向对照红实测）。
//   SOUL_PROGRAM 的生效宿主=SoulMarkManager（pet_factory.cpp:276 加载 soul_lib dylib），
//   全库 503 处 SOUL_PROGRAM 亦全部在 soul_lib/suit_lib（moves_lib 零实注先例）。
//   看板行 moves 1412 基线不动（本单零 SKILL_EFFECT 注册），soul 官方 405→+5。
//
// 场景回归：test/scenario/scenario_7870_defeat_family_fang_kaixuan.cpp（187+201）、
//   scenario_7871_defeat_family_beiming.cpp（218）、
//   scenario_7872_defeat_family_zhongming.cpp（349）、
//   scenario_7873_defeat_family_shengjian.cpp（380）、
//   scenario_7874_defeat_family_attribution.cpp（归因过滤专项）。

#include <utils/sbs_rand.h>

#include "common.h"

#include "../gaia_wudao_runtime.h"   // K449 2574 场地盾查询面（gaia_bench_target_locked）

#include <abnormal-system/abnormal-types.h>
#include <entities/skills.h>

namespace {

// ── 存储键段 9932xxxxx（2026-11-05 python 全资源扫描 FREE，check_storage_keys
//    静态扫同段无撞；owner∈{0,1}）─────────────────────────────────────
constexpr int k187FangBase = 993200010;      // +side：永昌玺方数槽（主魂印 2057 同键对账）
constexpr int k187AuditBase = 993200020;     // +side：击败归零发生的回合（审计）
constexpr int k187ArmFlagBase = 993200030;   // +side：watcher 已挂旗（幂等）

constexpr int k201ArmedKey = 993200040;      // 持有者 soulmark_storage：武装旗
constexpr int k201ConsumedKey = 993200050;   // 持有者 soulmark_storage：已消费旗
constexpr int k201AuditBase = 993200060;     // +side：触发回合（审计）
constexpr int k201ArmFlagBase = 993200070;   // +side：watcher 已挂旗

constexpr int k218LedgerKey = 993200080;     // 持有者 soulmark_storage：上次击败回合攻击伤害账本
constexpr int k218LedgerRoundKey = 993200090;  // 持有者 soulmark_storage：账本对应击败回合（审计）
constexpr int k218ArmFlagBase = 993200100;   // +side：watcher 已挂旗

constexpr int k349AuditBase = 993200110;     // +side：支援恢复触发回合（审计）
constexpr int k349ArmFlagBase = 993200120;   // +side：watcher 已挂旗
constexpr int k345AuditBase = 993200125;     // +side：345 同构审计键（预留给解注释后的 345）

constexpr int k380AuditBase = 993200130;     // +side：复活触发回合（审计）
constexpr int k380ArmFlagBase = 993200140;   // +side：watcher 已挂旗

constexpr int kMonZhixuShenghua = 4321;      // 至序圣华（380 复活对象，monsters 表实证）
constexpr int kMark187 = 187;
constexpr int kMark201 = 201;
constexpr int kMark218 = 218;
constexpr int kMark345 = 345;
constexpr int kMark349 = 349;
constexpr int kMark380 = 380;
constexpr int k349HealPct = 35;              // 终命之璨 35%（effect_des 349 字面）
constexpr int k345HealPct = 30;              // 终命之辉 30%（effect_des 345 字面，预留）

// 归因过滤（本族统一门，event_center.h 载荷契约注的字面实现）：
// actor==自己 && actor!=target。actor=-1（无归因致死）与 actor==target（自毁类）
// 均不通过。target=被击败方 side。
inline bool defeat_attributed_to(const BattleEvent& ev, int side) {
    return ev.actor == side && ev.actor != ev.target;
}

// 持有者宿主槽的 soulmark_storage（host_slot 战斗内不变，bind_soulmark_args 约定）。
inline std::map<int, std::any>& host_storage(BattleContext* ctx, int side, int host_slot) {
    return ctx->seerRobot[side].elfPets[host_slot].soulmark_storage;
}

// "本次成功执行的技能是否攻击技"（soul_lib 私副本，k_anomaly_threshold_1210:114
// 同款——读 ws.skill_type_view 实际物化类型，变型技/替换技正确）。
inline bool j324_executed_skill_is_attack(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->ws.skill_type_view[owner] != static_cast<int>(SkillType::Attribute);
}

// 受援者/持有者的体力上限（numericalBase 为面板口径，heal 管线同源）。
inline int j324_pet_max_hp(const ElfPet& pet) {
    const int mh = pet.numericalBase[NumericalPropertyIndex::HP];
    return mh > 0 ? mh : std::max(1, pet.hp);
}


// ═════════════════════════════════════════════════════════════════════════
// 187 永昌玺（始皇帝·嬴政 4468 / 主魂印 2057 词条）
// 击败对手后 → 方数槽归 0 + 审计。攒方/君临天下主体=主魂印 2057 范围（记档）。
// ═════════════════════════════════════════════════════════════════════════
EffectResult j187_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int side = a.owner;
    if (ctx->plugin_storage.count(k187ArmFlagBase + side)) {
        return EffectResult::kOk;   // 重入幂等（再上场重跑防双挂）
    }
    ctx->plugin_storage[k187ArmFlagBase + side] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_OPPONENT_DEFEATED, side, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [side](BattleContext* c, const BattleEvent& ev) {
            if (!c || c->is_boss_challenge) {
                return;   // 主魂印 2057"所有特效boss无效"罩词条 → 双保险（记档）
            }
            if (!defeat_attributed_to(ev, side)) {
                return;   // 归因过滤：只认自己击败对方（-1 无归因/自毁类均不过）
            }
            if (!onstage_has_soul_mark(c, side, kMark187)) {
                return;   // "每次在场…"：持有者须在场
            }
            c->plugin_storage[k187FangBase + side] = 0;   // 方数归 0（主魂印同键对账）
            c->plugin_storage[k187AuditBase + side] = c->roundCount;   // 审计：归零发生回合
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════
// 201 凯旋预言（天后·弗丽嘉 4498 / 主魂印 2098 词条）
// 挂载即武装（armed 旗=持有者 soulmark_storage，跨下场保留）；持有精灵击败对手
// 时消费：恢复自身全部体力 + 所有技能 PP 值 → 消费后不自动重武装（记档）。
// ═════════════════════════════════════════════════════════════════════════
EffectResult j201_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int side = a.owner;
    const int slot = a.host_slot;
    auto& st = host_storage(ctx, side, slot);
    if (!st.count(k201ArmedKey) && !st.count(k201ConsumedKey)) {
        st[k201ArmedKey] = 1;   // 挂载即武装（armed/consumed 均存在时不重武装）
    }
    if (ctx->plugin_storage.count(k201ArmFlagBase + side)) {
        return EffectResult::kOk;   // watcher 幂等
    }
    ctx->plugin_storage[k201ArmFlagBase + side] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_OPPONENT_DEFEATED, side, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [side, slot](BattleContext* c, const BattleEvent& ev) {
            if (!c || c->is_boss_challenge) {
                return;   // 主魂印 2098"（boss无效）"罩词条 → 双保险（记档）
            }
            if (!defeat_attributed_to(ev, side)) {
                return;
            }
            auto& st = host_storage(c, side, slot);
            auto it = st.find(k201ArmedKey);
            if (it == st.end() || any_int_or(it->second) != 1) {
                return;   // 未武装不触发（已消费/未挂载）
            }
            if (!g_core || !g_core->pp_restore) {
                return;
            }
            // 恢复自身全部体力（moves_lib heal_max_hp_fraction(denom=1) 的等价直呼形：
            // heal_amount(ctx, side, max_hp)——soul_lib 无该 helper）
            (void)g_core->heal_amount(c, side, j324_pet_max_hp(c->getPet(side)));
            (void)g_core->pp_restore(c, side, 9999, /*only_empty=*/false);   // 所有技能 PP 满
            st.erase(k201ArmedKey);   // "下次击败"=一次性消费
            st[k201ConsumedKey] = 1;
            c->plugin_storage[k201AuditBase + side] = c->roundCount;   // 审计
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════
// 218 北冥狂刃（霍光 4515 / 主魂印 2116 词条）
// ①攻击技能必暴（ROUND_START 每回合重写 ws.must_crit）；②击败时把裸伤台账记入
// 账本（soulmark_storage，下场保留），攻击技使用后附加账本等量真实伤害。
// ═════════════════════════════════════════════════════════════════════════
EffectResult j218_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int side = a.owner;
    const int slot = a.host_slot;
    if (ctx->plugin_storage.count(k218ArmFlagBase + side)) {
        return EffectResult::kOk;   // watcher 幂等
    }
    ctx->plugin_storage[k218ArmFlagBase + side] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_OPPONENT_DEFEATED, side, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [side, slot](BattleContext* c, const BattleEvent& ev) {
            if (!c || c->is_boss_challenge) {
                return;   // 主魂印 2116"所有特效boss无效"罩词条 → 双保险（记档）
            }
            if (!defeat_attributed_to(ev, side)) {
                return;
            }
            if (!onstage_has_soul_mark(c, side, kMark218)) {
                return;   // "霍光持有时"：持有者须在场
            }
            auto& st = host_storage(c, side, slot);
            // "上次击败对手的回合所造成攻击伤害"=击败时点的裸伤台账（battleFsm.cpp:907
            // 写点；每攻击尝试清零 → 此刻读到的就是击败那一击的攻击伤害）。
            st[k218LedgerKey] = c->ws.raw_attack_damage[side];
            st[k218LedgerRoundKey] = c->roundCount;   // 审计：账本对应回合
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// 必暴主体：ROUND_START 每回合重写（ws.must_crit 每回合 reset，j_must_crit_window
// e_must_crit 同款写点）。
EffectResult j218_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int side = a.owner;
    if (!onstage_has_soul_mark(ctx, side, kMark218)) {
        return EffectResult::kOk;   // 持有者不在场不重写
    }
    ctx->ws.must_crit[side] = true;
    return EffectResult::kOk;
}

// 真伤附加主体：ON_SKILL_HIT（FIRST/SECOND 同函数体）——攻击技使用后附加账本
// 等量真实伤害。账本不清零（"上次击败"=最近一次击败，新击败覆盖；记档）。
EffectResult j218_on_skill_hit(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int side = a.owner;
    if (!onstage_has_soul_mark(ctx, side, kMark218)) {
        return EffectResult::kOk;
    }
    if (!j324_executed_skill_is_attack(ctx, side)) {
        return EffectResult::kOk;   // 只认攻击技（词条"自身攻击技能…"语感）
    }
    auto& st = host_storage(ctx, side, a.host_slot);
    auto it = st.find(k218LedgerKey);
    const int ledger = it == st.end() ? 0 : any_int_or(it->second);
    if (ledger <= 0) {
        return EffectResult::kOk;   // 从未击败过：无附加
    }
    if (!g_core || !g_core->deal_true_damage) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_true_damage(ctx, 1 - side, ledger, side);
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════
// 345/349 终命之辉/璨（奥菲利娅 4706 / 主魂印 2291(30%)/2344(35%) 词条）
// 被支援者（同队在场精灵）击败对手 → 支援者（持有者，背包存活）为其恢复
// 受援者最大体力的 pct%。345/349 共用实现体，mark id + pct 参数化。
// ═════════════════════════════════════════════════════════════════════════
void j324_support_heal_watch(BattleContext* ctx, int side, int mark_id, int audit_base,
                             int pct) {
    ctx->event_center_.register_watcher(
        EventType::EVENT_OPPONENT_DEFEATED, side, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [side, mark_id, audit_base, pct](BattleContext* c, const BattleEvent& ev) {
            if (!c || c->is_boss_challenge) {
                return;   // 主魂印 2291/2344"（boss无效）"罩词条 → 双保险（记档）
            }
            if (!defeat_attributed_to(ev, side)) {
                return;
            }
            const int holder_slot = c->find_mark_in_roster(side, mark_id);
            if (holder_slot < 0) {
                return;   // 持有者不在背包（含消逝）不支援
            }
            const ElfPet& holder = c->seerRobot[side].elfPets[holder_slot];
            if (holder.hp <= 0) {
                return;   // "自身存活于出战背包时"：阵亡不支援
            }
            if (holder_slot == c->on_stage[side]) {
                return;   // 支援是场下语义：持有者亲自在场时不复读（2291 字面门）
            }
            // 受援者 = 同队在场精灵（击败者本人）。
            const int aided_slot = c->on_stage[side];
            const ElfPet& aided = c->seerRobot[side].elfPets[aided_slot];
            if (aided.hp <= 0) {
                return;   // 受援者已倒（异常兜底，正常击败时受援者活着）
            }
            // K449 2574 场地盾（「{0}回合内，双方的在场精灵无法成为场下精灵的
            // 效果目标」）：支援回血 = 场下源（持有者背包存活）→ 场上目标（受援者）
            // 的真实效果施加——盾活跃（严格当轮匹配 stamp）时本施加被阻。查询面
            // ../gaia_wudao_runtime.h（普通 N 回合窗：施放方离场/断回合/到期三路
            // stamp 冻结自假）。⚠️ 本入口是 2574 首个已实现消费者（行为正负例
            // T8374.d）；2026-11-11 派发缺口 2 全量清查后，同形态的另两个已实现
            // 入口（roster_field 903 星皇之赐 T8374.e、k421 2456 永沐挂曝 T8374.f）
            // 已各自接线——349 侧改动维持 2026-11-10 现状未扩（专项审结论见 K449
            // 完成报告：本门只影响 349/345 支援路径，187/201/218/380 各自独立
            // watcher 不经本函数）。来源宿主 slot/side 判别与非场下分支（上方
            // holder 三门）全部保留不动。
            if (gaia_bench_target_locked(c, side)) {
                return;   // 在场受援者是场下效果的锁定目标 → 支援回血被场地盾阻
            }
            if (!g_core || !g_core->heal_amount) {
                return;
            }
            const int amount = j324_pet_max_hp(aided) * pct / 100;   // 分母=受益者口径（记档）
            if (amount <= 0) {
                return;
            }
            (void)g_core->heal_amount(c, side, amount);
            c->plugin_storage[audit_base + side] = c->roundCount;   // 审计
        },
        WatcherScope::TEAM);
}

EffectResult j349_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int side = a.owner;
    if (ctx->plugin_storage.count(k349ArmFlagBase + side)) {
        return EffectResult::kOk;   // watcher 幂等
    }
    ctx->plugin_storage[k349ArmFlagBase + side] = 1;
    j324_support_heal_watch(ctx, side, kMark349, k349AuditBase, k349HealPct);
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════
// 380 圣剑重铸（至序圣华 4321 / 圣律剑灵 4400 系词条）
// 己方击败对手时 → 至序圣华（monster_id=4321）满血复活。持有者不在场不设门
//（她可能就是被复活的死者本人，记档）。主体先制+1/圣剑物件=主魂印范围（记档）。
// ═════════════════════════════════════════════════════════════════════════
EffectResult j380_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int side = a.owner;
    if (ctx->plugin_storage.count(k380ArmFlagBase + side)) {
        return EffectResult::kOk;   // watcher 幂等
    }
    ctx->plugin_storage[k380ArmFlagBase + side] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_OPPONENT_DEFEATED, side, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [side](BattleContext* c, const BattleEvent& ev) {
            if (!c || c->is_boss_challenge) {
                return;   // 主魂印 2516 系"（boss无效）"罩词条 → 双保险（记档）
            }
            if (!defeat_attributed_to(ev, side)) {
                return;   // 己方击败对手（对手击败不复活）
            }
            if (!g_core || !g_core->revive_pet) {
                return;
            }
            for (int slot = 0; slot < 6; ++slot) {
                const ElfPet& pet = c->seerRobot[side].elfPets[slot];
                if (pet.hp > 0) {
                    continue;   // 活着无需复活
                }
                if (c->is_vanished(side, slot)) {
                    continue;   // 消逝不可逆（revive_pet 双保险外的显式门）
                }
                if (pet.soulMark.monster_id != kMonZhixuShenghua) {
                    continue;   // 只复活至序圣华
                }
                const int hp = j324_pet_max_hp(pet);   // 满血复活（记档：文本无锚，先例口径）
                (void)g_core->revive_pet(c, side, slot, hp);
                c->plugin_storage[k380AuditBase + side] = c->roundCount;   // 审计
                break;   // "至序圣华复活"单数
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +5（187/201/218/349/380 入账，
// 只增不改；345 注释块留档，见头注【注册键查】）。词条文本均无 boss 尾注 →
// 节点 boss_invalid 一律不标（主魂印尾注由回调内 is_boss_challenge 双保险承接）。

SOUL_PROGRAM(187, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &j187_arm,
                              /*once=*/false, /*early=*/true)});

SOUL_PROGRAM(201, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &j201_arm,
                              /*once=*/false, /*early=*/true)});

SOUL_PROGRAM(218, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &j218_arm,
                              /*once=*/false, /*early=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &j218_round_start),
                    SOUL_NODE(State::BATTLE_FIRST_ON_SKILL_HIT, &j218_on_skill_hit),
                    SOUL_NODE(State::BATTLE_SECOND_ON_SKILL_HIT, &j218_on_skill_hit)});

// 349/380 用 ROSTER scope（出战背包存活即注册，TEAM 连续效果）：支援是场下语义
// （349 持有者在背包时就要挂好监听）、380 的至序圣华可能已阵亡（监听必须在她
// 死亡期间活着）——STAGE scope 的 owner_on_stage 门两者都不满足。
SOUL_PROGRAM(349, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &j349_arm,
                              /*once=*/false, /*early=*/true,
                              SoulScope::ROSTER)});

SOUL_PROGRAM(380, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &j380_arm,
                              /*once=*/false, /*early=*/true,
                              SoulScope::ROSTER)});

// ── 345 终命之辉（30% 档）——注册键撞号留档（k259_roll_speed 占 SoulMarkProgram
//    345，覆盖=改写存量，见头注【注册键查】）。k259 迁移后解开本块即插即用：
// SOUL_PROGRAM(345, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &j345_arm,
//                               /*once=*/false, /*early=*/true)});
// （j345_arm 同 j349_arm 形态：j324_support_heal_watch(ctx, side, kMark345,
//   k345AuditBase, k345HealPct)。）
