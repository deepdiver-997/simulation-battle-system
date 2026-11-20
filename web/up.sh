#!/bin/sh
# 在仓库根（main-local 或任意工作区）执行：编译最新 + 启动对战调试台全家桶。
# 用法：web/up.sh [网关端口]        （默认 8070；sim_server 端口固定 4399）
#       手机/局域网测试：SBS_WEB_HOST=0.0.0.0 web/up.sh   （手机访问 http://<本机局域网IP>:8070）
#       ⚠️ 仅网关对外开放；sim_server 恒绑定 127.0.0.1（自研二进制协议不出本机，见 web/README.md 安全节）
# 脚本本身会退出，两个服务以 nohup 常驻——停服：pkill -f build/sim_server; pkill -f web.gateway.app
set -e
cd "$(git rev-parse --show-toplevel)"

PORT="${1:-8070}"
WEB_HOST="${SBS_WEB_HOST:-127.0.0.1}"

echo "── 1/3 编译引擎（含插件 dylib 部署）……"
cmake --build build -j8 > /tmp/sbs_up_build.log 2>&1 || { tail -20 /tmp/sbs_up_build.log; exit 1; }

echo "── 2/3 刷新效果注册表（覆盖率/未实现徽标数据源）……"
./build/sim_server --dump-registry-exit build/registry.json | tail -1

echo "── 3/3 启动 sim_server(4399, 仅本机) + 网关($WEB_HOST:$PORT)……"
pkill -f "build/sim_server" 2>/dev/null || true
pkill -f "web.gateway.app" 2>/dev/null || true
sleep 1
nohup ./build/sim_server --port 4399 > /tmp/sbs_sim_server.log 2>&1 &
nohup web/.venv/bin/python -m web.gateway.app --host "$WEB_HOST" --port "$PORT" > /tmp/sbs_gateway.log 2>&1 &
disown -a 2>/dev/null || true
sleep 2

if curl -s -m 2 "http://127.0.0.1:$PORT/api/health" | grep -q '"ok": true'; then
    echo "✅ 调试台就绪：http://127.0.0.1:$PORT/"
    if [ "$WEB_HOST" = "0.0.0.0" ]; then
        LAN_IP=$(ipconfig getifaddr en0 2>/dev/null || true)
        [ -n "$LAN_IP" ] && echo "   局域网访问（手机）：http://$LAN_IP:$PORT/"
    fi
    echo "   日志：/tmp/sbs_sim_server.log ・ /tmp/sbs_gateway.log（停服：pkill -f build/sim_server; pkill -f web.gateway.app）"
else
    echo "❌ 网关未就绪，看日志：/tmp/sbs_gateway.log"
    exit 1
fi
