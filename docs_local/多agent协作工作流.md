# 多 agent 协作工作流（hub + worktree）

> 建立时间：2026-09-17。解决的核心问题：**要并行的那些文件（插件源码、场景测试、内部文档）全在 `.gitignore` 里**，
> 所以普通分支给不了任何隔离——两个 agent 在同一个工作目录改同一个未跟踪文件时，git 不会警告，谁后写谁赢。
>
> 本文件在 hub 里，不进公开仓库。

## 一、三个位置，各管什么

```
code/c++/project/
  Simulation-battle-system/   ← 原仓库：main = 公开面（只有内核）。推送 GitHub 只在这里做
  sbs-local/                  ← hub：本地协作仓（origin = 原仓库本地路径，无公开远端）
      main        = 内核，镜像原仓库 main
      main-local  = 完整工作集（内核 + 插件 + 场景测试 + 内部文档）← agent 的基线
  agents/<名字>/              ← 各 agent 的 worktree（从 hub 派生）
```

| 角色 | 位置 | 说明 |
|---|---|---|
| 公开面 | 原仓库 `main` | 只有 `include/ src/ CMakeLists.txt tools/ docs/` 等内核路径。95 个文件 |
| 协作面 | hub `main-local` | 195 个文件：内核 + `resources/`（插件）+ `test/scenario/`（71 个场景）+ `docs_local/` |
| 工作区 | `agents/<名字>/` | 每个 agent 一份，独立分支、独立 `build/`、独立插件 dylib |

只读大件（`scripts/data` 149M 数据库、`reference/` 456M 语料）**软链**到原仓库共享，不复制：
数据库是只读打开的（`OfficialDataStore::initialize` → `open_read_only`，src 里没有任何写库语句）。

## 二、日常操作

### 开一个新机制的并行工作区

```sh
cd /Users/zhuhongrui/code/c++/project/sbs-local
docs_local/tools/new-agent-worktree.sh vanish        # → agents/vanish，分支 feat/vanish
# 2026-10-24 起 worktree 区迁移：~/.agent-workspaces/workspaces/sbs-local/agents/<名>
cd ~/.agent-workspaces/workspaces/sbs-local/agents/vanish
cmake -B build && cmake --build build -j8
```

**开工先占场景号**（2026-09-26 起）：

```sh
docs_local/tools/claim-scenario.sh zhakes_4762   # 在 hub 申请下一个空号并提交占位骨架
```

占位文件落在 main-local 上（可编译的最小骨架，不挂回归），各分支合并永远不撞号；
之后在自己的 worktree 里把断言填进同名文件即可。⚠️ 新增场景/插件源文件后要跑一次
`cmake -B build`（GLOB 的新 target 需要重新 configure 才生成），之后正常 `--build`。

⚠️ 构建与运行都必须在工作区**目录内**：运行时按相对路径加载
`scripts/data/processed/seer_unity.sqlite` 和 `resources/<lib>/liblib_1.dylib`，
CWD 决定加载哪一份——这正是各 agent 互不干扰的机制本身。

### 跑场景用例

```sh
./build/sim_scenario_038_round_window_first_second     # 单个场景
```

新增场景 = 新建 `test/scenario/scenario_0NN_<名字>.cpp` 即可（CMake 是 `GLOB CONFIGURE_DEPENDS`，
不用改 CMakeLists）。**公共基架 `test/scenario/scenario_harness.h` 要冻结**，别让两个 agent 同时改。

### 提交与合并

在各自工作区里正常 `git add` / `git commit`（此时插件和测试**已经是被跟踪的**，改了什么 git 记得住）。
合并回 hub：

```sh
cd ../sbs-local
git merge feat/vanish            # 逐个 merge，冲突当场解
```

### 把内核改动发到公开面

```sh
docs_local/tools/promote-core.sh main-local "feat: 消逝机制内核支持"
```

它只把 `include src CMakeLists.txt tools docs` 提升到 hub 的 `main`，然后打印在原仓库里
review + 提交的命令。**脚本不代你推送**——推送前你自己 `git diff --cached` 看一眼。

## 三、红线

1. **绝不 `git add -f <目录>`**。原仓库的 `local` 分支就是这么把
   `resources/*/liblib_1.dylib`（607KB + 806KB，每次构建都变）提交进去的，历史白胖。要加就精确到
   `*.cpp` / `*.h`。
2. **别给 hub 加 GitHub 远端**。hub 存在的意义就是"随便提交、没有公开出口"。发布只走原仓库。
3. **别在 hub 的 `main` 上做效果开发**。`main` 是内核镜像，效果/插件工作在 `main-local` 及其派生分支。
4. **每个工作区各自 `cmake -B build`**，不要共享 `build/`：CMake cache 里是绝对路径，共享会串。
5. `main-local` 上那些未跟踪转已跟踪的文件（`resources/`、`test/`、`docs_local/`）现在由 git 管了，
   **不要再跑原仓库的 `docs_local/tools/local-snapshot.sh`**（那个机制已被本工作流取代；
   原仓库的 `local` 分支保留作历史，但不再更新）。
6. **写 shell 脚本时，变量展开不要紧跟中文**。macOS 的 `/bin/sh`（bash 3.2）在 UTF-8 环境下
   会把紧跟 `$VAR` 的多字节字符吃进变量名：

   ```sh
   NAME=vanish
   echo "A: $NAME（基线"       # ❌ 输出 "A: ��基线"——变量名被当成 NAME（基线
   echo "C: ${NAME}（基线"      # ✅ 用大括号界定
   printf 'B: %s（基线\n' "$NAME"   # ✅ 或者用 printf
   ```

   本仓库注释/输出中文密集，这个坑很容易踩（本工作流的两个脚本都踩过）。
