// K461 · 未受击降属 + 属性技减伤票两族 —— 1638×7 + 1645×7（2026-11-13，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k461，分支 feat/k461，基点 31fe17d7 含
// K455-K460 全部先例；场景 test/scenario/scenario_8480_k461.cpp，号段 8480-8489）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-13）══
//   1638（args_num=2）"{0}回合内若自身未受到攻击伤害则令对手全属性-{1}"
//   1645（args_num=3）"{0}回合内对手使用属性技能时自身下{1}次受到的攻击伤害减少{2}%"
//
// ══ 载体切片（unity 库 json_each + side_effect.arg_count 顺序切分，14 载体
//   全链零剩余；实现读行参不硬编码，场景 T8480-pre 以实际加载器 load_skill_effects
//   逐载体锁定）══
//   1638 [{0}=回合,{1}=级数]（{1} 全=1，{0}∈{2,3,4}）：
//     27429 雪径独行 [191(1),806(2),1638(2),843(2)]=[4|3,1|**3,1**|2,2]、
//     27648 遮天残翼 [191(1),1638(2),861(2)]=[4|**3,1**|4,2]、
//     27721 不朽地纹 [48(1),1011(2),1638(2)]=[4|3,50|**3,1**]、
//     28342 星辰假日 [191(1),741(2),1638(2)]=[4|3,250|**3,1**]、
//     28651 邪恶信仰 [48(1),850(2),1638(2)]=[4|4,50|**4,1**]（唯一 {0}=4）、
//     36770 销身汲骸术 [2058(0),1638(2),433(7)]=[]|**3,1**|3,0,0,1,0,1,0]、
//     38612 赤采 [789(1),1638(2)]=[1|**2,1**]（唯一 {0}=2；攻击技载体）。
//   1645 [{0}=回合,{1}=次数,{2}=%]（{1} 全=1、{2} 全=50，{0}∈{3,4}——{1}/{2} 无
//     真实载体差分，多值维度由 EffectParamOverride 变档腿承担，K453 2456/K458 35245
//     同款装配期官方覆盖基建）：
//     27453 命轮无常 [1440(1),693(2),1644(3),1645(3)]=[2|2,100|4,2,1|**4,1,50**]、
//     27606 幽冥火种 [191(1),1035(5),1645(3)]=[4|70,22,2,100,2|**3,1,50**]、
//     27628 天灾初现 [191(1),1372(3),1645(3)]=[4|4,100,28|**4,1,50**]、
//     27639 钢魂肃杀 [585(6),877(1),716(2),1645(3)]=[1×6|1|3,1|**3,1,50**]、
//     27854 林语虫鸣 [48(1),1585(2),1645(3)]=[4|3,2|**3,1,50**]、
//     27941 神权忤逆 [191(1),1231(3),1165(2),1645(3)]=[4|100,6,3|4,2|**4,1,50**]、
//     37098 盛宴洗礼 [1645(3),820(2)]=[**3,1,50**|1,2]（纯链 cat2 攻击技载体）。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例）══
//   1638 窗口：register_round_effect @ BATTLE_ROUND_END、InRounds（"{0}回合内"
//     含当回合；先手生效 R..R+{0}-1、后手顺延 R+1..R+{0}——起点折算由 helper 内生
//     round_effect_start_round；tick 侧以 start_round 实时门配对跳过施放回合伪进，
//     K458 1565 同款"有效性由效果自己的条件在回调里实时判"sanctioned 模式）。
//   1638 门："自身未受到攻击伤害"按**回合结算口径**——ROUND_END tick 时判本回合
//     是否被红伤命中（门在位判读）。检测面 = REDUCE_PCT·MITIGATE 管线条目
//     （K456 1369 回退同槽同类别；MITIGATE 不可被"挡伤失效"抑制，检测不被吞）：
//     d.defenderId==self && d.isRed && d.final>0 时记 hit 回合戳（同步写，无事件
//     drain 时序问题）。final>0 = 受到的是**落地伤害**（被 BLOCK 归零的攻击不算
//     "受到攻击伤害"；粉伤不进红管线、属性直伤只跑 BLOCK|CAP 子集，两类天然不触
//     检测——K456 减伤同面）。戳存 回合+1（1 基，0=未受击哨兵——引擎回合记账 0 基，
//     直接存 rc 会与首回合相撞假闭门，实测踩坑记档）→ tick 判 戳≠当前回合+1 = 门开。
//   1638 降属 = **boost 写面真写**：g_core->stat_drop 六槽逐槽（0..5 含命中，
//     "全属性"六项口径 common.h 2026-09-18）、actor=属主（查免弱 STAT_DROP 与
//     ATTACH_BAN、-6 钳制）——与 K459 1804 的读面钳制相反：write_ability_level
//     唯一落点、本体+视图同步、emit EVENT_STAT_CHANGED（场景做三面差分：本体
//     变化/事件计数/跨回合持久）。逐槽结果 SUCCESS/AT_FLOOR/IMMUNE 记审计掩码。
//   1645 触发面 = **EVENT_SKILL_USED watcher**（469/718 直系同字面"对手使用属性
//     技能"——Skills::execute 起点 emit、used_attribute 载荷、miss/被封也算"使用"，
//     E13 锚点口径；ev.actor==opp 过滤）。watcher register_round 显式取
//     round_effect_start_round(self,{0},InRounds) 折算（§五.0 后手顺延：后手窗
//     [R+1,R+{0}]、先手 [R,R+{0}-1]；718 未折算是其记档捷径，本单按官方通用规则
//     折算）——duration={0} 由 event_center 按注册回合自然过期，回调内再以
//     [窗起点,窗终点] 回显双保险（watcher 晚摘一格不误触）。scope=ON_STAGE +
//     watcher_valid_id 实参（K459 场景记档：默认 0 与初值 1 不匹配会被首cleanup摘）。
//   1645 票 = plugin_storage counts 凭证（K459 1804 同构；**重触发刷新不叠加**：
//     同窗再触发余票重置为 {1}，K456/K459 覆盖键记档同口径，官方无叠加实测）。
//     票生命 = 授予后无回合界（"下{1}次"字面，窗过期不再触发但已授票照常消费）、
//     ON_STAGE 换宠 epoch 作废（管线条目 run() 现场 valid_id 跳过 + 重施放重接线）。
//   1645 消费 = register_damage_effect(REDUCE_PCT, self, MITIGATE)（K456 1369 回退
//     同通道）：d.defenderId==self && d.isRed && d.final>0 时 cnt-- 并
//     d.final = d.final*(100-{2})/100。打盔管线整段不跑 → 天然不消费（811 记档
//     同口径）；"免疫下N次攻击伤害"DAMAGE 票在 BLOCK 相归零——本消费在 REDUCE_PCT
//     相先于 BLOCK，免疫趟照消费（官方无实测，按管线序记档）；MITIGATE 不被
//     蚀砚之泪"挡伤失效"抑制（"减伤不能被无视"官方口径）。接线 per epoch
//     wired 旗（pipeline 无同源去重——不接线会重施放叠条目，双条目对 cnt=2 的票
//     同刀双消费，见下"变异"节 M8 面）。
//   ⚠️ K433 记档甄别：k433_tiandi_gongchuang_pool.cpp:38 "1645/1855 双窗复合语义
//     待拆"是候选复杂度记档非退役——本单即"拆"：触发窗（watcher duration）与
//     消费票（无回合界 counts）分属两块状态，各自独立生命周期。
//
// ══ 动工前置三查（2026-11-13，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep -w 1638/1645（src/ include/ 全量）零命中；resources 命中逐处甄别：
//      soul_lib/k_high_dmg_convert.cpp kMark1638 = 魂印空间 1638（K404 幻境界神，
//      SOUL_PROGRAM——双空间两套语义，K458 1565/K404 自记"effect_info 技能空间异文
//      模板=未受击降属"即本单）；moves_lib k433/k434/k437/k442/k444/k447 的 1645
//      命中全为候选遗留注释记档；boss_invalid_effects.json 含两 id = 官方 boss 无效
//      名单（loader boss_invalid 标签消费，K456/K459 同判非实现豁免）。
//   ② effect_info 两 id 在列 args_num=2/3、side_effect arg_count 2/3 一致、14 载体
//      顺序切分零剩余（上表）；custom_effect_overrides 无两 id window 声明
//      （→ InRounds 默认家族口径）。
//   ③ registry 基线 moves=1550/soul=415：moves 无两 id（soul 1638=K404 魂印空间
//      同号不同义）；HUB 全 worktree（agents/endgame-walk/k462/k463/k464）grep
//      SKILL_EFFECT(1638/1645)/SOUL_PROGRAM(1638/1645) 零命中——真缺口。
//   记档壁垒甄别：tools/db_patches 无两 id（全库仅 1526_split_900001.sql，异 id）；
//      无 900000 拆分迁移与故意退役记录。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456 键式）══
//   ⚠️ 1638 段**双空间拆分**：soul 空间 1638（K404 幻境界神）已占
//     1016380000+owner*16+槽（运行时区间 ...0000-...0031，soul_lib/
//     k_high_dmg_convert.cpp:118 + 场景 7613 同键段）→ 本单 moves 空间 1638 用
//     **1016380100** 子块（...0100-...0125）：与 soul 运行时区间不相交、不越
//     1016390000 段界、两文件 constexpr 基址不同 → check_storage_keys.py 按
//     （容器,基础键）各自独立不报撞（K458 1565 双空间同款）。
//   1645 段 1016450000 全库 grep 零占用（2026-11-13）。
//   1638（基 1016380100）：slot0=末 tick 门判（1=开→已执行降属/0=闭）、
//     1=末次降属回合、2={0}回显、3={1}回显、4=累计 tick 计数、5=累计降属执行数、
//     6=累计逐槽 stat_drop 数（6×执行）、7=受击戳（回合+1，1 基；0=未受击——检测 lambda 写/tick 读）、
//     8=累计 cast 计数、9=跳过原因（0=无/1=缺 stat_drop 原语）、10=窗起点回显、
//     11=检测条目接线 epoch（==pipeline_valid_id[self]）、12=末次降属 SUCCESS 掩码、
//     13=末次降属 IMMUNE 掩码、14=末次降属 AT_FLOOR 掩码
//   1645（基 1016450000）：slot0=余票、1=消费条目接线 epoch、2={0}回显、3={1}回显、
//     4={2}%回显（活跃票）、5=窗起点、6=窗终点（含）、7=累计触发数、8=累计 cast 数、
//     9=跳过原因（0=无/1=缺事件中心）、10=累计消费数、11=末次消费回合、
//     12=末次触发回合、13=末次减伤执行回合、14=watcher 句柄（重施放摘旧挂新）
//   布局自防撞：单基址 per effect + owner×16 步长 > 槽位上限 14 → 同基址内 owner
//   间无交叠；跨文件撞键由 tools/check_storage_keys.py 门禁静态扫。
//
// ══ 变异测试实测（2026-11-13，本 worktree；权威轮 10/10 真红 rc=134 → 字节级
//   还原 → SHA==底本 → deploy 复绿 rc=0；证据 build/k461-mutations/M*/：apply.log/
//   grep-verify/build.log/dylib-sha-mutated/run.rc+run.log/first-kill/
//   restore-build.log/dylib-sha-restored/rerun.rc+rerun.log 全链；底本 SHA 绑定
//   本定稿源码、记于 build/k461-mutations/baseline.sha（同源重编同 SHA 实验证实，
//   K459 同口径），变异 dylib SHA 均≠底本、还原后逐字回到底本。历史轮
//   build/k461-mutations-r1/（底本 3d0b9c7f…，头注修正前源码；变异集合与首杀
//   特征一致）。══
//   M1 1638 门反转（受击判 d.isRed→判非红伤）→ 首杀 T8480.a 受击戳=0
//     （红伤不再记戳——门数据面；受击回合照降在其后连续可观测）；
//   M2 1638 写面置换（stat_drop 真写→view_levels 读面钳制，本体不动零 emit）→
//     首杀 T8480.a 对手六槽本体 -1（真写差分面）；
//   M3 1638 窗起算（tick start 实时门删除）→ 首杀 T8480.f side1 的对手 R1 未降
//     （后手施放回合伪进降属——顺延面）；
//   M4 1638 窗时长 {0}→{0}+1 → 首杀 T8480.a R3 tick 过期（计数 3≠2，多跳）；
//   M5 1645 触发面置换（EVENT_SKILL_USED→EVENT_HIT 且删 used_attribute 门）→
//     首杀 T8480.i 票尽全伤带（R2 拍打误触发→消费错票，减伤带漂移）；
//   M6 1645 票刷新→叠加（cnt += {1}）→ 首杀 T8480.l2 重触发刷新为 2（实得 3）；
//   M7 1645 消费不扣票（cnt-- 删除——永续减伤）→ 首杀 T8480.i 余票=0（实得 1）；
//   M8 1645 接线 per-epoch 旗删除（重施放叠 MITIGATE 条目）→ 首杀 T8480.l2 R4
//     恰减一次带（双条目同刀各消费一次 → 双倍减落 8）；
//   M9 1645 {2}% 行参硬编码 50（变档腿 {2}=0 应不减）→ 首杀 T8480.l3 减 0%=基线带；
//   M10 键位步长（owner×16→owner×8 同基址交叠）→ 首杀 T8480.f side1 cast=1
//     （步长改后插件写键与镜像审计读面失配 + side1 键段撞 side0 槽 8..14 串账）。
//   概率面 N/A（两族无概率行参无掷点通道——K456 同口径记档）；once 面 N/A
//   （无 once 语义；生命周期面由 M4 窗时长 + 场景 g/o 离场清理覆盖）；回退分支
//   面 N/A（无「未触发则…」子句）。

