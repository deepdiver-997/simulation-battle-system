// K440 · 探索七批自选三族 —— 75「命中时有30%概率令对手麻痹、睡眠或害怕」×6 +
// 737「{0}%使对手{1}，若没有触发，则回合结束时减少对手1/{2}最大体力」×6 +
// 734「若对手不处于能力提升或下降状态时则恢复{0}点体力」×6
//（场景 test/scenario/scenario_8280_k440_rand_cond_heal.cpp；号段 8280-8289 一文件
//  覆盖三族，scenario_8270 同式；2026-11-09，基线 main-local = feat/k439 合并点
//  d01eff02，registry moves=1495 / soul=411）
//
// ══ registry 基线重对账（K439 头注起算派发口径 802 族/1900 引用）══
// 方法 = K438/K439 同款 **json_each 精确元素匹配 × 主检出 DB + worktree DB 快照双库
// 交叉 × 纯 Python 全表 JSON 解析**（脚本 /tmp/k440_inventory.py，2026-11-09 现查）：
//   主仓 ~/code/c++/project/sbs-local/scripts/data/processed/seer_unity.sqlite 与
//   worktree 拷贝 md5 同源 fc4f7508c08c207e1afa2a6cd5d027e0（两份物理文件独立读，
//   载体/槽位双口径逐 id 全等）。基线 ./build/sim_server --dump-registry-exit 实测
//   moves=1495 / soul=411（与派发"registry 1495 基线"一致）。
// ⚠️ 对账勘误（头注留痕）：现库盘点 = **801 族 / 1887 载体（1894 槽位）**，与派发
//   结转账 802 族 / 1900 引用差 1 族 / 13 载体——派发账系 K439 期旧库（md5 bc8889ff）
//   账面算术结转（807-5 / 1930-30），而现库已被管线重建（mtime 2026-11-09 03:09）。
//   漂移逐族可对：75 5→6 / 1559 3→6 / 1930 1→6 / 2128 3→6 / 2166 5→6（五族较
//   K434/K437 头注旧账合计 **+13 载体，与引用差额精确咬合**）；-1 族为旧库有而新库
//   清零的族（旧快照已不在，无由逐族指认，记档）。本批起算口径 = 现库 801 族/1887
//   载体；1459/1575 亦按现库 ×7（K437 头注 ×5 为旧账，K433 计数与本批一致）。
// 被引用效果 id 2294（K430/K433/K434 记档一致）；friend-only 独有 id 0 个。
//
// ══ 选族与逐族裁决（候选池 = 现库载体 ≥5 的 66 族，自选 ≤6 载体甜点区）══
// 排除判据沿 K430/K433/K434/K437/K438/K439 全部头注排除清单 + 纠错记档：
//   - 2190×13 哑模板 / 1538×8 内核缺口 / 445×7 经济类 / 41×7 无 effect_info 行
//     （现查 args_num=NULL、info 空，四查②不过）/ 1565×7 随机 PP——不翻案。
//   - 随机类沿派发口径：attach_random_anomalies 通道已解锁，非该通道随机（1565
//     随机 PP、1494/1457 随机 PP 归零腿）仍排除。
//   - K433 7 载体遗留逐条复核不变：2028×7（天敌+必中跨两通道，K434 判据不变）/
//     1459×7、1575×7（先后手门+技能无效通道体量大）/ 1645×7、1855×7（双窗复合
//     语义待拆）/ 1799×7、1863×7（K433 现查纯链 2/7、1/7）/ 1627×7（随机攻击次数
//     +秒杀通道）/ 2075×7（"未击败"门需击败通道，K433 体量记档）。
//   - K434/K437 记档不变：91×6 状态镜像无一等公民 / 546×6 七参槽位编码需专查 /
//     1930×6 先制连击 multiline / 1559×6 ignore_pp 通道待建 / 2027×6 击败跨宠 PP /
//     2128×6 heal-reduction 通道待查 / 2166×6 击败通道+混链 / 2100×6 护盾门（现查
//     混链 2/6：2101、2166 未注册，且"护盾状态"查询面未过三查，留通道设计单）/
//     2226×6（混链 2/6 + 免疫攻击票 counts 语义需专查）/ 1938×6（纯链 2/6）/
//     1150×6（全纯但模板尾句截断「消除成功对手{0}」args_num=6 六槽编码需 BWIKI
//     逐载体互证，K434 546 同判据）/ 1250×6、2233×5、1629×6、941×6（通道可做——
//     EVENT 窗/速度门解码体量超本批，留下一班候选）。
//   - **674×6 排除记档（本批复核不过，防复踏）**：模板「攻击结束时恢复{0}点体力」
//     无门；4399 文六载体（16620/16996/17519/17721/19094/36361）**全部**带「（若当
//     回合）未击败对手」门——恢复量六处与切片 [100/100/200/150/250/350] 逐条吻合、
//     但门子句为质变差异（击杀回合回不回血），且 6/6 系统性偏离非孤例（K439 30484
//     判例不适用）。落地该门需 AFTER_ACTION+击败判定换通道（J50 644 形），与模板
//     字面冲突——按"复核不过如实排除记档，不硬落地"处理，待官方实测仲裁。
// 【落地 ×3】75 / 737 / 734 —— 全 ×6 载体、切片全自洽（见下）、通道全备零新增：
//   · 75 = K437 头注明文"下一班低垂果实"（"通道同款可做（零参、池{麻痹0,睡眠8,
//     害怕6}、概率 30 写死文案）"），K438/K439 未取、现库 ×6。全纯 6/6。
//   · 737 = 概率申报（K422 917 apply_anomaly_checked 直系）+ 未触发兜底
//     （J50 644「当回合未击败对手，则减少对手当前体力1/{0}」AFTER_ACTION 单发
//     同形——两件都是既有一等公民）。全纯 6/6。
//   · 734 = 条件门（对手净槽：无提升且无下降）→ 恢复{0}点体力（heal_amount 原语；
//     K433 1339「若对手不处于异常状态」条件门判据同款）。纯链 5/6（唯一混链
//     17129 链上 735「对手处于能力提升则下回合先制+1」未注册——宏按 id 注册非按
//     载体，其断言面由五条纯链载体覆盖，K438 1624 混链记档同口径）。
//
// ══ 四查（2026-11-09 本 worktree）══
// ① 内核硬编码：(^|[^0-9.])(75|734|737)([^0-9.]|$) 对 src/ include/ grep rc=1 零命中
//   （75 为两位数，边界 grep 双写：(^|[^0-9.])75([^0-9.]|$) 与 skill/effect 上下文
//   各查一遍，零命中）。
// ② effect_info 三 id 在列且 args_num = side_effect.arg_count（75=0 / 734=1 / 737=3）；
//   custom_effect_programs / custom_effect_overrides 三 id 0 命中。
// ③ 全库并集无注册：resources/ test/ `SKILL_EFFECT((75|734|737),` 零命中（grep rc=1）；
//   git grep 对 main-local/main/feat/k437/feat/k438/feat/k439/feat/j337 与兄弟
//   worktree（agents/night-fuzz、endgame-walk）同款零命中——真缺口。75 在 K437
//   头注仅为"留下一班低垂果实"推介，非注册面占用。
// ④ parser 覆盖面（冻结不扩，只记录）：75/734/737 三模板均不含「令/使+{状态}」形
//   与五类条件形 → parse_effect_unit 纯惰性；注册落地后 clone_effect 命中插件函数
//   （src/entities/skills.cpp:675 只在 effect.logic 为空时走 parser）。
//
// ══ 载体切片（side_effect.arg_count 平面切片，脚本核算零剩余零串位）══
//   75 []：11138 七色光芒[75]特75 / 12932 混沌声波[75]特130 / 13060 战羊气刃[75]物120 /
//     15480 扰乱侵袭[74,75]物70 / 34658 异能冲撞[74,75]物130 / 35057 和光斩[75,188]物85
//     ——零参宏，量写死模板文案（§7.5 口径：P=30、池三元素，插件写死+头注注明）。
//   734 [n]：17129 地魔穿心箭[734,735][150] / 30267 踏飒流星[734][200] /
//     30672 拈花成剑[734][200] / 31348 灵动之击[734][200] / 31990 碎岩角[734][200] /
//     38292 明洲祛秽[734,1126][200]。
//   737 [pct,code,denom]：17133 诛仙灭神斩[737][30,8,5] / 17269 龙魂焚火咒[737][50,2,4] /
//     17810 诛天断尘斩[737,687][50,8,4] / 18065 雷神龙魂灭[737][30,0,5] /
//     19454 绯红破空杀[737,903][50,8,4] / 32576 失神咒念波[945,737][40,8,4]。
// 4399 文本（reference/scrape_4399/out/pets_full.jsonl，md5 同源
// 03a7cbe48f69b9d188c274a20b333eaf）18 载体逐条核对：
//   · 737 六条全部与模板同义（「N%对手X，若没有触发X，则回合结束时减少对手1/D最大
//     HP/体力」逐参吻合；链上兄弟子句各自在册）——6/6 过。
//   · 734 六条全部「若对手不处于能力提升或（/或者）下降状态则恢复 N 点体力」——
//     6/6 过（混链兄弟子句另计）。
//   · 75 三条与模板同义（11138「命中时有30%几率令对手麻痹、睡眠或害怕」逐字；12932/
//     13060「30%令对方麻痹、睡眠或害怕」同义）；⚠️ 三条变体记档（按模板落地，
//     宏按 id 注册非按载体）：15480「30%概率出现中毒、烧伤、冻伤、麻痹、睡眠、害怕」
//     （六池变体）、34658/35057「10%麻痹、10%睡眠、10%害怕」（逐种独立 10% 变体）——
//     池构成/概率值为量变，模板三元素池+30% 申报为权威（K439 30484/K438 31439
//     孤例裁决口径）。35057 链上 188=「对手处于异常状态则威力翻倍」（e_predator_
//     cond_amp.cpp:103 在册）对应 4399 文尾句；其「先制+3」子句无链上载体，系
//     4399 页面聚合文，不属 75 宏语义。
//
// ══ 实现形态（同形套路照抄先例，通道零新增）══
//   · 时点 = SKILL_EFFECT（effect_register_state default；miss/被盔桶不跑——75「命中
//     时」、734/737 攻击技上效同此口径，640/1339/K422 917 同款）。
//   · 【75】attach_random_anomalies(ctx, opp, {麻痹0,睡眠8,害怕6}, count=1, P=30,
//     owner, ChanceSource::Skill)——AbnormalStatusId 枚举直查 abnormal-types.h:17/23/25
//     （K439 1498 命名四元素池同法）；无行参 → P=30 插件写死（§7.5 零参宏口径）。
//     申报概率走亮节闸门管辖（原语内掷骰前改写，battle_primitives.cpp 判定点）。
//     无兜底子句 → landed==0 什么都不做（审计槽显式清账可断）。
//   · 【737】apply_anomaly_checked(ctx, opp, {1}码, random_anomaly_duration()=2,
//     owner, {0}%, ChanceSource::Skill)（battle_primitives.h:67 常量 2 回合——模板无
//     时长子句，917 同口径）。「触发」= 四档落地账 SUCCESS/REPLACED_EXISTING/
//     DURATION_EXTENDED/CONVERTED（battle_primitives.h:28-37；K439 1498 landed 口径
//     同源），ROLL_FAILED/免疫/抗性/弹控/目标已倒 = 未触发 → 兜底：AFTER_ACTION
//     双侧单发注册（J50 j50_register_after_action 同式——register_round_effect
//     rounds=1 InRounds 含当回合、桶只跑行动方 52 教训）→ tick 未击败门
//     （opp.hp>0，J50 934 存活镜像）→ fixed_damage(ctx, opp, max_hp/{2})
//     （CoreApi:103；量=**对手**最大体力 1/{2}，numericalBase[HP] 直读——
//     canglan_nutao_skills.cpp:31 同式）。644 的"当前体力1/{0}"与 737 的"最大体力
//     1/{2}"系逐字差异，不共用分母口径。
//   · 【734】门 = 对手六槽 ability_levels 全零（kAbilityLevelSlotCount=6 含命中——
//     「能力提升或下降状态」两向净空；K439 1146 提升态计数同表遍历、K434 1325 负槽
//     计数同法取反）。门开 → heal_amount(ctx, owner, {0})（CoreApi:70，吃封回血+
//     恢复修正%，返回 SUCCESS/INVALID_PARAM——heal 实绩读 hp 前后差落审计）；门闭
//     → 不恢复（审计显式清账）。
//   · 生存门（K422 2295 / K435 130 / K438/K439 口径）：目标已倒 → 整条跳过（三宏
//     同门，审计槽 9=1；734 虽回自身血，门读对手态、对手已倒无意义，同口径跳过）。
//   · 审计槽布局（K437/K438/K439 同构：10^9 + id×10^4 + owner×16 + slot；新增前
//     tools/check_storage_keys.py 全库无撞 rc=0）：
//     75（1000750000）：slot 0 申报概率（写死 30）/ 1 requested / 2 rolled / 3 landed /
//       4..6 新落异常 id 差集（升序，-1=未用）/ 8 累计使用计数 / 9 跳过原因
//       （0=无/1=目标已倒/2=原语槽缺失）
//     737（1007370000）：slot 0 行参 P / 1 行参 异常码 / 2 行参 分母 /
//       3 触发判定（1=四档落地/0=未触发）/ 4 兜底 armed（1/0）/ 5 兜底 fired（1/0）/
//       6 兜底固伤实参 / 8 累计使用计数 / 9 跳过原因
//     734（1007340000）：slot 0 行参 n / 1 对手提升态种数 / 2 对手下降态种数 /
//       3 门判定（1=净槽开/0=闭）/ 4 heal 实绩（hp 前后差）/ 8 累计使用计数 /
//       9 跳过原因

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k75AuditBase = 1000750000;    // 10^9 + 75×10^4
constexpr int k734AuditBase = 1007340000;   // 10^9 + 734×10^4
constexpr int k737AuditBase = 1007370000;   // 10^9 + 737×10^4

