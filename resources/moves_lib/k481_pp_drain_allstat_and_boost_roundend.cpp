// K481 · PP 消耗降属 + 提升期回合末抽血两族 —— 2035×5 + 728×5（2026-11-18，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k481，分支 feat/k481，基点 4e026fb8
// （实含 K461-K480 主线实现：K474 官方渲染序/K470 heal-drain 双族/K458 PP 原语/
//  K473 PP 倍率槽/K461 stat_drop 全属性/K480 EVENT_HIT 时点先例齐备）；
// 场景 test/scenario/scenario_8680_k481.cpp，号段 8680-8689）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-18）══
//   2035（args_num=3）"下{0}次攻击命中时消耗所使用攻击技能的全部PP值，每消耗{1}点
//         令对手[color=#52a5f2]全属性[/color]-{2}"
//         （effect_des 无行；analyze 同文。蓝色高亮=全属性位，K474 1150 六槽 delta
//         行同款渲染面；本模板无六槽行参、六槽同值 -{2}，序不敏感——全属性六项
//         口径 = 攻/特攻/防/特防/速/**命中**，common.h:140 用户 2026-09-18 定版）
//   728（args_num=1）"[iconHit]自身处于能力提升状态时，[color=#57c975]回合结束时[/color]
//         直接减少对手1/{0}最大体力"（effect_des 无行；绿色高亮=回合末时点位）
//
// ══ 载体切片（unity 库 json_each 精确元素 + side_effect.arg_count 顺序切分，
//   10 载体链上伴生全在册（191/2189/440/87/1152/1821/777/979/180/763/1895/1226/
//   1234/702/843/1558 已注册实现）零剩余；场景 T8680-pre 以实际加载器
//   load_skill_effects 逐载体锁定）══
//   2035 [{0}=次数,{1}=每档点数,{2}=每档级数]（5 载体，{0}∈{1,2}、{1}∈{1,3}、
//     {2} 全=1——{2} 无真实差分载体，多值维度由 EffectParamOverride 变档腿承担，
//     K480 {0}=5/K458 35245 同款装配期官方覆盖基建；场景 T8680.d2 以学习者 4401
//     变档 {2}=2 锁行参消费面）：
//     28355 止戾·飒空龙心 [979(2),87(0),1558(2),702(1),2035(3)]
//       =[2,3|∅|2,100|2|**1,1,1**]（cat4 属性技载体——"接下来"从下次攻击起算），
//     28501 歼灭协议 [1389(3),1852(1),2035(3)]
//       =[3,100,0|1|**1,1,1**]（cat4；1852=非提升则自升——与 2035 无门不互斥），
//     28743 孽冤悲哭 [191(1),2035(3),2189(2),843(2)]
//       =[4|**2,1,1**|100,23|2,2]（唯一 {0}=2 载体），
//     29349 蓄势回击 [191(1),1226(3),2035(3)]
//       =[4|3,300,300|**1,3,1**]（唯一 {1}=3 载体——阶梯档差分主载体），
//     38114 飞絮空澜没 [1234(1),1895(3),2035(3),1821(1)]
//       =[1|100,100,15|**1,1,1**|2]（cat2 攻击技载体——**自身攻击不算"下{0}次"**
//         的结构甄别主载体，见下"时点甄别"；⚠️ 首轮切片核验曾把 2035 误配 [1,1,2]/
//         1821 误配 []，T8680-pre 加载器实测纠正为 [1,1,1]/[2]——顺序切分以
//         side_effect.arg_count 游标为准，与 K473 28203 切面甄别记档同源）。
//   728 [{0}=分母]（5 载体，{0}∈{3,4,5}）：
//     17104 黑夜灭魂决 [728(1),440(2)]=[**5**|3,2]（cat2 特160；440=对手 PP 消耗×2 窗），
//     17291 英魂圣裁决 [728(1),763(6)]=[**4**|1,0,0,0,1,0]（cat1 物160；763=**非提升
//       时自升**六槽行——门时点甄别关键伴生，见下），
//     19214 圣杖叱喝斩 [180(0),728(1)]=[∅|**4**]（cat1；180=消对手回合类效果零参），
//     31324 狂刃裂甲 [777(1),728(1)]=[1|**4**]（cat1 物85 pp20），
//     31464 夺魄魅魂击 [1152(1),728(1)]=[50|**3**]（cat1 物150；1152=**非提升则增伤
//       50%**——第二条互补腿）。
//
// ══ 2035 读面甄别（谁消耗谁的 PP）═══════════════════════════════════════
//   "下{0}次攻击命中时消耗所使用攻击技能的全部PP值" = **自身**的攻击命中时消耗
//   **自己所使用**攻击技能的全部 PP。证据三线：
//   ① 载体文：4399 六处载体描述"接下来的攻击将倾注全力/下次攻击将会倾注全力"
//     （moves.info，28355/28501/28743/29349 四条逐字）——主语省略=施放方自身；
//     "倾注全力"= 把该击的 PP 全押上（消耗面在自身、收益=对手降属）；
//   ② 句式对照：对手侧消耗族官方文带显式"对手"主语（2189"自身的PP值总和高于对手
//     时{0}%令对手{1}"、440"**对手**使用技能消耗的PP值变为{1}倍"）——本模板无
//     "对手"主语修饰"攻击"，消耗与降属的受事各归其位（消耗=自身技能、降属=对手）；
//   ③ 伴生结构：三 cat4 属性技载体自身无攻击，"下{0}次攻击"只能是施放方后续攻击
//     （若读对手攻击，属性技载体挂"对手攻击命中"票与"接下来的攻击将倾注全力"文
//     两头对不上）。
//
// ══ 时点甄别（板行指令：命中时点沿 K480 AFTER_ACTION/时点甄别先例）═════════
//   · "攻击命中"检测通道 = **EVENT_HIT**（skills.cpp:1293/1302，HIT 分支终点 emit；
//     miss/SKILL_INVALID 不发——"命中"字面结构性成立；HIT_INVALID 算命中照发）。
//     ⚠️ 属性技能命中也发 EVENT_HIT（emit 注释"属性技能也算命中"）→ 回调须以
//     **executed_skill_is_attack**（common.h 执行期判据，读 ws.skill_type_view——
//     ON_SKILL_HIT 后回调语境合法）过滤出**攻击技**命中；"攻击命中时"字面即此。
//   · **武装攻击自身不在"下{0}次"内**（"下"=未来，K480 711 NextRounds"本回合不算"
//     同族语义）：引擎面结构性保证——EVENT_HIT 在 Skills::execute 内 emit 入队，
//     投递点 = **State 桶执行完后统一 drain**（battleFsm.cpp:1322 注释）；武装发生在
//     SKILL_EFFECT 桶（晚于 ON_SKILL_HIT 桶）→ 武装当次攻击的 EVENT_HIT 已在
//     ON_SKILL_HIT 桶末 drain 掉，晚注册的 watcher 结构性收不到（无需 seq 守卫；
//     K474 DETECT 票 seq+round 双守卫是"只随本次用技"同向语义，此处反向"严格未来"
//     由投递序天然满足）。攻击技载体 38114 的首击不消费、第二击起逐击消费
//     （T8680.h 锁死）。
//   · 同回合额外行动攻击算"下{0}次"（未来攻击即未来，无回合界字面——票为次数型
//     非窗口型，套路目录 §四）。
//   · PP 消费时点语义：watcher 回调跑在 ON_SKILL_HIT 桶末 drain（早于该击的
//     SKILL_EFFECT/ATTACK_DAMAGE 桶，更早于 AFTER_ACTION 的
//     consume_selected_skill_pp（battleFsm.cpp:1981/2123））→ 读到的 skills[slot].pp
//     是**当次攻击正常 1 点消耗前**的剩余全量——"消耗全部PP值"按消耗读面
//     （当次技能剩余 PP 全量，板行指令；K458 pp_zero_slot 逐槽先例）在此全额清零，
//     AFTER_ACTION 的常规消耗对已 0 槽幂等（consume_selected_skill_pp pp<=0 短路，
//     battleFsm.cpp:459）。
//   · 消耗槽位 = roundChoice[self][1]（**玩家点的槽位**口径——替换技"PP 扣除走
//     原槽位"，battleFsm.cpp:481 注释直系；resolve_selected_skill_index 同源逻辑
//     插件侧以 roundChoice 直读复刻）。
//   · 728 时点：门在 **SKILL_EFFECT（施放/命中时点）在位判读**，扣血在 ROUND_END
//     单发 tick。判据（互补腿三查）：
//     ① 17291 英魂圣裁决 [728,763]——763"自身**不**处于能力提升状态时则自升
//       （攻+1 速+1）"：门若改在 ROUND_END 判，763 自升腿会先把自身送进提升态、
//       728 回合末门必开 → 两腿同发，互补闭环破——门只能与 763 同快照（施放时点）；
//     ② 31464 夺魄魅魂击 [728,1152]——1152"非提升则增伤 50%"（伤害期读）与 728
//       同理互斥（K470 634 互补腿"两腿共享同一'当前'快照"裁定直系）；
//     ③ 模板语序"[条件]时，[回合结束时][动作]"= K470 674"攻击结束时恢复"同款
//       条件前置句式（674 门在 AFTER_ACTION 判"未击败"——条件归属动作时点；
//       本族条件面是**持续态在位判读**（K289 659"任一 ability_levels>0"直系），
//       在位时点=施放时点，回退负测（门闭后获提升不补触发）T8680.j 锁死）。
//     ⚠️ 官方无门时点实测，按互补腿结构建模记档。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例）════════════════════
//   · 2035 票 = **plugin_storage counts 凭证**（K461 1645"下{1}次"票直系：授予后
//     无回合界、ON_STAGE 换宠 epoch 作废（watcher scope=ON_STAGE+valid_id 实参
//     K459/K470 口径）、重施放刷新不叠加——余票重置 {0}、旧 watcher 拆除防双响应
//     （K480 711 重施放拆旧同款）、耗尽即 remove_watcher（不留滞留 watcher——
//     once=false 多击消费，耗尽自拆是计数票的结构收口））。
//   · PP 消费 = **g_core->pp_zero_slot 原语**（battle_primitives.h:770：pp==-1
//     无限槽不参与、pp<=0 幂等、实际归零 emit EVENT_PP_REDUCED——清 PP 必走原语，
//     直写 skills[i].pp 绕事件=私造通道禁；actor 传**自身**：消费是施放方自己的
//     成本，pp_retain（琼华之庇）只保对手来源（actor==1-target），自身成本不可被
//     自己的庇保留——原语语义自裁）。消耗量 = 原语调用前后差（K458 选槽助手同读法）。
//   · 阶梯降属 = steps = 消耗量 / {1}（整数除，"每消耗{1}点"字面；余数不足一档
//     不降），每槽降 steps×{2}：**stat_drop 六槽逐槽**（0..5 全属性含命中；
//     弱化原语查免弱 STAT_DROP/ATTACH_BAN、-6 钳制，K461 1638"全属性"写面直系；
//     actor=属主 self——"调用方必须如实声明：效果属主施加就传属主"
//     battle_primitives.h:505）。逐槽结果 SUCCESS/IMMUNE/AT_FLOOR 记审计掩码
//     （K461 同款）。steps×{2}<=0（{2}=0/消耗=0）自然 no-op 不调原语。
//   · 降属**延后到本行动 AFTER_ACTION 桥**（K470 674 桥式直系）：命中当击
//     （ON_SKILL_HIT 桶末 drain）只做 PP 消费 + pending 入账 + 桥武装；桥 tick 在
//     本行动 AFTER_ACTION 排空 pending → 六槽 stat_drop。甄别理由（重要记档，
//     全量门 8410 实证）：命中时点降属早于**本次攻击自身的 SKILL_EFFECT 链**——
//     K454 场景 8410 T8419g 载体 28501（本单 2035 真实载体）+ 次击 38250（1974
//     消强）：命中时点把对手 +2 等级先压穿（-6 钳）→ 同击的消强找不到正等级 →
//     消强失败链断（回归 1/1013 红）。降属是"消耗的后果"，语义归属攻击**结算
//     之后**——延后即解，且不改变终态断言面（同回合内落地）。同回合多击（额外
//     行动）= pending 累计 + 同键桥覆盖重武装（timed_bucket (source<<32)|effect
//     去重键），每次 AFTER_ACTION 排空一次；桥 InRounds rounds=1 当回合过期，
//     无跨回合残留。生存门在**桥时点**判（彼时伤害已落地）：目标已倒 → 排空
//     不降（击杀回合降属不施加，K430/K470 口径；官方无击杀回合实测按"后果不
//     施加于已倒者"建模记档，T8680.g 锁死）。官方无降属时点实测，按交互证据
//     建模记档。
//   · 728 扣血 = **deal_pink_damage(ctx, opp, max_hp_of(opp)/{0}, FIXED, actor=self)**
//     正式管线（插件侧粉伤唯一入口——固定档吃免粉/护罩/固定抗性、emit
//     EVENT_TAKE_DAMAGE；**不直写 hp**，837 点数吸取直写盲区对照，板行指令）。
//     量 = 对手最大体力 / {0} 整数除（"1/{0}最大体力"字面；max_hp_of
//     common.h:311= numericalBase[HP]，引擎 PERCENT 换算同源）。"直接减少"= 直接
//     扣血语义（非红伤公式），载体切片 {0}∈{3,4,5} 无百分比档声明 → 固定档落地
//     （DamageKind::PERCENT 是"amount=占最大体力百分比"另档，两档抗性面不同，
//     不猜——官方无实测，固定档为"固伤面"默认口径记档）。
//   · 728 tick = register_round_effect @ BATTLE_ROUND_END、InRounds rounds=1
//     （"回合结束时"=施放回合的回合末——ROUND_END 时点在施放之后、同回合必经；
//     后手施放同：ROUND_END 在双方行动后。K470 674 InRounds 单发同构）。tick 内
//     owner 已倒/opp 已倒早退（K453 T8413/K470 存活门同款）。重施放 = 同
//     (source_id=728) 键覆盖（timed_bucket 同源去重）→ 单发刷新不叠加。
//
// ══ 动工前置三查（2026-11-18，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep -w 2035/728（src/ include/ 全量）零命中——无路由无硬编码，
//     SKILL_EFFECT default 桶（skills.cpp 未列 id 全落 SKILL_EFFECT）；
//   ② effect_info 2035（args_num=3）/728（args_num=1）行即模板（同文查无重复族；
//     2035 近邻=440"对手 PP 消耗倍率"（已实现 batch_b）/1895 吸取+未受固伤掷点
//     （已实现 j_drain_fixed_fallback_anomaly），728 近邻=763/1152 非提升门腿
//     （已实现）——异文已核）；side_effect 两行 ac=3/1 与 args_num 一致、10 载体
//     顺序切分零剩余（上表）；effect_des 两 id 无行；custom_effect_overrides/
//     programs/selection_routes/combo_rules 两 id 零行；tools/db_patches grep 两 id
//     零命中（无 900000 拆分迁移与故意退役记录）；
//   ③ registry moves=1590/soul=415（build/baseline-gate.log 1012 场景基线）无两 id；
//     本库 resources/test 全量 grep SKILL_EFFECT(2035/728、SOUL_PROGRAM(2035/728、
//     kMark 间接名零命中；HUB 全 worktree（agents/endgame-walk/k482/k483/k484/
//     web-replay）并集同 grep 零命中——真缺口。
//   双空间甄别（K458/K480 流程）：effect_icon 两 id 各有行（728→3046、2035→4434）
//     = 魂印空间**模板占用**，但 SOUL_PROGRAM 全库零注册（无运行时键段占用）→
//     审计基址免子块拆分（K480 1557 同判：soul 无实现=无键段）。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456/K461
//   键式；slot 0..15，owner×16 步长 > 槽位上限 → 同基址 owner 间无交叠）══
//   新增基址 1020350000/1020350100/1000728000 全库 grep 零占用（2026-11-18 现查）+
//   tools/check_storage_keys.py 门禁静态扫。
//   2035（基 1020350000）：
//     slot 0  {0} 次数行参回显
//     slot 1  {1} 每档点数回显
//     slot 2  {2} 每档级数回显
//     slot 3  剩余票（施放时重置 {0}；每命中消费 -1）
//     slot 4  累计命中消费次数（票逐次消耗面）
//     slot 5  末次消耗 PP 量（原语前后差）
//     slot 6  末次降属总档数（steps×{2}，命中时点计算值——施加在桥，未降写 0）
//     slot 7  末次消费回合
//     slot 8  累计消耗 PP 总量
//     slot 9  末次消费槽号（roundChoice 口径；-1=越界未消费）
//     slot 10 末次桥降属 SUCCESS 掩码（bit=引擎槽 0..5；桥 tick 写）
//     slot 11 末次桥降属 IMMUNE 掩码
//     slot 12 末次桥降属 AT_FLOOR 掩码
//     slot 13 累计 cast（武装）计数
//     slot 14 活跃 watcher id（重施放拆旧/耗尽自拆）
//     slot 15 跳过原因（0=无/1=缺 pp_zero_slot/2=槽号越界）
//   2035 桥子块（基 1020350100，+100 子块 K458 1565 双空间子块同法）：
//     slot 0  降属 pending 排空账（命中时点累加 steps×{2}；桥 tick 排空清 0）
//   728（基 1000728000）：
//     slot 0  {0} 分母回显
//     slot 1  门判定（1=开·已武装/0=闭——施放时点在位判读面）
//     slot 2  末次扣血回合
//     slot 3  末次计算量（max_hp/{0}）
//     slot 4  末次实际扣血（hp 前后差；免疫/护罩全挡=0）
//     slot 5  累计 tick 到达计数
//     slot 6  累计武装计数
//     slot 7  累计实际落地（>0）次数
//     slot 8  累计 cast 计数
//     slot 9  跳过原因（0=无/1=opp 已倒/2=owner 已倒/3=缺原语）
//
// ══ 变异测试实测（2026-11-18，本 worktree，定稿源面 13 变异 13 类真红；证据
//   build/k481-mutations/M*/：semantic.diff+grep-verify+build.log+build.rc+
//   dylib-sha-mutated+run.log+run.rc+first-kill+restore-cmp.rc+restore-build.log+
//   dylib-sha-restored+rerun.rc 全链；每次变异删 .o 强编（macOS Make 3.81 mtime
//   整数秒截断——K468/K473 记档）+SHA 双守卫，还原 cmp 字节级 + SHA==底本再
//   强编 deploy 复绿 rc=0）══
//   底本 dylib SHA256（变异轮源面，"删 .o 增量强编法"锚定、双跑复现）=
//   86f4fe19b0b74e0f2d3c1aec4d92910240660ed89c17345766dd3834466eed03
//   （resources/moves_lib/liblib_1.dylib）。本头注 M 节回填后源面仅注释行数
//   差异，终稿 dylib SHA256 =
//   4591e4d36513fa07db01eae4201c63f8a2720f8b3b36317013c2e5be768e9b77
//   （build/k481-mutations/baseline.sha 终录同值）。⚠️ SHA 锚定方法论记档：同源码以
//   "全量首建"与"删 .o 增量强编"两法产出的 dylib SHA 不同（macOS ld 输入集
//   差异，非源面差异——首法 6aa6f552/c9f1b6a6 各轮实测）；变异协议底本必须与
//   还原步**同法**锚定（增量强编法），否则还原步 SHA 永不回底本。
//   历史轮记档：首轮 12 变异（命中时点直降实现）全红后，全量门 8410 红测出
//   降属时点交互缺陷 → 改桥式延后（见「实现口径」节）→ M6 杀点随循环迁移到
//   桥 tick、T8680.g 断言翻转为"击杀不降（掩码 0）"、新增 M13 → 权威轮重跑。
//   13 变异 13 类真红（rc=134 CHECK abort）：
//   M1  票耗尽自拆删除 → 首杀 T8680.a R3 watcher id=1≠0（滞留 watcher 未拆）；
//   M2  事件通道置换（EVENT_HIT→EVENT_SKILL_USED）→ 首杀 T8680.a R2 消耗量=缺席
//       ≠4（回调首行 type 守卫拒收新通道事件——通道契约面；R1 结构性排除仍成立）；
//   M3  攻击过滤删除（executed_skill_is_attack→恒假短路）→ 首杀 T8680.c R3 缩头
//       属性技命中消费票（剩余票 0≠1——"攻击命中"字面锁定）；
//   M4  {1} 除数破坏（steps=consumed 直取）→ 首杀 T8680.b 总档=7≠2（7/3 整数除
//       阶梯面）；
//   M5  {2} 乘法删除（total=steps）→ 首杀 T8680.d2 总档=2≠4（override {2}=2
//       变档腿锁行参消费）；
//   M6  全属性槽覆盖（桥 tick 六槽→五槽跳命中槽）→ 首杀 T8680.a SUCCESS 掩码
//       =31≠63（全属性六项含命中口径）；
//   M7  作用域置换（ON_STAGE→TEAM）→ 首杀 T8680.n 换宠后拍打 pp=0≠5（离场
//       epoch 作废面）；
//   M8  审计键步长折半（owner×16→×8）→ 首杀 T8680.e side1 剩票读穿 side0 键
//       （-1≠1——per-owner 分账面）；
//   M9  PP 消耗读面破坏（pp_zero_slot→pp_reduce(1) 常规消耗读法）→ 首杀
//       T8680.a 消耗量=1≠4（"全部PP值"=常规消耗前剩余全量）；
//   M10 728 门时点翻转（施放门删+tick 回合末在位重判）→ 首杀 T8680.i 无提升局
//       武装计数=1≠0（无条件武装面；T8680.j 互补腿面同链红——763 自升喂门）；
//   M11 728 {0} 硬编码 5 → 首杀 T8680.k1 计算量=19999≠33333（{0}=3 真实载体
//       行参消费面）；
//   M12 728 tick 注册删除 → 首杀 T8680.h tick 到达=缺席≠1（单发窗注册面）；
//   M13 桥生存门删除（目标已倒照降）→ 首杀 T8680.g 击杀回合掩码=63≠0（降属
//       后果不施加于已倒者）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k2035AuditBase = 1020350000;   // 10^9 + 2035×10^4（全库零占用）
constexpr int k2035BridgeBase = 1020350100;  // 2035 降属桥子块（+100，K458 1565 双空间
                                             // 子块同法；pending 排空账专用）
