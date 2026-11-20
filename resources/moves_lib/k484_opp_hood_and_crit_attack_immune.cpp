// K484 · 对手护罩 + 暴击免疫两族 —— 1939×4 + 2334×4（2026-11-19，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k484，分支 feat/k484，基点 7968ed04
// 含 K461-K483 主线实现；场景 test/scenario/scenario_8710_k484.cpp，号段 8710-8719）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-19）══
//   1939（args_num=1）"为对手附加{0}点护罩"
//   2334（args_num=1）"打出致命一击时下{0}回合免疫对手的攻击"
//
// ══ 载体切片（unity 库 side_effect.arg_count 顺序切分，2026-11-19 全载体自洽零剩余；
//   1939 切片与 K472 T8590-pre 表逐项一致；实现读行参不硬编码）══
//   1939 [{0}=护罩点数]（4 载体；{0}∈{200,300}）：
//     28261 欺诈戏法  [962,927,1938,1939]     →962=[1]|927=[4,2,3]|1938=[300]|**1939=[300]**（cat4）、
//     29166 停霜漫怀  [993,2230,1939,1938,843]→993=[1,15]|2230=[4,2,100,28]|**1939=[300]**|1938=[300]|843=[2,2]、
//     36583 分你一些  [1835,1938,1939]        →1835=[1]|1938=[200]|**1939=[200]**（cat1 攻击载体）、
//     38154 派派大扫除[2052,1938,1939,1835]   →2052=[2]|1938=[300]|**1939=[300]**|1835=[1]（cat1）。
//   ⚠️ 四载体 1938+1939 同链成对（4399 原文"为对手附加200点护盾；为对手附加200点护罩"，
//     K472 头注同源记档）——1938 已实现（K472），本单转正孪生 1939。
//   2334 [{0}=回合]（4 载体；{0}∈{1,2}；⚠️ 板行"5载体"修正：side_effect/friend_side_effect
//     全表 LIKE 精确切分恰 4 条，38272 天叩·千钧一发（嘲风另一技）side_effect=[2417,29,810,
//     1156] **不含 2334**，raw_json 命中皆 id 数字子串假阳性——载体数以 DB 为权威记档）：
//     38067 平庸或毁灭  [2216,2308,1356,1355,553,2334]→2216=[100]|2308=[2]|1356=[3]|1355=[1]|
//       553=[100,22]|**2334=[1]**（cat2 p160）、
//     38076 王·千秋稽首 [760,2331,1837,2334,2333]    →760=[]|2331=[]|1837=[100,31,1]|
//       **2334=[1]**|2333=[45,45]（cat1 p160）、
//     38271 地拜·千里一息（嘲风 4871）[2227,179,2274,2334]→2227=[2]|179=[160]|2274=[30]|
//       **2334=[2]**（cat2 p240；派发"嘲风 38271/38272 链"=嘲风双技链 38271 为 2334 载体）、
//     38411 意聚魂燃    [1767,1021,2334,1355]        →1767=[]|1021=[100]|**2334=[2]**|1355=[1]
//       （cat2 p150；同链 1355="暴击→免下{0}次异常"=**次数票**兄弟，与 2334 **回合窗票**
//       同链异通道——差分面见 T8711.d）。
//
// ══ 1939 实现口径（套路目录 §零决策树①：同形套路照抄先例）══
//   K472 1938 直系孪生转正（遗留记档"1939护罩孪生四载体链面静默可立后续单"）：
//   1938 = shield_bank_.add → 1939 = **hood_bank_.add**（唯一差分=盾/罩两面原语）。
//   作用方向 = 落 **target（对手）**身上：①effect_info"为对手附加"全文；②J3 2005 直系
//     先例（j_prob_anomaly_fallback.cpp:548 shield_bank_.add 落 target——"盾上对手
//     不是笔误"，:43 头注同文，4399 大雪纷飞同构原文；2026-11-19 三验行号现核）；
//     ③载体链 4399 原文互证（"为对手附加200点护盾；为对手附加200点护罩"）。不猜。
//   护罩面 = hood_bank_（**粉伤槽**：battleContext.cpp:336-348 PinkDamagePhase::HOOD 消费
//     FIXED/PERCENT、破罩 emit EVENT_SHIELD_BROKEN:343）——与 1938 盾（shield_bank_ 红伤
//     槽，battle_primitives.cpp:1096-1103 只吸收 DamageKind::NORMAL）互为两面：盾吃红不吃
//     粉、罩吃粉不吃红（K472 T8590.f 同款红粉分档实测面）。add(priority=0, {0},
//     source_id=1939, duration=0) 常驻；{0} 在位即消费（<=0 自然 no-op；族内 {0}∈{200,300}
//     无 0 档实测载体）；重施放 plain add 叠加（1938/K478 1446 无去重先例同口径）。
//   罩槽满（kMaxShields=8）静默丢弃 → 审计 add 前后 quantity_of 差值记"落地数"
//     （族内无 8 罩实测载体，N/A 不立变异，1938 同款）。duration=0+tick_duration 无 FSM
//     调用方 → 无"到期"语义（K472/K478 双证）；护罩挂 ElfPet 本体随宠走（perform_switch
//     不清 bank，battleFsm.cpp:580 全文无 bank 清理——离场"随宠不触发"口径，1446 三分辨
//     同源记档）。
//
// ══ 2334 实现口径（套路目录 §零决策树①：先例组合，通道零新增）══
//   暴击判据 = **K480 1557 crit_happened 直系**：SKILL_EFFECT 时点注册 FIRST/SECOND_
//     AFTER_ACTION 双桶探针（duration=1、source_id=2334 同源去重、acting_side 过滤）→
//     探针读 ctx->crit_happened[self]（skills.cpp:845 每次技能使用先清、:988 ①.5 掷定；
//     AFTER_ACTION 读到的是本次技能的结果）。
//   ⚠️ 时点甄别（板行"AFTER_ACTION 面/查询时机甄别"）：crit 掷定在 resolve_skill_execution
//     （battleFsm.cpp:1927 ON_SKILL_HIT 时点）内 query_usage ①.5——**早于** SKILL_EFFECT
//     桶，SKILL_EFFECT 时直读与 AFTER_ACTION 探针读行为等价（窗形状都是"下{0}回合"，
//     两者都在本轮 R 内、窗起点 R+1）。取 **AFTER_ACTION 探针面**：1557/1689 直系判据链
//     （"打出"=伤害结算完毕面，非掷定面）；miss 趟 SKILL_EFFECT 整桶不跑→探针不注册→
//     "打出"字面结构性成立（K452/1557 同口径）。盔趟（SKILL_INVALID 分支效果照常注册并
//     执行，battleFsm.cpp:999-1000；①.5"技能无效…都保留暴击结果"）→ 暴击照判——与官方
//     "打在盔上一样可以触发暴击"口径一致。crit_happened 与 1557 的分岔：1557 是"本回合
//     未打出"（否定判定同面）、2334 是"打出时"（肯定判定同面），读数相同、门向相反。
//   "免疫对手的攻击"通道 = **ImmunityType::DAMAGE 窗口票**（免疫面先例甄别）：受击面
//     拦截（damage 管线 BLOCK 相 battleContext.cpp:720-723 is_immune→consume_immune，
//     命中归零+emit EVENT_DAMAGE_IMMUNED:725）——**非**技能无效（consume_hit_invalid/
//     SEAL）**非**封属，非 PINK_DAMAGE（"攻击"=红伤；粉伤走另一 type，T8711.e 差分面）。
//     窗口票 counts=0：窗内每次攻击都查、**不逐次消耗**（rule_center.h consume_immune
//     "命中 counts==0 不扣"；次数票才逐次扣）——与同链兄弟 1355（次数票）构成通道差分。
//   窗界 = **"下{0}回合" NextRounds 直系（d5 1091 通道）**：暴击在 R →
//     register_next_rounds_effect(ROUND_START, self, 2334, rounds=1) → R+1 ROUND_START
//     tick 授票 grant_immunity(DAMAGE, duration={0})——window_expired 判定式
//     （rule_center.h:1738 current_round-register_round>=remaining）→ 覆盖 [R+1, R+{0}]
//     含端点，恰好"下{0}回合"。⚠️ 不在 AFTER_ACTION 直授（duration={0} 会覆盖当回合 R
//     ——当回合对手后手攻击被误挡=多覆盖一回合；d5 1091 头注同判据"直接在桥上授票会
//     多覆盖当回合"）。重暴击 = 同 source_id=2334 句柄复用刷新（rule_center.h grant_immune
//     ①路径；K480 711"重触发同源键覆盖刷新不叠加"同口径）+ register_next_rounds_effect
//     ROUND_START 同 source_id 同源去重——连续暴击窗连续（T8711.f 刷新腿锁定）。
//   scope = ON_STAGE（默认）：换宠 clear_on_stage 当场删票（battleContext.h:1120
//     invalidate_on_stage_effects → rule_center_.clear_on_stage——离场清理结构性成立，
//     T8711.g 锁定）。⚠️ 引擎结构边界（如实记档）：①属性伤害不吃常规挡伤票
//     （battleContext.cpp:713-717 skip_routine_block——"免疫下N次攻击伤害"票同面）；
//     ②真伤/粉伤不受 DAMAGE 票影响（管线分面）。族内 4 载体全 cat1/cat2 攻击技，
//     "对手的攻击"红伤主面已覆盖，边界不扩写不改内核。
//
// ══ 审计槽（K472/K480 布局同构：10^9 + id×10^4 + owner×16 + slot，slot≤9）══
//   1939（基 **1019390100** = 10^9+1939×10^4+0100 子块）：K472 孪生族口径——soul 1938
//     已在档（K392 锁伤异文）、同链 1938 moves 键取 +0100（K458 1565/K461 1638/K465
//     1863 先例），1939 作 1938 姊妹同取 +0100 给将来 soul 侧同基段留位。slot0=累计落地
//     次数（add 前后 quantity_of 差>0 才计）、1=末次附加回合、2={0}回显、3=末次 target、
//     4=末次附加后罩余量 quantity_of(1939)、5=累计附加总点数（按落地差值计）、6=累计
//     cast 计数（含 <=0 no-op）、7=末次 add 前余量、9=跳过原因（0=无/1=罩槽满落空）
//     ——与 k472 1938 布局逐槽同构（盾→罩）。
//   2334（基 1023340000 = 10^9+2334×10^4 平段；soul 2334 无行、K480 711 平段同款甄别）：
//     slot0={0}回显、1=末次探针暴击旗（1=暴）、2=末次暴击武装回合、3=末次免疫窗起点
//     （R+1）、4=末次免疫窗终点（R+{0} 含）、5=授票 tick 执行计数、6=末次授票回合、
//     7=累计暴击门通过数、8=累计 SKILL_EFFECT cast 计数、9=跳过原因（0=无/1={0}<=0
//     关闸档/2=缺原语）。
//   两基址 + 全部键位全库 grep 零占用（2026-11-19 现查）+ tools/check_storage_keys.py
//   过门复扫；owner×16 步长 > slot 上限 9 → 同基址 owner 间无交叠（K472 布局自防撞同款）。
//   两族键段共存：soul 1939/2334 双空间均无行（registry soul=415 现查零命中）——同号
//   共存拆分无需求，+0100 子块纯孪生族留位（T8712 同局双族交叉断言防串）。
//
// ══ 动工前置三查（2026-11-19，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep：1939/2334 裸 id 词界全扫 src/ include/ 零命中（无路由无硬编码无 case）。
//   ② effect_info 1939/2334 行即模板（args_num=1/1；同文查无重复族）；side_effect.arg_count
//     顺序切分 8 载体零剩余（上表）；custom_effect_overrides/programs 无两 id 行；
//     effect_des 两 id 无行；boss_invalid_effects.json 1939 在册（官方"boss 无效"机器
//     口径——DisabledEffects 装配期整体摘除，插件侧零工作量，K478 1446 同款记档）、
//     2334 不在册；**无 900000 拆分迁移与故意退役记录**（tools/db_patches/ 全目录 grep
//     两 id 零命中；tasklog/看板/06-交接 无"拆分/迁移/退役"记录）。
//   ③ registry 基线 moves=1590/soul=415/suit=4（build/registry.json 2026-11-19 dump）：
//     两 id 双空间零注册——真缺口。跨 worktree（k483/endgame-walk/web-replay/agents）
//     SKILL_EFFECT(1939|2334/SOUL_PROGRAM(1939|2334 零命中；constexpr 间接注册兜底
//     kMark1939/kMark2334 零命中。parser 覆盖面：两模板句形（"为对手附加{0}点护罩"/
//     "打出致命一击时下{0}回合…"）在 effect_unit_parser 条件模板集之外逐字核对无同形
//     （K480 同款核对口径），注册落地后 clone_effect 命中插件函数不再走 parser。
//
// ══ 变异测试实测（2026-11-19，本 worktree；9/9 真红→字节级还原→SHA==底本→强编
//   deploy 复绿；证据 build/k484-mutations/M*/：apply.diff+grep-confirm+
//   mutated-build.log/rc+dylib-sha-mutated+run.log/rc+first-kill+restored-build.log/rc
//   +dylib-sha-restored+rerun.log/rc 全链；macOS GNU Make 3.81 mtime 整数秒截断 →
//   驱动**每次 build 前删 .o 强编**+SHA 双守卫，K468/K472 记档沿用）══
//   底本 dylib SHA256 = 164e1ccedfece934c54e1b4b968bc4922a051f54920d7f7a6dccdd5ffc70a64f
//   （resources/moves_lib/liblib_1.dylib，变异轮源面）；每轮还原 cmp 字节级 + SHA 逐字
//   回底本 + 场景复绿（rc=0 / 138 断言）。⚠️ 本头注 M 节回填后源面已变，终稿 dylib
//   SHA256 = 3343d3af75c7f90db0fce9765b2ebbd69ad41f3ccf44fcad6ea9d73d06829e9d
//   （功能零差异——仅注释字节，k480 同款记档）。
//   M1  1939 方向翻转（罩落 self 而非 opp）→ 首杀 T8710.a（对手 quantity_of(1939)=0
//       ≠200；方向负测同杀）——作用方向锁定；
//   M2  1939 {0} 行参硬编码 300（36583 应 200）→ 首杀 T8710.a 同一断言（=300≠200；
//       与 M1 同杀点不同值，apply.diff 区分）——行参逐字面消费锁定；
//   M3  1939 通道置换（hood_bank_→shield_bank_ 粉伤槽→红伤槽）→ 首杀 T8710.a（罩面
//       读 0）+T8710.b 红粉分档全腿——盾/罩两面原语甄别锁定；
//   M4  2334 暴击门取反（!crit→crit）→ 首杀 T8711.a（门通过数=0≠1：恒暴腿门闭）；
//       T8711.b 负测腿同杀（crit0 门开误武装）——门方向正负双面锁定；
//   M5  2334 窗 InRounds 化（AFTER_ACTION 直授=当回合覆盖+末回合丢失）→ 首杀
//       T8711.a（R2 授票 tick=1≠0——直授无 next-round tick）+T8711.e 当回合落地腿
//       ——NextRounds"下{0}回合"起点锁定（d5 1091 通道）；
//   M6  审计键 owner 维度拉平（base+owner*16+slot→base+slot）→ 首杀 T8712.c（owner0
//       cast 计数=2≠1——双边施法串账）；⚠️ 首轮未杀记档：仅读 owner1 缺省值的
//       "absence 面"（T8710.d/T8712.a）对拉平不敏感（写位搬家后 absence 仍成立），
//       **正面碰撞面**（双边各施一次、各读各的）才是杀点——T8712.c 为此补立；
//   M7  2334 免疫时长行参硬编码 2（{0}=1 载体应 1）→ 首杀 T8711.c（R3 免疫事件=2
//       ≠1：单回合窗被拉长成两回合）——时长行参逐字面锁定；
//   M8  2334 票 scope ON_STAGE→TEAM（换宠不清票）→ 首杀 T8711.g（R3 换宠回合
//       行动面 rc=2 票仍在=未清）——离场清理锁定；
//   M9  2334 探针 acting_side 过滤摘除 → **无观察面变异，撤**（K448 MA/MD/MG 先例）：
//       桶按 (state, owner) 配对、execute_registered_actions 只跑行动方——双桶注册的
//       probe 只会在 self 自己行动的 AFTER_ACTION 桶执行，对手行动永不投递本条目
//       → 过滤恒真=纯防御性冗余（897/J12/J300/K480 同款保留，机制由桶结构本身承载）；
//   M10 2334 探针续命（duration=1→9）→ 首杀 T8711.h（R2 拍打暴击再武装 slot7=2≠1）；
//       ⚠️ 首轮未杀记档：T8711.f 的 R2 是**同载体再施放**——同 source_id 容器去重
//       把续命探针顶掉，变异不可见；换非载体出手腿（拍打）后容器无新探针可顶、
//       续命探针独跑暴露——探针生命周期锁定。
//   （概率/回退分支：两 id 均无掷点无回退子句——N/A 记档不立变异；0% 在位即消费
//   对 1939/2334 不适用。）

