# 对战引擎 fuzz（seed 循环）

`sim_fuzz_battle`：生成式随机对战压测器。fuzzer **不发明非法输入**——

- **队伍生成**：从官方 DB（`scripts/data/processed/seer_unity.sqlite`）抽真实精灵
  （学习表 ≥5 个互异技能），走 `PetFactory` 组装：魂印（effect_icon 链路）、技能槽位规则
  （1-4 槽学习表 / 第 5 槽可隐藏技、5 技互异）、通用特性、空位（pet_id=-1）全部由引擎
  自己的校验把关。队伍规模 1~6 只随机，同侧精灵 id 不重复（擂台规则口径）。
- **对战驱动**：每个决策点问 `server::compute_legal_actions`（与客户端合法按钮同源，
  已含切换锁/死亡/凝滞瘫痪过滤），在「可用技能 / 可换精灵 / 可嗑药剂 / 空过(NONE)」
  里均匀采样。驱动走 `m_buffer` 提交路径，与真实客户端一致。药剂仅选技期可用，
  回合末死切不能嗑药；用户 2026-11-06 权威说明见
  `docs_local/docs/02-效果系统/时点判定流程表.md`「选择使用技能 / 选择使用药剂」补充。
- **可复现**：全随机源 = 引擎自有 `sbs_rand()`，`sbs_srand(seed)` 在队伍生成与每场对战前分别播种。
  任何发现都能用 `--seed N` 单跑复现（`--show-party N` 查看该 seed 的双方阵容）。
- **零内核改动接入**：只依赖生产接口（ScenarioBattle 同款驱动路径 +
  `compute_legal_actions` + `PetFactory`），fuzz 引入的所有引擎侧修改都是**修复**，
  不是为 fuzz 打的洞。

> **工作区约定**：夜跑由 `tools/night/fuzz_night.sh` 自管（专用 worktree
> `agents/night-fuzz`，ASan 插件 deploy 与主工作区完全隔离）。人工调试时从
> main-local 建临时 worktree 即可。历史教训（2026-09-30）：在共享目录里切分支，
> 并行会话切回后 fuzz 提交被混进主线——分支必须配 worktree 用。
> `scripts`/`reference` 软链与 `*.dylib` 为机器本地物，新 worktree 需重建软链
> 并重跑三个 `*_plugin_deploy` 目标。

## 构建

```bash
# 普通版（快速迭代 / 大规模跑不变量+确定性 oracle）
cmake -B build && cmake --build build --target sim_fuzz_battle

# sanitizer 版（独立 build 目录！见下方 ⚠️）：ASan + UBSan（UBSan 报错即 abort）
cmake -B build-fuzz -DSBS_FUZZ_SANITIZERS=ON
cmake --build build-fuzz --target sim_fuzz_battle moves_plugin_deploy soul_plugin_deploy suit_plugin_deploy
```

⚠️ **`SBS_FUZZ_SANITIZERS` 会把带 ASan 的插件 dylib 部署进源码树 `resources/*_lib/`**，
普通 build 运行时也加载这份 dylib → 混用必崩。fuzz 战役结束后必须恢复普通部署：

```bash
cmake --build build --target moves_plugin_deploy soul_plugin_deploy suit_plugin_deploy
```

⚠️ 消毒选项通过 `add_compile_options` 在 **CMakeLists 顶部**全局生效——必须覆盖
sim_core 等全部目标。曾经放中部导致 sim_core 无消毒、与消毒插件混链出容器注解
desync（见发现 #6）。新增目标时留意这一点。

## 运行（从仓根跑；二进制自己也会向上找仓根）

```bash
./build/sim_fuzz_battle --from 0 --count 1000            # seed 0..999，默认确定性双跑
./build-fuzz/sim_fuzz_battle --from 0 --count 3000       # ASan/UBSan 战役
./build/sim_fuzz_battle --seed 446 --verbose             # 单 seed 复现（崩溃排查）
./build/sim_fuzz_battle --show-party 446                 # 查看某 seed 的阵容
```

| 参数 | 说明 |
|---|---|
| `--from N` / `--count N` / `--seed S` | seed 区间；`--seed` = 单跑 |
| `--max-points P` | 决策点上限（默认 400；打满记 stalled，不算失败） |
| `--no-determinism` | 关闭同 seed 双跑比对（提速 ~2×） |
| `--allow-arg-fault` | 效果参数解包失效降级为计数（默认 exit 4） |
| `--engine-logs` | 保留引擎 `[INFO]` 战斗日志（默认 stdout → /dev/null 静音） |
| `--verbose` | 每个 seed 打一行结果 |

