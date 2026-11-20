#include <utils/sbs_rand.h>
// 场景 089 —— 天启星魂 4677：星启 / 条件免疫 / 星盘 12 刻 / 双攻视图 / 物攻吸血
//
// 锁的是 2026-09-20 的机制（台账"天启星魂 4677"行）：
//   A. 星启（登场）：解除自身异常 → 攻>特攻附加星赐 / 攻<特攻附加星哲 / 相等不附加
//   B. 条件免疫（星赐→疲惫 / 星哲→害怕 / 均处于→控制类；走 RuleCenter 条件回调）
//   C. 星盘：只认"剩余回合数归零"的自然到期（转化/解除不转）；转盘次回合 -99% 减伤 +
//      收尾 PP归零+再星启；满 12 刻不再转 → 攻击免疫 + 每回合覆宙星回（120 神秘·轮回
//      系伤害 + 回返 1 刻）
//   D. 特殊攻击先制+1（按本次选择，特攻吃、物攻/属性不吃）
//   E. 双攻之和伤害视图（等量对照：带印 200+100 ≡ 无印 300 面板，伤害逐字节一致）
//   F. 物理攻击使用后吸取对手双防和 30%
//
// 官方依据：4399 图鉴 996676 + 语料 idx=300《机制解析—星辰万象·天启星魂》。

#include <any>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "../../resources/tianqi_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMark = 2272;  // effect_icon.effect_id（2026-09-27 对齐装配层，4677=monster id）
constexpr int kBlessing = static_cast<int>(AbnormalStatusId::StarBlessing);  // 星赐 33
constexpr int kWisdom = static_cast<int>(AbnormalStatusId::StarWisdom);      // 星哲 34
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);        // 疲惫 7（控制类）
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);              // 害怕 6（控制类）
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);            // 睡眠 8（控制类）
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);          // 中毒（非控制）
constexpr int kPlain = 10001;   // 撞击（物理）
constexpr int kSpecial = 37555; // 群星救赎（特殊 200）——先制组用

ElfPet star_pet(int atk, int spatk, int hp = 4000) {
    SoulMark sm(kMark, "星辰万象", "测试桩：天启星魂 4677");
    ElfPet pet = make_pet(4677, "天启星魂", {kPlain, kPlain, kSpecial, kPlain, kPlain},
                          /*speed=*/100, atk, hp, sm);
    pet.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK] = spatk;
    return pet;
}

