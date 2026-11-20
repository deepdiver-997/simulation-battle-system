// K449 · 4766 灭战神决·盖亚 魂印 2377 —— 武道三道轮回机（血/邪/天 道位 × J11 姿态）
//（2026-11-10，worktree k449；全库首实现——S29 调研 §二.4 立案的第二阶段）
//
// 行清单（effect_icon row 1967 主库现查，唯一携带宠 4766；双库一致；args "6 6 50 1"）：
//   tips 七段：
//   ① 「登场时自身开启武道，初始为血之道,武道会以血、邪、天、血的轨迹进行衍化，
//      每完成一次完成轨迹的衍化，自身恢复全部PP值」
//   ② 「回合结束时自身恢复最大体力的1/6，自身处于血之道时效果翻倍；自身使用技能时
//      附加对手最大体力1/6的百分比伤害，自身处于邪之道时效果翻倍；自身造成的攻击
//      伤害提升50%，自身处于天之道时效果翻倍」
//   ③ 「血之道：自身抵挡本场战斗自身在场的前10回合受到的攻击，触发后衍化」
//   ④ 「邪之道：自身抵挡本次在场超过2回合的精灵的攻击，触发后衍化」
//   ⑤ 「天之道：自身抵挡非致命一击，触发后衍化」
//   ⑥ 「逆转轨迹：武道中的抵挡攻击效果改为以百分比伤害形式反弹受到的攻击伤害」
//   ⑦ 「（boss无效）」
// ⚠️【库版留档（2026-11-10 K450 新库 ConfigPackage20261106181039，指纹
//   1ad2b1186ca28f94）】新库 row 1967 tips 缩为 3 段：③④⑤三道抵挡子句与
//   ⑥逆转轨迹子句被官方包移除，仅存 ①轨迹衍化+轮回 PP、②三段道位条件效果、
//   ⑦boss无效。本实现的三道抵挡/触发衍化/逆转轨迹机按**移除前官方原文**
//   （2026-10-22 快照 ~/.agent-workspaces/db_backup/seer_unity_20261022.sqlite
//   row 1967 全文）+BWIKI 攻略语料落地，实现子句在现库已无 tips 支撑——
//   「衍化触发条件在现行官方文本中未定义」如实留档；场景 8370 三道轮回腿
//   按 K449 看板行验收本机实现，若官方实证道机另有触发（如技能侧 2294 推进/
//   2547 胜之道互动），撤抵挡观察者保留②族即可，余量留续单。
//   行参切分：db[0]=6 回合末回血分母、db[1]=6 使用技能附加分母、db[2]=50 增伤%、
//   db[3]=1 恒 1（官方 tips 无对应子句，不消费——808 db[2] 同款纪律，记档）。
//   血道 10 回合窗 / 邪道 2 回合阈为 tips 字面常量（args 无对应位）。
//   BWIKI/官方语料交叉：fandom/4399 tips 与 DB 原文逐字一致（2026-11-10 web 现查），
//   无超出 DB 文本的判定细则——下列口径歧义按字面+家族内锚裁决，全部留档：
//
//   【口径 D1】「每完成一次轨迹的衍化」= **走完 血→邪→天→血 全轨迹**（回到血之道）
//     才回满 PP，非逐次衍化回满。依据：轨迹句把「血」列两次（起点+终点），"完成
//     轨迹"指走完该列；逐次回满没有"完成轨迹"字样的落点。反读（逐次回满）无文本
//     支撑，留档不采。
//   【口径 D2】「逆转轨迹…改为以百分比伤害形式反弹受到的攻击伤害」= 抵挡仍生效
//     （final 归零），并将**挡前 final** 以 PERCENT_VALUE 粉伤反弹攻方（100%）。
//     依据：家族内 2292「把正转期间**抵挡的伤害**以固伤反馈」同族口径——"抵挡的
//     伤害"=挡前值；且若逆转改为"照常受伤+反弹"则道机防御身份消失，与"武道中的
//     **抵挡**攻击效果**改为**反弹"（改形态不改存废）文意相悖。反读（受伤+反弹）
//     留档不采。
//   【口径 D3——2026-11-10 废除】旧读「按本次登场起算」无据：现行 des 446 原文
//     「本场战斗自身在场的前10回合」= **战斗累计在场**（kGaiaFieldRoundsKey
//     soulmark 跨切换保留、ROUND_START 逐轮+1）；登场开启（盘面重开）与战斗累计
//     在场是两件事，已分离建模（T8371E 差分锁定）。
//   【口径 D4——2026-11-10 三次订正（旧"阻塞注"作废）】「邪之道：本次在场超过
//     2回合的精灵」= **攻方**（对手当前宠）**本次登场以来**连续在场 >2 回合
//     （des 447 原文）。追踪=战斗级 EVENT_SWAP watcher（battleFsm.cpp:1658/1685
//     **主动中切实际 emit**，首回合房主先登 defer :1648-1660；死亡换宠不 emit，
//     :640 注）+ ROUND_START 槽变更兜底。旧注「EVENT_SWAP 已声明未 emit、插件
//     无法观察对手入场、需内核补 emit」**撤回**——那是对 :640 死亡换宠注释的误读。
//     现实现对手手龄不从盖亚登场重锚（T8377a 锁）；同回合对手中切由 watcher 当场
//     重锚（T8377b 锁）。残差（如实记档，无内核阻塞）：①对手在盖亚首次登场**前**
//     已换宠 → 首入场种子取最老估计 entry=0（手龄高估 → 邪道多挡不少挡）；
//     ②死亡换宠 → 次轮 ROUND_START 兜底重锚（半轮口径，入场部分轮不计）。
//   【口径 D5】三道抵挡不响应属性直伤（skip_routine_block，J11 记账同款护栏），
//     只认红伤（isRed）。
//   【口径 D6——2026-11-10 废除】旧「正交双动作」（2294 翻向 / 本机推道位）被
//     主会话实锤为存量缺陷（同一"衍化"两套动作不成立，旧 T963 断言正是缺陷
//     锁）：现行 des 449 定版共用衍化函数 gaia_wudao_evolve——沿声明轨迹推进
//     一道 + 消费 2293 次数 + 走完轨迹回满 PP + 无后继衍化关闭；2294 与魂印
//     触发衍化共用同一函数（j_gaia_stance.cpp 2294 已改），963 T963.a 已随改。
//
// 联动（派发点名 4766/4758 现查结论）：4758 神决·盖亚魂印 2385（row 1969）=
// 固伤/增伤/回血条件翻倍机，无武道子句；五技 229x 全部 4766 独占（monsters.raw_json
// 现查，4758 不带）——**两形态间无直接联动通道**。真正的机间联动在 4766 自身：
// 魂 2377 道位机 × J11 姿态机（2292/2293/2294 写 kGaiaStanceKey，本机抵挡回调读
// 同一槽裁决 抵挡/抵挡+反弹——共享头 gaia_wudao_runtime.h，零新内核通道）。
// 联动验证走场景 8370 联动腿（2292 改逆转 → 抵挡转反弹；2293 复原 → 回抵挡）。
//
// 三查记录（2026-11-10）：①内核 grep 2377 无行为硬编码（effect_info 2377=技能空间
// 同号异义「对手每处于1种能力下降威力提升{0}」——坑集 §六.2 撞号甄别，魂印以
// effect_icon tips 为权威）②effect_meta 无声明 ③soul_lib/registry 无 2377 注册
// （grep 全库 soul 侧零命中）——真缺口。载体唯一：4766（effect_icon pet_id=[4766]，
// row 1967；boss_invalid 名单在列）。
// 先例：K180 608（soul 侧 enter+BLOCK 观察者+反弹）、K341 711（EVENT_SKILL_USED
// watcher+回合末回血）、K172 808（db_at 行参/恒 1 废位）、J11 2291（ws 增伤通道）、
// 2274（pp_restore 全满）、crawford_runtime.h（跨 dylib 共享头形态）。
// 场景回归：test/scenario/scenario_8370_gaia_wudao_2377.cpp（8370 号段）。