#include "common.h"

#include <effects/damage_pipeline.h>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1638AuditBase = 1016380100;   // moves 空间 1638 子块（soul 空间 K404 占 1016380000）
constexpr int k1645AuditBase = 1016450000;   // 10^9 + 1645×10^4

inline int audit_key_of(int base, int owner, int slot) { return base + owner * 16 + slot; }

int storage_int_local(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    return any_int_or(it->second, def);
}

void set_storage_int_local(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// ══════════════ 1638 {0}回合内若自身未受到攻击伤害则令对手全属性-{1} ══════════════

// 受击检测条目：本回合被红伤落地命中 → 记 hit 回合戳=回合+1（tick 判 戳==回合+1 = 门闭）。
// REDUCE_PCT·MITIGATE 槽（K456 回退同槽；MITIGATE 不可被"挡伤失效"抑制——检测不被吞；
// 属性直伤只跑 BLOCK|CAP 子集、粉伤不进红管线 → 两类天然不误记）。
void k461_1638_wire_hit_probe(BattleContext* ctx, int self) {
    const int wired_key = audit_key_of(k1638AuditBase, self, 11);
    if (storage_int_local(ctx, wired_key) == ctx->pipeline_valid_id[self]) {
        return;   // 本 epoch 已接线（重施放不叠条目——管线条目无同源去重）
    }
    const int hit_key = audit_key_of(k1638AuditBase, self, 7);
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, self, DamageEffectCategory::MITIGATE,
        [self, hit_key](BattleContext* c, int) {
            if (!c) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;   // 只记"自己受到的落地攻击伤害"（红伤；零伤/粉伤/他人不碰）
            }
            // 戳存 回合+1（1 基）：0 = 未受击哨兵——引擎回合记账 0 基（R1=rc0），
            // 直接存 rc 会让「未受击」缺省值与首回合相撞假闭门（实测踩坑记档）。
            set_storage_int_local(c, hit_key, c->roundCount + 1);
        });
    set_storage_int_local(ctx, wired_key, ctx->pipeline_valid_id[self]);
}

