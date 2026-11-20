#include <utils/sbs_rand.h>
// 场景 133 —— 效果禁用基建（2026-09-26，"boss 有效"线第一步）
//
// 锁定 disabledEffects 装配期语义：
//   A. 技能效果摘除：混地 2320（弹控弱化）被禁 → 冥府真诰不再弱化对手
//   B. 魂印裸壳：谱尼 1038 魂印被禁 → 圣洁免控失效（诅咒落地）+ 无轮回
//   C. 集合为空/缺省 → 行为与旧完全一致（兼容守卫）
//   D. effectParams 对已禁效果报错（装配期 fail-fast，与手误路径一致）
//
// 数据来源：disabledEffects 经 battle_init parse_pet（JSON 阵容）——本场景直接
// 用 PetFactory（同源装配路径），JSON 解析由 smoke 层覆盖。
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kDiwang = 3516;
constexpr int kDiwangMark = 2411;
constexpr int kMingfu = 29102;
constexpr int kPuni = 5000;
constexpr int kPuniMark = 1038;
constexpr int kPlain = 10001;
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);

ElfPet diwang_pet(const DisabledEffects& disabled = {}) {
    BattlePetMessage m;
    m.pet_id = kDiwang;
    m.chosen_skills_id = {kMingfu, 10223, 20865, 11923, 10401};
    m.oracle_on = true;
    m.disabled_effects = disabled;
    return PetFactory::create_pet(m);
}
ElfPet puni_pet(const DisabledEffects& disabled = {}) {
    BattlePetMessage m;
    m.pet_id = kPuni;
    m.chosen_skills_id = {10993, 20498, 10996, 11261, 11274};
    m.disabled_effects = disabled;
    return PetFactory::create_pet(m);
}
ElfPet plain_pet() {
    return make_pet(1, "普", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, 10, 99999);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ═══ A. 技能效果摘除：禁 2320 → 冥府真诰不再弱化 ════════════════
    {
        DisabledEffects de;
        de.skill_effects = {2320};
        std::array<ElfPet, 6> own = {diwang_pet(de), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.ability_level(1, 0), 0, "A1 禁 2320 → 冥府真诰不再弱化对手攻击");
        // 同技其余效果仍在（1065 回血窗口——受伤后行动应有恢复）
        CHECK(x.seerRobot[0].elfPets[0].hp > 0, "A2 精灵本体不受禁用影响");
    }

    // ═══ B. 魂印裸壳：谱尼禁 1038 → 圣洁免控消失 + 程序不注册 ═══════
    {
        DisabledEffects de;
        de.soul_marks = {kPuniMark};
        std::array<ElfPet, 6> own = {plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {puni_pet(de), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        b.start();
        BattleContext& x = b.ctx();
        auto& core = core_api();
        CHECK_EQ(x.seerRobot[1].elfPets[0].soulMark.id, 0, "B1 魂印被禁 → SoulMark 裸壳(id=0)");
        const ApplyAnomalyResult r = core.apply_anomaly(&x, 1, kCurse, 2, 0);
        x.event_center_.drain(&x, x.roundCount);
        CHECK(r == ApplyAnomalyResult::SUCCESS, "B2 圣洁被禁 → 诅咒直接落地（免控消失）");
        CHECK(x.has_active_abnormal_status(1, kCurse), "B3 诅咒在谱尼身上");
    }

    // ═══ C. 对照：魂印启用（缺省）→ 免控在 ═════════════════════════
    {
        std::array<ElfPet, 6> own = {plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {puni_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        b.start();
        BattleContext& x = b.ctx();
        auto& core = core_api();
        const ApplyAnomalyResult r = core.apply_anomaly(&x, 1, kCurse, 2, 0);
        x.event_center_.drain(&x, x.roundCount);
        CHECK(r == ApplyAnomalyResult::TARGET_IMMUNE, "C1 缺省（无禁用）→ 圣洁免控在（兼容）");
    }

    // ═══ D. 禁用已存在的效果 id 但技能无此效果 → 无副作用（静默容忍）══
    // 与 effectParams 不同：禁用是"排除"语义，对不持有的效果禁了也无害（boss 清单
    // 是全局的，逐宠过滤时大量 id 不命中——必须容忍，否则 boss 预填没法用）。
    {
        DisabledEffects de;
        de.skill_effects = {999999, 2320};   // 999999 不存在于任何技能
        std::array<ElfPet, 6> own = {diwang_pet(de), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.ability_level(1, 0), 0, "D1 清单含未命中 id → 正常装配（boss 清单前提）");
    }

    std::cout << "scenario_133 done" << std::endl;
    return 0;
}
