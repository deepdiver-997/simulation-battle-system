# sbs Windows 全量回归门（PowerShell 实现，决策记录 D2：弃 git-bash 包装）
#
# 等价 Mac 侧基线流程：
#   cmake 全量构建（含插件 deploy 自定义 target）→ 轮跑 build/sim_scenario_*.exe → 统计红绿。
#
# 用法（任意目录可调，仓库根从脚本位置自动解析；场景二进制必须以仓库根为 CWD 运行）：
#   powershell -ExecutionPolicy Bypass -File tools\windows\gate.ps1                     # 全量
#   powershell -ExecutionPolicy Bypass -File tools\windows\gate.ps1 -Filter 'scenario_001*'   # 子集
#   powershell -ExecutionPolicy Bypass -File tools\windows\gate.ps1 -SkipBuild          # 只跑已构建的
#   powershell -ExecutionPolicy Bypass -File tools\windows\gate.ps1 -ConfigureOnly      # 只过 configure
#
# 退出码：0=全绿；1=有红/构建失败。红名单落 build\gate_failures.txt。

param(
    [string]$BuildDir = 'build',
    [int]$Jobs = [Math]::Max(1, [Environment]::ProcessorCount - 1),
    [string]$Filter = '*',
    [int]$TimeoutSec = 120,
    [switch]$SkipBuild,
    [switch]$ConfigureOnly,
    [string]$MsysRoot = 'C:\msys64'
)

$ErrorActionPreference = 'Continue'   # PS5.1：Stop + PS流重定向会把原生命令 stderr 首行当终止错误炸脚本；失败由 \$LASTEXITCODE/显式 throw 兜底
# gate.ps1 位于 <repo>\tools\windows\ → 上溯三层到仓库根
$repo = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSCommandPath))

# UCRT64 工具链进 PATH（cmake/ninja/gcc 不需要 bash 登录环境）
$env:PATH = (Join-Path $MsysRoot 'ucrt64\bin') + ';' + (Join-Path $MsysRoot 'usr\bin') + ';' + $env:PATH

$buildPath = Join-Path $repo $BuildDir

# 0) 官方数据库预检（场景二进制从仓库根按相对路径加载）
$db = Join-Path $repo 'scripts\data\processed\seer_unity.sqlite'
if (-not (Test-Path $db)) {
    throw "缺官方数据库：$db`n从 Mac 同步：rsync + scp，命令见 docs_local/docs/07-工具与测试/Windows-移植.md §5"
}

# 1) configure（build 目录不存在时）
if (-not (Test-Path (Join-Path $buildPath 'CMakeCache.txt'))) {
    Write-Host "=== configure（Ninja / Release）==="
    $ucrtFwd = (Join-Path $MsysRoot 'ucrt64') -replace '\\', '/'
    cmake -S $repo -B $buildPath -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$ucrtFwd"
    if ($LASTEXITCODE -ne 0) { throw "configure 失败（exit $LASTEXITCODE）" }
}
if ($ConfigureOnly) { Write-Host 'configure OK（-ConfigureOnly 到此为止）'; exit 0 }

# 2) 全量构建（ALL 含 moves/soul/suit 插件 + 三个 *_plugin_deploy 拷贝 target）
if (-not $SkipBuild) {
    Write-Host "=== 全量构建（-j$Jobs，首次约 30-60 分钟：855 场景二进制 + 3 插件）==="
    cmake --build $buildPath -j $Jobs
    if ($LASTEXITCODE -ne 0) { throw "构建失败（exit $LASTEXITCODE）" }
}

# 3) 轮跑场景（顺序执行保随机序可复现；-Jobs 只作用于构建阶段）
# D16 golden 快照：每个场景输出时点状态 hash 链到 build\golden\<name>.chain；
# 与 Mac 侧同名文件逐行 diff，第一个失配行即漂移时点。
$goldenDir = Join-Path $buildPath 'golden'
New-Item -ItemType Directory -Force $goldenDir | Out-Null

function Invoke-Scenario {
    param([string]$Exe, [int]$TimeoutSeconds)
    $env:SBS_SCENARIO_TRACE = Join-Path $goldenDir (((Split-Path -Leaf $Exe) -replace '^sim_', '') -replace '\.exe$', '.chain')
    $p = Start-Process -FilePath $Exe -NoNewWindow -PassThru
    if (-not $p.WaitForExit($TimeoutSeconds * 1000)) {
        try { $p.Kill() } catch {}
        return 124   # 超时，按红处理
    }
    return $p.ExitCode
}

$bins = @(Get-ChildItem (Join-Path $buildPath 'sim_scenario_*.exe') | Sort-Object Name)
if ($Filter -ne '*') { $bins = @($bins | Where-Object { $_.Name -like "sim_$Filter" }) }
if ($bins.Count -eq 0) { throw "没有匹配的场景二进制（Filter=$Filter）。先全量构建。" }

Write-Host ("=== 轮跑 {0} 个场景（cwd=仓库根，单例超时 {1}s）===" -f $bins.Count, $TimeoutSec)
$pass = 0; $fail = 0
$failures = New-Object System.Collections.Generic.List[string]
Push-Location $repo
try {
    foreach ($b in $bins) {
        $code = Invoke-Scenario -Exe $b.FullName -TimeoutSeconds $TimeoutSec
        if ($code -eq 0) {
            $pass++
        } else {
            $fail++
            $line = '{0}  exit={1}' -f $b.Name, $code
            $failures.Add($line)
            Write-Host "FAIL  $line"
        }
    }
} finally {
    Pop-Location
}

if ($failures.Count -gt 0) {
    Set-Content -Path (Join-Path $buildPath 'gate_failures.txt') -Value $failures -Encoding UTF8
}

Write-Host ''
Write-Host ('=== 回归门结果：PASS {0} / FAIL {1} / 共 {2} ===' -f $pass, $fail, $bins.Count)
if ($failures.Count -gt 0) {
    Write-Host ('红名单已写 ' + (Join-Path $buildPath 'gate_failures.txt'))
    exit 1
}
exit 0