// ROUND_END tick：start 实时门（后手施放回合伪进跳过，K458 1565 同款）→ 门判 →
// 门开则六槽 stat_drop 真写（actor=属主：查免弱/附加禁令、-6 钳制）。
// 行参 [self, opp, drop, start_round]（cast 时打包，无 owner/target 前缀）。
EffectResult k461_1638_round_end_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int drop = args.int_args[2];
    const int start_round = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp) {
        return EffectResult::kOk;
    }
    // 窗口起点实时门：桶执行面只有 isExpired 门无 start 下界门（K458 缺口记档）——
    // 后手的施放回合 ROUND_END 会伪进回调，此处按折算起点跳过（施放回合已"错过结算"）。
    if (ctx->roundCount < start_round) {
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 4),
                          storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 4)) + 1);
    // 门判：本回合未受落地红伤（戳≠当前回合）→ 门开。
    const int hit_stamp = storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 7));
    const bool gate_open = (hit_stamp != ctx->roundCount + 1);
    set_storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 0), gate_open ? 1 : 0);
    if (!gate_open) {
        return EffectResult::kOk;   // 受击回合门闭：本回合不降（负测面）
    }
    if (drop <= 0) {
        return EffectResult::kOk;   // "全属性-0" 自然 no-op（不执行不审计降属面）
    }
    if (!g_core || !g_core->stat_drop) {
        set_storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    // 写面真写：六槽逐槽弱化原语（本体+视图同步、emit EVENT_STAT_CHANGED——
    // 与 K459 1804 读面钳制的三面差分由场景锁定）。逐槽结果记审计掩码。
    int ok_mask = 0, immune_mask = 0, floor_mask = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const StatDropResult r = g_core->stat_drop(ctx, opp, i, drop, /*actor=*/self);
        switch (r) {
            case StatDropResult::SUCCESS: ok_mask |= (1 << i); break;
            case StatDropResult::IMMUNE: immune_mask |= (1 << i); break;
            case StatDropResult::AT_FLOOR: floor_mask |= (1 << i); break;
            case StatDropResult::INVALID_PARAM: break;
        }
    }
    set_storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 12), ok_mask);
    set_storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 13), immune_mask);
    set_storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 14), floor_mask);
    set_storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 5),
                          storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 5)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 6),
                          storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 6))
                              + (ok_mask != 0 ? 6 : 0));
    set_storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 1), ctx->roundCount);
    return EffectResult::kOk;
}