constexpr int k728AuditBase = 1000728000;    // 10^9 + 728×10^4（全库零占用）

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

// ── "自身处于能力提升状态"在位判读（K289 659 直系：任一能力等级 >0，六槽含命中）──
bool k481_self_has_boost(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            return true;
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// 2035 —— 下{0}次攻击命中时消耗所使用攻击技能的全部PP值，每消耗{1}点令对手全属性-{2}
// 行参布局 [owner, target, {0}=次数, {1}=每档点数, {2}=每档级数]
// （args_num=3 + [owner,target] 前缀）。
// ═══════════════════════════════════════════════════════════════════════════

EffectResult k481_2035_stat_drop_tick(BattleContext* ctx, const EffectArgs& args);

// EVENT_HIT watcher：自身攻击技命中 → 消耗当次技能全部剩余 PP + 排入降属桥。
// （回调跑在命中当击 ON_SKILL_HIT 桶末 drain——早于常规 PP 消耗点；降属延后至
//   本行动 AFTER_ACTION 桥，见文件头「时点甄别」。）
void k481_2035_on_hit(BattleContext* c, const BattleEvent& ev) {
    if (!c || ev.type != EventType::EVENT_HIT) {
        return;
    }
    const int self = ev.actor;   // EVENT_HIT: actor=命中方、target=对方（skills.cpp emit 口径）
    if (self < 0 || self > 1) {
        return;
    }
    // "攻击命中"：属性技能命中也发 EVENT_HIT → 执行期攻击判据过滤
    // （ws.skill_type_view 本次物化，ON_SKILL_HIT 后回调语境合法，common.h）。
    if (!executed_skill_is_attack(c, self)) {
        return;
    }
    // 票面：无余票（理论不可达——耗尽即拆 watcher；双保险防串账）不消费。
    int remaining = storage_int_local(c, audit_key_of(k2035AuditBase, self, 3), 0);
    if (remaining <= 0) {
        return;
    }
    const int per = storage_int_local(c, audit_key_of(k2035AuditBase, self, 1), 0);
    const int level = storage_int_local(c, audit_key_of(k2035AuditBase, self, 2), 0);
    // 消耗槽位 = 玩家点的槽位（替换技 PP 走原槽位口径；越界 → 记跳过、不消费票）。
    int slot = -1;
    if (c->roundChoice[self][0] == 1 /* SELECT_SKILL，battleFsm ActionType 同值 */) {
        const int idx = c->roundChoice[self][1];
        const int n = static_cast<int>(c->getPet(self).skills.size());
        if (idx >= 0 && idx < n) {
            slot = idx;
        }
    }
    if (slot < 0) {
        set_storage_int_local(c, audit_key_of(k2035AuditBase, self, 15), 2);
        return;
    }
    if (!g_core || !g_core->pp_zero_slot) {
        set_storage_int_local(c, audit_key_of(k2035AuditBase, self, 15), 1);
        return;
    }
    // "消耗所使用攻击技能的全部PP值"：当次技能剩余全量（常规 1 点消耗在
    // AFTER_ACTION，尚未发生）→ pp_zero_slot 原语全额清零（无限槽/已 0 幂等）。
    const int before = c->getPet(self).skills[static_cast<std::size_t>(slot)].pp;
    (void)g_core->pp_zero_slot(c, self, slot, /*actor=*/self);
    const int after = c->getPet(self).skills[static_cast<std::size_t>(slot)].pp;
    const int consumed = before - after;
    set_storage_int_local(c, audit_key_of(k2035AuditBase, self, 5), consumed);
    set_storage_int_local(c, audit_key_of(k2035AuditBase, self, 7), c->roundCount);
    set_storage_int_local(c, audit_key_of(k2035AuditBase, self, 9), slot);
    set_storage_int_local(c, audit_key_of(k2035AuditBase, self, 8),
                          storage_int_local(c, audit_key_of(k2035AuditBase, self, 8)) + consumed);
    set_storage_int_local(c, audit_key_of(k2035AuditBase, self, 4),
                          storage_int_local(c, audit_key_of(k2035AuditBase, self, 4)) + 1);
    // 票逐次消耗（"下{0}次"计数面）；耗尽自拆 watcher（不留滞留——once=false 多击面）。
    --remaining;
    set_storage_int_local(c, audit_key_of(k2035AuditBase, self, 3), remaining);
    if (remaining <= 0) {
        const int wid = storage_int_local(c, audit_key_of(k2035AuditBase, self, 14), 0);
        if (wid > 0) {
            c->event_center_.remove_watcher(wid);
        }
        set_storage_int_local(c, audit_key_of(k2035AuditBase, self, 14), 0);
    }
    // "每消耗{1}点令对手全属性-{2}"：阶梯档 = 消耗量 / {1}（不足一档不降）。
    int steps = 0;
    if (per > 0 && consumed > 0) {
        steps = consumed / per;
    }
    const int total = steps * level;
    set_storage_int_local(c, audit_key_of(k2035AuditBase, self, 6), total);
    if (total <= 0) {
        return;
    }
    // 降属**延后到本行动 AFTER_ACTION 桥**（K470 674 桥式直系）：命中当击只入账
    // pending + 武装桥——本次攻击自身的 SKILL_EFFECT 链（消强/吸血/回复等）先于
    // 桥跑，"消耗的后果"不扰动触发攻击自身的结算链（K454 8410 交互实证记档：
    // 命中时点降属会把对手等级先压穿，同击携带的 1974 消强找不到正等级 → 消强
    // 成功链断——延后即解）。同回合多击（额外行动）= pending 累计 + 同键桥覆盖
    // 重武装，每次 AFTER_ACTION 排空一次。
    set_storage_int_local(c, audit_key_of(k2035BridgeBase, self, 0),
                          storage_int_local(c, audit_key_of(k2035BridgeBase, self, 0)) + total);
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(2035, 0, self, -1, EffectArgs(std::vector<int>{self}),
                     &k481_2035_stat_drop_tick);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     c->roundCount);
        ce->source_id_ = 2035;
        c->register_skill_effect(st, self, std::move(ce));
    }
}

