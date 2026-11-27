// K446 · 探索十一批余量自选六族 —— 2080「自身处于护盾状态时威力提升{0}%」×4 +
// 1844「附加对手已损失体力值{0}%的百分比伤害」×4 +
// 1653「释放技能时对手每残留{0}%的体力值则此技能附加{1}点固定伤害」×3 +
// 1972「对方场下每死亡一只精灵则附加给对手{0}点固定伤害」×4 +
// 1835「100%令对手全属性+{0}」×4 +
// 2308「打出致命一击时令对手随机{0}个技能PP值归0」×3
//（场景 test/scenario/scenario_8340_k446_pool.cpp；号段 8340-8349 一文件覆盖六族，
//  scenario_8320/8330 同式；2026-11-09，worktree k446，基点 = main-local 最新含
//  K444 四族（1680/996/825/1477）+ K445 的 1468，registry moves=1513 / soul=411 起算）
//
// ══ registry 基线重对账（派发口径 K445 后 registry 1513 基线）══
// 方法 = K438-K445 同款三方法：**json_each 精确元素匹配 × 主检出 DB + 独立快照双库
// 交叉 × 纯 Python 全表解析**（脚本 /tmp/k446/inv.py、/tmp/k446/slice2.py，
// 2026-11-09 现查）。主库 ~/code/c++/project/sbs-local/scripts/data/processed/
// seer_unity.sqlite（worktree scripts/data 软链同文件，md5 2d94f45050c98ec0586b201ce
// 52d939b——2026-11-09 19:16 管线重建 + apply_all 重放后的现库，与 K444 头注的
// 43663cc5 不同源版本、与 20261022 快照 e800c441 独立），快照库
// ~/.agent-workspaces/db_backup/seer_unity_20261022.sqlite（arg_count id=919=1 双库
// 均核验）。权威注册面 = ./build/sim_server --dump-registry-exit
// （build/registry.json）实测 **moves=1513 / soul=411**，与派发一致。
// ⚠️ 对账记档：现库盘点 = **783 族（与派发口径逐族咬合）/ 1533 载体并集去重**——
//   K444 头注 788 族 − K444 落地 4（1680/996/825/1477）− K445 落地 1（1468）= 783，
//   完全咬合（本批盘点用 dump 注册面做减法；方法①装载器口径读 side_effect 权威列
//   ——900001 类内核拆分效果在册非缺口；裸 grep 注册集会多出 697/699 内核 case 通道
//   假缺口，以 dump 注册面为准，K442/K444 同口径）。方法②双库交叉差异仅 900001
//   一族（快照库早于 D15 拆库补丁的 raw_json 陈旧缺席——与 K442/K444 记档同因，
//   dump 面在册，非缺口）；方法① × ③ 全表差异 0 族。≤6 载体候选 769 族，排除
//   判据沿 K430~K445 全部头注 + 派发新增（条件先制族 616/668/1484/1807/1810/2072/
//   2083/2247/2302/1251/953/171；暴击掷点 1640/1349；PP 通道 2067/1442；cap 通道
//   798/1834；次数型乘区 1849；直写 hp 1416；伤害查询面 634/178/518/1430/1074/1006；
//   信仰对象 2474；674/1147 仲裁；1688 归 K447；2190 哑模板/41 无 effect_info 行/
//   445 经济类/1538 内核缺口/1565/1494/1457 随机 PP 类/K433 七载体/K434·K437 记档/
//   705 互换体力/2012/1436/536/2165 击败转移/1838/1786/1369/1196/1389 命中失效窗/
//   2230/2060/2231 HEAL_BLOCK 窗）后余 715 族，全部 ≤6 载体（分布 1:381/2:128/3:104/
//   4:70/5:30/6:2）。盘点脚本与逐族明细：/tmp/k446/{inv,filter,tmpl,slice2}.py、
//   cand_detail.json（715 族全量导出，剩余族留给后续批次）。
//
// ══ 选族与裁决（六族全部 ≤6 载体、通道全一等公民既有先例、零内核改动）══
//   · 2080 = 盾态门（shield_bank_ 查询：K445 1468 清盾同字段 k445_purge_both_fixed_
//     pp.cpp:169 total()；burn_trio_skills.cpp:1175 empty() 双查先例）+ 变威力
//     （ws.skill_power_view SKILL_EFFECT 时点写：burn_trio 53/1377 同款
//     burn_trio_skills.cpp:525/1451「view.value>0 守卫 + 整体赋值自动置 rewritten」，
//     batch_b_skills.cpp:560-562 102 翻倍直系；battleWorkspace.h:66-87 SkillView
//     「效果侧写入=声明变威力」头注）。量 = ×(100+{0})/100 → checked_pct_mul
//     （calculation.h:18，溢出响亮报错）。动作全打自身 → 不设对手生存门（K444 口径
//     只约束"动作打对手"的族）。
//   · 1844 = 对手已损失体力查询（max_hp_of - hp：514 直系 kongyuan_skills.cpp:442-461
//     「附加对手已损失生命值{0}%的百分比伤害」同形逐字）+ PERCENT_VALUE 指定数值档
//     （battle_primitives.h:327 头注点名本族语义「附加自身已损失体力50%的百分比伤害」
//     ——值由来源算出、走百分比抗性/护罩）。对手生存门 + 实扣审计（K443/K445 口径）。
//   · 1653 = 残留步数（floor(hp×100/max/{0}) 整数除，751「附加对手当前体力{0}%」
//     同款直算 kongyuan_skills.cpp:468-486）+ 固定伤害附加（990 同款 FIXED 档粉伤
//     g_break_round3.cpp:70「插件造固定伤害唯一正规入口」；SKILL_EFFECT 即发，
//     990/1170/K445 ③ 段同款家族一致性）。对手生存门。
//   · 1972 = 对方场下死亡数（遍历 seerRobot[opp].elfPets 跳过 on_stage 与在场位——
//     j_round_heal_drain.cpp:74-95 heal_off_field_party 同款队伍遍历先例）+ 固定
//     伤害附加（同 1653 的 990 通道）。对手生存门 + 计数 0 如实不附档。
//   · 1835 = 对手全属性+{0}（六槽逐项 stat_change：1378 六槽循环直系 burn_trio_
//     skills.cpp:869-887；目标置 opp。提升走 stat_change 真原语**不吃免弱**——增益
//     语义，common.h/burn_trio 535 注；与 938「使对手全属性-{0}」的 stat_drop
//     （各查免弱、各钳 -6，batch_a_skills.cpp:531-551）互为镜像、通道各一）。
//     对手生存门（动作打对手）+ 行参 0 关闸档。
//   · 2308 = 暴击门（crit_happened 查询：731「本回合打出致命一击则下{0}回合…必定
//     致命」同判据 batch_a_skills.cpp:557-570 头注「query_usage ①.5 在 ON_SKILL_HIT
//     掷、SKILL_EFFECT 时已可见；miss/打盔不进 SKILL_EFFECT → "打出"天然成立」；
//     掷点位 src/entities/skills.cpp:964）+ pp_zero_slot 随机挑槽（K445 ④ 段
//     k445_zero_random_pp_slots 逐字节同款 Fisher-Yates 本地实现，j105 1235/1441
//     先例）。与 1640/1349 排除判据不冲突：那是"SKILL_EFFECT 写 must_crit 赶不上
//     **本次**攻击的暴击判定"；本族**读**本次已掷结果（①.5 先于 SKILL_EFFECT），
//     731 判例直证同读法一等公民。对手生存门 + 未暴击门闭档。
//   组合复杂度：六族各自单宏两段（门+动作）以内，全既有原语/查询面直调——低于
//     1468（清/门/固伤/PP 四段）与 2492 单宏四段量级，无需新内核原语。
//
// ══ boss_invalid_effects.json 记档 ══
//   1835/2080/2308 在 resources/boss_invalid_effects.json（:884/:1055/:1222）——该面
//   是 effect_icon.tips 尾注（boss 无效）解析产物，**只被魂印程序注册口消费**
//   （soul_mark_manager.cpp:446-530 boss_invalid 节点过滤；include/plugin/
//   plugin_interface.h:50 SoulNodeRef 字段）。本单六族全为 moves SKILL_EFFECT 注册
//   （skills.cpp 注册口无此标签消费），不受影响——记档防后人误读。
//
// ══ 动工前置四查（2026-11-09 本 worktree）══
// ① 内核硬编码：(^|[^0-9.])(2080|1844|1653|1972|1835|2308)([^0-9.]|$) 对 src/
//   include/ grep rc=1 零命中（/tmp/k446/kernel_grep.out 空）。
// ② effect_info 六 id 在列且 args_num = side_effect.arg_count（2080/1844/1972/1835
//   =1、1653=2 直等；**2308 side_effect 表缺行** → 装载器按官方仓口径回退
//   effect_info.args_num=1，official_data_repository.cpp:170「表缺行时」头注，
//   三载体切片 [1]/[2]/[2] 与 4399 文逐条吻合）；custom_effect_programs / overrides /
//   custom_skill_combo_rules / custom_skill_selection_routes 六 id 全 0 命中。
// ③ 全库注册并集无注册：resources/ test/ examples/ `SKILL_EFFECT\((2080|1844|1653|
//   1972|1835|2308),` 零命中（grep rc=1）；跨 worktree（k445 / k447 / 主仓）同款
//   零命中（rc=1）——真缺口。
// ④ parser 覆盖面（冻结不扩，逐字核对 src/effects/effect_unit_parser.cpp）：
//   parse_effect_unit 主刀「令/使对手{状态」要求 { 紧随——1835「令对手全属性+{0}」/
//   2308「令对手随机{0}个」{ 前有字 → 不命中（:75-76）；第二刀五类条件模板
//   「若属性相同/不处于异常状态」等六族全无（:147-176）；fallback 3「附加+固定伤害」
//   只在主刀命中后的「未触发则」子句里递归（:210-218），六族主句无该形 → 六 id 均
//   -1 纯惰性；注册落地后 clone_effect 命中插件函数（src/entities/skills.cpp:675
//   只在 effect.logic 为空时走 parser）。
// 审计键段（10^9 + id×10^4 + owner×16 + slot；六基段 grep 全库零占用——
//   /tmp/k446/base_grep*.out 空；tools/check_storage_keys.py --quiet gate 内复扫）：
//   2080（1020800000）/ 1844（1018440000）/ 1653（1016530000）/ 1972（1019720000）/
//   1835（1018350000）/ 2308（1023080000）。
//
// ══ 载体切片（side_effect.arg_count = effect_info.args_num 平面切片，脚本
//    /tmp/k446/slice2.py 核算，2026-11-09；全载体 leftover=0；链上未注册 id 静默
//    跳过——宏按 id 注册非按载体，混链载体断言面由纯链载体覆盖，K438 1624 /
//    K440 734 / K442 1773 / K444 同口径记档）══
//   2080 [pct]（args_num=1）：36878 大地之疵 [2079,1110,2080]|[_,100] /
//     37152 琉光莹莹 [868,2097,2080]|[3,2,50] / 37258 厉武圣猎斩 [175,2080,757]|
//     [0,100,1,50,16,100,6] / 37721 超充能炮 [942,1361,2080]|[3,2,100]——
//     {0}∈{50,100}；**4/4 链全纯**。
//   1844 [pct]（args_num=1）：36059 霞宇天灵阵 [1002,990,436,1844]|[1,300,40,40] /
//     36718 华夫美食会 [1799,1818,1844]|[50,8,1,2,25]（1799/1818 未注册静默）/
//     38158 开道利斧 [766,511,1844]|[2,300,30,2,30,30] / 38436 冰结飞羽击
//     [931,593,1171,1844]|[2,0,0,45,1,300,30]——{0}∈{25,30,40}。
//   1653 [step_pct,fixed]（args_num=2）：34796 异域大雪崩 [1484,1652,1653]|
//     [_,10,10,10,50]（1484 未注册静默——1484 系条件先制族禁区，链上惰性不碍）/
//     35554 清风笑红颜 [1234,1653,1749]|[1,10,40,150,200]（1749 未注册静默）/
//     37917 瞬影天雷斩 [494,1653]|[10,30]——{0}全 10、{1}∈{30,40,50}。
//   1972 [fixed]（args_num=1）：29334 世末歌者 [2247,1305,1972]|[100,100,100]
//     （2247 未注册静默）/ 36553 缘尽·鬼神散 [699,1568,1934,1972]|[20,100]
//     （699 内核 case 通道/1568 未注册静默；1934=己方场下死亡增伤同链异族）/
//     36690 激流葬命斩 [1159,1857,1972,2017]|[1,25,2,100,2?——1857 ac=2 [25,2]、
//     2017 ac=0] / 37425 芸生遍刻 [1972]|[200] **单效果纯链**——{0}∈{100,200}。
//   1835 [delta]（args_num=1）：28025 殉血铸身 [1747,1191,1835,54]|[1,1,1,3,2] /
//     36543 记往岁 [1768,1835]|[1,300,1] / 36583 分你一些 [1835,1938,1939]|
//     [1,200,200] / 38154 派派大扫除 [2052,1938,1939,1835]|[2,300,300,1]——
//     {0} 全 1；36583/38154 链上 1938/1939（附加护盾/护罩）未注册静默。
//   2308 [slots]（args_num=1，side_effect 缺行回退）：37966 撼·万阵来迎
//     [797,966,2308,1450]|[2,100,22,1,1,300,2] / 38067 平庸或毁灭 [2216,2308,1356,
//     1355,553,2334]|[100,2,3,1,100,22,1]（2216/2334 未注册静默——2334 系暴击门
//     免疫族留池）/ 38108 炽热爆炎 [2308,1840]|[2,50]——{0}∈{1,2}。
// 4399 文（reference/scrape_4399/out/pets_full.jsonl，主仓路径；按技能名取 effect
//   全文逐字核对，2026-11-09 现查，脚本 /tmp/k446/x4399b.py）：
//   · 2080 3/3 收录全逐字：「自身处于护盾状态时威力提升50/50/100%」（厉武圣猎斩/
//     琉光莹莹/超充能炮）；大地之疵未收录——孤行按切片落地（K441 25726 / K442
//     38358 / K444 同判据）。
//   · 1844 4/4 收录全逐字：「附加对手已损失体力值25/30/30/40%的百分比伤害」（华夫
//     美食会/开道利斧/冰结飞羽击/霞宇天灵阵）；霞宇天灵阵同段并现 436「附加已损失
//     体力值40%的**固定**伤害」——436=自身已损失·固定 vs 1844=对手已损失·百分比，
//     与两 effect_info 模板逐字对应，PERCENT_VALUE 档选型直证。
//   · 1653 3/3 逐字：「释放技能时对手每残留10%的体力值则此技能附加50/40/30点固定
//     伤害」（异域大雪崩/清风笑红颜/瞬影天雷斩）。
//   · 1972 4/4 收录全逐字：「对方场下每死亡一只精灵则附加给对手100/100/100/200点
//     固定伤害」（世末歌者/激流葬命斩/缘尽·鬼神散/芸生遍刻；激流葬命斩 1857 ac=2
//     [25,2] 吃「威力提升25%…小于2点…翻倍」两参数、1972 恰 [100]——切片回改
//     slice2.py 直证）。
//   · 1835 4/4 收录全逐字：「100%使/令对手全属性+1」（殉血铸身「100%使」、分你一些/
//     记往岁「令」、派派大扫除「100%令」——对手增益系 troll/支援技语义，4399 与
//     effect_info 模板同文互证）。
//   · 2308 2/2 收录全逐字：「打出致命一击时令对手随机2个技能PP值归0」（平庸或毁灭/
//     炽热爆炎）；撼·万阵来迎未收录——孤行按切片 [1] 落地（同上判据）。
//
// ══ 实现形态（同形套路照抄先例，通道零新增）══
//   · 时点 = SKILL_EFFECT（effect_register_state default；miss/被盔桶不跑——六模板
//     均为技能上效字面，K440-K445 同口径）。行参布局 [owner, opp, 行参…]（loader
//     注入 owner/target 前缀 + 行参原序，K443/K445 同款）。
//   · 审计槽布局（K437-K445 同构）：
//     2080：slot 0 行参 pct / 1 门判定（1=盾态 0=无盾）/ 2 实测盾值 / 3 写前威力
//       视图 / 4 写后威力视图 / 8 累计使用计数 / 9 跳过原因（0=无/1=行参 pct<=0
//       关闸/2=威力视图未物化（value<=0，53/1377 同守卫））
//     1844：slot 0 行参 pct / 1 对手已损失 / 2 伤害实参 / 3 对手实扣（deal 前后
//       hp 差，效果体内自测）/ 8 累计使用计数 / 9 跳过原因（0=无/1=对手已倒/
//       2=缺原语槽/3=对手满血 lost<=0 如实零附档/4=行参 pct<=0 关闸）
//     1653：slot 0 行参 step_pct / 1 行参 fixed / 2 残留步数 / 3 伤害实参 / 4
//       对手实扣 / 8 累计使用计数 / 9 跳过原因（0=无/1=对手已倒/2=缺原语槽/
//       3=步数 0 如实档/4=行参非正关闸）
//     1972：slot 0 行参 fixed / 1 对方场下死亡数 / 2 伤害实参 / 3 对手实扣 / 8
//       累计使用计数 / 9 跳过原因（0=无/1=对手已倒/2=缺原语槽/3=死亡数 0 如实档/
//       4=行参 fixed<=0 关闸）
//     1835：slot 0 行参 delta / 1 六槽调用数 / 2 六槽实变数（写后 ability_levels
//       逐槽读数与写前差非零计数）/ 8 累计使用计数 / 9 跳过原因（0=无/1=对手已倒/
//       2=缺原语槽/3=行参 delta=0 关闸）
//     2308：slot 0 行参 slots / 1 暴击实测（crit_happened）/ 2 实归零槽数 /
//       8 累计使用计数 / 9 跳过原因（0=无/1=未暴击门闭/2=对手已倒/3=缺原语槽/
//       4=行参 slots<=0 关闸）
// 场景回归：test/scenario/scenario_8340_k446_pool.cpp（号段 8340-8349，确定性多腿：
//   2080 门开/门闭/双变体；1844 半血门开/满血零附/双变体；1653 满血 10 步/半血
//   5 步/变体载体；1972 五死门开/全活门闭/变体载体；1835 双载体 +1 精确；2308
//   暴击门开/未暴门闭/变体载体；量值全走审计槽精确断言，概率仅 2308 挑槽——
//   断言计数不挑槽身份，sbs_srand 播种）。
//
// ══ 变异测试（12 变异 = 每族 2，2026-11-09 本 worktree 全部真转红：改→rm .o 强制
//    重编+显式 moves_plugin_deploy→单跑 8340 必红（红点全部落在预期断言腿）→
//    字节级还原（cmp + md5 双验 0d24c87d70db7e9329b86b3c8a1c0bbd）→rm .o 重编+
//    deploy→回绿 SBS_SCENARIO_CHECKS=82；脚本 /tmp/k446/{mutate,restore}.sh）══
//   M1 2080 门恒开（has_shield = true）→ T2 门判定红（1≠0）——盾态门方向锁定；
//   M2 2080 pct 折半（pct → pct/2 入乘区）→ T1 写后视图红（240≠320）——{0}% 逐字
//     面消费锁定（防后人"顺手"改口径，坑集 §7 同族）；
//   M3 1844 已损失基数改最大体力（max-hp → max）→ T5 已损失审计红（600000≠300000）
//     ——「已损失」= max_hp - hp 基数锁定；
//   M4 1844 零附门松绑（lost<=0 → lost<0）→ T6 满血档红（9: 0≠3）——满血零附如实档；
//   M5 1653 步数除子 +1（×{0} → ×({0}+1)）→ T9 步数红（9≠10）——「每残留{0}%」
//     floor 除子锁定；
//   M6 1653 固定伤折半 → T9 实参红（150≠300）——{1} 逐字面消费锁定；
//   M7 1972 死亡判据收严（hp<=0 → hp<0）→ T13 死亡数红（0≠5）——hp==0 计死边界；
//   M8 1972 固定伤折半 → T13 实参红（500≠1000）；
//   M9 1835 目标反转（opp → owner）→ T16 对手槽位红（0≠1）——「令对手」方向锁定；
//   M10 1835 delta 翻倍（delta → delta×2）→ T16 槽位红（2≠1）——行参逐字面消费；
//   M11 2308 门恒开（crit = 1）→ T19 未暴腿红（1≠0）——crit_happened 门锁定；
//   M12 2308 挑槽数 +1（slots → slots+1）→ T18 实归零红（3≠2）——{0} 槽数消费。

