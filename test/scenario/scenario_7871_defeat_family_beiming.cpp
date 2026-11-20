// 场景 7871 —— J324 击败族：218 北冥狂刃（必暴 + 击败回合攻击伤害真伤附加）
//
//   218 北冥狂刃（effect_des 218，宿主=霍光 4515 魂印 2116 词条）：
//     「自身攻击技能必定打出致命一击，且使用后附加上次击败对手的回合所造成
//     攻击伤害100%的真实伤害，下场后保留」
//     ①必暴 = ROUND_START 每回合重写 ws.must_crit[owner]（j_must_crit_window 同款
//     写点；属性技无伤害不判暴，天然攻击技限定）；②真伤附加 = 击败回调把
//     ws.raw_attack_damage[side]（裸伤台账，battleFsm.cpp:907 写点）记入持有者
//     soulmark_storage 账本（"下场后保留"字面），ON_SKILL_HIT 攻击技后
//     deal_true_damage(opp, 账本, owner)。账本不清零（"上次击败"=最近一次，记档）。
//
//   A 组 ★ 必暴主体：R1 互拍 → ws.must_crit[0]==true（窗口写入）
//   B 组 ★ 首击无附加：未击败时对手掉血 == 裸伤台账（无真伤段）+ 账本不存在
//   C 组 ★ 击败记账 + 下次攻击附加：击败 fodder → 账本=裸伤；R2 拍 mate →
//         mate 掉血 == R2 裸伤 + 账本（等式两侧都取自实测值，伤害浮动自洽）
//   D 组 ★ 击败回合当击不附加本次值：R2 使用属性技（缩头，无伤害阶段）→ 零附加
//   E 组 ★ 账本跨下场保留：断言账本落位持有者 soulmark_storage（非 on_stage）
//
// 断言纪律：速度差钉死先手；submit_operation + drive() 有限推进（禁 while(!FINISHED)）；
// 伤害浮动系数（calculation.h 217+sbs_rand()%39）不进断言面——全部用"掉血==台账"
// 相对断言（同一击的 final 与台账同源，浮动自洽）；无概率分支。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace j324keys {
constexpr int k218LedgerKey = 993200080;
constexpr int k218LedgerRoundKey = 993200090;
}  // namespace j324keys

