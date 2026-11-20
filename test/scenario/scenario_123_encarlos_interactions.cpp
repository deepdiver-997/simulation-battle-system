// 场景 123 —— 光螳螂交互组：龙·守地灵弹控反弹 / 1258 次免不可传承 / 第五不可穿盔 / 机盖双序弹伤
//
// 官方口径（用户 2026-09-24 实战口径 + 语料 idx=149/319）：
//   A. 龙·守地灵（26595，龙神哈莫 3809）191 弹控：{4}回合内免疫并反弹所有受到的异常
//      状态。螳螂第五（光·断罪裁决）未击败 → 1040 的 80% 疲惫被免疫并**反弹回螳螂**
//      （螳螂自身无免控/弹控/次免）——螳螂已出手，疲惫影响后续回合（"未击败疲惫招笑"）。
//   B. 天河圣芒斩 1258 次免**不可传承**：ON_STAGE 票，英卡洛斯切走即失效，登场者不吃。
//   C. 第五技能击杀**不可穿盔**：对手有攻击盔 → 第五被拦（SKILL_INVALID），无伤害无击杀。
//   D. 二命螳螂 高伤打机盖（圣甲·盖亚 3056「狂」）：机盖死亡判定弹等量真伤 → 螳螂被弹到
//      0 血 → 圣光祝福锁血残留 1 →（弹没弹死）机盖死亡照常成立。先手/后手两个顺序结果一致。
//      （弹伤在死亡漏斗拦截器层，语料 opus 965872689626480658；机盖弹红伤等量真伤。）
//
// 元素/技能素材：阳光普照10635（光145）；光·断罪裁决31847；龙·守地灵26595（属性技，
// 191 弹控 4 回合）；雷皇现世23360。光→元素0 = 1.0（锁后 2.0）。
#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kEncarlos = 3577;
constexpr int kMark = 1164;
constexpr int kBlessUsedKey = 990116402;

constexpr int kSkillLight = 10635;   // 阳光普照：光系 145 无特效
constexpr int kFifth = 31847;        // 光·断罪裁决（第五）
constexpr int kTianhe = 31846;       // 天河圣芒斩（1258 次免）
constexpr int kSkillAttr = 20001;    // 缩头（属性技占位）
constexpr int kShoudiling = 26595;   // 龙·守地灵（191 弹控 4 回合）
constexpr int kXitaoPet = 4622;
constexpr int kXitaoMark = 2217;     // 希拓：烧伤闭环（回合开始烧伤 + 烧伤对手锁疗）

ElfPet encarlos_mix(const std::array<int, 5>& skills, int atk, int hp, int speed = 300,
                    SoulMark mark = {}) {
    return make_pet(kEncarlos, "英卡洛斯", skills, speed, atk, hp, std::move(mark));
}

ElfPet hamo(int hp) {
    // 龙神哈莫 3809：只带龙·守地灵（属性技，慢速后手开弹控）
    return make_pet(3809, "龙神哈莫", {kShoudiling, kShoudiling, kShoudiling, kShoudiling, kShoudiling},
                    /*speed=*/100, /*atk=*/100, hp);
}

ElfPet gaia(int hp, int speed, int atk = 100) {
    // 圣甲·盖亚 3056「狂」：死亡判定弹等量真伤/反同归锁血
    return make_pet(3056, "圣甲盖亚", {kSkillLight, kSkillLight, kSkillLight, kSkillLight, kSkillLight},
                    speed, atk, hp, SoulMark(736, "圣甲盖亚", "狂"));
}

ElfPet gaia_kit_skill(const std::array<int, 5>& skills, int hp, int atk) {
    // 圣甲·盖亚 3056「狂」指定技能组（如狂绝冲撞 19366：面板先制+3）
    return make_pet(3056, "圣甲盖亚", skills, /*speed=*/100, atk, hp,
                    SoulMark(736, "圣甲盖亚", "狂"));
}

ElfPet filler(int id) {
    return make_pet(id, "填充", {kSkillAttr, kSkillAttr, kSkillAttr, kSkillAttr, kSkillAttr},
                    100, 100, 3000);
}

std::array<ElfPet, 6> solo(ElfPet pet) { return {pet, pet, pet, pet, pet, pet}; }

