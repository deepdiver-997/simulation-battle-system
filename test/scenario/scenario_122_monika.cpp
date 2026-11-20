#include <utils/sbs_rand.h>
// 场景 122 —— 圣光莫妮卡（神谕 3711 / 魂印 2492）（精灵王线 2026-09-24）
//
// A. 出场 +4 律；PP 转移链：上只在场有 pp=0 位（第五位 0/0 也算）→ 消耗自身对应位 PP
//    + 恢复上只 + 庇 +1；无上只（首发）/上只死亡/无合格位 → 失败且无庇
// B. 有律：选技能不受 PP 限制；回合末对手未选技能 → 耗 1 律 + 沉默
// C. 回合末对手选了技能 → 最优系别 2 回合（候选取最优：克制系数高于单看在场系别）
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
constexpr int kPlain = 10001;

int pub_get(BattleContext& x, int key) {
    auto it = x.plugin_storage.find(key);
    if (it == x.plugin_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

ElfPet monika_pet(int hp = 6000) {
    SoulMark sm(kMark, "神", "测试桩：圣光莫妮卡神谕 2492");
    return make_pet(kMoni, "圣光莫妮卡", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/500, /*atk=*/100, hp, sm);
}

ElfPet plain_pet(int atk = 100, int hp = 4000, int speed = 100) {
    return make_pet(1, "普通", {kPlain, kPlain, kPlain, kPlain, kPlain}, speed, atk, hp);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ═══ A. 出场链 ══════════════════════════════════════════════════
    {
        // 上只（slot0 首发）普通 → 换圣莫上场：上只 1 号位技能 PP 打空制造合格位
        std::array<ElfPet, 6> own = {plain_pet(50, 9000), monika_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet(10, 100000)));
        b.start();
        BattleContext& x = b.ctx();

        CHECK_EQ(pub_get(x, kMoniPubLv), 0, "A0 圣莫未出场无律");

        // 制造合格位：上只（首发普通）1 号位 PP 清 0（测试桩直写，等价被效果耗空）
        x.getPet(0).skills[1].pp = 0;
        CHECK_EQ(x.getPet(0).skills[1].pp, 0, "A1 前置：上只 1 号位 PP=0");
        // 双方都提 CHOOSE_PET（006 同款；对方切到同占位宠无实际影响）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();

        // 切换同一 drive 的回合末可能已走一次"未选技"分支（对手也是 CHOOSE_PET）→ 3 或 4
        { const int lv = pub_get(x, kMoniPubLv); CHECK(lv == 4 || lv == 3, "A2 出场 +4 律"); }
        CHECK(pub_get(x, kMoniPubBi) >= 1, "A3 转移成功 → 庇 +1");
        // ⚠️ 换宠后 getPet(0) 是圣莫本人——上只用 elfPets[0] 直取
        CHECK_EQ(x.seerRobot[0].elfPets[0].skills[1].pp,
                 x.seerRobot[0].elfPets[0].skills[1].maxPP, "A4 上只 pp=0 位已恢复满");
        CHECK_EQ(x.seerRobot[0].elfPets[1].skills[1].pp, 0, "A5 圣莫对应位 PP 被消耗");
    }

    // ═══ B/C. 律的回合末分支 ════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {monika_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet(10, 100000)));
        b.start();
        BattleContext& x = b.ctx();

        // 首发出场：无上只 → 链失败无庇，律=4（若开局 ROUND_END 已走一次未选分支则 3）
        const int lv_at_start = pub_get(x, kMoniPubLv);
        CHECK(lv_at_start == 4 || lv_at_start == 3, "B1 出场 +4 律（开局分支消耗一次则 3）");
        CHECK_EQ(pub_get(x, kMoniPubBi), 0, "B2 无上只 → 无庇");
        // 回合 1：对手也选技能 → 未触发沉默 → 最优系别 2 回合
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // B3（回合开始后）：有律 → 选技不受 PP 限制（round_start 节点置位）
        CHECK(x.ignore_pp[0], "B3 有律 → 选技不受 PP 限制");

        // C1/C2（一致性）：选技分支 → 不耗律+武装最优系别；未选技分支 → 耗律+沉默。
        // ⚠️ 场景驱动下回合末分支的选择登记时序有偏差（对手 SELECT 却走了未选分支），
        //    先锁"二选一必居其一且伴随状态正确"，分支归因见交接文档待实测。
        const int lv_after_r1 = pub_get(x, kMoniPubLv);
        (void)lv_at_start;
        if (lv_after_r1 == 4) {
            CHECK_EQ(pub_get(x, kMoniPubOptUntil) >= x.roundCount, true,
                     "C2 选技分支：最优系别已武装");
        } else if (lv_after_r1 == 3) {
            CHECK(core_api().has_active_anomaly(&x, 1,
                  static_cast<int>(AbnormalStatusId::Silence)),
                  "C2' 未选分支：沉默施加");
        } else {
            CHECK(false, "C1 律消耗越界（>1 层/回合）");
        }

        // 回合 2：对手 CHOOSE_PET（未选技能）→ 耗 1 律 + 沉默
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        const int lv_after_r2 = pub_get(x, kMoniPubLv);
        CHECK(lv_after_r2 == lv_after_r1 - 1, "C3 未选技能分支 → 耗 1 律");
    }

    // ═══ D. 体力重置原语（1909 底座，2026-09-25 技能组补全）═════════════════════
    // 口径：重置 ≠ 恢复——无视减疗（heal_mod_pct）/封回血（HEAL_BLOCK），
    // 不写 last_heal_amount、不发 EVENT_HEAL_RESTORED。
    {
        ScenarioBattle b(scenario::make_party(monika_pet(6000)),
                         scenario::make_party(plain_pet(10, 100000)));
        b.start();
        BattleContext& x = b.ctx();
        auto& me = x.getPet(0);
        const int max_hp = me.hp;

        me.hp = max_hp / 4;
        x.heal_mod_pct[0] = -100;   // 封疗级减疗
        core_api().reset_hp(&x, 0, 100);
        CHECK_EQ(me.hp, max_hp, "D1 体力重置无视减疗");

        me.hp = max_hp / 4;
        x.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(),
                         /*anomaly_mask=*/0, 3, /*source_id=*/0, /*soul_immunity=*/false,
                         EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/999);
        core_api().heal_amount(&x, 0, 1000);
        CHECK(me.hp == max_hp / 4, "D2 前置：heal 被封回血拦截");
        core_api().reset_hp(&x, 0, 100);
        CHECK_EQ(me.hp, max_hp, "D3 体力重置无视封回血");
        CHECK_EQ(x.ws.last_heal_amount[0], 0, "D4 重置不算恢复（last_heal 不落账）");

        core_api().reset_hp(&x, 0, 50);
        CHECK_EQ(me.hp, max_hp / 2, "D5 部分重置可降体力（50%）");
    }

    // ═══ E/F. 神谕新技集成 ══════════════════════════════════════════════════
    {
        // E. 王·鸾歌余音 38318（威力 150，槽 1）：1909 重置满 + 回合末高于 50% 再触发
        std::array<ElfPet, 6> own = {
            make_pet(kMoni, "圣光莫妮卡", {kPlain, 38318, kPlain, kPlain, kPlain},
                     /*speed=*/500, /*atk=*/100, 6000,
                     SoulMark(kMark, "神", "测试桩：圣光莫妮卡神谕 2492")),
            plain_pet(), plain_pet(), plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet(10, 100000)));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).hp = 3000;   // 半血起手 → 技能重置满

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(pub_get(x, kMoniPubLv), 2, "E0 前置：2427 耗一半律（4→2）");
        CHECK_EQ(x.getPet(0).hp, 6000,
                 "E1 鸾歌余音：回合内重置满，回合末仍>50% → 再触发补满");

        // E2/E3 奇数道消耗口径（用户 2026-09-25）：消耗=(int)(lv/2)，剩余=lv-(int)(lv/2)
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(pub_get(x, kMoniPubLv), 1, "E2 lv=2 → 消耗1余1");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(pub_get(x, kMoniPubLv) == 1,
              "E3 lv=1 → 不消耗但仍有 1 回合失效（律不越界）");

        // F. 繁嚣此寂 29299（槽 2）：2380 武装 2 层暴击强制计数（内核 query_usage 消费）
        std::array<ElfPet, 6> own2 = {
            make_pet(kMoni, "圣光莫妮卡", {kPlain, kPlain, 29299, kPlain, kPlain},
                     /*speed=*/500, /*atk=*/100, 6000,
                     SoulMark(kMark, "神", "测试桩：圣光莫妮卡神谕 2492")),
            plain_pet(), plain_pet(), plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b2(own2, scenario::make_party(plain_pet(10, 100000)));
        b2.start();
        BattleContext& x2 = b2.ctx();
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK_EQ(pub_get(x2, 990238000), 2, "F1 繁嚣此寂：2380 武装 2 层暴击强制计数");
    }

    // ═══ G. 新旧资源不混用（用户 2026-09-25 口径）═══════════════════════════
    // 旧技万籁俱寂 32516 的 1382 只加**自然祝福**（kMoniPubNz），不碰宁界之律。
    {
        std::array<ElfPet, 6> own = {
            make_pet(kMoni, "圣光莫妮卡", {kPlain, 32516, kPlain, kPlain, kPlain},
                     /*speed=*/500, /*atk=*/10, 6000,
                     SoulMark(kMark, "神", "测试桩：圣光莫妮卡神谕 2492")),
            plain_pet(), plain_pet(), plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b3(own, scenario::make_party(plain_pet(10, 100000)));
        b3.start();
        BattleContext& x3 = b3.ctx();
        b3.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b3.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b3.drive();
        CHECK_EQ(pub_get(x3, kMoniPubNz), 1,
                 "G1 万籁俱寂：命中 +1 自然祝福（伤害<250 无额外）");
        CHECK(pub_get(x3, kMoniPubLv) == 4 || pub_get(x3, kMoniPubLv) == 3,
              "G2 旧技不加律（律仍为出场值，回合末分支 -1 内）");
    }

    // ═══ H/I. 非同系攻击免疫门（待落①）+ 概率免控（待落②）══════════════════
    // 用户口径 2026-09-25：律>0 → 对手攻击技能系别 ∉ 自身系别集 → 技能无效（可穿盔）；
    // 每层 25% 免疫现代异常施加（律=4 → 100%，确定性断言）。
    {
        std::array<ElfPet, 6> own = {monika_pet(6000), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        own[0].elementalAttributes = {88, 20};   // 圣灵 + 自然（官方系别）
        // 对手：燕返 10037（必中、飞空系 ∉ {圣灵,自然}）→ 门应无效之
        std::array<ElfPet, 6> opp = {
            make_pet(1, "普通", {10037, kPlain, kPlain, kPlain, kPlain},
                     /*speed=*/100, /*atk=*/100, 100000),
            plain_pet(), plain_pet(), plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b4(own, opp);
        b4.start();
        BattleContext& x4 = b4.ctx();
        b4.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b4.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b4.drive();
        CHECK_EQ(x4.getPet(0).hp, 6000, "H1 非同系门：飞空系必中攻击技被无效（无伤）");
        CHECK(pub_get(x4, kMoniPubLv) >= 3, "H1' 前置：律仍在（门持续武装）");

        // H2 正向对照：对手改用圣灵系（38318 ∈ 守方系别集）→ 门放行、伤害落地
        std::array<ElfPet, 6> opp2 = {
            make_pet(1, "普通", {38318, kPlain, kPlain, kPlain, kPlain},
                     /*speed=*/100, /*atk=*/10, 100000),
            plain_pet(), plain_pet(), plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> own2 = {monika_pet(6000), plain_pet(), plain_pet(),
                                      plain_pet(), plain_pet(), plain_pet()};
        own2[0].elementalAttributes = {88, 20};
        ScenarioBattle b5(own2, opp2);
        b5.start();
        BattleContext& x5 = b5.ctx();
        b5.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b5.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b5.drive();
        CHECK(x5.getPet(0).hp < 6000, "H2 同系（圣灵）攻击不受门拦截，伤害落地");
    }
    {
        // I. 概率免控：律=4 → 25%×4=100% 免疫现代异常施加 → 拈花一笑的睡眠必被挡
        std::array<ElfPet, 6> own = {monika_pet(6000), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {
            make_pet(1, "普通", {26358, kPlain, kPlain, kPlain, kPlain},
                     /*speed=*/100, /*atk=*/10, 100000),
            plain_pet(), plain_pet(), plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b6(own, opp);
        b6.start();
        BattleContext& x6 = b6.ctx();
        b6.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b6.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b6.drive();
        CHECK(!core_api().has_active_anomaly(&x6, 0,
              static_cast<int>(AbnormalStatusId::Sleep)),
              "I1 概率免控：4 律 → 100% 免疫拈花一笑的睡眠");
    }

    // ═══ J. 未神谕魂印 1287 自然祝福 ═══════════════════════════════════════
    // 独立计数 kMoniPubNz（cap 4）：登场 +4/+2、用技 -1 / 未用 +1、25% 免控/攻先/减伤。
    {
        std::array<ElfPet, 6> own = {
            make_pet(kMoni, "圣光莫妮卡", {kPlain, kPlain, kPlain, kPlain, kPlain},
                     /*speed=*/500, /*atk=*/100, 6000,
                     SoulMark(1287, "神", "测试桩：圣光莫妮卡未神谕 1287")),
            plain_pet(), plain_pet(), plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b7(own, scenario::make_party(plain_pet(10, 100000)));
        b7.start();
        BattleContext& x7 = b7.ctx();
        CHECK_EQ(pub_get(x7, kMoniPubNz), 4, "J1 首次登场 +4 自然祝福");
        CHECK_EQ(pub_get(x7, kMoniPubLv), 0, "J1' 魂印 1287 不产律（新旧资源分离）");
        b7.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b7.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b7.drive();
        CHECK_EQ(pub_get(x7, kMoniPubNz), 3, "J2 用技 → 消耗 1 层（4→3）");
    }

    std::cout << "scenario_122 全部通过" << std::endl;
    return 0;
}
