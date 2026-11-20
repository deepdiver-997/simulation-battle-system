#include <utils/sbs_rand.h>
// 场景 090 —— 克雷弗德 4644：德莱赫默伴生构造系统 + 协议四技 + 超越式
//
// 锁的是 2026-09-20 的机制口径（用户拍板，见 crawford_runtime.h / 魂印文件头）：
//   A. 构造点亮（技能命中生效→发布槽位图）+ 守备回合快照（-10%×空缺 加法桶）+ PP 恢复
//   B. 构造II"对手下次攻击-50%"：写己方防守桶（40 快照+50=90）、跨回合重申、攻击管线消费
//   C. 激发快照：+15%×构造 通用增伤桶、不回 PP
//   D. 被控回合：守备→激鸣雷封（麻痹对手+300 护盾）；嗑药回合不触发
//   E. 被控回合：激发→核威强袭（300 点×克制倍数属性伤害）
//   F. 协议III·维：全属性+1（半血翻倍+2）、构造III 先手不耗 PP、每回合吸取 300（半血翻倍 600）、疲惫
//   G. 协议IV·解：麻痹 + 回血=造成伤害；2 回合内对手主动切换→登场者麻痹
//   H. 超越式·变革律：<2 构造吸取 1/3；≥2 复读已点亮构造（免先出手）+ 不耗 PP +
//      视图钳制（无视自身下降/对手提升）+ 击败全恢复
//   I. 隔断式·建构律：抵挡下 1 次攻击（属性不消耗）、未满体力吸取 300、德莱赫默额外行动
//   J. 调和式·进化律：附加攻速总和百分比粉伤 10%→25%→40% 递增封顶
//   K. 与混沌魔君索伦森(1011)交互：反弱成功后 0 弱化被"压回与自身相同"，龙威照发；
//      索全属性-6 减无可减（AT_FLOOR）→ 2026-10-25 口径：反馈未成功、龙威不发
//   L. 核威强袭多属性：混沌次元被电均值 2×=600、远古战斗被机械均值 2×=600
//      （属性直伤；J322 2026-11-05 聚合口径改官方算术平均——原"相乘"4×=1200 作废，
//       电→混沌/电→次元、机械→远古/机械→战斗 四格均 2.0，均值即 2×）
//   M. 构造III 不消耗 PP，但 0 PP 依然不可选（免消耗不豁免选择门）
//   N. ×晨曦之星447（feat/suit-fx 合入）：0 构造守备 -40 + 构造II -50 + 晨曦 -10
//      同桶加算钳恰好 100% → 对手后手高威力技能红伤归零；无套装对照 = 剩 ~10%
//   O. ×神觉·米斯蒂克(4676)（feat/shenjue-mistike 合入）：登场禁令挡下攻击技能的
//      异常+弱化附加 → 协议I·激 反馈被整次拦（stat_drop=IMMUNE）→ 无龙威、无疲惫，
//      失败分支清自身下降（含米斯蒂克②的随机-1）
//   P. 协议II·限的 -50%（2026-09-20 口径修正）：绑定自身——主动切换撤旗清槽、
//      新宠与再上场都不吃；超越式复读构造II 幂等，桶里仍只有一笔（覆盖不叠加）
//
// 官方依据：4399 图鉴 993768 + CHIKA 实测截图（语料 opus 942798742330802179）。

#include <any>
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "../../resources/crawford_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMark = 2231;  // effect_id（2026-09-27 对齐装配层，4644=monster id）
constexpr int kElemChaos = 222;    // 混沌
constexpr int kElemDimension = 17;  // 次元
constexpr int kElemAncient = 18;    // 远古
constexpr int kElemFight = 11;      // 战斗
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);
constexpr int kPlain = 10001;      // 撞击（普通物理 40）
constexpr int kProto1 = 37354;     // 协议I·激
constexpr int kProto2 = 28725;     // 协议II·限
constexpr int kProto3 = 28726;     // 协议III·维
constexpr int kProto4 = 37357;     // 协议IV·解
constexpr int kFifth = 37358;      // 超越式·变革律
constexpr int kBlock = 28724;      // 隔断式·建构律
constexpr int kTune = 37356;       // 调和式·进化律
constexpr int kSorensenMark = 1011;  // 混沌魔君索伦森 魂印（压强化窗口族）
constexpr int kMistiqueMark = 2268;  // 神觉·米斯蒂克 魂印（effect_id，2026-09-27 对齐）
constexpr int kChenxiPieces[4] = {1301035, 1301036, 1301037, 1301038};  // 晨曦之星 447 四件

