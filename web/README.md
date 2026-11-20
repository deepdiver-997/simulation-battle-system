# sbs 对战调试台（web/）

浏览器里的组队 / 装配 / 跑模拟对战工具：搜索精灵→配 5 技能→双方各 6 只→开局，
事件带按 FSM 时点逐行回放；技能/魂印带"库里有、插件没实现"徽标；
图鉴文本 4399 爬取优先，数据库只做回退（神谕等 4399 没有的内容）。

```
浏览器 (Vue3 + Element Plus, web/frontend)
   │  REST + WebSocket (JSON)
   ▼
网关 (aiohttp, web/gateway) ──── 读 seer_unity.sqlite / 4399 jsonl / 注册表导出
   │  二进制线协议 (10 字节帧头, 与 include/server/protocol.h 一致)
   ▼
sim_server (现有 TCP 协议, 未改动协议本身)
```

## 启动（一条命令）

```sh
web/up.sh            # 编译引擎 + 刷注册表 + 起 sim_server(4399) + 网关(8070)
                     # 服务常驻；停服：pkill -f build/sim_server; pkill -f web.gateway.app
```

### 在 main-local 上跑（不出工作区）

main-local 合并本线后同样适用：在 hub 根目录 `web/up.sh` 即可
（首次需 `web/.venv/bin/pip install -r web/gateway/requirements.txt` 与
`cd web/frontend && npm install && npm run build`——up.sh 不装依赖、只做构建+启动）。

### 手动三步（都在仓库根执行）

```sh
# 1) sim_server（已有服务可跳过；必须在仓库/工作区根目录运行）
cmake --build build -j8
./build/sim_server                     # 默认 127.0.0.1:4399

# 2) 注册表导出（"未实现"徽标的数据源；改过插件后重跑一次）
./build/sim_server --dump-registry-exit build/registry.json

# 3) 网关（同时托管前端构建产物）
web/.venv/bin/pip install -r web/gateway/requirements.txt   # 首次
web/.venv/bin/python -m web.gateway.app --port 8070
```

打开 <http://127.0.0.1:8070/>。

前端改代码后：`cd web/frontend && npm install && npm run build`（开发期也可
`npm run dev` 起在 5173，已配好到 8070 的 API/WS 代理）。

覆盖率页的"重新对账"按钮等价于第 2 步（需 `build/sim_server` 存在）。

## 功能与口径

| 页 | 内容 |
|---|---|
| 组队对战 | 双方 6 槽组队（精灵/常规+第五/通用特性/自定义六维数值）、示例阵容、双方操作台常开（技能剩余 PP、换宠血量、**真实药剂名与库存**）、**空操作**（超时跳过、回合类与异常照常）、**自由选操作方**（可先给对手提交，服务端 pending 队列排队）、自动应答（无可用技能兜底空操作）、逐步模式、断点中文名下拉、全量状态、事件带回放 |
| 图鉴搜索 | 精灵/技能/通用特性搜索；精灵详情：种族值、魂印（4399 文本优先）、推荐培养、4399 技能表、数据库可学技能表（含第五技能阶段与实现徽标） |
| 效果覆盖率 | 库里引用 vs 动态库已注册的差集清单；徽标口径见下 |

**组队两模式（2026-09-25；同日随引擎"双背包模型校准"更新口径）**：
- **直开 6v6**：只传出战背包 6 只（原行为）；我方（玩家 0）默认**房主**，界面已标注。
- **BP 赛前快照**：额外传 `"standby": {"side1": [petId×≤6], "side2": [...]}` →
  引擎 `standby_initialized=true`，格劳瑞 1551 自爆等"赛前读者"按双背包语义结算
  （消耗数>5 时自爆改为消耗至 1 点体力）。直开模式该快照不初始化、待命 6 格全空。
  引擎口径（2026-09-25）：每边**两张静态背包**（出战 6 + 待命 6，合计至多 12 只）；
  待命位精灵王/1551 旗标由 Room 装配期按 id 判定，战斗中只读；出战槽序 = 出战顺序。
  前端待命背包即 6 格静态格，与该模型一一对应。
- **组队卡编辑**：出战槽支持**拖动交换**（含拖到空槽=移动）、**覆盖**（点源槽"覆盖"
  → 点目标槽整份粘贴）、**单只删除**；两侧"清空"保留。