// 1638 主体。int_args = [owner, target, {0}=回合, {1}=级数]（args_num=2 + 前缀）。
EffectResult effect_1638_unhit_all_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1638, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1638, args,
                     "1638 布局=[owner,target,回合,级数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int drop = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1638,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && drop >= 0, 1638, args, "需要 回合>0、级数>=0");
    set_storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 2), rounds);
    set_storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 3), drop);
    set_storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 8)) + 1);
    // 受击检测接线（per epoch；重施放不叠条目）。
    k461_1638_wire_hit_probe(ctx, self);
    // 窗口起点折算（先算好随 payload 下发：tick 的 start 实时门读它——
    // InRounds 家族口径，先手 R、后手顺延 R+1；重施放 = 同 (source_id,effect_id)
    // 键覆盖旧锚（timed_bucket 同源去重），窗口刷新不叠加）。
    const EffectWindowKind wk = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(/*effect_id=*/1638)
        : EffectWindowKind::InRounds;
    const int start_round = ctx->round_effect_start_round(self, rounds, wk);
    set_storage_int_local(ctx, audit_key_of(k1638AuditBase, self, 10), start_round);
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, /*effect_id=*/1638, rounds,
                          {self, opp, drop, start_round}, &k461_1638_round_end_tick);
    return EffectResult::kOk;
}

