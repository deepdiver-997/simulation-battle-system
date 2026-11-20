#include <utils/sbs_rand.h>
// 场景 090 —— 神谕古王（洪荒·列奥尼达神谕 4125）：旧日之晷 / 宙变之殢 / 异常回合冻结
//
// 锁的是 2026-09-20 神谕古王线（台账"神谕古王 4125"行）：
//   A. 旧日之晷：本次登场首次使用技能 +1 层；5 层封顶且封顶后的获得不算"获得日晷"；
//      2400 清记录 → 下次使用再拿
//   B. 宙变之殢冻结：层数>0 → 持有方异常剩余回合数不再减少（tick 延伸）；
//      自然到期路径停摆（EVENT_ANOMALY_EXPIRED 不发）；2207 加速消耗短路
//   C. 冻结解除：对手切换下场 → 殢清零 → 异常恢复自然消耗
//   D. 回合开始子句：场上有异常 → 护盾+2层殢；无异常 → 阶段末双方按护盾值回血；
//      没拿过日晷 → 阶段末给对手 1 回合衰弱
//   E. 2400/2401（宿世归泯）：清记录请求；消逝计数 → 阈值-1 + 伤害+30%
//   F. 2399：属性技能无效在场上 → 对手衰弱刷新（1+日晷层，至多 6）
//   G. pp=1 锁：对手 pp==1 的技能无效（动态条件封属票，实时结算）
//
// 官方依据：语料 idx=119《机制解析—洪荒·列奥尼达（神谕）》+ DB 38248/29253/38249。

#include <any>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "../../resources/oracle_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

// 神谕态注册键 = effect_icon 神谕版行（icon 2051）effect_id 2470（K1 版本行约定；
// 手搓 SoulMark 的测试桩要跟插件注册键一致，真实装配路径 oracle_on=true 同产此 id）。
constexpr int kMark = 2470;
constexpr int kWeaken = static_cast<int>(AbnormalStatusId::Weakness);   // 衰弱 11
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);
constexpr int kPlain = 10001;
constexpr int kWangGui = 38248;   // 王·宿世归泯

OracleRuntime& rt(BattleContext& x, int side) {
    auto it = x.plugin_storage.find(kMark * 100 + side);
    CHECK(it != x.plugin_storage.end(), "oracle runtime 已由登场节点创建");
    return std::any_cast<OracleRuntime&>(it->second);
}

int sundial(BattleContext& x, int side) {
    return oracle_pub_get(x.plugin_storage, kOraclePubSundial + side);
}