#include "common.h"

#include "../gaia_wudao_runtime.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 2377;
constexpr int kWatchWiredKey = 990113001;   // plugin_storage：EVENT_SKILL_USED watcher
                                            // 已挂旗（TEAM watcher 战斗级一次，K341 同款）
// ⚠️ 键距纪律（2026-11-10 二次实锤）：+side 偏移复合键的碰撞基础键扫描查不出
// （kOppTrackWiredKey+0 原取 990113002 撞 kWatchWiredKey(…001)+1 → side1 的 rider
// 永不注册）——段内旗键取**奇偶隔离基距**：watcher 旗 990113001/002、追踪旗
// 990113003/004，相邻基距至少间隔 side 上限（2）。
constexpr int kOppTrackWiredKey = 990113003; // plugin_storage：EVENT_SWAP 手龄追踪
                                            // watcher 已挂旗（战斗级一次，含首入场初始化）
// tips 字面常量（args 无对应位，D1/D3/D4 口径）：
constexpr int kBloodWindowRounds = 10;      // 血之道：在场前 10 回合
constexpr int kEvilStayThreshold = 2;       // 邪之道：攻方在场超过 2 回合

// ── 宿主行参数（方案 B：db_at 读 owned_int_args 原序，与真实对局一致）─────
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
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
    const ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    row.db = &host.soulMark.args.owned_int_args;
    row.monster_id = host.soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// BLOCK 回调/watcher 无 SoulNodeArgs，走 find_pet_with_soulmark 兜底读行参。
