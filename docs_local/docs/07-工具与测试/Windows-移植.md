# Windows 移植（引擎 + 网关全栈上 Windows）

> **2026-11-06 现状勘误（非复测）**：本文按移植过程留档，M0 待机等早期状态及 D14
> 「32 红均为平台消费序漂移」不能当成现状。后续 u64 修复后 **850/855、剩 5 红**
> 是 §8 的历史最后记录，本轮未重跑；D14 统一归因已标为历史误诊。
> 初稿状态（历史）：**M0 环境准备完成，等待 wuying 恢复可达**（见 §2 现场记录）。
> 目标：标准 Windows 环境（不绑死 wuying）能构建引擎 + 跑通 855 场景回归，最终引擎+网关全栈可跑。
> 基线：Mac main-local @ 0cd1e16c，CMake 全量门 855 场景全绿。
> 分支：`feat/windows-port`（worktree `~/code/c++/project/sbs-win`）。全程不 push、不动 main。
> 明确不移植：夜跑 bash 工具链（build.sh/claim/finish，任务书既定）。

## 1. 范围与里程碑

| 里程碑 | 内容 | 验收 |
|---|---|---|
| M0 | 环境选型 + 差距盘点 + wuying 环境脚本 + 回归门脚本 | 本文 + tools/windows/*（已完成，Mac 侧验证过 worktree 可构建） |
| M1 | Windows 引擎构建 + 855 场景回归 vs Mac 基线 | gate.ps1 全绿；差异逐条定性 |
| M2 | sim_server 上 Windows（net 层 poller + main.cpp 信号管道） | 控制台 F 组 smoke 对 Windows sim_server 跑通 |
| M3 | gateway（Python + Vue/vite）上 Windows | 浏览器全栈走通一局 |

## 2. 现场记录（2026-10-25）

### 2.1 隧道断因与修复（已闭环）

- **现象**：10-03 04:30 起隧道死三周，mx1:2222 无监听、auth.log 零重连尝试。
- **取证**：任务 XML 两个配置缺陷叠加——触发器只有 `LogonTrigger`、`LogonType=InteractiveToken`：
  任务**骑在 admin 的交互桌面会话上**。事件日志证实 10-01~10-06 **没有重启**（上次开机 6-03），
  死因是 10-03 04:30 **交互会话被注销**（时点凌晨、会话存活约 7 小时，最像无影平台对断开
  客户端的空闲会话自动注销；审计日志未开，确切 initiator 无从考证）。会话一没，循环进程
  随会话被杀，LogonTrigger 在下次登录前永不点火 → 无自愈。
- **修复（已落地并探针验证）**：`WuyingRelayTunnel` 重注册为 v2——
  `<LogonType>S4U</LogonType>` + `<BootTrigger delay=30s>`（保留 LogonTrigger，
  IgnoreNew 防双实例；StopOnIdleEnd/StopIfGoingOnBatteries 关掉；加 RestartOnFailure），
  ssh 参数补 `-o UserKnownHostsFile=<绝对路径>`（S4U 环境不保证有 USERPROFILE）。
  S4U 探针（独立任务实测）全过：admin 身份 ✓ 提权 ✓ USERPROFILE ✓ 私钥可读 ✓ 出网 mx1:22 ✓。
  **此后登录界面/重启/注销状态都能自起隧道**。注册时不杀旧实例（红线），v2 在下次开机/登录接管。
- ⚠️ 红线照旧（SERVERS.md）：不杀骑隧道的 ssh.exe、不关机、升配后调回、mx1 只读、mx2 不碰。

### 2.2 分支与验证

- 主工作树 merge（main→main-local）已由用户收敛为 141ab0e5；`feat/windows-port` 已 rebase
  到其上（34525a56），Mac 侧重建 scenario_001 绿。
- Mac 侧验证：sim_core + 三插件构建绿，scenario_001 三断言 PASS（exit 0）。

## 3. 决策记录

| # | 决策 | 理由 | 弃选 |
|---|---|---|---|
| D1 | 工具链 **MSYS2 UCRT64 MinGW-w64 GCC**（env-setup.ps1 一键装） | ① 源码是 GCC/Clang 语系 C++20，MinGW 自带 dirent.h 等，3 处无条件 `#include <dirent.h>` 零改动过编；② 全程清华镜像不碰 GitHub（本机 GitHub ~100KB/s，vcpkg/VS 组件源都疼）；③ 任意标准 Windows 复现（同学电脑同样跑 env-setup.ps1）；④ UCRT64 是 MSYS2 当前推荐运行时 | MSVC（留作后续可选：需守卫 3 处 dirent、SQLite/nlohmann 走 vcpkg 碰 GitHub、GNU 扩展逐个排查，收益仅在诊断与调试器） |
| D2 | 回归门 **PowerShell**（gate.ps1） | 标准 Windows 自带 PS 5.1+，无 shell 依赖；git-bash 方案弃（多一层 MSYS 依赖，与 D1 的 MSYS2 bash 也是两回事） | git-bash 包装 |
| D3 | 插件加载沿用现有 **LoadLibraryA** 分支 | 代码已全移植（见 §4），仓库内路径全 ASCII 相对路径，A 版无碍。W 化（含路径 UTF-8→宽字符）列为可选硬化项，不阻塞 | 首轮就换 LoadLibraryW（收益为零、改动面大） |
| D4 | `-march` 旗标 **Windows 不设** | x86-64-v2 只存在于 deploy/Dockerfile（mx2 Linux 部署管线），Mac 开发构建本就没有；Windows 构建用工具链默认 | — |
| D5 | net 层 M1 **不动**，M2 写 **wsapoll** 后端 | 场景回归只链 sim_core（不含 net）；poller 接口是 readiness 语义，wsapoll 天然贴合，IOCP 属过度设计。现 reactor.cpp 对 _WIN32 直接 `#error`，M1 构建不含这些文件即可 | M1 就写 IOCP |
| D6 | DB 与构建产物**真实拷贝，不用软链** | NTFS symlink 要特权且行为差异多；fuzz_night.sh 的 scripts 软链属夜跑工具链，不移植 | — |
| D7 | 场景回归**顺序执行**（gate.ps1 不并行跑） | 保随机序/时序可复现，对齐 Mac 基线循环；-Jobs 只作用于构建 | 并行跑场景 |
| D8 | Defender 排除做成**可选开关**（env-setup.ps1 -AddDefenderExclude） | 实时扫描显著拖慢 855 个小二进制的链接；SSH 会话自带管理员权限可直接加，但改安全策略须显式 | 默认静默加排除 |
| D9 | `.ps1` 一律 **UTF-8 带 BOM** | PowerShell 5.1 在中文系统对无 BOM 文件按 GBK 解析，中文注释变乱码直接 ParserError（2026-10-25 实测） | — |
| D10 | 代码同步用 **git archive → scp → tar 解压**（rsync 语法上不可行） | Windows 侧无 rsync（首次同步时 MSYS2 也没装）；tracked 文件即全部所需（855 场景+726 插件源+全部头文件），git archive 天然排除 build/.git/DB；7.2MB 压缩包走隧道 19s | rsync（远端无二进制；装完 MSYS2 后如需增量可 `pacman -S rsync` 配 `--rsync-path` 再评估） |
| D11 | 运维引擎 **pwsh7**：sshd `DefaultShell` 已切 `C:\Program Files\PowerShell\7\pwsh.exe`，全部 WuyingOp* 计划任务用 pwsh7；`tools/windows/*.ps1` 保持 **PS5.1 双兼容**（带 BOM、EAP=Continue） | PS7 默认 UTF-8（日志无 GBK 乱码）、原生命令 stderr 不再触发 EAP；双兼容让脚本在任何标准 Windows（只有 5.1）也能跑 | 全面转 PS7-only 语法（要保底同学电脑的裸 5.1） |
| D12 | 长任务一律走**计划任务**（WuyingOpBuildKG / WuyingOpGate / WuyingOpEnvSetup），不用 `Start-Process` 假分离 | Windows OpenSSH 在会话结束时按 job 对象清杀整棵进程树（两次静默死亡实证）；计划任务由调度器服务拉起，与 SSH 会话解耦 | 后台 nohup 类方案（Windows 无对应物） |
| D13 | 全工程 rand/srand **链接期改道**（`-Wl,--wrap`）到引擎自有 MINSTD（Apple-libc 兼容） | std::rand 序列实现定义相关，Mac 校准的场景断言依赖序列复现。曾试 `-include`+#define：被 libstdc++ `<cstdlib>` 的 `#undef rand`（cstdlib:121/123）击毙。wrap 对预处理免疫；sbs_rand.dll 进程级单状态对齐 macOS 语义；WIN32 全守卫，Mac/ Linux 零变化 | ①接受 53 红（弱）；②改调用点为自有 RNG（400+ 文件，且 Mac 序列会变，基线作废） |
| D14 | **历史误诊（2026-11-06 标注）**：当时将 32 红一概定性为平台 rand 消费序漂移并决定 M1 不修；不再作为现状结论 | 当时取证：scenario_3620 可见 battle 轨迹相同但 view 35 vs 70，抽样表现为 roll 翻转；这些现象不足以证明消费次数不同。后续 u64 修复实锤：MINSTD 的 `16807UL*g_ctx` 在 Windows 32 位 unsigned long 溢出，多数差异来自**序列实现错误**而非消费序（见 §8）。修复后历史最后记录为 **850/855、剩 5 红**，不是本轮复测；余下按各自签名调查，不可泛归平台差异 | 当时弃选：M1 内逐场景修消费序；保留原决策链，根因判断以后续取证为准 |
| D15 | RNG 终态 = **源码级替换**（sed 机械迁移 452 文件 `std::rand/srand`→`sbs_rand/sbs_srand`），wrap 与 #define 两方案全部退役 | 用户定调社区开放：任意工具链/平台必须同种子同序列。`-Wl,--wrap` 是 GNU-ld 专属（MSVC/lld-link 不支持）；#define 已被 `<cstdlib>` #undef 证死；源码级替换对预处理与链接器零假设。SHARED 库形态保留（宿主+插件单状态）。**验证协议**：Mac 门 855/855 ✓ + Windows 门红名单必须与迁移前**逐字节一致**（823+同一 32 红）——替换必须行为中性 | 保留 wrap（可工作但不满足多工具链）；改调用点为手写（452 文件，量大但机械） |
| D16 | 场景回归新增**时点状态 golden 快照**环节（当时的设计决策，后续进展见 §8） | 动机：跨平台不一致需定位首个失配时点，不能只看终态 CHECK；D14 的统一归因后来已被 u64 取证纠正。设计见 §7.1，历史运行记录见 §8 | 只加更多 CHECK 断言（治标，维护成本高） |

## 4. 盘点结论

### 已移植区（读码确认，无需动）
- `include/utils/dynamic_library.h` + `.cpp`：Windows 宏分支（LoadLibraryA/GetProcAddress/FreeLibrary）+ **FindFirstFileA 目录扫描**完整。
- `src/effects/effect.cpp`、`src/entities/soul_mark_manager.cpp`、`src/entities/suit_manager.cpp`：目录扫描与扩展名探测（.dll）Windows 分支**全部齐备**；effect.cpp 的 dirent 有 `#if !defined(_WIN32)` 守卫。
- CMakeLists：插件/场景 GLOB、`*_plugin_deploy` 拷贝 target 全平台中立；`find_package(SQLite3)`/`find_path(nlohmann)` 由 MSYS2 包满足。
- gateway（web/，Python+Vue/vite）与构建本身无平台绑定（M3 验证即可）。

### 差距清单

| 点 | 文件 | 现状 | 动作 | 里程碑 |
|---|---|---|---|---|
| 无条件 `#include <dirent.h>` ×3 | src/utils/dynamic_library.cpp:3、src/entities/soul_mark_manager.cpp:9、src/entities/suit_manager.cpp:3 | MSVC 必挂；MinGW 自带 dirent 可过 | D1 下不动；若日后上 MSVC 再加 `#if !defined(_WIN32)` 守卫 | （MSVC 备用） |
| reactor `#error` | src/net/reactor.cpp:16-27、src/net/poller_platform.cpp（仅 kqueue/epoll） | Windows 编不进 | M2 写 wsapoll Poller（接口 readiness 语义），reactor 的 inet/socket 头补 winsock 分支 | M2 |
| 自管道信号 | src/server/main.cpp（pipe/write/read + SIGINT/SIGTERM/SIGPIPE） | POSIX 专属 | M2 改 SetConsoleCtrlHandler（或 localhost socket 对当管道）；sim_server 专用，M1 不受影响 | M2 |
| training_cli 原生 socket | tools/training_cli.cpp | BSD socket + 无 winsock 初始化 | M2/M3 顺手（WSAStartup + closesocket/BOOL 尾注） | M2.5 |
| WSAStartup 全库缺失 | — | 无任何 winsock 初始化 | 随 M2 net 层一起落 | M2 |
| 插件导出符号可见性 | resources/*_lib（MinGW 默认全导出） | Mac 用默认可见性，MinGW 同 | 预计零改动；若 GetProcAddress 找不到入口再议 .def/显式导出 | M1 验证点 |
| 场景二进制体积 | build/ | Mac 单个 ~4MB、全量 build 目录 ~8GB | wuying 磁盘预留 ≥15GB；不够先升配（用完调回） | M1 |

### 需编译期实证的疑点（M1 首跑重点看）
- GNU 扩展遗漏（`__attribute__`、statement-expr 等）——MinGW GCC 大多直接过。
- C++20 特性在 MSYS2 GCC（当前 14.x）的行为差异。
- 855 场景中依赖未定义行为/平台随机源（`std::random_device` 实现）的个别案例——出现红场景按"移植 bug / 平台差异 / 测试需适配"三分类定性（任务书要求，逐条记录到 §8 运行台账）。

### 已实锤的坑（2026-10-25 编译攻坚记录，修复均已提交）
1. **Windows 宏撞车**：`DamageKind::TRUE` 撞 `windows.h` 的 `TRUE=1`（12 处→`TRUE_DMG`）；`Command::ERROR` 撞 `wingdi.h` 的 `ERROR=0`（wire 数值 33 不变→`CMD_ERROR`）。教训：grep 排查范围必须含 `tools/`（training_cli 漏过一次）。
2. **CMake 插件产物撞号**：MinGW 的 DLL 是 RUNTIME 产物且带 `lib` 前缀（`lib_1` → `liblib_1.dll`），`LIBRARY_OUTPUT_DIRECTORY`（macOS 的 dylib 走它）在 DLL 平台**不生效**；三插件全堆 build 根 → ninja "multiple rules generate"。补 RUNTIME/ARCHIVE_OUTPUT_DIRECTORY 各归各家。注意 import library（`.dll.a`）走 ARCHIVE。
3. **ninja 重复输出是解析期报错**：烂 build.ninja 活不到"重新生成"那步（鸡生蛋）——改了 CMakeLists 必须删 build 目录。
4. **隐式传递包含**：libc++（Mac）会传递提供 `uint32_t` 等，libstdc++（MinGW）不传递——全仓补 `<cstdint>`/`<cstring>` 直接包含（13 文件）。
5. **POSIX 专属 API**：`arpa/inet.h`+`ntohl`（pet_factory，手写 be32_to_host 内联化）、`localtime_r`（battleFsm，Windows CRT 的 `localtime_s` **参数序相反**）、net 层全家桶（fcntl/getnameinfo/kqueue 等，M2 wsapoll 后端解决，M1 用 `if(NOT WIN32)` 守卫不编 net/server/training_cli）。
6. **第 4 处宏撞车 + 场景侧 POSIX**：场景助手函数 `near()` 撞 windef.h 的 `near` 宏（5910/7310/7320 → `near_val`）；两个场景用 `htonl` 造测试载荷（136/172 → 手写 host_to_be32）；`std::fabs` 缺 `<cmath>`（150）。
7. **git archive 同步=全量重建**：archive 把所有文件 mtime 重置为 commit 时间，tar 解开后 ninja 视为全变。小修走 scp 单文件，只有大改动才走 archive（接受一次全重建）。
8. **M2 验收脚本长输出接 `| head` = 整链背压卡死**（2026-10-25 实测，表现成"对局卡死"假象）：`ssh wuying "pwsh 跑对局脚本" | head -120`——head 读满 120 行退出后管道破裂，ssh→pwsh(Write-Host)→子进程 stdout 管道→training_cli→sim_server 逐级阻塞，服务器实际早已 `battle finished (normal)`，帧全堵在发送缓冲。**长任务输出一律落远端文件再读，绝不在 ssh 管道接 head**。修后（cli stdout 重定向文件）同一对局一次通过。
9. **读 exe 的 UTF-8 中文输出**：pwsh 按 `[Console]::OutputEncoding`（中文系统=GBK）解码重定向输出，UTF-8 中文全乱码。脚本开头 `[Console]::OutputEncoding = [Text.Encoding]::UTF8`（观感问题，不影响二进制协议帧）。
10. **git archive 不含被 ignore 的目录骨架**：`scripts/data/processed/`（只放 DB）不入库，新目标树 tar 解完没有该目录，`Copy-Item` 直接报错——先 `New-Item -Force` 再拷 DB/插件 DLL。
11. **winsock 头下限**：`WSAPoll/WSAPOLLFD/inet_pton` 只在 `_WIN32_WINNT >= 0x0600` 声明——三处（poller_platform/reactor/training_cli）include winsock2.h 前显式 `#ifndef _WIN32_WINNT #define 0x0600`。MinGW 的 `socklen_t`/`NI_MAXHOST` 在 ws2tcpip.h 齐备无需自备；`setsockopt` 第 4 参统一按 `const char*` 转发（POSIX `void*` 兼容）；`INADDR_LOOPBACK` 部分头没有，回环地址直接写 `htonl(0x7F000001)`。
12. **Windows 无 pipe() 且 WSAPoll 只吃 SOCKET**：Reactor 唤醒通道用回环 TCP 互联 socket 对等价（临时监听 127.0.0.1:0 → connect+accept → 关监听），两端必须关 Nagle（TCP_NODELAY）否则唤醒字节被 Nagle+延迟 ACK 拖住。
13. **验证脚本禁用 `;` 串联多步**（2026-10-25 重犯教训）：`build ; test` 里 build 失败后 test 照跑旧二进制，结果"全绿"是假的——多步验证一律 `&&` 链或独立命令逐个看退出码；同族坑 8（管道背压）。

## 5. 环境搭建与代码同步（wuying 恢复后照此执行）

```powershell
# 1) 环境（首次，管理员 PowerShell）
powershell -ExecutionPolicy Bypass -File tools\windows\env-setup.ps1 -AddDefenderExclude

# 2) configure 预检（不构建）
powershell -ExecutionPolicy Bypass -File tools\windows\gate.ps1 -ConfigureOnly
```

Mac → wuying 代码同步（在 sbs-win worktree 根执行；D10：git archive 只含 tracked 文件）：

```bash
# 1) 打 tracked 全集（855 场景 + resources 插件源 + 头文件；天然排除 build/.git/DB）
git archive feat/windows-port --format=tar.gz -o /tmp/sbs-win.tar.gz
# 2) 传 + 解（远端目标 C:\Users\admin\sbs-win）
ssh wuying 'New-Item -ItemType Directory -Force C:\Users\admin\sbs-win | Out-Null'
scp /tmp/sbs-win.tar.gz wuying:sbs-win.tar.gz
ssh wuying 'tar -xzf C:/Users/admin/sbs-win.tar.gz -C C:/Users/admin/sbs-win'
# 3) DB 单独传（36MB，gitignore 不随树走；2026-10-25 已传，字节数核对一致）
scp scripts/data/processed/seer_unity.sqlite wuying:sbs-win/scripts/data/processed/
```

注意：
- **远端默认 shell 是 PowerShell**——ssh 命令里的 `&` 链、cmd 语法不可用，用 `;`。
- `.git` 不同步（bundle 方案备而不用：全量 history ~626MB；Windows 侧暂不落提交）。
- 传大文件走隧道 ~400KB/s（29MB→19s、36MB→89s 实测），wuying 本机下载走自己网络（清华镜像不限于此）。

## 6. 回归对比方法（M1 验收口径）

- Windows：`gate.ps1` 全量 → 与 Mac 基线（855）对比。
- **M1 早期实测记录（2026-10-25；后续 u64 修复与最后记录见本节末和 §8）**：
  - Mac 基线（feat/windows-port 全部改动后复验）：**855/855 全绿**。
  - Windows 首轮 gate：802/855（53 红，全部 0xC0000409 abort-on-CHECK，单一根因 = std::rand 序列实现相关）。
  - D13 wrap 落地后：**823/855**——21 个序列敏感场景翻绿（scenario_760 实锤修复：30 断言全过）。
  - 当时剩余 32 红被归为「rand 消费序漂移」（**D14 历史误诊**）：scenario_3620 两侧
    battle 日志逐行一致但 power_view 35 vs 70，抽样 044/080/4960 出现 roll 翻转。
    这些是当时的观察，不足以排除 RNG 实现缺陷；后续 u64 修复已推翻统一归因。
  - 红名单存档：wuying `build\gate_failures.txt`。
- **后续取证 / 历史最后记录**：§8 的 u64 修复将门从 823 提到 **850/855（剩 5 红）**，
  实锤 MINSTD 乘法位宽溢出造成序列错误；不能再称 32 红全是消费序漂移。剩余 5 红及
  部分绿场景的容差内漂移按 §8 签名继续定位，M1.5 的确定性加固规划不等于已实现。
  这些均是历史记录，**2026-11-06 文档勘误没有重新构建或复测 Windows/Mac**。
- 红场景处理：`build\gate_failures.txt` 逐条 → 单场景两侧各跑对比输出 → 定性三分类 → 结论追加 §8 台账。
- Mac 侧同基线复跑：`cmake --build build -j8 && for bin in build/sim_scenario_*; do ./$bin || FAIL; done`（常识速查 §四 的既定口径）。

## 7. M1.5 规划（本轮定稿，未实现）

### 7.1 D16：时点状态 golden 快照（诊断利器 + 回归安全网）

动机（历史规划）：当时 32 红暴露了跨平台不一致；D14 的统一根因判断后来已被 u64
取证纠正。"绿≠无漂移"仍成立——现有场景只断言采样终态，过程中间的细节漂移不可见。
快照机制让每次漂移**直接定位到第一个失配时点**，把"消费序漂移 hunting"变成可二分的证据。

设计要点：
1. **挂点**：battleFsm 的时点通知处（DEBUG 日志枚举的那些阶段）——每个时点结束序列化一次。
2. **序列化**：sim_core 内新增规范化 state digester（battleContext+workspace 的全量字段）。
   铁律：键排序输出、浮点定格式（%a 或定宽）、排除指针/时间戳/平台相关值——否则快照自身
   跨平台不幂等，满屏假阳性。**复用既有 fullstate JSON 思路**（web 侧已有取证格式），不另造方言。
3. **产出**：默认 hash 链（`时点名:hash` 逐行，855 个小文件存 `test/scenario/golden/`，随场景入库）；
   环境变量 `SBS_SCENARIO_TRACE_FULL=1` 时落全量 JSON 供 diff。默认关闭零开销。
4. **消费**：gate.ps1 与 Mac 循环跑场景时逐时点比对 hash；首个失配时点即漂移点，
   配合 FULL dump 两侧 diff 直接看到第一个分叉字段。
5. **维护**：引擎有意变更 → 重录 golden（与现有场景校准工作流同构，taskctl 口径不变）。

预估：digester 1-2 天 + gate 接线半天。**跑训练数据（M4）前置依赖**：训练数据的可复现性
等价于"任意两台机器同种子逐时点一致"，golden 机制就是它的验收器。

### 7.2 确定性后续规划（不能沿用「32 红全是消费序」前提）

golden 快照用于逐场景定位首个漂移时点，再按实际证据区分 RNG 序列实现、消费序、
容器/效果序、FP 分支或其它输入差异，不能先验归因。§8 已记录 u64 修复后的剩余 5 红
及盲区签名，这是历史最后记录、不是本轮复测。后续规划的验收目标仍为 Windows 门
855/855 **且** golden hash 链两侧全等；本轮未实施或验收这些目标。

## 8. 运行台账（M1 起追加）

| 日期 | 事件 | 结论 |
|---|---|---|
| 2026-10-25 | M0：盘点+脚本+文档落盘；Mac worktree 验证绿；wuying 隧道断（10-03 起），等无影客户端登录恢复 | M0 完成，M1 待机 |
| 2026-10-25 | 用户登录拉起隧道；断因取证（会话注销非重启）；任务 v2（S4U+BootTrigger）注册+探针验证；分支 rebase 到 141ab0e5；代码+DB 全量同步上 wuying（855 场景/418 moves 源/36MB DB 核对一致）；env-setup 启动 | M1 环境安装中 |
| 2026-10-25 | 环境就绪（MSYS2 UCRT64 GCC16.2/CMake4.4/Ninja）；pwsh7 迁移（DefaultShell+launcher）；四批编译修复（TRUE/ERROR/near 宏、缺头、POSIX API、CMake 归位、net/fuzz 守卫）→ 全量构建 855 exe + 3 插件 DLL 全绿；首轮 gate **802/855**（53 红 = std::rand 序列，单根因）；Mac 基线复验 855/855 | M1 构建达成，回归定性中 |
| 2026-10-25 | D13 终版（-Wl,--wrap 链接期改道，#define 方案被 cstdlib #undef 击毙）→ gate 复跑 **823/855**；当时将剩余 32 红统归 rand 消费序漂移（D14，**后续 u64 取证已纠正的历史误诊**）；当时 M1 收口 | 当时结论：可构建+可运行+823 绿、32 红统一定性；该归因以后续取证为准，历史最后记录见下方 850/855 |
| 2026-10-25 | D15 源码级 sed 迁移（452 文件，wrap 退役）：Mac 门 **855/855** 零变化；wuying 验证门 **823/855 且红名单与 wrap 后基线逐字节一致**（行为中性验证协议通过） | D15 落地 |
| 2026-10-25 | **M2 代码落地**（feat/windows-net 分支，worktree sbs-win-net）：①poller_platform.cpp 新增 WSAPoll 第三分支（D5 落地，映像表即注册表，无变更队列）；②reactor.cpp 去 `#error`——winsock2 头分支+WSAStartup(call_once)+socket API 差异封装（SOCKET↔int、WSAGetLastError/kSockErr*、ioctlsocket(FIONBIO)、回环 socket 对唤醒通道）；③main.cpp Windows 停机=SetConsoleCtrlHandler+手动重置事件（SIGPIPE 兜底不移植）；④training_cli winsock 化（WSAStartup/inet_pton/closesocket）；⑤CMake 解锁 sim_net/sim_server/sim_training_cli（sim_net 链 ws2_32），两个 POSIX 测试目标维持 NOT WIN32。Mac 侧 sim_net_test+e2e 复验全绿后提交（6 commits） | M2 代码就绪 |
| 2026-10-25 | **M2 验收**：A=WuyingOpNetBuild（克隆 WuyingOpBuildKG XML 改路径/目标）编译 sim_net+sim_server+sim_training_cli 全绿（LastTaskResult=0，exe+libsbs_rand.dll 落地，目标树 C:\Users\admin\sbs-win-net 与主线程完全隔离）；B=sim_server.exe 起服 127.0.0.1:4399（后端=wsapoll），PowerShell TcpClient 发 HELLO(36) 帧收到 JSON 指纹 `{"protoVersion":1,"registryHash":"9cda0a42e25092b7","stateSchemaHash":"39936744ff540485"}`；C=training_cli auto solo 完整对局 9 回合 → BATTLE_OVER(winner=0)，会话/对局干净回收，期间 5s 间隔 HELLO 探针持续应答；D=Mac 侧 worktree 全量 gate **855/855** | **M2 完成（A/B/C/D 全过）**；验收 harness 卡死一例系脚本自身 `\| head` 管道背压（坑 8），非移植缺陷 |
| 2026-10-25 | 发现（非本次引入，上游既有）：Reactor 的 wakeup_pipe **从未注册进 poller**（POSIX/Windows 同样），跨线程 post 的唤醒字节无人消费——靠 service 的 1s idle sweep 定时器兜底（wait() 每 ≤1s 返回一次 drain_posted），跨线程写延迟上限 1s。Windows 移植保持对等未动；建议列 M1.5/确定性加固时一并修（poller 注册 wakeup fd 或改 eventfd 式唤醒） | 遗留候选 |
| 2026-10-25 | **feat/windows-net 并回 feat/windows-port**（1e967dd2，文档冲突双保留）+ **wakeup_pipe 注册修复**（64e9240a，构造时 control(kAdd)，三 poller 后端通用；顺手修 sbs_rand target 缺 include 目录）。验证：Mac 严格链（&&）BUILD/NET_TEST/E2E_OK + 855/855；wuying gate **823/855**（红名单不变）；wuying HELLO 冒烟过（wsapoll 起服 + 帧应答 + 会话干净回收） | 上游既有延迟 bug 修复（跨线程投递 1s 上限→立即唤醒）；M2 正式收编主线 |
| 2026-10-25 | **u64 修复**（64c…：MINSTD 乘法 16807UL*g_ctx 在 Windows 32 位 unsigned long 溢出，序列悄悄变质——D16 快照 610 字段唯 rand 不同当场抓获；坑 14：跨平台位宽敏感算术一律 uint64_t）→ gate **823→850/855**（27 红翻绿）；845 链对比 MATCH=815 | 快照机首战立功：抓住实现级位宽 bug + 修正 D14 的"消费序漂移"误诊（多数差异是序列错误非调用次数） |
| 2026-10-25 | **剩余 5 红漂移签名**（FULL 全量文本字段级 diff，前时点均零差异）：2460=Windows 多触发 2500 回血（lastHeal）；2520=Windows 无异常+对方全能力-6 vs Mac 有异常不掉能力且 rand 消费 1v2；2550=Windows 多跑 500 粉伤+回血+firstExhaust；2570=纯 rand 消费 3v4（先制判定路径）；179=链已全等仍红→断言读 v1 盲区（std::any 存储/效果注册序）。另 25 条绿场景链有容差内漂移 | 同状态不同分支→条件输入在 v1 盲区（std::any 存储/桶内效果序）——M1.5 下轮靶心：盲区入链 + 按签名读效果实现逐类修 |
| 2026-10-25 | 用户升配 8C/16G（变配重启，隧道 v2 自动重连实证）；sbs-win 目录 845 链对比完成；**wuying 已按用户指示关机**（下次需无影客户端开机；升配档记得调回 4C/8G） | M1.5 下一轮（盲区入链 + 按签名修效果）需要时再开机 |
| 2026-11-21 | **全量复测（M1/M2 现状刷新）**：sbs-win-net 擦树重建（archive 同步 main-local@74c8ba1f + DB 13a802c7，**两侧同 DB 口径**；Mac 侧本地门同步全量 1018/1018 绿作基准；场景数已从 982 涨到 1018）；WuyingOpGateNet982 计划任务跑 gate.ps1 全量：2918 步构建约 70 分钟（8C/16G）→ **PASS 1012 / FAIL 6 / 共 1018**，红全为 0xC0000409 abort-on-CHECK；D16 链对拍（fresh vs fresh，1006 链）MATCH=978 / DIFF=28 = 6 终态红 + 22 绿漂移 | Windows 门现状落定；红与漂移清单定性见 §9 |
| 2026-11-21 | **M3 验收（引擎+网关全栈上 Windows）**：web/up.ps1（编译+registry dump+网关+开浏览器；Python 三级回退 venv→py/python→UCRT64 pacman python-aiohttp；杀旧走 CIM 按 CommandLine 匹配）在 wuying 实测全链绿：pacman 自动装 aiohttp 依赖树 → 双服务拉起（sim_server 4399 + 网关 8070）→ HTTP 200 / health ok / 4399 可达 → 云电脑桌面浏览器打开调试台 | **M3 完成**；M1~M3 全栈收口 |
| 2026-11-21 | **CI 三平台门**：gate.yml 加 windows-nightly（windows-latest + MSYS2 UCRT64，抽样 102 场景）；四轮迭代实录：① 公开仓文件名含 `?`（idx059_gelaoshi?_59.md）→ windows checkout 直接 128，改名修复；② db 镜像 HEAD 探测被 GitHub API 未认证限流（curl -sf 静默吞 403 → python 空输入炸）→ 带 runner token；③ 镜像自带 C:\mingw64 抢编译器 + GITHUB_PATH 前置实测不生效 + setup-msys2 默认 release:true 把 MSYS2 装进 runner temp（根不可钉）→ release:false/update:true 用镜像 C:\msys64 + 工具链/SQLite3 全显式绝对路径 + 动态定位兜底；④ 抽样踩红 115_medicine_system（CI-Windows/Linux 红、wuying/Mac 绿——跨环境敏感）→ known-failures 兜底，集合外新红才触发哨兵。同日 Ubuntu CI 全量首跑 **11 红平台差异**（§9）；smoke 首跑红 = 撞号治理漏网（1006→9901006 后 test.cpp 还拿旧 id，getEffectFunc 未命中返回 noop 非空断言拦不住）→ 已修 + 夜跑门补 smoke（本地门与 CI 同面） | CI 门就位；windows job 按 §9 known-failures 收缩 |

## 9. 平台差异清单（2026-11-21 三平台全量对拍）

口径：同一 commit（main-local@74c8ba1f 前身，含 smoke 修复）+ **同一 DB**（13a802c7，
两侧核对一致）。Mac 基线 = 本地门 1018/1018 绿；Windows = wuying gate.ps1 全量；
Linux = ubuntu-24.04 CI 全量（OSS 线 2026-11-21 首次跑到全量）。golden 链对拍用
**两侧 fresh 产出**，不与入库 845 链比（引擎一个月变更会满屏假差异）。

### 终态红

| 平台 | 红数 | 场景 |
|---|---|---|
| Mac（arm64，基准） | 0 | — |
| Windows（wuying MinGW UCRT64 GCC16.2） | 6 | 2460_lowdmg_gate3 / 2520_window_roll_anomaly / 2550_untouched_gate / 2570_window_drain（§8 历史 4 红，签名未变）/ **7931_selection_timing / 8510_k464（新增）**；历史第 5 红 179 已绿 |
| Linux CI（ubuntu-24.04） | 11 | **快照缺失类 4（§9.2 决定性实验，非平台差）**：025 / 115 / 5660 / 6150；**平台/工具链候选 7（§9.3）**：009 / 010 / 038 / 074 / 136 / 172 / 874（全名见小节） |

- **两平台红名单零交集**（Linux 红的 Windows 全绿、Windows 红的 Linux 全绿）——
  不是单一平台偏差，而是各平台各有一组 rand 消费序/条件盲区敏感面。
- 115_medicine_system 在 Linux 与 **CI-Windows**（windows-latest 的 ucrt64 当前 GCC）
  红但在 wuying 绿——同一"Windows"面内还存在**编译器小版本/DB 环境差**敏感，
  CI 与 wuying 不能互相当作同一环境（DB 也不同：CI=镜像昨日缓存，wuying=本地今晨）。
- 全部终态红都是 abort-on-CHECK 断言（Windows 0xC0000409 / Linux SIGABRT），无挂死。

### 绿但漂移（D16 链 DIFF，断言容差内）

- Mac vs Windows：978/1006 链逐字节一致；DIFF=28 = 6 终态红 + **22 绿漂移**。
- 抽样签名：2230_hit_roll_fallback 首失配在 First Attack Damage（命中 roll）；
  320_qiaote_4186 首失配在 After Defeating Opponent（击杀后处理）——与 §8
  rand 消费序/条件盲区签名同族。定性仍按三分类（移植 bug / 平台差异 / 测试需适配），
  M1.5 盲区入链后逐个归因；**不能先验归因**（D14 教训）。

### CI known-failures 兜底（gate.yml windows-nightly）

抽样门 `$knownRed` 集合 = {115} ∪ wuying-6；集合外新红才把 job 打红（哨兵不失灵）。
**只减不增**：M1.5 每修一个就从集合摘一个，摘前先在 wuying/CI 复测确认绿。

### 9.1 快照机制现状确认（2026-11-21 口径对齐）

- **每一步 dump？** 是：`BattleFsm::log()`（battleFsm.cpp:1473）是全部 FSM 时点日志唯一出口，
  每条调一次 `fsm::snapshot_record`——把规范化状态（rand 状态 + FSM 标量 + workspace 全字段 +
  DamageSnapshot/粉伤 + 规则票 + 双方六宠必变状态）按**固定键序**序列化后 FNV-1a 哈希，
  追加 `<hash16> <phase>` 进链文件。仅设 `SBS_SCENARIO_TRACE` 时启用（平时零开销不落盘）；
  `SBS_SCENARIO_TRACE_FULL` 另落人读全量文本。
- **盲区（v1 头注释明示）**：std::any 三兄弟（soulmark/plugin/on_stage_storage）、pet.marks 明细、
  anomaly_view_layers、attack_boost_grants、timed bucket 效果体——**链相同 ≠ 盲区外相同**。
- **「全绿」是唯一现行硬门**（exit code）；**「链逐字节相同」尚未接线为自动门**——
  只有手工对拍工具（tools/windows/compare_golden.sh / 对拍脚本），入库 845 条 golden 是
  10-25 陈旧基线，无人比对。接线方案（待实施）：① Mac 当前 HEAD 重录 1006 条入库存替 845；
  ② gate.ps1 加 -CompareGolden、Mac 门/CI 抽样接逐字节比对，DIFF>0 即红；
  ③ 现 22 条绿漂移会立刻打红 wuying 门——作为 M1.5 工作清单逐个归因摘除。

### 9.2 快照缺失类（决定性实验，2026-11-21）

- 背景：battle_items 官方快照（`data/raw/official/Battleitem_reconstructed_pre_k450.json`，
  19,799 字节手工重建）**不入库**；本地/wuying import 时消费它（battle_items 91 行），
  CI 侧 import 走 [WARN] 降级跳过（import_seer_unity_sqlite.py:1090）→ **CI DB 药剂表为空**。
- 决定性实验：Mac 本地建沙盒（DB 副本 DELETE FROM battle_items）复跑 11 个 Linux 红场景
  → **025/115/5660/6150 四个在 Mac+空表也红（134）**——它们是 CI 环境缺失，不是平台差异
  （115 在 CI-Windows 同红与此完全吻合：CI 两平台 DB 同为空表）。
- 修复选项（待用户拍板）：快照 base64（~26KB）塞 GitHub Actions variable/secret，
  db job 解码落地后再 import——红线不破（secret 非仓库内容），CI DB 与本地对齐，
  四红消失、knownRed 收缩；不修则这 4 个要在两平台 CI knownRed 里挂名。

### 9.3 平台/工具链差候选（7 个，待逐个归因）

009 / 010 / 038 / 074 / 136 / 172 / 874：Linux CI 红、Mac 绿、wuying 绿。
注意 CI 与 wuying 都是 libstdc++（MinGW 同源），容器序相同 → 差异面缩到：
GCC 版本（CI 最新 vs wuying 16.2 钉版）/ glibc vs UCRT / x86-64 与 arm64 的编译器 UB 利用法。
M1.5 归因靶心，不许先验定性（D14 教训）。

### 9.4 静态扫描结论（2026-11-21，跨平台位宽/序类）

| 类 | 结果 |
|---|---|
| `unsigned long`/`long long` 算术 | 引擎侧已全量显式 `long long`（battle_primitives/shield_bank 均有收窄前置提升）；sbs_rand u64 修复无复发 |
| 超 INT_MAX 常量 | **实锤 2 文件**：j_kill_boost_transfer（9911253100/9911253200 超 int，实际生效键=mod 2^32 截断值）+ tianqi（~0ULL→int）——**已修**（9a320cdf：显式落截断值/等值 -1，6400 复绿、-Woverflow 清零）；leichen_runtime.h 系 1.02e9 在范围内（虚惊） |
| `long` 单独使用 | official_data_repository.cpp:72 解析累加器 → 已改 long long（坑 14 卫生） |
| `size_t`→`int` 收窄 | 仅 yanshou_4875.cpp:229（skills.size()≤6 有界，卫生级不动）；`.size()-1` 下溢 0 命中 |
| `char` 符号性（arm64 无符号 vs x86 有符号） | 0 命中（解析路径均显式 unsigned char） |
| `long double`（x86-64 Linux 80 位 vs 其他 64 位） | 0 使用 |
| 容器迭代序（libc++ vs libstdc++） | 状态路径干净：RuleCenter=vector（插入序）、event_center 投递走 by_type_ 的 vector + id find（注册序）、cleanup 逐元素独立判定序无关；breakpoints 仅 membership |
| `std::rand` 残留 | 0（仅注释提及 srand 口径） |
| 快照浮点 `%.6g/%.9g` | lib stdio 的 %g 舍入无跨平台强制一致，但 978/1006 链一致已实证 libc↔UCRT 现值域安全；硬化建议：改打 IEEE 位模式（%08x），链基线重录时一并落 |
| 快照 FNV 常量 | 现为 `std::size_t`（Win64 亦 64 位无实险），建议显式 uint64_t（坑 14），随基线重录落 |


### 9.5 链逐字节硬门（2026-11-21 接线落地）

- **基线重录**：test/scenario/golden 845→1006（main-local@9a320cdf 全量 1018/1018 绿的
  Mac 产链；683 条与旧基线逐字节相同——一个月效果新增只影响各自场景，基线大部分本就稳；
  12 个早退型场景不产链）。
- **Mac 本地门（tools/night/build.sh）**：场景循环随跑落链 + 同平台**零容忍**逐字节比对
  （GOLDEN_DIFF/NO_BASELINE/NO_CHAIN）。Mac 与基线同平台同种子，不一致只可能是
  引擎变更未重录或非确定性 bug——无清单、无豁免。
- **wuying gate.ps1**：`-CompareGolden` 默认开；预期平台漂移清单
  `tools/windows/golden_expected_diff.txt`（28 = 6 终态红 + 22 绿漂移）**只减不增**，
  清单外 diff / NO_BASELINE / NO_CHAIN 打红。
- **CI windows 抽样**：链落盘 + 同清单比对（SHA256 等价逐字节）；ubuntu 全量暂不接
  （7 平台候选未归因，接了即常红噪声）。
- **治理映射（AGENTS.md §2 分层）**：效果纯新增 PR = 场景+链成对提交（NO_BASELINE 门）；
  效果改口径 = needs-ruling（改既有场景期望/golden 同一裁决口径）；内核变动 =
  kernel-approved + **全量 golden 重录随 PR**（机械 diff 可审）。build.sh 属
  tools/night 灰名单，公开仓不含——公开面（gate.ps1/CI）走预期清单口径。

### 9.6 dump 取证通道（dump.yml）

`gh workflow run dump -f scenarios="scenario_009_... ..."` → ubuntu 构建指定场景 +
`SBS_SCENARIO_TRACE_FULL` 全量文本 → artifact `full-dumps`。与 Mac 侧同场景 FULL dump
做块对齐字段 diff，首个分叉字段即漂移点。⚠️ 快照开关只认 `SBS_SCENARIO_TRACE`
（state_snapshot.cpp:116），单设 FULL 不产文件——dump.yml 已按双变量落。

### 9.7 平台候选首例签字：009_combo_syntax_parser（2026-11-21）

- FULL dump 块对齐 diff：前 10 块（0..9）逐字段一致、`rand=1` 两端全同——**RNG 序列排除**；
  首个分叉块 #10（First Attack Damage）仅两字段：`ws.powView[0].value` Mac=170 / Linux=0、
  `ws.powView[0].rw` Mac=1 / Linux=0——**改写在 Linux 未发生**，其余 600+ 字段全同。
- 条件输入全部排除：roundChoice/skills(pp,id)/velem/pet elem 逐字段同——非 RNG、
  非容器序、非位宽、非快照盲区（velem 在链内）。
- 179 效果有**双实现史**（j_cond_power_rewrite.cpp 头注自证：解析器内建 SameElement
  pet-vs-pet 降级近似 vs 插件 clone_effect 缓存取代 + custom_effect_programs 行短路）。
  签字：**分发/装载层分歧嫌疑**（哪条路径被调用在 Linux 上不同）——下轮插桩实证
  （打印 179 的 dispatcher 解析目标即可钉死）。剩余 6 候选同法取证。

### 9.8 CI windows 链比对降级 report-only（2026-11-21 终版口径）

首秀实证：exit 码全绿（含快照通道生效后无已知红）而 **98/102 抽样链 vs 基线漂移**——
CI 的 setup-msys2 `update:true` 拉滚动最新 MinGW，与 wuying 钉版 GCC 16.2 之间是
**编译器版本的系统性漂移面**（非个别场景）。据此定口径：
- 链逐字节契约绑定「引擎 + **钉住工具链**」：Mac（零容忍）+ wuying（预期清单）。
- CI windows job = exit 码哨兵（knownRed 兜底）+ 链比对 **report-only**（不阻断），
  漂移清单进 step summary 供人看趋势。
- 顺带的治理信号：`update:true` 的滚动 MinGW 本身就是一个不受控变量（工具链治理
  候选：CI 侧钉 GCC 版本，或改 update:false + 显式包版本）。

### 9.9 平台候选签字补全（dump 块对齐，2026-11-21）

| 场景 | 签名 | 归类 |
|---|---|---|
| 009 / 010 | 前 10 块逐字段全同、rand=1 全同；块 #10（First Attack Damage）仅 `powView[0].value/rw` 两字段分叉（Mac=170/1，Linux=0/0）——**179 条件威力改写未发生** | 同根：分发/装载层（179 双实现史） |
| 038 | 前 39 块全同后死于 `meta->window == EffectWindowKind::NextRounds` | EffectMeta 元数据装载/解析分歧 |
| 074 | 前 184 块全同后死于 `power1=0 vs power0+50` | 威力加成未生效（同改写族嫌疑） |
| 874 | `got_atk=330 vs panel[0]*110/100=363`（Linux 面板基值 330 vs Mac 300 推断） | 面板合成/百分比分歧 |
| 136 / 172 | Linux 初始化期死（无任何 dump 块）——party/pet_factory 构建期差异 | 初始化期（待插桩） |

主题收敛：**全部 7 例都不在 RNG/容器序/位宽类**（rand 逐字段全同），集中指向
效果元数据装载、分发路径、面板合成三个面——M1.5 插桩靶心。

### 9.10 2026-11-22 追记：7 候选改判 + 数据时效类 + CI 首次双绿

- **009/010/038/074/874/136/172 全部改判为环境差（非平台差）**，两个机制：
  1. **custom 表缺失**（§9.2 同构放大）：z9 patch（原 c1，改名避让 apply_all 字母序）
     固化五表 93 行 → 009/010/038/074/874 绿。教训：apply_all 目录字母序重放，
     建表 patch（j3/j4/r1/x2）之前的固化 patch 会 no such table。
  2. **官方数据时效**（镜像滚动）：本地 DB 落后镜像 13 张官方表（monsters +12 等），
     场景断言钉旧数据 → 刷新后漂。**称号 id 第三次后移**（433→434，K450 后再犯）：
     repository 新增 find_title_id_by_name，136/172 改按名锚定（K450 注释里的意图
     这次真正落地）；172 的 JSON fixture 也动态拼 id。
  - 沙盒决定性实验 + CI 终验：**ubuntu 1018 全量首次全绿**，windows 同 run 双绿
    （run 44935138749）。
- **链门数据刷新触发器（§9.5 补充）**：官方镜像滚动 → 数据敏感场景链全漂
  （本次 1006/1006、MATCH=0，形态=对照局/额外段时点如实记录，exit 全绿）→
  重录基线（44bbba5b）。重录前审查口径：diff 首个分叉点为"时点扩展型"即合法
  （隔离实验：旧 DB 拷贝 diff=0，实证变化全部来自数据）。
- **管线解释器绑定**：python3 已滚到 3.15（无 solaris），管线用 **python3.14**
  跑（decode/import）——绑定教训记档，防再踩。
- debug 分支 debug/m15-179（179 分发插桩，SBS_DEBUG_DISPATCH 门控）留档未合并；
  009/010 最终根因（custom_prog 短路插件 + 场景构造使新口径恒 FALSE）以沙盒
  实验与 CI 验证闭环，插桩分支可按需复用。
- 当前真·平台差存量 = **wuying 6 红**（§8 签名族，MinGW GCC16.2 vs arm64 clang）。

### 9.11 2026-11-22 终章：wuying 6 红根因修复（方案 B，用户拍板内核仲裁）

- **根因实锤链**：① wuying 6 红场景 FULL dump 与 Mac 块对齐——首分叉字段均为
  "某效果在单侧触发/不触发"（2460=949 低伤回满、2520=全能力-6、2550=粉伤 500、
  8510=击杀/留1血、7931=先制等级 3v0、2570=rand 消费次数分叉）；
  ② 注册表对账（--dump-registry-exit 双侧 diff）：moves/suit 全量一致、
  **soul 2553 仅 Mac 有**（动态库注册侧实锤）；③ 全量扫描：**15+ 对 SKILL_EFFECT
  双注册**（949/929/91/903/898/881/868/853/851/846/844/840/837/819/760/72/655/179…，
  备案"死代码/实验遗留"模式），last-wins 下生效者 = **静态初始化序**——
  MinGW .ctors 逆序 vs Mach-O/ELF .init_array 正序 → Windows 生效旧实现、
  Mac/Linux 生效新实现。
- **wuying 禁用实验**（gates2 版 949 注销重编译）：失败点从 T2460 前移至 T2461——
  生效实现切换实锤；连带暴露 2461.a 的深层分叉（R3 攻击伤害数值差 → 949 回满
  误触发）。
- **修复（48c1c6af，内核三注册表）**：撞号按「basename+全路径」稳定字典序仲裁
  （basename 跨平台无关，方向固定 key 大者胜）；fn=nullptr = 显式运行期注销，
  注销态条目接受任何非空接管（7921 MissingMoves1470 注销/恢复双向豁免）。
- **验证闭环**：全量门 1019 全绿（basename 大者恰为全部备案对现役实现，翻转面
  为零）→ oss-sync → wuying 增量重建 → **6 红场景 exit 全绿 + 链与 Mac 基线
  MATCH=6 DIFF=0（逐字节）** → CI dispatch（44995817685）终验。
- 实验环境：wuying S4U 计划任务（WuyingOpProbeBuild/Retest949/Sync2，不骑交互
  会话——InteractiveToken 任务随会话断开被清，本次 build interrupted 实证）；
  probe 现场 sbs-win-net2 + probe_dumps{,2} 保留，旧 sbs-win-net 顶层句柄待
  重启后清理。

| 2026-11-22 | **M1.5 收口**：方案 B 注册仲裁全链闭环——Mac 全量门 1019 绿（翻转面零）→ wuying 增量重建 **6 红 exit 全绿 + 链 vs Mac MATCH=6 DIFF=0（逐字节）** → CI 最终 dispatch 45001971226 同 commit 双绿（build-test 3m14s + windows-nightly 26m）。三平台行为面自此单实现一致；备案双注册对（15+）的清理转效果层批产积压（仲裁已保证跨平台确定性，清理非紧迫）。wuying 实验收尾：killall 客户端休眠（升配 8C 记得调回 4C/8G）；sbs-win-net 旧目录顶层句柄待重启清（sbs-win-net2 现役） | **wuying 门现状 = 全绿**；M1.5 平台差目标达成 |
