#ifndef GAIA_WUDAO_RUNTIME_H
#define GAIA_WUDAO_RUNTIME_H
// 盖亚武道共享运行时 —— J11 技能侧姿态机（moves_lib/j_gaia_stance.cpp）×
// K449 魂印侧三道轮回机（soul_lib/k449_gaia_wudao_2377.cpp）的共享键约定 +
// inline 存取器（crawford_runtime.h 同款形态：全部存储访问集中在本头，跨 dylib
// 只走 fundamental type int——typeinfo 全局唯一，两插件互读安全）。
//
// ── 宿主槽逐键生命周期表（2026-11-10 二次修正：冻结待证，不冒称官方实测/裁定）──
// 背景：J11 五键族原宿主 ctx->plugin_storage（battle 全局、按 side）→ b866cb2c 迁
// **ElfPet::on_stage_storage**（perform_switch 引擎自动清，battleFsm.cpp:609，主动
// 中切+死亡换宠共同漏斗）。⚠️ 该迁移依据的「登场初始血道 → 轨迹方向也随登场重置」
// 是**语法推断不是定案**：主会话 2026-11-10 驳回把雷伊的上下场刷新裁定套盖亚、
// 也驳回「未裁定=下场永保留已证」——逐键现状与建议如下，全部**冻结待官方实测/
// 语料定案**：
//   · 道位 kGaiaWayKey/开启旗 kGaiaActiveKey：来源=2377 tips「登场时自身开启武道，
//     初始为血之道」（开局开启子句，语法只锁"开启时点=登场、初始值=血道"）。
//     当前主线=on_stage（再登场重置血道，T8371E 现测行为）。建模 A=维持现状
//     （每次登场重开）；建模 B=soulmark 持久（下场保留、再登场仅在未开启时开）。
//     官方文本对"再登场是否重置"**无句**——两案均无实证。
//   · 姿态 kGaiaStanceKey（2292/2293/2294 方向机）：来源=技能组轨迹方向语义；
//     方向与道位同属轨迹。当前=on_stage（随登场重置）。建模同上两案，与道位
//     **应同槽**（方向是轨迹的属性，拆槽会产生半持久半重置的混合态）。
//   · 挡伤账 kGaiaAccountKey（2292「正转期间抵挡的伤害」）：账期=姿态期。当前=
//     on_stage。建模随姿态同槽。
//   · 进化次数 kGaiaEvolveKey（2293「下{0}次衍化」）：当前=on_stage。建模随姿态。
//   · armed kGaiaArmedKey / 登场轮 kGaiaEntryRoundKey / 对手追踪 kGaiaOppSlotKey+
//     kGaiaOppEntryRoundKey / 抵挡观察者 epoch kGaiaBlockEpochKey：**机械键**
//     （再登场重武装/血道窗按登场起算/邪道阈按对手在场/幂等旗）——on_stage
//     **无争议**（任何生命周期建模下都随登场重置）。
//   · armed 旗随槽清=再登场自动重武装 CAP/BLOCK 记账观察者（修 J11 潜伏缺陷：
//     旧旗 plugin_storage 换宠不清而观察者已随 pipeline epoch 作废 → 再登场
//     "旗在观察者亡"）。首次 J11 宏触发 j11_ensure_accounting 重武装；epoch 作废
//     +幂等旗双保险。
// ⚠️ 2300 watcher 句柄 kGaiaW2300HandleKey 留 plugin_storage： watcher 注册为
//   WatcherScope::TEAM（切换保留）——**官方文本对 2300 的持续期无锚**（「攻击伤害
//   被限制时{0}%令对手{1}」无窗词）→ 当前实现=施放后战斗级常驻+同键换绑防叠加。
//   **未决建模点（如实列出，不借句柄保存反证效果应跨宠）**：若官方实证应随施放方
//   下场失效，句柄应迁 on_stage 且 enter/下场时拆旧挂新；若实证战斗级常驻则维持
//   现状。另：TEAM watcher 常驻不产生"队友串权"——回调 ev.actor!=self 早退，
//   仅施放者本人的攻击被限制才申报（j_gaia_stance.cpp 2300 回调首门）。
//   迁槽与否以官方实测为准，两案均为插件侧改动、零内核。
//
// ── 联动面（2377 逆转轨迹 × J11 姿态）────────────────────────────────
// 魂印 2377 第⑥句「逆转轨迹：武道中的抵挡攻击效果改为以百分比伤害形式反弹
// 受到的攻击伤害」——"逆转轨迹"= J11 姿态机的逆转档（2292「改为逆转」置位、
// 2293「复原」归零、2294「衍化自身的武道」取反）。soul 侧抵挡回调经
// gaia_stance() 读本槽裁决 抵挡/抵挡+反弹，零新内核通道。
//
// ── 键段（9901129xx 一段一族；J11 2026-10-02 原段沿用，soul 四键顺延）──
// 全部 on_stage_storage（另注者除外）；map 每宠一份，side 天然隔离，
// 键不再 +side（镜像对局双方各一只盖亚各自成槽，互不串）。
constexpr int kGaiaStanceKey = 990112911;      // 姿态 int：0=正转（初始）1=逆转
                                               // （moves 写 / soul 读——联动面）
