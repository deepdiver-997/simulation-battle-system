// K499 · 自身强化翻倍姊妹双族 —— 537×5 + 653×5（2026-11-23，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k499，分支 feat/k499，基点 a7c6cf0a；
// 场景 test/scenario/scenario_8870_k499.cpp，号段 8870-8879）
//
// ══ 家族语义（effect_info 官方模板 + 官方载体文案逐位互证，unity 库现查 2026-11-23）══
//   537（args_num=6）"自身攻击{0}，若对手处于能力提升状态则强化效果翻倍"
//     ——模板只写首个槽位，{0}..{5} = **官方渲染序六槽 flat 数组**（攻/防/特攻/
//     特防/速/命中 各自等级变化量，0=不变；与 K475 546 的 {1}..{6} 同构——546
//     模板同样只写首槽、余参省写）。载体互证：24897 邪心乱舞"全属性都提升了，
//     对手处于能力提升状态时强化翻倍"（[1,1,1,1,1,1]=六槽全 +1 逐位咬合）、
//     24132/24823/27830 同文全 +1、23535 不屈龙魂 [0,1,1,1,1,1]"特防、特攻、
//     速度、防御、命中上升了"（槽0=攻=0，未提及攻击——首槽 0 与文案互证）。
//     26987 魔界菌株 [1,1,0,0,0,0] 文案"使自身攻击+2、防御+1"——**量不符**
//     （行参攻=1、防=1）：4399 info 列转述陈旧/编辑口径，DB 行参权威（K475
//     15242 火舞狂杀"4399 攻-1 系编辑错误"同判据记档），实现与场景均按行参。
//   653（args_num=6）"{0}，若对手处于能力下降状态，强化效果翻倍"——同构姊妹，
//     {0}..{5} 同为官方序六槽 flat。门对偶：537 读**提升**、653 读**下降**。
//   「强化效果翻倍」读面甄别（官方无实测，按载体上下文定案记档）：
//     **施加量翻倍**（门开时各槽施加 delta = 行参×2，如 +1→+2），
//     否证"变化量翻倍"（把既成强化等级×2）读法——载体句法"全属性都提升了，
//     对手处于能力提升状态时强化翻倍"中"强化"回指**本招施加的强化**（分句
//     紧邻自升句，若指既成等级会写成"自身强化等级翻倍"类表述）；在树先例
//     707"恢复效果翻倍"/2171(K490)"效果翻倍"均为施加量×2。施加走 stat_change
//     原生通道，越界拒写/封锁语义由内核自理（本招 delta∈{0,1}、翻倍后 {0,2}，
//     无钳写歧义）。
//   条件门读法（**存储面** ability_levels）：
//     537 = 对手六槽（含命中）任一 **>0**（K431 184 直系同判据
//     ——k431_duishou_tisheng_pool.cpp has_any_positive_level；K459 1804 读面
//     先例：1767 类"视为"只改 view_levels，读存储不读视图）；
//     653 = 对手六槽任一 **<0**（K490 2171 存储面负槽直系——门为布尔判，
//     "负槽和>0"≡"任一负槽"，两表述等价）。
//     两门互为镜像：提升不开下降门、下降不开提升门（场景镜像差分腿锁死）。
//     ⚠️ 存储面 vs 视图面的**行为判别在本族不可达**（N/A 记档，2026-11-23 现查）：
//     视图在回合起点自本体重基（battleFsm.cpp sync_workspace_from_on_stage：
//     view_levels := ability_levels），而现役视图改写族（1767 翻对手正槽/
//     1874·1750 翻自身负槽/j_negative_view_rewrite.cpp）的改写全部被 AFTER_ACTION
//     探针还原在**持有者自己的行动窗内**——本族门读的是对手，等自己出手时对手
//     视图已复原；自身载体链（537×5/653×5 十链现查）无视图改写兄弟。故
//     "门读视图"变异在全部可达状态上与读存储外延等价，无行为红点（K475
//     "按适用面 N/A 记档，不硬造"判例）。选存储面是**语义面**决策：权威平面、
//     先例一致，且对将来越窗存续的视图改写效果天然免疫。
//
// ══ 载体切片（unity 库 SQL 现查 2026-11-23；side_effect.arg_count=6 与
//   effect_info.args_num=6 两表一致，10 载体顺序切分零剩余；实现读行参不硬
//   编码，场景以实际加载切片逐位锁定）══
//   537 [{0}..{5}=官方序六槽]（5 载体）：
//     22768 末日宣战 [537]|[1,1,0,1,1,0]（**纯链**），
//     24897 邪心乱舞 [537,57,695]|[1,1,1,1,1,1, 4,3, 3,50]，
//     25302 圣诞彩宴 [537,707,843]|[1,1,1,1,1,1, 4,3,2, 2,2]，
//     25321 天蔚之魂 [537,707,843]|[1,1,1,1,1,1, 4,3,2, 2,1]，
//     27830 白霞逐夜 [537,532,1612]|[1,1,1,1,1,1, 4,3, 3,300,100,5,200]。
//   653 [{0}..{5}=官方序六槽]（5 载体）：
//     23535 不屈龙魂 [653]|[0,1,1,1,1,1]（**纯链**），
//     24132 光境幻生 [653,57,48]|[1,1,1,1,1,1, 5,3, 5]，
//     24823 天心唯仁 [653,615,776]|[1,1,1,1,1,1, 3,250,2, 2]，
//     26987 魔界菌株 [653,57,620]|[1,1,0,0,0,0, 3,3, 3,5]，
//     29034 永生囚徒 [653,1067,571,843]|[1,1,1,1,0,1, 4,2,2,2, 5,1000, 2,2]。
//   ⚠️ 官方序 [攻,防,特攻,特防,速,命中] → 引擎槽序 {0,2,1,3,4,5}（kStatMap，
//     stat_dispel.cpp effect 585 同表；K475 546/K498 518 直系）。本族现役载体
//     的 flat 图样对"引擎序直读"多数不敏感（全 1 档），但 22768 [1,1,0,1,1,0]
//     与 26987 [1,1,0,0,0,0] 在防/特攻两位**可判别**（引擎序直读会把防写到特攻
//     槽）——场景逐位断言锁死映射（M3 变异判别面）。
//
// ══ 实现口径（SKILL_EFFECT 时点同步执行；无路由/无内核改动；无窗/无票/
//   无 watcher——门在效果执行时现判，门闭整段 no-op，门开按槽施加）══
//   1. 六参连读：int_args = [owner, opp, {0}..{5}]（args_num=6 + loader
//     owner/target 前缀）。逐槽 map 到引擎槽后按方向选通道（内核既定通道）：
//     - delta>0 = 自身强化 → stat_change(ctx, self, slot, delta×2 或 delta)
//       （STAT_BOOST 封锁/AT_CAP 拒写在原语内自理，battle_primitives.cpp:1462+）；
//     - delta<0 = 自身下降 → stat_drop(ctx, self, slot, -delta, actor=self)
//       （现役载体无负档——防御性分支，官方语义"强化"以正档为主，负档若
//       出现按弱化原语走，不私造通道；K475 k475_apply_level_delta 同构）；
//     - delta==0 = 不调用（stat_change"值没变不写不 emit"档，零位天然 no-op）。
//   2. 无概率行参：模板无 % 字面 → 不走申报制，零 rand 消费。
//   3. 门开施加翻倍 = 行参×2 一次调用（非两次调用——事件面单条
//      EVENT_STAT_CHANGED/槽，与"强化效果翻倍"单效果语义一致）。
//
// ══ 生命周期 ══
//   - 无锚无票无 watcher：施加面 = ability_levels[self]（**on-stage 作用域**，
//     换宠/开战由内核 clear_ability_levels 归零——离场清理结构性成立，场景
//     以"换宠后等级归零"腿锁定）。
//   - 重施放 = 再用载体技：门现判现施加（门开两回合连用逐次翻倍叠加，
//     stat_change 原语内 AT_CAP 拒写兜底），无去重无 once 面。
//   - 一次性效果（无 duration 无驻留显示态，显示 gate 规范 §1.1）：无描述
//     义务、无 fullstate 条目断言义务；可见性由 EVENT_STAT_CHANGED/等级承载。
//
// ══ plugin_storage：**零键**（K498 同款决断）══
//   本族全部断言可行为化（施加结果=ability_levels 逐位、翻倍差分=门开关两局
//   对照、AT_CAP/BLOCKED=内核原语返回面），无需审计槽。候选键段甄别现查
//   （2026-11-23）：10^9+537×10^4=1005370000 / 10^9+653×10^4=1065300000 全库
//   grep 零占用；**soul 同号甄别**——soul 653 已实现（k324_cut_or_heal_dual.cpp
//   SOUL_PROGRAM(653)，registry soul=417 含 653），若将来需键段须按 K469 子块
//   先例取 +0100（1005370100/1065300100）；soul 537 无实现无数据（registry
//   soul 无 537）。本单不落键，仅记档防后续撞段。
//
// ══ 动工前置三查（2026-11-23，k499 worktree 现查）══
//   ①内核 grep 537/653（裸 id 与注册宏）src/ include/ **零命中**；parser
//     （effect_unit_parser.cpp）无"能力提升/下降条件"句式覆盖——真缺口。
//   ②effect_info 537/653 均在列 args_num=6，精确模板全表唯一（LIKE 近亲无同
//     文）；side_effect.arg_count 两表一致=6；10 载体切片零剩余；
//     friend_side_effect 全空；custom_effect_programs/overrides 两表零行。
//   ③registry 基线 moves=1619/soul=417/suit=4（build/registry.json 现导），
//     moves 无两 id、soul 653 在列（k324，他空间无冲突——moves 616×soul 616
//     K469/K303 存量先例）；全库 resources/test grep SKILL_EFFECT(537/653 零
//     命中、兄弟 worktree 同 grep 零命中；docs_local/tasklog 无拆分迁移与
//     故意退役记录。
// 先例：K431 184/418（对手提升条件族：存储面任一正等级门 + 官方码槽位映射 +
//   审计段布局直系）、K475 546（官方渲染序六槽 flat + kStatMap + 行参权威判
//   例）、K490 2171（存储面负槽读法 + 翻倍=施加量×2 判例）、K498 518（零
//   plugin_storage 决断 + 文件头三查记档格式直系）、stat_dispel 585（kStatMap
//   原表）。
// 场景回归：test/scenario/scenario_8870_k499.cpp（号段 8870-8879）。

