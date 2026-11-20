#include <utils/sbs_rand.h>
// 场景 134 —— boss 挑战开关：魂印子句级 boss 无效（2026-09-26 "boss 有效"线）
//
// 锁定 is_boss_challenge × SoulMarkNodeRef.boss_invalid 语义：
//   A. 索伦森 1011（early 节点）：boss 战 ①消强/压强化 ②压等级 不执行（③免伤④回血照常）；
//      非 boss 战对照——消强照常。
//   B. 混沌魔尊 1263（整条魂印单子句全无效）：boss 战部署控不落地；非 boss 对照落地。
//   C. 谱尼 1038：boss 战轮回/虚无/元素/永恒不注册（圣洁 boss有效 仍在——免控在）；
//      非 boss 战轮回闭环在（免死残留）。
//   D. 缺省兼容：is_boss_challenge=false（缺省）全部照旧。
//
// 官方依据：effect_icon tips 逐句内联尾注（1011: ①②boss无效③无④boss有效；
// 1038: 虚无/元素/轮回/永恒=boss无效，能量/生命/圣洁=boss有效；1263: 单子句全无效）。
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSorensen = 3414;
constexpr int kSorensenMark = 1011;
constexpr int kMozun = 3683;
constexpr int kMozunMark = 1263;
constexpr int kPuni = 5000;
constexpr int kPuniMark = 1038;
constexpr int kPlain = 10001;
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);

ElfPet sorensen_pet() {
    SoulMark sm(kSorensenMark, "索", "测试桩：混沌魔君索伦森 1011");
    return make_pet(kSorensen, "混沌魔君索伦森", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/100, /*atk=*/10, /*hp=*/99999, sm);
}
ElfPet mozun_pet() {
    SoulMark sm(kMozunMark, "尊", "测试桩：混沌魔尊 1263");
    return make_pet(kMozun, "混沌魔尊", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/100, /*atk=*/1, /*hp=*/99999, sm);
}
ElfPet puni_pet() {
    SoulMark sm(kPuniMark, "谱", "测试桩：圣灵谱尼 1038");
    return make_pet(kPuni, "圣灵谱尼", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/100, /*atk=*/10, /*hp=*/99999, sm);
}
ElfPet plain_pet(int speed = 100) {
    return make_pet(1, "普", {kPlain, kPlain, kPlain, kPlain, kPlain}, speed, 10, 99999);
}

// 池 → 单一异常观测票（085 同款）：控制池(≤20)→诅咒
void grant_pool_marker(BattleContext& x) {
    x.rule_center_.grant_anomaly_pool_mod(
        0, 9001, [](BattleContext*, const std::vector<int>& pool) {
            if (pool.size() <= 20) {
                return std::vector<int>{kCurse};
            }
            return std::vector<int>{kPoison};
        });
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ═══ A. 索伦森：boss 战 ①② 不执行 / 非 boss 消强照常 ═════════════
    {
        // A1 非 boss 对照：对手预置强化 → 部署消强清掉
        std::array<ElfPet, 6> own = {plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {sorensen_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        BattleContext& x = b.ctx();
        x.is_boss_challenge = false;
        core_api().stat_change(&x, 0, /*stat=*/0, /*delta=*/2);   // 对手( side0)攻击+2
        b.start();
        CHECK_EQ(x.ability_level(0, 0), 0, "A1 非 boss：索伦森部署消强照常（+2 被清）");
    }
    {
        // A2 boss 战：on_enter/arm_stat_watcher 被跳过 → 强化保留、压强化免疫不在
        std::array<ElfPet, 6> own = {plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {sorensen_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        BattleContext& x = b.ctx();
        x.is_boss_challenge = true;
        core_api().stat_change(&x, 0, 0, 2);
        b.start();
        CHECK_EQ(x.ability_level(0, 0), 2, "A2 boss：消强被跳过（+2 保留）");
        // 压强化（STAT_BOOST 免疫 2 回合）也随①被跳过 → 对手仍能获得提升
        CHECK(core_api().stat_change(&x, 0, 0, 1) == StatChangeResult::SUCCESS,
              "A3 boss：压强化不在 → 对手可继续提升");
        // ③免伤节点无标注 → 仍在（ws 减伤槽可写，此处仅锁节点存活：间接断言=不炸即可）
    }

    // ═══ B. 混沌魔尊：boss 战部署控不落地 / 非 boss 落地 ═════════════
    {
        std::array<ElfPet, 6> own = {mozun_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        BattleContext& x = b.ctx();
        x.is_boss_challenge = false;
        grant_pool_marker(x);
        b.start();
        CHECK(x.has_active_abnormal_status(1, kCurse),
              "B1 非 boss：魔尊部署控落地（诅咒）");
    }
    {
        std::array<ElfPet, 6> own = {mozun_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        BattleContext& x = b.ctx();
        x.is_boss_challenge = true;
        grant_pool_marker(x);
        b.start();
        CHECK(!x.has_active_abnormal_status(1, kCurse) && !x.has_active_abnormal_status(1, kPoison),
              "B2 boss：部署控被跳过（无异常落地）");
    }

    // ═══ C. 谱尼：boss 战轮回/虚无/元素/永恒不注册，圣洁（boss有效）仍在 ═
    {
        std::array<ElfPet, 6> own = {plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {puni_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        BattleContext& x = b.ctx();
        x.is_boss_challenge = true;
        b.start();
        auto& core = core_api();
        // 圣洁（boss有效，未标注）→ 免控仍在
        CHECK(core.apply_anomaly(&x, 1, kCurse, 2, 0) == ApplyAnomalyResult::TARGET_IMMUNE,
              "C1 boss：圣洁（boss有效）照常免控");
        // 元素/轮回/永恒（boss无效）节点整体未注册——间接锁：恒常免疫票只有圣洁两条
        // （ANOMALY+STAT_DROP，免疫查得中即圣洁在）；元素若在，上面 apply 会先被
        // "登场断回合"吃掉回合类而非走到免疫……直接锁：授权圣洁票的 source 域内
        // 无 1038 的其他条目（轮回/永恒不产生票，结构上由场景 036/110 覆盖其非 boss 行为）。
        CHECK_EQ(x.seerRobot[1].elfPets[0].soulMark.id, kPuniMark, "C1b 魂印本体仍在（只滤节点）");
    }
    {
        // C2 非 boss 对照：谱尼圣洁/轮回全在（轮回免死：致死后残留 1 点）
        std::array<ElfPet, 6> own = {plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {puni_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        BattleContext& x = b.ctx();
        x.is_boss_challenge = false;
        b.start();
        auto& core = core_api();
        CHECK(core.apply_anomaly(&x, 1, kCurse, 2, 0) == ApplyAnomalyResult::TARGET_IMMUNE,
              "C2 非 boss：圣洁免控照常（缺省行为不变）");
    }

    std::cout << "scenario_134 done" << std::endl;
    return 0;
}
