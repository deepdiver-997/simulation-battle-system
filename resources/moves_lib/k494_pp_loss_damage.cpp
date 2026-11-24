// K494 · 2024 双方当前 PP 缺额固伤（2026-11-23）；1564 天赋门缺口不注册。
// 工单 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:99〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets ，当前只读库现查：
// 2024 args_num/arg_count=1/1，载体28416/36668=[10]、36872/37109=[20]；
// 1564两表=2/2，载体28664/34093/34141/34600/34869。九条全链切片均耗尽，伙伴链0。
// 三查：src/include无同号硬编码；effect_meta无覆盖（2024分类Other，1564分类Heal）；
// 两id的custom overrides/programs/selection_routes/combo_rules均0行、无同文异id；
// 全worktree moves/soul无运行时实现，db_patches/tasklog无拆分迁移或退役记录。
// soul同号仅effect_icon数据：1564行1229/pet3964，2024行1637/pet4420，不是moves实现。
//
// 2024短句锚点：“双方当前已损失技能PP值总和”。在SKILL_EFFECT现场只读双方在场
// 精灵各五槽的max(0,maxPP-pp)，不是累计消耗、剩余PP、仅所选槽或全背包。
// pp=-1无限槽不参与；空槽/非正上限同理。PP恢复后缺额立即变小，换宠立即读新宠。
// 〔源: include/entities/skills.h:286-288〕〔源: include/entities/elf-pet.h:198〕
// 〔源: src/primitives/battle_primitives.cpp:1932-1942〕
// 本次正常PP扣费在AFTER_ACTION，故不预扣本次1点；后手会看到先手已扣的PP。
// 〔源: src/fsm/battleFsm.cpp:2105-2109〕〔源: src/fsm/battleFsm.cpp:2247-2251〕
// 倍数沿K443 checked_pct_mul；伤害走deal_pink_damage(FIXED,actor=owner)，吃固定
// 抗性/护罩/免粉，不直写HP、PP，不改红伤倍率；0倍/0缺额无伤害事件。
// 〔源: resources/moves_lib/k443_reflect_taken.cpp:120-126〕
// 一次性无驻留态/审计槽，候选键1015640000/1020240000均未占用，无需soul子块。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:34-45〕
//
// 1564 blocked：实例天赋输入/查询缺失。培养配置与ElfPet无talent字段，面板固定+31；
// 1470仅获批满31特化，不等于存在可测正反的“满天赋门”。不把常量true或私有storage
// 冒充天赋，不只做回血就注册整id。后续需实例天赋兼容输入→宠本体→培养/复制/查询，
// 再以T=30/31门、heal_amount实得、满血/封回血=0、概率0/50/100与镜像验1564。
// 〔源: include/entities/pet_factory.h:30-41〕〔源: include/entities/elf-pet.h:172-213〕
// 〔源: src/entities/pet_factory.cpp:719-725〕
// 〔源: resources/moves_lib/j_1470_talent_power.cpp:3-5〕

#include "common.h"

#include <limits>
#include <numerical-calculation/calculation.h>

namespace {

long long current_pp_loss(const ElfPet& pet) {
    long long lost = 0;
    for (const auto& skill : pet.skills) {
        if (skill.id <= 0 || skill.maxPP <= 0 || skill.pp < 0) {
            continue;
        }
        lost += std::max(0, skill.maxPP - skill.pp);
    }
    return lost;
}

EffectResult effect_2024_current_pp_loss_damage(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2024, args, "需要ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2024, args,
                     "2024布局=[owner,target,倍数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int multiplier = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner < 2 && target >= 0 && target < 2 && owner != target,
                     2024, args, "需要不同的双方");
    EFFECT_ARG_GUARD(multiplier >= 0 && multiplier <= std::numeric_limits<int>::max() / 100,
                     2024, args, "倍数非负且可转为安全百分比乘区");
    if (multiplier == 0 || ctx->getPet(owner).hp <= 0 || ctx->getPet(target).hp <= 0
        || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const long long lost = current_pp_loss(ctx->getPet(owner))
                         + current_pp_loss(ctx->getPet(target));
    EFFECT_ARG_GUARD(lost <= std::numeric_limits<int>::max(), 2024, args, "PP缺额总和超int");
    const int amount = checked_pct_mul(static_cast<int>(lost), multiplier * 100 - 100,
                                       "2024 PP缺额倍率");
    if (amount > 0) {
        (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2024, effect_2024_current_pp_loss_damage)
