// K427 · 瑞亚 4888 补散批 —— 段外载体散号 517/574/1625/2205 四宏
// （看板 K427，a01-k427 2026-11-07，worktree ~/.agent-workspaces/workspaces/sbs-local/k427，
//  分支 feat/k427；场景号段 8110-8119，场景 test/scenario/scenario_8110_ruiya_pool.cpp。
//  选载体依据：K420 头注"遗留记档"点名的瑞亚 4888 链级缺口段外散号（517/574/1625/2205
//  ——众神裁决 22563 / 众神祝福 23108 / 神母撼天 38342 / 慈恩宣叙调 38329），registry
//  基线 moves=1456 / soul=411（主库 build/registry.json 2026-11-07 现查）；4888 本体
//  已由 K420 落 2433/2434/2435/2437 四宏 + J332 落 2436 路由行，本单补齐散号段。）
//
// 行清单（effect_info.args_num 与 side_effect.arg_count 一致（517|3、574|0、1625|3、
// 2205|0），切片以 side_effect.arg_count 为准（official_data_repository.cpp:174
// query_side_effect_arg_count().value_or(args_num)，两表同值时以 side_effect 为准）；
// 切片逐位核对（json_each 顺序消耗）——2026-11-07 本 worktree DB 现查）：
//   517  [3] "{0}%概率使对手害怕，每损失{1}%体力，害怕概率提升{2}%"
//        载体 6：正义狂厉决 19143[191,517][4,60,5,5]（517 切片 [60,5,5]）、
//        众神裁决 22563[517][65,5,5]（4888 学面）、惊魂鹰爪 32946[700,517][1,50,5,5]、
//        枯枝沙尘暴 33664[945,517][50,5,5]（945 吃 1 参后 517 剩 2 参——官方切片
//        数据自缺 1 参，引擎 build_skill_effects 顺序消耗后 517 args=[5,5] →
//        int_count=4 < 5 守卫 no-op，fail-closed 由 EFFECT_ARG_GUARD 承担，记档）、
//        黑暗光球 34270[517][20,20,10]、层层压迫 34483[517][50,5,5]
//   574  [0] "消耗自身全部体力，令己方下次使用的技能必定先手、必定命中，下次命中的
//        攻击技能必定打出致命一击"
//        载体 4：众神祝福 23108[574][]（4888 学面）、白帝托孤 23479[574][]、
//        无迹 28297[574][]、堕·无神无迹 28384[2051,574][]（2051 arg_count=1、切片
//        空参——2051 args 饿死与本宏无关，记档；"574" 的其余 LIKE 命中全为 1574 子串噪声）
//   1625 [3] "造成的伤害高于{0}则{1}%令自身全属性+{2}"
//        载体 5：不灭狂刀 34626[808,1203,1625][60,300,1,3,300,100,1]（1625=[300,100,1]）、
//        芒枝交错 36455[947,1625][300,4,300,100,1]（=[300,100,1]）、
//        灭魔乱九垓 37769[2240,990,1625][50,400,400,100,1]（=[400,100,1]）、
//        闪袭 38336[1010,1625][1,50,300,100,1]（=[300,100,1]）、
//        神母撼天 38342[699,697,1533,1625,67][300,300,100,3,3]（=[100,3,3]；4888 学面）
//   2205 [0] "自身处于异常状态时，克制倍数取自身对对手、对手对自身克制倍数中的最大值"
//        （type=1 + iconInherent = 固有为——plugin_interface.h:108 口径）
//        载体 4：窃命·银岚 37471[2020,2205,1878,2158][3,15]、撬动世界 38079[2205,…]、
//        破军灭魂袭 38196[797,713,2205,1249][2,30,50]、慈恩宣叙调 38329[2205,…]
//        （4888 lv76 extra_moves，monsters.raw_json 现查；learnable 表无行）
//   链上邻号已注册不在本单：38342 的 699/697/1533/67、38329 的 2218/713/1091、
//   28384 的 2051、38329 整链 friend_side_effect 全空。
//
// 动工前置四查（2026-11-07）：
//   ① 内核 grep（(^|[^0-9.])<id>([^0-9.]|$)，src/ include/）：574/1625/2205 零命中；
//     517 命中 5 行全为 **注释**——include/abnormal-system/abnormal-types.h:287-298
//     的 effect_des 517「信仰对象」概念词条（kind=6，狂信 41 配套，kFaithTargetStorageKey=517
//     存 ElfPet::soulmark_storage）——effect_des 概念词条空间与 effect_info 技能模板
//     空间同号不相干（K420 2433 魂印空间撞号同款口径，id_collision.h"撞号只在同一个
//     map 内才有覆盖语义"）；且该键在 soulmark_storage（每宠）、本单审计键在
//     ctx->plugin_storage（战斗级），容器也不同。
//   ② effect_info 4/4 在列（517|3、574|0、1625|3、2205|0）且 == side_effect.arg_count
//     （4/4 同值）；custom_effect_programs / custom_effect_overrides /
//     custom_skill_selection_routes / custom_skill_combo_rules 四表零命中。
//   ③ 全库并集无注册：registry moves=1456/soul=411 无 4 id；resources/（moves_lib+
//     soul_lib）SKILL_EFFECT/CARRY_EFFECT 零命中；全分支（main-local/origin/main/
//     本地全部 feat）git grep 零注册；兄弟 worktree（j335/k426/endgame-walk）零命中；
//     boss_invalid_effects.json（resources/ 与 deploy/out/ 两份）零命中。
//   ④ 废弃 parser 覆盖面（EffectUnit 兜底，2026-11-05 冻结）：四条模板对
//     parse_status_inflict_primary 四 pattern（令/使+对手/自身+**紧跟{**）零命中
//     ——517「使对手害怕」后跟文字非占位符、574「令己方」非四 pattern、1625
//     「令自身全属性」后跟文字、2205 无令/使主句；parse_fallback_unit 二型
//     （恢复+自身／附加+固定伤害）与 parse_second_pass_unit 条件刀（若属性相同+威力提升/
//     先出手+降低对手所有PP/若后出手+消除对手回合类/不处于异常状态+吸取对手最大体力/
//     体力不足+秒杀）逐对零命中 → 全 4 id = NONE 安全跳过（现态 = 链节点静默 no-op，
//     无 K416 1762 型寄生）。本单注册后 clone_effect 先命中、parser 通道对该族关闭。
//
// 形态裁决与实现口径：
//   - 517 = 1365 直系同形（j_dyn_chance_drain.cpp:56，J93；「命中后{0}%令对手{1}，
//     自身每损失{2}%的体力概率提升{3}%」——517 仅把异常码从行参改为模板明写"害怕"）
//     ：动态概率 = {0} + floor(已损百分比/{1}) × {2}（已损 = (max-hp)×100/max 整数
//     分步、clamp [0,100]），apply_anomaly_checked 申报制（原语掷骰前问闸门改写再掷，
//     K422 917 三依据同款：亮节闸门管辖 + EVENT_ANOMALY_ATTACH_LOW_CHANCE 事件面 +
//     100% 短路不耗 rand），random_anomaly_duration。已损主语 = 自身（517 模板无主语，
//     同族 1365 显式"自身每损失"、1845 显式"对手每损失"——族内显式化口径互证）。
//     害怕码 = AbnormalStatusId::Fear=6（abnormal-types.h:23）。生存门（opp.hp<=0 整条
//     跳过，J129 2295 口径"对尸体施加无意义"——比直系 1365 多一道，按 K422 新纪律收）。
//     守卫：布局 [owner,target,base,step,up]（int_count>=5）、owner/target∈{0,1}、
//     base>0（36 全部载体切片 base>0；base=0 无意义）、step>0（除零防）、up 在位即消费
//     （up=0 合法=不加成；负 up 由 clamp[0,100] 兜底）。33664 载体切片缺参 → 守卫
//     no-op（官方数据自缺，不代补）。
//   - 574 = 1275 守御八方直系同形（diwang_3512_skills.cpp:207，帝网；自爆 + 传承
//     凭证族）+ 59 自爆契约（j_self_destruct_relay_boost.cpp:66：hp=0 →
//     defeat_pet(HP_CONSUME)——穿残留免死、actor=对手、先接线后自爆）。
//     · 死亡形态 = 即时 HP_CONSUME（59/753/1276 直系），**不用** 1275 的印记形态
//       （那是哈莫链复刻专用，需其 soul 文件节点③收尾——574 无此链）。
//     · 凭证授予（ charges=1，全队共享）：ws.guaranteed_first=1（必先 tier1——
//       "必定先手"独立比较项，1275 官方实测定档"必先只是高先制等级"的 boss 图标
//       不适用本档）+ ws.must_hit_grant=true（"必定命中"——81 必中族同款 plain 凭证，
//       模板无条件限定词；与 1275 的"弱化劈裂条件凭证"**有意不同**：1275 两层是官方
//       实测合同，574 无官方实测按模板字面全额授予，SOP §三.7 必中语义兜底）+
//       ws.must_crit=true（"必定打出致命一击"，must_crit 单列通道 crit_rate=0 也必暴）。
//     · 消费口径 = 1275 官方实测档：真正出手（SELECT_SKILL 且未被控拦下）即耗整束
//       ——嗑药/切换/被控不武装不消耗（used_skill_this_round 门 + armed roundCount 门
//       ——自爆当轮不消费：grant 发生在本轮 MOVE_RIGHT 之后，armed 永不等于本轮）。
//       记档两处保守档：①"下次命中的攻击技能"的 crit 在属性技出手轮不保留（整束
//       同耗，1275 同款——无 hit-detect 机器可复用，私造即违规）；②攻击 miss 轮
//       crit 同耗（"下次命中"字面语义要求 miss 保留，留官方对账翻案）。
//     · 接线（1275 同款）：MOVE_RIGHT 单态（该态双 side 全跑，battleFsm.cpp:1731）+
//       FIRST/SECOND AFTER_ACTION 双态（robotId 键控、只有行动方那条跑）+ ENTER_STAGE
//       TEAM watcher 登场重挂（tick 锚注册者 epoch——1275 场景 781 实测教训：施放者
//       死亡 → 名下 tick 作废，传承是全队资产必须活过死亡）。grant 可累计（同一
//       精灵连用两次 = 2 次，字面"下次"逐次成立，无官方反证）。
//     · 零参守卫：[owner,target]（int_count>=2）。
//   - 1625 = 1223 直系同形（j_threshold_prio_window.cpp:123，J61；"造成的攻击伤害若
//     高于{0}则…先制+{2}"）+ 2500 全属性通道（all_stats_up 六槽含命中）：
//     SKILL_EFFECT 桶武装 FIRST/SECOND AFTER_ACTION 探针（duration=1 归属本次用技，
//     robotId 键控只有行动方跑）→ 探针读 resolvedDamage.final（"造成的伤害"族=
//     结算后 final，1256/1029/1223 同读法——非 422/1533 的裸伤台账族）> {0} 才触发
//     （== 不算，1223 同款严格不等）→ roll_percent({1}) → all_stats_up(自身, {2})
//     （增益不吃免弱，2376 同款）。概率门 = 效果体 roll_percent（非异常施加、无
//     apply_anomaly_checked 申报通道，482 概率门同款本通道掷点）。守卫：布局
//     [owner,target,threshold,chance,bonus]（int_count>=5）、owner/target∈{0,1}、
//     chance∈[0,100]（0=永不，合法档）。
//   - 2205 = 2500 钩子直系（d5_D13.cpp:40；"此技能以最优系别计算克制倍数"→
//     ws.restraint_view 直写）**但语义为携带类固有为**（iconInherent + 模板无
//     "此技能"限定词——2500 显式"此技能"、2205 通篇"自身"，sprite 分类
//     plugin_interface.h:108 口径）：CARRY_EFFECT 战斗开始武装 + SKILL_EFFECT 双注册。
//     · 武装（carry_2205）：per-side 幂等，往 FIRST/SECOND SKILL_EFFECT 双态挂
//       TEAM 常驻 tick（duration=-1；robotId 键控只有本侧行动时跑）。
//     · tick（= SKILL_EFFECT 链节点同函数）：①在场宠技能组扫 carryEffects 含 2205
//       才生效（"携带=loadout 事实，效果体自判"——battleContext.cpp:179 扫描不按
//       存活过滤的配套自查，换宠后非携带者出场自动失活）；②"自身处于异常状态"
//       任一异常扫描 0..43（2340 同款）；③门开 → restraint_view[owner] =
//       max(restraint_multiplier(自身→对手), restraint_multiplier(对手→自身))
//       （克制矩阵双向格取大——防"自身→对手微弱被对手反向克制碾压"的补偿读法，
//       文本双项自洽；**只改自身出手方向**：受击方向的 max 会让"对手微弱打我"变
//       2×=自伤，非增益意图，v1 记档受击面不实现）；④门闭/未携带 → restraint_view
//       复位 -1.0（清同回合早先动作的陈旧写值——ws.reset 每回合清，但同回合多动作
//       段（额外行动）需显式复位；复位先于后写者（key 2205 小于 2500/2197 镜像
//       219790，timed_bucket 按 key 升序 → 后写者正常覆盖，2197 拦锁同款序律））。
//     · 宠元素读 ws.view_elementalAttributes（回合视图，2210/2500 同源）；
//       restraint_multiplier 原语入参 (atk[2], def[2])（core_api.h:112）。
//     · SKILL_EFFECT(2205) 注册是**硬性必需**：2205 在四载体链 side_effect 内，
//       链节点 clone_effect(2205) 查的是 SKILL_EFFECT 注册表——只注 CARRY 会让
//       链节点无实现（技能组载入即半残，fail-closed 教训 R4 §3.2）；链节点与
//       tick 写同一值（幂等双写，无串写面）。审计槽区分两路触发不区分（同值）。
//
// 存储键段（K422 同式 base=10^9+id×10^4，+owner×16+slot；新增前已 grep 全库无撞，
// tools/check_storage_keys.py 门禁兜底 rc=0）：
//   517  1005170000+owner*16+s：s0 最终概率（clamp 后）/ s1 已损百分比 / s2 申报结果
//        （ApplyAnomalyResult；-1=目标已倒整条跳过）
//   574  1005740000+owner*16+s：s0 传承剩余次数 / s1 本轮武装回合（-1=未武装，
//        消费时复位）/ s2 接线标记 / s3 本轮 crit 武装位（与 s1 同步复位）/
//        s4 自爆执行位 / s5 累计消费次数
//   1625 1016250000+owner*16+s：s0 阈值（{0} 行参消费钉）/ s1 本次 final 伤害 /
//        s2 roll 落地（1/0；-1=门闭未掷）/ s3 已消费 bonus（{2} 行参消费钉）
//   2205 1022050000+owner*16+s：s0 门（0/1）/ s1 写入倍率×1000 / s2 在场宠携带位
//        （0/1）/ s3 carry 武装标记
//
// 遗留记档（不产码）：
//   - 574 的"下次命中的攻击技能"命中保留语义（miss/属性技轮 crit 不保留）——保守档，
//     留官方对账翻案（见形态裁决）；翻案改法 = AFTER_ACTION 探针读 hit 台账分束消费。
//   - 2205 的受击方向不实现（自伤向，见形态裁决③）；额外行动段（EXTRA_ACTION 桶）
//     不武装 tick——v1 覆盖主行动，留对账。
//   - 517 载体 33664 枯枝沙尘暴切片缺参（945 吃 1 参后仅剩 2）——官方数据面问题，
//     引擎守卫 no-op 承担，留数据刀单（不在本单范围：DB 零改红线）。
//   - 1538 是内核缺口（派发明示勿碰），本单未触碰。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace {

// ── 审计槽（键段声明见文件头；K422 同式 helper）─────────────────────────────
constexpr int k517AuditBase  = 1005170000;
constexpr int k574AuditBase  = 1005740000;
constexpr int k1625AuditBase = 1016250000;
constexpr int k2205AuditBase = 1022050000;

inline int key517(int owner, int slot)  { return k517AuditBase + owner * 16 + slot; }
inline int key574(int owner, int slot)  { return k574AuditBase + owner * 16 + slot; }
inline int key1625(int owner, int slot) { return k1625AuditBase + owner * 16 + slot; }
inline int key2205(int owner, int slot) { return k2205AuditBase + owner * 16 + slot; }

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

// ── AFTER_ACTION 桶的当前行动方（diwang_3512_skills.cpp 同款）────────────────
int k427_bucket_acting_side(const BattleContext* ctx) {
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

// ═══════════════ 517 {0}%使对手害怕，每损失{1}%体力概率+{2}% ═══════════════
// 官方模板："517 {0}%概率使对手害怕，每损失{1}%体力，害怕概率提升{2}%"
// （args_num=3，切片 [base,step,up]；布局=[owner,target,base,step,up]）。
EffectResult effect_517_fear_scales_lost_hp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 517, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 517, args,
                     "517 布局=[owner,target,基础概率,步长,增量]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int base_chance = args.int_args[2];
    const int step = args.int_args[3];
    const int up = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     517, args, "需要 owner/opp ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(base_chance > 0 && step > 0, 517, args, "需要 基础概率>0 且 步长>0");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    // 生存门（2295 口径）：目标已倒 → 整条跳过（不申报、不耗 rand）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, key517(owner, 0), 0);
        set_storage_int_local(ctx, key517(owner, 1), 0);
        set_storage_int_local(ctx, key517(owner, 2), -1);
        return EffectResult::kOk;
    }
    // 动态概率（1365 直系公式）：已损百分比（对 max_hp），每 {1}% 一档 +{2}，clamp [0,100]。
    const int max_hp = max_hp_of(ctx, owner);
    const int lost_pct = max_hp > 0 ? (max_hp - ctx->getPet(owner).hp) * 100 / max_hp : 0;
    const int chance =
        std::max(0, std::min(100, base_chance + (lost_pct / step) * up));
    set_storage_int_local(ctx, key517(owner, 0), chance);
    set_storage_int_local(ctx, key517(owner, 1), lost_pct);
    // 申报制施加（害怕=6，模板明写异常码不占行参；原语内掷骰，>=100 不耗 rand）。
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, static_cast<int>(AbnormalStatusId::Fear), random_anomaly_duration(),
        owner, chance, ChanceSource::Skill);
    set_storage_int_local(ctx, key517(owner, 2), static_cast<int>(r));
    return EffectResult::kOk;
}