HostRow host_row_probe(BattleContext* ctx, int owner, int mark_id) {
    SoulNodeArgs a;
    a.owner = owner;
    a.opp = 1 - owner;
    a.host_slot = ctx ? ctx->find_pet_with_soulmark(owner, mark_id) : -1;
    return host_row(ctx, a, mark_id);
}

// 对手最大体力（面板口径，heal/粉伤管线同源——j_defeat_family 同读法）。
int opp_max_hp(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return 1;
    }
    const ElfPet& opp = ctx->seerRobot[1 - owner].elfPets[ctx->on_stage[1 - owner]];
    return opp.numericalBase[NumericalPropertyIndex::HP];
}

// 某侧当前宠连续在场回合数（round N 时 roundCount==N-1，lag 一轮 → +1 归位）。
int stay_rounds(const BattleContext* ctx, int entry_round) {
    if (!ctx) {
        return 0;
    }
    return ctx->roundCount - entry_round + 1;
}

// ── 对手在场追踪刷新（enter + 每回合 ROUND_START；D4 口径）────────────────
void k2377_refresh_opp(BattleContext* ctx, int owner) {
    const int opp = 1 - owner;
    const int cur_slot = ctx->on_stage[opp];
    if (gaia_opp_slot(ctx, owner) != cur_slot) {
        // 主路径由 EVENT_SWAP watcher 精确重锚；此兜底只接**死亡换宠**（无 emit，
        // battleFsm.cpp:640 口径）的槽变更——入场轮记为检测轮（半轮口径：死亡换宠
        // 入场的部分轮不计入手龄，记档）。主动中切时槽相同不会误触发。
        gaia_side_set(ctx, owner, kGaiaOppSlotSideKey, cur_slot);
        gaia_side_set(ctx, owner, kGaiaOppEntrySideKey, ctx->roundCount);
    }
}

