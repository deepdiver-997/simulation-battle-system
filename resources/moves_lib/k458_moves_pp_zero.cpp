// K458 · PP 归零两族 —— 1565×8 + 1494×6（2026-11-11，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k458，分支 feat/k458，基点 3efc428d；
// 场景 test/scenario/scenario_8450_k458.cpp，号段 8450-8459）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库现查 2026-11-11）══
//   1565（args_num=2）"{0}回合内每回合结束时使对手随机{1}个技能PP归零"
//   1494（args_num=2）"自身满体力时{0}%令对手随机{1}个技能PP值归零"
//
// ══ 载体切片（unity 库 json_each + side_effect.arg_count 顺序切分，14 载体
//   全链零剩余；场景 T8450-pre 以实际加载器 load_skill_effects 逐载体锁定）══
//   1565 [{0}=回合,{1}=槽位]（8 载体，{1} 全=1、{0}∈{2,3}）：
//     27118 山林相予 [191(1),47(1),463(2),1565(2)]=[4|4|3,300|**3,1**]、
//     27343 袅袅纤枝 [1397(2),707(3),1565(2)]=[1,2|3,3,3|**3,1**]、
//     27616 古树庇护 [1687(1),1582(5),1565(2)]=[4|50,27,50,1,300|**3,1**]、
//     29504 洁净圣域 [191(1),47(1),1565(2),1926(1)]=[4|4|**3,1**|2]
//       （cat4 属性技 must_hit——链上兄弟全窗口/票授予零 rand，精确选槽腿主载体）、
//     35245 核心破解 [1565(2)]=[**3,1**]（纯链单效果）、
//     36113 霹雳旋风踢 [1565(2),573(4)]=[**2,1**|2,100,0,2]（唯一 {0}=2 载体）、
//     38442 欲茧徒囚 [1565(2),1779(2)]=[**3,1**|2,3]、
//     38474 金属缠绕 [1207(2),1565(2)]=[1,1|**3,1**]。
//   1494 [{0}=%,{1}=槽位]（6 载体）：
//     33654 笛音零落 [945(0),1494(2)]=[]|[**100,1**]、
//     33995 杏之舞 [1025(1),1494(2)]=[3|**100,1**]、
//     34624 奋力搏杀 [1494(2)]=[**100,1**]（纯链单效果）、
//     35000 九幽断川镰 [1494(2),1584(1)]=[**100,2**|50]（{1}=2；兄弟 1584
//       "出手时若自身**未满**体力则…"——满体力载体下自然闭、零 rand）、
//     35685 去留皆亡 [1494(2)]=[**100,1**]、
//     37637 沐日夕光 [1395(0),1158(2),1494(2)]=[]|[1,50]|[**70,2**]
//       （唯一 {0}=70 载体——掷点差分腿主载体）。
//   ⚠️ 1565 全部真实载体 {1}=1——{1}≥2 无实测载体；多槽维度差分由 1494 真实
//     载体（35000/37637 {1}=2）与 EffectParamOverride 变档腿（装配期官方覆盖
//     基建，K453 2456 N=2/N=0 同款）承担；实现侧 n=min({1},槽位数) 通用。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例）══
//   · 随机选槽 = 656（resources/moves_lib/j_legacy_dmg_pp.cpp effect_656_
//     chance_random_pp_zero）部分 Fisher-Yates 同构：池 {0..槽位数-1}，每抽
//     j = i + sbs_rand()%(size-i)、swap 后归零 pool[i]——n 抽互不相重
//     （去重结构性成立；同 tick 内不重复选槽）。
//   · 归零 = g_core->pp_zero_slot 原语（include/primitives/battle_primitives.h:770：
//     pp==-1 无限槽 INVALID_PARAM 不参与不发事件；pp<=0 幂等成功无变化；
//     pp_retain（琼华之庇）对**对手来源**（actor==1-target）保留并发 blocked
//     事件；实际归零 emit EVENT_PP_REDUCED（slot=槽、amount=原值））——
//     清 PP 必走原语（直写 skills[i].pp 绕过事件 = 私造通道，禁；core_api.h
//     118-125 填充纪律）。actor 传施放方（对手来源语义成立）。
//   · 1565 窗口 = register_round_effect @ BATTLE_ROUND_END、InRounds
//     （1655 batch_a_skills.cpp:725 同文形直系注册面——"{0}回合内每回合结束时
//     …PP…"；⚠️ 派发预检提示的 K456 register_next_rounds_effect+ROUND_START
//     是"回合**开始**掷点"族口径（掷点须赶在对手行动前武装、施放时该时点已过，
//     故首跳+NextRounds 余窗），本族 tick 在回合**末**、施放当回合的 ROUND_END
//     尚未经过 → InRounds 注册面自然含施放回合（先手即当回合首 tick）。
//     三查修正记档。）
//   · 1565 后手顺延 = 官方 §五.0"先/后出手"裁定（docs_local/docs/02-效果系统/
//     官方机制理解与引擎缺口对照.md:114，用户 2026-09-13 引擎级通用规则）：
//     "N回合内"先手生效 R..R+N-1、后手顺延 R+1..R+N（施放回合已"错过结算"）。
//     起点折算由 round_effect_start_round 内生（场景 038 W1 已锚元数据）；
//     ⚠️ **内核缺口记档**：桶执行面只有 isExpired 门（timed_bucket.cpp
//     execute_at），无 start 下界门——ROUND_END 这类"施放时点后仍在同回合内"
//     的触发，后手的施放回合 tick 会伪进回调（038 的 MOVE_RIGHT 触发天然不受
//     影响——该时点施放时已过）。内核共享文件不动（红线6）→ 插件 tick 以
//     **start_round 实时门**配对折算值（"有效性由效果自己的条件在回调里实时判"
//     sanctioned 模式，K456 减伤条目同款；门在一切 rand 消费之前，被跳过 tick
//     零抽不污染随机序列）。1655 同族后手行为此前无场景锚点（scenario_145 只测
//     先手），本缺口与裁定落地面记档为遗留，供内核微任务/1655 复核参考。
//     重施放 = 同 (source_id=1565, effect_id=1565) 键覆盖（timed_bucket 同源
//     去重）→ 窗口刷新不叠加；窗过期由 ContinuousEffect::isExpired 自动跳过；
//     isRoundEffect → 入"回合类可清集合"（断回合可清）+ ON_STAGE 换宠 epoch
//     作废（离场清窗，1655/K456 同生命周期）。
//   · 1494 = 单发（SKILL_EFFECT 时点）：满体力门（925 effect_batch8.cpp:102
//     同款判据 hp >= max_hp_of——cast 时点在位实时判读，无闩锁：非满不触发、
//     回满再触发）→ roll_percent({0}) 掷点（**在位即消费**：0% 短路 false 不
//     翻转、100% 短路 true 零抽、区间内一抽；掷歪=零抽不选槽，K456/K398 纪律）
//     → 选槽归零。
//     守卫差异记档：{0} 用 [0,100] 而非 >0（显式 0% 是关闸档、在位消费，
//     K314/K340 纪律）；{1} 用 >=0（"随机 0 个"=自然 no-op，不报参数故障）。
//
// ══ 动工前置三查（2026-11-11，k458 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep 1565/1494（src/ include/ 全量）零命中——无路由无硬编码，
//     SKILL_EFFECT default 桶（skills.cpp:182-183 未列 id 全落 SKILL_EFFECT）；
//   ② effect_info 1565/1494 行即模板（PP归零族 40+ 同文近邻中 583/656/1222/
//     716/780/1126 等已注册为模板；1565/1494 未注册）；soul 空间 effect_icon
//     1565 = 闭月之姿·貂蝉（K407 已注册 SOUL_PROGRAM）——双空间两套语义，
//     同号不同义不涉本单；1494 魂印空间无行；
//   ③ registry moves=1542（K456 合入后基线）无两 id；全部 worktree（k457/
//     k459/k460/endgame-walk/agents）moves_lib+soul_lib 并集 grep
//     SKILL_EFFECT(1565/1494)/SOUL_PROGRAM(1565/1494) 零命中——真缺口。
//   遗留记档：1618"{0}回合内每回合结束时令对手随机{1}个技能PP值归零"为 1565
//   近孪生（文案差"值"字）、未注册未立项——后续单候选。
//
// ══ plugin_storage 审计键（10^9+id×10^4 段位口径；owner×16 步长、槽 ≤9 →
//   同基址 owner 间无交叠，K456 键式）══
//   ⚠️ 1565 段**双空间拆分**：soul 空间 1565（K407 貂蝉）已占
//   1015650000+owner*16+槽（运行时区间 ...0000-...0031，
//   soul_lib/k407_solitude_dream_gender.cpp:183）→ 本单 moves 空间 1565 用
//   **1015650100** 子块（...0100-...0125）：与 soul 运行时区间不相交、
//   不越 1015660000 段界、两文件 constexpr 基址不同 → check_storage_keys.py
//   按（容器,基础键）各自独立不报撞。
//   1494 段 1014940000 全库 grep 零占用（2026-11-11）。
//   1565（基 1015650100）：slot0=末 tick 实际归零 mask（bit=槽号）、
//     1=累计 tick 计数、2=末 tick 回合 roundCount、3={1} 行参回显、
//     4=累计抽取数、8=累计实际归零槽数、9=跳过原因（0=无/1=缺 pp_zero_slot）
//   1494（基 1014940000）：slot0=末次实际归零 mask、1=满体力门（1 开/0 闭）、
//     2=末次掷点（1 中/0 歪；门闭写 0——掷点未发生）、3={0}% 回显、5={1} 回显、
//     6=累计掷中数、4/8/9 同 1565 语义（抽取数/实际归零数/跳过原因——由共用
//     选槽助手维护；⚠️ 首版把 1494 专属回显放 slot4、掷中数放 slot8，与助手
//     自增面撞槽被场景审计抓出——slot4 被抽取计数覆盖成 2、slot8 被归零数叠成 2，
//     2026-11-11 修正收拢）
//
// ══ 变异测试实测（2026-11-12 续作补齐；证据 build/k458-mutations/M*/，每份含
//   apply.diff/apply.log/build.log/dylib-sha-mutated/run.log+rc/first-kill/
//   grep-verify/restore-build.log/dylib-sha-restored/rerun.log+rc 全链）══
//   底本 dylib SHA256 = 69cb7d3f39b88334242bf305b80bce276494b2b4de62e327b47a3f6a
//   f893eecc（resources/moves_lib/liblib_1.dylib）；每次还原后 SHA 逐字回到底本、
//   场景复绿（rc=0 / SBS_SCENARIO_CHECKS=237）。共 8 类真红（rc=134 CHECK abort）：
//   M1  选槽去重：部分 Fisher-Yates → 有放回均匀抽（j=rand%size 不缩池）
//       → 杀点 T8450.e R1 mask 期望{2,4}实得{4}（同槽重复抽 popcount=1）。
//   M2  通道：register_round_effect 窗口注册绕过 → cast 时直调 tick
//       → 杀点 T8450.a R2 tick 计数期望 2 实得 1（窗口不存在、R2/R3 无 tick）。
//   M3  顺延门：tick 内 start_round 实时门整体删除（后手顺延失效）
//       → 杀点 T8450.b 后手施放回合 tick 计数期望 0 实得 1（§五.0 裁定面）。
//   M4  窗口行参：{0} rounds 硬编码 3 → 杀点 T8450.a R2 tick 计数期望 2 实得 1
//       （29504 [3,1] 载体 N=3 但硬编码恰同值不红 → M4b 复杀）。
//   M4b 同变异换杀点：T8450.c R3 过期计数期望 2 实得 3（{0}=2 载体 36113 边界）。
//   M5  满体力门：1494 门恒开（删 !full 支）→ 杀点 T8450.j 门闭掷点未发生
//       期望 0 实得 1（半血掷点照进）。
//   M6  概率在位消费：掷歪仍选槽（删 !hit 早退）→ 杀点 T8450.l 掷歪不计数
//       期望 0 实得 1（掷歪连选槽都不该进，掷中计数先爆）。
//   M7  actor 来源：1565 tick 归零 actor 传 target 自身（庇保留对手来源分支失效）
//       → 杀点 T8450.d mask 期望{1,3}实得{0,1,3}（槽0 琼华之庇被破）。
//   M8  per-owner 键位：1565 审计键 owner 恒 0（双边串账）
//       → 杀点 T8450.f R2 side0 tick 计数期望 2 实得 3（side1 先执行挤同一键）。