#include "common.h"

namespace {

// ── 官方渲染序 → 引擎槽序（kStatMap：stat_dispel.cpp effect 585 同表；
//   K475 546 / K498 518 直系）──
// 官方：0攻 1防 2特攻 3特防 4速 5命中；引擎 stat 域：0攻 1特攻 2防 3特防 4速 5命中。
constexpr int kOfficialToEngineSlot[6] = {0, 2, 1, 3, 4, 5};

// ── 537 门：对手处于能力提升状态 = 对手本体六槽（含命中）任一 **正** 等级
//   （存储面直读——K431 184 has_any_positive_level 同判据；K459 1804 先例：
//   读存储不读视图）──
bool k499_opp_any_positive(const BattleContext* ctx, int opp) {
    if (!ctx || opp < 0 || opp > 1) {
        return false;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] > 0) {
            return true;
        }
    }
    return false;
}

// ── 653 门：对手处于能力下降状态 = 对手本体六槽（含命中）任一 **负** 等级
//   （存储面直读——K490 2171 存储面负槽直系；布尔判下"负槽和>0"≡"任一负槽"）──
bool k499_opp_any_negative(const BattleContext* ctx, int opp) {
    if (!ctx || opp < 0 || opp > 1) {
        return false;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            return true;
        }
    }
    return false;
}

