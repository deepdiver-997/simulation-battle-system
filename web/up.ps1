# web/up.ps1 —— up.sh 的 Windows 版（编译 + 注册表刷新 + 起 sim_server/网关 + 开浏览器）
#
# 用法（仓库根或任意目录可调，仓库根从脚本位置自动解析；sim_server/网关都以仓库根为 CWD）：
#   powershell -ExecutionPolicy Bypass -File web\up.ps1                # 默认 8070
#   powershell -ExecutionPolicy Bypass -File web\up.ps1 -Port 8080
#   局域网访问：-WebHost 0.0.0.0（sim_server 恒绑定 127.0.0.1，自研二进制协议不出本机）
# 停服：Get-Process sim_server | Stop-Process -Force
#       Get-CimInstance Win32_Process -Filter "Name='python.exe'" |
#         ? { $_.CommandLine -like '*web.gateway.app*' } | % { Stop-Process -Id $_.ProcessId -Force }
# 依赖：MSYS2 UCRT64（C:\msys64，见 tools\windows\env-setup.ps1）、Python 3（首跑自动建 web\.venv 装依赖）
# ⚠️ 前端产物 web\frontend\dist\ 不入库：缺 index.html 时按提示 npm run build 或从开发机拷贝
# 本文件必须 UTF-8 带 BOM（D9：PS5.1 中文系统对无 BOM 文件按 GBK 解析）
param(
    [int]$Port = 8070,
    [string]$WebHost = '127.0.0.1'
)
$ErrorActionPreference = 'Continue'   # 同 gate.ps1：PS5.1 对原生命令 stderr 重定向 + Stop 会把首行 stderr 当终止错误炸脚本；失败由 $LASTEXITCODE 兜底
$repo = Split-Path -Parent (Split-Path -Parent $PSCommandPath)   # web\up.ps1 → 上溯两级到仓库根
Set-Location $repo
$env:PATH = 'C:\msys64\ucrt64\bin;C:\msys64\usr\bin;' + $env:PATH   # cmake/ninja/gcc + 运行期 MinGW/sqlite DLL
$logs = Join-Path $env:TEMP 'sbs_up_logs'
New-Item -ItemType Directory -Force $logs | Out-Null

# 1) 编译引擎（build 缺失时先 configure，口径同 gate.ps1；deploy 是 ALL target，部署随构建走）
if (-not (Test-Path (Join-Path $repo 'build\CMakeCache.txt'))) {
    Write-Host '── 0/4 configure（Ninja / Release）……'
    cmake -S $repo -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
    if ($LASTEXITCODE -ne 0) { throw "configure 失败（exit $LASTEXITCODE）" }
}
Write-Host '── 1/4 编译引擎（含插件部署）……'
cmake --build build -j $env:NUMBER_OF_PROCESSORS > (Join-Path $logs 'build.log') 2>&1
if ($LASTEXITCODE -ne 0) {
    Get-Content (Join-Path $logs 'build.log') -Tail 25
    throw "编译失败（完整日志 $logs\build.log）"
}

# 2) 注册表刷新（覆盖率/未实现徽标数据源；改过插件后必须重跑）
Write-Host '── 2/4 刷新效果注册表……'
./build/sim_server.exe --dump-registry-exit build/registry.json | Select-Object -Last 1
if ($LASTEXITCODE -ne 0) { throw "registry dump 失败（exit $LASTEXITCODE）" }