fuzz 自身输出全走 **stderr**（stdout 可能被静音）。

## oracle 分层（决定发现面的关键设计）

| 层 | 机制 | 抓哪类问题 | exit |
|---|---|---|---|
| 挂死 | 每局 `alarm(120)`，正常对局毫秒级 | **逻辑死循环/活锁**（控制权不回归的，
  决策点类守卫全部失效，alarm 是唯一兜网） | 43 |
| 内存/UB | ASan + UBSan（-fno-sanitize-recover），**全目标统一消毒** | 越界、UAF、有符号溢出等 | abort |
| 不变量 | 每决策点断言：`hp ≤ 体力上限`（引擎 `reduce_max_hp_*` 自维护的那条）、
  `pp ≥ 0`、槽位界内、**已登记死亡 → hp ≤ 0**（死亡裁定权威） | 效果绕过原语直写状态 | abort |
| 确定性 | 同 seed 双跑，逐决策点状态签名序列必须一致 | "确定性路径偷耗 sbs_rand()"、
  跨局全局状态泄漏 | 3 |
| 效果参数 | `effect_arg_fail_fast`（同场景 harness 口径） | 解包失效=假绿 | 4 |
| 非预期异常 | 非 `bad_any_cast` 的新异常逃逸到驱动层 | 引擎/效果契约破坏 | 5 |

确定性双跑至今零命中——它没抓到 bug，但它**持续守护**着"确定性路径短路不消耗
sbs_rand()"这条全库纪律，任何新效果破坏它都会立刻红。

## 发现总账（2026-09-30 首役 → 2026-10-03，累计 4 万+ seed）

**7 项已修复 + 1 项结案 + 1 项开放**。按根因分类（不是按症状）——同一句
"fuzz 崩了"背后是完全不同类别的问题：

### A. 语义/规格缺口（逻辑错误，影响最大）

**#2 引擎活锁：CHOOSE_AFTER_DEATH 无限自旋**（seed 11263，挂死 oracle，exit 43）
- 症状：一场对局 120 秒不结束，`run()` 单次调用内自旋
  `handle_ChooseAfterDeath`（sample 证实循环里只有这一个 handler），服务端
  战斗线程会整体静默挂死——**不崩、不退、CPU 100%**，比崩溃更难发现。
- 根因链：场上精灵被打死 → FSM 进入"死后选择"等待换宠指令 → 等待期间某效果
  把死宠奶活 → `need_input()` 的"场上有人倒下"判据失效返回 false，而 handler
  只有"收到输入 / 没收到报错"两个分支，**没有"等待前提已消失"分支** → 状态
  永不推进 → 自旋。结构根源是死亡语义在引擎里**三处判据各自为政**
  （`need_input` 的 `hp==0` vs `AFTER_DEFEATED` 的 `hp<=0` vs handler 无前提检查）。
- 修法：handler 顶部加前提守卫（两边场上都活着 → `generateState()` 推进进
  下回合）+ `need_input` 判据统一 `hp <= 0`。验证：120s 挂死 → 1s 正常收官。
- **延伸影响**：这个发现直接推动了死亡裁定语义的正式定稿——"治疗不可逆已裁定
  死亡 / 亡语豁免 / 复活走 `revive_pet` 唯一出口（免死是预死亡信号，裁决前拉起
  体力）"，落地为三层防护：场景 092 语义钉子 + `is_death_registered` 门控 +
  fuzz 死亡 oracle；主线 terminal-dswitch D1-D5 随后把"全灭判负 FINISH"也补上
  （fuzz 曾定量暴露：修复前 **100% 对局**以"全灭卡死+服务端兜底"收尾，引擎自身
  从未走到 FINISHED）。
- **分类要点：这是纯逻辑错误，不涉任何内存问题**——抓它的 oracle 是挂死检测，
  不是 sanitizer。若只拿 sanitizer 做 fuzz，这一类（往往也是影响最大的）会全部漏掉。

### B. 事件/类型契约违反（以崩溃为症状）

**#1 效果 452 攻击吸血炸进程**（seed 446，崩溃 oracle）：插件存储键位
`连续序号+owner` 跨界碰撞（`002+0 == 001+1`），0 号方的注册守卫**布尔**落进
1 号方 rounds 的读键位，watcher 抛式 `any_cast<int>(bool)` 直接 abort。
修法：`owner*2` 错键 + 指针形式 any_cast（seed 446）。

**#5 getPet(-1) 越界读 ×3**（seed ~1601 段，UBSan 数组界检查）：事件载荷契约
缺口——`ev.actor/ev.target` 可为 -1（无行动方事件：效果类粉伤/反伤链），三个
watcher 未守卫范围即 `getPet()`，读 `SeerRobot[2]` 之外邻接内存拿**垃圾数据算
反弹伤害**（ASan 看不见——越界发生在对象内部，只有 UBSan 数组界检查可见）。
修法：范围守卫 + 全族审计。