// 2035 降属桥 tick（AFTER_ACTION 双桶、InRounds rounds=1；桶只跑行动方条目）：
// 排空 pending → 六槽 stat_drop（0..5 含命中；查免弱/ATTACH_BAN、-6 钳；actor=
// 属主——battle_primitives.h:505 如实声明口径）。生存门在桥时点判（伤害已落地）：
// 目标已倒 → 排空不降（击杀回合不降——K430/K470 口径；官方无击杀回合实测，
// 按"后果不施加于已倒者"建模记档，T8680.g 锁死）。
EffectResult k481_2035_stat_drop_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    const int opp = 1 - self;
    const int pending = storage_int_local(ctx, audit_key_of(k2035BridgeBase, self, 0), 0);
    int ok_mask = 0;
    int immune_mask = 0;
    int floor_mask = 0;
    if (pending > 0) {
        if (ctx->getPet(opp).hp > 0 && g_core && g_core->stat_drop) {
            for (int s = 0; s < 6; ++s) {
                const StatDropResult r = g_core->stat_drop(ctx, opp, s, pending, self);
                switch (r) {
                    case StatDropResult::SUCCESS:
                        ok_mask |= (1 << s);
                        break;
                    case StatDropResult::IMMUNE:
                        immune_mask |= (1 << s);
                        break;
                    case StatDropResult::AT_FLOOR:
                        floor_mask |= (1 << s);
                        break;
                    default:
                        break;
                }
            }
        }
        set_storage_int_local(ctx, audit_key_of(k2035BridgeBase, self, 0), 0);   // 排空
    }
    set_storage_int_local(ctx, audit_key_of(k2035AuditBase, self, 10), ok_mask);
    set_storage_int_local(ctx, audit_key_of(k2035AuditBase, self, 11), immune_mask);
    set_storage_int_local(ctx, audit_key_of(k2035AuditBase, self, 12), floor_mask);
    return EffectResult::kOk;
}

