# D 组·第五技批产 —— 新会话提示词模板（2026-09-28）

> 用法：复制下面整段给新会话，把 **`【任务序号】`** 换成工单表里的序号
> （可多个，如 D2、D5；序号表见 `docs_local/docs/05-任务清单/已收官/技能组批产工单.md` D 组表）。
> 多会话并行时：一个会话领一个序号，互不重叠（效果 id 天然不重叠，场景编号按下面规则自选）。

---

你是赛尔号战斗模拟引擎（C++17，仓库 /Users/zhuhongrui/code/c++/project/sbs-local，主分支 main-local）的技能批产工程师。任务：给工单 D 组序号 **【任务序号】** 的精灵实现第五技能的缺失效果。

## 第一步：领取（hub 登记优先！）再建工作区
```
cd /Users/zhuhongrui/code/c++/project/sbs-local
# ① 先在主检出的启动看板把本任务改成 🔄 进行中（附 worktree/分支名）并提交到
#    main-local——hub 看板是唯一领取登记处，在自己分支上改 = 其它会话看不见 = 重复领取
# ② 然后切分支和工作区（从更新后的 main-local，领取提交自动带上）：
git worktree list   # 已有同名 worktree 则复用
git worktree add ../agents/d-【任务序号】 -b feat/d-【任务序号】 main-local
cd ../agents/d-【任务序号】
mkdir -p scripts && ln -sfn /Users/zhuhongrui/code/c++/project/sbs-local/scripts/data scripts/data
cmake -B build && cmake --build build -j8
```

## 第二步：必读（按序）
1. `docs_local/docs/05-任务清单/已收官/技能组批产工单.md` —— D 组表：你的序号对应的精灵/第五技/缺失效果 id
2. `docs_local/docs/05-任务清单/已收官/技能组批产盘点-明细.md` —— 官方模板原文
3. `docs_local/docs/06-交接/魂印与技能机制-交接.md` 附录一/二 —— 注册键教训 + 顺序守则
4. `docs_local/docs/02-效果系统/技能判定流程与无效效果体系.md` §7.6 —— 第五技三分层（is_locked/罚站/沉默）
5. 实现范式样板：`resources/moves_lib/batch_a_skills.cpp` + `test/scenario/scenario_140*.cpp`

## 第三步：实现
- 效果官方模板查库：`sqlite3 /Users/zhuhongrui/code/c++/project/Simulation-battle-system/scripts/data/processed/seer_unity.sqlite "SELECT id, args_num, info FROM effect_info WHERE id IN (...)"`；携带技能参数切片看 `moves.side_effect_arg`（切分规则：side_effect.arg_count 优先、effect_info.args_num 兜底，见 src/db/official_data_repository.cpp build_skill_effects）。
- 新效果放**一个新文件** `resources/moves_lib/d_fifth_skills.cpp`（SKILL_EFFECT 宏），不改他人文件；实现前先 grep 全库确认是否已有实现/参数泛化。
- 模板有截断/歧义：以**携带技能的 4399 描述原文**为准（reference/scrape_4399/out/pets_full.jsonl，grep 技能名），对照参数切片逐位核；仍不确定取保守实现+注释记待拍板。
- 概率类效果走 `apply_anomaly_checked` 申报（亮节闸门管辖；本地 roll_percent 会被场景 103 抓）。
- 「未击败对手则X / 击败对手则Y」族：X/Y 在击败时点之后生效的，参考 1679 的延迟注册（OLD_ROUND_END_1 一次性节点）与 840 族。
- 2295 类「废除此技能」：`Skills::is_locked = true`（用户 2026-09-28 定稿，同第五解封线三分层）。

## 硬性禁令
- 禁改内核（src/、include/）与 soul_lib；需要新原语的效果跳过并记录。
- 引用数 >100 的效果 id、697/699（内核穿透凭证）不在本工单，跳过。
- 不动其他 moves_lib 文件；场景编号 **170 起**自选不与已有冲突（ls test/scenario/ 核对）。

## 验收
```
cmake --build build -j8        # 必须全量构建——只编单目标不会部署插件 dylib
for bin in build/sim_scenario_*; do ./"$bin" >/dev/null 2>&1 || echo "FAIL $(basename $bin)"; done   # 应无输出
./build/sim_server --dump-registry-exit build/registry.json
```
⚠️ 新场景/新文件 `git add -f`（历史教训：ignore 规则静默吞文件）。

## 收尾
- 每只精灵一个 commit；**不要合并 main-local、不要 push**。
- 汇报：实现清单（效果 id → 方式一句话）、跳过项及原因、全套回归结果。