**#3-系统性 抛式 any_cast 家族**：452 崩溃顺藤摸瓜发现全库 **42 文件 ~120 处**
同款取式写法（类型不符即炸）。治理：三家族 `common.h` 增加 `any_int_or` 安全
助手，机械替换全清零；上役 14 个 fault seed 复验全部归零。

### C. 数据一致性（不变量 oracle 独有发现）

**#4 效果 773 体力互换裸 swap**（seed 17026，不变量断言报 `hp=185 > max_hp=144`）：
`std::swap(self.hp, other.hp)` 无视"削减体力上限"类效果——对面高上限精灵的体力
换给上限被压低的一方就越界。同族 479"残留 N 点体力"裸写配置值同险。
排查路径：不变量的精确值（185−144=41）+ DB 技能载荷反查（19194 → 效果 [773,447]）
直接定位；`heal_impl`/嗑药/上限增减等正经通道全有钳制，问题专挑插件野路子直写。
修法：按 2496 换血先例各自钳到自身上限（官方环境堆不出该组合，对官方口径无差异）。

### D. 构建配置缺陷（崩溃但引擎无辜）

**#6 seed 88 container-overflow**：影子内存探针证明崩溃时向量状态完全合法，
但 libc++ push 未解毒——根因是 fuzz 构建变体的 `SBS_FUZZ_SANITIZERS` 放在
CMakeLists 中部，**sim_core 没被消毒**（`_LIBCPP_HAS_ASAN=0`），与消毒插件的
inline 拷贝经弱符号绑定混用，容器注解"一边毒化一边不解"。修法：消毒全局化，
seed 88 五连跑零报告。**教训：sanitizer 报告≠引擎 bug，根因必须挖到能解释全部
观察为止。**

### E. 开放中

**#7 attack_boost_grants 注解失同步**（seed 1241，布局敏感）：-O0 下向量对象
与影子注解的状态矛盾在引擎代码里构造不出来（对象 size=1 时 elem0 必未毒化），
同一配置重建即翻转。疑似 libc++ 注解层边缘问题而非引擎逻辑。夜跑每 6000 seed
持续采样，第二个样本（不同 seed/位置）即突破口。
**样本 2 已至（2026-10-06，seed 555481）**：完全同签名——
stage_simple_attack_damage 读 12 字节 cap-1 区域 offset 8（pct 成员）
container-overflow，与样本 1 仅 seed 不同。两样本锁死问题面：
只与 attack_boost_grants 的 grant/consume/stage 三点交错有关，
与具体效果/精灵无关。归档：build/night/fuzz/crash-manual-20261006-7-sample2/。

## 战役台账

| 战役 | 规模 | 结果 |
|---|---|---|
| 首役（普通 20000 + ASan 3000） | ~46000 对局 | 452 / 活锁 / any_cast 家族 |
| 终验（普通 20000，修复后） | 40000 对局 | 全绿，确定性 100% |
| 统一消毒后（ASan 3000×2 + 6000） | ~24000 对局 | getPet(-1)、#7 首曝；复验全绿 |
| 夜跑（cron 02:30，6000/夜） | 持续 | 首夜全绿 |

## 与回归的关系

- `tools/run_regression.py` 的 335 场景测试是**确定性精确断言**（守口径）；
  fuzz 是**随机扫荡**（找场景想不到的交互）。两者互补，各自都有关键发现：
  场景 092 钉死亡语义，fuzz 抓活锁——先有 fuzz 发现，才有场景钉子。
- 夜跑：`tools/night/fuzz_night.sh`（cron 02:30，专用 worktree 隔离，
  块崩=发现归档 `build/night/fuzz/crash-*/` 后继续）。

## 未来方向

- **libFuzzer 覆盖引导**：`run_one_battle(seed)` 已是独立单元，包一层
  `LLVMFuzzerTestOneInput`（字节流 → seed/决策流）即可获得覆盖率反馈，
  把探索引向稀有魂印/效果分支。
- **协议层非法输入 target**：`room.cpp` 直接从 socket 填 `m_buffer`（4×int），
  恶意客户端是真实威胁面——随机填非法操作（切死宠/PP=0 技能）验证
  `BattleFsm::operation()` 拒绝路径不越界不崩。
- **#7 追捕**：夜跑采样，出现第二个样本（不同 seed/位置）即定位突破口。
- 多进程并行与 crash 语料库沉淀。