// ── 登场：开启武道（初始血之道）+ 武装抵挡观察者 + 攻击附加 watcher ────────
EffectResult k2377_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->pp_restore || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    // ① 开启武道：初始血之道 + 登场回合 + 对手在场快照（on_stage_storage 换宠由
    //    引擎清空——"登场时开启武道"下场重置零内核支持，crawford 同款）。
    //    开启旗 kGaiaActiveKey（2026-11-10 K449 续作）：缺省 0 与血道 0 值解歧义，
    //    2575「消除对手的武道」清此旗 → 抵挡观察器停止触发（消除武道的引擎落点）。
    gaia_pet_set(ctx, owner, kGaiaWayKey, 0);
    gaia_pet_set(ctx, owner, kGaiaActiveKey, 1);
    gaia_pet_set(ctx, owner, kGaiaTrajPosKey, 0);
    gaia_pet_set(ctx, owner, kGaiaEntryRoundKey, ctx->roundCount);
    gaia_pet_set(ctx, owner, kGaiaCycleKey, 0);
    // 对手手龄：**不重锚**（des 447 退修点——对手已站 N 轮后我方中切，手龄保留）。
    // 追踪 = 战斗级 EVENT_SWAP watcher（主动中切有 emit；死亡换宠无 emit 走
    // ROUND_START 槽变更兜底）。首入场（旗未挂）种子 = **开局轮 0**：开局在场对手
    // 恒为开局入场（精确）；对手若在我入场前已换宠（引擎无 on_stage_since、swap
    // 事件先于我 watcher 不存在）→ 取最老估计（邪道多挡不少挡），残差记档。
    if (!ctx->plugin_storage.count(kOppTrackWiredKey + owner)) {
        ctx->plugin_storage.emplace(kOppTrackWiredKey + owner, 1);
        gaia_side_set(ctx, owner, kGaiaOppSlotSideKey, ctx->on_stage[1 - owner]);
        gaia_side_set(ctx, owner, kGaiaOppEntrySideKey, 0);   // 开局轮种子（见上注）
        ctx->event_center_.register_watcher(
            EventType::EVENT_SWAP, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor != 1 - owner) {
                    return;   // 只追踪对手侧中切（emit 时 on_stage 已更新，:1636 注）
                }
                gaia_side_set(c, owner, kGaiaOppSlotSideKey, c->on_stage[1 - owner]);
                gaia_side_set(c, owner, kGaiaOppEntrySideKey, c->roundCount);
            },
            WatcherScope::TEAM);
    }
    k2377_refresh_opp(ctx, owner);
    // ② 抵挡观察者：每 epoch 一次（K180 arm 幂等同款；旗在 on_stage_storage——
    //    换宠清槽=再登场重武装，与引擎 pipeline epoch 作废合成完整下场语义）。
    ElfPet& pet = ctx->getPet(owner);
    auto it = pet.on_stage_storage.find(kGaiaBlockEpochKey);
    if (it == pet.on_stage_storage.end()) {
        it = pet.on_stage_storage.emplace(kGaiaBlockEpochKey, int{-1}).first;
    }
    if (int* epoch = std::any_cast<int>(&it->second)) {
        if (*epoch == ctx->pipeline_valid_id[owner]) {
            return EffectResult::kOk;   // 同上场期重复触发（如 retrigger_entrance）→ 不重挂
        }
        *epoch = ctx->pipeline_valid_id[owner];
    }
    const int self = owner;
    ctx->register_damage_effect(
        DamagePhase::BLOCK, self, DamageEffectCategory::BLOCK,
        [self](BattleContext* c, int bucket_owner) {
            if (!c || !g_core || !g_core->deal_pink_damage || !g_core->pp_restore) {
                return;
            }
            if (c->is_boss_challenge) {
                return;   // ⑦ boss 无效（观察者手动门）
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;   // 只认落在自己身上的红伤（D5）
            }
            if (!onstage_has_soul_mark(c, self, kMarkId)) {
                return;
            }
            if (d.skip_routine_block) {
                return;   // 属性直伤不触发抵挡/衍化（D5，J11 记账同款护栏）
            }
            if (!gaia_traj_dao_active(c, self)) {
                return;   // 武道被消除（2575 等）→ 抵挡/衍化停止（开启旗缺省 0 与
                          // 血道 0 值解歧义，2026-11-10 K449 续作）
            }
            const int way = gaia_way(c, self);
            // ── 各道抵挡条件（③④⑤；不中即不触发、照常受伤）──
            bool trigger = false;
            if (way == 0) {
                // 血之道（des 446 现行原文）：「自身抵挡本场战斗自身在场的前10回合
                // 受到的攻击」——**本场战斗累计在场**（kGaiaFieldRoundsKey，soulmark
                // 跨切换保留、ROUND_START 逐轮 +1），非本次登场窗。2026-11-10 修正
                // 旧 D3 误读（原文无「本次登场」字样）。
                trigger = gaia_soul_int(c, self, kGaiaFieldRoundsKey) <= kBloodWindowRounds;
            } else if (way == 1) {
                // 邪之道：攻方（对手当前宠）本次在场超过 2 回合（D4）
                const int opp_entry = gaia_opp_entry_round(c, self);
                trigger = stay_rounds(c, opp_entry) > kEvilStayThreshold;
            } else if (way == 2) {
                // 天之道：抵挡非致命一击（暴击穿透）
                trigger = !d.isCrit;
            }
            if (!trigger) {
                return;
            }
            // ── ⑥ 逆转轨迹（联动面）：读 J11 姿态槽——逆转档抵挡转反弹（D2）──
            const int blocked = d.final;   // 挡前快照（反弹额）
            if (gaia_stance(c, self) == 1 && d.attackerId >= 0 && d.attackerId <= 1) {
                g_core->deal_pink_damage(c, d.attackerId, blocked,
                                         DamageKind::PERCENT_VALUE, self);
            }
            // 抵挡落地：133 同位全字段清零（下游不读幻影伤害）
            d.base = 0;
            d.afterAdd = 0;
            d.afterMul = 0;
            d.final = 0;
            d.addPct = 0;
            d.mulCoef = 0.0;
            // ── 触发后衍化（des 446-448「触发后衍化」+ des 449 衍化遵循声明轨迹）
            //    → **共用衍化函数** gaia_wudao_evolve（与 2294 同一动作：沿声明轨迹
            //    推进一道 + 消费 2293 次数 + 走完轨迹回满 PP + 无后继衍化关闭）。
            //    旧 D6「正交双动作」（2294 翻向 / 本机推道位）按主会话实锤废除。
            (void)gaia_wudao_evolve(c, self,
                                    g_core ? g_core->stat_change : nullptr,
                                    g_core ? g_core->pp_restore : nullptr);
        });
    // ③ 攻击附加 watcher（②「自身使用技能时附加对手最大体力1/6的百分比伤害」）：
    //    TEAM 作用域战斗级一次（K341 同款——重复登场不叠加，旗在 plugin_storage）。
    if (!ctx->plugin_storage.count(kWatchWiredKey + owner)) {
        ctx->plugin_storage.emplace(kWatchWiredKey + owner, 1);
        ctx->event_center_.register_watcher(
            EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                if (!c || !g_core || !g_core->deal_pink_damage) {
                    return;
                }
                if (ev.actor != owner) {
                    return;
                }
                if (c->is_boss_challenge) {
                    return;   // ⑦ boss 无效（watcher 手动门）
                }
                if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                    return;
                }
                const HostRow row = host_row_probe(c, owner, kMarkId);
                const int denom = db_at(row, 1, 6);   // db[1]=分母 6
                if (denom <= 0) {
                    return;
                }
                // 使用技能附加：②原文**无条件基础值**（1/6）+「自身处于邪之道时
                // 效果翻倍」（基础/翻倍拆分，翻倍判 active——同前注）。
                int rider = opp_max_hp(c, owner) / denom;
                if (gaia_traj_dao_active(c, owner) && gaia_way(c, owner) == 1) {
                    rider *= 2;   // 邪之道翻倍（②）
                }
                if (rider > 0) {
                    g_core->deal_pink_damage(c, 1 - owner, rider,
                                             DamageKind::PERCENT_VALUE, owner);
                }
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 回合开始：攻击伤害 +{2}%（天之道翻倍）重写 AMP 加算通道（J11 2291 同窗法）
//    + 对手在场追踪刷新（D4）───────────────────────────────────────────────
EffectResult k2377_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    k2377_refresh_opp(ctx, owner);
    // 累计在场回合（des 446「本场战斗自身在场的前10回合」）：ROUND_START 且在场
    // 逐轮 +1，soulmark 跨切换保留——「换宠不清累计」与「登场盘面重开」两件事。
    gaia_soul_set(ctx, owner, kGaiaFieldRoundsKey,
                  gaia_soul_int(ctx, owner, kGaiaFieldRoundsKey) + 1);
    // 攻击伤害提升：②原文先给**无条件基础值**（+50%）再「自身处于天之道时效果
    // 翻倍」——基础/道位翻倍拆分（主会话实锤：消除武道≠消除整个魂印；翻倍判
    // active，防缺省 way=0 在未开启时冒充血道翻倍）。
    const HostRow row = host_row(ctx, a, kMarkId);
    const int amp = db_at(row, 2, 50);   // db[2]=50
    if (amp <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[owner] += amp;
    if (gaia_traj_dao_active(ctx, owner) && gaia_way(ctx, owner) == 2) {
        ctx->ws.damage_add_pct[owner] += amp;   // 天之道翻倍
    }
    return EffectResult::kOk;
}

// ── 回合结束：恢复最大体力 1/{0}（血之道翻倍）──（BATTLE_ROUND_END=官方
//    "战斗阶段结束时"惯例；K313/K341 同款 heal_amount 通道，吃封回血）──────
EffectResult k2377_round_end_heal(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int denom = db_at(row, 0, 6);   // db[0]=分母 6
    if (denom <= 0) {
        return EffectResult::kOk;
    }
    // 回合末回血：②原文**无条件基础值**（1/6）+「自身处于血之道时效果翻倍」
    // （基础/翻倍拆分，翻倍判 active——同 round_start 注）。
    const int max_hp = std::max(
        1, ctx->getPet(owner).numericalBase[NumericalPropertyIndex::HP]);
    int amount = max_hp / denom;
    if (gaia_traj_dao_active(ctx, owner) && gaia_way(ctx, owner) == 0) {
        amount *= 2;   // 血之道翻倍（②）
    }
    if (amount > 0) {
        (void)g_core->heal_amount(ctx, owner, amount);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（2377 入账，只增不改）────
// 全节点 boss_invalid（⑦ 尾注；arm 节点亦然——boss 对局半初始化违背 §注册形态
// "同一子句统一置位"纪律）。early 节点=登场开启武道+武装（activate_soul_mark
// 战斗开局/换宠上场即执行）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k2377_enter,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k2377_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k2377_round_end_heal,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