// ── 按方向选通道（内核既定通道，不私造）：正=stat_change（自身强化：STAT_BOOST
//   封锁/AT_CAP 拒写在原语内）；负=stat_drop（钳 -6/附加禁令在原语内）；
//   0=不调用（stat_change"值没变不写不 emit"档）。返回结果码；-1 = delta0 未调用。
int k499_apply_level_delta(BattleContext* ctx, int target, int engine_slot, int delta,
                           int actor) {
    if (delta == 0) {
        return -1;
    }
    if (delta < 0) {
        if (g_core && g_core->stat_drop) {
            return static_cast<int>(g_core->stat_drop(ctx, target, engine_slot, -delta, actor));
        }
        return -1;
    }
    if (g_core && g_core->stat_change) {
        return static_cast<int>(g_core->stat_change(ctx, target, engine_slot, delta));
    }
    return -1;
}

// ── 双族公共执行体：门谓词注入（537=任一正 / 653=任一负），其余全同构 ──
// int_args = [owner, opp, {0}..{5}]（官方序六槽 flat + loader 前缀）。
EffectResult k499_execute_doubling_boost(BattleContext* ctx, const EffectArgs& args,
                                         int effect_id, bool (*gate)(const BattleContext*, int)) {
    EFFECT_ARG_GUARD(ctx != nullptr, effect_id, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, effect_id, args,
                     "布局=[owner,opp,官方序攻,防,特攻,特防,速,命中]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     effect_id, args, "需要 owner/opp ∈ {0,1}");
    if (!g_core || (!g_core->stat_change && !g_core->stat_drop)) {
        return EffectResult::kOk;
    }

    // 条件门：对手能力状态（存储面）。
    const bool open = gate(ctx, opp);
    const int factor = open ? 2 : 1;   // 翻倍=施加量×2（读面甄别见文件头）
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[2 + i] * factor;
        const int engine_slot = kOfficialToEngineSlot[i];
        (void)k499_apply_level_delta(ctx, self, engine_slot, delta, self);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 537 —— 自身攻击{0}（+省写 {1}..{5} 官方序六槽），对手处于能力提升状态则
//   强化效果翻倍（施加量×2）──
EffectResult effect_537_opp_boost_doubling_boost(BattleContext* ctx, const EffectArgs& args) {
    return k499_execute_doubling_boost(ctx, args, 537, &k499_opp_any_positive);
}

// ── 653 —— {0}（+省写 {1}..{5} 官方序六槽），对手处于能力下降状态则强化效果
//   翻倍（施加量×2）——537 的镜像姊妹 ──
EffectResult effect_653_opp_drop_doubling_boost(BattleContext* ctx, const EffectArgs& args) {
    return k499_execute_doubling_boost(ctx, args, 653, &k499_opp_any_negative);
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(537, effect_537_opp_boost_doubling_boost)
SKILL_EFFECT(653, effect_653_opp_drop_doubling_boost)
