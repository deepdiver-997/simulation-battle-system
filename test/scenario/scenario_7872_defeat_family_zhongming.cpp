// 场景 7872 —— J324 击败族：349 终命之璨（支援：受援者击败 → 恢复其最大体力 35%）
//
//   349 终命之璨（effect_des 349，宿主=奥菲利娅 4706 魂印 2344 词条）：
//     「被支援者击败对手后，自身为其恢复最大体力的35%」（4 层版 345 同构 30%，
//     注册键撞号记档跳过——见 j_defeat_family.cpp 头注）。
//     支援者=词条持有者（背包存活、不在场），受援者=同队在场精灵（击败者本人），
//     恢复分母=受益者口径（受援者 max_hp，记档）；ROSTER scope 挂载（支援是场下
//     语义，持有者在背包时监听就要活着）。
//
//   A 组 ★ 触发位：持有者背包待命、打击者（受援者）残血击败 → 精确恢复
//         max*35/100（1000 满血 → +350）+ 审计落位
//   B 组 ★ 归因过滤对照：对手击败我方 → 零恢复
//   C 组 ★ 在场门：持有者亲自在场击败 → 不复读（支援是场下语义）
//
// 断言纪律：速度差钉死先手；submit_operation + drive() 有限推进（禁
// while(!FINISHED)）；heal 不含随机 → 恢复量精确断言。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace j324keys {
constexpr int k349AuditBase = 993200110;
}  // namespace j324keys

namespace {

constexpr int kPaiDa = 10004;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

SoulMark mark349() {
    SoulMark sm(349, "J324", "test");
    sm.monster_id = 4706;   // 奥菲利娅
    return sm;
}

// 受援者/打击者：hp=1000（恢复量 = 350 精确可断）。
ElfPet striker_pet(int speed, int atk) {
    ElfPet p = make_pet(11, "打击者", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, atk, /*hp=*/1000);
    accuracy_100(p);
    return p;
}

// 支援者（持有者）：背包待命，拍不死。
ElfPet holder_pet(int speed) {
    ElfPet p = make_pet(4706, "奥菲利娅", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, /*atk=*/1, /*hp=*/99999, mark349());
    accuracy_100(p);
    return p;
}

ElfPet fodder_pet() {
    ElfPet p = make_pet(1, "fodder", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/1);
    accuracy_100(p);
    return p;
}

ElfPet killer_pet() {
    ElfPet p = make_pet(3, "killer", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/500, /*atk=*/999, /*hp=*/99999);
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

constexpr int kMaxHp = 1000;
constexpr int kHeal35 = 350;   // 1000 * 35 / 100

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 触发位：受援者残血击败 → 精确恢复 35% ══
    {
        ElfPet striker = striker_pet(/*speed=*/300, /*atk=*/500);
        ElfPet holder = holder_pet(/*speed=*/1);
        std::array<ElfPet, 6> party{striker, holder, holder, holder, holder, holder};
        std::array<ElfPet, 6> foes{fodder_pet(), striker_pet(1, 1), striker_pet(1, 1),
                                   striker_pet(1, 1), striker_pet(1, 1), striker_pet(1, 1)};
        ScenarioBattle b(std::move(party), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 400;   // 残血 40%（恢复 350 → 750 精确）
        CHECK(ctx.getPet(0).hp == 400, "A 前置：受援者残血 400/1000");

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();

        CHECK(ctx.getPet(1).hp <= 0, "A 前置：对手 fodder 被击倒");
        CHECK_EQ(ctx.getPet(0).hp, 400 + kHeal35,
                 "A ★ 受援者击败 → 恢复其最大体力 35%（+350 精确）");
        CHECK_EQ(read_int_key(ctx, j324keys::k349AuditBase), ctx.roundCount,
                 "A ★ 349 审计槽 = 触发回合");
        std::cout << "  [A] 受援者击败 → +350 精确恢复 + 审计\n";
    }

    // ══ B 归因过滤对照：对手击败我方 → 零恢复 ══
    {
        ElfPet striker = striker_pet(/*speed=*/1, /*atk=*/1);
        striker.hp = 1;   // 被对手先手拍死
        ElfPet holder = holder_pet(1);
        std::array<ElfPet, 6> party{striker, holder, holder, holder, holder, holder};
        std::array<ElfPet, 6> foes{killer_pet(), striker_pet(1, 1), striker_pet(1, 1),
                                   striker_pet(1, 1), striker_pet(1, 1), striker_pet(1, 1)};
        ScenarioBattle b(std::move(party), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // killer 出手（NONE=跳过出手）
        b.drive();

        CHECK(ctx.getPet(0).hp <= 0, "B 前置：我方首发被对手击倒");
        CHECK_EQ(read_int_key(ctx, j324keys::k349AuditBase), 0,
                 "B ★ 对手击败（actor=1≠0）→ 零恢复（归因过滤）");
        std::cout << "  [B] 归因过滤对照 → 零触发\n";
    }

    // ══ C 在场门：持有者亲自在场击败 → 不复读 ══
    {
        ElfPet holder = holder_pet(/*speed=*/300);   // 持有者首发且出手
        std::array<ElfPet, 6> party{holder, striker_pet(1, 1), striker_pet(1, 1),
                                    striker_pet(1, 1), striker_pet(1, 1), striker_pet(1, 1)};
        std::array<ElfPet, 6> foes{fodder_pet(), striker_pet(1, 1), striker_pet(1, 1),
                                   striker_pet(1, 1), striker_pet(1, 1), striker_pet(1, 1)};
        ScenarioBattle b(std::move(party), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 400;   // 持有者自己残血（她在场=无支援者→无恢复）

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();

        CHECK(ctx.getPet(1).hp <= 0, "C 前置：持有者击败 fodder");
        CHECK_EQ(ctx.getPet(0).hp, 400,
                 "C ★ 持有者在场（无场下支援者）→ 不复读、零恢复");
        CHECK_EQ(read_int_key(ctx, j324keys::k349AuditBase), 0, "C ★ 审计槽不动");
        std::cout << "  [C] 在场门 → 不复读\n";
    }

    std::cout << "scenario_7872 PASS (J324 349 终命之璨)" << std::endl;
    return 0;
}
