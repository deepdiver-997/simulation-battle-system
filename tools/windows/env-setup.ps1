# sbs Windows 移植 - 环境安装脚本（幂等，可重复执行）
#
# 装什么：MSYS2 + UCRT64 MinGW-w64 工具链（GCC / CMake / Ninja / SQLite3 / nlohmann-json / Python / git）。
# 为什么这套：见 docs_local/docs/07-工具与测试/Windows-移植.md 决策记录 D1。
# 全程走清华镜像，不碰 GitHub；不绑死特定机器（任何标准 Windows 都能跑本脚本）。
#
# 用法（管理员 PowerShell，SSH 会话自带管理员权限）：
#   powershell -ExecutionPolicy Bypass -File tools\windows\env-setup.ps1 [-MsysRoot C:\msys64] [-AddDefenderExclude]

param(
    [string]$MsysRoot = 'C:\msys64',
    [switch]$AddDefenderExclude,
    [switch]$SkipPackageInstall
)

$ErrorActionPreference = 'Continue'   # PS5.1：Stop + PS流重定向会把原生命令 stderr 首行当终止错误炸脚本；失败由 \$LASTEXITCODE/显式 throw 兜底
$ProgressPreference = 'SilentlyContinue'   # PS5.1 的 IWR 进度条会让大文件下载慢一个量级
try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch {}

function Test-MsysRoot { Test-Path (Join-Path $MsysRoot 'usr\bin\bash.exe') }

# ── 1/4 MSYS2 本体 ──────────────────────────────────────────────────────────
if (-not (Test-MsysRoot)) {
    Write-Host "[1/4] 未检测到 MSYS2，从清华镜像下载安装器 ..."
    $inst = Join-Path $env:TEMP 'msys2-installer.exe'
    $url  = 'https://mirrors.tuna.tsinghua.edu.cn/msys2/distrib/msys2-x86_64-latest.exe'
    Invoke-WebRequest -Uri $url -OutFile $inst -UseBasicParsing
    Write-Host "      静默安装到 $MsysRoot （几分钟，无窗口）..."
    # 安装器是 GUI 子系统程序，必须 Start-Process -Wait，& 不等 GUI 进程
    $proc = Start-Process -FilePath $inst -ArgumentList @('install', "--root", "$MsysRoot", '--confirm-command') -PassThru
    $proc.WaitForExit()
    if (-not (Test-MsysRoot)) { throw "MSYS2 安装未完成或缺组件：$MsysRoot" }
} else {
    Write-Host "[1/4] MSYS2 已存在：$MsysRoot"
}

# ── 2/4 pacman 换清华源（已配置过则跳过）──────────────────────────────────
Write-Host '[2/4] 配置 pacman 清华镜像 ...'
$pacmanD = Join-Path $MsysRoot 'etc\pacman.d'
$mirrors = @{
    'mirrorlist.msys'    = 'Server = https://mirrors.tuna.tsinghua.edu.cn/msys2/msys/$arch/'
    'mirrorlist.ucrt64'  = 'Server = https://mirrors.tuna.tsinghua.edu.cn/msys2/mingw/ucrt64/'
    'mirrorlist.mingw64' = 'Server = https://mirrors.tuna.tsinghua.edu.cn/msys2/mingw/x86_64/'
    'mirrorlist.clang64' = 'Server = https://mirrors.tuna.tsinghua.edu.cn/msys2/mingw/clang64/'
}
foreach ($name in $mirrors.Keys) {
    $p    = Join-Path $pacmanD $name
    $line = $mirrors[$name]
    if (-not (Test-Path $p)) {
        Set-Content -Path $p -Value $line -Encoding ASCII
    } elseif (-not (Select-String -Path $p -Pattern 'tuna' -Quiet)) {
        $old = Get-Content $p -Raw
        Set-Content -Path $p -Value ("$line`r`n$old") -Encoding ASCII
    }
}

# ── 3/4 keyring 初始化（安装器通常已做，兜底）────────────────────────────
if (-not (Test-Path (Join-Path $MsysRoot 'etc\pacman.d\gnupg'))) {
    Write-Host '[3/4] 初始化 pacman keyring ...'
    & (Join-Path $MsysRoot 'usr\bin\bash.exe') -lc 'pacman-key --init && pacman-key --populate msys2'
} else {
    Write-Host '[3/4] keyring 已就绪'
}

# ── 4/4 工具链包（--needed 幂等）──────────────────────────────────────────
if (-not $SkipPackageInstall) {
    Write-Host '[4/4] 安装 UCRT64 工具链包（首次约 1-2 GB 下载）...'
    $pkgs = @(
        'mingw-w64-ucrt-x86_64-gcc',
        'mingw-w64-ucrt-x86_64-cmake',
        'mingw-w64-ucrt-x86_64-ninja',
        'mingw-w64-ucrt-x86_64-sqlite3',
        'mingw-w64-ucrt-x86_64-nlohmann-json',
        'mingw-w64-ucrt-x86_64-python',
        'mingw-w64-ucrt-x86_64-git'
    ) -join ' '
    & (Join-Path $MsysRoot 'usr\bin\bash.exe') -lc "pacman -Sy --noconfirm --needed $pkgs"
    if ($LASTEXITCODE -ne 0) { throw "pacman 安装失败（exit $LASTEXITCODE）" }
} else {
    Write-Host '[4/4] 跳过包安装（-SkipPackageInstall）'
}

if ($AddDefenderExclude) {
    # 实时扫描会显著拖慢编译/链接 855 个小二进制的链接；SSH 会话自带管理员权限，可直接加。
    try {
        Add-MpPreference -ExclusionPath $MsysRoot
        Write-Host "Defender 排除已加：$MsysRoot"
    } catch {
        Write-Warning "Defender 排除失败（不影响安装，构建会慢一些）：$($_.Exception.Message)"
    }
}

# ── 验证 ────────────────────────────────────────────────────────────────────
Write-Host ''
Write-Host '=== 工具链验证 ==='
& (Join-Path $MsysRoot 'usr\bin\bash.exe') -lc @'
set -e
echo "gcc:    $(gcc --version | head -1)"
echo "cmake:  $(cmake --version | head -1)"
echo "ninja:  $(ninja --version)"
echo "python: $(python --version 2>&1)"
echo "git:    $(git --version | head -1)"
echo "sqlite: $(pkg-config --modversion sqlite3 2>/dev/null || ls /ucrt64/include/sqlite3.h >/dev/null && echo sqlite3.h OK)"
'@
Write-Host ''
Write-Host '环境就绪。下一步：'
Write-Host '  1) 同步代码（rsync from Mac，命令见 Windows-移植.md §5）'
Write-Host '  2) powershell -ExecutionPolicy Bypass -File tools\windows\gate.ps1 -ConfigureOnly   # 先过 configure'
