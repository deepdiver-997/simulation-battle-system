// K435 · 性别条件附加固伤单宏 —— 130「对方为{0}则附加{1}点伤害」×7
//（场景 test/scenario/scenario_8200_xingbie_fushang.cpp；号段 8200-8209，
//  2026-11-08，基线 main-local 6bef39e1，registry moves=1480 / soul=411）
//
// ══ 语义现查（2026-11-08）——**{0} 是性别码，不是属性码（K433 排除注记证伪）**══
// 官方模板（effect_info id=130，args_num=2；side_effect.arg_count=2 一致）：
//   「对方为{0}则附加{1}点伤害」；analyze 原文 "[sprite name=iconHit]对方为{0}则附加
//   {1}点伤害（boss有效）"——iconHit/boss有效为表现层 flavor，无独立机制。
// **性别裁决证据链（四源互证）**：
//   ① BWIKI 权威文案（K432 先例口径：官方效果描述数据镜像）wiki.biligame.com/seer/
//     效果:130 逐条实例化——深蓝利刃 12297 [2,60]→"对方为**雌性**则附加60点伤害"、
//     深红之光 12304 [1,100]→"对方为**雄性**则附加100点伤害"、其余 5 载体 [1,300]
//     全为"对方为雄性则附加300点伤害"；
//   ② 同门三兄弟同模板句式同 param 型（[5,0,0]）同码表：129「对方为{0}则技能威力
//     翻倍」（BWIKI 效果:129：蔚蓝之击[2]/赤色旋风[2]→雌性、噩梦袭击[1]/萌兔灵击[1]/
//     致命影袭[1]/碧灵轻语[1]→雄性；**Flash 端模板字面带"性"字**"对方为X性则…"）、
//     131「对方为{0}则免疫当前回合伤害」（BWIKI 效果:131：蓝光幻盾[2]→雌性、
//     红光幻盾[1]→雄性；DB 实参 21193=[2]/21197=[1] 逐位吻合）；
//   ③ **链内互补自洽（决定性）**：混链载体 19343/34612 的 898 官方模板
//     「若对手**不是雄性精灵**，则恢复{0}点体力」（effect_info:898 原文带"雄性精灵"字样）
//     与 130 切片成对出现——19343：130[1,250]"对方为雄性则附加250" + 898[250]
//     "不是雄性则恢复250"（逻辑补集成对，BWIKI 两技页同文）；
//   ④ 引擎先例 600（qiling_skills.cpp:208-211）：gender_code==1→MALE、==2→FEMALE
//     同码表（DB monsters.gender：0=无性别/1=雄/2=雌，pet_factory.cpp:31-45
//     map_gender；2026-09-22 该映射曾被纠偏，600 是 gender 首个读者）。
//   → 码表定案：**{0}=1→雄性、{0}=2→雌性、其它码=数据漂移（guard 响亮报错）**。
// ⚠️ **勘误记档（存量误读，非本单改动面）**：129（j_power_cond_double.cpp:8,18-21，
//    J289）与 131（k419_meiheweite_pool.cpp:36-38,78，K419）头注均把 {0} 读作
//    skill_types 元素 id（"[2]=水、[1]=草系"）——BWIKI 三页实例文案证伪（129 的
//    蔚蓝之击[2] 实为"对方为雌性"、131 的红光幻盾[1] 实为"对方为雄性"）。两单的
//    条件面按性别重实现属后续修复工单（回归面 = 各自场景号段），本单不动存量文件。
//
// ══ 载体 7 条全量切片（unity 库 moves 表 json 解析、side_effect.arg_count 平面切片，
//    sum(arg_count)==len(arg) 全 7 行零剩余零串位，2026-11-08 脚本核算）══
//   12297 深蓝利刃   [130]           [2,60]   雌性+60（物100 普通；使用者 奈哲尔962 水超能）
//   12304 深红之光   [130]           [1,100]  雄性+100（特100 普通；使用者 奈尼尔965 火超能）
//   19343 净世闭月断 [180,130,898]   [1, 250, 250]  雄性+250（物160 火神秘 must_hit；
//                  **混链**：180 已注册 stat_dispel.cpp:464 / **898 未注册保持 no-op**
//                  ——J334 17603 / K430 36351 / K431 16620 混链同口径记档，不越族代做；
//                    898 另有 7 载体独立成族，留续单）
//   33282 寒风席卷   [130]           [1,300]  雄性+300（物140 自然冰）
//   33633 萌喵呼唤   [130]           [1,300]  雄性+300（特120 圣灵暗影）
//   34424 夜色之芒   [130]           [1,300]  雄性+300（特130 暗影电）
//   34612 狂影之掠   [915,130,898]   [2,50,6, 1,300, 300]  雄性+300（特85 邪灵神秘
//                  先制+3；混链：915 已注册 e_break_round.cpp:159 / 898 同上 no-op）
//   纯链 5/7、混链 2/7（缺腿都是 898）；130 全部主链，friend 侧零引用。
//
// ══ 四查（2026-11-08）══
// ① 内核硬编码：语义位 grep SKILL_EFFECT(130 / case 130: / effect_130 / kMark130 /
//   EFFECT_130 / SOUL_PROGRAM(130 对 src/ include/ **0 命中**（"case 1306/1307/1308"
//   为 1306/1307/1308 不同 id 的子串噪声）；数字边界 grep 仅
//   battleContext.cpp:623 `r.final * 130 / 100` 乘区常量巧合，无行为硬编码。
// ② effect_info 130 在列（args_num=2）= side_effect.arg_count（2）一致；
//   custom_effect_programs（6 行）/ custom_effect_overrides（17 行）按 effect_id 查
//   130 **零命中**。
// ③ 全库并集无注册：resources/ test/ 内 SKILL_EFFECT(130,（精确逗号边界）与
//   kMark130 词界 grep 零命中；兄弟 worktree（agents/night-fuzz / endgame-walk /
//   k434，k434 尚未开工无占）git grep 同款零命中——真缺口（K433 排除清单明记
//   "留下一批"，无撞族）。
// ④ 废弃 parser 覆盖面（冻结不扩，只记录）：**全惰性**——主刀四型 {"令对手{",
//   "令自身{","使对手{","使自身{"}（effect_unit_parser.cpp:75）要求令/使动词，本模板
//   "对方为{"无动词不中；兜底 3)"附加+固定伤害"需"固定伤害"字样，本模板是
//   "附加{1}点伤害"（无"固定"二字，effect_unit_parser.cpp:133）不中；第二刀 5 类
//   条件模板（179/700/1083/1257/456，:148-197）均不中 → parse_effect_unit 返回 -1，
//   现状静默 no-op。注册落地后 clone_effect 命中插件函数、parser 路径不再驱动
//   （src/entities/skills.cpp:675 只在 effect.logic 为空时走 parse_effect_unit）。
//
// ══ 实现形态（同形套路照抄，通道零新增）══
// 落点直系先例 = **428**（e_predator_cond_amp.cpp:48-62「遇到天敌时附加{0}点固定
//   伤害」）：SKILL_EFFECT 时点（effect_register_state default =
//   BATTLE_FIRST_SKILL_EFFECT，src/entities/skills.cpp:184；miss/被盔桶不跑）
//   条件成立 → `deal_pink_damage(FIXED)` 即时档附加（"附加"打在攻击技能上，
//   1158/774 同口径记档）。条件先例 = **600**（qiling_skills.cpp:195-226）：性别门
//   读 `ctx->getPet(opp).gender`（本体枚举，Gender::MALE=0/FEMALE=1/NONE=2，
//   include/entities/elf-pet.h:21-25；harness make_pet 恒 NONE，场景显式置位）。
//   粉伤通道语义：臣服 / 攻空票在 deal_pink_damage 入口拦
//   （battle_primitives.cpp:1283-1295），免粉 PINK_DAMAGE 票在粉伤管线拦
//   （run_pink_damage → pink_damage_pipeline_.run，battle_primitives.cpp:975-1005，
//   final=0 即不落血）——拦下时返回仍为 SUCCESS（"已处理不落地"），
//   场景以同种子 hp 差值为准、返回码作辅助观测（T8 实证）。
// 生存门（K430 1079 / K431 184 口径）：目标已倒 → 整条跳过（reason=2）；粉伤原语
//   自带 TARGET_DEFEATED 档（rider 先于红伤落地打死目标 → 返回 TARGET_DEFEATED、
//   不计成功，场景 T 块实测钉死）。
// "（boss有效）"：表现层 flavor，引擎无 boss 分面，不建模（头注记档）。
//
// ══ 审计槽（无随机精确断言口径，K431 184 同式）══
// 130 键段 = 1,001,300,000（10^9 + 130×10^4）+ owner×16 + slot（新增前已跑
//   tools/check_storage_keys.py 全库无撞）：
//   slot 0  申报性别码（{0}：1=雄 / 2=雌）
//   slot 1  申报固伤量（{1}）
//   slot 2  映射 Gender 枚举 int（MALE=0 / FEMALE=1）
//   slot 3  对手 gender 枚举观测（int 原值）
//   slot 4  条件结果（1=真 / 0=假）
//   slot 5  deal_pink_damage 返回码（FixedDamageResult int，battle_primitives.h:538）
//   slot 6  成功附加计数（==SUCCESS 才 +1）
//   slot 7  最近拒绝原因（0 无 / 1 条件假 / 2 目标已倒 / 3 内核槽缺失）

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k130AuditBase = 1001300000;   // 10^9 + 130×10^4