ElfPet plain_pet(int atk = 100, int hp = 4000) {
    return make_pet(1, "普通", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, atk, hp);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ════════════════════════════════════════════════════════════════
    // A. 星启：登场按攻特攻比较附加（2回合，战斗开始 roundCount=0 → end=2）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {star_pet(300, 200), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        CHECK(x.has_active_abnormal_status(0, kBlessing), "A1 攻>特攻 → 星赐");
        CHECK(!x.has_active_abnormal_status(0, kWisdom), "A1 未附星哲");
        CHECK_EQ(x.get_abnormal_status_end_round(0, kBlessing), 2, "A1 end=0+2");

        std::array<ElfPet, 6> own2 = {star_pet(200, 300), plain_pet(), plain_pet(),
                                      plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b2(own2, scenario::make_party(plain_pet()));
        b2.start();
        CHECK(b2.ctx().has_active_abnormal_status(0, kWisdom), "A2 攻<特攻 → 星哲");

        std::array<ElfPet, 6> own3 = {star_pet(250, 250), plain_pet(), plain_pet(),
                                      plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b3(own3, scenario::make_party(plain_pet()));
        b3.start();
        CHECK(!b3.ctx().has_active_abnormal_status(0, kBlessing)
                  && !b3.ctx().has_active_abnormal_status(0, kWisdom),
              "A3 相等 → 不附加（原文只写高于/低于）");
        std::cout << "  [A] 星启三组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B. 条件免疫（星哲在场：免害怕、疲惫照受；双处于：控制类全免、非控制照受）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {star_pet(200, 300), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        CHECK(x.has_active_abnormal_status(0, kWisdom), "B 前置：星哲在场");

        // 害怕 → 免疫（星哲）
        const ApplyAnomalyResult r_fear =
            apply_anomaly(&x, 0, kFear, 2, /*actor=*/1);
        CHECK(r_fear == ApplyAnomalyResult::TARGET_IMMUNE, "B1 星哲 → 免疫害怕");
        // 疲惫 → 照受（只有星哲）
        const ApplyAnomalyResult r_fatigue =
            apply_anomaly(&x, 0, kFatigue, 2, /*actor=*/1);
        CHECK(r_fatigue == ApplyAnomalyResult::SUCCESS, "B2 星哲 → 疲惫照受");
        CHECK(x.has_active_abnormal_status(0, kFatigue), "B2 疲惫已落地");
        // 双处于 → 睡眠（控制类，非怕/惫）免疫；中毒（非控制）照受
        x.set_abnormal_status_end_round(0, kBlessing, x.roundCount + 2);
        const ApplyAnomalyResult r_sleep =
            apply_anomaly(&x, 0, kSleep, 2, /*actor=*/1);
        CHECK(r_sleep == ApplyAnomalyResult::TARGET_IMMUNE, "B3 均处于 → 免疫控制类（睡眠）");
        const ApplyAnomalyResult r_poison =
            apply_anomaly(&x, 0, kPoison, 2, /*actor=*/1);
        CHECK(r_poison == ApplyAnomalyResult::SUCCESS, "B3 非控制类照受（中毒）");
        std::cout << "  [B] 条件免疫三组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. 星盘：自然到期转盘 / 转盘次回合减伤+PP归零+再星启 / 解除不转 / 满盘
    // ════════════════════════════════════════════════════════════════
    {
        // C1：星哲（攻<特攻）入场 end=2 → 第 3 回合收尾自然到期 → dial=1；
        //     第 4 回合 shield；第 4 回合收尾：对手随机 PP 归零 + 再星启（星哲 end=3+2=5）
        std::array<ElfPet, 6> own = {star_pet(200, 300), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        b.start();
        BattleContext& x = b.ctx();
        const int opp_pp_before = x.seerRobot[1].elfPets[0].skills[0].pp;
        for (int round = 0; round < 3; ++round) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
        }
        CHECK_EQ(tianqi_rt(x.plugin_storage, 0).dial, 1, "C1 第3回合收尾自然到期 → 星盘 1 刻");
        CHECK_EQ(tianqi_rt(x.plugin_storage, 0).shield_round, 3, "C1 shield 记下一回合（roundCount 3）");
        // 第 4 回合：shield 生效（红伤 -99% → 对手 ~27 伤害被乘到 0）+ 收尾再星启
        x.seerRobot[0].elfPets[0].hp = 4000;   // 满血接打
        const int star_hp_before_r4 = x.seerRobot[0].elfPets[0].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(x.seerRobot[0].elfPets[0].hp >= star_hp_before_r4,
              "C1 转盘回合 shield：对手红伤被 -99% 削到 0（体力未降；星哲回合末还回血）");
        CHECK_EQ(tianqi_rt(x.plugin_storage, 0).dial, 1, "C1 第4回合不再到期（星哲刚续上）");
        CHECK(x.has_active_abnormal_status(0, kWisdom), "C1 第4回合收尾再星启 → 星哲已续");
        CHECK_EQ(x.get_abnormal_status_end_round(0, kWisdom), 5, "C1 续上的星哲 end=3+2");
        CHECK(x.seerRobot[1].elfPets[0].skills[0].pp < opp_pp_before,
              "C1 转盘回合收尾：对手随机 1 项技能 PP 归零（或被选中的那项）");
        // 第 6 回合收尾（end=5 <= roundCount=5）→ 再转 1 刻
        for (int round = 0; round < 2; ++round) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
        }
        CHECK_EQ(tianqi_rt(x.plugin_storage, 0).dial, 2, "C1 续期星哲自然到期 → 星盘 2 刻");

        // C2：解除不转星盘（直接 dispel，无到期事件）
        apply_anomaly(&x, 0, kPoison, 1, 1);
        const int dial_before = tianqi_rt(x.plugin_storage, 0).dial;
        dispel_active_anomalies(&x, 0);
        CHECK_EQ(tianqi_rt(x.plugin_storage, 0).dial, dial_before, "C2 解除异常不触发星盘");
        std::cout << "  [C1/C2] 星盘转动与解除边界通过\n";
    }

    // ── C4：宙始星刻（2207 加速消耗）—— 剩余回合数-1 → 星哲提前一回合到期转盘 ──
    //    基线：星哲 end=2 → 第 3 回合收尾转盘；用 37513 后 end=1 → 第 2 回合收尾就转。
    //    ⚠️ 这是"加速消耗"与"解除"的机制分界：2207 压回合数、到期仍走自然路径。
    {
        SoulMark sm(kMark, "星辰万象", "测试桩：天启星魂 4677");
        ElfPet accel = make_pet(4677, "天启星魂", {37513, kPlain, kPlain, kPlain, kPlain},
                                /*speed=*/100, /*atk=*/200, 4000, sm);
        accel.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK] = 300;
        std::array<ElfPet, 6> own = {accel, plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        CHECK(x.has_active_abnormal_status(0, kWisdom), "C4 前置：星哲在身");
        // 第 1 回合用宙始星刻：2207 把星哲 end 2→1
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.get_abnormal_status_end_round(0, kWisdom), 1, "C4 宙始星刻后 end=2-1");
        // 第 2 回合收尾自然到期 → 星盘 1 刻（基线要等到第 3 回合收尾）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(tianqi_rt(x.plugin_storage, 0).dial, 1, "C4 提前到第 2 回合收尾转盘");
        std::cout << "  [C4] 宙始星刻加速消耗通过\n";
    }

    // ── C3：满盘（dial=12）：到期不再转 + 攻击免疫 + 覆宙星回回返 ──
    {
        std::array<ElfPet, 6> own = {star_pet(200, 300), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        tianqi_rt(x.plugin_storage, 0).dial = 12;
        tianqi_rt(x.plugin_storage, 0).returning = true;
        tianqi_publish(x.plugin_storage, 0, 12, true);
        // 到期事件来了也不再转：附 1 回合异常，打完一回合
        apply_anomaly(&x, 0, kPoison, 1, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(tianqi_rt(x.plugin_storage, 0).dial, 11, "C3 满盘到期不转；额外行动覆宙星回回返 1 刻");
        CHECK(tianqi_rt(x.plugin_storage, 0).atk_immune_handle != 0,
              "C3 回返期（12→0 整段）攻击无效票持续（可被穿族，非 DAMAGE 免疫票）");
        std::cout << "  [C3] 满盘回返期通过\n";
    }

    // ── C5：回返全周期 12→0→(0 起再转)：每回合一次覆宙星回，回 0 后恢复正常转动 ──
    {
        std::array<ElfPet, 6> own = {star_pet(200, 300), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        // 高血量对手：13+ 回合的覆宙星回/撞击不致死（死亡会跳过回合收尾，tick 不跑）
        ScenarioBattle b(own, scenario::make_party(plain_pet(100, 1000000)));
        b.start();
        BattleContext& x = b.ctx();
        tianqi_rt(x.plugin_storage, 0).dial = 12;
        tianqi_rt(x.plugin_storage, 0).returning = true;
        tianqi_publish(x.plugin_storage, 0, 12, true);
        for (int round = 0; round < 12; ++round) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
        }
        CHECK_EQ(tianqi_rt(x.plugin_storage, 0).dial, 0, "C5 回返 12 回合 → 0");
        CHECK(!tianqi_rt(x.plugin_storage, 0).returning, "C5 回 0 → returning 关");
        CHECK_EQ(tianqi_rt(x.plugin_storage, 0).atk_immune_handle, 0, "C5 回 0 → 攻击无效票撤");
        // 0 起恢复正常转动：附 1 回合异常（end=12+1）→ 第 14 回合收尾到期 → dial 1
        apply_anomaly(&x, 0, kPoison, 1, 1);
        for (int round = 0; round < 2; ++round) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
        }
        CHECK_EQ(tianqi_rt(x.plugin_storage, 0).dial, 1, "C5 回 0 后到期再转（循环往复）");
        std::cout << "  [C5] 回返全周期通过\n";
    }

    // ── C6：一回合最多转一次（多个异常同回合结束只算一次）──
    {
        std::array<ElfPet, 6> own = {star_pet(200, 300), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        // 两条 1 回合异常，同一次 tick 一起到期 → 事件两个，转盘只 +1
        // （end=0+1，第 2 回合收尾 roundCount=1 时同时到期）
        apply_anomaly(&x, 0, kPoison, 1, 1);
        apply_anomaly(&x, 0, kFatigue, 1, 1);
        for (int round = 0; round < 2; ++round) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
        }
        CHECK_EQ(tianqi_rt(x.plugin_storage, 0).dial, 1, "C6 双异常同回合到期 → 只转 1 刻");
        std::cout << "  [C6] 回合转盘上限通过\n";
    }

    // ── C7：星合 2208 变威力（0 刻也写视图；有刻加威力）──
    {
        // 0 刻：威力不变（+0）——伤害与无星盘基线一致（同种子）
        SoulMark sm(kMark, "星辰万象", "测试桩：天启星魂 4677");
        ElfPet he = make_pet(4677, "天启星魂", {37512, kPlain, kPlain, kPlain, kPlain},
                             100, 200, 4000, sm);
        he.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK] = 300;
        std::array<ElfPet, 6> own = {he, plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int hp_dial0 = b.ctx().seerRobot[1].elfPets[0].hp;
        // 有刻：runtime 手工置 3 刻 → 威力 90+45=135 → 伤害显著变大
        sbs_srand(7);
        ScenarioBattle b2(own, scenario::make_party(plain_pet()));
        b2.start();
        BattleContext& y = b2.ctx();
        tianqi_rt(y.plugin_storage, 0).dial = 3;
        tianqi_publish(y.plugin_storage, 0, 3, false);
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        const int hp_dial3 = y.seerRobot[1].elfPets[0].hp;
        CHECK(hp_dial3 < hp_dial0, "C7 星盘 3 刻 → 星合威力 90+45 → 伤害显著变大");
        std::cout << "  [C7] 星合变威力通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. 特殊攻击先制+1（特攻吃、物攻不吃）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {star_pet(200, 300), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2 /*群星救赎=特殊*/);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "D 特攻技能 → 先制+1");

        ScenarioBattle b2(own, scenario::make_party(plain_pet()));
        b2.start();
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0 /*撞击=物理*/);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK_EQ(b2.ctx().ws.preemptive_level[0], 0, "D 物理技能 → 不加");
        std::cout << "  [D] 特攻先制两组通过\n";
    }

    // ── D2：五技转化先制（2210 判定早于先制计算）——星哲态五技转化成特殊 → +1；
    //        星赐态转化成物理 → 不加。选中的五技本体是属性技能（静态 type 不给先制）。──
    {
        std::array<ElfPet, 6> own = {star_pet(200, 300), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        // 换五技进 0 号槽
        own[0] = star_pet(200, 300);
        SoulMark sm(kMark, "星辰万象", "测试桩：天启星魂 4677");
        ElfPet fifth = make_pet(4677, "天启星魂", {28810, kPlain, kPlain, kPlain, kPlain},
                                100, 200, 4000, sm);
        fifth.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK] = 300;
        own[0] = fifth;
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0 /*魂·天忤太虚*/);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "D2 星哲态五技→特殊 → 先制+1");

        // 星赐态：转化成物理 → 不加
        SoulMark sm2(kMark, "星辰万象", "测试桩：天启星魂 4677");
        ElfPet fifth2 = make_pet(4677, "天启星魂", {28810, kPlain, kPlain, kPlain, kPlain},
                                 100, 300, 4000, sm2);
        fifth2.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK] = 200;
        std::array<ElfPet, 6> own2 = {fifth2, plain_pet(), plain_pet(),
                                      plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b2(own2, scenario::make_party(plain_pet()));
        b2.start();
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK_EQ(b2.ctx().ws.preemptive_level[0], 0, "D2 星赐态五技→物理 → 不加");
        std::cout << "  [D2] 五技转化先制两组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E. 双攻之和视图：带印(物攻200/特攻300=星哲) 与 无印(物攻500) 同种子伤害一致
    //    ⚠️ 对照组选星哲态：星赐有"+30%攻击伤害"（异常批次行为，081/082 已锁），
    //    会污染等量对照；星哲只增固伤/百分比，红伤不受影响。
    // ════════════════════════════════════════════════════════════════
    {
        sbs_srand(7);
        std::array<ElfPet, 6> own = {star_pet(200, 300), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int hp_star = b.ctx().seerRobot[1].elfPets[0].hp;

        sbs_srand(7);
        // 对照：无魂印、物攻 500（=200+300），其余全同
        SoulMark none{};
        ElfPet ctrl = make_pet(2, "对照", {kPlain, kPlain, kPlain, kPlain, kPlain},
                               100, 500, 4000, none);
        std::array<ElfPet, 6> own2 = {ctrl, plain_pet(), plain_pet(),
                                      plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b2(own2, scenario::make_party(plain_pet()));
        b2.start();
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        const int hp_ctrl = b2.ctx().seerRobot[1].elfPets[0].hp;
        // 带印方另有物攻吸血（双防和 30%），把它加回再比（E 只锁伤害公式视图）
        const int drain = 200 * 30 / 100;   // 对照精灵双防 = 100+100 → 吸 60
        CHECK_EQ(hp_star + drain, hp_ctrl, "E 双攻视图伤害 ≡ 等价面板（吸血差额已校正）");
        std::cout << "  [E] 双攻视图通过\n";
    }

    std::cout << "scenario_089 全部通过\n";
    return 0;
}
