// K467 · 恢复抑制族 —— 2128×6（2026-11-13，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k467，分支 feat/k467，基点 31fe17d7；
// 场景 test/scenario/scenario_8540_k467.cpp，号段 8540-8549）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查
//   2026-11-13）══
//   2128（args_num=2）"对手不处于异常状态时对手{0}回合内体力恢复效果减少{1}%"
//
// ══ 载体切片（unity 库 SQL 现查 + side_effect_arg 按 effect_info.args_num 顺序
//   切分，6 载体全链零剩余；场景 T8540-pre 以实际加载器 load_skill_effects 逐载体
//   锁定；实现读行参不硬编码）══
//   2128 [{0}=回合,{1}=%]（6 载体，{0} 全=2、{1}∈{30,50,60,100}）：
//     37432 日月轮·同天   [847(1),1342(1),697(0),2128(2)]=[200,200|∅|**2,50**]、
//     37548 十月的唱片     [1002(1),2128(2),1366(3)]=[1|**2,30**|25,25,100]、
//     37612 烈焰·高能射线 [1878(1),2073(1),2128(2)]=[3|50|**2,50**]、
//     37820 刺针瘴舞       [1542(2),1589(1),2128(2)]=[1,1|2|**2,50**]、
//     38187 兰烬蘧栩       [948(1),188(0),2128(2),1832(4)]=[22|∅|**2,100**|2,100,300,2]、
//     38295 炼·死镜冥啼   [2423(0),1978(3),1047(3),1360(1),2128(2)]
//                        =|∅|[100,0,3]|[60,7,3]|100|**2,60**。
//     ⚠️ 38295 链上 1047 带 60% 掷点、1978 带消强分支——行为腿避开该载体（切片
//     核验承担）；{1} 差分腿由 37432(50)/37548(30)/38187(100) 三真实载体承担。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例）══
//   · **降疗通道 = ctx->heal_mod_pct[opp]**（heal 减益面既有原语通道：heal_impl
//     battle_primitives.cpp:1498 读、heal_amount/heal 全系恢复必经；直写 hp 禁）。
//     直系先例 = 2241/761（j_purge_success2.cpp grant_heal_reduce_window：授予即
//     生效 + ROUND_START 捕获-还原 tick，注册 rounds+1 格含还原 tick；2116/
//     d5_D28/j_first_anomaly_reverse_healreduce 同款）。2128 = 2241 + 异常门。
//   · **条件门 = 对手无任一异常**（"对手不处于异常状态时"）：
//     - 判据 = g_core->has_active_anomaly 扫官方全集 0..kOfficialAbnormalStatusMaxId
//       （f_drain_family 1214 同判据——任一异常在位即"处于异常状态"；自动吃
//       "视为不处于异常"虚拟视图 K6 口径）；
//     - **在位判读**（cast 时点，K458 1494 满体力门同款：无闩锁）——门闭（对手有
//       异常）→ 不写降疗、**不武装窗**（slot8=0 负测面）；
//     - **窗内每回合 ROUND_START tick 重判**（"…时"= while 前件，持续语义）：
//       对手获异常 → 该 tick 写回 orig（降疗关闭，"对手获异常后门闭"负测面）；
//       异常解除 → 下一 tick 重开（K458 "回满再触发"同款可逆门）。
//     - ⚠️ 结构粒度记档：heal_impl 在**每次恢复时**读 heal_mod_pct，无"逐恢复"
//       钩子 → 门的重判粒度 = ROUND_START tick（回合首快照），回合中途获/解异常
//       的影响从下个回合首生效——heal 减益面无 K289 式逐段管线可挂（管线只管
//       伤害），官方无逐恢复粒度实测；这是通道结构上限，非实现选择。
//   · **窗口 = register_round_effect(ROUND_START, rounds+1) 捕获-还原**（2241 同构，
//     InRounds：ROUND_START 施放当回合已过 → 首个 tick 在 R+1；末 tick（R+rounds）
//     写回 orig 即"窗满还原"；占位 rounds+1 是为装下还原 tick）。降疗覆盖
//     [cast, R+rounds-1]（先手）；后手施放 = 起点 fold R+1（round_effect_start_round
//     内生，§五.0），降疗仍自 cast 即生效（2241 "授予即生效"同款相位，官方无
//     后手相位实测——沿先例记档）。重施放 = 同 (source_id=2128) 桶键覆盖
//     （timed_bucket 同源去重）→ 窗刷新不叠加：**基线复用**（slot5 存 orig 基线、
//     slot8 为 live 标志——live 中重 cast 捕 slot5 基线而非已降值，防 pct 叠加
//     与还原值链式泄漏；还原 tick 清 live）。
//   · 生命周期：回合类条目（isRoundEffect）→ 断回合可清 + ON_STAGE 换宠 epoch
//     作废；⚠️ 施放方换宠 → 还原 tick 不再跑，heal_mod_pct[opp] 停留在末次写入值
//     （2241 同款既有语义；对手换宠/结算面清零见 battleContext.h:1147
//     "恢复效果修正不继承"）——场景 T8540.e 按可观测面锁定。
//
// ══ 动工前置三查（2026-11-13，k467 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep 2128（src/ include/ 全量）零命中——无路由无硬编码，SKILL_EFFECT
//     default 桶（skills.cpp:182-183 未列 id 全落 SKILL_EFFECT）；
//   ② effect_info 2128 行即模板（args_num=2，param=[]）；effect_des 两 id 无行；
//     载体 6 条顺序切分零剩余；boss_invalid_effects.json:1082 含 2128 = **soul
//     空间**尾注名单（namespace=soul，来源 effect_icon.tips"（BOSS无效）"，宠物
//     3491 魂印行 1712——同号串空间，K453 2456/K456 1369 判例，moves 载体无
//     boss 无效语义，不冲突）；
//   ③ registry moves=1556/soul=415（build/baseline-gate.log 基线）无两 id；本库
//     resources/test grep SKILL_EFFECT(2128)/SOUL_PROGRAM(2128)/kMark2128 零命中；
//     全部并行 worktree（k466/k468/agents/endgame-walk/web-replay）同 grep 零命中
//     （撞号=换号重发纪律）；soul 空间同号甄别：effect_icon 3491 行（effect_id=
//     2128，tips 为魂印语义"无效攻击+减伤+睡眠"）**未注册**（registry soul 无
//     2128）→ 双空间共存无运行时键段冲突，moves 侧占 1021280000 基址（若将来
//     soul 空间实现 2128，按 K458 先例取 1021280100 子块）；custom_effect_overrides
//     两 id 零行（InRounds 默认族）；tasklog 8540 段本单 alloc、无 900000 拆分迁移
//     与故意退役记录——真缺口。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456/K458 键式；
//   1021280 段全库 grep 零占用 2026-11-13，check_storage_keys.py 门禁兜底）══
//   2128 基 1021280000：slot0=末次门读数（1=对手无异常/0=有异常；cast 与 tick 共用）、
//     1=末次 heal_mod_pct 写入种类（1=降疗 orig-pct/0=门闭写回 orig/2=窗满还原 orig）、
//     2=末次门判定回合（roundCount）、3={1}% 行参回显、4={0} 回合行参回显、
//     5=活动窗基线 orig（cast 捕获；live 中重 cast 复用此基线）、
//     6=累计 tick 到达计数（含还原 tick——窗过期/once 变异面）、
//     7=累计 cast 计数（含门闭 cast）、8=窗 live 标志（1=live/0=无窗；cast 门闭
//     写 0——门闭不武装负测面；还原 tick 清 0）、
//     9=跳过原因（0=无/1=缺 has_active_anomaly——守卫级，report_effect_arg_fault
//     不占此槽）
//   布局自防撞：单基址 per effect + owner×16 步长 > 槽位上限 9 → 同基址 owner 间
//   无交叠；与 K455(1017860)/K456(1013690/1011960)/K457(1014590/1017990)/
//   K458(1015650100/1014940)/K459(1020750/1018040)/K460/K461/K462(1015750/1018550)/
//   K463(1022260/1020270) 已落键段零交叠；姊妹文件 2067 基=1020670000 不同段。
//
// ══ 变异测试实测（2026-11-13/17，本 worktree；证据 build/k467-mutations/M*/，
//   每份含 semantic.diff+build.log+run.rc+first-kill+dylib-sha-mutated/restored+
//   rerun.rc；还原后 dylib SHA==底本 2973a0cb… 再强编 deploy 复绿 rerun rc=0/
//   SBS_SCENARIO_CHECKS=238；M1-M8 覆盖 门×2/通道/键位/生命周期/概率/once/回退
//   七类，全部真红一杀）══
//   M1 门·cast 判读删除（opp_clean=true 恒开）→ T8540.c R1 门闭不降疗红
//     （2026-11-13 前执行者轮，证据链完整）；
//   M2 门·tick 重判删除（窗内获异常不闭门）→ T8540.d R2 获异常后门闭红
//     （2026-11-13 前执行者轮，首轮还原踩同秒 mtime 假绿坑 loop.log RESTORE-SHA-
//     MISMATCH，loop2 sleep 重试后链完整）；
//   M3 通道·降疗写错边（heal_mod_pct[self]≠opp）→ T8540.a R1 降疗 50% 红
//     （2026-11-13 变异构建踩同秒 mtime 坑 dylib SHA 未变即假底本，loop2 记
//     SHA-UNCHANGED 后额度中断；2026-11-17 续作轮 touch 强编补齐：真红 rc=134
//     → 还原 src/dylib SHA==底本 → rerun 238 绿）；
//   M4 键位·owner 步长归零（audit_key_of owner*16→*0 两边撞键）→ T8540.f R1
//     回补触发 s2067(1,6)=-1 红（2026-11-17）；
//   M5 生命周期·还原 tick 不注册（register rounds+1→rounds）→ T8540.a R3 窗满
//     还原红（降疗永久化）（2026-11-17）；
//   M6 概率·tick 申报面归零（dodge_rate=0 重写）→ T8540.g 70% 档统计带红
//     （实得 miss=1 vs ≥54）（2026-11-17）；
//   M7 once·回补窗末单发删除（门 <start）→ T8540.g 统计带红（逐回合回补 +3
//     污染 miss 记账，实得 0）（2026-11-17）；
//   M8 回退分支·pp_restore only_empty 翻转（全槽→只补空槽）→ T8540.f PP 实变
//     槽 5→1 红（2026-11-17）。