// int_args = [owner, target, {0}, {1}, {2}]。
EffectResult effect_2035_pp_drain_allstat_steps(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2035, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 2035, args,
                     "2035 布局=[owner,target,次数,每档点数,每档级数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int times = args.int_args[2];
    const int per = args.int_args[3];
    const int level = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 2035,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(per > 0, 2035, args, "需要 {1} 每档点数 > 0（除数）");
    // {0} 在位即消费：显式 0 = 关闸档（不武装不报故障，K314/K340 纪律；{2}<=0 =
    // 阶梯降属自然 no-op——票照发、消耗照走、降属步进 0）。
    set_storage_int_local(ctx, audit_key_of(k2035AuditBase, self, 0), times);
    set_storage_int_local(ctx, audit_key_of(k2035AuditBase, self, 1), per);
    set_storage_int_local(ctx, audit_key_of(k2035AuditBase, self, 2), level);
    set_storage_int_local(ctx, audit_key_of(k2035AuditBase, self, 13),
                          storage_int_local(ctx, audit_key_of(k2035AuditBase, self, 13)) + 1);
    if (times <= 0) {
        return EffectResult::kOk;
    }
    // 重施放刷新：余票重置 {0}（不叠加，K461 1645 口径）+ 拆旧 watcher 防双响应
    // （K480 711 重施放拆旧同款；摘不到 = 已耗尽自拆/已作废，安全 no-op）。
    const int old_watcher = storage_int_local(ctx, audit_key_of(k2035AuditBase, self, 14), 0);
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }
    set_storage_int_local(ctx, audit_key_of(k2035AuditBase, self, 3), times);
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_HIT, self, ctx->roundCount,
        /*duration_rounds=*/0,   // 次数票无回合界（K461 口径）；生命周期=耗尽自拆+ON_STAGE
        /*once=*/false, &k481_2035_on_hit,
        WatcherScope::ON_STAGE,   // 换宠 epoch 作废（离场清理）
        /*valid_id=*/ctx->watcher_valid_id[self]);   // 必传当前 epoch（K453 教训）
    set_storage_int_local(ctx, audit_key_of(k2035AuditBase, self, 14), watcher_id);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 728 —— 自身处于能力提升状态时，回合结束时直接减少对手1/{0}最大体力
