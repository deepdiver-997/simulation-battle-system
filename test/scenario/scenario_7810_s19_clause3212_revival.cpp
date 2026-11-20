// 场景 7810 —— S19 方案 B 追认收尾：K193（827）3212 行补子句复活验证
//（2026-11-05，worktree ../agents/s19；号段 7810-7819 → T7810-T7813 四块，单文件）
//
// 背景（docs_local/docs/06-交接/魂印行参读取路径缺陷-2026-10-21.md §四）：
//   827 霜焱 3212 行 args[80,1,1]（row536）带补子句旗 db[2]=1 ——「若对手不处于异常
//   状态，则属性技能命中时消除对手回合类效果」。缺陷期 a.row = DB args[2:]，
//   row_or(2,-1) 对 row536 读到 -1 → 该子句在真实对局**永不触发**（已上线功能缺失）。
//   2026-10-21 方案 B 落地（24ba7f0b，k193_cond_freeze.cpp 迁 db_at 读 owned_int_args
//   原序）→ 旗位改读 **DB args 第 3 位**（db_at(row,2,-1)），row536 口径下 =1 → 子句
//   应复活。本场景 = 追认收尾的复活实证（用户 2026-11-05 拍板走 B）。
//
// 构造口径（新范式，缺陷文档 §五「夜跑已采取的口径」）：SoulMark args 按 **DB args
//   原样**构造（row536=[80,1,1] / row521=[80,1]），**不补 {0,0} 占位**——与真实对局
//   pet_factory 装载口径（owned_int_args = DB args 原样）逐位一致。
//
// 断言口径（复用 3245 的「回合类占位」观测面）：
//   T7810 复活主证：row536 旗在 + 对手无异常 + 属性技命中 → 对手回合类被消除。
//   T7811 对照（异常门）：同上但对手带烧伤 → 门闭，回合类存活。
//   T7812 对照（旗位锚）：row521=[80,1]（DB 第 3 位无旗）→ 子句不设，回合类存活
//     ——证明旗确实读自 DB args[2]，而非 fallback/其它通道（复活依赖 row536 形态）。
//   T7813 对照（技能类型门）：row536 + 攻击技 → 门闭，回合类存活。
//   （概率类无涉：补子句路径零随机掷点，断言全精确；无 while(!FINISHED) 步进。）
#include <array>
#include <iostream>
#include <memory>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <effects/continuousEffect.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;     // 撞击：物理攻击载体
constexpr int kSuoTou = 20001; // 缩头：属性技载体
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);

template <int MarkId>
ElfPet mark_pet(const std::vector<int>& db_args) {
    std::vector<int> args = db_args;   // S19：按 DB args 原样构造（不补 {0,0} 占位）
    ElfPet p = make_pet(1, "印主", {kJi, kSuoTou, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/50, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(MarkId, "K193 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void act(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 024/3245 同款：给对手挂回合类占位（ROUND_START 桶，duration=1）。
void give_foe_round_marker(BattleContext& ctx) {
    Effect round_effect(90002, 0, /*owner=*/1, /*left_round=*/1,
                        EffectArgs(std::vector<int>{0}));
    auto ce = std::make_unique<ContinuousEffect>(
        round_effect, State::BATTLE_ROUND_START, /*owner=*/1, /*duration=*/1,
        ctx.roundCount);
    ctx.registerEffect(State::BATTLE_ROUND_START, 1, std::move(ce),
                       EffectContainer::Skill);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<int> row_521{80, 1};     // 3203 行：无补子句旗
    const std::vector<int> row_536{80, 1, 1};  // 3212 行：db[2]=1 补子句旗

    // ══ T7810 复活主证：row536（DB 原样 [80,1,1]）属性技命中 + 对手无异常
    //    → 对手回合类被消除（缺陷期本构造口径下子句死、占位必存活）══
    {
        ScenarioBattle b(make_party(mark_pet<827>(row_536)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        give_foe_round_marker(ctx);
        REQUIRE(ctx.has_round_effects(1));
        act(b, 1);   // 缩头（属性技，必中）
        CHECK(!ctx.has_round_effects(1),
              "T7810 827 row536：DB args 原样构造 → 属性技命中消除对手回合类（补子句复活）");
        std::cout << "  [T7810] 3212 补子句复活主证 通过\n";
    }

    // ══ T7811 对照（异常门）：对手带烧伤 → 门闭，回合类存活 ══
    {
        ScenarioBattle b(make_party(mark_pet<827>(row_536)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(try_apply_anomaly(&ctx, 1, kBurn, /*duration=*/3, /*actor=*/0));
        give_foe_round_marker(ctx);
        REQUIRE(ctx.has_round_effects(1));
        act(b, 1);
        CHECK(ctx.has_round_effects(1),
              "T7811 827：对手有异常 → 补子句门闭，回合类不被消除");
        std::cout << "  [T7811] 异常门对照 通过\n";
    }

    // ══ T7812 对照（旗位锚）：row521=[80,1]（DB 第 3 位无旗）→ 回合类存活 ══
    {
        ScenarioBattle b(make_party(mark_pet<827>(row_521)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        give_foe_round_marker(ctx);
        REQUIRE(ctx.has_round_effects(1));
        act(b, 1);
        CHECK(ctx.has_round_effects(1),
              "T7812 827 row521：DB args[2] 无旗 → 子句不设（旗确读 DB 第 3 位）");
        std::cout << "  [T7812] 旗位锚对照 通过\n";
    }

    // ══ T7813 对照（技能类型门）：row536 + 攻击技 → 回合类存活 ══
    {
        ScenarioBattle b(make_party(mark_pet<827>(row_536)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        give_foe_round_marker(ctx);
        REQUIRE(ctx.has_round_effects(1));
        act(b, 0);   // 撞击（攻击技）
        CHECK(ctx.has_round_effects(1),
              "T7813 827：攻击技 → 属性技门闭，回合类不被消除");
        std::cout << "  [T7813] 技能类型门对照 通过\n";
    }

    std::cout << "scenario_7810 PASS (S19 3212 补子句复活验证)" << std::endl;
    return 0;
}