// ══════════════ 1645 {0}回合内对手使用属性技能时自身下{1}次受到的攻击伤害减少{2}% ══════════════

// 减伤消费条目：cnt>0 且自己受到落地红伤 → 消费一票、final 按 {2}% 削减。
// per-epoch 接线（重施放不叠条目——双条目会对同一刀各消费一次 = 双倍减，M8 杀面）。
void k461_1645_wire_reduce(BattleContext* ctx, int self) {
    const int wired_key = audit_key_of(k1645AuditBase, self, 1);
    if (storage_int_local(ctx, wired_key) == ctx->pipeline_valid_id[self]) {
        return;
    }
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, self, DamageEffectCategory::MITIGATE,
        [self](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;   // 只减自己受到的落地红伤（攻击伤害）
            }
            const int cnt_key = audit_key_of(k1645AuditBase, self, 0);
            const int remain = storage_int_local(c, cnt_key);
            if (remain <= 0) {
                return;   // 无票（凭证耗尽的常态路径；打盔趟管线整段不跑天然不到这）
            }
            set_storage_int_local(c, cnt_key, remain - 1);
            set_storage_int_local(c, audit_key_of(k1645AuditBase, self, 10),
                                  storage_int_local(c, audit_key_of(k1645AuditBase, self, 10)) + 1);
            set_storage_int_local(c, audit_key_of(k1645AuditBase, self, 11), c->roundCount);
            const int pct = storage_int_local(c, audit_key_of(k1645AuditBase, self, 4));
            if (pct > 0) {
                d.final = d.final * (100 - pct) / 100;   // K456 回退同式（pct∈[0,100] 守卫）
            }
            set_storage_int_local(c, audit_key_of(k1645AuditBase, self, 13), c->roundCount);
        });
    set_storage_int_local(ctx, wired_key, ctx->pipeline_valid_id[self]);
}