inline int audit_key(int base, int owner, int slot) { return base + owner * 16 + slot; }

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

void bump_use_count(BattleContext* ctx, int base, int owner) {
    set_storage_int_local(ctx, audit_key(base, owner, 8),
                          storage_int_local(ctx, audit_key(base, owner, 8), 0) + 1);
}

// 75 命名三元素池（模板字面：麻痹、睡眠、害怕。K437 851 五元素池 / K439 1498 四元素
// 池同法：AbnormalStatusId 枚举直查，异常码 0/8/6 见 abnormal-types.h:17/25/23）。
std::vector<int> k440_pool_named_three() {
    return {static_cast<int>(AbnormalStatusId::Paralysis),   // 麻痹 0
            static_cast<int>(AbnormalStatusId::Sleep),       // 睡眠 8
            static_cast<int>(AbnormalStatusId::Fear)};       // 害怕 6
}

// AFTER_ACTION 双侧单发注册（J50 j50_register_after_action 同式：InRounds rounds=1
// 含当回合；桶只跑行动方，52 教训）。
void k440_register_after_action(BattleContext* ctx, int owner, int effect_id, int rounds,
                                std::vector<int> payload, EffectFn fn) {
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, effect_id, rounds, payload, fn);
    }
}

// apply 单异常的四档落地账（battle_primitives.h:28-37；K439 1498 landed 口径同源）。
bool k440_anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS ||
           r == ApplyAnomalyResult::REPLACED_EXISTING ||
           r == ApplyAnomalyResult::DURATION_EXTENDED ||
           r == ApplyAnomalyResult::CONVERTED;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 75 —— 命中时有30%概率令对手麻痹、睡眠或害怕
