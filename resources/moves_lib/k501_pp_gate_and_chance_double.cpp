// K501 · 1555 无提升全槽PP减 / 1710 掷点伤害翻倍（2026-11-23）。
// 工单 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:106〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets ，本区只读库双表现查：
// 1555 args_num/arg_count=1/1，27880柱神祈愿[48,1555,780]=[4,1,3,100,1]、
// 34056娇影笑春风[913,514,1555]=[100,25,1]、34121圣浴之耀[1555]=[1]、
// 34691虚弱魔咒[1555]=[1]（四载体切片全 arg=1，纯链×2）；
// 1710 args_num/arg_count=1/1，35264乌天魂风祭[1124,1710,1206]=[2,100,50,20,10,40]、
// 36764引光灵息[1710]=[50]、36918澹拂花漫径[1710,1106,989]=[50,320,100,8,320,300]、
// 36976急火飞奔[1710]=[50]（四载体切片全 arg=50，纯链×2）。八链零剩余，friend 链无两 id。
// 三查：src/include 无同号行为硬编码；custom_effect_overrides/programs/
// selection_routes/combo_rules 均无行；废弃 parser 无两模板覆盖（"降低对手所有PP"
// 是 700 句式，非本族"所有技能PP值"；无"概率伤害翻倍"规则）→ 注册前完全惰性。
// soul 同号仅 effect_icon 数据行（1555=icon1196/pet3956、1710=icon1328/pet4120，
// kind=[5,11]/[5,14] 魂印索引，文案语义与技能族无关），无运行时实现 → 无键段
// 交叠免子块（K481 728 判据）。db_patches/tasklog 无拆分迁移/退役记录
// （tasklog "1710-1719" 是 J75 场景号段 alloc，与效果 id 无关）。
//
// ══ 1555 短句锚点："对手不处于能力提升时令对手所有技能PP值-{0}" ══
// 门甄别 = 对手 ability_levels 六槽**存储面**任一正槽（K490 2171 翻倍门直系，
// j_gaia_stance 779 同面旁证；不读 ws.view_levels——同链视图手术不擦存储，
// 存储面判别是本族既定口径）。"所有技能"= 对手在场宠五槽全槽。
// 落点 = pp_reduce 正式原语（全槽减、钳 0、无限 PP 槽 -1 跳过、逐实变槽
// EVENT_PP_REDUCED；core_api.h:122 直系）。amount=0 走原语自然零点（INVALID_PARAM
// 无变化，不跳过登记，K490 零级数同款）。
// 时点 = SKILL_EFFECT 命中趟（HIT 分支；miss/被盔走 SKILL_INVALID 分支结构性
// 不触发，T8782 同款；属性技也算命中——27880 cat4 载体有效）。
// 〔源: include/plugin/core_api.h:122-130〕
// 〔源: src/primitives/battle_primitives.cpp:1906-1932〕
// 〔源: src/entities/skills.cpp:1240〕
//
// ══ 1710 短句锚点："{0}%的概率造成伤害翻倍，对手不处于异常状态时概率翻倍" ══
// 掷点 = 本地 roll_percent（0/100 短路不耗 rand）。**无申报闸门通道**：ChanceTag
// 仅 AnomalyAttach（rule_center.h:99-101），本效果不施加异常、伤害翻倍无闸门语义，
// K456 头注同款口径记档。
// 概率翻倍门 = 对手**当前异常判读**：全合法码 has_active_anomaly 任一命中 → 不翻倍
// （概率保持 {0}），全不命中 → 概率 ×2（K491 2204 对手异常判据直系：含附属类、
// 吃 K6 虚拟异常视图遮蔽，不缩成控制/弱化类、不从本体槽绕过）。
// 翻倍面 = SkillView 效果侧赋值通道（当前视图 ×2 并置 rewritten，非伤害 AMP、
// 非静态改值）：一次性命中修正，沿 K491 2204 不照搬 K489 跨回合双桶窗。
// 〔源: resources/moves_lib/k489_power_double_window.cpp:39-44〕
// 〔源: resources/moves_lib/k491_conditional_power.cpp:55-71〕
// 〔源: resources/moves_lib/k493_anomaly_round_amp.cpp:71-83〕
// 〔源: include/fsm/battleWorkspace.h:80-95〕
//
// ══ 审计槽（plugin_storage，K490 布局同构：10^9 + id×10^4 + owner×16 + slot）══
// 1555（1015550000）：slot 0 行参 amount / 1 提升门（1=对手有提升·跳过 / 0=门开）
//   / 2 fire 计数（调用原语）/ 3 本刀实际总扣减（逐槽前后差合计，-1 无限槽不计）
//   / 8 进入计数（SKILL_EFFECT 入口 bump，miss/被盔=0）/ 9 跳过原因
//   （0=无 / 1=对手有提升 / 2=目标已倒 / 3=缺原语）
// 1710（1017100000）：slot 0 行参 percent / 1 异常门（1=对手有异常·不翻倍 /
//   0=无异常·翻倍）/ 2 有效概率（{0} 或 2×{0}）/ 3 掷点结果（1=翻倍 / 0=掷歪）
//   / 8 进入计数（非攻击技不 bump）/ 9 跳过原因（0=无 / 1=非攻击技 /
//   2=威力未物化 / 3=缺原语）
// 新增键 1015550000/1017100000 全库 grep 零命中（2026-11-23 现查）；审计槽为
// 一次性效果的过程取证（非驻留显示态），随 ctx 析构，不新增 G4 债务。
//
// ══ 离场清理 ══
// 两效果均一次性（SKILL_EFFECT 内联执行完毕即无痕）：无 duration 桶/票/watcher/
// 驻留存储语义，plugin_storage 审计键是战斗账本（对局级）。一次性无驻留显示态，
// 不造空描述/伪窗（显示 gate 规范 §6 #2）。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:39〕