inline int key130(int owner, int slot) { return k130AuditBase + owner * 16 + slot; }

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
// 130 —— 对方为{0}则附加{1}点伤害
// 行参布局 [owner, opp, 性别码, 固伤量]（DB 两参 {0}..{1}；skills.cpp 加 [0,1] 前缀）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_130_gender_cond_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 130, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 130, args,
                     "130 布局=[owner,opp,性别码,固伤量]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int gender_code = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     130, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(gender_code == 1 || gender_code == 2, 130, args,
                     "性别码 " + std::to_string(gender_code) + " 越界（官方码 1=雄 / 2=雌；"
                     "BWIKI 效果:130/129/131 三页互证）");
    EFFECT_ARG_GUARD(amount > 0, 130, args, "需要 固伤量 > 0");

    // 性别码 → Gender 枚举（600 同表：1=MALE / 2=FEMALE；pet_factory map_gender 同源）。
    const Gender want = (gender_code == 1) ? Gender::MALE : Gender::FEMALE;
    set_storage_int_local(ctx, key130(owner, 0), gender_code);
    set_storage_int_local(ctx, key130(owner, 1), amount);
    set_storage_int_local(ctx, key130(owner, 2), static_cast<int>(want));
    set_storage_int_local(ctx, key130(owner, 7), 0);

    // 生存门（1079/2295 口径）：目标已倒 → 整条跳过。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, key130(owner, 7), 2);
        return EffectResult::kOk;
    }

    // 性别门：对手**本体** gender 枚举相等才开（无性别 NONE 不属雄/雌——600 同判）。
    const Gender actual = ctx->getPet(opp).gender;
    set_storage_int_local(ctx, key130(owner, 3), static_cast<int>(actual));
    const bool cond = (actual == want);
    set_storage_int_local(ctx, key130(owner, 4), cond ? 1 : 0);
    if (!cond) {
        set_storage_int_local(ctx, key130(owner, 7), 1);
        return EffectResult::kOk;
    }

    // 落点 = deal_pink_damage(FIXED) 真原语（428 即时档同款；免粉/臣服/攻空票在
    // 原语内拦，拦下返回 SUCCESS 但不落地——场景以 hp 差值为准）。
    if (!g_core || !g_core->deal_pink_damage) {
        set_storage_int_local(ctx, key130(owner, 7), 3);
        return EffectResult::kOk;
    }
    const FixedDamageResult r =
        g_core->deal_pink_damage(ctx, opp, amount, DamageKind::FIXED, owner);
    set_storage_int_local(ctx, key130(owner, 5), static_cast<int>(r));
    if (r == FixedDamageResult::SUCCESS) {
        set_storage_int_local(ctx, key130(owner, 6),
                              storage_int_local(ctx, key130(owner, 6), 0) + 1);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(130, effect_130_gender_cond_fixed)