#include "common.h"

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1565AuditBase = 1015650100;   // moves 空间 1565 子块（soul 空间 K407 占 1015650000）
constexpr int k1494AuditBase = 1014940000;   // 10^9 + 1494×10^4

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

// ── 随机选槽 + 逐槽原语归零（656 部分洗牌同构；bump 审计槽）──
// 返回**实际归零**槽 mask（bit=槽号；pp 已 0 / 无限槽 / 庇保留不算）。
int k458_zero_random_slots(BattleContext* ctx, int target, int want, int actor,
                           int audit_base, int owner) {
    if (!g_core || !g_core->pp_zero_slot) {
        set_storage_int_local(ctx, audit_key_of(audit_base, owner, 9), 1);
        return 0;
    }
    int mask = 0;
    int zeroed = 0;
    std::vector<int> pool;
    const int n_slots = static_cast<int>(ctx->getPet(target).skills.size());
    for (int i = 0; i < n_slots; ++i) {
        pool.push_back(i);
    }
    const int n = std::min(want, static_cast<int>(pool.size()));
    for (int i = 0; i < n; ++i) {
        const int j = i + (sbs_rand() % (static_cast<int>(pool.size()) - i));
        std::swap(pool[static_cast<std::size_t>(i)], pool[static_cast<std::size_t>(j)]);
        const int slot = pool[static_cast<std::size_t>(i)];
        const int before = ctx->getPet(target).skills[static_cast<std::size_t>(slot)].pp;
        (void)g_core->pp_zero_slot(ctx, target, slot, actor);
        const int after = ctx->getPet(target).skills[static_cast<std::size_t>(slot)].pp;
        set_storage_int_local(ctx, audit_key_of(audit_base, owner, 4),
                              storage_int_local(ctx, audit_key_of(audit_base, owner, 4)) + 1);
        if (after < before) {
            mask |= (1 << slot);
            ++zeroed;
        }
    }
    set_storage_int_local(ctx, audit_key_of(audit_base, owner, 8),
                          storage_int_local(ctx, audit_key_of(audit_base, owner, 8)) + zeroed);
    return mask;
}