#include "common.h"

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k2128AuditBase = 1021280000;   // 10^9 + 2128×10^4

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

// 对手处于任一异常？（官方异常码 0..kOfficialAbnormalStatusMaxId 全枚举，
// f_drain_family 1214 同判据；has_active_anomaly 自动吃"视为不处于异常"视图）。
bool k467_has_any_anomaly(BattleContext* ctx, int target) {
    if (!g_core || !g_core->has_active_anomaly) {
        return false;   // 缺原语 = 门不可判（cast/tick 侧各自记 slot9 跳过）
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, target, id)) {
            return true;
        }
    }
    return false;
}

// ── 2128 的 ROUND_START tick —— payload={owner, opp, rounds, pct, orig, arm}。
// 窗内逐回合重判门（"…时"= while 前件）：无异常 → 维持降疗；有异常 → 写回 orig
// （门闭）。窗满 tick（roundCount > arm+rounds-1）→ 写回 orig（还原，2241 同构）。
EffectResult k467_2128_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    const int orig = args.int_args[4];
    const int arm = args.int_args[5];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || owner == opp) {
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key_of(k2128AuditBase, owner, 6),
                          storage_int_local(ctx, audit_key_of(k2128AuditBase, owner, 6)) + 1);
    if (ctx->roundCount > arm + rounds - 1) {
        // 窗满还原 tick（注册面 rounds+1 的末格）：写回捕获基线、清 live。
        ctx->heal_mod_pct[opp] = orig;
        set_storage_int_local(ctx, audit_key_of(k2128AuditBase, owner, 1), 2);
        set_storage_int_local(ctx, audit_key_of(k2128AuditBase, owner, 2), ctx->roundCount);
        set_storage_int_local(ctx, audit_key_of(k2128AuditBase, owner, 8), 0);
        return EffectResult::kOk;
    }
    // 窗内：门实时重判（ROUND_START 粒度，见文件头结构粒度记档）。
    const bool opp_clean = !k467_has_any_anomaly(ctx, opp);
    set_storage_int_local(ctx, audit_key_of(k2128AuditBase, owner, 0), opp_clean ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k2128AuditBase, owner, 2), ctx->roundCount);
    if (opp_clean) {
        ctx->heal_mod_pct[opp] = orig - pct;   // 维持降疗（重开面）
        set_storage_int_local(ctx, audit_key_of(k2128AuditBase, owner, 1), 1);
    } else {
        ctx->heal_mod_pct[opp] = orig;         // 门闭：本回合降疗关（获异常负测面）
        set_storage_int_local(ctx, audit_key_of(k2128AuditBase, owner, 1), 0);
    }
    return EffectResult::kOk;
}