constexpr int kGaiaAccountKey = 990112921;     // 正转期间挡下的伤害账 int
constexpr int kGaiaEvolveKey = 990112931;      // 2293：下 N 次衍化全属性+1 int
constexpr int kGaiaArmedKey = 990112941;       // 记账节点已武装旗 int
constexpr int kGaiaWayKey = 990112951;         // 道位 int：0=血 1=邪 2=天（2377）
constexpr int kGaiaEntryRoundKey = 990112952;  // 本次登场的 roundCount（round N
                                               // 时 roundCount==N-1，lag 一轮）
// 对手手龄追踪（des 447「本次在场超过2回合的精灵」）：plugin_storage +side（973/974
// 槽位快照、975/976 入场轮）——**战斗级追踪**：EVENT_SWAP watcher（主动中切有 emit，
// battleFsm.cpp:1641；死亡换宠无 emit → ROUND_START 槽变更兜底，半轮口径记档）全战斗
// 记录对手当前宠入场轮，我方任意时刻（含中途登场）读到的都是对手真实手龄，不再从盖亚
// 自己登场重锚。首入场种子=开局轮 0：开局在场对手恒精确；对手入场前已换宠取最老估计
// （残差记档）。
// ⚠️ +side 偏移族键距纪律（两次实锤后成文，2026-11-11 展开审计）：plugin_storage
// +side 复合键的相邻基距必须间隔 ≥ side 上限 2——990113002 曾撞 990113001+1 致 side1
// 追踪旗永不注册（T8376 前身实锤）；990112974 曾同时是 kGaiaOppSlotSideKey(973)+side1
// 与 kGaiaOppEntrySideKey(974)+side0 的展开位 → 镜像对局 side1 写对手槽位快照会覆写
// side0 的对手入场轮（2026-11-11 展开审计实锤，kGaiaOppEntrySideKey 已改 990112975，
// T8378e 镜像差分锁）。
constexpr int kGaiaOppSlotSideKey = 990112973;   // 对手在场槽位快照（plugin+side = 973/974）
constexpr int kGaiaOppEntrySideKey = 990112975;  // 对手当前宠入场 roundCount（plugin+side =
                                                 // 975/976；原 974 与 973+side1 碰撞见上注）
constexpr int kGaiaCycleKey = 990112955;       // 完整轨迹轮回数 int（审计探针）
constexpr int kGaiaBlockEpochKey = 990112956;  // 抵挡观察者 epoch 幂等旗 int
// ── 2026-11-10 三次修正（现行 effect_des 446-449 证据落地）──────────────
// soulmark_storage（本场战斗累计量——des446「本场战斗自身在场的前10回合」是
// 战斗累计在场，非本次登场；换宠不清累计，本次上场盘面仍按开启子句重开）：
constexpr int kGaiaFieldRoundsKey = 990112977;  // 本场战斗累计在场回合 int
                                               // （soulmark；ROUND_START 且在场 +1）
// on_stage（轨迹位置随开启子句重开——「登场时…初始为血之道」）：
constexpr int kGaiaTrajPosKey = 990112978;      // 声明轨迹位置 int（active 序列下标）
// ── 2026-11-10 K449 续作（2574/2575 + 联动面预留）────────────────────────
// ⚠️ 生命周期纪律（主会话 2026-11-10 定）：武道轨迹键**留在 on_stage_storage 不迁**
//    ——「场上存在武道」按双方**在场**实际武道/不武建模（effect_icon 2147 ②原文），
//    不扫描替补武道充当触发源；雷伊裁定不套盖亚，盖亚下场语义未裁定前维持现状
//    （迁 soulmark_storage 需官方实测依据，另行立项）。
constexpr int kGaiaActiveKey = 990112957;      // 武道开启旗 int（2377 enter 置 1；
                                               // 2575「消除对手的武道」清 0——
                                               // 消除后 2377 抵挡观察器停止触发；
                                               // 缺省 0 与血道 0 值解歧义）
