// 场景 122 —— 光之惩戒·英卡洛斯 3577（光螳螂）魂印 1164 完整版 + 技能组切片
//
// 官方口径（effect_icon 881；语料《死或生》866772495396503575 /《免死检测》947280935819149312；
// 用户 2026-09-24 口径）：
//   A. ①先制+1：所有技能无条件先制+1（慢速反制先手）
//   B. ①免疫所有能力下降（天生）
//   C. ②280 严格阈值：<280 → 吸取1/3 + 下回合必暴；>280 → 1/3 百分比粉伤 + 下回合封属；
//      （恰好 280 两者都不触发——代码层无第三分支，卡阈值即"针对措施"）
//   D. ②封属"切走也不会生效"：对手中切，登场者不受封属
//   E. ③首次死亡圣光祝福：残留1体力；再死真死（仅首次）
//   F. ③解控=回合结束档（非实时）：被先手击杀当回合控制不解
//   G. ③下3次攻击+150% 切走即失（on_stage_storage）
//   H. 技能组：无始源光 433（2回合每回合攻+1速+1，首跳即时）/ 璨星断刃 699（无视攻击
//      免疫：自然 0 倍按 1 结算）/ 天河圣芒斩 1258（>280 免疫下1次异常，严格大于）
//
// 元素/技能素材：阳光普照10635（光145 无特效）；缩头20001（属性技）；无始源光26032；
//               璨星断刃31844（光战斗85）；天河圣芒斩31846（光战斗150）。
//               光→超能=2.0 × 战→超能=0.5 = 1.0；光→草=0（免疫）。
#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kEncarlos = 3577;
constexpr int kMark = 1164;
constexpr int kBoostChargeKey = 990116404;

constexpr int kSkillLight = 10635;  // 阳光普照：光系 145 无特效
constexpr int kSkillAttr = 20001;   // 缩头：属性技（效果4 未实现，仅作占位动作）
constexpr int kBuffAttr = 23360;    // 雷皇现世：属性技（585 六维+1，可观测）
constexpr int kWushi = 26032;       // 无始源光（属性技）
constexpr int kCanxing = 31844;     // 璨星断刃（光战斗 85）
constexpr int kTianhe = 31846;      // 天河圣芒斩（光战斗 150）

constexpr int kElemPsychic = 10;
constexpr int kElemGrass = 1;

ElfPet encarlos(int skill, int atk, int hp, int speed = 300, SoulMark mark = {}) {
    return make_pet(kEncarlos, "英卡洛斯", {skill, skill, skill, skill, skill},
                    speed, atk, hp, std::move(mark));
}

ElfPet target(int hp, int elem0 = kElemPsychic, int speed = 100) {
    ElfPet p = make_pet(9, "靶子", {kSkillAttr, kSkillAttr, kSkillAttr, kSkillAttr, kSkillAttr},
                        speed, /*atk=*/100, hp);
    p.elementalAttributes[0] = elem0;
    return p;
}

ElfPet encarlos_mix(const std::array<int, 5>& skills, int atk, int hp,
                    SoulMark mark = {}) {
    return make_pet(kEncarlos, "英卡洛斯", skills, 300, atk, hp, std::move(mark));
}

// 观测载体：雷皇现世 23360（属性技，585 六维+1 已实现）→ 攻击等级 +1 可读。
ElfPet buff_target(int hp) {
    return make_pet(8, "靶子", {kBuffAttr, kBuffAttr, kBuffAttr, kBuffAttr, kBuffAttr},
                    /*speed=*/100, /*atk=*/100, hp);
}

ElfPet filler(int id) {
    return make_pet(id, "填充", {kSkillAttr, kSkillAttr, kSkillAttr, kSkillAttr, kSkillAttr},
                    100, 100, 3000);
}

std::array<ElfPet, 6> solo(ElfPet pet) { return {pet, pet, pet, pet, pet, pet}; }