#include "common.h"

#include <utility>
#include <vector>

namespace {

// ── 审计键基址（见文件头「审计槽」节）──
constexpr int k1939AuditBase = 1019390100;   // 10^9 + 1939×10^4 + 0100 子块（孪生族留位）
constexpr int k2334AuditBase = 1023340000;   // 10^9 + 2334×10^4（soul 2334 无行，平段）
constexpr int kSourceHood = 1939;            // 护罩 source_id（池读数定位用）

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

// ══════════ 1939 为对手附加{0}点护罩（K472 1938 直系孪生，盾→罩面替换）══════════

// 1939 主体。int_args = [owner, target, {0}=点数]（args_num=1 + [owner,target] 前缀）。
// 罩落 **target（对手）**身上——J3 2005 直系先例（"盾上对手不是笔误"）+ 载体链原文互证。
EffectResult effect_1939_hood_to_opponent(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1939, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1939, args,
                     "1939 布局=[owner,target,点数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int points = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1939,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(points >= 0, 1939, args, "需要 点数>=0（0=自然 no-op）");
    set_storage_int_local(ctx, audit_key_of(k1939AuditBase, self, 6),
                          storage_int_local(ctx, audit_key_of(k1939AuditBase, self, 6)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1939AuditBase, self, 1), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k1939AuditBase, self, 2), points);
    set_storage_int_local(ctx, audit_key_of(k1939AuditBase, self, 3), opp);
    if (points <= 0) {
        return EffectResult::kOk;   // "附加 0 点护罩"=自然 no-op（不报故障）
    }
    const int before = ctx->getPet(opp).hood_bank_.quantity_of(/*source_id=*/kSourceHood);
    set_storage_int_local(ctx, audit_key_of(k1939AuditBase, self, 7), before);
    // J3 2005 直系孪生：普通罩 priority=0、source_id=效果 id、duration=0 常驻（粉伤槽）。
    ctx->getPet(opp).hood_bank_.add(/*priority=*/0, points, /*source_id=*/kSourceHood,
                                    /*duration=*/0);
    const int after = ctx->getPet(opp).hood_bank_.quantity_of(/*source_id=*/kSourceHood);
    if (after == before) {
        set_storage_int_local(ctx, audit_key_of(k1939AuditBase, self, 9), 1);   // 罩槽满落空
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key_of(k1939AuditBase, self, 0),
                          storage_int_local(ctx, audit_key_of(k1939AuditBase, self, 0)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1939AuditBase, self, 5),
                          storage_int_local(ctx, audit_key_of(k1939AuditBase, self, 5)) + after - before);
    set_storage_int_local(ctx, audit_key_of(k1939AuditBase, self, 4), after);
    return EffectResult::kOk;
}