constexpr int kGaiaShengKey = 990112958;       // 胜之道开启旗 int（soul 2547 ②；
                                               // 预留键——触发挡伤后关闭，语料 idx488）
// ⚠️ 2574 窗口模型（2026-11-10 二次修正，主会话驳「场地级跨换宠免断」推断）：
//    改走**普通 N 回合窗**（2291 同款：当回合直写 + NextRounds ROUND_START tick 重写，
//    条目属施放方）——离场（施放方换宠→条目 epoch 作废）/断回合（invalidate_all_
//    round_effects）/到期（tick 耗尽）三路全部自动停；查询改读 **stamp**（tick 每轮
//    重写为 roundCount+1 的绝对轮戳，条目死则戳冻结 ≤ roundCount → 查询自假），
//    见 gaia_bench_target_locked。kGaiaShield2574Key 存 stamp 而非过期轮。
constexpr int kGaiaShield2574Key = 990112959;  // 2574 场地盾 stamp int（plugin_storage
                                               // +side = 959/960；stamp > roundCount 活跃）
// plugin_storage（特例，见上）：
constexpr int kGaiaW2300HandleKey = 990112971;  // 2300 watcher 句柄（TEAM，换绑用；
                                               // +side = 971/972）
constexpr int kGaiaBuWuKey = 990112981;         // 不武计数 int（plugin_storage +side =
                                               // 981/982；⚠️ 原 990112972 与
                                               // 2300 句柄 side1(971+1) **碰撞**
                                               // ——2026-11-10 主会话实锤，基础键
                                               // 静态扫描查不出偏移复合键，已改段；
                                               // des 569「己方获得的」side 级资源
                                               // 上限 4、额外视为武道；soul 2547 ①
                                               // 写（率通道阻塞未实现）②消费）

#include <any>
#include <map>

// 本地版安全读 int（语义同 soul_lib/moves_lib 各自 common.h 的 any_int_or：
// 类型不符按缺失回退，绝不抛——fuzz any_cast_fault 家族纪律）。不 include 两边
// common.h：两插件头是两份文件，本头必须自包含。
inline int gaia_int_or(const std::any& a, int fallback = 0) {
    if (const int* p = std::any_cast<int>(&a)) {
        return *p;
    }
    return fallback;
}

// ── per-pet int 槽存取（全部经本头，check_storage_keys 单文件纪律）────────
inline int gaia_pet_int(const BattleContext* ctx, int side, int key) {
    if (!ctx || side < 0 || side > 1) {
        return 0;
    }
    const auto& slot = ctx->seerRobot[side].elfPets[ctx->on_stage[side]]
                           .on_stage_storage;
    const auto it = slot.find(key);
    return it == slot.end() ? 0 : gaia_int_or(it->second);
}

inline void gaia_pet_set(BattleContext* ctx, int side, int key, int value) {
    if (!ctx || side < 0 || side > 1) {
        return;
    }
    ctx->getPet(side).on_stage_storage[key] = value;
}

// ── soulmark_storage 存取（下场保留槽——本场战斗累计量宿主）──────────────
// 寻址 = **在场宠**的 soulmark_storage（累计量在宠对象上跨切换保留；宠再登场时
// on_stage[side] 重新指向它 → 读写续接）。仅限「本场战斗累计」语义的键。
inline int gaia_soul_int(const BattleContext* ctx, int side, int key) {
    if (!ctx || side < 0 || side > 1) {
        return 0;
    }
    const auto& slot = ctx->seerRobot[side].elfPets[ctx->on_stage[side]]
                           .soulmark_storage;
    const auto it = slot.find(key);
    return it == slot.end() ? 0 : gaia_int_or(it->second);
}

inline void gaia_soul_set(BattleContext* ctx, int side, int key, int value) {
    if (!ctx || side < 0 || side > 1) {
        return;
    }
    ctx->getPet(side).soulmark_storage[key] = value;
}

// ── 姿态（联动面便捷读）─────────────────────────────────────────────
inline int gaia_stance(const BattleContext* ctx, int side) {
    return gaia_pet_int(ctx, side, kGaiaStanceKey);
}