// ── 1565 单 tick：对手随机 {1} 槽归零 + 审计 ──
EffectResult k458_1565_round_end_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    // tick 行参 = [self, opp, slots, start_round]（cast 时打包，无 owner/target 前缀）
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int slots = args.int_args[2];
    const int start_round = args.int_args[3];
    // ⚠️ 窗口起点实时门（官方 §五.0"先/后出手"裁定，docs_local/docs/02-效果系统/
    //   官方机制理解与引擎缺口对照.md:114）：后手顺延 → 起点 R+1；桶执行面只有
    //   isExpired 门（timed_bucket.cpp execute_at），施放当回合的 ROUND_END 仍会
    //   进到本回调——后手必须在此跳过（施放回合已"错过结算"，属 R+1..R+N 窗外）。
    //   先手 start=R 门恒开；N==1 后手不顺延（round_effect_start_round 定例）。
    //   门放在一切 rand 消费之前：被跳过的 tick 零抽（随机序列记账不受污染）。
    if (ctx->roundCount < start_round) {
        return EffectResult::kOk;
    }
    const int mask = k458_zero_random_slots(ctx, opp, slots, /*actor=*/self,
                                            k1565AuditBase, self);
    set_storage_int_local(ctx, audit_key_of(k1565AuditBase, self, 0), mask);
    set_storage_int_local(ctx, audit_key_of(k1565AuditBase, self, 1),
                          storage_int_local(ctx, audit_key_of(k1565AuditBase, self, 1)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1565AuditBase, self, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k1565AuditBase, self, 3), slots);
    return EffectResult::kOk;
}

