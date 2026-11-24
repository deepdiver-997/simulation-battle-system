// K487 · 回合窗回血+血量先制 / 对手提升门伤害封顶 两族 —— 953×5 + 798×5
// （2026-11-19 初版 / 2026-11-23 续作核验，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k487，分支 feat/k487；
// 11-23 编排把分支基点重置到 079a0e0f、官方 DB 22 日 23:18 重建——三查与 10 载体
// 切片已对现库 SQL 逐位重核零漂移；场景 test/scenario/scenario_8742_k487.cpp，
// 号段 8742-8749，原号 8740 被主线 expel-switch 线占用顺延）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查
//   2026-11-19）══
//   953（args_num=4）"{0}回合恢复最大体力的1/{1}，体力低于{2}%时先制+{3}"
//   798（args_num=2）"若对手处于能力提升状态，则对手{0}回合内造成的伤害不超过{1}"
//   effect_des 两 id 均无行（953/798 无 kind 行——官方 info 即全文，K480 同形记档）
//   → 模板全文即权威；28592 福龙临世 tips"开始恢复自己的体力了"与回血子句互证
//   （先制子句 tips 无转述——模板即权威，K473 2230 同款口径）。
//
// ══ 载体切片（unity 库 json_each 精确元素 + side_effect.arg_count 依序切分，
//   10 载体链上伴生除 1117 外全在册（1117 未注册 = clone 无实现静默跳过，
//   K469 705/902 同款 inert——33362 只进切片核验不进行为面，防 parser 兜底
//   干扰）；场景 T8742-pre 以实际加载器 load_skill_effects 逐载体锁定）══
//   953 [{0}=回合,{1}=分母,{2}=%,{3}=先制量]（5 载体全 cat4 属性技 p0）：
//     24921 坠天断魂曲 [952(1),953(4),776(1)]=[1|**4,3,40,2**|2]（952=全属性+1
//       若对手降属翻倍、776=下1回合攻击伤害翻倍），
//     25269 地底核心 [585(6),953(4),843(2)]=[1,1,1,1,1,1|**4,3,40,2**|2,1]
//       （⚠️ 843=下2回合全技能先制+1——先制断言面避开本载体），
//     26577 冰霸霸之魂 [4(3),4(3),4(3),953(4),780(3)]
//       =[0,100,2|1,100,2|3,100,2|**4,3,40,2**|3,50,1]（780 50% 掷点污染 rand），
//     28592 福龙临世 [48(1),953(4),557(3)]=[4|**4,3,40,2**|4,100,20]
//       （48=4回合免异常、557=对手正先制技100%异常20——对手 NONE 趟 inert，
//       **主载体**：官方 tips 三句与三效果逐条互证），
//     28714 风翼护体 [138(2),48(1),953(4)]=[1,3|3|**3,3,60,1**]
//       （唯一 {0}=3/{2}=60/{3}=1 差分载体；138=先出手1回合免攻击伤+反弹）。
//   798 [{0}=回合,{1}=封顶]（5 载体）：
//     17772 凛冬之怒 [180(0),798(2)]=[∅|**3,200**]（cat1 p85 prio3；180=消对手
//       回合类效果已注册 stat_dispel），
//     33362 小满麦渐黄 [1117(0),798(2)]=[∅|**3,300**]（cat2 特150；1117 未注册
//       inert——切片核验载体），
//     35691 卸力毒针 [798(2)]=[**3,200**]（cat1 物140 **纯链单效果主载体**），
//     36229 银鸣 [913(1),798(2)]=[100|**2,280**]（cat1 p85 prio3；唯一 {0}=2
//       差分载体；913=消对手回合类效果+成功则下回合增伤100%——对手无回合类
//       效果时 inert），
//     37618 绝霭纤介 [1230(2),458(1),1736(1),798(2)]
//       =[1,2|100|5|**3,300**]（cat1 物150；1230=低体门下2次先制+1 已注册
//       j_low_hp_next_priority——血量高门闭 inert）。
//
// ══ 953 实现口径（套路目录 §零决策树①：条件先制族走选择期路由唯一正位）══
//   · **通道 = 选择期路由（custom_skill_selection_routes 953 行，expansion_7.sql）
//     + 插件体双半**：条件先制必须走表（J66 勘误系结论——用技期注册赶不上首回合
//     先手判定；454/616/1117 同判）。⚠️ 路由命中后 loadSkills `continue` 吞掉普通
//     SKILL_EFFECT 注册（skills.cpp:636-638）→ 回血半由 body 自挂（539 直系：
//     "先制半在选择期判，威力半由 body 自挂当回合 SKILL_EFFECT"）：
//     ①先制半：MOVE_RIGHT 桶每次选择在位重判——门 = hp×100 < max_hp×{2}
//       （严格低于整数直乘，454/1397/K469 616 同款口径；恰 {2}% → 门闭，
//       T8742.c 锁边界负测）→ ws.preemptive_level[self] += {3}（优先级修改面，
//       不触技能基础优先级——基值先制哨兵其后叠条件加成，先手权比较前生效）。
//       无账本无 latch：每回合 on_selected 重装节点即判当前血量面（回血后门闭/
//       受击后门开，T8743 三段锁定——K469 M11 幸存教训同款防覆盖）。
//     ②回血半：body 无条件（回血子句不带门）自挂当回合 FIRST/SECOND_SKILL_EFFECT
//       桶武装节点（rounds=1、InRounds N=1 本回合、539 同款"同回合执行是期望行为"
//       ——节点挂比武装点更晚的桶，命中真实用技窗口）；武装 tick 里
//       register_round_effect(ROUND_REDUCTION_ALL_ROUND_MINUS, self, 953, {0},
//       ...) 开窗——**tick 时点 = 回合扣减点**（762 直系：953 字面无"使用技能"
//       限定（2230 有、953 无）→ 无 used_skill 门、不用 AFTER_ACTION 桶；
//       每回合恢复最大体力 1/{1}，heal_max_hp_fraction 正式原语（吃封回血+
//       恢复修正全链，实际量=前后差审计）；已阵亡不结算（762 同款）；
//       {1}<=0 = 回血子句关闸（在位即消费，K314/K340 纪律）。
//   · 窗界 = InRounds（模板无"下"字——2230/K456 首跳族同判；先手当回合即结算
//     第 1 次、后手 round_effect_start_round 顺延，common.h:195 官方回合数判定
//     契约）→ 重施放 = 同 (state,owner,source_id) 键覆盖刷新不叠加（timed_bucket
//     同源去重）；回合类条目断回合可清 + ON_STAGE 换宠 epoch 作废（离场清理
//     结构性成立，T8744 锁）。
//
// ══ 798 实现口径 ══
//   · 门 = **对手处于能力提升状态**在位判读 = ability_levels[opp] 任一槽 >0
//     （六槽含命中，K289 659/K481 728 直系）——判读时点 = SKILL_EFFECT 施放时点
//     **武装快照**（门闭后对手获提升不补武装、窗内提升被消除不解除封顶）。
//     甄别三线：①姊妹 766/1897 门（消除成功）在武装时点、窗随后独立延续；
//     ②模板语序"[条件]时，则[对手{0}回合内…]" = 条件前置句式（K481 728 门时点
//     甄别③ K470 674 直系——条件归属动作时点，非持续在位门）；③695 式"每回合
//     重评"家族模板均带"每回合"限定词，本模板无 → 武装快照档。官方无门重评
//     实测，按姊妹结构建模记档（T8747 锁死——变异"门每回合重评"杀点）。
//   · 封顶 = **DamagePhase::CAP 一等公民通道 MITIGATE 类别**（K1 766 直系 +
//     K466 1834 纪律移植）："伤害不超过{1}"无"点"无"攻击"字样——绝对值封顶
//     （1834 同形判读）、红伤管线全包（攻击红伤 + 属性直伤 isRed 面，766
//     "官方锁伤生效于属性伤害"口径——1834 因明写"攻击伤害"才排除属性伤，
//     本模板无此限定不排除，T8748 属性直伤封顶正测锁死）；粉伤/真伤不走红
//     管线不封（766/1834 同口径记档）。697「无视伤害限制」凭证消费（766 纪律
//     ：凭证命中不封顶但窗不消耗——窗口型非次数票，无"消费"语义可耗）。
//   · 窗界 = [start, start+{0})，start = round_effect_start_round(owner, {0},
//     InRounds)——后手顺延（common.h:195 契约；⚠️ 766 未折算 = 旧形态记档，
//     本单按官方回合数判定折算，后手施放者不因先手权序少一回合保护）。
//     CAP 条目无自动过期 → lambda 自判窗界（damage_pipeline.h 头注③ sanctioned
//     模式：有效性由效果自己的条件在回调里实时判）+ 窗外命中计数审计。
//   · 生命周期 = 重施放代际递增旧条目自灭（K466 1834 直系：generation 不匹配
//     即 return，刷新不叠加）+ ON_STAGE 绑定 pipeline_valid_id（换宠 epoch
//     作废 = 离场清理结构性成立，register_damage_effect 默认 scope）。
//
// ══ 动工前置三查（2026-11-19，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep -w 953/798（src/ include/ 全量）零命中——无路由无硬编码；
//   ② effect_info 953（args_num=4）/798（args_num=2）行即模板，**同文查无重复**
//     （953 近邻=1053/1883/2099/2528 异文回血族、798 近邻=766/1897"攻击伤害
//     不超过{1}**点**"绝对值姊妹+1834 无"点"同形直系——异文已核不改行参语义）；
//     side_effect 两行 ac=4/2 与 args_num 一致、10 载体顺序切分零剩余（上表）；
//     custom_effect_overrides/programs/combo_rules 两 id 零行（→ InRounds 默认
//     家族口径）；effect_des 两 id 无行；boss_invalid_effects.json 含两 id =
//     effect_icon.tips 尾注官方名单（R7"消费层未实现"语义，非退役记录——
//     K456 1369 判例同款）；tools/db_patches grep 两 id 零命中——**无 900000
//     拆分迁移与故意退役记录**（派发项确认）；
//   ③ registry 基线 moves=1599/soul=415/suit=4（build/baseline-gate.log 1017
//     场景，K485 合入后）moves 空间无两 id（952/954…959 邻位在册、953 缺位；
//     797/799…808 邻位在册、798 缺位）——真缺口；resources/test/examples 全库
//     SKILL_EFFECT(953/798 零命中；SOUL_PROGRAM(798 = **K245 已实现**
//     （resources/soul_lib/k245_lowgate_para_highgate_amp.cpp:165，格劳瑞 3150
//     低体麻痹+高体增伤族，审计键 991445000-991447000 旧段位）——**双空间同号
//     两套语义共存**（K480 soul711/K469 soul616 同判），本单键段与其零交叠且按
//     K465/K469 子块先例取 +0100；SOUL_PROGRAM(953 零命中（effect_icon 953 行
//     =宠 3347 魂印模板占用、无运行时注册 → 审计基址免子块拆分，K481 728 判据
//     ：soul 无实现=无键段）；兄弟 worktree（agents/endgame-walk/k484/k488/
//     k489/web-replay）三查零命中——真缺口。
//   ★ 路由授权与总数：custom_skill_selection_routes 现态 **38 行**（K469
//     expansion_6 后）→ 本单扩行 953 一行（plain）= 39 行态，引用面 479/475 →
//     484/480（五载体链上均无既有路由 id，SQL 核算 2026-11-19）；扩行授权 =
//     板行明写"血量门先制+{3}（616 先例直系）"——条件先制族唯一正位即路由行
//     （K469 J335 形三件套先例：补丁+插件体+场景面）；共享库本 worktree 零写入
//     （场景私有 DB 自备，K469 同款），集成侧 apply_all 重放后收敛。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456/K469
//   键式；新增段全库 grep 零占用 2026-11-19，check_storage_keys.py 门禁静态扫）══
//   953（基 1009530000）：slot0={0}回显 1={1}回显 2={2}回显 3={3}回显
//     4=末次选择判门（1=开/0=闭）5=累计判门次数 6=累计门开次数 7=末次判门回合
//     8=末次出值后 preemptive_level 快照 9=末次武装窗起点回显 10=累计回血 tick 数
//     11=末次回血实际量 12=末次回血回合 13=末次回血前 hp 14=末次回血后 hp
//     15=跳过原因（0=无/1=缺 heal_amount/2={1}<=0 关闸/3={0}<=0 关闸）
//   798（基 **1007980100**，soul 798=K245 已实现 → +0100 子块拆分，基段
//     1007980000 留白）：slot0={0}回显 1={1}cap回显 2=末次门判（1=武装/0=门闭）
//     3=末次武装时对手正槽计数 4=窗起点回显 5=累计封顶次数 6=末次封顶回合
//     7=末次命中原伤 8=末次命中 post-cap 值 9=累计 697 穿透数 10=武装代际
//     11=累计武装次数 12=窗终点（exclusive）回显 13=累计门外命中数（窗过期后
//     被攻击次数——窗边界面）15=跳过原因（0=无/1=门闭不武装）
//
// ══ 变异测试（权威轮 build/k487-mutations/，底本 SHA 见其 baseline.sha；
//   变异驱动删 .o 强编 + SHA 双守卫——GNU Make 3.81 mtime 整数秒截断真根因，
//   K468 实证；底本与还原同法锚定 K481）══ 2026-11-23 续作轮 12 变体实录：
//   M1 先制加成摘除→T8742.b/T8743 红｜M2 门 <→<=（恰 40% 开）→T8742.c 红
//   M3 端侧窗界 >=→>【幸存记档】＝桶自身过期算式（InRounds 折算后
//     registered_round=start/duration=rounds）恰在窗终点丢条目，lambda 端侧
//     检查为防御纵深、无可观测差异面——load-bearing 的起点侧由 M12 补杀
//   M4 回血量 denom→denom+1→T8742.a 红（10000→7500）
//   M5 start=roundCount（顺延断）→T8742.c 红（窗起点/当回合零 tick）
//   M6 798 门恒开→T8745.a 红（无提升不得武装）
//   M7 封顶写摘除→T8745.b 红（超侧恰 200）｜M8 窗界 >=→>→T8746 红
//   M9 快照档→每回合重评→T8747 红（窗内消提升后仍封顶被破坏）
//   M10 代际守卫摘除→T8749.b 红（封顶计数=2；⚠️ 同载体同 cap 封顶写幂等
//     计数不可见——T8749.b 已按 K469 M11 幸存先例改跨载体 36229/35691 差分）
//   M11 697 凭证判读摘除→T8748.a 红（loss>500）｜M12 起点侧守卫摘除→T8742.c 红
//   （cast 当回合伪进 tick）。12 变体 11 杀 + M3 结构冗余记档；证据
//   build/k487-mutations/（run_mutations.sh 驱动 + 逐变体 apply/diff/SHA/rc 证据）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k953AuditBase = 1009530000;   // 10^9 + 953×10^4（soul 953 无实现免拆）
constexpr int k798AuditBase = 1007980100;   // 10^9 + 798×10^4 + 0100 子块（soul 798=K245 拆分）

