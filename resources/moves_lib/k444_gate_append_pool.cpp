// K444 · 探索十批余量自选四族 —— 1680「对手不处于异常状态时附加{0}点真实伤害」×5 +
// 996「自身体力高于1/{0}时，附加自身当前体力{1}%的百分比伤害」×5 +
// 825「若对手处于能力提升状态则附加对手最大体力1/{0}的百分比伤害」×5 +
// 1477「消除敌我双方能力提升状态，消除任意一方成功则吸取对手最大体力的1/{0}」×5
//（场景 test/scenario/scenario_8320_k444_gate_append_pool.cpp；号段 8320-8329 一文件
//  覆盖四族，scenario_8300 同式；2026-11-09，worktree k444，基点 = main-local 最新
//  含 K442 四族 + K443 内核查询面/929，registry moves=1508 / soul=411 起算）
//
// ══ registry 基线重对账（派发口径 K443 后 registry 1508 基线）══
// 方法 = K438-K442 同款三方法：**json_each 精确元素匹配 × 主检出 DB + worktree DB
// 快照双库交叉 × 纯 Python 全表解析**（脚本 /tmp/k444/inv.py、/tmp/k444/slice.py，
// 2026-11-09 现查）。主仓 ~/code/c++/project/sbs-local/scripts/data/processed/
// seer_unity.sqlite（2026-11-09 19:16 管线重建 + apply_all.sh 全补丁重放，arg_count
// id=919=1 核验）与 worktree 软链同文件，md5 43663cc554dd156a2e239645d74cd402——
// 双库交叉为**同源库退化交叉**（K443 记档同口径：拷贝快照独立读 + 权威 dump 注册面
// 交叉补强）。权威注册面 = ./build/sim_server --dump-registry-exit
// （build/registry_k444_baseline.txt）实测 **moves=1508 / soul=411**，与派发
// "registry 1508 基线"一致。
// ⚠️ 对账记档：现库盘点 = **788 族（与派发口径逐族咬合）/ 2294 载体并集去重**——
//   K442 头注 793 族 − K442 落地 4（558/1540/1448/1773）− K443 落地 1（929）= 788，
//   完全咬合（本批盘点脚本用 dump 注册面做减法，K442 勘误口径：装载器读 side_effect
//   列，900001 在册非缺口——方法③ raw_json 陈旧对它的缺席仍现，方法①正确，同判）。
//   注册面除 moves_lib 裸 grep 外还含内核 case 通道 id（697/699 穿透凭证、900001 拆
//   分效果等）——K442 头注"dump 注册面含 900001/697/699（内核通道）"本批逐 id 复核
//   成立（src/entities/skills.cpp:277/281 case 697/699 直证），裸 grep 集会多出这 2 族
//   假缺口，已用 dump 对齐。
//
// ══ 选族与排除（候选池 = 现库 788 族，≤6 载体甜点区 774 族）══
// 排除判据沿 K430~K443 全部头注 + K442 复审新增 + 本批派发补充（逐条核对未取）：
//   - 674×6 不落地（未击败门系统性偏离待官方仲裁，K440 排除记档）；1147×5 同判据
//     （未击败门 = 674 同门，K442 记档）；
//   - 929 归 K443（已落地）；**1468×3 归 K445 单不碰**（K443 头注留档"37973 共链
//     1468 未实现——留后续立单"，本批派发明示 K445 worker 认领，跨 worktree grep
//     未见 k445 已实现，避让）；
//   - 2190×13 哑模板判例（J319，K438 三方法复核不翻案）；41×7 无 effect_info 行 /
//     445×7 经济类 / 1538×8 内核缺口——不翻案；
//   - 1565×7 / 1494×6 / 1457×6 非随机异常通道（随机 PP 类，K438 派发口径）；
//   - K433 7 载体遗留（2028/1459/1575/1645/1855/1799/1863/1627/2075）与 K434/K437
//     记档（91/546/1930/1559/2027/2128/2166/2226/1938/1150）逐条沿用；
//   - K442 复审新增排除防复踏：616/668/1484/1807/1810/2072/2083/2247/2302/1251/
//     953/171（条件先制族 = selection_effects_ 路由禁区，src/ 只读）；705×6（互换
//     体力无原语）；634/178/518/1430/1074/1006（"造成的伤害>X"门需**已造成伤害量**
//     查询面——K443 落地的 round_taken_attack_damage 是**受击**口径，不是已造成口径，
//     两查询面不可混用，派发明示）；798/1834（伤害 cap 通道未建）；1849（次数型增幅
//     乘区未建，套路目录 §四 明文）；2012/1436/536/2165/2027（击败/转移通道体量）。
//   - 本批另核（候选池复审新增排除，防复踏）：1640×5/1349×5（"出手时…打出致命一击"
//     ——暴击掷点在 query_usage ①.5（src/entities/skills.cpp:964，miss 后、技能效果
//     执行前），SKILL_EFFECT 时点写 ws.must_crit 赶不上本次攻击的暴击判定；改判据需
//     effect_register_state 路由进 EXTRA_ACTION 及更早时点 = src/ 硬编码表，夜跑禁区，
//     同 §七点五 J66 "case 版先手断言必挂"口径）；2067×6/1442×6（闪避窗 + PP 恢复
//     ——1559 "ignore_pp 通道待建"同域记档，PP 通道未建）；1838×6/1786×6/1369×6/
//     1196×6/1389×6（"命中效果失效/攻击技能无效/回合类效果被消除"窗——命中效果失效
//     票与回合类效果消除/被消除事件通道未核一等公民，非本批体量）；1416×6（消耗全部
//     体力直写 hp 禁令 + 下只精灵跨宠窗）；2474×5（"信仰对象"查询面缺位）；2230×6/
//     2060×5/2231×5（"若未恢复体力"判 HEAL_BLOCK 落地窗/消耗体力多通道复合，窗口
//     tick 体量超本批）；1688×5（1477 超集 + 回合类效果消除，1477 落地后余量重盘）。
// 【落地 ×4】全 ≤6 载体、通道全为一等公民既有先例、零内核改动：
//   · 1680 = 异常门（对手不处于**任意**异常 → has_any_active_abnormal_status 取反，
//     battleContext.h:797 全官方异常槽遍历；1339 激活条件判据直系，
//     k433_tiandi_gongchuang_pool.cpp effect_1339）+ 真伤通道（deal_true_damage
//     穿盾/罩/抗性/免疫，core_api.h:145；1820 SKILL_EFFECT 时点真伤直系，
//     burn_trio_skills.cpp:1414-1431）。
//   · 996 = 高体门（hp > max_hp/{0} 严格不含等号——1540/K442 同门 1447 直系，
//     batch_c_skills.cpp:1313）+ 百分比伤害（PERCENT_VALUE 指定数值档，粉伤·百分比
//     抗性/护罩管辖区——battle_primitives.h:327 语义注释 + 799 "等量百分比伤害"
//     PERCENT_VALUE 直系 batch_b_skills.cpp:2654-2656）；量 = 自身当前体力 ×{1}%
//     （int 乘区 checked_pct_mul 溢出响亮报错，calculation.h:18；K443 929 ×N 同款
//     pct_delta 折算）。
//   · 825 = 提升门（对手六槽 ability_levels 任一 >0——§七点五 门判据②型；K440 734
//     同表遍历 k440_rand_cond_heal_pool.cpp:354-361 取 opp 侧正向）+ 百分比伤害
//     （PERCENT_VALUE，量 = 对手最大体力 1/{0}——max_hp 读 numericalBase[HP] 权威槽，
//     common.h:311 max_hp_of / drain_hp 同读法 battle_primitives.cpp:1983-1985；
//     1211 "消除任意一方成功"家族同域 batch_c_skills.cpp:1188）。
//   · 1477 = 双侧消除（clear_stat_boosts 真原语 {owner, 1-owner} 循环累计清除数——
//     1211 直系 batch_c_skills.cpp:1197-1200，只提升侧不核下降）+ 成功门（任一侧
//     清除数 >0）+ 吸取（g_core->drain_hp 分数版真原语：目标 max_hp/denom 固定伤害 +
//     heal_impl 等量，battle_primitives.cpp:1972-1991 内核集中语义；1062 吸取族先例
//     f_drain_family.cpp:97-113 同槽调用）。
//
// ══ 载体切片（side_effect.arg_count = effect_info.args_num 平面切片，脚本
//    /tmp/k444/slice.py 核算，2026-11-09；四 id SQL 直查 arg_count=args_num 一致，
//    custom_effect_programs/overrides/combo_rules/selection_routes 四 id 全 0 命中）══
//   1680 [n]（args_num=1）：34983 彼岸挽尘花[881,1221,1048,1680][1,50,200→200] /
//     35792 凡尘弥天蚀[1056,1048,1680][2,100,250→250] / 36847 锦·撷芳绘岚霞
//     [1414,1680,1245,2075][10,250,2,2→250] / 37195 王·千春共绽[760,699,2123,1680]
//     [3,200→200] / 38433 创·黄金纪元[1956,2034,1680,1163][…→200]——零剩余。
//   996 [denom,pct]（args_num=2）：19784 光宇天轮诀[996,997][2,30|2,4] /
//     30840 时空枪击[928,996][1|2,30] / 35988 末路流沙[996][2,30] /
//     36966 妙偶咒言[996][2,30] / 37483 生死盘灵[1080,996,997,1933][2,30|…]——
//     五载体 [2,30] 全同参、零剩余。
//   825 [denom]（args_num=1）：17992 超能碎地拳[85,825][3] / 36438 雷牙之撕[825][3] /
//     36623 刺骨凛[2000,494,825][3] / 36624 葬生影[2000,494,825][3] /
//     37507 措手不及[825,810][3,100→3]——五载体 [3] 全同参、零剩余。
//   1477 [denom]（args_num=1）：29092 哀·逆命不却[2083,191,738,2311,1477][…→3] /
//     35135 黑暗扭转[1477][3] / 36849 根源调和[1477][3] / 37998 愈色森罗
//     [2192,1477,718][3,3,2→3] / 38494 神魔灭绝劈[2554,2498,1477,820,2555][…→3]——
//     五载体 [3] 全同参、零剩余。
// 纯链比（链上其余 id 全在册）：1680 3/5（36847 链 2075、37195 链 2123 未注册）；
//   996 **5/5 全纯**；825 **5/5 全纯**；1477 2/5（29092 链 2083/2311、37998 链 2192、
//   38494 链 2554/2555 未注册）——宏按 id 注册非按载体，混链载体断言面由纯链载体
//   覆盖（K438 1624 / K440 734 / K442 1773 同口径记档）。
// 4399 文（reference/scrape_4399/out/pets_full.jsonl，md5 03a7cbe48f69b9d188c274a20
//   b333eaf 与 K440-K442 头注同源）20 载体逐条核对（按技能名匹配，15 收录 5 未收录）：
//   · 1680 3/3 逐字：「对手不处于异常状态时附加250/250/200点真实伤害」（凡尘弥天蚀/
//     锦·撷芳绘岚霞/创·黄金纪元，量与切片 [250/250/200] 逐条吻合；"真实伤害"与
//     effect_info 模板同文——TRUE_DMG 通道选型直证）。
//   · 996 5/5 逐字：「自身体力高于1/2时，附加自身当前体力30%的百分比伤害」
//     （时空枪击/妙偶咒言/生死盘灵三处与模板逐字同文；光宇天轮诀/末路流沙两处图鉴
//     文作「固定伤害」——**变体记档**：3/5 赞同模板「百分比」+ effect_info 为权威
//     （K439 30484 判例：以 effect_info 模板为权威），非 K440 674 式 6/6 系统性门
//     偏离，不排除、按模板落地）。
//   · 825 5/5 收录：刺骨凛/葬生影/措手不及三处逐字「若对手处于能力提升状态则附加
//     对手最大体力1/3的百分比伤害」；超能碎地拳/雷牙之撕两处图鉴文作「减少对手1/3
//     最大体力」——同上变体记档（3/5 赞同模板「百分比伤害」，按模板落地）。
//   · 1477 2/2 逐字：「消除敌我双方能力提升状态，消除任意一方成功则吸取对手最大体力
//     的1/3」（根源调和/愈色森罗，与 effect_info 模板逐字同文）。
//   未收录 5：彼岸挽尘花/王·千春共绽/哀·逆命不却/黑暗扭转/神魔灭绝劈——按切片落地，
//   孤行记档（K441 25726 / K442 38358 同判据）。
//
// ══ 四查（2026-11-09 本 worktree）══
// ① 内核硬编码：(^|[^0-9.])(1680|996|825|1477)([^0-9.]|$) 对 src/ include/ grep
//   rc=1 **零命中**（/tmp/k444/kernel_grep.out 空）。
// ② effect_info 四 id 在列且 args_num = side_effect.arg_count（1680=1/996=2/825=1/
//   1477=1，SQL JOIN 直查 OK 三行全等）；custom_effect_programs / custom_effect_
//   overrides / custom_skill_combo_rules / custom_skill_selection_routes 四 id
//   0 命中。
// ③ 全库并集无注册：resources/ test/ examples/ `SKILL_EFFECT\((1680|996|825|1477),`
//   零命中（grep rc=1）；git grep 对 main-local 零命中（rc=1）；兄弟 worktree k445
//   （resources/ test/ examples/）同款零命中（rc=1）——真缺口。
// ④ parser 覆盖面（冻结不扩，逐字核对 src/effects/effect_unit_parser.cpp）：
//   1680 含「不处于异常状态」但**非**「吸取对手最大体力」组合（second-pass pattern 4
//   双标记不命中，:173-175）；「附加+真实伤害」≠ fallback pattern 3 的「附加+固定伤害」
//   （:132-133）；996/825 的「百分比伤害」≠「固定伤害」（:133）；1477 含「吸取对手
//   最大体力」但**非**「不处于异常状态」组合（:173-175），主句无「令/使{状态}」形
//   → parse_effect_unit 四模板均返回 -1 纯惰性；注册落地后 clone_effect 命中插件函数
//   （src/entities/skills.cpp:675 只在 effect.logic 为空时走 parser）。
// 审计键段（10^9 + id×10^4 + owner×16 + slot；tools/check_storage_keys.py --quiet
//   rc=0 无跨文件撞键；四基段 grep 全库零占用）：
//   1680（1016800000）/ 996（1009960000）/ 825（1008250000）/ 1477（1014770000）。
//
// ══ 实现形态（同形套路照抄先例，通道零新增）══
//   · 时点 = SKILL_EFFECT（effect_register_state default；miss/被盔桶不跑——四模板
//     均为攻击技上效字面，K440-K442 同口径）。
//   · 生存门（K422/K438/K439/K440/K442 口径）：四族动作全打对手 → 对手已倒整条跳过
//     （审计槽 9=1）。
//   · 审计槽布局（K437-K442 同构）：
//     1680：slot 0 行参 n / 1 门判定（1=对手无异常门开/0=闭）/ 2 对手异常实测（0/1）/
//       3 真伤实参 / 4 对手实扣 dealt（hp 差）/ 8 累计使用计数 / 9 跳过原因
//       （0=无/1=对手已倒/2=缺原语槽）
//     996：slot 0 行参 denom / 1 行参 pct / 2 门判定（1=开/0=闭）/ 3 阈值（max/d）/
//       4 实测自身 hp / 5 百分比伤害实参（checked_pct_mul 折算值）/ 6 对手实扣 dealt /
//       8 累计使用计数 / 9 跳过原因（0=无/1=对手已倒/2=缺原语槽）
//     825：slot 0 行参 denom / 1 对手提升种数 / 2 门判定（1=开/0=闭）/ 3 伤害实参
//       （opp_max/d）/ 4 对手实扣 dealt / 8 累计使用计数 / 9 跳过原因（0=无/1=对手
//       已倒/2=缺原语槽）
//     1477：slot 0 行参 denom / 1 自身清除项数 / 2 对手清除项数 / 3 成功门（1=任一
//       成功/0=双空）/ 4 对手实扣 dealt（drain hp 差）/ 5 自身回血实绩（hp 差）/
//       6 drain 返回档（DrainHpResult）/ 8 累计使用计数 / 9 跳过原因（0=无/1=对手
//       已倒/2=缺原语槽/3=无成功消除）
// 场景回归：test/scenario/scenario_8320_k444_gate_append_pool.cpp（号段 8320-8329，
//   一文件覆盖四族，21 腿 110 检查全绿：1680 门开/门闭/双变体 4 腿；996 门开/边界/
//   低于/三变体 7 腿；825 门开/净槽闭/只降闭/变体 4 腿；1477 双侧/opp 单边/self 单边/
//   双净/双变体 6 腿）。
//
// ══ 变异测试（8 变异 = 每族 2，改→单跑必红→字节级还原（cmp/md5 双验）→rm .o 强制
//    重编+显式 moves_plugin_deploy 刷新 dylib→回绿；红点全部落在预期断言腿）══
//   M1 1680 门方向反转（==0 → !=0）→ T1 门判定红（0≠1）——「不处于异常状态」
//     门向锁定（T2 门闭腿同步证门闭不附伤）；
//   M2 1680 真伤减半（amount → amount/2）→ T1 对手实扣红（100≠200）——行参量
//     精确消费（真伤通道量值）；
//   M3 996 门改非严格（> → >=）→ T6 边界腿红（门判定 1≠0）——「高于」不含等号
//     （1447 同门判据锁定，K442 M3 同款）；
//   M4 996 百分比折半（checked_pct_mul 结果 /2）→ T5 伤害实参红（45000≠90000）
//     ——{1}% 逐字面消费锁定（防后人"顺手"改口径，坑集 §7 同族）；
//   M5 825 负槽计入提升（>0 → !=0）→ T14 提升种数红（1≠0）——「能力提升」只数
//     正槽锁定（§七点五 门判据②型）；
//   M6 825 量读自身最大体力（opp → owner）→ T12 伤害实参红（133333≠200000）
//     ——「对手最大体力」主体锁定；
//   M7 1477 成功门强制开（cleared>0||… → true）→ T19 成功门红（1≠0）——「消除
//     任意一方成功」条件门锁定（双净不清零不吸取）；
//   M8 1477 吸取分母翻倍（denom → denom×2）→ T16 对手实扣红（100000≠200000）
//     ——「最大体力的1/{0}」分母字面锁定。
//   还原后全绿复核：scenario_8320 ALL PASS（110 检查 0 FAILED）。