// 行参布局 [owner, target, {0}=分母]（args_num=1 + [owner,target] 前缀）。
// 门在 SKILL_EFFECT（施放时点）在位判读（互补腿 763/1152 结构证据，见头注）；
// 门开 → 武装本回合 ROUND_END 单发 tick → 粉伤固定档扣 max_hp/{0}。
// ═══════════════════════════════════════════════════════════════════════════

// ROUND_END 单发 tick：固定档粉伤扣对手 max_hp/{0}（正式管线：免粉/护罩/固定抗性
// 可挡、emit EVENT_TAKE_DAMAGE——不直写 hp，837 盲区对照）。
EffectResult k481_728_round_end_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key_of(k728AuditBase, self, 5),
                          storage_int_local(ctx, audit_key_of(k728AuditBase, self, 5)) + 1);
    // 存活门：owner 已倒（同归/亡语同波窗口）→ 扣血无来源（K453 T8413 同款）。
    if (ctx->getPet(self).hp <= 0) {
        set_storage_int_local(ctx, audit_key_of(k728AuditBase, self, 9), 2);
        return EffectResult::kOk;
    }
    // 生存门：目标已倒 → 整条跳过（K430/K470 口径）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key_of(k728AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->deal_pink_damage) {
        set_storage_int_local(ctx, audit_key_of(k728AuditBase, self, 9), 3);
        return EffectResult::kOk;
    }
    const int amount = max_hp_of(ctx, opp) / denom;   // "1/{0}最大体力"（整数除）
    set_storage_int_local(ctx, audit_key_of(k728AuditBase, self, 3), amount);
    set_storage_int_local(ctx, audit_key_of(k728AuditBase, self, 2), ctx->roundCount);
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    const int hp0 = ctx->getPet(opp).hp;
    (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::FIXED, self);
    const int actual = hp0 - ctx->getPet(opp).hp;
    set_storage_int_local(ctx, audit_key_of(k728AuditBase, self, 4), actual);
    if (actual > 0) {
        set_storage_int_local(ctx, audit_key_of(k728AuditBase, self, 7),
                              storage_int_local(ctx, audit_key_of(k728AuditBase, self, 7)) + 1);
    }
    return EffectResult::kOk;
}