inline int audit_key_of(int base, int owner, int slot) { return base + owner * 16 + slot; }

int get_audit(const BattleContext* ctx, int base, int owner, int slot, int def = 0) {
    const auto it = ctx->plugin_storage.find(audit_key_of(base, owner, slot));
    return it == ctx->plugin_storage.end() ? def : any_int_or(it->second, def);
}

void set_audit(BattleContext* ctx, int base, int owner, int slot, int value) {
    ctx->plugin_storage[audit_key_of(base, owner, slot)] = value;
}

void add_audit(BattleContext* ctx, int base, int owner, int slot, int delta) {
    set_audit(ctx, base, owner, slot, get_audit(ctx, base, owner, slot) + delta);
}

// "对手处于能力提升状态"在位判读（K289 659/K481 728 直系：任一能力等级 >0，
// 六槽含命中）。
bool k487_has_boost(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

int k487_boost_slot_count(const BattleContext* ctx, int side) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            ++n;
        }
    }
    return n;
}

// ═══════════════════════════════════════════════════════════════════════════
// 953 —— {0}回合恢复最大体力的1/{1}，体力低于{2}%时先制+{3}
// ═══════════════════════════════════════════════════════════════════════════

// 回合扣减点回血 tick（762 直系 + K473 相对区间守卫；payload=[self, opp, {1}=分母,
// start=窗起点, rounds=窗长]）。⚠️ 桶机制 isExpired 只有过期终门、无"起点前不生效"
// 门（registered_round 只喂过期算式）→ 后手顺延窗的 cast 当回合伪进由本守卫挡
// （K462/K473 同款；门在一切副作用之前：被跳过 tick 零审计零副作用）。
EffectResult k487_953_regen_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int denom = args.int_args[2];
    const int start = args.int_args[3];
    const int rounds = args.int_args[4];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount < start || ctx->roundCount >= start + rounds) {
        return EffectResult::kOk;   // 窗外（含后手 cast 当回合伪进/窗后过期）
    }
    set_audit(ctx, k953AuditBase, self, 12, ctx->roundCount);
    if (ctx->getPet(self).hp <= 0) {
        return EffectResult::kOk;   // 已阵亡不结算（762 同款）
    }
    // {1} 在位即消费：<=0 = 回血子句关闸（不尝试不报故障，K314/K340 纪律）。
    if (denom <= 0) {
        set_audit(ctx, k953AuditBase, self, 15, 2);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->heal_amount) {
        set_audit(ctx, k953AuditBase, self, 15, 1);
        return EffectResult::kOk;
    }
    const int hp_before = ctx->getPet(self).hp;
    g_core->heal_amount(ctx, self, max_hp_of(ctx, self) / denom);
    const int actual = ctx->getPet(self).hp - hp_before;
    set_audit(ctx, k953AuditBase, self, 10,
              get_audit(ctx, k953AuditBase, self, 10) + 1);
    set_audit(ctx, k953AuditBase, self, 11, actual);
    set_audit(ctx, k953AuditBase, self, 13, hp_before);
    set_audit(ctx, k953AuditBase, self, 14, ctx->getPet(self).hp);
    return EffectResult::kOk;
}