// 零参宏（args_num=0）：概率 30 与三元素池均写死在模板文案 → 插件写死 + 本头注
// 注明（§7.5 口径：先制量写死同款）。行参布局 [owner, opp]（skills.cpp 加前缀）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_75_rand_fear_sleep_paralysis(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 75, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 75, args,
                     "75 布局=[owner,opp]（零参宏）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     75, args, "需要 owner/opp ∈ {0,1}");

    if (!g_core || !g_core->attach_random_anomalies) {
        set_storage_int_local(ctx, audit_key(k75AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    // 生存门：目标已倒 → 整条跳过（K438/K439 口径）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k75AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    constexpr int kDeclaredP = 30;   // 模板文案「30%概率」——零参宏写死（勿改行参）
    bool before[kOfficialAbnormalStatusSlotCount] = {};
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        before[id] = g_core->has_active_anomaly(ctx, opp, id);
    }
    const RandomAnomalyResult r = g_core->attach_random_anomalies(
        ctx, opp, k440_pool_named_three(), /*count=*/1, /*probability_pct=*/kDeclaredP,
        owner, ChanceSource::Skill);
    set_storage_int_local(ctx, audit_key(k75AuditBase, owner, 0), kDeclaredP);
    set_storage_int_local(ctx, audit_key(k75AuditBase, owner, 1), r.requested);
    set_storage_int_local(ctx, audit_key(k75AuditBase, owner, 2), r.rolled);
    set_storage_int_local(ctx, audit_key(k75AuditBase, owner, 3), r.landed);
    for (int s = 4; s <= 6; ++s) {
        set_storage_int_local(ctx, audit_key(k75AuditBase, owner, s), -1);
    }
    int filled = 4;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId && filled <= 6; ++id) {
        if (!before[id] && g_core->has_active_anomaly(ctx, opp, id)) {
            set_storage_int_local(ctx, audit_key(k75AuditBase, owner, filled), id);
            ++filled;
        }
    }
    bump_use_count(ctx, k75AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k75AuditBase, owner, 9), 0);   // 无跳过（显式清账）
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 737 —— {0}%使对手{1}，若没有触发，则回合结束时减少对手1/{2}最大体力
// 行参布局 [owner, opp, P, 异常码, 分母]。触发账 = 四档落地；未触发 → AFTER_ACTION
// 单发兜底（J50 644 同形），未击败门内 fixed_damage = 对手最大体力/分母。
// ═══════════════════════════════════════════════════════════════════════════
namespace {

// 兜底 tick：payload=[owner, opp, denom]。未击败门（J50 934 存活镜像同款）内
// deal 对手最大体力 1/{2} 固定伤害（CoreApi fixed_damage，粉伤路径）。
EffectResult k440_737_fallback_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 未击败门：击杀回合不兜底（644 同口径）
    }
    const int max_hp = ctx->getPet(opp).numericalBase[NumericalPropertyIndex::HP];
    const int amount = max_hp / denom;
    set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 5), 1);
    set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 6), amount);
    if (amount > 0 && g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, opp, amount);
    }
    return EffectResult::kOk;
}

}  // namespace