// ── 574 传承 tick：MOVE_RIGHT 武装（1275 直系；guaranteed_first 在 memset 之后写）──
EffectResult k427_574_move_right_tick(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 574, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 1, 574, args,
                     "574 tick 布局=[owner]");
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (storage_int_local(ctx, key574(owner, 0)) <= 0) {
        return EffectResult::kOk;   // 次数耗尽/未授予
    }
    if (!used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;   // 嗑药/切换回合：不武装也不消耗
    }
    ctx->ws.guaranteed_first[owner] = 1;      // 必先 tier1（"必定先手"）
    ctx->ws.must_crit[owner] = true;          // 必定暴击（must_crit 单列通道）
    ctx->ws.must_hit_grant[owner] = true;     // 必定命中（81 必中族 plain 凭证）
    set_storage_int_local(ctx, key574(owner, 1), ctx->roundCount);
    set_storage_int_local(ctx, key574(owner, 3), 1);
    return EffectResult::kOk;
}

// ── 574 传承 tick：AFTER_ACTION 消费（1275 同款行动方门 + 本轮武装门）─────────
EffectResult k427_574_after_action_tick(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 574, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 1, 574, args,
                     "574 tick 布局=[owner]");
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || k427_bucket_acting_side(ctx) != owner) {
        return EffectResult::kOk;
    }
    // ⚠️ 武装标记必须查 **presence**（diwang 原版 find 语义）——storage_int_local 的
    // 缺席默认 0 会把"未武装"误判成"roundCount=0 的 R1 已武装"（授予当轮即被消费，
    // 场景 T8110b R1 实证）。缺席 → 不消费。
    const auto armed = ctx->plugin_storage.find(key574(owner, 1));
    if (armed == ctx->plugin_storage.end()
        || any_int_or(armed->second) != ctx->roundCount) {
        return EffectResult::kOk;   // 本轮没武装（授予轮/嗑药轮）→ 不消费
    }
    set_storage_int_local(ctx, key574(owner, 1), -1);
    set_storage_int_local(ctx, key574(owner, 3), 0);
    if (!used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;   // 被控无法出手 → 不消耗
    }
    const int charges = storage_int_local(ctx, key574(owner, 0));
    if (charges > 0) {
        set_storage_int_local(ctx, key574(owner, 0), charges - 1);
        set_storage_int_local(ctx, key574(owner, 5),
                              storage_int_local(ctx, key574(owner, 5), 0) + 1);
    }
    return EffectResult::kOk;
}

