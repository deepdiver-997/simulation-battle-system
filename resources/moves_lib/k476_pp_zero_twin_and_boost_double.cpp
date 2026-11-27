// K476 · PP 归零近孪生 + 强化翻倍两族 —— 1618×2 + 512×6（2026-11-17，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k476，分支 feat/k476，基点 fda62989
// （树内另含 K469/K470/K471 实现——派发口径"不含 K469-K475"与实存有差，独立三查
// 以实存为准记档；直系先例 K455-K462 全在树内）；场景 test/scenario/scenario_8630_
// k476.cpp，号段 8630-8639）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-17）══
//   1618（args_num=2）"{0}回合内每回合结束时令对手随机{1}个技能PP值归零"
//     —— K458 1565"{0}回合内每回合结束时使对手随机{1}个技能PP归零"近孪生（文案差
//     一"值"字；effect_des 无 1618 行 → effect_info 即权威全文；语义同构）。K458
//     遗留记档"1618 近孪生候选"本单转正。通道决策沿 K458 核验正确先例：回合末 tick
//     BATTLE_ROUND_END + InRounds、MINSTD 固定随机源选槽可复现、pp_zero_slot 原语。
//   512（args_num=6）"自身{0},40%概率强化翻倍（boss有效）"
//     —— 六行参=自身六槽提升量，**官方文本序** [攻,防,特攻,特防,速,命中]（5/6 真实
//     载体 tips 逐位吻合：22527 [1,1,0,1,0,0]="攻击、防御、特防提升了"、23037
//     [1,1,0,0,1,1]="攻击、防御、速度、命中提升了"、27398 [1,1,0,1,1,0]="攻击、双防、
//     速度"、28717 [1,1,0,1,1,1]="攻击、防御、特防、速度、命中"、27604 全 1="全属性
//     都提升了"）；引擎槽序 [攻,特攻,防御,特防,速,命中]（battleContext.h:433）→
//     kStatMap[6]={0,2,1,3,4,5}（battleContext.h:439 官方序→引擎序映射，stat_dispel
//     kStatMap 同一套）。40% 写死在模板文案（args_num=6 全被六槽占、无概率行参位
//     ——套路目录 §七点五"零参宏把量写死在模板文案里，插件侧写死并在注释注明"）。
//     "（boss有效）"= 官方 boss 战限定标注（引擎无 boss 区分，本单不设门，记档）。
//     翻倍 = 强化效果翻倍（993/1378 直系同构：roll_percent 命中 → 六槽再写一遍）。
//
// ══ 载体切片（unity 库 json_each + side_effect.arg_count 顺序切分，8 载体全链
//   零剩余；场景 T8630-pre 以实际加载器 load_skill_effects 逐载体锁定）══
//   1618 [{0}=回合,{1}=槽位]（2 载体，行参全 [4,1]——真实载体无族内差分，
//     多值维度由 EffectParamOverride 变档腿承担，K453 2456/K458 35245 同款）：
//     27330 完美复制 [48(1),45(1),51(1),1618(2)]=[4|4|4|**4,1**]
//       （cat4 属性技；链上兄弟 48 免疫票/45/51 属性视图拷贝窗口全零 rand（现查
//       anomaly_channel.cpp:131 / j_defense_sync.cpp:45 / j_power_view3.cpp:148
//       ——45/51=ws.battle_attrs 拷贝+register_round_effect，48=票授予）→ 精确
//       选槽主载体）、
//     28558 恶魔心跳 [885(1),799(2),1618(2)]=[1|2,2|**4,1**]
//       （885 全属性+1 体力门翻倍、799 恢复+等量百分比伤——均即时零 rand；
//       1618 部分后手顺延腿主载体）。
//   512 [攻,防,特攻,特防,速,命]（6 载体，全 cat4 属性技）：
//     22527 扭曲命运 [512]=[**1,1,0,1,0,0**]（纯链——掷歪/掷中双腿主载体）、
//     23037 光芒力量 [512]=[**1,1,0,0,1,1**]（纯链；唯一速/命档——kStatMap 错序
//       变异杀点：官方1(防)错写引擎槽1(特攻)时特攻+1/防御+0 立红）、
//     27217 祈愿之星 [512(6),439(2),776(1)]=[**2,1,0,1,0,0**|3,300|2]
//       （唯一 {0}=2 档——增量硬编码 1 变异杀点；链上兄弟 439 窗口注册/776 票
//       授予零 rand；官方 tips 只写"自身攻击能力提升了"为官方文案省略，切片
//       arg_count=6 顺序切分零剩余为权威）、
//     27398 可口奶油 [512(6),1226(3),524(2)]=[**1,1,0,1,1,0**|3,300,300|3,2]、
//     27604 鬼迷妖影 [512(6),707(3),843(2)]=[**1,1,1,1,1,1**|4,3,3|2,2]
//       （唯一全六槽载体）、
//     28717 砂石四散 [512(6),58(1),481(2)]=[**1,1,0,1,1,1**|2|2,2]。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例）══
//   · 1618 = K458 1565 同构直系（k458_moves_pp_zero.cpp 结构/注释密度/断言口径
//     一起抄）：随机选槽 = 656 部分 Fisher-Yates 同构（池 {0..槽位数-1}，每抽
//     j = i + rng.next()%(size-i)、swap 后归零 pool[i]，n 抽互不相重）；归零 =
//     g_core->pp_zero_slot 原语（pp<=0 幂等、无限槽 INVALID_PARAM 不参与、
//     pp_retain 琼华之庇对对手来源保留并发 blocked 事件、实际归零 emit
//     EVENT_PP_REDUCED）；窗口 = register_round_effect @ BATTLE_ROUND_END、
//     InRounds（1655 同文形直系注册面；施放当回合的 ROUND_END 尚未经过 → 先手
//     即当回合首 tick）；后手顺延 = 官方 §五.0"先/后出手"裁定（官方机制理解与
//     引擎缺口对照.md:114）：先手生效 R..R+{0}-1、后手顺延 R+1..R+{0}——起点
//     折算由 round_effect_start_round 内生；⚠️ 内核缺口记档（K458 同款）：桶执行
//     面只有 isExpired 门（timed_bucket.cpp execute_at）无 start 下界门——ROUND_END
//     触发后手的施放回合 tick 会伪进回调 → 插件 tick 以 start_round 实时门配对
//     （门在一切 rand 消费之前，被跳过 tick 零抽不污染随机序列，K456 减伤条目
//     sanctioned 模式同款）。重施放 = 同 (source_id=1618, effect_id=1618) 键覆盖
//     （窗口刷新不叠加）；窗过期由 ContinuousEffect::isExpired 自动跳过；
//     isRoundEffect → 入"回合类可清集合" + ON_STAGE 换宠 epoch 作废（离场清窗）。
//   · 512 = SKILL_EFFECT 时点单发（993/1378 直系同构——cat4 属性技全 must_hit+
//     acc100 场景口径下落地零 rand，链上兄弟 439/776/1226/524/707/843/58/481
//     全窗口/票授予零 rand）：官方文本序六槽逐槽 g_core->stat_change 真写
//     （零增量槽不调——事件面干净；本体/视图一并同步、emit EVENT_STAT_CHANGED，
//     禁直写 ability_levels）→ roll_percent(40) 掷点（**在位即消费**：写死常量
//     仍守在位消费语义——40∈(0,100) 区间内一抽；1378 {1}% 行参版直系同构，本族
//     40 写死模板见头注）→ 命中则六槽增量再写一遍（993"强化效果翻倍"=all_stats_up
//     二次调用同构、1378 roll_percent 命中=二次提升同构）。stat_change 返回
//     SUCCESS/AT_CAP/BLOCKED/INVALID_PARAM 四态——AT_CAP/BLOCKED 非本次变更
//     （等级不动），逐槽 SUCCESS 记实变掩码；BLOCKED 面（STAT_BOOST 封锁族
//     索伦森 1011）场景专腿断言（封锁时掷点照常发生——封锁挡的是提升不是掷点）。
//   · ⚠️ 512 无窗口/无票/无状态——离场清理结构性成立（提升写 ability_levels 本体，
//     换宠全槽清零为引擎通用行为 battleContext.h clear_all_ability_levels）。
//
// ══ 动工前置三查（2026-11-17，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep \b1618\b/\b512\b（src/ include/ 全量）零命中——无路由无硬编码，
//     SKILL_EFFECT default 桶（skills.cpp 未列 id 全落 SKILL_EFFECT）；
//   ② effect_info 1618 行 args_num=2 / 512 行 args_num=6 即模板（1618 无
//     effect_des 行；512 的 effect_des id=512 行 desc"直到下回合结束对手受到的
//     百分比伤害提升1/4"（鬼祰·魍 monster 4444）与模板不同文=SOP §六.1 错位行，
//     不取，effect_info 模板为权威）；custom_effect_overrides 无两 id 窗口声明
//     （→ InRounds 默认家族口径）；
//   ③ registry 基线 moves=1572/soul=415：moves 无两 id；**soul 空间 1618 已实现**
//     （K387 地葬界神 4024，k387_order_drain_shield.cpp——tips 权威"先/后手吸取+
//     承伤转护盾"，与 effect_info PP 归零模板异文=双空间两套语义，K458 1565/
//     K407 貂蝉同判；其审计键走 9901618xx 段与本单 moves 段不撞）；soul 无 512。
//     全部 worktree（k475/endgame-walk/web-replay/agents）moves_lib+soul_lib 并集
//     grep SKILL_EFFECT(1618/512)/SOUL_PROGRAM(1618/512)/kMark 两式零命中——真缺口。
//   记档壁垒甄别：tools/db_patches 无两 id 补丁；无 900000 拆分迁移与故意退役
//     记录（全库 grep 现查 2026-11-17）。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456/K458 键式）══
//   1618 基 1016180000：全库 grep 零占用（soul 空间 1618=K387 走 9901618xx，
//     双空间不相交）。slot0=末 tick 实际归零 mask（bit=槽号）、1=累计 tick 计数、
//     2=末 tick 回合 roundCount、3={1} 行参回显、4=累计抽取数、8=累计实际归零
//     槽数、9=跳过原因（0=无/1=缺 pp_zero_slot）——与 K458 1565 同布局（近孪生
//     同构，场景断言逐槽对齐）。
//   512 基 1015120000：全库 grep 零占用。slot0=末次掷点（1 翻倍/0 单倍——掷点
//     发生即写，掷歪写 0）、1=末次逐槽实变掩码（引擎槽 bit，SUCCESS 才置——
//     AT_CAP/BLOCKED/INVALID_PARAM 不算）、2=累计 SKILL_EFFECT 触发数、3=累计
//     掷中数、4=跳过原因（0=无/1=缺 stat_change）、5..10=官方文本序六行参回显
//     （[攻,防,特攻,特防,速,命]）。
//
// ══ 变异测试实测（2026-11-17；证据 build/k476-mutations/M*/，每份含
//   apply.diff/grep-verify/build.log+rc/dylib-sha-mutated/run.log+rc/first-kill/
//   restore-build.log+rc/dylib-sha-restored/rerun.log+rc 全链）══
//   底本 dylib SHA256 = 68c8809f27de3f7db3412be91251804f97dd261527df3f5c4162c5e5
//   ce330f6d（build/plugins/moves_lib/liblib_1.dylib，含 K476 注册的实现底本）；
//   每次还原后 SHA 逐字回到底本、场景复绿（rc=0 / SBS_SCENARIO_CHECKS=231）。
//   共 12 类真红（rc=134 CHECK abort；删 .o 强编+SHA 双守卫——Make 3.81 mtime
//   整数秒截断，K468 实证）：
//   M1  选槽去重：部分 Fisher-Yates → 有放回均匀抽（j=rand%size 不缩池）
//       → 杀点 T8630.d mask 期望{1,3}实得{3}（5 抽有放回覆盖不满全槽）。
//   M2  通道：register_round_effect 窗口注册绕过 → cast 时直调 tick
//       → 杀点 T8630.a R2 tick 计数期望 2 实得 1（窗口不存在、R2 起无 tick）。
//   M3  顺延门：tick 内 start_round 实时门整体删除（后手顺延失效）
//       → 杀点 T8630.b 后手施放回合 tick 计数期望 0 实得 1（§五.0 裁定面）。
//   M4  窗口行参：{0} rounds 硬编码 4 → 杀点 T8630.c R3 过期计数期望 2 实得 3
//       （{0}=2 override 载体边界；真载体 [4,1] 与硬编码同值不红 → override 腿
//       专门承担，K458 M4/M4b 同教训）。
//   M5  槽位行参：{1} slots 硬编码 1 → 杀点 T8630.d {1} 回显期望 5 实得 1。
//   M6  per-owner 键位：1618 tick 审计 owner 恒 0（双边串账）
//       → 杀点 T8630.f R2 side0 tick 计数期望 2 实得 3（side1 先执行挤同一键）。
//   M7  actor 来源：1618 tick 归零 actor 传 target 自身（庇保留对手来源分支失效）
//       → 杀点 T8630.d mask 期望{1,3}=10 实得 11（槽0 琼华之庇被破）。
//   M8  概率 40→0：掷中腿灭 → 杀点 T8630.j 掷点审计期望 1 实得 0。
//   M9  概率 40→100：掷歪腿灭 → 杀点 T8630.i 掷点审计期望 0 实得 1。
//   M10 六槽映射错序：kStatMap {0,2,1,3,4,5}→{0,1,2,3,4,5}（官方文本序直写引擎槽）
//       → 杀点 T8630.k1 23037 实变掩码期望{0,2,3}=13 实得 11（官方1(防) 错写
//       引擎槽1(特攻)——23037 唯一速/命档载体的错序甄别面）。
//   M11 翻倍支删除：掷中不再二次提升 → 杀点 T8630.j 掷中计数期望 1 实得缺键 -1。
//   M12 增量行参硬编码 1：六槽 delta 读行参 → 恒 1
//       → 杀点 T8630.i 官方序回显 slot10(官方5=命) 期望 0 实得 1（27217 攻+2 档
//       面 T8630.k2 同杀）。

