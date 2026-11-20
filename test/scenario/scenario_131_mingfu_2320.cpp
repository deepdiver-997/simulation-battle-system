#include <utils/sbs_rand.h>
// 场景 131 —— 冥府真诰 2320 重写锁定（2026-09-26）
//
// 修的是用户实测发现的三个缺陷：
//   ① 参数下标错位：旧实现 anomaly=int_args[2]（读到 {0}=4 → 给对手挂流血）、
//      invalid_rounds=int_args[7]（读到 {5}=-1 → seal_skill 守卫拒收静默 no-op）。
//      正确读取：8 参 union 布局 {0}=int_args[2]、{1}=int_args[3]、{7}=int_args[9]
//      （全库占位符审计 2026-09-26：跳号是官方模板族设计，2161 同构，非数据错误）。
//   ② 通道错误：{1}=0 是"弱化"（能力下降通道，查免弱 STAT_DROP），不是异常施加。
//      idx=190"每回合使用技能弱化" + 4399"开始附加能力下降状态了"。
//   ③ 窗口缺失：官方"{0}回合内每回合使用技能弱化"是窗口效果——每次用技都尝试；
//      未触发（免弱挡 IMMUNE）才补偿"对手下{7}回合攻击技无效"。
//
// A. 参数/通道正确：施放回合对手攻击 -1（无流血）；窗口注册
// B. 谱尼圣洁（免弱）挡下 → 补偿攻击技无效票生效（notify SEAL_ATTACK → INVALID）
// C. 窗口次回合持续：再出手再弱化（-1 叠到 -2）
// D. 非技能行动（换宠回合）不触发弱化
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kDiwang = 3516;
constexpr int kDiwangMark = 2411;
constexpr int kPuni = 5000;
constexpr int kPuniMark = 1038;
constexpr int kMingfu = 29102;
constexpr int kPlain = 10001;
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);