#include "common.h"

#include <limits>
#include <numerical-calculation/calculation.h>

namespace {

constexpr int k1555AuditBase = 1015550000;  // 10^9 + 1555×10^4
constexpr int k1710AuditBase = 1017100000;  // 10^9 + 1710×10^4

inline int audit_key(int base, int owner, int slot) { return base + owner * 16 + slot; }

int storage_int_local(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

void set_storage_int_local(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

void bump_use_count(BattleContext* ctx, int base, int owner) {
    const int key = audit_key(base, owner, 8);
    set_storage_int_local(ctx, key, storage_int_local(ctx, key) + 1);
}

// 对手是否处于能力提升：ability_levels 六槽存储面任一正槽（2171 翻倍门同面）。
bool has_any_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < 6; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// 对手是否处于异常：全合法码 has_active_anomaly（2204/2137 同判据）。
bool has_any_anomaly(BattleContext* ctx, int side) {
    if (!g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, side, id)) {
            return true;
        }
    }
    return false;
}

// 在场宠五槽 PP 合计（-1 无限槽不计；K494 2024 同款读法）。
int total_pp(const ElfPet& pet) {
    int sum = 0;
    for (const auto& skill : pet.skills) {
        if (skill.id <= 0 || skill.maxPP <= 0 || skill.pp < 0) {
            continue;
        }
        sum += skill.pp;
    }
    return sum;
}

EffectResult effect_1555_pp_gate_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1555, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1555, args,
                     "1555 布局=[owner,target,PP减量]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target == 1 - owner, 1555, args,
                     "需要 owner/target 为对向双方");
    EFFECT_ARG_GUARD(amount >= 0, 1555, args, "需要 PP 减量非负");
    bump_use_count(ctx, k1555AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k1555AuditBase, owner, 0), amount);
    if (ctx->getPet(owner).hp <= 0 || ctx->getPet(target).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k1555AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->pp_reduce) {
        set_storage_int_local(ctx, audit_key(k1555AuditBase, owner, 9), 3);
        return EffectResult::kOk;
    }
    if (has_any_boost(ctx, target)) {
        set_storage_int_local(ctx, audit_key(k1555AuditBase, owner, 1), 1);
        set_storage_int_local(ctx, audit_key(k1555AuditBase, owner, 9), 1);
        return EffectResult::kOk;  // 对手处于能力提升：整句门闭，不扣 PP。
    }
    set_storage_int_local(ctx, audit_key(k1555AuditBase, owner, 1), 0);
    const int before = total_pp(ctx->getPet(target));
    const PpReduceResult rc = g_core->pp_reduce(ctx, target, amount);
    const int after = total_pp(ctx->getPet(target));
    set_storage_int_local(ctx, audit_key(k1555AuditBase, owner, 3), before - after);
    set_storage_int_local(ctx, audit_key(k1555AuditBase, owner, 9), 0);
    if (rc == PpReduceResult::SUCCESS) {
        set_storage_int_local(ctx, audit_key(k1555AuditBase, owner, 2),
                              storage_int_local(ctx, audit_key(k1555AuditBase, owner, 2)) + 1);
    }
    return EffectResult::kOk;
}

EffectResult effect_1710_chance_power_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1710, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1710, args,
                     "1710 布局=[owner,target,概率%]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int percent = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target == 1 - owner, 1710, args,
                     "需要 owner/target 为对向双方");
    EFFECT_ARG_GUARD(percent >= 0 && percent <= std::numeric_limits<int>::max() / 2, 1710, args,
                     "需要 概率非负且翻倍不溢出");
    if (!executed_skill_is_attack(ctx, owner)) {
        set_storage_int_local(ctx, audit_key(k1710AuditBase, owner, 9), 1);
        return EffectResult::kOk;  // 属性技/非攻击语境：无"造成伤害"可翻。
    }
    if (!g_core || !g_core->has_active_anomaly) {
        set_storage_int_local(ctx, audit_key(k1710AuditBase, owner, 9), 3);
        return EffectResult::kOk;  // 缺判读原语：不掷点（避免误翻倍通道）。
    }
    bump_use_count(ctx, k1710AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k1710AuditBase, owner, 0), percent);
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value < 0) {
        set_storage_int_local(ctx, audit_key(k1710AuditBase, owner, 9), 2);
        return EffectResult::kOk;  // -1 尚未物化；0 是合法攻击威力，仍按契约掷点。
    }
    const bool foe_anomaly = has_any_anomaly(ctx, target);
    const int effective = foe_anomaly ? percent : percent * 2;
    set_storage_int_local(ctx, audit_key(k1710AuditBase, owner, 1), foe_anomaly ? 1 : 0);
    set_storage_int_local(ctx, audit_key(k1710AuditBase, owner, 2), effective);
    const bool hit = roll_percent(effective);
    set_storage_int_local(ctx, audit_key(k1710AuditBase, owner, 3), hit ? 1 : 0);
    set_storage_int_local(ctx, audit_key(k1710AuditBase, owner, 9), 0);
    if (hit) {
        view = checked_pct_mul(view.value, 100, "effect_1710");  // ×2，并置 rewritten。
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1555, effect_1555_pp_gate_reduce)
SKILL_EFFECT(1710, effect_1710_chance_power_double)