#include "common.h"

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1618AuditBase = 1016180000;   // 10^9 + 1618×10^4（soul 空间 K387 走 990 段）
constexpr int k512AuditBase = 1015120000;    // 10^9 + 512×10^4

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

// ── 随机选槽 + 逐槽原语归零（K458 1565 同构；bump 审计槽）──
// 返回**实际归零**槽 mask（bit=槽号；pp 已 0 / 无限槽 / 庇保留不算）。
int k476_zero_random_slots(BattleContext* ctx, int target, int want, int actor,
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
        const int j = i + (ctx->rng().next() % (static_cast<int>(pool.size()) - i));
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

// ── 1618 单 tick：对手随机 {1} 槽归零 + 审计（K458 1565 tick 同构）──
EffectResult k476_1618_round_end_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    // tick 行参 = [self, opp, slots, start_round]（cast 时打包，无 owner/target 前缀）
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int slots = args.int_args[2];
    const int start_round = args.int_args[3];
    // ⚠️ 窗口起点实时门（官方 §五.0"先/后出手"裁定）：后手顺延 → 起点 R+1；桶执行
    //   面只有 isExpired 门，施放当回合的 ROUND_END 仍会进到本回调——后手必须在此
    //   跳过（施放回合已"错过结算"，属 R+1..R+N 窗外）。先手 start=R 门恒开；
    //   N==1 后手不顺延（round_effect_start_round 定例）。
    //   门放在一切 rand 消费之前：被跳过的 tick 零抽（随机序列记账不受污染）。
    if (ctx->roundCount < start_round) {
        return EffectResult::kOk;
    }
    const int mask = k476_zero_random_slots(ctx, opp, slots, /*actor=*/self,
                                            k1618AuditBase, self);
    set_storage_int_local(ctx, audit_key_of(k1618AuditBase, self, 0), mask);
    set_storage_int_local(ctx, audit_key_of(k1618AuditBase, self, 1),
                          storage_int_local(ctx, audit_key_of(k1618AuditBase, self, 1)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1618AuditBase, self, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k1618AuditBase, self, 3), slots);
    return EffectResult::kOk;
}