// ══════════ 2334 打出致命一击时下{0}回合免疫对手的攻击 ══════════

// ── AFTER_ACTION 桶的当前行动方（897/J12/J300/K480 同款小工具，插件各自持有一份）──
int k484_bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

// 免疫票授予 tick：R+1 ROUND_START 一次性执行（d5 1091 直系通道——"下{0}回合"当回合
// 尾不覆盖）。payload = [self, opp, rounds]。同 source_id=2334 句柄复用刷新（重暴击
// 连续窗），scope=ON_STAGE（换宠 clear_on_stage 删票=离场清理）。
EffectResult k484_2334_grant_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(self, ImmunityType::DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds,
                        /*source_id=*/2334, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/2334,
                        /*tier=*/ImmunityTier::Modern);
    set_storage_int_local(ctx, audit_key_of(k2334AuditBase, self, 5),
                          storage_int_local(ctx, audit_key_of(k2334AuditBase, self, 5)) + 1);
    set_storage_int_local(ctx, audit_key_of(k2334AuditBase, self, 6), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k2334AuditBase, self, 3), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k2334AuditBase, self, 4),
                          ctx->roundCount + rounds - 1);
    return EffectResult::kOk;
}

// 2334 行动末探针：打出致命一击 → 注册下回合 ROUND_START 授票 tick。
// payload = [self, opp, rounds]。
EffectResult k484_2334_after_action(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (k484_bucket_acting_side(ctx) != self) {
        return EffectResult::kOk;   // 只在本侧行动结束那格结算（1557 同款）
    }
    // 判据：本次技能打出致命一击。crit_happened 每次技能使用先清（skills.cpp:845）、
    // ①.5 掷定（:988）；AFTER_ACTION 时点读到的是本次技能的结果（1557 直系）。
    const bool crit = ctx->crit_happened[self];
    set_storage_int_local(ctx, audit_key_of(k2334AuditBase, self, 1), crit ? 1 : 0);
    if (!crit) {
        return EffectResult::kOk;   // 未打出致命一击 → 不武装（门向与 1557 相反）
    }
    set_storage_int_local(ctx, audit_key_of(k2334AuditBase, self, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k2334AuditBase, self, 7),
                          storage_int_local(ctx, audit_key_of(k2334AuditBase, self, 7)) + 1);
    // "下{0}回合"：NextRounds 本回合不算 → R+1 ROUND_START tick 授票（d5 1091 通道；
    // register_next_rounds_effect 同 source_id 同源去重=同回合重暴击不叠 tick）。
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, /*effect_id=*/2334,
                                /*rounds=*/1, {self, opp, rounds}, &k484_2334_grant_tick);
    return EffectResult::kOk;
}

// 2334 主体。int_args = [owner, target, {0}=回合]（args_num=1 + [owner,target] 前缀）。
EffectResult effect_2334_crit_attack_immune_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2334, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2334, args,
                     "2334 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 2334,
                     args, "需要 owner/target ∈ {0,1}");
    set_storage_int_local(ctx, audit_key_of(k2334AuditBase, self, 0), rounds);
    set_storage_int_local(ctx, audit_key_of(k2334AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k2334AuditBase, self, 8)) + 1);
    // {0} 在位即消费：显式 0 = "下 0 回合免疫"自然关闸档（不武装不报故障，K314/K340
    // 纪律：不写 >0 守卫盖关闸档；负值 = 布局错误走 EFFECT_ARG_GUARD 面）。
    if (rounds <= 0) {
        set_storage_int_local(ctx, audit_key_of(k2334AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    // AFTER_ACTION 双桶探针（1557 同款：duration=1、source_id=2334 同源去重）。
    const std::vector<int> payload{self, opp, rounds};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(2334, 0, self, -1, EffectArgs(payload), &k484_2334_after_action);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 2334;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(1939, effect_1939_hood_to_opponent)
SKILL_EFFECT(2334, effect_2334_crit_attack_immune_window)