#include "common.h"

#include <numerical-calculation/calculation.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int k1680AuditBase = 1016800000;   // 10^9 + 1680×10^4
constexpr int k996AuditBase  = 1009960000;   // 10^9 + 996×10^4
constexpr int k825AuditBase  = 1008250000;   // 10^9 + 825×10^4
constexpr int k1477AuditBase = 1014770000;   // 10^9 + 1477×10^4

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

// 六槽提升种数（K440 734 同表遍历，k440_rand_cond_heal_pool.cpp:354-361 取正向）。
int count_boost_kinds(const BattleContext* ctx, int side) {
    int kinds = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            ++kinds;
        }
    }
    return kinds;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1680 —— 对手不处于异常状态时附加{0}点真实伤害
// 行参布局 [owner, opp, n]（args_num=1，loader 注入 owner/target 前缀 + 行参原序）。
// 门 = !has_any_active_abnormal_status(opp)（battleContext.h:797 全官方异常槽遍历，
// 1339 判据直系）；动作 = deal_true_damage（core_api.h:145，1820 直系——真伤穿
// 护盾/护罩/抗性/免疫，"真实伤害"与粉伤 FIXED 分属两档，battle_primitives.h:330）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1680_no_anomaly_true_damage(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1680, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1680, args,
                     "1680 布局=[owner,opp,n]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1680, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 1680, args, "需要 n > 0");

    set_storage_int_local(ctx, audit_key(k1680AuditBase, owner, 0), amount);
    if (!g_core || !g_core->deal_true_damage) {
        set_storage_int_local(ctx, audit_key(k1680AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    // 生存门（K442 口径）：对手已倒 → 整条跳过。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k1680AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    // 激活条件 = 对手不处于任意异常状态（1339 判据直系，激活时点求值）。
    const int opp_anomaly = ctx->has_any_active_abnormal_status(opp) ? 1 : 0;
    set_storage_int_local(ctx, audit_key(k1680AuditBase, owner, 2), opp_anomaly);
    const bool cond = opp_anomaly == 0;
    set_storage_int_local(ctx, audit_key(k1680AuditBase, owner, 1), cond ? 1 : 0);
    if (!cond) {
        set_storage_int_local(ctx, audit_key(k1680AuditBase, owner, 3), 0);
        set_storage_int_local(ctx, audit_key(k1680AuditBase, owner, 4), 0);
        bump_use_count(ctx, k1680AuditBase, owner);
        set_storage_int_local(ctx, audit_key(k1680AuditBase, owner, 9), 0);
        return EffectResult::kOk;
    }
    const int before = ctx->getPet(opp).hp;
    g_core->deal_true_damage(ctx, opp, amount, owner);
    const int dealt = before - ctx->getPet(opp).hp;
    set_storage_int_local(ctx, audit_key(k1680AuditBase, owner, 3), amount);
    set_storage_int_local(ctx, audit_key(k1680AuditBase, owner, 4), dealt);
    bump_use_count(ctx, k1680AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k1680AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 996 —— 自身体力高于1/{0}时，附加自身当前体力{1}%的百分比伤害
// 行参布局 [owner, opp, denom, pct]。门 = hp > max_hp/denom 严格不含等号（1447 同门
// 判据 batch_c_skills.cpp:1313）；量 = hp × pct%（checked_pct_mul 溢出响亮报错）；
// 落点 = deal_pink_damage(PERCENT_VALUE)（值由来源算出、走百分比抗性/护罩，
// battle_primitives.h:327；799 "等量百分比伤害"同槽直系 batch_b_skills.cpp:2654）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_996_hp_high_percent_damage(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 996, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 996, args,
                     "996 布局=[owner,opp,denom,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     996, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0 && pct > 0, 996, args, "需要 denom/pct > 0");

    set_storage_int_local(ctx, audit_key(k996AuditBase, owner, 0), denom);
    set_storage_int_local(ctx, audit_key(k996AuditBase, owner, 1), pct);
    if (!g_core || !g_core->deal_pink_damage) {
        set_storage_int_local(ctx, audit_key(k996AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k996AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    const int threshold = max_hp_of(ctx, owner) / denom;
    const int self_hp = ctx->getPet(owner).hp;
    set_storage_int_local(ctx, audit_key(k996AuditBase, owner, 3), threshold);
    set_storage_int_local(ctx, audit_key(k996AuditBase, owner, 4), self_hp);
    const bool cond = self_hp > threshold;   // "高于"严格不含等号（1447 同门判据）
    set_storage_int_local(ctx, audit_key(k996AuditBase, owner, 2), cond ? 1 : 0);
    if (!cond) {
        set_storage_int_local(ctx, audit_key(k996AuditBase, owner, 5), 0);
        set_storage_int_local(ctx, audit_key(k996AuditBase, owner, 6), 0);
        bump_use_count(ctx, k996AuditBase, owner);
        set_storage_int_local(ctx, audit_key(k996AuditBase, owner, 9), 0);
        return EffectResult::kOk;
    }
    // 量 = 自身当前体力 ×{1}%（int 乘区 checked_pct_mul：pct_delta = pct-100）。
    const int dmg = checked_pct_mul(self_hp, pct - 100, "996 自身体力百分比乘区");
    const int before = ctx->getPet(opp).hp;
    (void)g_core->deal_pink_damage(ctx, opp, dmg, DamageKind::PERCENT_VALUE, owner);
    const int dealt = before - ctx->getPet(opp).hp;
    set_storage_int_local(ctx, audit_key(k996AuditBase, owner, 5), dmg);
    set_storage_int_local(ctx, audit_key(k996AuditBase, owner, 6), dealt);
    bump_use_count(ctx, k996AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k996AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 825 —— 若对手处于能力提升状态则附加对手最大体力1/{0}的百分比伤害
// 行参布局 [owner, opp, denom]。门 = 对手六槽 ability_levels 任一 >0（§七点五 门判据
// ②型，K440 734 同表遍历 opp 侧）；量 = 对手最大体力/denom（numericalBase[HP] 权威
// 槽，drain_hp 同读法）；落点 = PERCENT_VALUE（同 996，799 直系）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_825_opp_boost_percent_damage(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 825, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 825, args,
                     "825 布局=[owner,opp,denom]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     825, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 825, args, "需要 denom > 0");

    set_storage_int_local(ctx, audit_key(k825AuditBase, owner, 0), denom);
    if (!g_core || !g_core->deal_pink_damage) {
        set_storage_int_local(ctx, audit_key(k825AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k825AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    const int boost_kinds = count_boost_kinds(ctx, opp);
    set_storage_int_local(ctx, audit_key(k825AuditBase, owner, 1), boost_kinds);
    const bool cond = boost_kinds > 0;
    set_storage_int_local(ctx, audit_key(k825AuditBase, owner, 2), cond ? 1 : 0);
    const int dmg = std::max(1, max_hp_of(ctx, opp) / denom);
    set_storage_int_local(ctx, audit_key(k825AuditBase, owner, 3), dmg);
    if (!cond) {
        set_storage_int_local(ctx, audit_key(k825AuditBase, owner, 4), 0);
        bump_use_count(ctx, k825AuditBase, owner);
        set_storage_int_local(ctx, audit_key(k825AuditBase, owner, 9), 0);
        return EffectResult::kOk;
    }
    const int before = ctx->getPet(opp).hp;
    (void)g_core->deal_pink_damage(ctx, opp, dmg, DamageKind::PERCENT_VALUE, owner);
    const int dealt = before - ctx->getPet(opp).hp;
    set_storage_int_local(ctx, audit_key(k825AuditBase, owner, 4), dealt);
    bump_use_count(ctx, k825AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k825AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1477 —— 消除敌我双方能力提升状态，消除任意一方成功则吸取对手最大体力的1/{0}
// 行参布局 [owner, opp, denom]。消除 = clear_stat_boosts 真原语 {owner, 1-owner} 循环
// 累计（1211 直系 batch_c_skills.cpp:1197-1200，本族只核提升侧）；吸取 =
// g_core->drain_hp 分数版（目标 max_hp/denom 固定伤害 + heal_impl 等量无条件——
// 封回血生效，battle_primitives.cpp:1972-1991 内核集中语义；1062 同槽先例
// f_drain_family.cpp:97-113）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1477_clear_both_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1477, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1477, args,
                     "1477 布局=[owner,opp,denom]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1477, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1477, args, "需要 denom > 0");

    set_storage_int_local(ctx, audit_key(k1477AuditBase, owner, 0), denom);
    if (!g_core || !g_core->clear_stat_boosts || !g_core->drain_hp) {
        set_storage_int_local(ctx, audit_key(k1477AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k1477AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    // 消除敌我双方提升（1211 {owner, 1-owner} 循环直系）。
    const int cleared_self = g_core->clear_stat_boosts(ctx, owner);
    const int cleared_opp = g_core->clear_stat_boosts(ctx, opp);
    set_storage_int_local(ctx, audit_key(k1477AuditBase, owner, 1), cleared_self);
    set_storage_int_local(ctx, audit_key(k1477AuditBase, owner, 2), cleared_opp);
    const bool any_success = cleared_self > 0 || cleared_opp > 0;
    set_storage_int_local(ctx, audit_key(k1477AuditBase, owner, 3), any_success ? 1 : 0);
    if (!any_success) {
        set_storage_int_local(ctx, audit_key(k1477AuditBase, owner, 4), 0);
        set_storage_int_local(ctx, audit_key(k1477AuditBase, owner, 5), 0);
        bump_use_count(ctx, k1477AuditBase, owner);
        set_storage_int_local(ctx, audit_key(k1477AuditBase, owner, 9), 3);
        return EffectResult::kOk;
    }
    // 吸取对手最大体力 1/{0}（kernel drain_hp 分数版真原语；实扣/回血走 hp 差审计）。
    const int foe_before = ctx->getPet(opp).hp;
    const int self_before = ctx->getPet(owner).hp;
    const DrainHpResult dr = g_core->drain_hp(ctx, /*actor=*/owner, /*target=*/opp, denom);
    set_storage_int_local(ctx, audit_key(k1477AuditBase, owner, 4), foe_before - ctx->getPet(opp).hp);
    set_storage_int_local(ctx, audit_key(k1477AuditBase, owner, 5), ctx->getPet(owner).hp - self_before);
    set_storage_int_local(ctx, audit_key(k1477AuditBase, owner, 6), static_cast<int>(dr));
    bump_use_count(ctx, k1477AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k1477AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1680, effect_1680_no_anomaly_true_damage)
SKILL_EFFECT(996, effect_996_hp_high_percent_damage)
SKILL_EFFECT(825, effect_825_opp_boost_percent_damage)
SKILL_EFFECT(1477, effect_1477_clear_both_drain)
