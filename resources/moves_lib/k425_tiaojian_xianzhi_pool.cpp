// K425 · 效果 1774 族 —— 当回合条件先制族 8 载体（2026-11-07，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k425，分支 feat/k425，场景段 8080-8089 未占用）
//
// ✅ 已落地（2026-11-07，J335 续单：内核+DB 授权单，worktree j335，分支 feat/j335）：
//   ① 路由行 = tools/db_patches/j4/selection_routes_expansion_5.sql（36→37 行态，
//      INSERT OR IGNORE (1774,'selection','FIRST_MOVE_RIGHT','plain',1,evidence)）；
//   ② 插件体 = resources/moves_lib/j335_cond_prio_vs_attack.cpp
//      （SKILL_EFFECT(1774)，2273 同形门判据：对手选攻击技 → preemptive +1）；
//   ③ 场景 = test/scenario/scenario_8080_cond_prio_vs_attack.cpp（号段续用
//      8080-8089，K425 未占用）+ 场景 7930 期望同步（37 行 / 引用面 469/465）。
//   本文件按原「留续单」段收尾约定改头注为落地形态，行清单/四查/键段留档。
//
// ── 官方语义现查（seer_unity.sqlite effect_info id=1774，2026-11-07）─────────
//   1774 | args_num=0 | 「若对手当回合使用的技能为攻击技能则自身额外先制+1」
//   param=[]，analyze 同文加色（"当回合/先制"高亮），type=1，key 空。
//   零参宏，先制量写死模板文案（+1），无行参可消费（§七点五口径）。
//   语义形态 = 《效果实现套路目录》§七点五 条件先制族：
//     门 = 对手本回合所选技能为攻击技（物理/特殊；SkillType 三值
//     Physical/Special/Attribute，include/entities/skills.h:21——"非属性"即攻击）；
//     出 = 自身本轮先制 +1（ws.preemptive_level）。
//   同形先例 = 2273「对手选择攻击技能时此技能先制+3」
//   （resources/moves_lib/artemis_skills.cpp:174 effect_2273_priority_vs_attack，
//   门判据 = roundChoice[target][0]==SELECT_SKILL 且 skills[idx].type !=
//   SkillType::Attribute）；载体描述自证：饭后甜点 28366 info
//   「若对手当回合使用攻击技能则自身出手更快」。
//
// ── 落地通道（原 K425 裁决段，J335 落地后收尾改写）────────────────────────
//   当回合条件先制的唯一活通道 = **选择期路由**：先手权在选技阶段就定
//   （src/fsm/battleFsm.cpp handle_BattleFirstMoveRight 入口 memset
//   ws.preemptive_level → 跑两侧 MOVE_RIGHT 桶 → 先制/速度比较；链上效果执行于
//   出招期，无法回改本轮先制）。正解 = custom_skill_selection_routes DB 路由行
//   （J327 起数据化；行只描述"何时装槽、如何喂参"，条件判定仍在插件体
//   MOVE_RIGHT 桶运行时执行——条件先制族现役载体 881/958/1081/1174/1956 等全部
//   持路由行，行为由场景 7931 走查守护）。effect_register_state 补 case（2020/
//   2273 旧形态）不可用：J66 实测勘误明令「条件先制族须走选择期路由（DB 加行），
//   勿再加 case」（src/entities/skills.cpp 勘误注）；存量 case 为既有记档项不翻案。
//
// ── 行清单（unity 库 json_each 精确匹配 side_effect 主链；friend_side_effect
//    8 行全空；切片基准 = side_effect.arg_count，两表不一致时以 side_effect 为准
//    （src/db/official_data_repository.cpp:173-174）；1774 双表一致
//    （side_effect.arg_count=0 = effect_info.args_num=0）。2026-11-07 逐行核算）──
//   28051 启明之辰     属性 p0   先0 [1774,191,847,1342] args[4,400,400]
//        → 1774:[] / 191:[4] / 847:[400] / 1342:[400]（0+1+1+1=3 ✓）
//        派利尔 4408 lv49
//   28366 饭后甜点     属性 p0   先0 [1617,1774,870] args[3,70,2,1,400]
//        → 1617:[3,70,2,1] / 1774:[] / 870:[400]（4+0+1=5 ✓）华夫家族 4526 lv57
//   28668 安魂引       属性 p0   先0 [1774,191,426,1400] args[4,3,100,3,100,6]
//        → 1774:[] / 191:[4] / 426:[3,100] / 1400:[3,100,6]（0+1+2+3=6 ✓）
//        混沌暗王霍德尔 3941（sp_hide_moves id=10042 隐藏技）
//   35652 几曾识干戈   物理 p150 先0 [697,1568,1774,1395] args[]（全链零参 ✓）
//        极战·阿尔斯兰 4327 lv75
//   35786 兽灵震谷爪   物理 p150 先0 [1774,664,1736] args[4]
//        → 1774:[] / 664:[] / 1736:[4]（0+0+1=1 ✓）花寅 4353 lv61
//   35907 御风无痕     物理 p85  先3 [1774,1099,769] args[1,100]
//        → 1774:[] / 1099:[1] / 769:[100]（0+1+1=2 ✓）圣谕之风·岚 4376 lv71
//   36277 雾停枝       特殊 p130 先0 [1774] args[]（纯单载体 ✓）流荧·雪 4448 lv45
//   36411 灰刻截光破   物理 p150 先0 [1774,925,937] args[50,100,30,2,1]
//        → 1774:[] / 925:[50] / 937:[100,30,2,1]（0+1+4=5 ✓）夜罗·琉彩 4472 lv61
//   链上 16 个兄弟效果（191/847/1342/1617/870/426/1400/697/1568/1395/664/1736/
//   1099/769/925/937）已全部注册在库（registry moves 2026-11-07 快照逐一比对
//   全 True），本族唯一缺口 = 1774 本体。
//
// ── 动工前置四查（2026-11-07，本 worktree + 主库现查）────────────────────────
//   ① 内核 grep 1774：src/ include/ 零命中（rc=1）——零行为硬编码；
//   ② effect_info 在列（args_num=0，param=[]）；8 载体切片逐位自洽（见行清单）；
//     custom_effect_overrides / custom_effect_programs 零命中（0/0）；
//   ③ 全库并集无注册：registry moves=1455 无 1774；resources/
//     SKILL_EFFECT(1774 / SOUL_PROGRAM(1774 / kMark1774 零命中（rc=1）；
//     全 feat/* 分支 git grep "SKILL_EFFECT(1774" 零命中（含并行 K424 未合并
//     提交）；boss_invalid_effects.json:835 命中 1774 = **soul 空间**
//     effect_icon 尾注同号行（文件 namespace:"soul"，source=effect_icon.tips
//     解析）——SKILL_EFFECT/SOUL_MARK 两注册空间互不相干，非撞号
//     （K418 1941 同款溯源）；
//   ④ 废弃 parser 覆盖面：模板零占位符（args_num=0，无 {} 可消费）且不含
//     第一刀「令/使+对手/自身+{」四形态（src/effects/effect_unit_parser.cpp:75）
//     、不含第二刀五条件形态（先出手/后出手/不处于异常状态/体力不足秒杀/
//     无异常吸取，同文件 :156-194）→ parse_effect_unit 返 -1，clone 无实现时
//     loadSkills 维持静默跳过语义——无「半解析静默误执行」形态。
//
// ── 键段 ──────────────────────────────────────────────────────────────────────
//   本单无新键段（不建任何槽/审计位；条件先制族在路由体即判即用，无跨回合状态）。
//
// ── 留续单（施工清单，已由 J335 全部落地——2026-11-07）─────────────────────
//   1. j4 扩行补丁 ✅ selection_routes_expansion_5.sql（37 行显式集合 + verify；
//      expansion_4 精确 36 断言同批放宽为 36 种子集——J332 对 expansion_3 先例）；
//   2. 插件体 ✅ j335_cond_prio_vs_attack.cpp（2273 同形门判据，量写死模板文案 +1；
//      审计槽 1017740001(+owner)=授予的先制量，缺席=门闭）；
//   3. 场景 ✅ scenario_8080（先手权结构断言 + 属性技反例 + 嗑药反例 + 3 载体
//      代表 36277/35652/28051 + 变异面）。
//
// 本文件无代码：翻译单元仅含注释（编译为空 TU，GLOB 自动收录，CMakeLists.txt:267）。
