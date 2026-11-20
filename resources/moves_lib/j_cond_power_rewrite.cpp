// J 组 J315 · 条件威力改写族 —— effect 179（1 宏 18 载体；1470 内核缺口停手记档）
//（2026-11-02，worktree ~/.agent-workspaces/workspaces/sbs-local/agents/j315）
//
// 家族语义（effect_info 官方模板即权威）：
//   179  [1] 若属性相同则技能威力提升{0}
//   1470 [0] 自身天赋值越高则技能威力越大 —— ⛔ 内核缺口停手（文末记档）
//
// 实现口径：
//   - 时点 = SKILL_EFFECT（effect_register_state 默认桶）：威力视图已物化
//     （battleFsm materialize），效果写入经 operator+= 置 rewritten → ATTACK_DAMAGE
//     收尾按 §8.7 变威力重算（暴击破防联动天然覆盖，K9 242/733/134 直系通道）。
//   - 判据 = **技能属性 == 使用者属性**（看板权威判据 + DB 实证）：谓词镜像内核本系
//     加成 involve()（calculation.h:139——技能单属性：使用者任一属性槽相等；技能双
//     属性：双槽全等）。技能侧读执行技能静态系别（roundChoice → skills[idx].element，
//     K9 134 同读法；内核 involve 同样用真实系别、不吃"以XX系别结算"视图改写）；
//     使用者侧读属性视图 ws.view_elementalAttributes[owner]（每回合从场面同步，
//     属性反转类改写后读视图才对，785/2492 同源）。
//     DB 实证（2026-11-02 unity 库）：179 全部 18 载体的学习者 type == 技能 type_id
//     （机械能量炮 12981 type6 ← 雷纳尔多 1277/星光·雷纳多 4937；电光骤闪 35515
//     type104 ← 艾斯普鲁 4302；星光旋转 34256 type102 ← 露姆 4060……18/18 无一例外）
//     ——"若属性相同"= 本系（STAB）判据。解析器内建的 SameElement（自身==对手精灵
//     属性，effect_unit.cpp:72 pet-vs-pet）系降级近似，本插件注册后经 clone_effect
//     缓存命中取代之（loadSkills :701 仅 !effect.logic 才走解析器）。
//   - {0} 口径 = **平直 +{0} 点**（口径歧义按 majority reading，记档）：
//     ①官方模板"威力提升{0}"无 % 号——官方带百分比的模板全数明写 {0}%（845/925/1443/
//       1953），带"点"的全数明写（1652/2108）；179 两头都不沾，4399 载体文同。
//     ②痴愚系 13739-13744 + 无相谛 37460 共 7 载体 category=物理 power=0，179 是其
//       唯一威力来源（无相谛官方 power"--" 第五技）——按百分比 0×(1+{0}/100)=0 即
//       死技；平直 +220..470 恰是合理威力面。同一效果 id 不能逐载体二义。
//     ③先例三处同向：内核解析器 PowerBoost +=（effect_unit.cpp:217）、场景 009 断言
//       （无相谛 view==170）、J51 记档标签"[提升点]"。
//     看板行"威力+{0}%"系速记偏差，不采纳（本行即记档）。
//   - 写法 = view += {0}（加法语义不设 view>0 守卫——威力0载体正是平直读法的依据面，
//     845 的 >0 守卫是乘算专属）；无 roll、无窗口，不设审计槽（SKILL_EFFECT 时点字段
//     断言即确定性）。
//   - 无攻击技门：官方模板无"攻击"限定，18 载体全 cat1/2（无属性技载体），与解析器
//     行为面一致（最小改动）。
//
// 1470 ⛔ 内核缺口停手（看板行预案命中）：
//   - 官方模板 args_num=0 零参数无公式；4399 载体文"自身天赋值越高则技能威力越大"
//     无量化（星光旋转 34256/月圆潮汐 35431 实文核对）。
//   - 内核"天赋值"不可读取：monsters 表无 gift/talent 字段（pragma 全列核对）、raw_json
//     无天赋数据、全内核 grep talent/gift 零字段——天赋恒 31 烘焙进面板合成
//     （pet_factory.cpp:583"种族×2+学习力÷4+天赋31"、pet_factory.h:28、
//     official_data_repository.h:228"与天赋恒 31 同理"）。技能选择期（乃至全程）
//     无该字段可读，亦无梯度可言 → 11 载体（简 13745/极 13746/魂系×6/星光旋转/
//     荧光重影击/谕·无畏战车/月圆潮汐/魂·岳鉴诸天）全部记档停手，moves 注册不增。
//
// 动工前置三查（2026-11-02 本 worktree 复核）：
//   ①内核 grep 179/1470：effect_unit_parser.cpp:145 有 179 解析器内建单元（SameElement
//     +PowerBoost），1470 零命中；effect_meta.cpp 无两 id 行（1795 为子串误中已排除）。
//   ②registry moves=1404 无 179/1470（看板行口径）；全分支 grep 无 SKILL_EFFECT(179/1470)。
//   ③⚠️ J51/J88 两代记档"SKILL_EFFECT(179) 插件未获调用/被解析路径遮蔽（实证）"破案
//     （本单 lldb 实证）：短路源不是解析器、是 **custom_effect_programs 表的 179 离线
//     程序行**（2026-10-01 无相谛迁移，unit_json=SameElement+PowerBoost 旧语义）——
//     loadSkills 对 custom_prog 命中的效果 `continue`（skills.cpp :650 段），插件与
//     文本解析器都被旁路。按看板权威判据处置：删该行（新判据技能属性==使用者属性
//     无法用单元 JSON 词汇表达）、重导 custom_effect_seed.sql（已随本单更新）；700/
//     1083/1257/1248/456 五行语义未动。插件注册后 clone_effect 命中缓存即接管分发。
//
// 载体切片（unity 库 json_each 精确匹配 18；side_effect.arg_count=1 顺序切分全自洽）：
//   179 [点数]：{20}机械能量炮12981/{30}仙气环绕32220/{30}幻视迷雾33905/{150}萌喵电刃
//     33567/{60}无情横扫34166/{20}无极海啸34357/{50}锐爪突袭34387/{50}凝冰仙气34602/
//     {40}充能臂甲35407/{100}电光骤闪35515/{170}无相谛37460/{160}地拜·千里一息38271/
//     {220}痴愚13739/{270}谄诳13740/{320}红莲13741/{370}皆苦13742/{420}非天13743/
//     {470}五衰13744。纯链 7 条（12981/32220/33567/33905/34166/34602/35515）。
// 先例：K9 242/733/134（soul_lib/k_cond_power_family.cpp，SKILL_EFFECT 条件改写+字段
//   断言+view 赋值形）、845（j_cond_amp5，威力提升乘算——口径对照面）、calculation.h
//   involve()（本系谓词母本）、J88 施工图（判据技能侧/视图侧读法两处先证）。
// 场景回归：test/scenario/scenario_7550_cond_power_rewrite.cpp（7550 号段，一文件一宏）；
//   同步勘误 scenario_009 A/A' 与 scenario_1780 T1781（两处钉死解析器 pet-vs-pet 旧
//   判据，随本插件改判据后按新口径重钉，详见各文件内注释）。

