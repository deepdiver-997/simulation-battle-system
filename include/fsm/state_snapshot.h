#ifndef FSM_STATE_SNAPSHOT_H
#define FSM_STATE_SNAPSHOT_H

#include <string>

class BattleContext;

namespace fsm {

// 时点状态快照（决策 D16，M1.5 golden 快照）。
//
// battleFsm::log() 每条 DEBUG 时点调一次：把当时的规范化状态文本做 FNV-1a 64 哈希，
// 以 `<hash16> <phase>` 追加进链文件。两侧平台跑同一场景后逐行 diff，第一个失配行
// 就是漂移发生的时点（32 红 = rand 消费序漂移的定位器；也是"绿≠无漂移"的安全网）。
//
// 环境变量（未设时零开销——单次静态 bool 判断后直接返回）：
//   SBS_SCENARIO_TRACE=<path>       链文件（逐行 hash），随场景入库 test/scenario/golden/
//   SBS_SCENARIO_TRACE_FULL=<path>  同时落全量规范化文本（供人读 diff；不入库）
//
// 覆盖面（v1）：sbs_rand 内部状态（首要漂移探测器）+ FSM 标量 + workspace 全字段
// （除 std::function 钩子）+ DamageSnapshot/PinkDamageResolved + 规则票（RuleCenter
// entries，跳过 std::function condition）+ 双方六宠必变状态（id/hp/盾罩/异常位/PP/系别）
// + 上下文关键标量。**未覆盖**（v1 已知盲区，需要时在 serialize 里补）：
// std::any 存储三兄弟（soulmark_storage/plugin_storage/on_stage_storage）、pet.marks 明细、
// anomaly_view_layers、attack_boost_grants 明细、timed bucket 效果体内容。
void snapshot_record(const BattleContext& ctx, const std::string& phase);

}  // namespace fsm

#endif // FSM_STATE_SNAPSHOT_H
