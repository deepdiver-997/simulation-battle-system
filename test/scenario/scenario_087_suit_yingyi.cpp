#include <utils/sbs_rand.h>
// 场景 087 —— 套装线端到端：影翼裁决 462（第三类效果来源的首个消费方）
//
// 官方（equip/suit 表 + B站《机制解析—影翼裁决》；装备 1301099-1301102 四件成套）：
//   ① 背包内所有精灵攻击、特攻+60、速度+80（数值层 equip_stat_*，非效果），
//      造成的攻击伤害提升40%（套装桶 ROUND_START 每回合重写 ws.damage_add_pct）
//   ② 回合开始时若自身没有护罩 → 受到的固定/百分比伤害减少5%，体力每降15%+5%，最高25%
//      （粉伤管线 REDUCE_EXTRA 乘法条目，按 plugin_storage 快照标志生效）
//   （登场首回合护罩联动分支未实现——见 resources/suit_lib/yingyi_462.cpp 头注释）
//
// 锁的机制（台账"套装线"行）：
//   A. 数据链：穿戴清单 → resolve_equipment → active_suits（件数≥cloths 全长）
//   B. 内核自动激活：init_battle → activate_suits → 套装桶 TEAM 常驻条目
//      （不依赖场景手动调用——072 时代"手动调"的过渡形态已收口）
//   C. ①：一回合后 ws.damage_add_pct[0]==40；未穿戴方 ==0
//   D. ②：无护罩方吃粉伤按 5% 起减（满血 1000→950）；未穿戴方不减
//
// 生命周期契约：TEAM 常驻、换宠/断回合/伤害抑制都不清（只有 clearAllEffects 清）。

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSkill = 10001;   // 撞击
// 影翼裁决四件（suit 表 cloths 全清单）
constexpr int kPieces[4] = {1301099, 1301100, 1301101, 1301102};

std::array<int, 5> plain_skills() { return {kSkill, kSkill, kSkill, kSkill, kSkill}; }

ElfPet plain_pet(int hp = 4000) {
    return make_pet(1, "普通", plain_skills(), 100, 1, hp);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ════════════════════════════════════════════════════════════════
    // A+B+C+D. 四件成套：数据链 → 自动激活 → 增伤槽 → 无护罩减粉
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<std::vector<int>, 2> equipment{
            std::vector<int>{kPieces[0], kPieces[1], kPieces[2], kPieces[3]},
            std::vector<int>{}};
        ScenarioBattle b(own, opp, {}, {}, equipment);
        b.start();
        BattleContext& x = b.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 0;

        CHECK_EQ(x.active_suits[0].size(), 1, "A1 四件成套 → 激活 1 套");
        CHECK_EQ(x.active_suits[0][0], 462, "A1 套装 = 影翼裁决");
        CHECK(x.active_suits[1].empty(), "A1 未穿戴方无激活套装");

        // ① 增伤槽：ROUND_START 节点每回合重写（跑完一回合读值）
        x.ws.damage_add_pct[0] = 0;
        x.ws.damage_add_pct[1] = 0;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.ws.damage_add_pct[0], 40, "C1 穿戴方通用增伤+40（每回合重写）");
        CHECK_EQ(x.ws.damage_add_pct[1], 0, "C2 未穿戴方不受影响");

        // ② 无护罩减粉：满血 → 5%；未穿戴方不减
        //    （deal_pink_damage 后 final 已落体力：1000 粉 → 扣 950）
        x.seerRobot[0].elfPets[x.on_stage[0]].hp = 4000;   // 满血（无损失 → 5% 档）
        const int hp0_before = x.seerRobot[0].elfPets[x.on_stage[0]].hp;
        (void)deal_pink_damage(&x, 0, 1000, DamageKind::FIXED, 1);
        const int lost0 = hp0_before - x.seerRobot[0].elfPets[x.on_stage[0]].hp;
        CHECK_EQ(lost0, 950, "D1 穿戴方无护罩 → 粉伤 1000×95%（5% 档，乘法向下取整）");

        const int hp1_before = x.seerRobot[1].elfPets[x.on_stage[1]].hp;
        (void)deal_pink_damage(&x, 1, 1000, DamageKind::FIXED, 0);
        const int lost1 = hp1_before - x.seerRobot[1].elfPets[x.on_stage[1]].hp;
        CHECK_EQ(lost1, 1000, "D2 未穿戴方粉伤全额（不减）");

        std::cout << "  [A-D] 影翼裁决成套链路通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E. 未成套（只穿 1 件）：不激活、无增伤槽写入
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<std::vector<int>, 2> equipment{
            std::vector<int>{kPieces[0]}, std::vector<int>{}};
        ScenarioBattle e(own, scenario::make_party(plain_pet()), {}, {}, equipment);
        e.start();
        BattleContext& x = e.ctx();
        CHECK(x.active_suits[0].empty(), "E1 1/4 件 → 不激活");
        x.ws.damage_add_pct[0] = 0;
        e.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        e.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        e.drive();
        CHECK_EQ(x.ws.damage_add_pct[0], 0, "E2 未激活 → 无增伤槽写入");

        std::cout << "  [E] 未成套不激活通过\n";
    }

    std::cout << "scenario_087 全部通过\n";
    return 0;
}