// ── 1618 —— {0}回合内每回合结束时令对手随机{1}个技能PP值归零 ──
// int_args = [owner, target, {0}=回合, {1}=槽位]（args_num=2 + [owner,target] 前缀）。
EffectResult effect_1618_round_end_random_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1618, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1618, args,
                     "1618 布局=[owner,target,回合,槽位]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int slots = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1618,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1618, args, "需要 回合 > 0");
    if (slots <= 0) {
        return EffectResult::kOk;   // "随机 0 个"=自然 no-op（不武装不报故障）
    }
    // 回合末窗口（InRounds；官方 §五.0 起点折算：先手含施放回合、后手顺延 R+1——
    // 折算值由 register_round_effect 内生；tick 侧以 start_round 实时门配对，见上）。
    const EffectWindowKind wk = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(/*effect_id=*/1618)
        : EffectWindowKind::InRounds;
    const int start_round = ctx->round_effect_start_round(self, rounds, wk);
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, /*effect_id=*/1618, rounds,
                          {self, opp, slots, start_round}, &k476_1618_round_end_tick);
    return EffectResult::kOk;
}

// ── 512 —— 自身{0}(六槽,官方文本序),40%概率强化翻倍（模板常量） ──
// int_args = [owner, target, 攻,防,特攻,特防,速,命]（args_num=6 + [owner,target] 前缀）。
// 官方文本序 [攻,防,特攻,特防,速,命中] → 引擎槽序 [攻,特攻,防御,特防,速,命中] 的
// 映射 kStatMap（battleContext.h:433/439 权威注释；stat_dispel kStatMap 同套）。
constexpr int k476_512_StatMap[6] = {0, 2, 1, 3, 4, 5};
// 40% 概率=模板文案常量（args_num=6 全被六槽占、无概率行参位；头注记档）。
constexpr int k476_512_DoubleChance = 40;