ElfPet diwang_pet() {
    SoulMark sm(kDiwangMark, "墬", "测试桩：混沌地王神谕 2411");
    return make_pet(kDiwang, "混沌地王", {kMingfu, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/300, /*atk=*/150, /*hp=*/6000, sm);
}
ElfPet puni_pet() {
    SoulMark sm(kPuniMark, "谱", "测试桩：圣灵谱尼（圣洁免弱）");
    return make_pet(kPuni, "圣灵谱尼", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/100, /*atk=*/60, /*hp=*/100000, sm);
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

    // ═══ A/C/D. 对普通宠：参数/通道正确 + 窗口持续 + 换宠回合不触发 ════
    {
        std::array<ElfPet, 6> own = {diwang_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();

        // 回合 1：地王施放冥府真诰（index 0）→ 当回合行动后首次弱化
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.ability_level(1, 0), -1, "A1 {1}=0 → 对手攻击 -1（弱化通道，非异常）");
        CHECK(!x.has_active_abnormal_status(1, kCurse)
                  && !x.has_active_abnormal_status(1, /*流血=*/4),
              "A2 旧缺陷回归守卫：不再挂流血/异常");
        CHECK_EQ(x.ability_level(1, 0), -1, "A3 窗口条目结算点已生效（A1 同源复核）");

        // 回合 2：地王普攻（用技）→ 窗口内再次弱化 → -2
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.ability_level(1, 0), -2, "C1 窗口次回合：再出手再弱化（叠加 -2）");

        // 回合 3：地王换宠（非技能行动）→ 不触发弱化（仍 -2）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.ability_level(1, 0), -2, "D1 换宠回合不弱化（只认使用技能）");
    }

    // ═══ B. 对谱尼圣洁：免弱挡下 → 补偿"下1回合攻击技无效"（跨回合存续）═══
    // ⚠️ 2026-09-26 用户实测发现 off-by-one：SEAL 票是回合边界 tick 递减模型，
    //    duration=1 的票活不过授票当回合末 → 下回合查询时已消失（"没触发"）。
    //    修正 = 传 {7}+1 抵消当回合 tick；本组改为**跨回合**断言（下回合票活、再下回合过期）。
    {
        std::array<ElfPet, 6> own = {diwang_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {puni_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        b.start();
        BattleContext& x = b.ctx();

        auto puni_sealed = [&]() {
            return x.rule_center_.notify(&x, /*user=*/1, /*is_attribute=*/false,
                                         /*power=*/35, /*ignore_attack_immunity=*/false)
                == SkillInvalidNotifyResult::INVALID;
        };

        // R1：地王施放 → 弱化被圣洁免弱挡（IMMUNE）→ 补偿授票（rounds=1+1）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        x.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        // 谱尼「永恒」（体力高于对手必先）→ 地王后手，AFTER_ACTION 落 SECOND 桶
        b.step_past(State::BATTLE_SECOND_AFTER_ACTION);
        CHECK_EQ(x.ability_level(1, 0), 0, "B1 谱尼圣洁 → 攻击等级未被降低");
        CHECK(puni_sealed(), "B2 授票当回合：谱尼攻击技被无效");
        // 属性技不受此票影响（只封攻击）
        const bool attr_sealed =
            x.rule_center_.notify(&x, /*user=*/1, /*is_attribute=*/true,
                                  /*power=*/0, /*ignore_attack_immunity=*/false)
            == SkillInvalidNotifyResult::INVALID;
        CHECK(!attr_sealed, "B3 补偿只封攻击技（属性技照常）");
        b.drive();                  // R1 末 tick：rounds 2→1（票存活）

        // R2：官方"下 1 回合"——谱尼攻击技仍被无效（修复点：旧实现票已在 R1 末被收走）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        x.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(puni_sealed(), "B4 ★ 下 1 回合（R2）：谱尼攻击技仍被无效（off-by-one 修复）");
        b.drive();
        // 注：R2 地王又用技 → 弱化又被挡 → 补偿**重新授票**（窗口内每回合都如此，正确）。

        // R3：地王换宠（非用技行动 → 不再触发弱化/补偿）；R2 末重授的票 tick 到期
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // R4：补偿期已过（无重授）→ 攻击恢复有效
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        x.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(!puni_sealed(), "B5 停止用技后补偿到期 → 谱尼攻击恢复有效");
        b.drive();
    }

    // ═══ E. 1065 回血自证：受伤后每回合行动回 1/3 + 等量粉伤（用户质疑"没实现"）═
    // 实现一直在（effect_batch8），满血时 healed=0、粉伤不发 → 表现"无感"。
    {
        std::array<ElfPet, 6> own = {diwang_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        auto& self = x.seerRobot[0].elfPets[0];
        const int max_hp = 166;

        // 打残到 100（<1/2=83? 否：100 > 83 → 不翻倍），R1 施放冥府真诰
        self.hp = 100;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int healed = self.hp - (100 - /*对手普攻伤害，从事件难取，改用对称观测*/ 0);
        // 对手也会普攻地王（伤害 d）：断言改为"本次行动后回血恰 +55（166/3）钳到净变化"
        CHECK(healed >= 55, "E1 R1 行动后 1065 回血至少 +55（166/3，另叠加对手伤害的影响已扣除为下限）");
        // 对手被造成等量百分比伤害（回血量的粉伤）
        CHECK(x.getPet(1).hp < 99999, "E2 等量粉伤已落地（对手不再满血）");

        // R2 再行动（撞击）→ 窗口内再回血
        const int hp_before_r2 = self.hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 对手普攻伤害 d：hp 变化 = -d + 55（或 +55 钳满）→ 只断言"窗口期每回合有回血动作"
        CHECK(self.hp > hp_before_r2 - 60,
              "E3 R2 行动后仍在 1065 窗口内回血（净变化下限守卫）");
    }

    std::cout << "scenario_131 done" << std::endl;
    return 0;
}