void run_round(ScenarioBattle& b, int idx0 = 0, int idx1 = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, idx1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A. 龙·守地灵弹控：第五未击败的疲惫被反弹回螳螂
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b({encarlos_mix({kFifth, kSkillLight, kSkillLight, kSkillLight, kSkillLight},
                                       200, 4000),
                          filler(2), filler(3), filler(4), filler(5), filler(6)},
                         {hamo(60000), filler(12), filler(13), filler(14), filler(15), filler(16)});
        b.start();
        // r1：螳螂用阳光普照（>280 → 粉伤分支），哈莫后手开龙·守地灵（191 弹控 4 回合
        //     + 884"使对手下次施放的技能无效"——2026-09-27 batch_b 落地后同样生效）。
        run_round(b, 1, 0);
        CHECK(b.ctx().getPet(1).hp > 0, "A 前置：哈莫存活");
        // r2：螳螂第五被 884 的次数盔拦下（对手"下次施放"= 本回合出手）→ 1040 不结算；
        //     哈莫原地 CHOOSE_PET（no-op）→ 不刷新 884。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.drive();
        CHECK_EQ(b.ctx().has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Fatigue)),
                 0, "A(884)：第五被无效 → 未击败疲惫不触发");
        // r3：哈莫原地 CHOOSE_PET（no-op，不再刷新 884）→ 次数盔已耗 → 第五落地，
        //     未击败 → 1040 疲惫被 191 免疫并反弹回螳螂。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.drive();
        CHECK(b.ctx().getPet(1).hp > 0, "A 前置：未击败（哈莫 6 万血）");
        CHECK_EQ(b.ctx().has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Fatigue)),
                 1, "A：疲惫被弹控反弹回螳螂（自身无免控/弹控/次免）");
        CHECK_EQ(b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)),
                 0, "A：哈莫自身免疫未吃疲惫");
    }

    // ════════════════════════════════════════════════════════════════
    // B. 天河 1258 次免不可传承：切走后登场者不吃免疫
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b({encarlos_mix({kTianhe, kSkillLight, kSkillLight, kSkillLight, kSkillLight},
                                       250, 4000, 300,
                                       SoulMark(kMark, "英卡洛斯", "光之惩戒")),
                          filler(2), filler(3), filler(4), filler(5), filler(6)},
                         {filler(12), filler(13), filler(14), filler(15), filler(16), filler(17)});
        b.start();
        run_round(b);   // r1：天河 >280 → 免疫下1次异常（ON_STAGE 票）
        // r2：英卡洛斯中切下场 → 票随 ON_STAGE epoch 失效；登场者不受传承。
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 直接施加异常验证：登场者（填充 2）无免 → 应成功落下。
        CHECK_EQ(apply_anomaly(&b.ctx(), 0, static_cast<int>(AbnormalStatusId::Blind), 2, 1),
                 ApplyAnomalyResult::SUCCESS, "B：次免不可传承（登场者照常吃异常）");
    }

    // ════════════════════════════════════════════════════════════════
    // C. 第五技能击杀不可穿盔：对手有攻击盔 → 第五被拦、无伤害无击杀
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(solo(encarlos_mix({kFifth, kSkillLight, kSkillLight, kSkillLight, kSkillLight},
                                           200, 4000, 300,
                                           SoulMark(kMark, "英卡洛斯", "光之惩戒"))),
                         solo(filler(12)));
        b.start();
        // 给对手挂攻击盔（count=1）：第五 → SKILL_INVALID，不穿盔、不造成伤害、无击杀。
        REQUIRE(seal_skill(&b.ctx(), /*source=*/1, /*target=*/0, /*effect_id=*/999,
                           /*attribute=*/false, /*attack=*/true, /*count=*/1,
                           /*duration_rounds=*/0, /*penetrable=*/false, /*source_slot=*/-1,
                           EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
                           /*consumed_when_pierced=*/false) >= 0);
        run_round(b, 0, 1);
        // 被盔 = 0 红伤（无击杀、不可穿盔）；0 < 280 → 走吸取档（max 3000/3 = 1000）。
        CHECK_EQ(b.ctx().getPet(1).hp, 3000 - 1000, "C：被盔拦下无红伤无击杀（不可穿盔），无伤害走吸取档");
    }

    // ════════════════════════════════════════════════════════════════
    // D1. 二命螳螂先手 高伤打机盖：弹出→祝福锁血1，机盖死亡成立
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(solo(encarlos_mix({kSkillLight, kSkillLight, kSkillLight, kSkillLight, kSkillLight},
                                           1500, 2000, 300,
                                           SoulMark(kMark, "英卡洛斯", "光之惩戒"))),
                         {gaia(2500, 100), filler(12), filler(13), filler(14), filler(15), filler(16)});
        b.start();
        run_round(b);   // 螳螂先手：高伤打致死 → 机盖死亡判定弹等量真伤 → 同步判定螳螂免死
        // （拦截器链里机盖反同归/螳螂圣光祝福谁先应答取决于注册序，但结果一致：
        //   螳螂被修正到 1 血存活 → 机盖"没弹死对面" → 自己不回血、死亡成立。）
        CHECK_EQ(b.ctx().getPet(1).hp, 0, "D1：机盖死亡成立（对面还活着，不触发回血）");
        CHECK_EQ(b.ctx().getPet(0).hp, 1, "D1：螳螂被弹出致死伤害 → 免死修正残留1（锁血）");
        CHECK(!b.ctx().pet_death_notified[0][b.ctx().on_stage[0]], "D1：螳螂未死（免死即时应答）");
        {
            auto it = b.ctx().seerRobot[0].elfPets[0].soulmark_storage.find(kBlessUsedKey);
            CHECK(it != b.ctx().seerRobot[0].elfPets[0].soulmark_storage.end()
                      && std::any_cast<int>(it->second) == 1,
                  "D1：机盖没先手杀 → 弹死即刻触发圣光祝福（非反同归代答）");
        }
        CHECK_EQ(b.state(), State::CHOOSE_AFTER_DEATH, "D1 前置：机盖阵亡选替补");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
    }

    // ════════════════════════════════════════════════════════════════
    // D3. 机盖先手（狂绝冲撞 19366 面板先制+3 > 螳螂先制+1）同回合击杀螳螂：
    //     祝福救下（消耗）→ 螳螂后手打致死机盖 → 弹伤致死且无免可应答
    //     → 螳螂真死、机盖留 1 存活（经典机盖反杀，同一回合内完成）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> en = {gaia_kit_skill({19366, 19366, 19366, 19366, 19366},
                                                   3000, 500),
                                    filler(12), filler(13), filler(14), filler(15), filler(16)};
        ScenarioBattle b(solo(encarlos_mix({kSkillLight, kSkillLight, kSkillLight, kSkillLight, kSkillLight},
                                           1500, 100, 100,
                                           SoulMark(kMark, "英卡洛斯", "光之惩戒"))),
                         en);
        b.start();
        // 同一回合：机盖先制+3 先手击杀螳螂 → 祝福救下（1 血、已消耗）→ 螳螂后手
        // 打致死机盖 → 弹全额真伤 → 螳螂 1 血被弹死（无免可应答）→ 机盖留 1。
        run_round(b, 0, 0);
        {
            auto it = b.ctx().seerRobot[0].elfPets[0].soulmark_storage.find(kBlessUsedKey);
            CHECK(it != b.ctx().seerRobot[0].elfPets[0].soulmark_storage.end()
                      && std::any_cast<int>(it->second) == 1,
                  "D3：先手击杀消耗了祝福（回合内曾被救回 1 血）");
        }
        CHECK_EQ(b.ctx().getPet(1).hp, 1, "D3：机盖弹死对面 → 自己留 1 存活");
        CHECK(b.ctx().pet_death_notified[0][b.ctx().on_stage[0]], "D3：螳螂被弹死（真死）");
        CHECK_EQ(b.state(), State::CHOOSE_AFTER_DEATH, "D3 前置：螳螂阵亡选替补");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
    }

    // ════════════════════════════════════════════════════════════════
    // D2. 机盖先手：同一交互、同一结果（弹伤在死亡判定，与出手顺序无关）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b({gaia(2500, 300), filler(12), filler(13), filler(14), filler(15), filler(16)},
                         {encarlos_mix({kSkillLight, kSkillLight, kSkillLight, kSkillLight, kSkillLight},
                                       1500, 2000, 300,
                                       SoulMark(kMark, "英卡洛斯", "光之惩戒")),
                          filler(2), filler(3), filler(4), filler(5), filler(6)});
        b.start();
        run_round(b);   // 机盖先手打一下 → 螳螂后手高伤打致死 → 同款弹伤+锁血
        CHECK_EQ(b.ctx().getPet(0).hp, 0, "D2：机盖死亡成立（与 D1 一致）");
        CHECK_EQ(b.ctx().getPet(1).hp, 1, "D2：螳螂被弹到锁血1（与 D1 一致）");
        {
            auto it = b.ctx().seerRobot[1].elfPets[0].soulmark_storage.find(kBlessUsedKey);
            CHECK(it != b.ctx().seerRobot[1].elfPets[0].soulmark_storage.end()
                      && std::any_cast<int>(it->second) == 1,
                  "D2：机盖没先手杀 → 弹死即刻触发圣光祝福（与 D1 一致）");
        }
        CHECK_EQ(b.state(), State::CHOOSE_AFTER_DEATH, "D2 前置：机盖阵亡选替补");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
    }

    // ════════════════════════════════════════════════════════════════
    // E. 希拓 × 烧伤机盖：机盖弹死希拓后想留 1 血，回血被希拓锁疗（条件=机盖烧伤）
    //    挡下 → 机盖真死 → 反同归（路径 B）把希拓从 0 血拉回 1 → "不管怎么弹都活"
    // ════════════════════════════════════════════════════════════════
    {
        ElfPet xitao = make_pet(kXitaoPet, "希拓", {kSkillLight, kSkillLight, kSkillLight, kSkillLight, kSkillLight},
                                /*speed=*/300, /*atk=*/1500, /*hp=*/1000,
                                SoulMark(kXitaoMark, "希拓", "烧伤闭环"));
        ScenarioBattle b(solo(xitao), solo(gaia(1500, 100)));
        b.start();
        run_round(b);
        // r1 链：回合开始希拓烧伤机盖 → 希拓攻击打致死 → 机盖弹全额真伤 → 希拓 0 血
        //       → 机盖"弹死对面"留 1 血 → 回血被锁疗挡下（机盖带烧伤）→ 机盖真死
        //       → 反同归重判把希拓拉回 1。
        CHECK(b.ctx().pet_death_notified[1][b.ctx().on_stage[1]], "E：机盖死亡成立（留血回血被锁疗挡下）");
        CHECK_EQ(b.ctx().getPet(0).hp, 1, "E：希拓被弹到 0 血 → 反同归拉回 1 存活");
        CHECK(!b.ctx().pet_death_notified[0][b.ctx().on_stage[0]], "E：希拓未死（不管怎么弹都活）");
    }

    // ════════════════════════════════════════════════════════════════
    // E2. 希拓后手打致死机盖：同样存活——反同归拉的是"当前出手流程窗口"的被弹方
    //     （先手方窗口内双死保先手、后手方窗口内双死保后手），后手希拓不违背该原则
    // ════════════════════════════════════════════════════════════════
    {
        ElfPet xitao = make_pet(kXitaoPet, "希拓", {kSkillLight, kSkillLight, kSkillLight, kSkillLight, kSkillLight},
                                /*speed=*/100, /*atk=*/1500, /*hp=*/1000,
                                SoulMark(kXitaoMark, "希拓", "烧伤闭环"));
        ScenarioBattle b(solo(xitao), solo(gaia(1500, 300)));
        b.start();
        run_round(b);
        // r1 链：回合开始烧伤机盖 → 机盖先手轻打希拓 → 希拓后手打致死 → 弹全额真伤
        //       → 希拓 0 血 → 机盖留 1 回血被锁疗挡下 → 机盖真死 → 反同归拉希拓回 1。
        CHECK(b.ctx().pet_death_notified[1][b.ctx().on_stage[1]], "E2：机盖死亡成立");
        CHECK_EQ(b.ctx().getPet(0).hp, 1, "E2：后手希拓同样被反同归拉回 1 存活");
        CHECK(!b.ctx().pet_death_notified[0][b.ctx().on_stage[0]], "E2：希拓未死");
    }

    std::cout << "scenario_123_encarlos_interactions: all checks passed" << std::endl;
    return 0;
}
