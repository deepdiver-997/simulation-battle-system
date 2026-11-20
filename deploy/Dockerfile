# deploy/Dockerfile — 服务器产物构建（linux/amd64，Ubuntu 22.04 对齐 RackNerd）
#
# 用法（仓库根）：
#   docker buildx build --platform linux/amd64 -f deploy/Dockerfile -t sbs-server:amd64 --load .
#
# 目标机硬约束（mail-system v8 docs/local/server-credentials.md，同机复用）：
#   - 永不在 1GB 内存的服务器上编译；
#   - CPU 是 Ivy Bridge → -march=x86-64-v2；
#   - 服务器是 Ubuntu 22.04 → 基础镜像必须同为 22.04（glibc/libstdc++ 精确对齐）。
#
# 产物布局 /out = 服务器 /opt/sbs 的最小镜像：
#   sim_server + libsbs_rand.so（D15 拆出的 SHARED RNG，sim_core PUBLIC 链接、无
#   RPATH——服务器侧靠 LD_LIBRARY_PATH=/opt/sbs 解析，2026-11-09 部署事故修复）
#   + resources/<lib>/liblib_1.so（运行时相对 CWD 加载，见
#   src/effects/effect.cpp 平台后缀与 pet_factory initialize_runtime_data）。
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake libsqlite3-dev nlohmann-json3-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY include ./include
COPY src ./src
COPY resources ./resources
RUN cmake -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SMOKE_TEST=OFF \
      -DBUILD_TRAINING_CLI=OFF \
      -DCMAKE_CXX_FLAGS_RELEASE="-march=x86-64-v2 -O2" \
 && cmake --build build -j"$(nproc)" \
      --target sim_server moves_plugin soul_plugin suit_plugin \
 && mkdir -p /out/resources/moves_lib /out/resources/soul_lib /out/resources/suit_lib \
 && cp build/sim_server /out/ \
 && cp build/libsbs_rand.so /out/ \
 && cp build/plugins/moves_lib/liblib_1.so /out/resources/moves_lib/ \
 && cp build/plugins/soul_lib/liblib_1.so  /out/resources/soul_lib/ \
 && cp build/plugins/suit_lib/liblib_1.so  /out/resources/suit_lib/ \
 && cp resources/boss_invalid_effects.json /out/resources/ \
 && echo "── /out 产物 ──" && find /out -type f | sort