ElfPet crawford(const std::array<int, 5>& skills, int speed = 200, int atk = 300,
                int hp = 4000, bool with_mark = true) {
    SoulMark sm = with_mark ? SoulMark(kMark, "德莱赫默", "测试桩：克雷弗德 4644") : SoulMark{};
    ElfPet pet = make_pet(4644, "克雷弗德", skills, speed, atk, hp, sm);
    for (Skills& s : pet.skills) {
        s.must_hit = true;   // 测试确定性：官方必中/非必中一律按命中跑
    }
    return pet;
}

ElfPet plain_pet(int atk = 100, int hp = 4000) {
    ElfPet pet = make_pet(1, "普通", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, atk, hp);
    for (Skills& s : pet.skills) {
        s.must_hit = true;
    }
    return pet;
}

ElfPet mistique() {
    SoulMark sm(kMistiqueMark, "神觉", "测试桩：神觉·米斯蒂克 4676");
    ElfPet pet = make_pet(4676, "神觉·米斯蒂克", {kPlain, kPlain, kPlain, kPlain, kPlain},
                          100, 100, 4000, sm);
    for (Skills& s : pet.skills) {
        s.must_hit = true;
    }
    return pet;
}

ElfPet sorensen() {
    SoulMark sm(kSorensenMark, "混沌魔君", "测试桩：混沌魔君索伦森 1011");
    ElfPet pet = make_pet(3414, "混沌魔君索伦森", {kPlain, kPlain, kPlain, kPlain, kPlain},
                          100, 100, 4000, sm);
    for (Skills& s : pet.skills) {
        s.must_hit = true;
    }
    return pet;
}

// 指定双系别分量的守方（核威强袭多属性测试用）。⚠️ 精灵元素存**分量对**（克制矩阵
// 只有叶子属性），组合系 id（如 110 混沌次元）不能当元素用——查表恒 1.0。
ElfPet element_pet(int primary, int secondary) {
    ElfPet pet = plain_pet();
    pet.elementalAttributes = {primary, secondary};
    return pet;
}

void set_all_levels(BattleContext& x, int side, int level) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        x.ability_levels[side][i] = level;
    }
}

bool all_levels_eq(BattleContext& x, int side, int level) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (x.ability_levels[side][i] != level) {
            return false;
        }
    }
    return true;
}