// int_args = [owner, target, {0}]。
EffectResult effect_728_boost_gate_round_end_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 728, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 728, args,
                     "728 布局=[owner,target,分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 728,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 728, args, "需要 {0} 分母 > 0");
    set_storage_int_local(ctx, audit_key_of(k728AuditBase, self, 0), denom);
    set_storage_int_local(ctx, audit_key_of(k728AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k728AuditBase, self, 8)) + 1);
    // 提升门：施放时点在位判读（K289 任一正槽直系；互补腿 763/1152 结构证据——
    // 门闭 → 不武装，回合末获提升不补触发（负测 T8680.j 锁死）。
    const bool gate = k481_self_has_boost(ctx, self);
    set_storage_int_local(ctx, audit_key_of(k728AuditBase, self, 1), gate ? 1 : 0);
    if (!gate) {
        return EffectResult::kOk;
    }
    // 回合末单发（InRounds 含施放回合——ROUND_END 同回合后至；per-owner 桶单跑，
    // 同 (source_id=728) 键重施放覆盖刷新不叠加）。
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, /*effect_id=*/728,
                          /*rounds=*/1, {self, opp, denom}, &k481_728_round_end_tick);
    set_storage_int_local(ctx, audit_key_of(k728AuditBase, self, 6),
                          storage_int_local(ctx, audit_key_of(k728AuditBase, self, 6)) + 1);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(2035, effect_2035_pp_drain_allstat_steps)
SKILL_EFFECT(728, effect_728_boost_gate_round_end_drain)
