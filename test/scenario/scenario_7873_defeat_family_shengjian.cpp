// 场景 7873 —— J324 击败族：380 圣剑重铸（己方击败 → 至序圣华复活）
//
//   380 圣剑重铸（effect_des 380，宿主=至序圣华 4321 魂印 1897 / 圣律剑灵 4400
//   魂印 1972 系统）：「己方在场精灵所有技能先制+1；己方击败对手时，至序圣华复活；
//   回合开始时，若己方没有圣剑，则圣剑重铸结束」
//   本单击败族条款 = 己方击败对手 → 扫己方 6 槽复活至序圣华（monster_id==4321，
//   hp<=0、未消逝）满血（文本无 hp 锚 → k_death_revenge 132 先例口径，记档）。
//   持有者不在场不设门（她可能就是被复活的死者本人）；ROSTER scope（监听须在她
//   死亡期间活着）。主体先制+1/圣剑物件=主魂印范围（记档）。
//
//   A 组 ★ 触发位：至序圣华首发被杀 → 队友击败对手 → 精确复活（hp==max）+ 审计
//   B 组 ★ 归因对照：对手连续击败我方 → 圣华保持死亡（零复活）
//   C 组 ★ 无死亡门：圣华活着时己方击败 → 扫槽跳过（hp>0 门）零动作
//
// 断言纪律：速度差钉死先手；submit_operation + drive() 有限推进（禁
// while(!FINISHED)）；复活不涉随机 → hp 精确断言。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace j324keys {
constexpr int k380AuditBase = 993200130;
}  // namespace j324keys

namespace {

constexpr int kPaiDa = 10004;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

SoulMark mark380() {
    SoulMark sm(380, "J324", "test");
    sm.monster_id = 4321;   // 至序圣华（持有者=她本人；复活扫描按 monster_id 找）
    return sm;
}

// 至序圣华（词条持有者）：hp 参数可调（A 组脆皮 / C 组坦）。
ElfPet shenghua(int hp) {
    ElfPet p = make_pet(4321, "至序圣华", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/1, /*atk=*/1, hp, mark380());
    accuracy_100(p);
    return p;
}

// 队友打击者：无词条。
ElfPet striker_pet(int speed, int atk) {
    ElfPet p = make_pet(11, "打击者", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, atk, /*hp=*/99999);
    accuracy_100(p);
    return p;
}

// 对手杀手（A 组：hp=1 会被反杀；B 组：坦）。speed 500 先手。
ElfPet killer_pet(int hp) {
    ElfPet p = make_pet(3, "killer", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/500, /*atk=*/999, hp);
    accuracy_100(p);
    return p;
}

ElfPet fodder_pet() {
    ElfPet p = make_pet(1, "fodder", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/1);
    accuracy_100(p);
    return p;
}

int read_int_key(const BattleContext& ctx, int key) {
    auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return 0;   // any_cast 指针形式：类型不符按缺失回退（全库 any_cast 治理口径）
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 触发位：圣华被杀 → 队友击败 → 精确复活 ══
    {
        ElfPet sh = shenghua(/*hp=*/1);   // 首发即倒
        ElfPet striker = striker_pet(/*speed=*/300, /*atk=*/500);
        std::array<ElfPet, 6> party{sh, striker, striker, striker, striker, striker};
        std::array<ElfPet, 6> foes{killer_pet(/*hp=*/1), striker_pet(1, 1), striker_pet(1, 1),
                                   striker_pet(1, 1), striker_pet(1, 1), striker_pet(1, 1)};
        ScenarioBattle b(std::move(party), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // killer 出手（NONE=跳过出手）
        b.drive();
        CHECK(ctx.getPet(0).hp <= 0, "A 前置：R1 至序圣华被对手先手击倒");
        CHECK(read_int_key(ctx, j324keys::k380AuditBase) == 0,
              "A 前置：对手击败（actor=1）→ 不复活");

        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 死切打击者（我方选宠）
        b.drive();

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0); // R2 拍杀 killer(hp=1)
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp <= 0, "A 前置：R2 队友击倒 killer");
        // 圣华在 slot 0（死切后 on_stage 已切打击者 slot 1，getPet(0) 读的是打击者）。
        CHECK(ctx.seerRobot[0].elfPets[0].hp > 0,
              "A ★ 己方击败对手 → 至序圣华复活（hp>0）");
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 1,
                 "A ★ 复活满血（文本无 hp 锚 → 复活先例满血口径，max=1 精确）");
        CHECK_EQ(read_int_key(ctx, j324keys::k380AuditBase), ctx.roundCount,
                 "A ★ 380 审计槽 = 复活回合");
        std::cout << "  [A] 队友击败 → 至序圣华精确复活\n";
    }

    // ══ B 归因对照：对手连续击败 → 圣华保持死亡 ══
    {
        ElfPet sh = shenghua(/*hp=*/1);
        ElfPet striker = striker_pet(/*speed=*/1, /*atk=*/1);
        striker.hp = 1;   // 死切替补也是脆皮（killer R2 再杀）
        std::array<ElfPet, 6> party{sh, striker, striker, striker, striker, striker};
        std::array<ElfPet, 6> foes{killer_pet(/*hp=*/99999), striker_pet(1, 1), striker_pet(1, 1),
                                   striker_pet(1, 1), striker_pet(1, 1), striker_pet(1, 1)};
        ScenarioBattle b(std::move(party), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // killer 出手（NONE=跳过出手）
        b.drive();
        CHECK(ctx.getPet(0).hp <= 0, "B 前置：R1 圣华被杀");

        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 我方死切选替补
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // killer 出手（NONE=跳过出手）
        b.drive();
        CHECK(ctx.getPet(0).hp <= 0, "B 前置：R2 替补也被 killer 杀（对手两连击败）");
        CHECK_EQ(read_int_key(ctx, j324keys::k380AuditBase), 0,
                 "B ★ 对手击败（actor=1≠0）→ 圣华保持死亡（归因过滤）");
        std::cout << "  [B] 归因对照 → 零复活\n";
    }

    // ══ C 无死亡门：圣华活着时己方击败 → 扫槽跳过 ══
    {
        ElfPet sh = shenghua(/*hp=*/99999);   // 坦度圣华背包待命（hp>0 门直接挡）
        // 圣华背包待命：首发=打击者（无词条），她存活于 slot1。
        ElfPet striker = striker_pet(/*speed=*/400, /*atk=*/500);
        std::array<ElfPet, 6> party{striker, sh, striker, striker, striker, striker};
        std::array<ElfPet, 6> foes{fodder_pet(), striker_pet(1, 1), striker_pet(1, 1),
                                   striker_pet(1, 1), striker_pet(1, 1), striker_pet(1, 1)};
        ScenarioBattle b(std::move(party), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp <= 0, "C 前置：己方（打击者首发）击败 fodder");
        CHECK(ctx.getPet(0).hp > 0 && ctx.seerRobot[0].elfPets[1].hp > 0,
              "C 前置：圣华活着（背包 slot1）");
        CHECK_EQ(read_int_key(ctx, j324keys::k380AuditBase), 0,
                 "C ★ 圣华活着（hp>0 门）→ 扫槽跳过零动作");
        std::cout << "  [C] 无死亡门 → 零动作\n";
    }

    std::cout << "scenario_7873 PASS (J324 380 圣剑重铸)" << std::endl;
    return 0;
}