7. **改插件后必须让 `*_deploy` 跑到，否则跑的是旧 dylib**。`resources/<lib>/liblib_1.dylib`
   是**独立自定义 target** 拷过去的（`soul_plugin_deploy` / `moves_plugin_deploy`）；
   `--target soul_plugin` 只编译链接，**不会**更新 `resources/` 里那份，而运行时按相对路径
   加载的正是 `resources/` 那份（`build/plugins/` 下的产物没有任何人加载）。
   症状极具误导：源码是新的、`build/plugins/` 下也是新的、跑出来却是旧行为。

   ```sh
   cmake --build build -j8                          # ✅ 日常一律这样（部署 target 是 ALL）
   cmake --build build -j8 --target soul_plugin      # ❌ 只编插件，resources/ 里那份会停在旧版
   cmake --build build -j8 --target soul_plugin soul_plugin_deploy   # ✅ 要单编插件就带上它
   ```

   实测（建 hub 时的隔离验证）：`--target soul_plugin` 后 `build/plugins/.../liblib_1.dylib`
   已变（`a92a9e67`）而 `resources/.../liblib_1.dylib` 仍是旧的（`33358f6d`），
   跑部署 target 后才同步。

## 四、并行排期：先冻接口，再分派

不按这个顺序，前面搭好了一样天天解冲突：

1. **内核接口先落地一个 commit**（消逝/死亡/额外精灵/位置查询/削减原语的原语声明与字段），
   效果 agent 只调不改 → 之后并行期几乎不碰同一个头文件。
2. **拆分粒度惯例（2026-10-01 用户定稿）**：**技能效果**与精灵无关 → 按家族/语义
   一文件（moves_lib 现状），不必刻意按精灵拆；**魂印**个人色彩强烈、通常按精灵
   成套一次实现 → 按精灵一文件（soul_lib 现状）。
   ~~`resources/soul_lib/lib_1.cpp` 先按家族拆~~ **✅ 已完成（2026-09-17）**：
   105KB 单体 → 10 个家族文件 + `common.h` + 15 行入口，照 `moves_lib` 先例。
   新增魂印**先选家族文件**，别再往 `lib_1.cpp` 里写：

   | 家族文件 | 内容 |
   |---|---|
   | `common.h` | 共享助手（`g_core` 是 inline 变量 = 全 .so 一份；跨家族新助手进这里） |
   | `legacy_guards.cpp` | 1001 索伦森 / 1002 兰斯洛特 / 1217 犀牛 |
   | `zha_liumeng_curse.cpp` | 2378 咤克斯 + 2386 琉梦（咒怨族） |
   | `miaoshi_4872.cpp` | 4872 妙时天女（特性复制族） |
   | `canglan_pink.cpp` | 2343 沧岚 + 2099 箫澈（粉伤时点族） |
   | `canglan_tears.cpp` | 1004/1005 沧岚泪 + 1006 朝曦 |
   | `wuxiang_2260.cpp` | 2260 无为觉者（多时点程序族） |
   | `qiling_1581.cpp` | 1581 启灵元神（神印族） |
   | `roster_field.cpp` | 2513 薇尔诗 + 903 星皇（场域常驻族） |
   | `undying_1067.cpp` | 1067 不灭·艾恩斯（免死族） |
   | `puni_1038.cpp` | 1038 圣灵谱尼七刻印 |
   | `chaos_sorensen.cpp` | **1011 混沌魔君索伦森（压制窗口族，2026-09-18 新建）** |

   待开工的三条线的落点因此互不重叠：消逝族（魂帝/空元/无为觉者）新建
   `vanish_*.cpp`、真 2 命族进 `undying_1067.cpp` 或新建、薇尔诗线进 `roster_field.cpp`。
   ~~唯一共享咽喉是 `CMakeLists.txt` 的源文件列表~~（**2026-09-26 已消除**：
   moves/soul/suit 三插件源与场景全部 GLOB 化，新增 `*.cpp` 免登记、合并零冲突）。

   拆分不变式（改完自己核一遍）：`SOUL_MARK` + `SOUL_PROGRAM` 注册总数**只增不改**
   （2026-09-18：5 + 13 = 18；拆分时为 5 + 12 = 17），且全量场景回归不得新增失败。
3. 内核热点文件 `include/fsm/battleContext.h`（60KB）、`include/primitives/battle_primitives.*`
   **同一时间只让一个 agent 动**。
4. `test/scenario/` 每个机制一个文件 → 天然无冲突；`CMakeLists.txt` 已全部 GLOB 化
   （2026-09-26），插件源与场景新增均免登记。

## 五、hub 基线里刻意排除的东西

| 排除项 | 原因 |
|---|---|
| `resources/*/*.dylib` | 构建产物，每次构建都变；已由 `*.dylib` 忽略 |
| `docs_local/test/` | 过期快照（46 个文件，落后于 `test/scenario/` 的 71 个），留着会被误编辑 |
| `scripts/` `reference/` | 只读大件，软链共享（见上） |
| `build/` `.cache/` | 生成物 |

## 六、当前基线的特殊情况

`main-local` 的基线取自原仓库**工作区真实状态**，因此包含三个**当时未提交**的内核改动
（`CMakeLists.txt` 的 moves_lib 家族拆分登记、`include/fsm/battleWorkspace.h` 的
`crit_rate_add`、`src/entities/skills.cpp` 的加算通道）——它们是进行中的 effect 32 蓄气族特性，
且**插件必须靠 CMakeLists 那个改动才能编译**，所以不能不取。

后果：hub 的 `main` 与原仓库 `main` 同为 `74b61d8`，但 `main-local` 的内核比它多这三处。
`promote-core.sh` 会把它们一并提升；如果那三处还在改，**等原仓库提交后再跑 promote**。