#include "common.h"

#include <numerical-calculation/calculation.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int k2080AuditBase = 1020800000;   // 10^9 + 2080×10^4
constexpr int k1844AuditBase = 1018440000;
constexpr int k1653AuditBase = 1016530000;
constexpr int k1972AuditBase = 1019720000;
constexpr int k1835AuditBase = 1018350000;
constexpr int k2308AuditBase = 1023080000;

inline int audit_key_of(int base, int owner, int slot) { return base + owner * 16 + slot; }

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

// 随机挑 want 个不同技能槽 PP 归零（K445 ④ 段逐字节同款本地实现；j105 1235/1441
// 先例）。返回实际归零槽数（pp_zero_slot SUCCESS 计数；无限槽 INVALID_PARAM 不计）。
int k446_zero_random_pp_slots(BattleContext* ctx, int target, int want, int actor) {
    if (!ctx || !g_core || !g_core->pp_zero_slot || want <= 0) {
        return 0;
    }
    constexpr int kN = 5;   // 技能槽固定 5（elf-pet.h std::array<Skills, 5>）
    int slots[kN] = {0, 1, 2, 3, 4};
    for (int i = kN - 1; i > 0; --i) {
        std::swap(slots[i], slots[ctx->rng().next() % (i + 1)]);
    }
    int zeroed = 0;
    for (int k = 0; k < want && k < kN; ++k) {
        if (g_core->pp_zero_slot(ctx, target, slots[k], actor) == PpReduceResult::SUCCESS) {
            ++zeroed;
        }
    }
    return zeroed;
}

