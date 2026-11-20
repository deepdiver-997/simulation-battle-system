// J330 · moves1470 天赋威力（1 宏 / 11 载体；2026-11-06）
//
// 官方模板：[0] 自身天赋值越高则技能威力越大；iconHit，当前库注明 boss 有效。
// 范围：只实现项目既定的满天赋 T=31 -> 180 特化；不支持非31输入，不从面板、
// 种族值或战斗账本反推天赋，不把常量特化宣传为 0..31 全档实现。
// 数值依据为 S26 收录的原作者分档资料（B级、非本单官方服实测）；旧文 PVE 无效
// 与当前库冲突时按当前库，不加 boss 无效门；预填跨空间问题归 R7，不在本单修。
// 〔源: docs_local/docs/05-任务清单/1470天赋威力调研-2026-11-05.md:45-48〕
// 〔源: docs_local/docs/05-任务清单/1470天赋威力调研-2026-11-05.md:183-193〕
// 〔源: docs_local/docs/05-任务清单/1470天赋威力调研-2026-11-05.md:288〕
//
// 时点/判据：复用默认 HIT / SKILL_EFFECT，MISS、普通盔、命中效果失效由通用
// 分发门负责；无另加的先后手、属性或基础威力>0 条件。只写本次执行的使用者视图。
// 〔源: src/entities/skills.cpp:228-230〕〔源: src/entities/skills.cpp:301-303〕
// 覆盖语义：view = 180（不是 +=180 或乘算），35431 的0与34256的140均得到180；
// 不改 Skills::power、类别、系别或 DB。烧伤先物化面板一半，本效果随后覆盖180。
// 〔源: src/fsm/battleFsm.cpp:498-513〕
// API：SkillView::operator=(int) 自动置 rewritten，ATTACK_DAMAGE 收尾重算并消费。
// 沿 J315/179 通道，但179是平加、本效果是覆盖；2210满31->180仅作实现形态先例。
// 〔源: include/fsm/battleWorkspace.h:76-90〕〔源: src/fsm/battleFsm.cpp:826-832〕
// 〔源: resources/moves_lib/j_cond_power_rewrite.cpp:117-127〕
// 〔源: resources/moves_lib/tianqi_skills.cpp:27-31〕
// 〔源: resources/moves_lib/tianqi_skills.cpp:53-60〕
//
// 动工前四查验（feat/j330，基线7e408856，只读 processed/seer_unity.sqlite）：
// 1. rg -n '\b1470\b' src include resources：内核无实现、moves仅旧注/场景号；
//    全部本地/远端 refs 的 src/include/moves 精确词扫描无非注释1470，未见未合并实现。
//    J315停手记档属实；其34256被举作179的旧例不采用，本次DB实查为纯[1470]。
//    〔源: resources/moves_lib/j_cond_power_rewrite.cpp:39-52〕
// 2. 动工前 hub registry 快照 moves=1413（不含1470）、soul=410（含1470）。
// 3. json_each 精确成员查：普通链11载体/11引用，伙伴链0/0；args_num/arg_count=0。
//    载体13745/13746/33310/33463/33653/33903/34256/34376/34942/35431/37964；
//    十技power=0、34256=140；整链参数均耗尽，1470切片均[]；37964零基偏移4。
//    〔源: docs_local/docs/05-任务清单/1470天赋威力调研-2026-11-05.md:71-87〕
// 4. custom_effect_programs / custom_effect_overrides WHERE effect_id=1470 均0行。
// 同号隔离：不碰K399 soul1470（暗夜·诺伊尔顿，独立注册空间/合法boss无效）。
// 〔源: resources/soul_lib/k399_hp_gate_output_family.cpp:18-24〕
// 回归：scenario_7920_talent_power / scenario_7921_talent_power_gates。

#include "common.h"

namespace {

constexpr int kFullTalentPower = 180;

EffectResult effect_1470_full_talent_power(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1470, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1470, args,
                     "1470 零参数布局=[owner,target]");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1470, args, "需要 owner ∈ {0,1}");
    ctx->ws.skill_power_view[owner] = kFullTalentPower;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1470, effect_1470_full_talent_power)