// 用技期武装 tick（SKILL_EFFECT 桶；539 双半直系——选择期 body 自挂的当回合节点）。
// payload=[self, opp, {0}=回合, {1}=分母]。
EffectResult k487_953_arm_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 开窗：回合扣减点 InRounds {0} 回合（先手当回合即结算第 1 次、后手顺延）。
    // {0}<=0 关闸档在 route body 已拦（不装 arm 节点）；重施放 = 同键覆盖刷新。
    // start 自算一遍进 payload（regen_tick 相对守卫用；与 register_round_effect
    // 内部折算同源同值——纯函数 owner/roundCount/preemptive_right）。
    const int start =
        ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    (void)register_round_effect(ctx, State::BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS,
                                self, /*effect_id=*/953, rounds,
                                {self, 1 - self, denom, start, rounds},
                                &k487_953_regen_tick);
    set_audit(ctx, k953AuditBase, self, 9, start);
    return EffectResult::kOk;
}

// ── 953 选择期路由体（MOVE_RIGHT 桶；行参 [owner,target,{0},{1},{2},{3}]，
//   args_num=4 + 前缀）──
// ①先制半：门开 → preemptive_level += {3}；②回血半：无条件自挂当回合
// SKILL_EFFECT 桶武装节点（路由 continue 吞普通注册 → 539 直系）。
EffectResult effect_953_regen_window_lowhp_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 953, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 953, args,
                     "953 布局=[owner,target,回合,分母,%,先制量]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int pct = args.int_args[4];
    const int prio = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 953,
                     args, "需要 owner/target ∈ {0,1} 且相异");
    set_audit(ctx, k953AuditBase, self, 0, rounds);
    set_audit(ctx, k953AuditBase, self, 1, denom);
    set_audit(ctx, k953AuditBase, self, 2, pct);
    set_audit(ctx, k953AuditBase, self, 3, prio);
    // {0} 在位即消费：显式 0 = 窗关闸档（不武装不报故障；负值 = 布局错误走 GUARD 面
    // 由运行时 report_effect_arg_fault 上报）。
    if (rounds <= 0) {
        set_audit(ctx, k953AuditBase, self, 15, 3);
    } else {
        // ②回血半：FIRST/SECOND 双桶武装节点（桶只跑行动方——K473/K468 教训同构；
        // rounds=1 本回合，技能真实执行趟才武装开窗）。
        for (const State st :
             {State::BATTLE_FIRST_SKILL_EFFECT, State::BATTLE_SECOND_SKILL_EFFECT}) {
            (void)register_round_effect(ctx, st, self, /*effect_id=*/953, /*rounds=*/1,
                                        {self, opp, rounds, denom}, &k487_953_arm_tick);
        }
    }
    // ①先制半：严格低于 {2}%（整数直乘无截断；恰 {2}% → 门闭，454/1397/K469 口径）。
    const bool open = ctx->getPet(self).hp * 100 < max_hp_of(ctx, self) * pct;
    set_audit(ctx, k953AuditBase, self, 4, open ? 1 : 0);
    set_audit(ctx, k953AuditBase, self, 7, ctx->roundCount);
    add_audit(ctx, k953AuditBase, self, 5, 1);
    if (!open) {
        return EffectResult::kOk;
    }
    add_audit(ctx, k953AuditBase, self, 6, 1);
    ctx->ws.preemptive_level[self] += prio;
    set_audit(ctx, k953AuditBase, self, 8, ctx->ws.preemptive_level[self]);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 798 —— 若对手处于能力提升状态，则对手{0}回合内造成的伤害不超过{1}
