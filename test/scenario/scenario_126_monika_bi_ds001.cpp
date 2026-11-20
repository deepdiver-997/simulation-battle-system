#include <utils/sbs_rand.h>
// 场景 126 —— 莫妮卡 琼华之庇传承给 DS-001（2026-09-25，语料 idx=94 口径）
//
// A. DS-001（真实魂印注册键 2414：乱流期=登场首 2 回合五条必发）首发连用第五技（极渊·渎神
//    38020：effect 1237 双方所有技能 PP 归零）——乱流期回合末"自身所有技能 PP+1"必发
//    → 每次用完全员回到 1 PP，"第三回合还有 1 pp"；第三回合用完即全消耗完
// B. 切莫妮卡：出战链恢复 DS-001 打空的技能 + 庇 +1（=1）
// C. 庇=1 保留：对手来源的 PP 归零**不生效**（pp 不变），且挂对手下回合攻先 ≤0 反应
// D. DS-001 自清（第五技 1237 双方归零）：**照常清空**（自清不走保留），
//    且**不触发**庇的"对手清 PP → 挂先制限制"反应（无归属事件被过滤）
// E. 二次传承：庇 =2（层数的增减伤语料未给值，先只记账）
#include <any>
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "../../resources/monika_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMark = 2492;
constexpr int kMoni = 3711;
constexpr int kDs = 4797;        // 极渊DS-001 monster id（ElfPet.id 用）
constexpr int kDsMark = 2414;    // 极渊DS-001 魂印注册键 = effect_icon.effect_id（装配层同源）
constexpr int kPlain = 10001;    // 撞击
constexpr int kFifth = 38020;    // 极渊·渎神（威力300；effect 1237 双方所有技能PP归零）
constexpr int kBiLimitKey = 2492300;  // 魂印：对手先制限制的 oss 键前缀（+被限制方）