- **药剂**：`"medicines": {"side1": {itemId: 数量}, ...}`（battle_items 表驱动；
  room.cpp 原来传空 → 谁都嗑不了药，已修）。前端组队卡里按方添加。
- **词条编辑**（2026-09-26）：改官方效果的数值参数（如把圣光吟诵 100% 疲惫改 50%）。
  数值来源 = `moves.side_effect_arg` 扁平数组（按 `effect_info.args_num` 切分，下标 =
  `effect_info.info` 模板 `{n}` 占位符），装配期烤进 EffectArgs——覆盖即"先改记录、
  后建效果"（`Skills::apply_arg_overrides`）。阵容 JSON per-pet 可选段：
  `"effectParams": [{"skill": 技能id, "effect": 效果id, "index": {n}下标, "value": 新值}]`。
  前端 PetPicker 技能表"参数"列 → 编辑弹窗（模板渲染 + 数字输入，只记与库值不同的项）；
  槽卡显示"词条已改 ×n"。index 越界按 0 补齐；覆盖不存在的效果 → 开局报错。
  元数据端点 `GET /api/moves/{id}/effects`；A/B 回归 `web/gateway/effect_param_ab_smoke.py`
  （0% 无疲惫 / 库值 100% 有疲惫）；场景 **128**（覆盖/尾参补齐/无覆盖原值/错效果抛错/按槽分发）。
  二期待做：魂印参数、字面量效果参数化。
- **手术台**（2026-09-26，二期）：运行时状态注入。**泊车时**（等输入/逐步停下/断点命中）
  发白名单操作集，服务端持 `run_mutex` 落地（battle_mutate 手术原语：改体力/PP/等级、
  上/解异常、挂/清死亡印记），逐条回执进事件带（🔧），随后自动刷新状态与合法动作集。
  协议 `DEBUG_MUTATE=24`（JSON `{"ops":[...]}`）。与一期同为**不裸写 context** 的设计：
  走引擎存储与校验（钳制/幂等），hp=0 是裸状态（不走死亡结算）、异常绕免疫/抗性直写
  （手术语义）。前端调试卡"🔧 手术台"弹窗；回归 `web/gateway/mutate_smoke.py`
  （注入后快照反映变更 + 注入的印记真实致死）；场景 **129**（原语白盒）。

**第五技能口径**：第五技能 = `hide_moves` 表（官方第五技能**不走**学习表，2240/2240 都不在
monster_learnable_moves，如圣灵谱尼"神灵救世光"、莱茵哈特"王·天乖陆离"）。引擎已放行：
hide_moves 技能只能占第 5 槽（PetFactory 校验）。learnable 的 tag 1..4 是另一套标记
（特训阶段），不是第五技能。无第五候选的精灵（波克尔等）任选 5 个。

**实现徽标口径**：数据库引用的 effect id 对比 `build/registry.json`（sim_server
`--dump-registry(-exit)` 导出的三注册表键集 + 时点/异常对照表）。`custom_effect_programs`
的离线程序算已实现，`custom_effect_overrides` 标注的 ignore/dead_column 显示"无需实现"，
map_to 追到映射目标。

**预提交视角**：`LEGAL_ACTIONS` 对没轮到的一方返回 `prospective:true` 的动作清单
（`canAct:false`），动作提交进服务端 pending 队列，轮到时派发。

**无头自检**（起好 sim_server 与网关后）：

```sh
web/.venv/bin/python -m web.gateway.smoke_e2e     # 搜索/详情/覆盖率 + WS 开局自动打完一局
# 网关开了鉴权时：web/.venv/bin/python -m web.gateway.smoke_e2e --token '其中一把'
```

## 对外部署（社区试用）

给社区朋友试用时网关务必开鉴权：每人发一把 token（逗号分隔），日志可归因。
朋友打开页面时前端会自动弹出令牌输入框（存 localStorage，之后请求自动带上）。

```sh
./build/sim_server &                                   # 只听 127.0.0.1:4399，绝不对外
web/.venv/bin/python -m web.gateway.app --host 0.0.0.0 --port 8070 \
    --auth-token '给张三的令牌,给李四的令牌'
```