namespace {

constexpr int kPaiDa = 10004;   // 拍打（物理攻击，纯载体）
constexpr int kAttr = 20001;    // 缩头（属性技，无伤害阶段）

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

SoulMark mark218() {
    SoulMark sm(218, "J324", "test");
    sm.monster_id = 4515;   // 霍光
    return sm;
}

ElfPet huoguang() {
    ElfPet p = make_pet(4515, "霍光", {kPaiDa, kAttr, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/99999, mark218());
    accuracy_100(p);
    return p;
}

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

int read_ledger(const BattleContext& ctx) {
    auto& st = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].soulmark_storage;
    auto it = st.find(j324keys::k218LedgerKey);
    if (it == st.end()) {
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

    // ══ A+B 首击无附加（必暴窗生效 + 账本空）══
    {
        ElfPet me = huoguang();
        ElfPet foe = mate_pet();
        ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
        b.start();
        BattleContext& ctx = b.ctx();

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 拍打
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();

        CHECK(ctx.ws.must_crit[0], "A ★ 218 必暴主体：ROUND_START 窗写入 ws.must_crit");
        const int raw1 = ctx.ws.raw_attack_damage[0];
        CHECK(raw1 > 0, "A 前置：R1 攻击伤害台账 > 0");
        const int hp_loss = 99999 - ctx.getPet(1).hp;
        CHECK_EQ(hp_loss, raw1,
                 "B ★ 首击（未击败）→ 对手掉血 == 裸伤台账（无真伤附加段）");
        CHECK_EQ(read_ledger(ctx), 0, "B ★ 无击败 → 账本不存在/为 0");
        CHECK(ctx.getPet(1).hp > 0, "B 前置：对手存活（mate 拍不死）");
        std::cout << "  [A+B] 必暴窗生效 + 首击无附加\n";
    }

    // ══ C2 账本生效：下一场同对局继续——对手死切补位后拍打 → 掉血 == 裸伤 + 账本 ══
    // （7871 A 段的 mate 是 make_party 单宠——没有替补可死切；用独立对局验证附加：
    //   先制造账本（对手 fodder hp=1 被秒），死切补 mate 后拍打断言等式。）
    {
        ElfPet me = huoguang();
        std::array<ElfPet, 6> foes{fodder_pet(), mate_pet(), mate_pet(),
                                   mate_pet(), mate_pet(), mate_pet()};
        ScenarioBattle b(make_party(std::move(me)), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 秒 fodder
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp <= 0, "C2 前置：R1 击倒 fodder");
        const int ledger = read_ledger(ctx);
        CHECK(ledger > 0, "C2 ★ 击败发生 → 账本 = 击败回合裸伤台账（>0）");
        CHECK_EQ(ledger, ctx.ws.raw_attack_damage[0],
                 "C2 ★ 账本值 == 击败回合攻击伤害台账（“上次击败的回合所造成攻击伤害”）");
        {
            auto& st = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].soulmark_storage;
            auto rit = st.find(j324keys::k218LedgerRoundKey);
            const int audit_val =
                rit == st.end() ? -1 : (std::any_cast<int>(&rit->second)
                                            ? *std::any_cast<int>(&rit->second)
                                            : -1);
            CHECK(audit_val == ctx.roundCount,
                  "C2 ★ 账本审计 = 击败发生回合");
        }

        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);     // 死切补 mate
        b.drive();
        const int hp_before = ctx.getPet(1).hp;
        CHECK_EQ(hp_before, 99999, "C2 前置：替补 mate 满血");

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 拍 mate
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int raw2 = ctx.ws.raw_attack_damage[0];
        CHECK(raw2 > 0, "C2 前置：R2 台账 > 0");
        const int loss = 99999 - ctx.getPet(1).hp;
        CHECK_EQ(loss, raw2 + ledger,
                 "C2 ★ R2 掉血 == R2 裸伤 + 账本（真伤附加精确落账，浮动自洽）");
        CHECK_EQ(read_ledger(ctx), ledger,
                 "C2 ★ 账本不清零（“上次击败”=最近一次，下一次攻击继续附加，记档）");
        std::cout << "  [C2] 真伤附加等式成立 + 账本不清零\n";
    }

    // ══ D 属性技不附加：账本>0 时用缩头（无伤害阶段）→ 对手掉血为 0 ══
    {
        ElfPet me = huoguang();
        std::array<ElfPet, 6> foes{fodder_pet(), mate_pet(), mate_pet(),
                                   mate_pet(), mate_pet(), mate_pet()};
        ScenarioBattle b(make_party(std::move(me)), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 秒 fodder
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(read_ledger(ctx) > 0, "D 前置：账本已记账");

        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        const int hp_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 缩头（属性技）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, hp_before,
                 "D ★ 属性技（无伤害阶段）→ 零附加零掉血（executed_skill_is_attack 门）");
        std::cout << "  [D] 属性技不附加\n";
    }

    // ══ E 账本跨下场保留：账本落位 soulmark_storage（非 on_stage 槽）══
    {
        ElfPet me = huoguang();
        std::array<ElfPet, 6> foes{fodder_pet(), mate_pet(), mate_pet(),
                                   mate_pet(), mate_pet(), mate_pet()};
        ScenarioBattle b(make_party(std::move(me)), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int slot = ctx.on_stage[0];
        auto& st = ctx.seerRobot[0].elfPets[slot].soulmark_storage;
        CHECK(st.count(j324keys::k218LedgerKey) == 1,
              "E ★ 账本落位持有者 soulmark_storage（ElfPet 级，随宠下场保留——“下场后保留”字面）");
        std::cout << "  [E] 账本存储位 = soulmark_storage（下场保留）\n";
    }

    std::cout << "scenario_7871 PASS (J324 218 北冥狂刃)" << std::endl;
    return 0;
}
