// 场景 7870 —— J324 击败族：187 永昌玺（方数归零）+ 201 凯旋预言（击败回满）
//
//   187 永昌玺（effect_des 187，宿主=始皇帝·嬴政 4468 魂印 2057 词条）：
//     「每次在场首次在回合开始达到6方时，开启君临天下；击败对手后方数归0」
//     本单击败族条款 = 击败对手 → 方数槽归 0（plugin_storage k187FangBase，主魂印
//     2057 同键对账）+ 审计槽记归零回合。攒方/君临天下主体=主魂印范围（记档）。
//   201 凯旋预言（effect_des 201，宿主=天后·弗丽嘉 4498 魂印 2098 词条）：
//     「持有精灵下次击败对手时恢复自身全部体力和所有技能PP值，下场保留」
//     = 挂载即武装（armed 旗写持有者 soulmark_storage——"下场保留"字面），击败
//     消费（回满血 + 全 PP 满）后 consumed 旗置位、不再自动重武装（记档）。
//
//   A 组 ★ 187 触发位：预置方数=5 → 击败 → 槽精确归 0 + 审计=当回合
//   B 组 ★ 187 归因过滤对照：对手击败我方（actor=1≠0）→ 方数保持 5、审计不动；
//         死切补位后的非击败回合同样零触发
//   C 组 ★ 187 boss 门：is_boss_challenge=true（主魂印"所有特效boss无效"罩词条
//         的回调双保险，K391 口径）→ 击败不归零
//   D 组 ★ 201 触发位：残血+缺 PP 状态击败 → 体力精确回满 + PP 全满 + armed 消费；
//         二次击败不再回满（一次性）；armed 旗落位=soulmark_storage（下场保留字面）
//
// 断言纪律：速度差钉死先手；全部 submit_operation + drive() 有限推进（禁
// while(!FINISHED)）；无概率分支（heal/PP 恢复不含随机；伤害浮动不进断言面）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

// 与 j_defeat_family.cpp 一致的键段（插件内 constexpr，场景按同值对账——
// 键是插件私有约定，场景只做只读断言/预置，改动即回归红）。
namespace j324keys {
constexpr int k187FangBase = 993200010;
constexpr int k187AuditBase = 993200020;
constexpr int k201ArmedKey = 993200040;
constexpr int k201ConsumedKey = 993200050;
constexpr int k201AuditBase = 993200060;
}  // namespace j324keys