// ── 2128 —— 对手不处于异常状态时对手{0}回合内体力恢复效果减少{1}% ──
// int_args = [owner, target, {0}=回合, {1}=%]（args_num=2 + [owner,target] 前缀）。
EffectResult effect_2128_conditional_heal_suppress(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2128, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2128, args,
                     "2128 布局=[owner,target,回合,%]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 2128,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && pct >= 0 && pct <= 100, 2128, args,
                     "需要 回合>0、%∈[0,100]（0%=关闸档，在位即消费）");
    if (!g_core || !g_core->has_active_anomaly) {
        set_storage_int_local(ctx, audit_key_of(k2128AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key_of(k2128AuditBase, self, 3), pct);
    set_storage_int_local(ctx, audit_key_of(k2128AuditBase, self, 4), rounds);
    set_storage_int_local(ctx, audit_key_of(k2128AuditBase, self, 6), 0);   // 本次窗 tick 计数清零
    set_storage_int_local(ctx, audit_key_of(k2128AuditBase, self, 7),
                          storage_int_local(ctx, audit_key_of(k2128AuditBase, self, 7)) + 1);
    // live 标志先读后写（本 cast 是否有未还原的旧窗——基线复用判据）。
    const bool live = storage_int_local(ctx, audit_key_of(k2128AuditBase, self, 8)) == 1;
    // 在位判读（cast 时点，无闩锁）：门闭 = 不写降疗、不武装新窗（官方前件不成立）。
    const bool opp_clean = !k467_has_any_anomaly(ctx, opp);
    set_storage_int_local(ctx, audit_key_of(k2128AuditBase, self, 0), opp_clean ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k2128AuditBase, self, 2), ctx->roundCount);
    if (!opp_clean) {
        if (!live) {
            set_storage_int_local(ctx, audit_key_of(k2128AuditBase, self, 8), 0);
        }
        return EffectResult::kOk;   // 门闭负测面（T8540.c 杀点）；live 旧窗保持
    }
    // 基线捕获：live 中重 cast 复用 slot5 基线（同键覆盖=刷新不叠加，防 pct
    // 叠加与还原链泄漏）；否则捕当前 heal_mod_pct。
    const int orig = live ? storage_int_local(ctx, audit_key_of(k2128AuditBase, self, 5))
                          : ctx->heal_mod_pct[opp];
    set_storage_int_local(ctx, audit_key_of(k2128AuditBase, self, 8), 1);
    set_storage_int_local(ctx, audit_key_of(k2128AuditBase, self, 5), orig);
    ctx->heal_mod_pct[opp] = orig - pct;   // 授予即生效（首手当回合，2241 同款）
    set_storage_int_local(ctx, audit_key_of(k2128AuditBase, self, 1), 1);
    // 捕获-还原窗（2241 同构）：注册 rounds+1 格，末格即还原 tick。
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, /*effect_id=*/2128,
                          rounds + 1,
                          {self, opp, rounds, pct, orig, ctx->roundCount},
                          &k467_2128_tick);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +1）──
SKILL_EFFECT(2128, effect_2128_conditional_heal_suppress)
