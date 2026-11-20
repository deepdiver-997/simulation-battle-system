// K442 · 探索九批余量自选四族 —— 558「若自身不处于能力提升或下降状态时则恢复{0}点
// 体力」×4 + 1540「自身体力高于最大体力的1/{0}时吸取对手{1}点体力」×5 +
// 1448「自身体力低于最大体力的1/{0}时造成的伤害提升{1}%」×5 +
// 1773「附加双方能力下降等级总和X{0}的固定伤害」×6
//（场景 test/scenario/scenario_8300_k442_cond_gate_pool.cpp；号段 8300-8309 一文件
//  覆盖四族，scenario_8290 同式；2026-11-09，worktree k442，基点 = main-local 最新
//  含 K441 五族，registry moves=1503 / soul=411 起算）
//
// ══ registry 基线重对账（派发口径 K441 后余量 793 族 / 1840 载体）══
// 方法 = K438-K441 同款三方法：**json_each 精确元素匹配 × 主检出 DB + worktree DB
// 快照双库交叉 × 纯 Python 全表解析**（脚本 /tmp/k442/inv.py、/tmp/k442/inv2.py，
// 2026-11-09 现查）。主仓 ~/code/c++/project/sbs-local/scripts/data/processed/
// seer_unity.sqlite 与 worktree 拷贝 md5 同源 05bd9327c1ef46d31182227c6ee594a7
// （两份物理文件独立读；side_effect.arg_count=1（id 919 补丁）已重放核验）。
// 权威注册面 = ./build/sim_server --dump-registry-exit（build/registry_k442_baseline.
// txt）实测 **moves=1503 / soul=411**，与派发"registry 1503 基线"一致。
// ⚠️ 对账勘误（头注留痕）：现库盘点 = **793 族（与派发完全咬合）/ 1839 载体（并集
//   去重口径）**，与派发"1840 载体"差 1——差额为计数口径差：派发账按 side+friend
//   双列分列计（1840），本批按载体并集去重计（1839；恰 1 载体在两列各引用一个未实现
//   族），族账不受影响，记档。
// ⚠️ 三方法交叉分歧 1 条（已裁决）：id **900001** 在 moves.side_effect 列有引用（8
//   载体链尾）而 raw_json 无——装载器读 side_effect 列（official_data_repository.
//   cpp:159 build_skill_effects），900001 = J325 拆分效果（内核 case 900001，
//   skills.cpp:236-244；db_patch j3/1526_split_900001.sql），**在册非缺口**，纯
//   Python(raw_json) 解析法对它的"缺席"系 raw_json 列陈旧，json_each 法（权威列）
//   正确。dump 注册面含 900001/697/699（内核通道），三方法在**权威列+dump 口径**
//   下全咬合。
//
// ══ 选族与排除（候选池 = 现库 793 族，≤6 载体甜点区 776 族）══
// 排除判据沿 K430~K441 全部头注排除清单 + 纠错记档（逐条核对未取）：
//   - 674×6 不落地（未击败门系统性偏离待官方仲裁，K440 排除记档）；
//   - 929×6 归 K443 单（K441 排除记档：需内核"本回合受击累计查询面"，本批不碰）；
//   - 2190×13 哑模板判例（J319，K438 三方法复核不翻案）；
//   - 1565×7 / 1494×6 / 1457×6 非随机异常通道（随机 PP 归零类，K438 派发口径）；
//   - 41×7 无 effect_info 行 / 445×7 经济类 / 1538×8 内核缺口——不翻案；
//   - K433 7 载体遗留（2028/1459/1575/1645/1855/1799/1863/1627/2075）与 K434/K437
//     记档（91/546/1930/1559/2027/2128/2166/2226/1938/1150）逐条沿用；
//   - 本批另核（候选池复审新增排除，防复踏）：616/668/1484/1807/1810/2072/2083/
//     2247/2302/1251/953/171（条件先制族——"本技能先制"需 selection_effects_ 路由
//     = src/entities/skills.cpp 硬编码表，夜跑禁区 src/ 只读，K442 派发明示不做内
//     核改动；§七点五 J66 勘误口径）；705×6（互换体力无原语，直写 hp 禁令）；
//     634×6/178×5/518×5/1430×6/1074×5/1006×4（"造成的伤害>X"门需**已造成伤害量**
//     查询面——ws.guard_detect_damage 系受击链首单笔快照（K441 929 排除记档同源），
//     主动伤害量无一等公民查询面，留通道设计单）；798×6/1834×6（伤害 cap 通道
//     未建——两族同在未实现池即证无既有消费方）；1849×6（次数型增幅乘区通道未建，
//     套路目录 §四 明文）；1147×5（未击败门 = 674 同判据）；2012×5/1436×5/536×5/
//     2165×5/2027×6（击败/转移通道体量）。
// 【落地 ×4】全 ≤6 载体、通道全为一等公民既有先例、零内核改动：
//   · 558 = K440 734「若对手不处于…」的**自身侧镜像**（门=六槽 ability_levels
//     全零双向净空，k440_rand_cond_heal_pool.cpp:354-361 同表遍历取 owner 侧）+
//     heal_amount 原语（core_api.h:70；heal 实绩读 hp 前后差落审计——734 同法）。
//   · 1540 = 高体门（「高于」严格不含等号——batch_c_skills.cpp:1313 已在册同门
//     1447 直系）+ 双原语吸取（kernel drain_hp_amount 语义复刻：battle_primitives.
//     cpp:1993-2012 = 固定伤害 + heal_impl **全额无条件** + 臣服(31)短路 +
//     TARGET_DEFEATED 门；插件侧按 f_drain_family.cpp:34（1214/840 先例）字面口径
//     ——fixed_damage(core_api.h:103 粉伤路径) + heal_amount 全额（封回血生效），
//     臣服短路照 kernel 复刻（battle_primitives.cpp:2004-2008 直证））。
//   · 1448 = 低体门（「低于」严格不含等号——batch_c_skills.cpp:1336 已在册同门
//     1135 直系）+ 通用增伤 AMP 加写（ws.damage_add_pct[owner] += {1}，SKILL_EFFECT
//     时点先于 ATTACK_DAMAGE 结算本次即享——batch_c_skills.cpp:446（1985）/441
//     k_archive_pool.cpp:411（2100）直系；battleWorkspace.h:298 AMP 加算槽）。
//   · 1773 = 双方负槽**等级总和**计数（k439_count_boost_states 镜像取负求和而非
//     计种——模板字面「等级总和」，K439 1146 计**种数**系「每处于{0}种」措辞差异，
//     两模板逐字不同不共用）× {0} → fixed_damage（core_api.h:103，K439 1146
//     :461 同通道）。生存门 = 对手已倒跳过（K439 1146 :444 同口径）。
//
// ══ 载体切片（side_effect.arg_count 平面切片，脚本 /tmp/k442/slice.py 核算）══
//   558 [n]：15367 均衡之光[558][200] / 29059 蟾宫折桂[191,1231,558][4|80,7,3|300] /
//     35476 停息驻军[558][200] / 37754 折钢棱锋[999,558,2033][1,20|300|2]——零剩余。
//   1540 [denom,amount]：28506 掣电相伴[1540][2,200] / 29421 无畏印[191,2226,1135,
//     1540,444][4|1|2|2,350|] / 29435 扭曲片像[191,159,1540][4|2,100,0|2,300] /
//     33973 噬电处刑[1449,1448,1540][3,3|2,100|2,200] / 37720 螺旋破坏[1220,1540]
//     [2|2,300]——零剩余。
//   1448 [denom,pct]：32994 万籁静愔愔[945,1447,1448][|2,50,15|2,100] /
//     33774 疾光淬电[797,1448][2|2,100] / 33973 噬电处刑（同上，与 1540 同链咬合）/
//     36795 溃散之光[1448][2,100] / 37059 泛舟[1484,1448][|2,100]——零剩余。
//   1773 [mult]：29089 浩晦之枯海[1773,1791,1115][50|100,300,200|1] /
//     35664 浮光霭霭[1750,1439,1773][|100|50] / 36280 巧寒戏清宵[973,14,1773]
//     [100,15|100|65] / 36470 温柔触摸[1318,1013,1773,1934][10,1|1,50|50|50] /
//     38112 不觉寒[1542,1773][15,1|50] / 38358 极影绘风岚[2349,547,191,1953,1773]
//     [35,4|5|60|70]——38358 链首 2349 无 side_effect 行，按装载器兜底口径
//     effect_info.args_num=0 切片（official_data_repository.cpp:170-174 value_or）
//     全链零残留，本批唯一兜底载体记档。
// 纯链比（链上其余 id 全在册）：558 4/4 全纯；1540 4/5（29421 链 2226 未注册）；
//   1448 4/5（33973 链 1540——本批落地后归纯）；1773 3/6（29089 链 1791、36470 链
//   1934、38358 链 2349 未注册）——宏按 id 注册非按载体，混链载体断言面由纯链载体
//   覆盖（K438 1624 / K440 734 同口径记档）。
// 4399 文（reference/scrape_4399/out/pets_full.jsonl，md5 03a7cbe48f69b9d188c274a20
//   b333eaf 与 K440/K441 头注同源）19 载体逐条核对（按技能名匹配，库无技能 id 列）：
//   · 558 4/4 逐字：「若自身不处于能力提升或下降状态时则恢复200/300点体力」（恢复量
//     四处与切片 [200/300/200/300] 逐条吻合）。
//   · 1540 5/5 逐字：「自身体力高于最大体力的1/2时吸取对手200/350/300/200/300点
//     体力」（分母/量逐条吻合切片）。
//   · 1448 5/5 逐字：「自身体力低于最大体力的1/2时造成的伤害提升100%」。
//   · 1773 5/5 逐字：「附加双方能力下降等级总和X50/X65的固定伤害」（×50/×65 与切片
//     吻合）；38358 极影绘风岚**图鉴未收录**——按切片 [70]+模板落地，孤行记档
//     （K441 25726 同判据）。合计 19 收录载体逐字同义、0 偏离、1 未收录。
//
// ══ 四查（2026-11-09 本 worktree）══
// ① 内核硬编码：(^|[^0-9.])(558|1540|1448|1773)([^0-9.]|$) 对 src/ include/ grep
//   rc=1 **零命中**（/tmp/k442/base_grep.out 空）。
// ② effect_info 四 id 在列且 args_num = side_effect.arg_count（558=1/1540=2/1448=2/
//   1773=1，SQL 直查一致）；custom_effect_programs / custom_effect_overrides 四 id
//   0 命中。
// ③ 全库并集无注册：resources/ test/ `SKILL_EFFECT\((558|1540|1448|1773),` 零命中
//   （grep rc=1）；git grep 对 main-local / origin/main / feat/k437-k441 / feat/k443
//   / feat/j337 / feat/endgame-walk 与兄弟 worktree（k443、endgame-walk）同款零命中
//   （全部 rc=1）——真缺口。
// ④ parser 覆盖面（冻结不扩，逐字核对 src/effects/effect_unit_parser.cpp）：
//   558 含「不处于…状态」字样但**非**「不处于异常状态」（second-pass pattern 4 字面
//   不命中，:173）；1540 含「吸取对手」但**非**「吸取对手最大体力」（pattern 4 双标记
//   不命中，:174-175）；1448 无任何 pattern 字面；1773 的「附加+固定伤害」仅存在于
//   parse_fallback_unit（:137-143）而 fallback 只在「令/使{状态}」主句（:74）未触发
//   分支可达——四模板 parse_effect_unit 均返回 -1 纯惰性；注册落地后 clone_effect
//   命中插件函数（src/entities/skills.cpp:675 只在 effect.logic 为空时走 parser）。
// 审计键段（10^9 + id×10^4 + owner×16 + slot；tools/check_storage_keys.py --quiet
//   rc=0 无跨文件撞键；四基段 grep 全库零占用）：
//   558（1005580000）/ 1540（1015400000）/ 1448（1014480000）/ 1773（1017730000）。
//
// ══ 实现形态（同形套路照抄先例，通道零新增）══
//   · 时点 = SKILL_EFFECT（effect_register_state default；miss/被盔桶不跑——四模板
//     均为攻击技上效/状态上效字面，734/75/K440-K441 同口径；1448 五载体全 cat∈{1,2}
//     攻击技，damage_add_pct 只由红伤管线消费）。
//   · 生存门（K422/K438/K439/K440 口径）：1540/1773 目标（对手）已倒 → 整条跳过
//     （审计槽 9=1）；558 全自体无对手读数不设；1448 门读自身不设。
//   · 审计槽布局（K437-K441 同构）：
//     558：slot 0 行参 n / 1 自身提升种数 / 2 自身下降种数 / 3 门判定（1=净槽开/0=闭）/
//       4 heal 实绩（hp 前后差）/ 8 累计使用计数 / 9 跳过原因（0=无/2=原语槽缺失）
//     1540：slot 0 行参 denom / 1 行参 amount / 2 门判定（1=开/0=闭）/ 3 固伤实参 /
//       4 对手实扣 dealt / 5 回血实绩 / 6 臣服短路（1/0）/ 8 累计使用计数 /
//       9 跳过原因（0=无/1=对手已倒/2=原语槽缺失）
//     1448：slot 0 行参 denom / 1 行参 pct / 2 门判定（1=开/0=闭）/ 3 阈值（max/d）/
//       4 实测自身 hp / 5 开闸（加 AMP）次数 / 8 累计使用计数 / 9 跳过原因（0=无）
//     1773：slot 0 行参 mult / 1 自身下降等级总和 / 2 对手下降等级总和 / 3 总和 /
//       4 固伤实参 / 8 累计使用计数 / 9 跳过原因（0=无/1=对手已倒/2=原语槽缺失）
// 场景回归：test/scenario/scenario_8300_k442_cond_gate_pool.cpp（号段 8300-8309，
//   一文件覆盖四族，18 腿全绿）。
//
// ══ 变异测试（8 变异 = 每族 2，改→单跑必红→字节级还原→rm .o 强制重编+显式
//    moves_plugin_deploy 刷新 dylib→回绿；红点全部落在预期断言腿）══
//   M1 558 门漏下降向（boost!=0||drop!=0 → boost!=0）→ T3 下降腿红（1200≠1000），
//     T1/T2 仍绿——门闭双向缺一不可；
//   M2 558 恢复量减半（amount → amount/2）→ T1 红（1100≠1200）——行参量精确消费；
//   M3 1540 门改非严格（> → >=）→ T6 边界腿红（恰 1/2 弹 200≠0）——「高于」不含等号
//     （1447 同门判据锁定）；
//   M4 1540 回血改按实扣（amount → dealt）→ T7 红（300150≠300200）——字面全额口径
//     锁定（kernel drain_hp_amount heal_impl 无条件直证，防后人"顺手统一"成按实扣）；
//   M5 1448 门改非严格（< → <=）→ T10 边界腿红（dmg 48≠24=基线）——「低于」不含等号
//     （1135 同门判据锁定）；
//   M6 1448 增伤折半（pct → pct/2）→ T9b 红出 ×2 带宽（1.5× 出 ±2 带；K441 M10
//     同款 atk=20 抬基线分辨 ×2/×1.5）——行参 pct 精确消费；
//   M7 1773 求和改计种（-=lv → +1）→ T12 红（150≠200）——「等级总和」字面锁定，
//     防"顺手统一"成 1146 计种；
//   M8 1773 漏对手侧（opp_sum → 0）→ T12 红（150≠200，审计槽 2 同步=0）——「双方」
//     范围锁定。
//   还原后全绿复核：scenario_8300 ALL PASS（83 检查 0 FAILED）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k558AuditBase  = 1005580000;   // 10^9 + 558×10^4
constexpr int k1540AuditBase = 1015400000;   // 10^9 + 1540×10^4
constexpr int k1448AuditBase = 1014480000;   // 10^9 + 1448×10^4
constexpr int k1773AuditBase = 1017730000;   // 10^9 + 1773×10^4

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