// 对方场下死亡数（hp<=0 且非在场位；j_round_heal_drain.cpp:74-95 队伍遍历同款）。
int k446_opp_dead_reserve_count(BattleContext* ctx, int opp) {
    const auto& pets = ctx->seerRobot[opp].elfPets;
    const int n = static_cast<int>(pets.size()) < 6 ? static_cast<int>(pets.size()) : 6;
    int dead = 0;
    for (int i = 0; i < n; ++i) {
        if (i == ctx->on_stage[opp]) {
            continue;   // 在场位不算"场下"
        }
        if (pets[i].hp <= 0) {
            ++dead;
        }
    }
    return dead;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 2080 —— 自身处于护盾状态时威力提升{0}%
// 行参布局 [owner, opp, pct]（args_num=1）。门 = shield_bank_ 非空（K445 1468 同
// 字段查询）；动作 = SKILL_EFFECT 时点写 ws.skill_power_view（53/1377 同款：读旧值
// >0 守卫 + checked_pct_mul ×(100+pct)/100 整体赋值自动置 rewritten——battleWorkspace.h
// SkillView 头注「效果侧写入=声明变威力」）。动作打自身 → 不设对手生存门。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_2080_shield_power_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2080, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2080, args,
                     "2080 布局=[owner,opp,pct]");
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 2080, args, "需要 owner ∈ {0,1}");
    set_storage_int_local(ctx, audit_key_of(k2080AuditBase, owner, 8),
                          storage_int_local(ctx, audit_key_of(k2080AuditBase, owner, 8), 0) + 1);
    set_storage_int_local(ctx, audit_key_of(k2080AuditBase, owner, 0), pct);
    // 行参在位即消费：pct<=0 = 关闸档（全库 {0}∈{50,100}，防御档）。
    if (pct <= 0) {
        set_storage_int_local(ctx, audit_key_of(k2080AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    // 盾态门：护盾槽非空（hood 是独立实体不计——"护盾状态"字面，1215 判据同域）。
    ElfPet& me = ctx->getPet(owner);
    const int shield_pts = me.shield_bank_.total();
    const bool has_shield = !me.shield_bank_.empty();
    set_storage_int_local(ctx, audit_key_of(k2080AuditBase, owner, 1), has_shield ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k2080AuditBase, owner, 2), shield_pts);
    if (!has_shield) {
        set_storage_int_local(ctx, audit_key_of(k2080AuditBase, owner, 4), 0);   // 未写视图
        set_storage_int_local(ctx, audit_key_of(k2080AuditBase, owner, 9), 0);   // 门闭如实档
        return EffectResult::kOk;
    }
    // 变威力：ON_SKILL_HIT 已物化面板值（battleFsm.cpp:513 materialize），SKILL_EFFECT
    // 时点在其上 ×(100+pct)/100（53/1377 同守卫同通道；视图未物化 value<=0 → 如实档）。
    auto& view = ctx->ws.skill_power_view[owner];
    set_storage_int_local(ctx, audit_key_of(k2080AuditBase, owner, 3), view.value);
    if (view.value <= 0) {
        set_storage_int_local(ctx, audit_key_of(k2080AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    const int after = checked_pct_mul(view.value, pct, "2080 威力提升乘区");
    view = after;   // operator= 自动置 rewritten（变威力声明）
    set_storage_int_local(ctx, audit_key_of(k2080AuditBase, owner, 4), view.value);
    set_storage_int_local(ctx, audit_key_of(k2080AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

SKILL_EFFECT(2080, effect_2080_shield_power_amp)

// ═══════════════════════════════════════════════════════════════════════════
// 1844 —— 附加对手已损失体力值{0}%的百分比伤害
// 行参布局 [owner, opp, pct]（args_num=1）。514（kongyuan_skills.cpp:442-461）同形
// 直系：lost = max_hp - hp，amount = lost×pct/100（checked_pct_mul 折算：
// lost×pct/100 ≡ checked_pct_mul(lost, pct-100)），PERCENT_VALUE 档（值已算出、
// 走百分比抗性/护罩——battle_primitives.h:327 头注点名本族）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1844_pink_opp_lost_pct(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1844, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1844, args,
                     "1844 布局=[owner,opp,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1844,
                     args, "需要 owner/opp ∈ {0,1}");
    set_storage_int_local(ctx, audit_key_of(k1844AuditBase, owner, 8),
                          storage_int_local(ctx, audit_key_of(k1844AuditBase, owner, 8), 0) + 1);
    set_storage_int_local(ctx, audit_key_of(k1844AuditBase, owner, 0), pct);
    // 生存门：对手已倒 → 整条跳过（K422/K440/K441/K443/K445 口径）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key_of(k1844AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->deal_pink_damage) {
        set_storage_int_local(ctx, audit_key_of(k1844AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    const int lost = max_hp_of(ctx, opp) - ctx->getPet(opp).hp;
    set_storage_int_local(ctx, audit_key_of(k1844AuditBase, owner, 1), lost);
    if (lost <= 0) {
        // 满血 → 已损失 0 → 如实零附档（514 同守卫；记档不造兜底）。
        set_storage_int_local(ctx, audit_key_of(k1844AuditBase, owner, 2), 0);
        set_storage_int_local(ctx, audit_key_of(k1844AuditBase, owner, 3), 0);
        set_storage_int_local(ctx, audit_key_of(k1844AuditBase, owner, 9), 3);
        return EffectResult::kOk;
    }
    if (pct <= 0) {
        set_storage_int_local(ctx, audit_key_of(k1844AuditBase, owner, 9), 4);
        return EffectResult::kOk;
    }
    // ×{0}% 乘区：checked_pct_mul(value, d) = value×(100+d)/100 → {0}% 用 {0}-100。
    const int amount = checked_pct_mul(lost, pct - 100, "1844 已损失体力百分比乘区");
    set_storage_int_local(ctx, audit_key_of(k1844AuditBase, owner, 2), amount);
    const int hp_before = ctx->getPet(opp).hp;
    // PERCENT_VALUE 档粉伤：吃护罩/百分比抗性（红伤相只在技能结算里跑，本附加不进）。
    (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE, owner);
    set_storage_int_local(ctx, audit_key_of(k1844AuditBase, owner, 3),
                          hp_before - ctx->getPet(opp).hp);
    set_storage_int_local(ctx, audit_key_of(k1844AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

SKILL_EFFECT(1844, effect_1844_pink_opp_lost_pct)

// ═══════════════════════════════════════════════════════════════════════════
// 1653 —— 释放技能时对手每残留{0}%的体力值则此技能附加{1}点固定伤害
// 行参布局 [owner, opp, step_pct, fixed]（args_num=2）。步数 = floor(hp×100/max
// ÷{0})（整数直算，751 同款直算先例 kongyuan_skills.cpp:468-486）；附加 = 990 同款
// FIXED 档粉伤（SKILL_EFFECT 即发，K445 ③ 段家族一致性）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1653_residual_steps_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1653, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1653, args,
                     "1653 布局=[owner,opp,step_pct,fixed]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int step_pct = args.int_args[2];
    const int fixed = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1653,
                     args, "需要 owner/opp ∈ {0,1}");
    set_storage_int_local(ctx, audit_key_of(k1653AuditBase, owner, 8),
                          storage_int_local(ctx, audit_key_of(k1653AuditBase, owner, 8), 0) + 1);
    set_storage_int_local(ctx, audit_key_of(k1653AuditBase, owner, 0), step_pct);
    set_storage_int_local(ctx, audit_key_of(k1653AuditBase, owner, 1), fixed);
    // 生存门：对手已倒 → 整条跳过。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key_of(k1653AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->deal_pink_damage) {
        set_storage_int_local(ctx, audit_key_of(k1653AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    // 行参在位即消费：任一非正 = 关闸档（全库 {0}=10、{1}∈{30,40,50}，防御档）。
    if (step_pct <= 0 || fixed <= 0) {
        set_storage_int_local(ctx, audit_key_of(k1653AuditBase, owner, 9), 4);
        return EffectResult::kOk;
    }
    // 残留步数：每满 {0}% 记一步（floor；hp×100 与 max×{0} 均 int64 内积防溢出——
    // 场景堆超大 HP 时中间积响亮而非回绕；步数上限 100/{0} 天然有界）。
    const int max_hp = max_hp_of(ctx, opp);
    const int hp_now = ctx->getPet(opp).hp;
    const int steps = static_cast<int>(
        (static_cast<int64_t>(hp_now) * 100) / (static_cast<int64_t>(max_hp) * step_pct));
    set_storage_int_local(ctx, audit_key_of(k1653AuditBase, owner, 2), steps);
    const int amount = steps * fixed;   // 步数 ≤100、fixed ≤200 → 乘积有界，直乘安全
    set_storage_int_local(ctx, audit_key_of(k1653AuditBase, owner, 3), amount);
    if (steps <= 0) {
        set_storage_int_local(ctx, audit_key_of(k1653AuditBase, owner, 9), 3);
        return EffectResult::kOk;
    }
    const int hp_before = ctx->getPet(opp).hp;
    (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::FIXED, owner);
    set_storage_int_local(ctx, audit_key_of(k1653AuditBase, owner, 4),
                          hp_before - ctx->getPet(opp).hp);
    set_storage_int_local(ctx, audit_key_of(k1653AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

SKILL_EFFECT(1653, effect_1653_residual_steps_fixed)

// ═══════════════════════════════════════════════════════════════════════════
// 1972 —— 对方场下每死亡一只精灵则附加给对手{0}点固定伤害
// 行参布局 [owner, opp, fixed]（args_num=1）。计数 = 对方队伍 hp<=0 且非在场位
// （j_round_heal_drain 队伍遍历同款）；附加 = 990 同款 FIXED 档粉伤。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1972_dead_reserve_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1972, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1972, args,
                     "1972 布局=[owner,opp,fixed]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int fixed = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1972,
                     args, "需要 owner/opp ∈ {0,1}");
    set_storage_int_local(ctx, audit_key_of(k1972AuditBase, owner, 8),
                          storage_int_local(ctx, audit_key_of(k1972AuditBase, owner, 8), 0) + 1);
    set_storage_int_local(ctx, audit_key_of(k1972AuditBase, owner, 0), fixed);
    // 生存门：对手（在场）已倒 → 整条跳过。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key_of(k1972AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->deal_pink_damage) {
        set_storage_int_local(ctx, audit_key_of(k1972AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    const int dead = k446_opp_dead_reserve_count(ctx, opp);
    set_storage_int_local(ctx, audit_key_of(k1972AuditBase, owner, 1), dead);
    if (dead <= 0) {
        // 每死亡一只才附加 → 0 死如实零附档（记档不造兜底）。
        set_storage_int_local(ctx, audit_key_of(k1972AuditBase, owner, 2), 0);
        set_storage_int_local(ctx, audit_key_of(k1972AuditBase, owner, 3), 0);
        set_storage_int_local(ctx, audit_key_of(k1972AuditBase, owner, 9), 3);
        return EffectResult::kOk;
    }
    if (fixed <= 0) {
        set_storage_int_local(ctx, audit_key_of(k1972AuditBase, owner, 9), 4);
        return EffectResult::kOk;
    }
    const int amount = dead * fixed;   // dead ≤5、fixed ≤200 → 有界直乘
    set_storage_int_local(ctx, audit_key_of(k1972AuditBase, owner, 2), amount);
    const int hp_before = ctx->getPet(opp).hp;
    (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::FIXED, owner);
    set_storage_int_local(ctx, audit_key_of(k1972AuditBase, owner, 3),
                          hp_before - ctx->getPet(opp).hp);
    set_storage_int_local(ctx, audit_key_of(k1972AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

SKILL_EFFECT(1972, effect_1972_dead_reserve_fixed)

// ═══════════════════════════════════════════════════════════════════════════
// 1835 —— 100%令对手全属性+{0}
// 行参布局 [owner, opp, delta]（args_num=1）。六槽逐项 stat_change（1378 六槽循环
// 直系 burn_trio_skills.cpp:869-887；目标置 opp）。增益语义不吃免弱（stat_change
// 真原语；与 938 stat_drop「各查免弱」互为镜像）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1835_opp_all_stats_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1835, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1835, args,
                     "1835 布局=[owner,opp,delta]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int delta = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1835,
                     args, "需要 owner/opp ∈ {0,1}");
    set_storage_int_local(ctx, audit_key_of(k1835AuditBase, owner, 8),
                          storage_int_local(ctx, audit_key_of(k1835AuditBase, owner, 8), 0) + 1);
    set_storage_int_local(ctx, audit_key_of(k1835AuditBase, owner, 0), delta);
    // 生存门：对手已倒 → 整条跳过（动作打对手）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key_of(k1835AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->stat_change) {
        set_storage_int_local(ctx, audit_key_of(k1835AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    if (delta == 0) {
        set_storage_int_local(ctx, audit_key_of(k1835AuditBase, owner, 9), 3);
        return EffectResult::kOk;
    }
    int called = 0;
    int changed = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int before = ctx->ability_levels[opp][i];
        (void)g_core->stat_change(ctx, opp, i, delta);
        ++called;
        const int after = ctx->ability_levels[opp][i];
        if (after != before) {
            ++changed;   // 原语到顶钳住（±6）不算实变——坑集 §1② 快照逐槽口径
        }
    }
    set_storage_int_local(ctx, audit_key_of(k1835AuditBase, owner, 1), called);
    set_storage_int_local(ctx, audit_key_of(k1835AuditBase, owner, 2), changed);
    set_storage_int_local(ctx, audit_key_of(k1835AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

SKILL_EFFECT(1835, effect_1835_opp_all_stats_up)

// ═══════════════════════════════════════════════════════════════════════════
// 2308 —— 打出致命一击时令对手随机{0}个技能PP值归0
// 行参布局 [owner, opp, slots]（args_num=1，side_effect 表缺行回退 args_num）。
// 门 = crit_happened[owner]（731 同判据：query_usage ①.5 在 ON_SKILL_HIT 掷、
// SKILL_EFFECT 时已可见——skills.cpp:964）；动作 = K445 同款随机挑槽 pp_zero_slot。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_2308_crit_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2308, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2308, args,
                     "2308 布局=[owner,opp,slots]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int slots = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 2308,
                     args, "需要 owner/opp ∈ {0,1}");
    set_storage_int_local(ctx, audit_key_of(k2308AuditBase, owner, 8),
                          storage_int_local(ctx, audit_key_of(k2308AuditBase, owner, 8), 0) + 1);
    set_storage_int_local(ctx, audit_key_of(k2308AuditBase, owner, 0), slots);
    // 门：本次攻击已暴击（miss/打盔不进 SKILL_EFFECT → "打出"天然成立，731 头注）。
    const int crit = ctx->crit_happened[owner] ? 1 : 0;
    set_storage_int_local(ctx, audit_key_of(k2308AuditBase, owner, 1), crit);
    if (!crit) {
        set_storage_int_local(ctx, audit_key_of(k2308AuditBase, owner, 2), 0);
        set_storage_int_local(ctx, audit_key_of(k2308AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    // 生存门：对手已倒（链上先行效果击倒 → 不追发，K445 同口径）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key_of(k2308AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->pp_zero_slot) {
        set_storage_int_local(ctx, audit_key_of(k2308AuditBase, owner, 9), 3);
        return EffectResult::kOk;
    }
    if (slots <= 0) {
        set_storage_int_local(ctx, audit_key_of(k2308AuditBase, owner, 9), 4);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key_of(k2308AuditBase, owner, 2),
                          k446_zero_random_pp_slots(ctx, opp, slots, owner));
    set_storage_int_local(ctx, audit_key_of(k2308AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

SKILL_EFFECT(2308, effect_2308_crit_pp_zero)