// ═══════════════════════════════════════════════════════════════════════════

// CAP 相位回调（K1 766 直系 + K466 1834 代际纪律）。
void k487_798_cap(BattleContext* c, int bucket_owner, int self, int opp, int cap,
                  int start, int expire, int generation) {
    if (!c || bucket_owner != self) {
        return;   // 红伤管线桶按承受方（owner=defender）配对——只管自己承伤
    }
    // 代际守卫：重施放后旧条目自灭（K466 1834 直系；桶条目本身 ON_STAGE
    // valid_id 换宠已作废，同 epoch 重授时代际不匹配者不得再写封顶）。
    if (get_audit(c, k798AuditBase, self, 10) != generation) {
        return;
    }
    DamageSnapshot& d = c->resolvedDamage;
    if (d.attackerId != opp || !d.isRed || d.final <= 0) {
        return;   // 只拦对手打出的红伤（攻击红伤 + 属性直伤 isRed 面——766 口径）
    }
    if (c->roundCount < start || c->roundCount >= expire) {
        add_audit(c, k798AuditBase, self, 13, 1);   // 窗外命中（含窗过期后）
        return;
    }
    // 697「无视伤害限制」凭证穿锁伤（K1 766 纪律；属性路径 pierce_limit 已在
    // 调用侧踢出 CAP 相——battle_primitives.cpp deal_attribute_damage）。
    if (c->ws.attack_credential[opp].ignore_damage_limit) {
        add_audit(c, k798AuditBase, self, 9, 1);
        return;
    }
    set_audit(c, k798AuditBase, self, 7, d.final);
    if (d.final > cap) {
        d.final = cap;   // 封顶（"造成的伤害不超过{1}"；MITIGATE 不可被挡伤失效无视）
        set_audit(c, k798AuditBase, self, 6, c->roundCount);
        add_audit(c, k798AuditBase, self, 5, 1);
    }
    set_audit(c, k798AuditBase, self, 8, d.final);
}

