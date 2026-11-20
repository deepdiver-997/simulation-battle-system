// 场景 5490 —— J220 概率随机清 PP 族（1519，moves_lib/j_chance_drop_pp_zero.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5490-5499）
//
// 断言口径（审计槽精确分档 + 逐槽 PP 绝对值；100% 档不耗 rand，槽随机但计数确定）：
//   - 1519 模板：{0}%令对手随机{1}项技能PP值归零，若对手处于能力下降状态则{2}%使对手
//     随机{3}项技能PP值归零。
//   - a) 对照：拍打（无 1519）→ 对手 0 槽归零、审计双键缺席。
//   - b) 34334 无影袭 [100,1,100,2]，门闭（对手无降态）→ 恰 1 槽归零、其余 4 槽保持
//     预设值；审计①=1001（hit*1000+清零数）、审计②=-1（门闭不申报）。
//   - c) 同载体，门开（对手防御 -1）→ 分支①1 槽 + 分支②2 槽 = 恰 3 槽归零、其余 2 槽
//     保持预设；审计①=1001、②=1002。
//   - d) 28447 埃灵游龙 [100,2,100,3]（链 [191,938,1519]，938 全属性-{0} 天然开门）→
//     分支①2 + 分支②3 = 全 5 槽归零；审计①=1002、②=1003（验证行参 2/3 档）。
//   - 逐槽断言：未归零槽必须**等于原预设 PP**（不是"等级和"式的弱断言）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa    = 10004;  // 拍打：物 40（填充/对照刀）
constexpr int kWuYing   = 34334;  // 无影袭 [1350,1519]|[1,100,1,100,2]
constexpr int kAiLing   = 28447;  // 埃灵游龙 [191,938,1519]|[4,1,100,2,100,3]（属性技）
constexpr int kAuditBase = 990151900;  // + owner*2：分支①；+1：分支②
constexpr int kMaxHp    = 5000;

const int kPreset[5] = {7, 9, 11, 13, 15};

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(int skill0) {
    ElfPet p = make_pet(1, "己方", {skill0, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    for (int i = 0; i < 5; ++i) {
        p.skills[static_cast<std::size_t>(i)].pp = kPreset[i];
    }
    return p;
}

int zero_pp_count(BattleContext& ctx) {
    int n = 0;
    for (const Skills& s : ctx.getPet(1).skills) {
        if (s.pp == 0) {
            ++n;
        }
    }
    return n;
}

// 未归零槽数（仍等于预设值）。
int untouched_pp_count(BattleContext& ctx) {
    int n = 0;
    for (int i = 0; i < 5; ++i) {
        if (ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp == kPreset[i]) {
            ++n;
        }
    }
    return n;
}

int audit(BattleContext& ctx, int key) {
    return std::any_cast<int>(ctx.plugin_storage.at(key));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5490.a 对照：无 1519 → 不归零、审计缺席 ══
    {
        ScenarioBattle b(make_party(self_pet(kPaiDa)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(zero_pp_count(ctx), 0, "T5490.a 对照：无 1519 不清 PP");
        CHECK_EQ(untouched_pp_count(ctx), 5, "T5490.a 对照：5 槽全部保持预设");
        CHECK(ctx.plugin_storage.count(kAuditBase) == 0 && ctx.plugin_storage.count(kAuditBase + 1) == 0,
              "T5490.a 对照：1519 审计双键缺席");
        std::cout << "  [T5490.a] 对照组通过\n";
    }

    // ══ T5490.b 34334：门闭 → 分支① 恰 1 槽 ══
    {
        ScenarioBattle b(make_party(self_pet(kWuYing)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(zero_pp_count(ctx), 1, "T5490.b 分支①：恰 1 槽归零（实测 " +
                      std::to_string(zero_pp_count(ctx)) + "）");
        CHECK_EQ(untouched_pp_count(ctx), 4, "T5490.b 分支①：其余 4 槽保持预设值");
        CHECK_EQ(audit(ctx, kAuditBase + 0), 1001, "T5490.b 审计①=1001（hit=1, clear=1）");
        CHECK_EQ(audit(ctx, kAuditBase + 1), -1, "T5490.b 审计②=-1（门闭不申报）");
        std::cout << "  [T5490.b] 门闭单分支通过\n";
    }

    // ══ T5490.c 34334：门开 → 分支①1 + 分支②2 = 3 槽 ══
    {
        ScenarioBattle b(make_party(self_pet(kWuYing)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][2] = -1;   // 对手防御 -1：处于能力下降状态
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(zero_pp_count(ctx), 3, "T5490.c 双分支：恰 3 槽归零（1+2，实测 " +
                      std::to_string(zero_pp_count(ctx)) + "）");
        CHECK_EQ(untouched_pp_count(ctx), 2, "T5490.c 双分支：其余 2 槽保持预设值");
        CHECK_EQ(audit(ctx, kAuditBase + 0), 1001, "T5490.c 审计①=1001");
        CHECK_EQ(audit(ctx, kAuditBase + 1), 1002, "T5490.c 审计②=1002（hit=1, clear=2）");
        std::cout << "  [T5490.c] 门开双分支通过\n";
    }

    // ══ T5490.d 28447：[100,2,100,3]，938 全属性-{0} 开门 → 全 5 槽 ══
    {
        ScenarioBattle b(make_party(self_pet(kAiLing)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(zero_pp_count(ctx), 5, "T5490.d 行参 2/3 档：5 槽全归零（实测 " +
                      std::to_string(zero_pp_count(ctx)) + "）");
        CHECK_EQ(audit(ctx, kAuditBase + 0), 1002, "T5490.d 审计①=1002（clear=2）");
        CHECK_EQ(audit(ctx, kAuditBase + 1), 1003, "T5490.d 审计②=1003（clear=3）");
        std::cout << "  [T5490.d] 2/3 参数档通过\n";
    }

    std::cout << "scenario_5490 PASS (J220 概率随机清PP族 1519)" << std::endl;
    return 0;
}
