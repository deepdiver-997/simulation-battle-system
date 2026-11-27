// 场景 130 —— 登场节点 early 语义批次（2026-09-26）
//
// 锁的是"ENTER_EXIT_STAGE 非 early 节点只在开局执行一次"导致的登场效果丢失：
//   A. 谱尼圣洁：免疫票 ON_STAGE 作用域 + 切上场重授（用户实测"切下去再上来免控失效"）
//   B. 沧岚 2343：官方"登场时获得400点护盾" = 每次登场重发（切回场盾 +400）
//   C. 格劳瑞 2532：登场 +1 霆 + 弹控三条武装 = 每次登场重发（函数内注释本有此意）
//   D. 龙王 2447：官方"战斗开始时"加入双龙 —— 切上场**不**重复加（防误改 early）
//
// 内核依据：OPERATION_ENTER_EXIT_STAGE 桶全战斗只在开局执行一次（battleFsm
// handle_OperationEnterExitStage 唯一执行点）；perform_switch 只重新注册节点，
// 仅 early 节点在 activate（开局 + 每次上场）时立即执行（jiyuan_ds001 先例）。
#include <any>
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "../../resources/gelaorui_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPuni = 5000;
constexpr int kPuniMark = 1038;
constexpr int kCanglan = 3740;
constexpr int kCanglanMark = 2343;
constexpr int kGelaorui = 4004;
constexpr int kGlaMark = 2532;
constexpr int kDragon = 4186;
constexpr int kDragonMark = 2447;
constexpr int kMozun = 3683;
constexpr int kMozunMark = 1263;
constexpr int kPlain = 10001;
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);

ElfPet puni_pet() {
    SoulMark sm(kPuniMark, "谱", "测试桩：圣灵谱尼七刻印");
    return make_pet(kPuni, "圣灵谱尼", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/300, /*atk=*/60, /*hp=*/100000, sm);
}
ElfPet mozun_pet() {
    return make_pet(kMozun, "混沌魔尊", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/100, /*atk=*/1, /*hp=*/99999,
                    SoulMark(kMozunMark, "混沌魔尊", "登场随机控+裸伤阈值"));
}
ElfPet canglan_pet() {
    SoulMark sm(kCanglanMark, "滴", "测试桩：怒涛·沧岚神谕 2343");
    return make_pet(kCanglan, "怒涛·沧岚", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/300, /*atk=*/150, /*hp=*/6000, sm);
}
ElfPet gla_pet() {
    SoulMark sm(kGlaMark, "霆", "测试桩：圣光格劳瑞神谕 2532");
    return make_pet(kGelaorui, "圣光格劳瑞", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/300, /*atk=*/150, /*hp=*/6000, sm);
}
ElfPet dragon_pet() {
    SoulMark sm(kDragonMark, "镇", "测试桩：镇世·乔特鲁德 2447");
    return make_pet(kDragon, "镇世·乔特鲁德", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/300, /*atk=*/150, /*hp=*/6000, sm);
}
ElfPet plain_pet(int atk = 10) {
    return make_pet(1, "普", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, atk, 99999);
}