- 未配置 `--auth-token` 时门禁关闭，本机开发零配置不变；`--host` 对外但未开鉴权
  会在启动日志里告警。
- 免鉴权面：`/`、`/assets/*`、`/api/health`（静态壳与健康探活）；其余 `/api/*` 与
  WS 握手都要 token。REST 走 `Authorization: Bearer`（前端自动带），WS 与潜在的
  `<img>` 走 `?token=`。比较用恒时函数，失败进封禁计数。
- 防护口径（内存态、重启清零；面向小范围试用，不是抗 DDoS——那层交给隧道/安全组）：
  同一 IP 鉴权失败 5 次/10 分钟 → 封 30 分钟；`/api` 限 240 次/分钟/IP；
  对局 WS 每 IP ≤ 3 条、全局 ≤ 32 条；覆盖率对账（拉子进程，最贵的端点）
  全局冷却 5 分钟（`--refresh-cooldown` 可调）。
- 前面有隧道/反代时加 `--trust-proxy`，按 `X-Forwarded-For` 末跳取真实客户端 IP
  （cloudflared 把真实来源追加在末位，客户端伪造的前置段被自然稀释）。

### HTTPS

有证书时网关可直接上 TLS。注意：内地机房对未备案域名的 80/443 会拦，
非标准端口（如 8443）实践可用：

```sh
web/.venv/bin/python -m web.gateway.app --host 0.0.0.0 --port 8443 \
    --auth-token '…' --tls-certfile fullchain.pem --tls-keyfile privkey.pem
```

自签证书浏览器会告警，两条正路：给域名用 DNS-01 签一张免费 Let's Encrypt
（不用开 80 端口），或直接走 Cloudflare Tunnel（TLS 在 CF 边缘终结，源站不需要证书）。

### 暴露方式怎么选

| 方式 | 适用 | 说明 |
|---|---|---|
| 直接放行网关端口 | 内地用户低延迟优先 | 安全组只放网关端口；sim_server 端口绝不对外；建议 systemd 以非 root 跑 |
| Cloudflare Tunnel | 不想开任何入站端口、怕被扫 | `cloudflared` 只出不进，源站 IP 隐藏，域名/TLS/备案问题一并绕开；免费档内地用户绕海外边缘，延迟增大但回合制调试台可接受，WS 免费档支持 |

## 已知边界

- pvp 模式（`--mode pvp`）的 `party` 提交不带装备段；调试台目前只走 solo
  `INIT_BATTLE_JSON`（一次给双方）。
- `image_url` 等 4399 立绘字段在现有爬取产物里是空的，前端暂未用图片；
  `/api/img` 代理已备好（限 4399 域名），后续补立绘直接接。
- 逐步模式下的暂停没有快照推送，需要手动点"全量状态"看效果表。

## 手机/局域网测试与安全边界

```
SBS_WEB_HOST=0.0.0.0 web/up.sh        # 手机访问 http://<Mac的局域网IP>:8070
```

暴露面设计（只开网关、引擎不出本机）：

- **网关（8070，aiohttp）**：`SBS_WEB_HOST=0.0.0.0` 时对局域网开放。它**没有鉴权**——
  同一 WiFi 下任何设备都能开局、用手术台注入状态、读注册表。调试台数据非敏感，
  风险 = 被同网段的人蹭着玩/干扰你的调试会话。
- **sim_server（4399，自研 C++ 二进制协议）恒绑定 127.0.0.1**（main.cpp 默认值，
  up.sh 也不传 --host）。它是手写协议解析、未经 fuzz 审计的 C++ 服务，是"缓冲区溢出
  类攻击"的真实暴露面——因此**不随 0.0.0.0 外露**；手机只需要网关（前端 + WS 都在
  8070，网关在本机代理到 4399）。需要多台电脑直连引擎再显式 `--host 0.0.0.0`，自担风险。
- **互联网方向**：家用路由 NAT 默认挡掉入站——"在外面"通常进不来。要防的是路由器
  上的端口转发/DMZ/UPnP 意外把 8070 映射出去，出远门前可检查一遍。公共 WiFi 下别开
  0.0.0.0（同网段不可信）；跨网络测试建议 Tailscale 一类组网回家，而不是开端口。