// ── 574 接线（幂等）：传承 tick 重挂（挂到本侧当前活精灵名下，同源覆盖）────────
void k427_574_register_ticks(BattleContext* ctx, int owner) {
    {
        Effect e(574, 0, owner, /*duration=*/-1, EffectArgs(std::vector<int>{owner}),
                 &k427_574_move_right_tick);
        auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_MOVE_RIGHT,
                                                     owner, -1, ctx->roundCount);
        ce->source_id_ = 574;
        ctx->register_skill_effect(State::BATTLE_FIRST_MOVE_RIGHT, owner, std::move(ce));
    }
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(574, 0, owner, /*duration=*/-1, EffectArgs(std::vector<int>{owner}),
                 &k427_574_after_action_tick);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, -1, ctx->roundCount);
        ce->source_id_ = 574;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
}

// ═══════════════ 574 消耗自身全部体力 → 己方下次出手必先/必中/必爆 ═════════
// 官方模板："消耗自身全部体力，令己方下次使用的技能必定先手、必定命中，下次命中的
// 攻击技能必定打出致命一击"（args_num=0；布局=[owner,target] 引擎注入）。
EffectResult effect_574_consume_hp_next_voucher(BattleContext* ctx,
                                                const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 574, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 574, args,
                     "574 零参布局=[owner,target]（引擎注入双方）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 574, args, "需要 owner ∈ {0,1}");
    // ① 接线先行（59 教训：登场事件先于监听器存在会漏接）——幂等标记 + ENTER_STAGE
    //    重挂 watcher（TEAM，撑过死亡换宠——1275 传承同款）。
    if (storage_int_local(ctx, key574(owner, 2)) != 1) {
        set_storage_int_local(ctx, key574(owner, 2), 1);
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                if (c && ev.actor == owner) {
                    k427_574_register_ticks(c, owner);
                }
            },
            WatcherScope::TEAM);
    }
    k427_574_register_ticks(ctx, owner);   // 首挂：承接后续回合（当回合 armed 门天然不消费）
    // ② 凭证挂账（可累计：连用两次 = 2 次，字面"下次"逐次成立）。
    const int charges = storage_int_local(ctx, key574(owner, 0), 0);
    set_storage_int_local(ctx, key574(owner, 0), charges + 1);
    // ③ 自爆（59/753/1276 直系契约：hp=0 → defeat_pet(HP_CONSUME)，穿残留免死、
    //    actor=对手；已死（反弹/反伤）→ 自爆无从谈起，凭证保留）。
    const int slot = ctx->on_stage[owner];
    ElfPet& self = ctx->seerRobot[owner].elfPets[slot];
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    self.hp = 0;
    set_storage_int_local(ctx, key574(owner, 4), 1);
    if (g_core && g_core->defeat_pet) {
        (void)g_core->defeat_pet(ctx, owner, slot, 1 - owner, DefeatCause::HP_CONSUME);
    }
    return EffectResult::kOk;
}