EffectResult effect_737_anomaly_roundend_fallback(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 737, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 737, args,
                     "737 布局=[owner,opp,P,异常码,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int denom = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     737, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0 && denom > 0, 737, args, "需要 P/分母 > 0");

    if (!g_core || !g_core->apply_anomaly_checked || !g_core->fixed_damage) {
        set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    // 生存门：目标已倒 → 整条跳过（含兜底）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 0), pct);
    set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 1), anomaly);
    set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 2), denom);
    // {1}=battle_effects 异常码（族内现值 8睡眠/2烧伤/0麻痹，registry anomalies 44 码
    // 全覆盖；0 麻痹是合法码位——592 载体 16149 先例，K433 记档）。
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, pct, ChanceSource::Skill);
    const bool landed = k440_anomaly_landed(r);
    set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 3), landed ? 1 : 0);
    if (landed) {
        // 触发档：兜底不 armed（显式清账，可断）。
        set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 4), 0);
        set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 5), 0);
        set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 6), 0);
    } else {
        // 未触发档（掷点失败/免疫/抗性/弹控/目标已倒同分支）→ 本回合结束兜底。
        k440_register_after_action(ctx, owner, 737, /*rounds=*/1, {owner, opp, denom},
                                   &k440_737_fallback_tick);
        set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 4), 1);
        // fired/量显式清账（击杀回合 tick 早退不再写—— armed 分支先清，未 fired 可断 0）。
        set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 5), 0);
        set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 6), 0);
    }
    bump_use_count(ctx, k737AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k737AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 734 —— 若对手不处于能力提升或下降状态时则恢复{0}点体力
// 行参布局 [owner, opp, n]。门 = 对手六槽 ability_levels 全零（两向净空）；门开 →
// heal_amount(owner, n)，实绩 = hp 前后差落审计；门闭 → 不恢复（显式清账）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_734_clean_foe_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 734, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 734, args,
                     "734 布局=[owner,opp,n]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     734, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 734, args, "需要 n > 0");

    if (!g_core || !g_core->heal_amount) {
        set_storage_int_local(ctx, audit_key(k734AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    // 生存门：对手已倒 → 整条跳过（门读对手态，对手已倒无判定意义，K438/K439 同口径）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k734AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k734AuditBase, owner, 0), amount);
    int boost_kinds = 0;
    int drop_kinds = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int lv = ctx->ability_levels[opp][i];
        if (lv > 0) {
            ++boost_kinds;
        } else if (lv < 0) {
            ++drop_kinds;
        }
    }
    set_storage_int_local(ctx, audit_key(k734AuditBase, owner, 1), boost_kinds);
    set_storage_int_local(ctx, audit_key(k734AuditBase, owner, 2), drop_kinds);
    if (boost_kinds != 0 || drop_kinds != 0) {
        // 门闭：对手带任一提升/下降 → 不恢复（显式清账可断）。
        set_storage_int_local(ctx, audit_key(k734AuditBase, owner, 3), 0);
        set_storage_int_local(ctx, audit_key(k734AuditBase, owner, 4), 0);
    } else {
        // 门开：对手净槽 → 恢复{0}点体力（半血观测防 clamp——坑集 §9 口径由场景保证）。
        set_storage_int_local(ctx, audit_key(k734AuditBase, owner, 3), 1);
        const int hp0 = ctx->getPet(owner).hp;
        (void)g_core->heal_amount(ctx, owner, amount);
        set_storage_int_local(ctx, audit_key(k734AuditBase, owner, 4),
                              ctx->getPet(owner).hp - hp0);
    }
    bump_use_count(ctx, k734AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k734AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

SKILL_EFFECT(75, effect_75_rand_fear_sleep_paralysis)
SKILL_EFFECT(737, effect_737_anomaly_roundend_fallback)
SKILL_EFFECT(734, effect_734_clean_foe_heal)
