// 场景 3750 —— K231 登场免麻痹+失神+属技无效/反击窗族（1396，soul_lib/k231_paralysis_immune_distraction.cpp）
//（2026-10-08，夜跑 k-prod-6；号段 3750-3759 → T3750 一块六例）
//
// 断言口径（审计槽精确 + 落地分支一致性，无随机硬断言）：
//   - a 免麻痹票 mask 特异性：直铺麻痹 → TARGET_IMMUNE；直铺烧伤 → SUCCESS
//     （mask 只免麻痹的精确证据）；无副作用Burn 照常活跃。
//   - b 失神申报审计（row1）：对手红伤命中 → 失神申报槽 == 50（db[0] 消费；
//     50% 落地不硬断言，K12 运行期自校准口径）。
//   - c 封属技 count=1：己方攻击命中 → 对手高速移动(20017) 第一次无效（速不动）、
//     第二次落地（def+1）——count=1 消费证据。
//   - d 反击粉伤+记忆：R1 对手红伤 D（记忆审计=D）→ R2 己方缩头命中 →
//     对手掉血 = D×60/100 精确（row2 db[2]=60 消费；PERCENT_VALUE 粉伤）。
//   - e 害怕爬坡（row1 基 50 步 20）：R1 申报=50；R2 申报 = R1 落地 ? 50 : 70；
//     R3 链式同判——审计梯度一致性（落地观测 = 对手 Fear 状态活跃）。
//     记忆=0（未受击）→ 无粉伤（last_red>0 门负例，对手满血不变）。
//   - f row2 审计互锚：失神申报 80、害怕基 60（db[0]/db[3] 行间互锚消费）。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;      // 拍打：攻击技 filler
constexpr int kGaoSu = 20017;    // 高速移动：属性技 filler（自身速+2，2830 实证 effect 4 可用）
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);
constexpr int kLastRedBase = 991428000;      // +owner：反击记忆（实现文件同锚）
constexpr int kFearAuditBase = 991430000;    // +owner：害怕申报%
constexpr int kDistractAuditBase = 991431000; // +owner：失神申报%

SoulMark make_mark(const std::vector<int>& row_args) {
    SoulMark sm(1396, "K231 魂印", "test", EffectArgs(row_args));
    sm.monster_id = 3813;   // 哀伤公爵·阿洛希尔
    return sm;
}

// 印主：魂印宠（skills[1]=缩头 属性技），先手。
ElfPet mark_pet(const std::vector<int>& row_args) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kGaoSu, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, 99999, make_mark(row_args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 对手：skills[4]=缩头；可攻击可属性。
ElfPet foe_pet(int atk) {
    ElfPet p = make_pet(2, "对手", {kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, atk, 99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

int audit_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

void one_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<int> row1{50, 1, 50, 50, 20, 1};
    const std::vector<int> row2{80, 1, 60, 60, 20, 1};

    // ══ T3750.a 登场免麻痹（mask 特异性：麻痹拒、烧伤收）══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Paralysis),
                               3, 1),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T3750.a 麻痹被免疫票拒绝");
        CHECK_EQ(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Burn), 3, 1),
                 ApplyAnomalyResult::SUCCESS, "T3750.a 非麻痹异常不受票影响");
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Burn)),
              "T3750.a 烧伤落地活跃");
        std::cout << "  [T3750.a] 登场免麻痹+mask特异性 通过\n";
    }

    // ══ T3750.b 受击失神申报审计（row1=50）══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(foe_pet(300)));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(audit_int(ctx, kDistractAuditBase + 0), 50, "T3750.b 失神申报 = db[0]");
        CHECK(audit_int(ctx, kLastRedBase + 0) > 0, "T3750.b 反击记忆已写入");
        std::cout << "  [T3750.b] 失神申报审计 通过\n";
    }

    // ══ T3750.c 攻击命中 → 封对手 1 次属技（count=1 消费）══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[1][4], 2, "T3750.c 对照：无票时高速移动落地 速+2");
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        one_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[1][4], 2, "T3750.c 第 1 次属技被无效（速不动）");
        one_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[1][4], 4, "T3750.c 票已耗 → 第 2 次属技落地 速+2");
        std::cout << "  [T3750.c] 封属 count=1 通过\n";
    }

    // ══ T3750.d 反击粉伤+记忆（row2 db[2]=60）══
    {
        ScenarioBattle b(make_party(mark_pet(row2)), make_party(foe_pet(2500)));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 1);
        const int mem = audit_int(ctx, kLastRedBase + 0);
        CHECK(mem > 0, "T3750.d 记忆 = R1 受击红伤");
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(foe_hp0 - ctx.getPet(1).hp, mem * 60 / 100, "T3750.d 反击粉伤 = 记忆×60%");
        std::cout << "  [T3750.d] 反击粉伤+记忆 通过（记忆=" << mem << "）\n";
    }

    // ══ T3750.e 害怕爬坡审计梯度（row1 基 50 步 20）+ 记忆 0 无粉伤 ══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(ctx.getPet(1).hp, foe_hp0, "T3750.e 记忆 0 → 无反击粉伤");
        int prev = audit_int(ctx, kFearAuditBase + 0);
        CHECK_EQ(prev, 50, "T3750.e R1 申报 = 基础 50");
        for (int round = 2; round <= 3; ++round) {
            const bool landed_prev = ctx.has_active_abnormal_status(1, kFear);
            one_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, -1);
            const int cur = audit_int(ctx, kFearAuditBase + 0);
            CHECK_EQ(cur, landed_prev ? 50 : std::min(100, prev + 20),
                     "T3750.e 爬坡梯度一致性");
            prev = cur;
        }
        std::cout << "  [T3750.e] 害怕爬坡梯度 通过（终值=" << prev << "）\n";
    }

    // ══ T3750.f row2 审计互锚（失神 80 / 害怕基 60）══
    {
        ScenarioBattle b(make_party(mark_pet(row2)), make_party(foe_pet(300)));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(audit_int(ctx, kDistractAuditBase + 0), 80, "T3750.f row2 失神申报 = 80");
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(audit_int(ctx, kFearAuditBase + 0), 60, "T3750.f row2 害怕申报 = 60");
        std::cout << "  [T3750.f] row2 审计互锚 通过\n";
    }

    std::cout << "scenario_3750 PASS (K231 登场免麻痹+失神+属技无效/反击窗族)" << std::endl;
    return 0;
}