EffectResult effect_512_self_boost_chance_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 512, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, 512, args,
                     "512 布局=[owner,target,攻,防,特攻,特防,速,命]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 512,
                     args, "需要 owner/target ∈ {0,1}");
    // 六槽行参（官方文本序原序回显审计；实现按 kStatMap 映射引擎槽）。
    int deltas[6];
    for (int i = 0; i < 6; ++i) {
        deltas[i] = args.int_args[2 + i];
        set_storage_int_local(ctx, audit_key_of(k512AuditBase, self, 5 + i), deltas[i]);
    }
    set_storage_int_local(ctx, audit_key_of(k512AuditBase, self, 2),
                          storage_int_local(ctx, audit_key_of(k512AuditBase, self, 2)) + 1);
    if (!g_core || !g_core->stat_change) {
        set_storage_int_local(ctx, audit_key_of(k512AuditBase, self, 4), 1);
        return EffectResult::kOk;
    }
    // 基础六槽提升：官方文本序逐槽 stat_change 真写（零增量槽不调——事件面干净；
    // SUCCESS 才记实变掩码——AT_CAP/BLOCKED 等级不动的档不算实变）。
    int changed_mask = 0;
    for (int i = 0; i < 6; ++i) {
        if (deltas[i] == 0) {
            continue;
        }
        const int slot = k476_512_StatMap[i];
        if (g_core->stat_change(ctx, self, slot, deltas[i]) == StatChangeResult::SUCCESS) {
            changed_mask |= (1 << slot);
        }
    }
    // 40% 掷点（在位即消费：区间内一抽；掷歪=单倍到此为止）。1378 {1}% 行参版
    // 直系同构；命中 → 六槽增量再写一遍（993"强化效果翻倍"同构）。
    const bool doubled = roll_percent(ctx->rng(), k476_512_DoubleChance);
    set_storage_int_local(ctx, audit_key_of(k512AuditBase, self, 0), doubled ? 1 : 0);
    if (doubled) {
        set_storage_int_local(ctx, audit_key_of(k512AuditBase, self, 3),
                              storage_int_local(ctx, audit_key_of(k512AuditBase, self, 3)) + 1);
        for (int i = 0; i < 6; ++i) {
            if (deltas[i] == 0) {
                continue;
            }
            if (g_core->stat_change(ctx, self, k476_512_StatMap[i],
                                    deltas[i]) == StatChangeResult::SUCCESS) {
                changed_mask |= (1 << k476_512_StatMap[i]);
            }
        }
    }
    set_storage_int_local(ctx, audit_key_of(k512AuditBase, self, 1), changed_mask);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(1618, effect_1618_round_end_random_pp_zero)
SKILL_EFFECT(512, effect_512_self_boost_chance_double)