// ── 1625 AFTER_ACTION 探针（j61_arm_gate 同构）：伤害>阈值 → roll → 全属性+{2} ──
EffectResult k427_1625_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = a.int_args[0];
    const int threshold = a.int_args[2];
    const int chance = a.int_args[3];
    const int bonus = a.int_args[4];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    set_storage_int_local(c, key1625(owner, 0), threshold);
    // "造成的伤害高于{0}"族 = 结算后 final（1256/1029/1223 同读法；== 不算）。
    const int final = c->resolvedDamage.final;
    set_storage_int_local(c, key1625(owner, 1), final);
    if (final <= threshold) {
        set_storage_int_local(c, key1625(owner, 2), -1);
        return EffectResult::kOk;   // 门闭：未掷、无增益
    }
    if (!roll_percent(chance)) {
        set_storage_int_local(c, key1625(owner, 2), 0);
        return EffectResult::kOk;   // 掷歪：无增益
    }
    set_storage_int_local(c, key1625(owner, 2), 1);
    all_stats_up(c, owner, bonus);   // 全属性+{2}（六槽含命中；增益不吃免弱）
    set_storage_int_local(c, key1625(owner, 3), bonus);
    return EffectResult::kOk;
}

// ═══════════════ 1625 造成的伤害高于{0}则{1}%令自身全属性+{2} ═══════════════
// 官方模板："造成的伤害高于{0}则{1}%令自身全属性+{2}"
// （args_num=3，切片 [threshold,chance,bonus]；布局=[owner,target,…]）。
EffectResult effect_1625_damage_high_stats_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1625, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1625, args,
                     "1625 布局=[owner,target,阈值,概率,增幅]");
    const int owner = args.int_args[0];
    const int chance = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1625, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 1625, args, "需要 概率 ∈ [0,100]");
    // 探针武装（j61_arm_gate 同构：FIRST/SECOND 双态、duration=1 判定归属本次用技；
    // robotId 键控只有行动方那条跑——resolvedDamage 读到的是自己的伤害）。
    const std::vector<int> payload(args.int_args, args.int_args + args.int_count);
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(1625, 0, owner, -1, EffectArgs(payload), &k427_1625_probe);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1625;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 2205 共享逻辑：门判 + restraint_view 写/复位（链节点与 carry tick 同函数）──
void k427_2205_apply(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    // ① 携带门：在场宠技能组含 2205（carryEffects 由 loadSkills 装槽——携带=loadout
    //    事实，效果体自判；换宠后非携带者出场自动失活）。
    bool carried = false;
    ElfPet& pet = ctx->getPet(owner);
    for (Skills& s : pet.skills) {
        for (const auto& [eid, fn] : s.carryEffects) {
            (void)fn;
            if (eid == 2205) {
                carried = true;
                break;
            }
        }
        if (carried) {
            break;
        }
    }
    set_storage_int_local(ctx, key2205(owner, 2), carried ? 1 : 0);
    if (!carried) {
        ctx->ws.restraint_view[owner] = -1.0;   // 复位（清陈旧写值）
        set_storage_int_local(ctx, key2205(owner, 0), 0);
        set_storage_int_local(ctx, key2205(owner, 1), 0);
        return;
    }
    // ② 异常门："自身处于异常状态"任一异常（2340 同款 0..43 扫描）。
    bool in_anomaly = false;
    if (g_core && g_core->has_active_anomaly) {
        for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
            if (g_core->has_active_anomaly(ctx, owner, id)) {
                in_anomaly = true;
                break;
            }
        }
    }
    set_storage_int_local(ctx, key2205(owner, 0), in_anomaly ? 1 : 0);
    if (!in_anomaly || !g_core || !g_core->restraint_multiplier) {
        ctx->ws.restraint_view[owner] = -1.0;   // 复位（公式回退按技能系别自然计算）
        set_storage_int_local(ctx, key2205(owner, 1), 0);
        return;
    }
    // ③ 门开：克制倍数 = max(自身→对手, 对手→自身)（宠元素回合视图，2500/2210 同源）。
    const int self_elem[2] = {ctx->ws.view_elementalAttributes[owner][0],
                              ctx->ws.view_elementalAttributes[owner][1]};
    const int opp_elem[2] = {ctx->ws.view_elementalAttributes[1 - owner][0],
                             ctx->ws.view_elementalAttributes[1 - owner][1]};
    const double m_self_to_opp = g_core->restraint_multiplier(self_elem, opp_elem);
    const double m_opp_to_self = g_core->restraint_multiplier(opp_elem, self_elem);
    const double mx = std::max(m_self_to_opp, m_opp_to_self);
    ctx->ws.restraint_view[owner] = mx;   // >=0 即被伤害公式覆盖采用
    set_storage_int_local(ctx, key2205(owner, 1),
                          static_cast<int>(mx * 1000.0 + 0.5));
}

