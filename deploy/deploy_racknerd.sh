#!/bin/sh
# deploy/deploy_racknerd.sh — sbs 控制台全家桶部署到 RackNerd（107.174.127.252）。
#
# 产物管线（与 mail-system 同机同约束，见其 docs/local/server-credentials.md）：
#   永不在 1GB 内存的服务器上编译；本地 docker buildx 出 amd64 成品，服务器零构建。
#
# 用法（仓库根）：
#   docker buildx build --platform linux/amd64 -f deploy/Dockerfile \
#     -t sbs-server:amd64 --load .        # 先构建镜像（或加 --build 让本脚本代跑）
#   sh deploy/deploy_racknerd.sh
# 可重复执行：rsync 幂等、token 已存在则保留、服务 restart 生效新产物。
#
# 连接：SBS_SERVER 缺省走 ~/.ssh/config 的 racknerd 别名（sshd 2222 + 阿里云
# ProxyJump——上海直连差，2026-10-19 起 22 口不再对外）。
set -e
SERVER=${SBS_SERVER:-racknerd}
ROOT=/opt/sbs
cd "$(git rev-parse --show-toplevel)"

if [ "${1:-}" = "--build" ]; then
    echo "── 0/6 docker 构建（amd64）……"
    docker buildx build --platform linux/amd64 -f deploy/Dockerfile \
        -t sbs-server:amd64 --load . > /tmp/sbs_docker_build.log 2>&1 || {
        tail -30 /tmp/sbs_docker_build.log; exit 1; }
fi

echo "── 1/6 抽取容器产物 → deploy/out/ ……"
docker rm -f sbs-extract >/dev/null 2>&1 || true
docker create --name sbs-extract sbs-server:amd64 >/dev/null
rm -rf deploy/out && mkdir -p deploy/out
docker cp sbs-extract:/out/. deploy/out/
docker rm sbs-extract >/dev/null

echo "── 2/6 容器内冒烟（bundle 布局 + 真 DB，dump-registry-exit）……"
docker run --rm --platform linux/amd64 \
    -v "$PWD/scripts/data/processed/seer_unity.sqlite:/out/scripts/data/processed/seer_unity.sqlite:ro" \
    -w /out --entrypoint /bin/bash sbs-server:amd64 \
    -c './sim_server --dump-registry-exit /tmp/registry.json && head -c 200 /tmp/registry.json && echo'

echo "── 3/6 上传产物与数据（bundle/DB/jsonl/gateway/dist/units）……"
# ⚠️ 顺序：bundle 的 rsync 带 --delete，会把目标目录里"源侧没有"的空脚手架
# 目录删掉——数据目录的 mkdir 必须放在 bundle rsync **之后**（实测踩过：
# 先 mkdir 后 rsync → scripts/ web/ 被清 → 传 DB 时目录不存在报错）。
# ⚠️ exclude 令牌文件：--delete 连 exclude 一起生效（保护接收侧不被删）——
#    2026-09-28 三次令牌丢失事故根因，见下方第 4 步注释。
rsync -az --delete --exclude='/sbs-gateway.env' deploy/out/ "$SERVER:$ROOT/"
ssh "$SERVER" "mkdir -p $ROOT/build $ROOT/scripts/data/processed $ROOT/reference/scrape_4399/out $ROOT/web/frontend"
rsync -az scripts/data/processed/seer_unity.sqlite "$SERVER:$ROOT/scripts/data/processed/"
rsync -az reference/scrape_4399/out/pets_full.jsonl reference/scrape_4399/out/hunyin.jsonl "$SERVER:$ROOT/reference/scrape_4399/out/"
rsync -az --delete --exclude __pycache__ web/gateway/ "$SERVER:$ROOT/web/gateway/"
rsync -az --delete web/frontend/dist/ "$SERVER:$ROOT/web/frontend/dist/"
scp -q deploy/systemd/sbs-sim.service deploy/systemd/sbs-gateway.service "$SERVER:/etc/systemd/system/"

echo "── 4/6 服务器侧安装（venv/token/registry/服务）……"
ssh "$SERVER" "
    set -eo pipefail
    export LD_LIBRARY_PATH=$ROOT
    if [ ! -x $ROOT/venv/bin/python ]; then
        apt-get install -y -q python3.10-venv >/dev/null
        python3 -m venv $ROOT/venv
    fi
    $ROOT/venv/bin/pip install -q -r $ROOT/web/gateway/requirements.txt
    # ⚠️ 令牌保留：bundle rsync --delete 会删掉 deploy/out 里没有的文件，其中包括
    # sbs-gateway.env——2026-09-28 三次\"令牌丢失\"事故的真凶（每次部署重置随机钥，
    # 渠道公共令牌一并蒸发）。改为部署时幂等保底：文件在则不动，缺则重建（含公共令牌）。
    if [ ! -f $ROOT/sbs-gateway.env ]; then
        echo \"SBS_AUTH_TOKEN=\$(openssl rand -hex 16),sbs-3021308f\" > $ROOT/sbs-gateway.env
        chmod 600 $ROOT/sbs-gateway.env
    fi
    mkdir -p $ROOT/web/.imgcache
    cd $ROOT
    ./sim_server --dump-registry-exit $ROOT/build/registry.json | tail -1
    systemctl daemon-reload
    systemctl enable --now sbs-sim sbs-gateway >/dev/null 2>&1
    systemctl restart sbs-sim sbs-gateway
    ufw allow 8070/tcp comment 'sbs console' >/dev/null 2>&1 || true
    # 网关首启要装载数据目录（~4s），重试探测而非固定 sleep——否则 set -e 会误判失败
    for i in \$(seq 1 15); do
        if curl -s -m 2 http://127.0.0.1:8070/api/health | grep -q 'ok'; then
            echo \"网关就绪（第 \${i} 次探测）\"
            break
        fi
        sleep 1
    done
    echo -n 'health: '; curl -s -m 3 http://127.0.0.1:8070/api/health
    echo
    ss -tln | grep -E ':4399 |:8070 ' | awk '{print \$4}'
"

echo "── 5/6 外部连通性 ……"
curl -s -m 5 "http://107.174.127.252:8070/api/health" && echo " ← 公网可达"

echo "── 6/6 访问令牌（控制台右上角 🔑 输入用）……"
ssh "$SERVER" "grep SBS_AUTH_TOKEN $ROOT/sbs-gateway.env"
echo "✅ 完成：http://107.174.127.252:8070/"