// ── 1565 —— {0}回合内每回合结束时使对手随机{1}个技能PP归零 ──
// int_args = [owner, target, {0}=回合, {1}=槽位]（args_num=2 + [owner,target] 前缀）。
EffectResult effect_1565_round_end_random_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1565, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1565, args,
                     "1565 布局=[owner,target,回合,槽位]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int slots = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1565,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1565, args, "需要 回合 > 0");
    if (slots <= 0) {
        return EffectResult::kOk;   // "随机 0 个"=自然 no-op（不武装不报故障）
    }
    // 回合末窗口（InRounds；官方 §五.0 起点折算：先手含施放回合、后手顺延 R+1——
    // 折算值由 register_round_effect 内生；tick 侧以 start_round 实时门配对，见上）。
    const EffectWindowKind wk = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(/*effect_id=*/1565)
        : EffectWindowKind::InRounds;
    const int start_round = ctx->round_effect_start_round(self, rounds, wk);
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, /*effect_id=*/1565, rounds,
                          {self, opp, slots, start_round}, &k458_1565_round_end_tick);
    return EffectResult::kOk;
}

// ── 1494 —— 自身满体力时{0}%令对手随机{1}个技能PP值归零 ──
// int_args = [owner, target, {0}=%, {1}=槽位]（args_num=2 + 前缀）。
EffectResult effect_1494_full_hp_chance_random_pp_zero(BattleContext* ctx,
                                                       const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1494, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1494, args,
                     "1494 布局=[owner,target,%,槽位]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    const int slots = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1494,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct >= 0 && pct <= 100, 1494, args, "需要 概率 ∈ [0,100]");
    // 满体力门：cast 时点在位实时判读（hp=当前、numericalBase[HP]=最大，925 判据；
    // 无闩锁——非满不触发、回满再触发）。门闭 → 不掷不选（掷点也未发生）。
    const bool full = ctx->getPet(self).hp >= max_hp_of(ctx, self);
    set_storage_int_local(ctx, audit_key_of(k1494AuditBase, self, 1), full ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k1494AuditBase, self, 3), pct);
    set_storage_int_local(ctx, audit_key_of(k1494AuditBase, self, 5), slots);
    set_storage_int_local(ctx, audit_key_of(k1494AuditBase, self, 2), 0);
    if (!full || slots <= 0) {
        return EffectResult::kOk;
    }
    // 掷 {0}%（roll_percent 在位即消费：0% 短路 false、100% 短路 true 零抽、
    // 区间内一抽；掷歪=零抽不选槽）。
    const bool hit = roll_percent(pct);
    set_storage_int_local(ctx, audit_key_of(k1494AuditBase, self, 2), hit ? 1 : 0);
    if (!hit) {
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key_of(k1494AuditBase, self, 6),
                          storage_int_local(ctx, audit_key_of(k1494AuditBase, self, 6)) + 1);
    const int mask = k458_zero_random_slots(ctx, opp, slots, /*actor=*/self,
                                            k1494AuditBase, self);
    set_storage_int_local(ctx, audit_key_of(k1494AuditBase, self, 0), mask);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(1565, effect_1565_round_end_random_pp_zero)
SKILL_EFFECT(1494, effect_1494_full_hp_chance_random_pp_zero)