// ── 道位（2377）：0=血之道 1=邪之道 2=天之道 ────────────────────────
inline int gaia_way(const BattleContext* ctx, int side) {
    return gaia_pet_int(ctx, side, kGaiaWayKey);
}

// ── 对手手龄（des 447 邪道门；战斗级，跨我方切换）───────────────────
// ── side 级资源读写（plugin_storage +side：不武计数 / 2574 场地盾过期轮）────
inline int gaia_side_int(const BattleContext* ctx, int side, int key) {
    if (!ctx || side < 0 || side > 1) {
        return 0;
    }
    const auto it = ctx->plugin_storage.find(key + side);
    return it == ctx->plugin_storage.end() ? 0 : gaia_int_or(it->second);
}

inline void gaia_side_set(BattleContext* ctx, int side, int key, int value) {
    if (!ctx || side < 0 || side > 1) {
        return;
    }
    ctx->plugin_storage[key + side] = value;
}

// ── 武道在位查询（联动面共享判据；2026-11-10 口径 = 双方**在场**实际武道/不武）──
// 轨迹武道：在场宠 2377 开启旗（2575「消除」可清 → 挡伤观察器停）。
inline int gaia_opp_slot(const BattleContext* ctx, int side) {
    return gaia_side_int(ctx, side, kGaiaOppSlotSideKey);
}

inline int gaia_opp_entry_round(const BattleContext* ctx, int side) {
    return gaia_side_int(ctx, side, kGaiaOppEntrySideKey);
}

inline bool gaia_traj_dao_active(const BattleContext* ctx, int side) {
    return gaia_pet_int(ctx, side, kGaiaActiveKey) == 1;
}

// 胜之道：4947 soul 2547 ②开启、触发挡伤后关闭（语料 idx488）。
inline bool gaia_sheng_active(const BattleContext* ctx, int side) {
    return gaia_pet_int(ctx, side, kGaiaShengKey) == 1;
}

// 不武（side 级）：des 569「不武额外视为武道」——计入「拥有武道」判据。
inline int gaia_buwu(const BattleContext* ctx, int side) {
    return gaia_side_int(ctx, side, kGaiaBuWuKey);
}

// 「拥有武道」总判据（2575 门共享；570 胜之道挡伤门亦按此——官方 570 原文
// 「拥有武道的对方」含 2377/胜之道/不武≥1，无 ≥2 门槛，idx488 的「至少2层」
// 是社区读法未采信、不硬编码）：轨迹武道 ∥ 胜之道 ∥ 不武≥1。
inline bool gaia_side_has_wudao(const BattleContext* ctx, int side) {
    return gaia_traj_dao_active(ctx, side) || gaia_sheng_active(ctx, side)
           || gaia_buwu(ctx, side) >= 1;
}

// ── 共用衍化函数（des 449 定版口径，2026-11-10 三次修正）────────────────
// des 449：「武道的衍化遵循精灵所声明的衍化轨迹，当武道没有可衍化的对象时，
// 武道的衍化改为关闭」。声明轨迹（2377 tips①）= 血→邪→天→血；逆转（2292）
// 沿同一名单反向行走（建模决策，官方未明写反向序列字面——记档）。2294 与
// 2377 触发后衍化**共用本函数**（同一"衍化"一个动作：推进道位 + 消费 2293
// 次数 + 走完轨迹回满 PP），不再允许 moves/soul 两机各自解释。
// 轨迹序列（way 值）：正转 [0,1,2,0]、逆转 [0,2,1,0]——末位=血=「完成一次
// 完成轨迹的衍化」→ 全 PP；到末位后无后继 → 衍化关闭（2293「复原」重启）。
// 原语经参传入（插件 TU 传 g_core->stat_change/g_core->pp_restore；场景 TU 传
// battle_primitives 自由函数——插件 dylib 不链接 sim_core，头内不能直引符号；
// 返回类型各异（StatChangeResult/int）→ 模板解耦签名，只要求可空指针可调用）。
template <typename StatChangeFn, typename PpRestoreFn>
inline bool gaia_wudao_evolve(BattleContext* ctx, int side,
                              StatChangeFn stat_change_fn,
                              PpRestoreFn pp_restore_fn) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    const int pos = gaia_pet_int(ctx, side, kGaiaTrajPosKey);
    constexpr int kSeqLen = 4;                 // 血→邪→天→血（含起点与终点）
    if (pos >= kSeqLen - 1) {
        return false;                          // 无后继 → 衍化关闭（des 449）
    }
    const int seq_fwd[kSeqLen] = {0, 1, 2, 0};
    const int seq_rev[kSeqLen] = {0, 2, 1, 0};
    const bool reverse = gaia_stance(ctx, side) == 1;
    const int next_pos = pos + 1;
    const int next_way = reverse ? seq_rev[next_pos] : seq_fwd[next_pos];
    gaia_pet_set(ctx, side, kGaiaTrajPosKey, next_pos);
    gaia_pet_set(ctx, side, kGaiaWayKey, next_way);
    // 2293「下{0}次衍化时全属性+1」：任何衍化（技能侧/魂印触发）统一消费。
    const int evolve_left = gaia_pet_int(ctx, side, kGaiaEvolveKey);
    if (evolve_left > 0) {
        gaia_pet_set(ctx, side, kGaiaEvolveKey, evolve_left - 1);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (stat_change_fn) {
                (void)stat_change_fn(ctx, side, i, 1);
            }
        }
    }
    // 「每完成一次完成轨迹的衍化，自身恢复全部PP值」：抵达末位（血）=完成。
    if (next_pos == kSeqLen - 1 && pp_restore_fn) {
        (void)pp_restore_fn(ctx, side, /*amount=*/999, /*only_empty=*/false);
    }
    if (next_pos == kSeqLen - 1) {
        // 审计探针：完成轨迹轮回数（kGaiaCycleKey，on_stage，enter 重置）。
        gaia_pet_set(ctx, side, kGaiaCycleKey, gaia_pet_int(ctx, side, kGaiaCycleKey) + 1);
    }
    return true;
}