int pub_get(BattleContext& x, int key) {
    auto it = x.plugin_storage.find(key);
    if (it == x.plugin_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

// side 主动切到 slot，对手随便出招，驱动一整回合
void do_switch(ScenarioBattle& b, int side, int slot) {
    b.submit_operation(side, BattleFsm::ActionType::CHOOSE_PET, slot);
    b.submit_operation(1 - side, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 池 → 单一异常的观测票（085 同款）：控制池(≤20)→诅咒、全池(44)→中毒
void grant_pool_marker(BattleContext& x) {
    x.rule_center_.grant_anomaly_pool_mod(
        0, 9001, [](BattleContext*, const std::vector<int>& pool) {
            if (pool.size() <= 20) {
                return std::vector<int>{kCurse};
            }
            return std::vector<int>{kPoison};
        });
}

bool has_status(BattleContext& x, int side, int id) {
    return x.has_active_abnormal_status(side, id);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    // ═══ A. 谱尼圣洁：切出免疫作废（不护队友）、切回重授 ══════════════
    {
        std::array<ElfPet, 6> own = {plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {puni_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        b.start();
        BattleContext& x = b.ctx();
        auto& core = core_api();

        auto apply_burn = [&]() {
            return core.apply_anomaly(&x, 1, /*流血=*/4, 1, /*actor=*/0);
        };
        CHECK(apply_burn() == ApplyAnomalyResult::TARGET_IMMUNE, "A1 开局谱尼在场 → 免疫在");
        x.event_center_.drain(&x, x.roundCount);

        // 切到 1 号位（普通精灵上场）：票随下场作废，且圣洁不护队友
        do_switch(b, 1, 1);
        CHECK(x.on_stage[1] == 1, "A2 普通队友上场");
        CHECK(apply_burn() == ApplyAnomalyResult::SUCCESS, "A3 谱尼场下 → 免疫不在（不护队友）");
        x.event_center_.drain(&x, x.roundCount);

        // 切回 0 号位：early 重授
        do_switch(b, 1, 0);
        CHECK(x.on_stage[1] == 0, "A4 谱尼回场");
        CHECK(apply_burn() == ApplyAnomalyResult::TARGET_IMMUNE, "A5 切回场 → 免疫重授");
        x.event_center_.drain(&x, x.roundCount);
    }

    // ═══ B. 沧岚 2343：切回场登场盾重发 ══════════════════════════════
    {
        std::array<ElfPet, 6> own = {canglan_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        CHECK_EQ(x.seerRobot[0].elfPets[0].shield_bank_.total(), 400, "B1 开局登场盾 400");

        do_switch(b, 0, 1);
        do_switch(b, 1, 2);   // 对手侧换位，普打普
        const int shield_before =
            x.seerRobot[0].elfPets[0].shield_bank_.total();

        // 切回：对手侧同槽 no-op 换宠（双方都出换宠 → 本回合无攻击，无吸收干扰）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 2);
        b.drive();
        const int shield_after = x.seerRobot[0].elfPets[0].shield_bank_.total();
        CHECK_EQ(shield_after - shield_before, 400, "B2 切回场 → 登场盾重发 +400");
    }

    // ═══ C. 格劳瑞 2532：切回场 +1 霆、弹控三条重武装 ════════════════
    {
        std::array<ElfPet, 6> own = {gla_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        CHECK_EQ(pub_get(x, kGlaPubTing + 0), 1, "C1 开局登场 +1 霆");

        do_switch(b, 0, 1);
        CHECK_EQ(pub_get(x, kGlaPubTing + 0), 1, "C2 切下场霆不变（发布槽下场保留）");
        do_switch(b, 0, 0);
        CHECK_EQ(pub_get(x, kGlaPubTing + 0), 2, "C3 切回场 → 再 +1 霆（弹控三条同节点重武装，"
                                                  "其为回合级 token、ROUND_END 结清，不经发布槽断言）");
    }

    // ═══ D. 龙王 2447：双龙是"战斗开始时"——切上场不重复加 ══════════
    {
        std::array<ElfPet, 6> own = {dragon_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        CHECK_EQ(static_cast<int>(x.extra_spirits[0].size()), 2, "D1 开局双龙入容器");

        do_switch(b, 0, 1);
        do_switch(b, 0, 0);
        CHECK_EQ(static_cast<int>(x.extra_spirits[0].size()), 2,
                 "D2 切回场不重复加龙（战斗开始时语义，非登场时）");
    }

    // ═══ E. 混沌魔尊 1263 × 谱尼圣洁：登场控 vs 免疫授予的时序（2026-09-26 二轮）═
    // 官方语义：魔尊部署=首次登场控（控制池）、换宠再登场=全池控（EVENT_ENTER_STAGE
    // watcher，drain 在状态桶后）。圣洁 early 授予在开局 activate 循环（先于一切桶
    // 节点）/换宠 perform_switch 内同步——两条施加路都晚于授予 → 挡得住。
    // ⚠️ 改 early 前圣洁授予在 challenger 桶（host 桶之后）——魔尊若为 host，部署控
    //    先落地，谱尼会被穿。此组同时锁死"early 化修复了该交互"这一事实。
    {
        // E1: 魔尊(host)部署控 → 谱尼(挑战方首发)免疫在 → 挡
        std::array<ElfPet, 6> own = {mozun_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {puni_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        BattleContext& x = b.ctx();
        grant_pool_marker(x); // 登场前限定异常池，确保观测票覆盖首次部署
        b.start();
        CHECK(!has_status(x, 1, kCurse) && !has_status(x, 1, kPoison),
              "E1 魔尊部署控 → 圣洁(early,先于桶)挡下");

        // E2: 谱尼切出/切回（重授）；魔尊切出/切回（第二次登场=全池控）→ 仍挡
        do_switch(b, 1, 1);   // 谱尼下场（票随下场作废；队友普通宠在场不享受）
        do_switch(b, 0, 1);   // 魔尊下场
        do_switch(b, 1, 0);   // 谱尼回场（perform_switch 内同步重授）
        do_switch(b, 0, 0);   // 魔尊回场 → watcher 全池控，目标=场上谱尼 → 挡
        CHECK(!has_status(x, 1, kCurse) && !has_status(x, 1, kPoison),
              "E2 切回重授后 → 魔尊换宠登场控仍被挡");
    }

    // E3 对照：无圣洁的普通宠吃同一部署控 → 异常落地（证明链路本身工作）
    {
        std::array<ElfPet, 6> own = {mozun_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        BattleContext& x = b.ctx();
        grant_pool_marker(x); // 控制首次登场的随机异常，而非事后注册
        b.start();
        CHECK(has_status(x, 1, kCurse),
              "E3 对照：普通宠被同一部署控命中（链路非静默 no-op）");
    }

    std::cout << "scenario_130 done" << std::endl;
    return 0;
}