#include "common.h"

namespace {

// 选中技能下标（-1 = 没选技能）——e4/batch_b 同件口径（roundChoice 选择期写入、跨
// ROUND_START 保留）。
inline int j315_selected_skill_index(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return -1;
    }
    if (ctx->roundChoice[side][0] != 1 /*SELECT_SKILL*/) {
        return -1;
    }
    const int idx = ctx->roundChoice[side][1];
    const int count = static_cast<int>(
        ctx->seerRobot[side].elfPets[ctx->on_stage[side]].skills.size());
    return (idx >= 0 && idx < count) ? idx : -1;
}

// 本系谓词（内核 calculation.h involve() 镜像）：技能单属性 → 使用者任一槽相等；
// 技能双属性 → 双槽全等。使用者侧读属性视图（每回合从场面同步）。
inline bool j315_same_element(const BattleContext* ctx, int owner, const int skill_elem[2]) {
    const int(&view)[2] = ctx->ws.view_elementalAttributes[owner];
    if (skill_elem[1] == 0) {
        return view[0] == skill_elem[0] || view[1] == skill_elem[0];
    }
    return view[0] == skill_elem[0] && view[1] == skill_elem[1];
}

// ── 179 —— 若属性相同则技能威力提升{0}（平直 +{0} 点，见头注口径记档）──────────
// int_args = [owner, target, 点数]（args_num=1，切片 side_effect.arg_count=1）。
EffectResult effect_179_same_element_power_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 179, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 179, args,
                     "179 布局=[owner,target,点数]");
    const int owner = args.int_args[0];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 179, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 179, args, "需要 提升点数 > 0");
    const int idx = j315_selected_skill_index(ctx, owner);
    if (idx < 0) {
        return EffectResult::kOk;
    }
    const Skills& skill = ctx->getPet(owner).skills[static_cast<std::size_t>(idx)];
    if (!j315_same_element(ctx, owner, skill.element)) {
        return EffectResult::kOk;   // 门：技能属性 == 使用者属性（本系判据）
    }
    ctx->ws.skill_power_view[owner] += amount;   // += 置 rewritten → §8.7 收尾重算
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(179, effect_179_same_element_power_up)
