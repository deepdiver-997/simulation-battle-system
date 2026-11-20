<!--
CI 会自动做路径分类（tools/ci/classify.sh）：效果层与内核层混碰的 PR 会被
自动拒绝；内核路径需要先开 issue 拿 kernel-approved 标签；新增的效果/场景
文件头 40 行内必须有证据链接（引注检查）。以下必填项缺一项会被打回。
-->

## 变更分类

- [ ] 效果·纯新增（resources/ 与 test/scenario/ 下只新增文件）
- [ ] 效果·改口径（修改了既有效果实现或场景期望值——请附差异证据）
- [ ] 内核（src/、include/；issue 编号：____，kernel-approved 标签：已挂/未挂）

## 场景号

scenario____（新增/修改的场景 id 列表；未含场景的 PR 请说明为何不需要）

## 变异证据（MUT）

摘除本实现 → 场景转红；还原 → 复绿。贴关键输出片段：

```
（FAIL sim_scenario_XXXX ... / PASS）
```

## 本地全量门

```bash
cmake --build build -j8 && for b in build/sim_scenario_*; do "$b" || echo FAIL "$b"; done
```

结论：构建 PASS / 场景 __ 个全 PASS（或列出红项与原因）

## 引注（证据链接）

- effect_id ____：依据 = <社区解析帖 / 实测记录 URL>
- （每个新增文件的头注释里也应带同款链接，CI 会逐文件检查）