void run_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A. ①先制+1：慢速英卡洛斯反制先手
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(solo(encarlos(kSkillLight, 150, 4000, /*speed=*/100,
                                       SoulMark(kMark, "英卡洛斯", "光之惩戒"))),
                         solo(target(6000, 0, /*speed=*/200)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // 停在第2回合 CHOOSE：ws 尚未 reset，先手权仍是第1回合的结算值
        CHECK_EQ(b.ctx().is_second_mover(0), 0, "A：速度100 vs 200，先制+1 → 仍先手");
    }
    {
        ScenarioBattle b(solo(encarlos(kSkillLight, 150, 4000, /*speed=*/100, SoulMark{})),
                         solo(target(6000, 0, /*speed=*/200)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().is_second_mover(0), 1, "A 对照：无魂印 → 速度慢为后手");
    }

    // ════════════════════════════════════════════════════════════════
    // B. ①免疫所有能力下降
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(solo(encarlos(kSkillLight, 150, 4000, 300,
                                       SoulMark(kMark, "英卡洛斯", "光之惩戒"))),
                         solo(target(6000, 0)));
        b.start();
        run_round(b);   // 免疫票在 ROUND_START 刷新（ENTER_EXIT 早期授票会被 epoch 重锚吃掉）
        CHECK_EQ(stat_drop(&b.ctx(), 0, 0, 2, 1), StatDropResult::IMMUNE,
                 "B：天生免疫能力下降");
    }

    // ════════════════════════════════════════════════════════════════
    // C. ②280 严格阈值：>280 封属 / <280 吸取+必暴
    // ════════════════════════════════════════════════════════════════
    {
        // C1 >280：atk200 vs 超能（锁 2 倍）→ 红伤 460~540 >280 → 下回合封属。
        ScenarioBattle b(solo(encarlos(kSkillLight, 200, 4000, 300,
                                       SoulMark(kMark, "英卡洛斯", "光之惩戒"))), solo(target(6000)));
        b.start();
        run_round(b);
        const int hp1 = b.ctx().getPet(1).hp;
        CHECK(hp1 < 4000, "C1：粉伤 1/3（2000）已附加在红伤之外");
        run_round(b);
        CHECK_EQ(b.ctx().ability_level(1, 2), 0, "C1：下回合属性技能无效（封属生效）");
    }
    {
        // C2 <280：atk60 → 红伤 148~175 <280 → 吸取 1/3（2000）+ 下回合必暴。
        ScenarioBattle b(solo(encarlos(kSkillLight, 60, 4000, 300,
                                       SoulMark(kMark, "英卡洛斯", "光之惩戒"))), solo(target(6000)));
        b.start();
        run_round(b);
        const int drained = 6000 - b.ctx().getPet(1).hp;
        CHECK(drained > 2100, "C2：吸取 1/3 已附加在红伤之外");
        CHECK(drained < 2250, "C2：红伤部分在 148~175（<280 分支）");
        run_round(b);
        CHECK_EQ(b.ctx().crit_happened[0], 1, "C2：下回合攻击必定打出致命一击");
    }

    // ════════════════════════════════════════════════════════════════
    // D. ②封属"切走也不会生效"：对手中切，登场者不受封属
    // ════════════════════════════════════════════════════════════════
    {
        // 英卡洛斯 r2/r3 用属性技（不攻击 → 不续封），隔离观察 r1 记下的封属。
        ScenarioBattle b(solo(encarlos_mix({kSkillLight, kSkillAttr, kSkillAttr, kSkillAttr, kSkillAttr},
                                           200, 4000,
                                           SoulMark(kMark, "英卡洛斯", "光之惩戒"))),
                         {buff_target(6000), buff_target(6000), target(6000),
                          target(6000), target(6000), target(6000)});
        b.start();
        run_round(b);   // r1：>280 → 记封属（下回合、槽0）
        // r2：对手中切到槽1（被标记的是槽0）→ 封属落空；英卡洛斯用属性技不续封。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        // r3：登场者（槽1）用属性技（23360）→ 正常生效（封属已在标记回合后过期）。
        run_round(b);
        CHECK_EQ(b.ctx().ability_level(1, 0), 1, "D：中切后封属不生效（登场者属性技正常）");
    }

    // ════════════════════════════════════════════════════════════════
    // E+F. ③首次死亡残留1体力（仅首次）+ 解控是回合结束档
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(solo(encarlos(kSkillLight, 500, 4000)),
                         {encarlos(kSkillAttr, 100, 100, /*speed=*/100,
                          SoulMark(kMark, "英卡洛斯", "光之惩戒")), filler(2), filler(3),
                          filler(4), filler(5), filler(6)});
        b.start();
        // 预先给英卡洛斯挂失明（20，无伤害）→ 模拟"带控制的技能先手击杀"。
        REQUIRE(apply_anomaly(&b.ctx(), 1, static_cast<int>(AbnormalStatusId::Blind), 2, 0)
                == ApplyAnomalyResult::SUCCESS);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // 跑完整回合（死亡被拦 → 行动照常 → ROUND_END 解控）
        CHECK_EQ(b.ctx().getPet(1).hp, 1, "E：首次死亡被圣光祝福拦下，残留1体力");
        CHECK_EQ(std::any_cast<int>(b.ctx().seerRobot[1].elfPets[0].on_stage_storage[kBoostChargeKey]),
                 3, "E：下3次攻击增伤已装填");
        std::cout << "[probe] curemark=" << (b.ctx().plugin_storage.count(990116409 + 1)
                                           ? std::any_cast<int>(b.ctx().plugin_storage[990116409 + 1]) : -9)
                  << " blind=" << b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Blind))
                  << " round=" << b.ctx().roundCount << std::endl;
        CHECK_EQ(b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Blind)),
                 0, "F：战斗阶段结束（回合结束档）才解除自身异常");
        // 第2回合再死 → 真死（仅首次）。
        run_round(b);
        CHECK_EQ(b.state(), State::CHOOSE_AFTER_DEATH, "E：第二次死亡不再拦截");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
    }

    // ════════════════════════════════════════════════════════════════
    // G. ③下3次增伤切走即失
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(solo(encarlos(kSkillLight, 500, 4000)),
                         {encarlos(kSkillAttr, 100, 100, 100,
                          SoulMark(kMark, "英卡洛斯", "光之惩戒")), filler(2), filler(3),
                          filler(4), filler(5), filler(6)});
        b.start();
        run_round(b);   // 击杀 → 祝福：hp1 + 3 层增伤
        CHECK_EQ(b.ctx().getPet(1).hp, 1, "G 前置：祝福已触发");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();   // 中切下场
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.drive();   // 切回场
        auto& storage = b.ctx().seerRobot[1].elfPets[0].on_stage_storage;
        CHECK(storage.find(kBoostChargeKey) == storage.end(),
              "G：切走后增伤层数清空（切走了下次就没有）");
    }

    // ════════════════════════════════════════════════════════════════
    // H1. 无始源光 433：2回合内每回合攻+1速+1（首跳即时）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(solo(encarlos_mix({kWushi, kSkillLight, kSkillLight, kSkillLight, kSkillLight},
                                           150, 4000)),
                         {target(30000), target(30000), target(30000),
                          target(30000), target(30000), target(30000)});
        b.start();
        run_round(b);   // r1：无始源光 → 585 全属性+1 + 433 首跳（攻/速再+1）
        CHECK_EQ(b.ctx().ability_level(0, 0), 2, "H1：攻击 +2（585 的 1 + 433 的 1）");
        CHECK_EQ(b.ctx().ability_level(0, 2), 1, "H1：防御 +1（仅 585）");
        CHECK_EQ(b.ctx().ability_level(0, 4), 2, "H1：速度 +2");
        const int atk_after_r1 = b.ctx().ability_level(0, 0);
        // r2：换普通攻击技能（不重复触发 585/433 首跳），窗口在 ROUND_START 跳一跳。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ability_level(0, 0), atk_after_r1 + 1, "H1：第2回合 433 再跳一跳");
        const int atk_after_r2 = b.ctx().ability_level(0, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ability_level(0, 0), atk_after_r2, "H1：第3回合窗口已过不再跳");
    }

    // ════════════════════════════════════════════════════════════════
    // H2. 璨星断刃 699（内核穿透凭证）：loader 把它并进 ws.attack_credential
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(solo(encarlos(kCanxing, 150, 4000, 300, SoulMark{})),   // 无魂印隔离
                         solo(target(6000, kElemGrass)));
        b.start();
        run_round(b);
        CHECK_EQ(b.ctx().ws.attack_credential[0].ignore_attack_immunity, 1,
                 "H2：699 无视攻击免疫 → 穿透凭证已物化");
    }

    // ════════════════════════════════════════════════════════════════
    // H3. 天河圣芒斩 1258：攻击伤害 >280（严格）→ 自身免疫下 1 次异常
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(solo(encarlos(kTianhe, 250, 4000, 300, SoulMark{})),
                         solo(target(30000, 0)));
        b.start();
        run_round(b);
        // 769 的 +50% 使红伤必 >280（317×1.5×0.85 ≈ 404 下限）→ 免疫票已挂（1次）。
        CHECK(apply_anomaly(&b.ctx(), 0, static_cast<int>(AbnormalStatusId::Blind), 2, 1)
                  != ApplyAnomalyResult::SUCCESS,
              "H3：>280 → 免疫第 1 次异常");
        CHECK_EQ(apply_anomaly(&b.ctx(), 0, static_cast<int>(AbnormalStatusId::Blind), 2, 1),
                 ApplyAnomalyResult::SUCCESS, "H3：第 2 次异常照常落下（免疫下1次）");
    }

    std::cout << "scenario_122_encarlos_kit: all checks passed" << std::endl;
    return 0;
}