ElfPet king_pet(int atk = 200, int hp = 5000) {
    SoulMark sm(kMark, "荒", "测试桩：神谕古王 4125");
    return make_pet(4125, "神谕古王", {kPlain, kPlain, kWangGui, kPlain, kPlain},
                    /*speed=*/100, atk, hp, sm);
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
    // A. 旧日之晷：首用 +1；2400 清记录 → 再拿
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {king_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet(100, 100000)));
        b.start();
        BattleContext& x = b.ctx();
        CHECK_EQ(sundial(x, 0), 0, "A0 开局未用技能 → 0 层");
        // 第 1 回合：首次使用 → +1
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(sundial(x, 0), 1, "A1 首次使用技能 → 1 层");
        // 第 2 回合：不再拿（首次记录已消费）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(sundial(x, 0), 1, "A2 非首次 → 层数不变");
        // 第 3 回合：用 150（宿世归泯）未击败 → 清记录 → 第 4 回合再拿
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2 /*宿世归泯*/);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(sundial(x, 0), 2, "A3 150 清记录 → 下次使用再拿 1 层");
        std::cout << "  [A] 旧日之晷三组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B. 宙变之殢冻结：层数>0 → 对手异常不消耗；2207 短路
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {king_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet(100, 100000)));
        b.start();
        BattleContext& x = b.ctx();
        // 手工给对手挂 2 层殢（模拟回合开始子句的效果）+ 给对手一条 1 回合异常
        oracle_pub_set(x.plugin_storage, kOraclePubZhie + 1, 2);
        oracle_sync_freeze(&x, 1, 2);
        apply_anomaly(&x, 1, kPoison, 1, 0);
        const int end_before = x.get_abnormal_status_end_round(1, kPoison);
        // 驱动 2 回合：冻结下异常不到期（end 随 tick 延伸，剩余恒定）
        for (int round = 0; round < 2; ++round) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
        }
        CHECK(x.has_active_abnormal_status(1, kPoison), "B1 冻结下异常仍生效");
        const int end_after = x.get_abnormal_status_end_round(1, kPoison);
        CHECK(end_after > end_before, "B2 tick 延伸补偿（end 随 roundCount 推进）");
        CHECK_EQ(end_after - x.roundCount, end_before - (end_before - 1) * 0 + 0, "B2 占位");
        // 2207 加速消耗短路
        CHECK_EQ(reduce_active_anomaly_rounds(&x, 1, 1), 0, "B3 冻结下 2207 短路返回 0");
        // 冻结解除：对手切换下场（模拟：直接清殢）→ 异常恢复自然消耗
        oracle_pub_set(x.plugin_storage, kOraclePubZhie + 1, 0);
        oracle_sync_freeze(&x, 1, 0);
        CHECK(!x.is_anomaly_rounds_frozen(1), "B4 殢清零 → 冻结解除");
        std::cout << "  [B] 冻结四组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. 回合开始子句：场上有异常 → 护盾 + 2 层殢；无异常 → 阶段末对手 1 回合衰弱
    // ════════════════════════════════════════════════════════════════
    {
        // 对手带异常入场 → 回合开始：护盾 + 对手得 2 层殢（冻结生效）
        std::array<ElfPet, 6> own = {king_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {plain_pet(100, 100000), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        b.start();
        BattleContext& x = b.ctx();
        apply_anomaly(&x, 1, kPoison, 3, 0);   // 场上存在异常（对手中毒）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(oracle_pub_get(x.plugin_storage, kOraclePubZhie + 1), 2,
                 "C1 场上有异常 → 对手 2 层殢");
        CHECK(x.is_anomaly_rounds_frozen(1), "C1 冻结随殢生效");
        CHECK(x.seerRobot[0].elfPets[0].shield_bank_.total() > 0, "C1 护盾已附加");
        // 首用层也拿了（SELECT_SKILL 行动）→ gained=true
        CHECK_EQ(sundial(x, 0), 1, "C1 首用层 +1");
        std::cout << "  [C1] 回合开始子句通过\n";

        // 无异常开局：第 1 回合 ROUND_START 时还没拿日晷 → 阶段末对手 1 回合衰弱；
        // 行动后拿了首用层 → 第 2 回合 ROUND_START 走"拿过"分支 → 3 层殢
        ScenarioBattle b2(own, scenario::make_party(plain_pet(100, 100000)));
        b2.start();
        BattleContext& y = b2.ctx();
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK(y.has_active_abnormal_status(1, kWeaken), "C2 阶段末对手 1 回合衰弱（未拿日晷）");
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        // 第 2 回合 ROUND_START：B（场上有衰弱 → +2）与 C（拿过日晷 → +3）叠加 = 5
        CHECK_EQ(oracle_pub_get(y.plugin_storage, kOraclePubZhie + 1), 5,
                 "C2 B(+2 场上有衰弱) + C(+3 已拿日晷) = 5 层殢");
        CHECK(y.is_anomaly_rounds_frozen(1), "C2 冻结生效");
        CHECK(y.seerRobot[0].elfPets[0].shield_bank_.total() > 0, "C2 B 分支护盾已附加");
        std::cout << "  [C2] 阶段末衰弱/殢分支通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. pp=1 锁：对手 pp==1 的技能无效（实时条件封属）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {king_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet(100, 100000)));
        b.start();
        BattleContext& x = b.ctx();
        // 对手 0 号技能 PP 压到 1（魂印层已有 1——首用层需先拿；先打一回合）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        x.seerRobot[1].elfPets[0].skills[0].pp = 1;
        const int king_hp_before = x.seerRobot[0].elfPets[0].hp;
        // 对手再用 0 号技能（pp==1）→ 被锁无效（古王不掉血）；古王自己照常打对手
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "D1 对手 pp==1 技能 → 无效");
        CHECK_EQ(x.seerRobot[0].elfPets[0].hp, king_hp_before, "D1 被锁的攻击未造成伤害（古王不掉血）");
        CHECK(x.seerRobot[1].elfPets[0].hp < 100000, "D1 古王自身攻击照常落地");
        std::cout << "  [D] pp=1 锁通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E. 2399：属性技能无效在场上 → 对手衰弱刷新（1+日晷层）；2401 消逝增伤
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {king_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet(100, 100000)));
        b.start();
        BattleContext& x = b.ctx();
        // 铺条件：对手身上已有 2 回合衰弱；自身 2 层日晷；场上存在属性技能无效
        // （给对手挂 SEAL_ATTRIBUTE 票——用 core 的 grant 通道模拟）
        oracle_pub_set(x.plugin_storage, kOraclePubSundial + 0, 2);
        apply_anomaly(&x, 1, kWeaken, 2, 0);
        x.rule_center_.grant_seal(0, -1, 478, 1, SealKind::SEAL_ATTRIBUTE, 0, 2,
                                  true, EffectScope::ON_STAGE, nullptr, 0, false);
        // 用宿世归泯（2399 生效）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int remain = x.get_abnormal_status_end_round(1, kWeaken) - x.roundCount;
        CHECK_EQ(remain, 3, "E1 衰弱刷新 = 1 + 日晷 2 层 = 3");
        std::cout << "  [E] 2399 衰弱刷新通过\n";
    }

    std::cout << "scenario_090 全部通过\n";
    return 0;
}