// 1645 主体。int_args = [owner, target, {0}=回合, {1}=次数, {2}=%]（args_num=3 + 前缀）。
EffectResult effect_1645_attr_use_damage_reduce_ticket(BattleContext* ctx,
                                                       const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1645, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1645, args,
                     "1645 布局=[owner,target,回合,次数,%]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int counts = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1645,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && counts >= 0 && pct >= 0 && pct <= 100, 1645, args,
                     "需要 回合>0、次数>=0、%∈[0,100]");
    set_storage_int_local(ctx, audit_key_of(k1645AuditBase, self, 2), rounds);
    set_storage_int_local(ctx, audit_key_of(k1645AuditBase, self, 3), counts);
    set_storage_int_local(ctx, audit_key_of(k1645AuditBase, self, 4), pct);
    set_storage_int_local(ctx, audit_key_of(k1645AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k1645AuditBase, self, 8)) + 1);
    if (counts <= 0) {
        return EffectResult::kOk;   // "下0次"=自然 no-op（不武装不报故障）
    }
    // 窗口起点折算（§五.0：先手 [R,R+{0}-1]、后手顺延 [R+1,R+{0}]；家族声明查询
    // InRounds 默认——custom_effect_overrides 无 1645 声明，三查②）。
    const EffectWindowKind wk = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(/*effect_id=*/1645)
        : EffectWindowKind::InRounds;
    const int start_round = ctx->round_effect_start_round(self, rounds, wk);
    set_storage_int_local(ctx, audit_key_of(k1645AuditBase, self, 5), start_round);
    set_storage_int_local(ctx, audit_key_of(k1645AuditBase, self, 6), start_round + rounds - 1);
    // 减伤消费接线（per epoch；重施放不叠条目）。
    k461_1645_wire_reduce(ctx, self);
    // 触发 watcher：EVENT_SKILL_USED（469/718 同字面"对手使用属性技能"——使用即触发，
    // miss/被封也算；target=opponent(user) 恒为己方，双过滤防串）。register_round 显式
    // 取折算起点（duration={0} → event_center 过期门 current-register>=duration）；
    // ON_STAGE + 实参 epoch（默认 0 会被首 cleanup 摘除，K459 场景记档）。重施放摘旧
    // 挂新（register_watcher 无同源去重，旧窗新参不撤会双触发）。
    const int watcher_handle_key = audit_key_of(k1645AuditBase, self, 14);
    const int old_watcher = storage_int_local(ctx, watcher_handle_key, -1);
    if (old_watcher >= 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }
    const int handle = ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, /*owner=*/self, /*register_round=*/start_round,
        /*duration_rounds=*/rounds, /*once=*/false,
        [self, opp](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != opp || ev.target != self || !ev.used_attribute) {
                return;   // 条件：对手使用的是属性技（使用即触发，E13 字面）
            }
            // 窗口双保险（watcher 过期摘除由 event_center 管，此处防晚摘一格）：
            const int win_lo = storage_int_local(c, audit_key_of(k1645AuditBase, self, 5));
            const int win_hi = storage_int_local(c, audit_key_of(k1645AuditBase, self, 6));
            if (c->roundCount < win_lo || c->roundCount > win_hi) {
                return;
            }
            // 授票 = 刷新不叠加（K456/K459 覆盖键记档同口径；同窗再触发重置为 {1}）。
            const int counts = storage_int_local(c, audit_key_of(k1645AuditBase, self, 3));
            set_storage_int_local(c, audit_key_of(k1645AuditBase, self, 0), counts);
            set_storage_int_local(c, audit_key_of(k1645AuditBase, self, 12), c->roundCount);
            set_storage_int_local(c, audit_key_of(k1645AuditBase, self, 7),
                                  storage_int_local(c, audit_key_of(k1645AuditBase, self, 7)) + 1);
        },
        WatcherScope::ON_STAGE, ctx->watcher_valid_id[self]);
    set_storage_int_local(ctx, watcher_handle_key, handle);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(1638, effect_1638_unhit_all_drop)
SKILL_EFFECT(1645, effect_1645_attr_use_damage_reduce_ticket)