int pub_get_oss_until(BattleContext& x, int key) {
    auto& oss = x.seerRobot[0].elfPets[x.on_stage[0]].on_stage_storage;
    auto it = oss.find(key);
    if (it == oss.end()) {
        return -1;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : -1;
}

int pub_get(BattleContext& x, int key) {
    auto it = x.plugin_storage.find(key);
    if (it == x.plugin_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

ElfPet ds_pet() {
    return make_pet(kDs, "极渊DS-001", {kPlain, kPlain, kPlain, kPlain, kFifth},
                    /*speed=*/500, /*atk=*/100, /*hp=*/3000,
                    SoulMark(kDsMark, "极渊DS-001", "算力"));
}

constexpr int kJiyuanMark() { return kDsMark; }  // 2026-09-27 对齐 effect_id（原误用 monster id）

ElfPet monika_pet() {
    return make_pet(kMoni, "圣光莫妮卡", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/500, /*atk=*/100, /*hp=*/6000,
                    SoulMark(kMark, "神", "测试桩：圣光莫妮卡神谕 2492"));
}

ElfPet plain_pet_foe() {
    return make_pet(3, "对手", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, 10, 100000);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(42);

    std::array<ElfPet, 6> own = {ds_pet(), monika_pet(), plain_pet_foe(),
                                 plain_pet_foe(), plain_pet_foe(), plain_pet_foe()};
    ScenarioBattle b(own, scenario::make_party(plain_pet_foe()));
    b.start();
    BattleContext& x = b.ctx();
    const auto round = [&](BattleFsm::ActionType op0, int idx0) {
        // 对手的 PP 会被 1237 清空——每回合开始回满，保证对手始终能提交有效选择
        // （否则 FSM 卡在等对手输入，后续 drive 全部空转）
        for (Skills& s : x.seerRobot[1].elfPets[x.on_stage[1]].skills) {
            if (s.pp != -1 && s.pp <= 0) {
                s.pp = s.maxPP;
            }
        }
        b.submit_operation(0, op0, idx0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
    };
    auto& ds = x.getPet(0);
    const int fifth_max = ds.skills[4].maxPP;

    // ═══ A. 乱流期：第五技清空全员 PP → 回合末必发 +1 → 始终保有 1 PP ════════
    round(BattleFsm::ActionType::SELECT_SKILL, 4);
    for (int i = 0; i < 5; ++i) {
        CHECK_EQ(ds.skills[i].pp, 1, "A1.R1 渎神清空 + 乱流必发回复 → 全员 1 PP");
    }
    round(BattleFsm::ActionType::SELECT_SKILL, 4);
    for (int i = 0; i < 5; ++i) {
        CHECK_EQ(ds.skills[i].pp, 1, "A1.R2 连续两回合使用，第三回合仍有 1 PP");
    }

    // ═══ A3. 第三回合：乱流已过，用完即全消耗完（20% 五条可能回 1，皆合法）════
    round(BattleFsm::ActionType::SELECT_SKILL, 4);
    for (int i = 0; i < 5; ++i) {
        std::cerr << ds.skills[i].pp << ",";
    }
    std::cerr << " prev_slot=" << x.prev_on_stage_slot[0]
              << " mark=" << x.getPet(0).soulMark.id << std::endl;
    const int fifth_after = ds.skills[4].pp;
    CHECK(fifth_after == 0 || fifth_after == 1, "A3 第三回合耗尽（乱流外 20% 回 1 皆合法）");
    // 模拟全消耗完（乱流期回合末 +1 必发会把 PP 抬回 1，此处清掉再切，出战链才有 0 位可恢复）
    for (int i = 0; i < 5; ++i) {
        ds.skills[i].pp = 0;
    }

    // ═══ B. 切莫妮卡：出战链恢复打空的技能 + 庇 =1 ══════════════════════════
    round(BattleFsm::ActionType::CHOOSE_PET, 1);
    CHECK_EQ(pub_get(x, kMoniPubBi), 1, "B1 出战链成功 → 庇 +1");
    CHECK_EQ(x.seerRobot[0].elfPets[0].skills[4].pp, fifth_max,
             "B2 DS-001 打空的第五技已恢复满");

    // ═══ C. 庇=1 保留：对手来源 PP 归零不生效 + 挂先制限制反应 ══════════════
    round(BattleFsm::ActionType::CHOOSE_PET, 0);   // 切回 DS-001
    CHECK_EQ(pub_get(x, kMoniPubBi), 1, "C1 前置：庇 =1（1 层即可跳 PP 清除）");
    const int hp_slot0_before = x.seerRobot[0].elfPets[0].skills[0].pp;
    core_api().pp_zero_slot(&x, /*target=*/0, /*slot=*/0, /*actor=*/1);
    x.event_center_.drain(&x, x.roundCount);   // 事件"入队+晚 drain"——手动泵一次
    CHECK_EQ(x.seerRobot[0].elfPets[0].skills[0].pp, hp_slot0_before,
             "C2 对手来源归零被保留：PP 不变");
    {
        // 反应落账：庇持有侧 oss 记录"对手下回合攻先 ≤0"
        auto& oss = x.seerRobot[0].elfPets[x.on_stage[0]].on_stage_storage;
        CHECK(oss.count(kBiLimitKey + 1), "C3 反应触发：对手下回合攻先限制已挂账");
    }

    // ═══ D. DS-001 自清（第五技）：照常清空 + 不触发反应 ════════════════════
    const int limit_before = pub_get_oss_until(x, kBiLimitKey + 1);
    round(BattleFsm::ActionType::SELECT_SKILL, 4);
    for (int i = 0; i < 5; ++i) {
        std::cerr << x.seerRobot[0].elfPets[0].skills[i].pp << ",";
    }
    std::cerr << " foe_pp=" << x.seerRobot[1].elfPets[0].skills[0].pp << std::endl;
    CHECK(x.seerRobot[0].elfPets[0].skills[0].pp <= 1,
          "D1 自清照常生效（保留不挡己方 1237 归零）");
    CHECK_EQ(pub_get_oss_until(x, kBiLimitKey + 1), limit_before,
             "D2 自清不触发反应：先制限制挂账不更新");

    // ═══ E. 二次传承：庇 =2（增减伤语料未给值，先记账）══════════════════════
    // 乱流期回合末 +1 必发会把 DS-001 的 PP 抬回 1——切莫妮卡前再次 stub 清零
    for (int i = 0; i < 5; ++i) {
        x.seerRobot[0].elfPets[0].skills[i].pp = 0;
    }
    round(BattleFsm::ActionType::CHOOSE_PET, 1);
    CHECK_EQ(pub_get(x, kMoniPubBi), 2, "E1 第二次出战链 → 庇 =2");

    std::cout << "scenario_126 全部通过" << std::endl;
    return 0;
}