// ── 798 用技期体（SKILL_EFFECT 桶；行参 [owner,target,{0}=回合,{1}=封顶]，
//   args_num=2 + 前缀）──
EffectResult effect_798_foeboost_damage_cap(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 798, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 798, args,
                     "798 布局=[owner,target,回合,封顶]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int cap = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 798,
                     args, "需要 owner/target ∈ {0,1} 且相异");
    set_audit(ctx, k798AuditBase, self, 0, rounds);
    set_audit(ctx, k798AuditBase, self, 1, cap);
    // {0}/{1} 在位即消费：显式 0 合法档（rounds=0 窗空转/封顶 0 全反档=受击归零，
    // 1834 cap=0 载体同形）；负值 = 布局错误走 GUARD 面。
    if (rounds < 0 || cap < 0) {
        set_audit(ctx, k798AuditBase, self, 15, 2);
        return EffectResult::kOk;
    }
    // 门 = 对手处于能力提升状态（任一正槽，六槽含命中）——施放时点在位判读
    // （武装快照档，见文件头甄别三线）。门闭不武装（负测腿）。
    const bool gate = k487_has_boost(ctx, opp);
    set_audit(ctx, k798AuditBase, self, 2, gate ? 1 : 0);
    set_audit(ctx, k798AuditBase, self, 3, k487_boost_slot_count(ctx, opp));
    if (!gate) {
        set_audit(ctx, k798AuditBase, self, 15, 1);
        return EffectResult::kOk;
    }
    // 武装：代际递增（重施放刷新不叠加）+ InRounds 窗起点（后手顺延，
    // common.h:195 契约——766 未折算旧形态记档）。
    const int generation = get_audit(ctx, k798AuditBase, self, 10) + 1;
    set_audit(ctx, k798AuditBase, self, 10, generation);
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    set_audit(ctx, k798AuditBase, self, 4, start);
    set_audit(ctx, k798AuditBase, self, 12, start + rounds);
    add_audit(ctx, k798AuditBase, self, 11, 1);
    ctx->register_damage_effect(
        DamagePhase::CAP, self, DamageEffectCategory::MITIGATE,
        [self, opp, cap, start, expire = start + rounds, generation](BattleContext* c,
                                                                     int bucket_owner) {
            k487_798_cap(c, bucket_owner, self, opp, cap, start, expire, generation);
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(953, effect_953_regen_window_lowhp_priority)
SKILL_EFFECT(798, effect_798_foeboost_damage_cap)