# 3) Python 解析（三级回退）：web\.venv → py -3 / python（标准 Windows，自动建 venv）
#    → MSYS2 UCRT64 python + pacman 装 aiohttp（wuying/裸 MSYS2 机：无官方 Python，
#    mingw python 的 pip 装不了 win_amd64 轮子，走 MSYS2 仓库的 python-aiohttp 包最稳）
$venvPy = Join-Path $repo 'web\.venv\Scripts\python.exe'
$py = $null
if (-not (Test-Path $venvPy)) {
    Write-Host '── 3/4 Python 解析……'
    if (Get-Command py -ErrorAction SilentlyContinue) {
        py -3 -m venv web\.venv
    } elseif (Get-Command python -ErrorAction SilentlyContinue) {
        python -m venv web\.venv
    }
}
if (Test-Path $venvPy) {
    $py = $venvPy
    & $py -m pip install -r web\gateway\requirements.txt -i https://pypi.tuna.tsinghua.edu.cn/simple
    if ($LASTEXITCODE -ne 0) { throw '网关依赖安装失败（pip 日志见上）' }
} else {
    $py = 'C:\msys64\ucrt64\bin\python.exe'
    if (-not (Test-Path $py)) { throw '找不到 Python 3：无 py -3 / python，也无 C:\msys64\ucrt64\bin\python.exe' }
    & $py -c 'import aiohttp' 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Host '   UCRT64 python 缺 aiohttp → pacman 装 mingw-w64-ucrt-x86_64-python-aiohttp……'
        C:\msys64\usr\bin\pacman.exe -S --needed --noconfirm mingw-w64-ucrt-x86_64-python-aiohttp
        if ($LASTEXITCODE -ne 0) { throw 'pacman 安装 aiohttp 失败（检查网络/镜像）' }
    }
}

# 3.5) 前端产物预检（网关只在 dist 存在时托管；缺了页面能起但首页 404，提前拦住）
if (-not (Test-Path (Join-Path $repo 'web\frontend\dist\index.html'))) {
    throw '缺 web\frontend\dist\index.html：在 web\frontend 下 npm install && npm run build，或从开发机拷 dist（dist 不入库）'
}

# 4) 起服务（先杀旧；Start-Process 分离常驻，本脚本退出不影响服务——对齐 up.sh 的 nohup 语义）
Write-Host ('── 4/4 启动 sim_server(4399, 仅本机) + 网关({0}:{1})……' -f $WebHost, $Port)
Get-Process sim_server -ErrorAction SilentlyContinue | Stop-Process -Force
Get-CimInstance Win32_Process -Filter "Name='python.exe'" |
    Where-Object { $_.CommandLine -like '*web.gateway.app*' } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
Start-Sleep -Seconds 1
Start-Process -FilePath (Join-Path $repo 'build\sim_server.exe') -ArgumentList '--port', '4399' `
    -WorkingDirectory $repo -WindowStyle Hidden `
    -RedirectStandardOutput (Join-Path $logs 'sim_server.log') `
    -RedirectStandardError (Join-Path $logs 'sim_server.err.log')
Start-Process -FilePath $py -ArgumentList '-m', 'web.gateway.app', '--host', $WebHost, '--port', "$Port" `
    -WorkingDirectory $repo -WindowStyle Hidden `
    -RedirectStandardOutput (Join-Path $logs 'gateway.log') `
    -RedirectStandardError (Join-Path $logs 'gateway.err.log')

# 5) 健康检查 + 开浏览器
$ready = $false
for ($i = 0; $i -lt 15; $i++) {
    Start-Sleep -Seconds 1
    try {
        $h = Invoke-RestMethod -Uri ("http://127.0.0.1:{0}/api/health" -f $Port) -TimeoutSec 2
        if ($h.ok) { $ready = $true; break }
    } catch {}
}
if ($ready) {
    Write-Host ("OK 调试台就绪：http://127.0.0.1:{0}/" -f $Port)
    if ($WebHost -eq '0.0.0.0') {
        $lan = (Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
            Where-Object { $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' } |
            Select-Object -First 1).IPAddress
        if ($lan) { Write-Host ("   局域网访问：http://{0}:{1}/" -f $lan, $Port) }
    }
    Write-Host ("   日志：{0}（sim_server.log / gateway.log；停服命令见本文件头注释）" -f $logs)
    Start-Process ("http://127.0.0.1:{0}/" -f $Port)
    exit 0
} else {
    Write-Host ("FAIL 网关未就绪，看日志：{0}" -f (Join-Path $logs 'gateway.err.log'))
    Get-Content (Join-Path $logs 'gateway.err.log') -Tail 15 -ErrorAction SilentlyContinue
    exit 1
}