// ── 2205 链节点（SKILL_EFFECT 注册——链上 clone_effect 的硬性需求，见文件头）────
EffectResult effect_2205_restraint_max_when_anomaly(BattleContext* ctx,
                                                    const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2205, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2205, args,
                     "2205 零参布局=[owner,target]（引擎注入双方）");
    k427_2205_apply(ctx, args.int_args[0]);
    return EffectResult::kOk;
}

// ── 2205 carry tick 载荷（武装时挂 FIRST/SECOND SKILL_EFFECT 双态 TEAM 常驻）────
EffectResult k427_2205_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    k427_2205_apply(ctx, args.int_args[0]);
    return EffectResult::kOk;
}

// ── 2205 carry 武装（init_battle 扫描；per-side 幂等）────────────────────────
void carry_2205_arm_restraint_tick(BattleContext* ctx, int owner, int skill_slot,
                                   int effect_id) {
    (void)skill_slot;
    (void)effect_id;
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    if (storage_int_local(ctx, key2205(owner, 3)) == 1) {
        return;   // 重入幂等（roster 多携带者/多技能槽防双挂）
    }
    set_storage_int_local(ctx, key2205(owner, 3), 1);
    for (const State st :
         {State::BATTLE_FIRST_SKILL_EFFECT, State::BATTLE_SECOND_SKILL_EFFECT}) {
        Effect e(2205, 0, owner, /*duration=*/-1, EffectArgs(std::vector<int>{owner}),
                 &k427_2205_tick);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, -1, ctx->roundCount);
        ce->source_id_ = 2205;
        ce->scope_ = EffectScope::TEAM;   // 活过换宠；携带门在场宠技能组自判
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(517, effect_517_fear_scales_lost_hp)
SKILL_EFFECT(574, effect_574_consume_hp_next_voucher)
SKILL_EFFECT(1625, effect_1625_damage_high_stats_up)
SKILL_EFFECT(2205, effect_2205_restraint_max_when_anomaly)
CARRY_EFFECT(2205, carry_2205_arm_restraint_tick)
