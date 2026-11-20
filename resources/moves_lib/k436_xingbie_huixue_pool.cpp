// K436 · 性别互补回血族单宏 —— 898「若对手不是雄性精灵，则恢复{0}点体力」×4
//（场景 test/scenario/scenario_8220_xingbie_huixue.cpp；号段 8220-8229，
//  2026-11-08，基线 main-local；registry moves 增前 1483 / soul=411）
//
// ══ 语义现查（2026-11-08）══
// 官方模板（effect_info id=898，args_num=1；side_effect.arg_count=1 一致）：
//   「若对手不是雄性精灵，则恢复{0}点体力」；analyze 原文 "[sprite name=iconHit]若对手
//   不是雄性精灵，则恢复{0}点体力（boss有效）"——iconHit/boss有效为表现层 flavor，
//   无独立机制（K435 130 同口径记档）。{0}=恢复量（无性别码行参——"雄性"是模板
//   字面硬编码，性别判据不来自 args）。
// **性别判据（K435 定案直系，第三源兑现）**：K435 头注证据链四源（BWIKI 效果:130/
//   129/131 三页 + 同门模板 + 引擎 600 pet_factory map_gender 先例）定案 130 的 {0}
//   为性别码（1=雄/2=雌），并记档 898「若对手**不是雄性精灵**」是链内互补对——本单
//   即该互补对的落地：判据 = 对手 gender != Gender::MALE（**含雌性与无性别**，
//   "三向门"：雄闭 / 雌开 / 无性别开；Gender 枚举 MALE=0/FEMALE=1/NONE=2，
//   include/entities/elf-pet.h:21-25）。
//   BWIKI 效果:898 页（2026-11-08 抓取）与 DB 逐条互证：净世闭月断"恢复250点体力"、
//   茸茸蓄力"恢复300点体力"、狂影之掠"恢复300点体力"、弧光爪"恢复200点体力"——
//   与 DB 切片 [250]/[300]/[300]/[200] 逐位吻合，模板字面即"不是雄性精灵"。
// ⚠️ **勘误记档**：K435 头注与派发口径「898 ×7 载体」是 side_effect LIKE '%898%'
//   子串误计——7 行命中里 28161 诚挚之心 / 28980 流浪童话 / 29357 铭记片像携带的
//   是 **1898**（「下{0}回合自身技能将对手的能力提升效果视为能力下降效果…」，
//   effect_info:1898 独立效果；JSON 精确元素匹配即排除）。真载体 = 4（DB json_each
//   精确匹配与 BWIKI 效果:898 页条目数**双双为 4**，互相印证）。"数字巧合不算"
//   纪律的活例。
//
// ══ 载体 4 条全量切片（unity 库 moves 表 json 解析、side_effect.arg_count 平面切片，
//    sum(arg_count)==len(arg) 全 4 行零剩余零串位，2026-11-08 脚本核算）══
//   19343 净世闭月断 [180,130,898]  [1, 250, 250]        恢复250（物160 火神秘 must_hit；
//                 **混链**：180 已注册 stat_dispel.cpp:464 / 130 已注册
//                 k435_xingbie_fushang_pool.cpp:181 / 898 本单——**130+898 同链成对
//                 直证**：雄性+250 伤害 / 非雄性恢复 250，性别互补成对）
//   29168 茸茸蓄力  [585,89,898]    [0,1,1,1,1,0, 3,1, 300]  恢复300（属性技 power=0；
//                 585 已注册 stat_dispel.cpp:475（六项属性提升，0=跳过）/ 89 已注册
//                 batch_b_skills.cpp:2840（吸血窗 [3,1]——3回合 1/1；本技无伤害不触发））
//   34612 狂影之掠  [915,130,898]   [2,50,6, 1,300, 300]  恢复300（特85 邪灵神秘 先制+3；
//                 混链：915 已注册 e_break_round.cpp:159 / 130 已注册 k435 / 898 本单）
//   37894 弧光爪    [743,898]       [1, 1, 200]           恢复200（物85 先制+3；
//                 743 已注册 j_reversal_prio.cpp:78（反转+先制窗 [1,1]））
//   纯链 0/4、混链 4/4（缺腿都只是 898 本尊）；friend 侧零引用（json_each 全库扫描）。
//
// ══ 四查（2026-11-08）══
// ① 内核硬编码：SKILL_EFFECT(898 / kMark898 / effect_898 / EFFECT_898 / SOUL_PROGRAM(898
//   / case 898: 对 src/ include/ **0 命中**；词界 grep -w 898 对 src/ include/ 亦 0 命中
//   （无 1898/28981 类子串噪声——四查里 LIKE '%898%' 的 7 行是 DB 侧 1898 串位，
//   内核侧干净）。
// ② effect_info 898 在列（args_num=1）= side_effect.arg_count（1）一致；4 载体切片
//   逐位自洽（上表）；custom_effect_programs（6 行）/ custom_effect_overrides（17 行）
//   按 effect_id=898 查 **零命中**。
// ③ 全库并集无注册：resources/ test/ 内 SKILL_EFFECT(898,（精确逗号边界）与 kMark898
//   词界 grep 零命中；兄弟 worktree（agents / endgame-walk / j336）与 hub 全分支
//   git grep 同款零命中——真缺口（K435 头注"898 另有 …独立成族，留续单"即本单）。
// ④ 废弃 parser 覆盖面（冻结不扩，只记录）：**全惰性**——主刀四型 {"令对手{",
//   "令自身{","使对手{","使自身{"}（effect_unit_parser.cpp:75）要求令/使动词，本模板
//   "若对手不是…则恢复{"无动词不中；兜底 3)"附加+固定伤害"需双词同现
//   （effect_unit_parser.cpp:132-133），本模板无"附加"/"固定伤害"字样不中 →
//   parse_effect_unit 返回 -1，现状静默 no-op。注册落地后 clone_effect 命中插件函数、
//   parser 路径不再驱动（src/entities/skills.cpp:675 只在未注册时走 parse_effect_unit）。
//
// ══ 实现形态（同形套路照抄，通道零新增）══
// 条件先例 = **600 / K435 130**（k435_xingbie_fushang_pool.cpp:150 同款 gender 门口）：
//   读 `ctx->getPet(opp).gender` 本体枚举；"不是雄性" = != Gender::MALE（无性别 NONE
//   属"不是雄性"——BWIKI 文案字面 + 130 三向门互补对齐，头注记档）。
// 恢复通道 = **heal_amount 真原语**（battle_primitives.h:551-554「按具体数值恢复
//   （吃封回血 + 恢复效果修正% + 记 last_heal）」；实现 heal_impl
//   battle_primitives.cpp:1462——HEAL_BLOCK 票 :1487 拦、heal_mod_pct+星赎 :1498
//   修、clamp 到 max_hp、actual>0 发 EVENT_HEAL_RESTORED）——**禁止直写 hp**：
//   封回血/减疗/回血触发监听全在通道内，绕过即 K 系纪律"自记状态绕原语"红线。
// 生存门 = **owner 侧 hp<=0**（k426_wuji_busan_pool.cpp:184 自体门同式：本效果受益
//   目标是 owner，已倒不经 heal——防"奶活"裸路径；130 的门在 opp 侧因其落点是对手
//   本体，两门各护各的落点，不混用）。登记前时点 owner 必活（能出手即活），场景
//   不可构造，同 K435/K431 记档；门本体实现照抄留档。opp 侧**不加门**：官方模板
//   无"对手存活"条款，倒下的对手性别仍可读（与 130 不同——130 的粉伤原语自带
//   TARGET_DEFEATED 档，898 不落伤害无需该档）。
// SKILL_EFFECT 时点（effect_register_state default = BATTLE_FIRST_SKILL_EFFECT，
//   src/entities/skills.cpp:184；miss/被盔桶不跑）——19343/34612 链内 130 先于 898
//   （side_effect 数组序），性别门互补保证同回合**至多一条**开：雄性=130 开/898 闭，
//   非雄性=130 闭/898 开，无性别=双闭→898 开（130 的 NONE 闭 + 898 的 NONE 开）。
// 行参布局 [owner, opp, 恢复量]（DB 一参 {0}；skills.cpp:29-36 build_effect_args_
//   for_skill 加 [0,1] 前缀）。
//
// ══ 审计槽（无随机精确断言口径，K435 898 哨兵位翻转即本单验收锚）══
// 898 键段 = 1,008,980,000（10^9 + 898×10^4）+ owner×16 + slot（新增前后均跑
//   tools/check_storage_keys.py 全库无撞）：
//   slot 0  申报恢复量（{0}）
//   slot 1  对手 gender 枚举观测（int 原值：0=MALE / 1=FEMALE / 2=NONE）
//   slot 2  条件结果（1=对手非雄性开 / 0=雄性闭）
//   slot 3  heal_amount 返回码（HealResult int：0=SUCCESS / 1=INVALID_PARAM，
//           battle_primitives.h:533-536）
//   slot 4  实际恢复量（hp_after − hp_before；封回血/满血封顶=0）
//   slot 5  实际恢复>0 成功计数
//   slot 6  最近拒绝原因（0 无 / 1 对手是雄性 / 2 owner 已倒 / 3 内核槽缺失）
//   slot 7  恢复后 owner hp 观测（精确断言辅助）
//   （heal_amount 返回码恒 SUCCESS 即使被拦/满血——battle_primitives.cpp:1547 只查
//     参数；"拦下不落地"以 slot4 hp 差值为权威观测，K435 T8 同口径。）

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k898AuditBase = 1008980000;   // 10^9 + 898×10^4