namespace {

constexpr int kPaiDa = 10004;   // 拍打（物理攻击，纯载体）

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

SoulMark mark_of(int id, int monster_id) {
    SoulMark sm(id, "J324", "test");
    sm.monster_id = monster_id;
    return sm;
}

// 187 持有者（嬴政位）：击杀版 atk=500。
ElfPet holder187(int atk) {
    ElfPet p = make_pet(4468, "嬴政", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/99999, mark_of(187, 4468));
    accuracy_100(p);
    return p;
}

// 201 持有者（弗丽嘉位）：hp=1000 让回满可精确断言。
ElfPet holder201() {
    ElfPet p = make_pet(4498, "弗丽嘉", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/500, /*hp=*/1000, mark_of(201, 4498));
    accuracy_100(p);
    return p;
}

// 对方 fodder：hp=1（一击必杀）/ mate：hp=99999（拍不死）。
ElfPet fodder_pet() {
    ElfPet p = make_pet(1, "fodder", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/1);
    accuracy_100(p);
    return p;
}
ElfPet mate_pet() {
    ElfPet p = make_pet(2, "mate", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p);
    return p;
}
// B 组对手杀手：先手（speed 500）+ 高攻。
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

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 187 触发位：预置方数 5 → 击败 → 归 0 + 审计 ══
    {
        ElfPet me = holder187(500);
        std::array<ElfPet, 6> foes{fodder_pet(), mate_pet(), mate_pet(),
                                   mate_pet(), mate_pet(), mate_pet()};
        ScenarioBattle b(make_party(std::move(me)), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[j324keys::k187FangBase] = 5;   // 模拟主魂印 2057 已攒 5 方
        CHECK_EQ(read_int_key(ctx, j324keys::k187FangBase), 5, "A 前置：方数槽已预置 5");

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();

        CHECK(ctx.getPet(1).hp <= 0, "A 前置：对手 fodder 被击倒");
        CHECK_EQ(read_int_key(ctx, j324keys::k187FangBase), 0,
                 "A ★ 187 击败对手 → 方数槽精确归 0");
        CHECK_EQ(read_int_key(ctx, j324keys::k187AuditBase), ctx.roundCount,
                 "A ★ 187 审计槽 = 击败发生回合");
        std::cout << "  [A] 187 击败 → 方数归 0 + 审计落位\n";
    }

    // ══ B 187 归因过滤对照：对手击败我方 → 不触发 ══
    {
        ElfPet me = holder187(/*atk=*/1);
        me.hp = 1;   // 首发即倒（对手先手拍死）
        ElfPet bench = make_pet(9, "bench", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                /*speed=*/1, /*atk=*/1, /*hp=*/99999);
        accuracy_100(bench);
        std::array<ElfPet, 6> party{me, bench, mate_pet(), mate_pet(), mate_pet(), mate_pet()};
        std::array<ElfPet, 6> foes{killer_pet(), mate_pet(), mate_pet(),
                                   mate_pet(), mate_pet(), mate_pet()};
        ScenarioBattle b(std::move(party), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[j324keys::k187FangBase] = 5;

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // killer 出手（NONE=跳过出手）
        b.drive();

        CHECK(ctx.getPet(0).hp <= 0, "B 前置：我方首发（187 持有者）被击倒");
        CHECK_EQ(read_int_key(ctx, j324keys::k187FangBase), 5,
                 "B ★ 对手击败我方（actor=1≠0）→ 方数不归零（归因过滤）");
        CHECK_EQ(read_int_key(ctx, j324keys::k187AuditBase), 0,
                 "B ★ 审计槽不动（无归因击败）");

        // 死切补位（死的是我方首发 → 我方选宠）+ 非击败回合：零触发
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp > 0 && ctx.getPet(0).hp > 0,
              "B 前置：死切后双方存活（非击败回合）");
        CHECK_EQ(read_int_key(ctx, j324keys::k187FangBase), 5,
                 "B ★ 非击败回合：方数槽保持 5");
        std::cout << "  [B] 187 归因过滤对照 + 非击败回合 → 零触发\n";
    }

    // ══ C 187 boss 门：is_boss_challenge → 双保险挡 ══
    {
        ElfPet me = holder187(500);
        std::array<ElfPet, 6> foes{fodder_pet(), mate_pet(), mate_pet(),
                                   mate_pet(), mate_pet(), mate_pet()};
        ScenarioBattle b(make_party(std::move(me)), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.is_boss_challenge = true;
        ctx.plugin_storage[j324keys::k187FangBase] = 5;

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();

        CHECK(ctx.getPet(1).hp <= 0, "C 前置：boss 门下击败照常发生");
        CHECK_EQ(read_int_key(ctx, j324keys::k187FangBase), 5,
                 "C ★ boss 挑战（主魂印 2057 boss 无效尾注罩词条）→ 方数不归零");
        CHECK_EQ(read_int_key(ctx, j324keys::k187AuditBase), 0, "C ★ 审计槽不动");
        std::cout << "  [C] 187 boss 门 → 零触发\n";
    }

    // ══ D 201 触发位：残血缺 PP 击败 → 回满 + 一次性消费 ══
    {
        ElfPet me = holder201();
        const int max_hp = me.numericalBase[NumericalPropertyIndex::HP];
        std::array<ElfPet, 6> foes{fodder_pet(), mate_pet(), mate_pet(),
                                   mate_pet(), mate_pet(), mate_pet()};
        ScenarioBattle b(make_party(std::move(me)), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();
        // 挂载即武装：armed 旗已写持有者 soulmark_storage（下场保留字面）。
        auto& st = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].soulmark_storage;
        CHECK(st.count(j324keys::k201ArmedKey) == 1,
              "D ★ 201 挂载即武装：armed 旗落在持有者 soulmark_storage（下场保留）");
        // 残血 + 缺 PP 预置。
        ctx.getPet(0).hp = 400;
        ctx.getPet(0).skills[0].pp = std::max(0, ctx.getPet(0).skills[0].pp - 2);
        const int pp_before = ctx.getPet(0).skills[0].pp;
        CHECK(ctx.getPet(0).hp == 400 && pp_before < ctx.getPet(0).skills[0].maxPP,
              "D 前置：残血 400 + skills[0] 缺 PP");

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();

        CHECK(ctx.getPet(1).hp <= 0, "D 前置：对手 fodder 被击倒");
        CHECK_EQ(ctx.getPet(0).hp, max_hp, "D ★ 201 击败 → 体力精确回满");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, ctx.getPet(0).skills[0].maxPP,
                 "D ★ 201 击败 → skills[0] PP 恢复满");
        for (std::size_t i = 1; i < ctx.getPet(0).skills.size(); ++i) {
            CHECK_EQ(ctx.getPet(0).skills[i].pp, ctx.getPet(0).skills[i].maxPP,
                     "D ★ 201 击败 → 所有技能 PP 满");
        }
        CHECK(st.count(j324keys::k201ArmedKey) == 0 && st.count(j324keys::k201ConsumedKey) == 1,
              "D ★ armed 消费 → consumed 置位（一次性）");
        CHECK_EQ(read_int_key(ctx, j324keys::k201AuditBase), ctx.roundCount,
                 "D ★ 201 审计槽 = 击败回合");

        // 二次击败不再回满（不自动重武装）。
        ctx.getPet(0).hp = 400;   // 再残血
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp < max_hp,
              "D ★ 消费后击败（打 mate 不死但已出手）→ 不再回满（consumed 门）");
        CHECK_EQ(read_int_key(ctx, j324keys::k201AuditBase), ctx.roundCount,
                 "D ★ 审计槽不增长（无第二次消费）");
        std::cout << "  [D] 201 回满 + 全 PP + 一次性消费 + 不重武装\n";
    }

    std::cout << "scenario_7870 PASS (J324 187 永昌玺 + 201 凯旋预言)"
              << std::endl;
    return 0;
}