// 2574 场地盾（「双方的在场精灵无法成为场下精灵的效果目标」）：**普通 N 回合窗**
// （2291 同款）——施放当回合直写 stamp（=roundCount+1）+ NextRounds ROUND_START
// tick 逐轮重写 stamp；条目属施放方 → 施放方换宠/被断回合/到期三路条目死亡后
// stamp 冻结 ≤ roundCount → 查询自假（离场/断回合/到期三路语义由普通 N 回合窗条目
// 生命周期自动承载）。
// 查询 = 严格当轮匹配（戳=当轮绝对轮号；ROUND_START tick 逐轮重写、失守首轮清零、
// 条目死亡冻结旧号）——roundCount 驱动后已进位，旧「stamp > roundCount」比较在
// 驱动后恒假；当轮匹配在 施放轮/守窗轮 的回合内恒真、失守轮当场转假。
// 消费面（2026-11-11 派发缺口 2 全量清查后定稿）：全库「场下源→场上目标」已实现
// 入口共三处，全部接入本门（各自带「源在场放行」半边，2513 场域同款门形）：
//   ① j_defeat_family.cpp 349/345 终命之璨/辉 支援回血（受援者=在场击败者）——
//      T8374.d；349 语义本就场下源（持有者在场不复读），单门直判。
//   ② roster_field.cpp 903 星皇之赐（场下星皇→在场行动精灵回 1/8）——T8374.e；
//      星皇首发在场放行。
//   ③ k421_meiheweite_lyrics.cpp 2456 永沐挂曝（背包梅赫维特→执行方在场宠挂曝）——
//      T8374.f；梅赫维特在场放行。
// 其余候选逐一核伪（官方文本载体不在「场下源→场上目标」形态，无门可挂——
// 详表见 K449 完成报告）：3550 之罚/之赐与 2532/1581/2546 目标均在**场下**（方向
// 反转）；380 圣剑重铸/复活族目标=**阵亡宠**（非「在场精灵」）；3512 死亡传承源=
// **阵亡宠**（majority reading：2546「场下首位阵亡精灵」措辞区分阵亡与场下存活，
// 阵亡不算「场下精灵」——残差记档）；moves 903 为在场攻击者附加（同号异义）；
// 2599 圣荷慰心支援官方文在场下→场上形态但全库无实现（无落点）。
inline bool gaia_bench_target_locked(const BattleContext* ctx, int target_side) {
    if (!ctx || target_side < 0 || target_side > 1) {
        return false;
    }
    const int stamp = gaia_side_int(ctx, target_side, kGaiaShield2574Key);
    return stamp != 0 && stamp == ctx->roundCount + 1;
}

// 场景/审计直读戳（绝对轮号；0 = 未活跃/已清零）。
inline int gaia_shield_stamp(const BattleContext* ctx, int target_side) {
    return gaia_side_int(ctx, target_side, kGaiaShield2574Key);
}

#endif // GAIA_WUDAO_RUNTIME_H