inline int key898(int owner, int slot) { return k898AuditBase + owner * 16 + slot; }

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

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 898 —— 若对手不是雄性精灵，则恢复{0}点体力
// 行参布局 [owner, opp, 恢复量]（DB 一参 {0}；skills.cpp 加 [0,1] 前缀）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_898_gender_cond_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 898, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 898, args,
                     "898 布局=[owner,opp,恢复量]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     898, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 898, args, "需要 恢复量 > 0");

    set_storage_int_local(ctx, key898(owner, 0), amount);
    set_storage_int_local(ctx, key898(owner, 6), 0);

    // 生存门（426 2344 自体门同式）：受益目标 owner 已倒 → 整条跳过（不经 heal，
    // 防"奶活"裸路径；登记前时点 owner 必活，场景不可构造——头注记档）。
    if (ctx->getPet(owner).hp <= 0) {
        set_storage_int_local(ctx, key898(owner, 6), 2);
        return EffectResult::kOk;
    }

    // 性别门（K435 定案互补对）：对手**本体** gender != MALE 即"不是雄性精灵"——
    // 雌性/无性别均开（BWIKI 模板字面；与 130 的 ==want 门互为补集）。
    const Gender actual = ctx->getPet(opp).gender;
    set_storage_int_local(ctx, key898(owner, 1), static_cast<int>(actual));
    const bool cond = (actual != Gender::MALE);
    set_storage_int_local(ctx, key898(owner, 2), cond ? 1 : 0);
    if (!cond) {
        set_storage_int_local(ctx, key898(owner, 6), 1);
        return EffectResult::kOk;
    }

    // 恢复落点 = heal_amount 真原语（封回血/恢复修正%/last_heal/回血触发全在通道内；
    // 满血封顶 actual=0 不计成功——slot4 差值为权威观测）。
    if (!g_core || !g_core->heal_amount) {
        set_storage_int_local(ctx, key898(owner, 6), 3);
        return EffectResult::kOk;
    }
    const int hp_before = ctx->getPet(owner).hp;
    const HealResult r = g_core->heal_amount(ctx, owner, amount);
    const int hp_after = ctx->getPet(owner).hp;
    set_storage_int_local(ctx, key898(owner, 3), static_cast<int>(r));
    set_storage_int_local(ctx, key898(owner, 4), hp_after - hp_before);
    set_storage_int_local(ctx, key898(owner, 7), hp_after);
    if (r == HealResult::SUCCESS && hp_after - hp_before > 0) {
        set_storage_int_local(ctx, key898(owner, 5),
                              storage_int_local(ctx, key898(owner, 5), 0) + 1);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(898, effect_898_gender_cond_heal)
