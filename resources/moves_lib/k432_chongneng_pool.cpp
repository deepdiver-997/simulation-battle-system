// K432 · 效果 42 族 —— 「{0}~{1}回合自己使用电招式伤害x2」充电/电系威力窗 9 载体
//（场景 test/scenario/scenario_8170_chongneng.cpp；号段 8170-8179，
//  2026-11-07，基线 main-local 4457fb7d，registry moves=1475 / soul=411）
//
// ══ 官方语义现查（2026-11-07）══
// ⚠️ effect_info **本地快照无 42 行**（id 缺口 0/21/31/41/42/174，K430 排除清单把
//   41 记作「effect_info 无行＝无模板权威，四查②不过」——42 在该口径下同病）。
//   本单**不按零产码收单**的新证据链（SOP §六.3「记档壁垒不是终审判决」+ §六.1
//   「模板缺失时按权威文案落地，K194 实证」）：
//   ① side_effect 表**有** 42 行：arg_count=2、样例 "1 1"（本表是引擎切片权威，
//     src/db/official_data_repository.cpp:173-174）；
//   ② 官方效果描述数据镜像（BWIKI 赛尔号WIKI「效果:42」页，镜像官方 effect 描述
//     数据）给出模板原文：**「{0}~{1}回合自己使用电招式伤害x2」**，参数个数 2，
//     与 side_effect.arg_count=2 一致；页面逐条列出本族全部 9 载体（20006/20122/
//     20476/20871/22944/23244/23359/26177/27475），每条的效果原文与 moves.info
//     自报文本全部吻合；
//   ③ 2013 官方技能表充电原文「一回合自己使用电招式伤害×2」（4399 技能表，与
//     [1,1] → "1~1回合" 逐字对应）；
//   ④ 复合行切片全自洽：4 条复合行 sum(arg_count)==len(side_effect_arg) 零剩余
//     （见下行清单），切片基准 side_effect.arg_count 与 effect_info.args_num 兜底
//     机制（official_data_repository.cpp:172 注释）在 42 无 effect_info 行时取
//     side_effect 值 2——**切片长度正确**。
//   → 语义完全确定、双源互证、可安全建模（41 是真无权威，42 有官方镜像权威，
//     二者不可同日而语）。
//
// ══ 模板解读 ══
//   「{0}~{1}回合自己使用电招式伤害x2」：
//   - {0}/{1} = 持续回合数区间 [下限, 上限]（同模板族 118「随机改为{0}~{1}点威力」/
//     683「随机附加{0}~{1}点固定伤害」/1144 同款区间记法——运行期闭区间掷点）；
//   - 倍数**固定 ×2**（模板字面 x2，非行参——9 载体 info 文案「威力提升/上升/翻倍」
//     无一携带数字，×2 由模板钉死）；
//   - "电招式" = 系别固定电（skill_types id=5；模板无系别行参，65 的 {1} 位在 42 缺席）。
//   窗口口径："{0}~{1}回合" 按 §五.0 用户裁定（2026-09-13，引擎级通用规则）走
//   **InRounds 含当回合**（65/44/469/1188 同款；65 家族 {0}=1 的 R2 过期差分已被
//   场景 842 钉死）。⚠️ 记档分歧面：社区攻略把充电读作「下回合电系×2」（圣霆·雷伊
//   技能表「使自身下1回合电系技能伤害×2」是**另一形态的模板**，非本族 9 载体的
//   官方数据）。若后续官方实测推翻 InRounds 口径，改
//   custom_effect_overrides(override_type='window', map_value='next_rounds') 数据行
//   即可（effect_window_kind 查表、register_round_effect 自动折算），零代码改动。
//   「伤害x2」的落地：伤害公式对威力线性 → 以**威力视图 ×2**落地（65 同通道，
//   差异面 = 整数取整 ±1；65 模板字面「威力为X倍」、42 字面「伤害x2」按同通道处理，
//   断言走威力视图字段+比值带，坑 6）。
//
// ══ 行清单（unity 库 moves 表 json 解析精确匹配；side_effect 主链，friend 全空。
//    切片 = side_effect.arg_count 平面顺序切片，official_data_repository.cpp:170-184。
//    2026-11-07 逐行核算，sum(arg_count)==len(args) 全 9 行零剩余）══
//   20006 充电       属性 p0 [42]              [1,1]            → 42:[1,1]
//   20122 电能聚合   属性 p0 [42]              [2,2]            → 42:[2,2]
//   20476 雷电召唤   属性 p0 [42]              [3,5]            → 42:[3,5]
//   20871 雷之怒     属性 p0 [42]              [2,2]            → 42:[2,2]
//   22944 雷电之鬃   属性 p0 [4,4,4,4,42]      [1,100,1,2,100,1,3,100,1,5,100,1, 1,1]
//        → 4×4:[1,100,1][2,100,1][3,100,1][5,100,1]（12 参）+ 42:[1,1]（3×4+2=14 ✓）
//   23244 充电身躯   属性 p0 [42,57,164]       [1,1, 3,3, 3,100,0]
//        → 42:[1,1] / 57:[3,3]（3回合每回合回1/3）/ 164:[3,100,0]（3回合受击100%麻痹；
//          {2}=0=麻痹码，effect_des 0=麻痹 ✓）（2+2+3=7 ✓）
//   23359 激雷地狱   属性 p0 [44,78,42]        [3, 3, 1,1]
//        → 44:[3]（3回合特伤减半）/ 78:[3]（3回合物攻必miss）/ 42:[1,1]（1+1+2=4 ✓）
//   26177 闪电召唤   属性 p0 [48,613,42]       [3, 2,5, 2,2]
//        → 48:[3]（3回合免异常）/ 613:[2,5]（2回合令对手电系无效，5=电 ✓）/
//          42:[2,2]（1+2+2=5 ✓）
//   27475 电荷依附   属性 p0 [191,42,613,843]  [4, 3,5, 5,7, 2,2]
//        → 191:[4]（4回合免疫反弹异常）/ 42:[3,5] / 613:[5,7]（5回合令对手地面系无效，
//          7=地面 ✓）/ 843:[2,2]（下2回合先制+2 ✓）（1+2+2+2=7 ✓）
//   ⚠️ 27475 的 42 切片 [3,5] 与其 info 自报「自身电系招式开始伤害翻倍了」——"翻倍"
//      是模板固定 ×2 的转述（BWIKI 效果:42 页同条原文即「3~5回合自己使用电招式
//      伤害x2」），不是 5 倍——区间参义在此行得到最终裁决。
//   链上伙伴 4/44/48/57/78/164/191/613/843 **registry 全注册** → 9/9 纯链。
//
// ══ 四查（2026-11-07）══
// ① 内核硬编码：(^|[^0-9.])42([^0-9.]|$) 面太宽，按语义位 grep——SKILL_EFFECT(42 /
//   case 42 / effect_42 / kMark42 / EFFECT_42 对 src/ include/ resources/ 均 0 命中
//   （effect_42[5-8] 系 425/426/427/428 不同 id）；battleFsm "42" 命中为其它常量噪声；
// ② effect_info 无 42 行（缺口族）——**side_effect 表在列**（arg_count=2），
//   custom_effect_overrides / custom_effect_programs 对 42 均 0 行；
//   模板权威改由官方镜像取得（见上，语义非不明）；
// ③ 全库并集无注册：resources/ test/ grep SKILL_EFFECT(42 / SOUL_PROGRAM(42 零命中；
// ④ 废弃 parser 覆盖面（冻结不扩，只记录）：42 无 effect_info 行 → effect_record.info
//   为空串 → parse_effect_unit("") 无 pattern 命中返回 -1 → **全惰性**（无半解析，
//   现状静默 no-op）。本注册落地后 clone_effect 命中插件函数、parser 路径不再驱动
//   （src/entities/skills.cpp:672 只在 effect.logic 为空时走 parse_effect_unit）。
//
// ══ 实现形态（同形套路照抄）══
// 直系同形先例 = H6 效果 65（resources/moves_lib/h_elem_attr_cond.cpp）：
//   「{0}回合内{1}系技能威力为{2}倍」——InRounds 窗 + FIRST/SECOND 两 SKILL_EFFECT
//   桶各挂一条同参 tick（execute_at 精确状态匹配、无镜像，timed_bucket.cpp:17；
//   每回合己方出手恰好命中己方所在侧的桶、单次触发；额外行动每攻各翻一次）+
//   tick 内静态面板判系别（roundChoice → skills[idx].element[0/1]；skill_element_view
//   在 ON_SKILL_HIT 才物化，SKILL_EFFECT 时点读不到）+ 属性技短路守卫（属性技无红伤）
//   + 威力视图赋值自动置 rewritten → 标准变威力重算管线伤害前重算。42 差异 =
//   系别/倍数是模板常量（电=5 / ×2），行参只有区间 [下限,上限]。
// 区间掷时长先例 = 效果 683（resources/moves_lib/heal_drain.cpp:87-103）：
//   low + (rng.next() % (high - low + 1)) 闭区间掷——引擎自有确定性 RNG（MINSTD，
//   include/utils/sbs_rand.h），场景闭式可核（K430 T10 同款）。[lo,lo] 载体（充电/
//   电能聚合等 7 条）退化为定值时长。
// 复用即刷新：register_round_effect 同 (state,owner) 再注册 → 覆盖刷新（65 同款注释）。
// 窗口起点折算：register_round_effect 内部走 round_effect_start_round（InRounds
//   后出手顺延一回合），先手位窗口 R1..R{N}、后手位 R2..R{N+1}（§五.0 官方回合数判定）。
//
// ══ 审计槽（无随机精确断言口径，F9 782 首创 / K430 同式）══
// 键段 = 1,000,420,000（10^9 + 42×10^4）+ owner×16 + slot（写前已跑
//   tools/check_storage_keys.py 全库无撞，rc=0）：
//   slot 0  申报下限 {0}                    slot 1  申报上限 {1}
//   slot 2  掷得时长（InRounds 窗长；[lo,lo] 载体==lo）
//   slot 3  最近一次翻倍后的威力视图值（0=尚未翻过）
//   slot 4  翻倍次数累计（tick 命中电系攻击技才 +1）
//   slot 5  窗口注册次数累计（复用刷新计次）
#include "common.h"