// 六槽双向净空计数（K440 734 同表遍历，k440_rand_cond_heal_pool.cpp:354-361）。
void k442_count_boost_drop(const BattleContext* ctx, int side, int& boost_kinds,
                           int& drop_kinds) {
    boost_kinds = 0;
    drop_kinds = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int lv = ctx->ability_levels[side][i];
        if (lv > 0) {
            ++boost_kinds;
        } else if (lv < 0) {
            ++drop_kinds;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 558 —— 若自身不处于能力提升或下降状态时则恢复{0}点体力
// 行参布局 [owner, opp, n]。门 = 自身六槽 ability_levels 全零（双向净空，K440 734
// 自身侧镜像）；动作 = heal_amount(自身, {0})（heal 实绩读 hp 前后差落审计）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_558_clean_self_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 558, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 558, args,
                     "558 布局=[owner,opp,n]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     558, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 558, args, "需要 n > 0");

    if (!g_core || !g_core->heal_amount) {
        set_storage_int_local(ctx, audit_key(k558AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k558AuditBase, owner, 0), amount);
    int boost_kinds = 0;
    int drop_kinds = 0;
    k442_count_boost_drop(ctx, owner, boost_kinds, drop_kinds);
    set_storage_int_local(ctx, audit_key(k558AuditBase, owner, 1), boost_kinds);
    set_storage_int_local(ctx, audit_key(k558AuditBase, owner, 2), drop_kinds);
    if (boost_kinds != 0 || drop_kinds != 0) {
        // 门闭：自身带任一提升/下降 → 不恢复（显式清账可断）。
        set_storage_int_local(ctx, audit_key(k558AuditBase, owner, 3), 0);
        set_storage_int_local(ctx, audit_key(k558AuditBase, owner, 4), 0);
    } else {
        // 门开：自身净槽 → 恢复{0}点体力（半血观测防 clamp——坑集 §4/§9 口径由场景保证）。
        set_storage_int_local(ctx, audit_key(k558AuditBase, owner, 3), 1);
        const int hp0 = ctx->getPet(owner).hp;
        (void)g_core->heal_amount(ctx, owner, amount);
        set_storage_int_local(ctx, audit_key(k558AuditBase, owner, 4),
                              ctx->getPet(owner).hp - hp0);
    }
    bump_use_count(ctx, k558AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k558AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1540 —— 自身体力高于最大体力的1/{0}时吸取对手{1}点体力
// 行参布局 [owner, opp, denom, amount]。门 = 自身 hp > max_hp/{0}（「高于」严格不含
// 等号，1447 同门 batch_c_skills.cpp:1313）。吸取 = 双原语（kernel drain_hp_amount
// 语义复刻：固定伤害 + heal 全额无条件 + 臣服短路 + 对手已倒门——f_drain_family
// 1214/840 字面口径）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1540_hp_high_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1540, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1540, args,
                     "1540 布局=[owner,opp,denom,amount]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1540, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0 && amount > 0, 1540, args, "需要 {0}/{1} > 0");

    if (!g_core || !g_core->fixed_damage || !g_core->heal_amount) {
        set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    // 生存门：对手已倒 → 整条跳过（drain_hp_amount TARGET_DEFEATED 同口径）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 0), denom);
    set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 1), amount);
    const int threshold = max_hp_of(ctx, owner) / denom;
    const bool gate_open = ctx->getPet(owner).hp > threshold;   // 「高于」不含等号
    set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 2), gate_open ? 1 : 0);
    set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 3), 0);
    set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 4), 0);
    set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 5), 0);
    if (!gate_open) {
        bump_use_count(ctx, k1540AuditBase, owner);
        set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 9), 0);
        return EffectResult::kOk;
    }
    // 臣服(31)短路：吸取属"固定伤害+回等量体力" → 伤害被拦时也不该回血（kernel
    // drain_hp_amount 同款，battle_primitives.cpp:2004-2008 直证复刻；插件侧查询走
    // g_core->has_active_anomaly——core_api.h:218 注记 BattleContext 成员非 inline
    // 插件不可直调）。
    if (g_core->has_active_anomaly(ctx, owner,
                                   static_cast<int>(AbnormalStatusId::Submission))) {
        set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 6), 1);
        bump_use_count(ctx, k1540AuditBase, owner);
        set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 9), 0);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 3), amount);
    const int before = ctx->getPet(opp).hp;
    (void)g_core->fixed_damage(ctx, opp, amount);
    const int dealt = before - ctx->getPet(opp).hp;
    // 恢复走字面全额（heal_amount 吃封回血/恢复修正；f_drain_family 1214 内核 drain
    // 系口径：粉伤被护罩全吃也照回）。回血实绩 = hp 前后差（封回血时钳 0 可断）。
    const int self_hp0 = ctx->getPet(owner).hp;
    (void)g_core->heal_amount(ctx, owner, amount);
    set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 4), dealt);
    set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 5),
                          ctx->getPet(owner).hp - self_hp0);
    bump_use_count(ctx, k1540AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k1540AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1448 —— 自身体力低于最大体力的1/{0}时造成的伤害提升{1}%
// 行参布局 [owner, opp, denom, pct]。门 = 自身 hp < max_hp/{0}（「低于」严格不含
// 等号，1135 同门 batch_c_skills.cpp:1336）；动作 = ws.damage_add_pct[owner] += pct
//（SKILL_EFFECT 时点先于 ATTACK_DAMAGE → 本次攻击即享，1985/2100 直系）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1448_hp_low_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1448, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1448, args,
                     "1448 布局=[owner,opp,denom,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1448, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0 && pct > 0, 1448, args, "需要 {0}/{1} > 0");

    set_storage_int_local(ctx, audit_key(k1448AuditBase, owner, 0), denom);
    set_storage_int_local(ctx, audit_key(k1448AuditBase, owner, 1), pct);
    const int threshold = max_hp_of(ctx, owner) / denom;
    const int hp = ctx->getPet(owner).hp;
    const bool gate_open = hp < threshold;   // 「低于」不含等号
    set_storage_int_local(ctx, audit_key(k1448AuditBase, owner, 2), gate_open ? 1 : 0);
    set_storage_int_local(ctx, audit_key(k1448AuditBase, owner, 3), threshold);
    set_storage_int_local(ctx, audit_key(k1448AuditBase, owner, 4), hp);
    set_storage_int_local(ctx, audit_key(k1448AuditBase, owner, 5), 0);
    if (gate_open) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);
        set_storage_int_local(ctx, audit_key(k1448AuditBase, owner, 5),
                              storage_int_local(ctx, audit_key(k1448AuditBase, owner, 5), 0) + 1);
    }
    bump_use_count(ctx, k1448AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k1448AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1773 —— 附加双方能力下降等级总和X{0}的固定伤害
// 行参布局 [owner, opp, mult]。计数 = 双方六槽负等级**求和**（「等级总和」字面，
// K439 1146 计种系「每处于{0}种」措辞差异不共用）；固伤 = 总和 × {0} →
// fixed_damage（粉伤路径，1146 :461 同通道）。生存门 = 对手已倒跳过。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1773_drop_sum_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1773, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1773, args,
                     "1773 布局=[owner,opp,mult]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int mult = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1773, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(mult > 0, 1773, args, "需要 {0} > 0");

    if (!g_core || !g_core->fixed_damage) {
        set_storage_int_local(ctx, audit_key(k1773AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k1773AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k1773AuditBase, owner, 0), mult);
    int self_sum = 0;
    int opp_sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] < 0) {
            self_sum += -ctx->ability_levels[owner][i];
        }
        if (ctx->ability_levels[opp][i] < 0) {
            opp_sum += -ctx->ability_levels[opp][i];
        }
    }
    const int total = self_sum + opp_sum;
    const int dmg = total * mult;
    set_storage_int_local(ctx, audit_key(k1773AuditBase, owner, 1), self_sum);
    set_storage_int_local(ctx, audit_key(k1773AuditBase, owner, 2), opp_sum);
    set_storage_int_local(ctx, audit_key(k1773AuditBase, owner, 3), total);
    set_storage_int_local(ctx, audit_key(k1773AuditBase, owner, 4), dmg);
    if (dmg > 0) {
        (void)g_core->fixed_damage(ctx, opp, dmg);
    }
    bump_use_count(ctx, k1773AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k1773AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(558, effect_558_clean_self_heal)
SKILL_EFFECT(1540, effect_1540_hp_high_drain)
SKILL_EFFECT(1448, effect_1448_hp_low_amp)
SKILL_EFFECT(1773, effect_1773_drop_sum_fixed)