int reduce_sum(BattleContext& x, int side) {
    int sum = 0;
    for (int i = 0; i < 4; ++i) {
        sum += x.ws.damage_reduce_add[side][i];
    }
    return sum;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ════════════════════════════════════════════════════════════════
    // A. 构造点亮 + 守备快照 + PP 恢复
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {crawford({kProto1, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 协议I·激
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(crawford_count_constructs(crawford_constructs(x.getPet(0))), 1,
                 "A1 协议I·激命中 → 构造I 点亮");
        CHECK_EQ(reduce_sum(x, 0), 40, "A1 回合1快照 0 构造 → 守备 -40%（加法桶）");
        CHECK(!crawford_rt(x.getPet(0)).excited_snapshot, "A1 快照=守备");
        CHECK_EQ(x.getPet(0).skills[0].pp, x.getPet(0).skills[0].maxPP,
                 "A1 守备回合结束恢复攻击技能 PP");

        // 回合2：撞击（不点亮）→ 快照 1 构造 → -30%；撞击 PP 同样恢复
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(reduce_sum(x, 0), 30, "A2 回合2快照 1 构造 → -30%");
        CHECK_EQ(x.getPet(0).skills[1].pp, x.getPet(0).skills[1].maxPP, "A2 撞击 PP 恢复");
        std::cout << "  [A] 构造点亮/守备快照/PP恢复 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B. 构造II"对手下次攻击-50%"：写入/重申/消费
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {crawford({kProto2, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 协议II·限（先手）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(reduce_sum(x, 0), 40, "B1 回合1：快照 -40 保留（-50 同回合已被对手攻击消费）");
        CHECK(!crawford_pub_opp_reduce(x.plugin_storage, 0), "B1 对手同回合攻击 → -50% 旗已消费");
        CHECK_EQ(crawford_count_constructs(crawford_constructs(x.getPet(0))), 1,
                 "B1 构造II 点亮");

        // 回合2：快照 1 构造 -30 + -50 跨回合重申 = 80；对手攻击 → 再次消费
        const int hp_before = x.getPet(0).hp;
        const int hp_before2 = x.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!crawford_pub_opp_reduce(x.plugin_storage, 0), "B2 跨回合重申的 -50% 再次被消费");
        CHECK_EQ(reduce_sum(x, 0), 30, "B2 消费后只剩快照 -30");
        CHECK(x.getPet(0).hp < hp_before2, "B2 减半后的伤害仍然落地");
        (void)hp_before;
        std::cout << "  [B] 构造II -50% 写入/重申/消费 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. 激发快照：+15%×构造 增伤桶、不回 PP
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {crawford({kPlain, kPlain, kPlain, kPlain, kPlain}, 200),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        crawford_set_constructs(x.getPet(0), 0b0011);   // 预置 2 构造
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(crawford_rt(x.getPet(0)).excited_snapshot, "C1 快照=激发");
        CHECK_EQ(reduce_sum(x, 0), 0, "C1 激发回合无守备减伤");
        CHECK(x.ws.damage_add_pct[0] > 29.0f && x.ws.damage_add_pct[0] < 31.0f,
              "C1 激发：+15%×2 通用增伤桶");
        CHECK(x.getPet(0).skills[0].pp == x.getPet(0).skills[0].maxPP - 1,
              "C1 激发回合不恢复攻击技能 PP");
        std::cout << "  [C] 激发快照/增伤 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. 被控回合：守备→激鸣雷封；嗑药回合不触发
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {crawford({kPlain, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()),
                         /*medicines0=*/MedicineStock{{{300011, 3}}});
        b.start();
        BattleContext& x = b.ctx();
        CHECK(apply_anomaly(&x, 0, kParalysis, /*duration=*/-1, /*actor=*/1)
                  == ApplyAnomalyResult::SUCCESS,
              "D 前置：克雷弗德麻痹");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 点了技能但无法行动
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(x.ws.extra_action_count[0] == 1, "D1 被控回合德莱赫默额外行动");
        CHECK_EQ(x.getPet(0).shield_bank_.quantity_of(kMark), 300, "D1 激鸣雷封 → 300 护盾");
        CHECK(x.has_active_abnormal_status(1, kParalysis), "D1 激鸣雷封 → 对手麻痹");

        // D2：嗑药回合（非 SELECT_SKILL）不触发
        std::array<ElfPet, 6> own2 = {crawford({kPlain, kPlain, kPlain, kPlain, kPlain}),
                                      plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                      plain_pet()};
        ScenarioBattle b2(own2, scenario::make_party(plain_pet()),
                          MedicineStock{{{300011, 3}}});
        b2.start();
        BattleContext& x2 = b2.ctx();
        apply_anomaly(&x2, 0, kParalysis, -1, 1);
        b2.submit_operation(0, BattleFsm::ActionType::USE_MEDICINE, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK(x2.ws.extra_action_count[0] == 0, "D2 嗑药回合不触发德莱赫默");
        CHECK_EQ(x2.getPet(0).shield_bank_.quantity_of(kMark), 0, "D2 无护盾");
        CHECK(!x2.has_active_abnormal_status(1, kParalysis), "D2 对手未麻痹");
        std::cout << "  [D] 被控守备行动/嗑药分支 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E. 被控回合：激发→核威强袭（300 点×克制倍数）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {crawford({kPlain, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        crawford_set_constructs(x.getPet(0), 0b0011);   // 2 构造 → 激发
        apply_anomaly(&x, 0, kParalysis, -1, 1);
        const int opp_hp_before = x.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(x.ws.extra_action_count[0] == 1, "E1 激发被控 → 额外行动");
        CHECK_EQ(x.getPet(0).shield_bank_.quantity_of(kMark), 0, "E1 激发分支不上护盾");
        const int elems[2] = {0, 0};
        int expected = 300;
        for (const int elem : {5, 6, 122}) {
            const int cand[2] = {elem, 0};
            expected = std::max(expected,
                                static_cast<int>(300 * core_api().restraint_multiplier(
                                                             cand, elems)));
        }
        CHECK_EQ(opp_hp_before - x.getPet(1).hp, expected, "E1 核威强袭 = 300×最高克制倍数");
        std::cout << "  [E] 核威强袭 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // F. 协议III·维：全属性+1（半血翻倍）/ 构造III 不耗 PP / 吸取 300（半血 600）/ 疲惫
    // ════════════════════════════════════════════════════════════════
    {
        // F1：体力充足 → 全属性+1；吸取 300；构造III 先手不耗 PP
        std::array<ElfPet, 6> own = {crawford({kProto3, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).hp = x.getPet(0).numericalBase[NumericalPropertyIndex::HP] - 500;
        const int opp_hp_before = x.getPet(1).hp;
        const int self_hp_before = x.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.ability_levels[0][0], 1, "F1 全属性+1");
        CHECK_EQ(x.getPet(0).skills[0].pp, x.getPet(0).skills[0].maxPP,
                 "F1 构造III 先出手 → 本技能不消耗 PP");
        CHECK_EQ(x.getPet(1).hp, opp_hp_before - 300, "F1 每回合吸取 300");
        CHECK_EQ(x.getPet(0).hp, self_hp_before + 300, "F1 吸取回自身");
        CHECK(x.has_active_abnormal_status(1, kFatigue), "F1 命中后 100% 疲惫");
        CHECK_EQ(crawford_constructs(x.getPet(0)) & 4, 4, "F1 构造III 点亮");

        // F2：半血 → 强化翻倍 +2；吸取翻倍 600
        std::array<ElfPet, 6> own2 = {crawford({kProto3, kPlain, kPlain, kPlain, kPlain}),
                                      plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                      plain_pet()};
        ScenarioBattle b2(own2, scenario::make_party(plain_pet()));
        b2.start();
        BattleContext& x2 = b2.ctx();
        const int max_hp = x2.getPet(0).numericalBase[NumericalPropertyIndex::HP];
        x2.getPet(0).hp = max_hp / 2 - 1;   // 严格低于 1/2
        const int self_before = x2.getPet(0).hp;
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK_EQ(x2.ability_levels[0][0], 2, "F2 半血 → 强化翻倍 +2");
        CHECK_EQ(x2.getPet(0).hp, self_before + 600, "F2 半血 → 吸取翻倍 600");

        // F3：对手厚护罩把吸取全吸掉 → 体力没变 = "未受到固伤" → +240 真伤，护罩照扣
        //     （用户 2026-09-20 实测口径："未受到"看结果体力；回血也不发生）
        std::array<ElfPet, 6> own3 = {crawford({kProto3, kPlain, kPlain, kPlain, kPlain}),
                                      plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                      plain_pet()};
        ScenarioBattle b3(own3, scenario::make_party(plain_pet()));
        b3.start();
        BattleContext& x3 = b3.ctx();
        x3.getPet(1).hood_bank_.add(0, 500, /*source_id=*/7, 0);   // 厚护罩：足够吸掉 300
        const int opp3_before = x3.getPet(1).hp;
        const int self3_before = x3.getPet(0).hp;
        b3.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b3.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b3.drive();
        CHECK_EQ(x3.getPet(1).hp, opp3_before - 240, "F3 吸取被护罩全吸 → 体力没变 → +240 真伤");
        CHECK_EQ(x3.getPet(1).hood_bank_.total(), 200, "F3 护罩照扣 300");
        CHECK_EQ(x3.getPet(0).hp, self3_before, "F3 没吸到 → 自身不回血（对手行动无伤害，同 F1 口径）");
        std::cout << "  [F] 协议III·维 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // G. 协议IV·解：麻痹 + 回血=造成伤害；主动切换 → 登场者麻痹
    // ════════════════════════════════════════════════════════════════
    {
        // G1：麻痹 + 回血=造成伤害（双方无增减伤 → 相等式断言）
        std::array<ElfPet, 6> own = {crawford({kProto4, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).hp = 1500;   // 留足回血空间（上限 4000）
        const int self_before = x.getPet(0).hp;
        const int opp_before = x.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(x.has_active_abnormal_status(1, kParalysis), "G1 命中后 100% 麻痹");
        CHECK_EQ(x.getPet(0).hp - self_before, opp_before - x.getPet(1).hp,
                 "G1 回血量 == 最终造成伤害");

        // G2：2 回合内对手主动切换 → 登场精灵麻痹
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK(x.has_active_abnormal_status(1, kParalysis), "G2 主动切换 → 登场者 100% 麻痹");
        std::cout << "  [G] 协议IV·解/切换麻痹 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // H. 超越式·变革律
    // ════════════════════════════════════════════════════════════════
    {
        // H1：<2 构造 → 吸取对手最大体力 1/3（4000/3 = 1333）
        std::array<ElfPet, 6> own = {crawford({kFifth, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).hp = 2000;   // 留吸取空间（2000 > 1333 不触顶）
        // 预置对手麻痹：<2 构造的先制-1 使克雷弗德后手，麻痹让对手不产生伤害扰动
        apply_anomaly(&x, 1, kParalysis, -1, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.getPet(0).hp, 2000 + 4000 / 3, "H1 <2 构造：吸取对手最大体力 1/3 回自身");

        // H2：≥2 构造 → 复读构造（免先出手）：不耗 PP + -50% 桶 + +50% 增伤；
        //     视图钳制：自身 -2 / 对手 +2 等价于无强化基线
        std::array<ElfPet, 6> own2 = {crawford({kFifth, kPlain, kPlain, kPlain, kPlain}),
                                      plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                      plain_pet()};
        ScenarioBattle b2(own2, scenario::make_party(plain_pet()));
        b2.start();
        BattleContext& x2 = b2.ctx();
        crawford_set_constructs(x2.getPet(0), 0b1111);   // 四构造全亮（激发 + 复读源）
        apply_anomaly(&x2, 1, kParalysis, -1, 0);   // 对手不动 → -50% 不被同回合消费
        x2.ability_levels[0][0] = -2;   // 自身攻击 -2（钳为 0）
        x2.ability_levels[1][0] = 2;    // 对手防御 +2（钳为 0）
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK_EQ(x2.getPet(0).skills[0].pp, x2.getPet(0).skills[0].maxPP,
                 "H2 ≥2 构造：本技能不消耗 PP（构造III 复读）");
        CHECK(reduce_sum(x2, 0) >= 50, "H2 构造II 复读 → -50% 写入己方防守桶");
        CHECK(x2.ws.damage_add_pct[0] > 49.0f, "H2 构造IV 复读 → +50% 增伤（含激发 30 则更大）");
        CHECK_EQ(x2.ws.view_levels[0][0], -2, "H2 AFTER_ACTION 还原自身视图");
        CHECK_EQ(x2.ws.view_levels[1][0], 2, "H2 AFTER_ACTION 还原对手视图");

        // H3：击败对手 → 恢复全部体力和所有技能 PP
        std::array<ElfPet, 6> own3 = {crawford({kFifth, kPlain, kPlain, kPlain, kPlain}),
                                      plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                      plain_pet()};
        std::array<ElfPet, 6> opp3 = scenario::make_party(plain_pet());
        ScenarioBattle b3(own3, opp3);
        b3.start();
        BattleContext& x3 = b3.ctx();
        x3.getPet(1).hp = 1;    // 一击必杀
        x3.getPet(0).hp = 100;  // 残血
        b3.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b3.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b3.drive();
        // 对手阵亡 → FSM 停在补位等待；先补位驱动（顺带冲掉排队中的死亡事件），再断言
        b3.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b3.drive();
        CHECK_EQ(x3.getPet(0).hp, x3.getPet(0).numericalBase[NumericalPropertyIndex::HP],
                 "H3 击败对手 → 恢复全部体力");
        CHECK_EQ(x3.getPet(0).skills[0].pp, x3.getPet(0).skills[0].maxPP,
                 "H3 击败对手 → PP 全恢复（消耗后回满）");
        std::cout << "  [H] 超越式·变革律 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // I. 隔断式·建构律：抵挡 1 次 / 吸取 300 / 德莱赫默额外行动
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {crawford({kBlock, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        // 对手高攻：击穿 300 护盾后仍能落血，让"抵挡已消费"可辨（否则护盾吸伤干扰断言）
        ElfPet strong_opp = plain_pet(2000);
        std::array<ElfPet, 6> opp_party = {strong_opp, strong_opp, strong_opp,
                                           strong_opp, strong_opp, strong_opp};
        ScenarioBattle b(own, opp_party);
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).hp = 3500;   // 未满体力 → 吸取 300
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.getPet(0).shield_bank_.quantity_of(kMark), 300, "I1 德莱赫默守备行动 → 300 护盾");
        CHECK(x.has_active_abnormal_status(1, kParalysis), "I1 德莱赫默守备行动 → 麻痹");
        CHECK_EQ(x.getPet(0).skills[0].pp, x.getPet(0).skills[0].maxPP - 1,
                "I1 隔断式 PP 正常消耗");
        // 对手麻痹被清除后攻击 → 被抵挡（0 伤）；再攻击 → 落地
        core_api().dispel_active_anomalies(&x, 1);
        const int hp_before = x.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.getPet(0).hp, hp_before, "I2 抵挡下 1 次对手的攻击（0 伤）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(x.getPet(0).hp < hp_before, "I3 次回合攻击照常落地（抵挡已消费）");
        std::cout << "  [I] 隔断式·建构律 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // J. 调和式·进化律：附加粉伤比例 10% → 25% → 40% 递增封顶
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {crawford({kTune, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(crawford_moves_rt(x.plugin_storage, 0).tune_1574_pct, 25, "J1 首用后 10+15=25");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(crawford_moves_rt(x.plugin_storage, 0).tune_1574_pct, 40, "J2 再用 25+15=40");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(crawford_moves_rt(x.plugin_storage, 0).tune_1574_pct, 40, "J3 封顶 40");
        std::cout << "  [J] 调和式·进化律 递增封顶 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // K. 与混沌魔君索伦森(1011 "对手能力等级不能超过自身")交互
    // ════════════════════════════════════════════════════════════════
    {
        // K1：双方全属性-1。克雷先手协议·激：反弱成功（索 -1→-2、自身清 0），
        //     随后索魂印发力把克雷的 0 压回与自身相同的 -2；反馈既已成功 → 龙威照发。
        std::array<ElfPet, 6> own = {crawford({kProto1, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        std::array<ElfPet, 6> opp = {sorensen(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        b.start();
        BattleContext& x = b.ctx();
        set_all_levels(x, 0, -1);
        set_all_levels(x, 1, -1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 协议I·激
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(all_levels_eq(x, 1, -2), "K1 反弱成功：索伦森 -1 → -2");
        CHECK(all_levels_eq(x, 0, -2), "K1 索魂印发力：克雷清零后的 0 被压回与自身相同的 -2");

        // 龙威（对手下1次使用的技能无效）：清掉疲惫后索出招 → 被无效（克雷零掉血）
        core_api().dispel_active_anomalies(&x, 1);
        const int hp_before = x.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 撞击
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.getPet(0).hp, hp_before, "K1 反馈成功的龙威：索伦森下次技能无效（零掉血）");

        // K2：索全属性-6 减无可减——stat_drop 全 AT_FLOOR → 2026-10-25 口径：反馈未成功
        // → 龙威不发（索下次技能正常生效）
        std::array<ElfPet, 6> own2 = {crawford({kProto1, kPlain, kPlain, kPlain, kPlain}),
                                      plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                      plain_pet()};
        std::array<ElfPet, 6> opp2 = {sorensen(), plain_pet(), plain_pet(), plain_pet(),
                                      plain_pet(), plain_pet()};
        ScenarioBattle b2(own2, opp2);
        b2.start();
        BattleContext& x2 = b2.ctx();
        set_all_levels(x2, 0, -1);
        set_all_levels(x2, 1, -6);
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK(all_levels_eq(x2, 1, -6), "K2 索 -6 减无可减：等级表面没变（AT_FLOOR）");
        CHECK(all_levels_eq(x2, 0, -6), "K2 克雷 0 被压回与自身相同的 -6");
        core_api().dispel_active_anomalies(&x2, 1);
        const int hp_before2 = x2.getPet(0).hp;
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK(x2.getPet(0).hp < hp_before2,
              "K2 全 AT_FLOOR = 反馈未成功 → 龙威不发（索下次技能正常生效）");
        std::cout << "  [K] 索伦森交互（反弱/压制/龙威） 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // L. 核威强袭多属性：300 点 × 最高克制倍数的属性直伤
    // ════════════════════════════════════════════════════════════════
    {
        // L1：守方混沌次元 {chaos,dimension}：电→混沌 2.0 / 电→次元 2.0 均值 → 600（J322）
        std::array<ElfPet, 6> own = {crawford({kPlain, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        std::array<ElfPet, 6> opp = {element_pet(kElemChaos, kElemDimension), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        b.start();
        BattleContext& x = b.ctx();
        crawford_set_constructs(x.getPet(0), 0b0011);   // 2 构造 → 激发
        apply_anomaly(&x, 0, kParalysis, -1, 1);                // 被控 → 德莱赫默核威强袭
        const int hp_before = x.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(hp_before - x.getPet(1).hp, 600,
                 "L1 混沌次元被电 → 300×2 = 600（两格 2.0 均值，J322 口径）");

        // L2：守方远古战斗 {ancient,fight}：机械→远古 2.0 / 机械→战斗 2.0 均值 → 600（J322）
        std::array<ElfPet, 6> own2 = {crawford({kPlain, kPlain, kPlain, kPlain, kPlain}),
                                      plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                      plain_pet()};
        std::array<ElfPet, 6> opp2 = {element_pet(kElemAncient, kElemFight), plain_pet(),
                                      plain_pet(), plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b2(own2, opp2);
        b2.start();
        BattleContext& x2 = b2.ctx();
        crawford_set_constructs(x2.getPet(0), 0b0011);
        apply_anomaly(&x2, 0, kParalysis, -1, 1);
        const int hp_before2 = x2.getPet(1).hp;
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK_EQ(hp_before2 - x2.getPet(1).hp, 600,
                 "L2 远古战斗被机械 → 300×2 = 600（两格 2.0 均值，J322 口径）");
        std::cout << "  [L] 核威强袭多属性 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // M. 构造III 不消耗 PP，但 0 PP 依然不可选（免消耗 ≠ 无限用）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {crawford({kProto3, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).skills[0].pp = 0;
        CHECK(x.getPet(0).skills[0].query_selectable(&x, 0) == SkillSelectionResult::PP_EMPTY,
              "M1 协议III·维 0PP：免消耗不豁免选择门");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(!x.operation_collected[0], "M2 提交 0PP 技能被拒（要求重选）");
        // 引擎语义：拒绝后 FSM 继续推进（真实对局由服务器重新提示玩家重选；
        // 该门只拦"0PP 被提交"，不提供重试循环——重试属于控制块职责）。
        std::cout << "  [M] 0PP 不可选/免消耗分立 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // N. ×晨曦之星447：0 构造守备 -40 + 构造II -50 + 晨曦 E -10 → 加算钳 100% 归零
    //    （对手后手高威力攻击；晨曦 F"对手攻击后回血"应 +100 —— 归零与回血同屏验证）
    // ════════════════════════════════════════════════════════════════
    {
        // N1：穿晨曦 → 红伤归零，只剩 F 的 +100 回血
        std::array<ElfPet, 6> own = {crawford({kProto2, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ElfPet heavy = plain_pet(2000);   // 高威力：攻 2000，裸伤 ~640
        std::array<ElfPet, 6> opp = {heavy, plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        std::array<std::vector<int>, 2> equipment{
            std::vector<int>(kChenxiPieces, kChenxiPieces + 4), std::vector<int>{}};
        ScenarioBattle b(own, opp, {}, {}, equipment);
        b.start();
        BattleContext& x = b.ctx();
        // 晨曦数值层：背包全体体力+60 → max=4060。扣 200（仍高于半血，E 保持 10% 档），
        // 让 F"对手攻击后回血 100"在断言里可见。
        x.getPet(0).hp = x.getPet(0).numericalBase[NumericalPropertyIndex::HP] - 200;
        const int hp_before = x.getPet(0).hp;
        const int opp_hp_before = x.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 协议II·限（先手）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.getPet(0).hp, hp_before + 100,
                 "N1 守备-40+构造II-50+晨曦-10 = 100% 加算钳 → 红伤归零；晨曦F 回血 +100");
        CHECK_EQ(x.getPet(1).hp, opp_hp_before, "N1 协议II·限为属性技，对手不掉血（sanity）");

        // N2：无套装对照 → 桶只剩 -90，晨曦缺位 → 红伤 ~10% 落地（非零、非致命）
        std::array<ElfPet, 6> own2 = {crawford({kProto2, kPlain, kPlain, kPlain, kPlain}),
                                      plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                      plain_pet()};
        std::array<ElfPet, 6> opp2 = {heavy, plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                      plain_pet()};
        ScenarioBattle b2(own2, opp2);
        b2.start();
        BattleContext& x2 = b2.ctx();
        const int hp_before2 = x2.getPet(0).hp;
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        const int loss = hp_before2 - x2.getPet(0).hp;
        CHECK(loss > 30 && loss < 120,
              "N2 无套装对照：-90% 后仍剩 ~10% 红伤落地（红伤归零依赖晨曦 E 并桶）");
        std::cout << "  [N] ×晨曦之星：减伤加算钳 100% 归零 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // O. ×神觉·米斯蒂克(4676)：登场禁令拦下攻击技能的异常/弱化附加 → 无反弱龙威
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {crawford({kProto1, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        std::array<ElfPet, 6> opp = {mistique(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        b.start();
        BattleContext& x = b.ctx();
        set_all_levels(x, 0, -1);   // 克雷自带 -1（反馈素材）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 协议I·激
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(all_levels_eq(x, 1, 0), "O1 登场禁令：反馈的弱化被整次挡下，米斯蒂克等级不变");
        CHECK(all_levels_eq(x, 0, 0), "O1 反馈失败分支：自身下降解除（含②的随机-1）");
        CHECK(!x.has_active_abnormal_status(1, kFatigue),
              "O1 禁令同拦异常附加：1398 的疲惫未落地");
        // 无龙威端到端：r2 米斯蒂克出招照常落地（没有封属票）→ 克雷掉血
        const int hp_before = x.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(x.getPet(0).hp < hp_before, "O2 无龙威：米斯蒂克攻击照常落地");
        std::cout << "  [O] ×米斯蒂克：禁令拦反弱/无龙威 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // P. 协议II·限的 -50%：绑定自身（切换失效）+ 复读幂等（覆盖不叠加）
    // ════════════════════════════════════════════════════════════════
    {
        // P1：限 → 对手嗑药（旗跨回合保留）→ 次回合克雷主动切换 → 对手打登场的新宠：
        //     无 -50%（EVENT_SWAP 已撤旗清槽），克雷再上场也不恢复。
        std::array<ElfPet, 6> own = {crawford({kProto2, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 限（点亮构造II+挂旗）
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);   // 对手嗑药：不攻击
        b.drive();
        CHECK(crawford_pub_opp_reduce(x.plugin_storage, 0), "P1 限后旗 pending（嗑药回合未消费）");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);     // 克雷主动切换下场
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手打登场的新宠
        b.drive();
        CHECK(!crawford_pub_opp_reduce(x.plugin_storage, 0), "P1 主动切换 → 旗撤销");
        int slot_sum = 0;
        for (int i = 0; i < 4; ++i) {
            slot_sum += x.ws.damage_reduce_add[0][i];
        }
        CHECK_EQ(slot_sum, 0, "P1 切换 → 减伤桶槽已清（新宠不吃 -50%）");

        // P2：限 → 对手嗑药 → 次回合超越式复读构造II → **幂等**：桶里仍只有一笔 50
        //     （修正前复读会抢第二个槽，那一刀会被两笔 -50% 叠到红伤归零）。
        std::array<ElfPet, 6> own2 = {crawford({kProto2, kFifth, kPlain, kPlain, kPlain}),
                                      plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                      plain_pet()};
        ScenarioBattle b2(own2, scenario::make_party(plain_pet()));
        b2.start();
        BattleContext& x2 = b2.ctx();
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 限
        b2.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b2.drive();
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 超越式（复读构造II）
        b2.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);   // 对手再嗑药：不消费
        b2.drive();
        int slot_sum2 = 0;
        for (int i = 0; i < 4; ++i) {
            slot_sum2 += x2.ws.damage_reduce_add[0][i];
        }
        CHECK(crawford_pub_opp_reduce(x2.plugin_storage, 0), "P2 复读后旗仍 pending");
        // 80 = 守备(4-1构造)×10=30 + 限 -50；复读修正前会再抢一槽 → 130。
        CHECK_EQ(slot_sum2, 80, "P2 复读幂等：限的 -50% 仍只有一笔（覆盖不叠加）");
        std::cout << "  [P] ×限绑定自身/复读幂等 通过\n";
    }

    std::cout << "scenario_090 全部通过" << std::endl;
    return 0;
}