#include <utils/sbs_rand.h>

namespace {

constexpr int k432ElemElectric = 5;        // skill_types id=5=电（模板"电招式"固定系别）
constexpr int k432Mult = 2;                // 模板固定 x2
constexpr int k432AuditBase = 1000420000;  // 10^9 + 42×10^4
constexpr int kActionSelectSkill = 1;      // = BattleFsm::ActionType::SELECT_SKILL

inline int key432(int owner, int slot) { return k432AuditBase + owner * 16 + slot; }

// 指针式 any_cast 安全读（common.h any_int_or 口径：类型不符按缺失回退，绝不抛）
inline int s432(const BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    return any_int_or(it->second, def);
}

inline void set_s432(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// ── 42 tick —— 己方攻击的 SKILL_EFFECT 时点：选中技能系别含电 → 威力视图 ×2 ──
// 载荷 [owner, elem, mult]（65 tick 同形照抄：h_elem_attr_cond.cpp:60-90）。
EffectResult k432_electric_power_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int elem = args.int_args[1];
    const int mult = args.int_args[2];
    if (owner < 0 || owner > 1 || mult <= 1) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return EffectResult::kOk;
    }
    const int slot = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (slot < 0 || slot >= static_cast<int>(skills.size())) {
        return EffectResult::kOk;
    }
    const Skills& used = skills[static_cast<std::size_t>(slot)];
    if (used.type == SkillType::Attribute) {
        return EffectResult::kOk;   // 属性技无红伤可翻（65 同守卫）
    }
    if (used.element[0] != elem && used.element[1] != elem) {
        return EffectResult::kOk;   // 非电系：威力不动（静态面板判据，65 同款）
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * mult;   // 置 rewritten → 标准变威力重算管线（65/609/132 同路）
        set_s432(ctx, key432(owner, 3), view.value);
        set_s432(ctx, key432(owner, 4), s432(ctx, key432(owner, 4), 0) + 1);
    }
    return EffectResult::kOk;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 42 —— {0}~{1}回合自己使用电招式伤害x2
// 行参布局 [owner, opp, 下限{0}, 上限{1}]（DB 两参；skills.cpp bind_participants
// 加 [0,1] 前缀）。倍数/系别为模板常量，不行参。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_42_electric_power_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 42, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 42, args,
                     "42 布局=[owner,opp,下限,上限]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int lo = args.int_args[2];
    const int hi = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     42, args, "需要 owner/opp ∈ {0,1} 且不等");
    EFFECT_ARG_GUARD(lo > 0 && hi >= lo, 42, args, "需要 回合下限>0 且 上限>=下限");

    // 区间掷时长（683 同款闭区间；sbs_rand = 引擎自有确定性 RNG，场景闭式可核）。
    const int rounds = choose_skill_range(*ctx, 42, 0, owner, owner, lo, hi);
    set_s432(ctx, key432(owner, 0), lo);
    set_s432(ctx, key432(owner, 1), hi);
    set_s432(ctx, key432(owner, 2), rounds);
    set_s432(ctx, key432(owner, 5), s432(ctx, key432(owner, 5), 0) + 1);

    // FIRST/SECOND 两桶各挂一条（65 同款：execute_at 精确状态匹配、无镜像——
    // 己方出手恰好命中己方所在侧的桶、单次触发；复用同 source_id 去重刷新）。
    register_round_effect(ctx, State::BATTLE_FIRST_SKILL_EFFECT, owner, 42, rounds,
                          {owner, k432ElemElectric, k432Mult}, &k432_electric_power_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_SKILL_EFFECT, owner, 42, rounds,
                          {owner, k432ElemElectric, k432Mult}, &k432_electric_power_tick);
    return EffectResult::kOk;
}

SKILL_EFFECT(42, effect_42_electric_power_window)
