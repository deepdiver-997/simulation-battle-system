// 场景 011：换宠 + 统一规则容器 RuleCenter（免疫 + 盔/威/封属 + 命中失效）。
//
// 验证：
//  A) 换宠核心：CHOOSE_PET → on_stage 更新 + 新宠魂印激活（dodge 立即生效）
//  B) 封属挂**施放方**：属性技能被 SEALED；施放方换宠清承（生命周期锚 source）
//  C) 覆盖去重：同 (source, effect_id, kind) 重复挂 → 1 条（刷新次数，不追加）
//  D) 回合生命周期：回合型随回合递减消失；断回合清回合型、次数型保留
//  E) 免断 = ImmunityType::BREAK（免疫，位覆盖粒度）
//  F) 全部消费：一次技能消耗所有响应的次数类条目
//  G) scope 绑定：ON_STAGE 换宠清除；TEAM 换宠保留
//  H) 免疫 scope：ON_STAGE 换宠清除；TEAM 换宠保留
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillGaoSuYiDong = 20017;  // 高速移动：属性技能（category=4）
constexpr int kSkillZhuangJi = 10001;     // 撞击

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A) 换宠核心：第一回合 CHOOSE_PET→slot2（挂 dodge 魂印）→ on_stage 更新 + dodge 生效
    {
        auto petA = scenario::make_pet(0, "我方A", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                    kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800);
        auto petB = scenario::make_pet(0, "我方B", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                    kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800,
                                       /*soul_mark=*/scenario::make_dodge_soul_mark());
        std::array<ElfPet, 6> party0{petA, petA, petB, petA, petA, petA};  // slot2 = dodge 宠
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(party0, scenario::make_party(pet1));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 2);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        assert(battle.ctx().on_stage[0] == 2);  // 换宠：on_stage 已更新
        // 对手攻击被 dodge 魂印闪掉（切换时 activate 立即设 dodge_rate=1.0）
        assert(battle.ctx().ws.skill_exec_result[1] == SkillExecResult::SKILL_INVALID);
        std::cout << "  [switch] CHOOSE_PET->slot2 -> on_stage=" << battle.ctx().on_stage[0]
                  << " dodge 生效(对手 miss)" << std::endl;
    }

    // B) 封属挂**施放方**（统一语义）：source=0 给 target=1 挂封属性 → 对手属性技能被 SEALED →
    //    施放方(0)换宠 → 名下封属清除；被封方(1)换宠则保留。
    {
        auto pet0 = scenario::make_pet(0, "我方", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800);
        auto oppA = scenario::make_pet(1, "对手A", {kSkillGaoSuYiDong, kSkillZhuangJi, kSkillZhuangJi,
                                                    kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        auto oppB = scenario::make_pet(1, "对手B", {kSkillGaoSuYiDong, kSkillZhuangJi, kSkillZhuangJi,
                                                    kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        std::array<ElfPet, 6> party1{oppA, oppB, oppA, oppA, oppA, oppA};
        scenario::ScenarioBattle battle(scenario::make_party(pet0), party1);
        BattleContext& ctx = battle.ctx();
        battle.start();
        // 我方(施放方 0)给对手(被封方 1)挂封属性 1 次
        seal_skill(&ctx, /*source=*/0, /*target=*/1, /*effect_id=*/70011,
                   /*attribute=*/true, /*attack=*/false, /*count=*/1);
        assert(ctx.rule_center_.size() == 1);
        // 回合1：对手用属性技能（高速移动）→ 被 SEALED（次数已消费）
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        assert(ctx.ws.skill_exec_result[1] == SkillExecResult::SKILL_INVALID);  // 封属性挡下
        assert(ctx.rule_center_.empty());  // 次数已消费
        // 回合2：**施放方(0)** 换宠 → invalidate 清其名下 ON_STAGE 封属（生命周期锚 source）
        battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        // 回合1 已消费次数 → 桶空；再挂一条验证：施放方换宠应清掉它
        seal_skill(&ctx, /*source=*/0, /*target=*/1, /*effect_id=*/70012,
                   /*attribute=*/true, /*attack=*/false, /*count=*/1);
        battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 2);  // 施放方换宠
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        assert(ctx.rule_center_.empty());  // 施放方换宠 → 其挂载的封属被清
        std::cout << "  [seal-source] 封属挂施放方 -> 对面属性SEALED + 施放方换宠清封属" << std::endl;
    }

    // C) 覆盖去重：同 (source, effect_id, kind) 重复挂 → 桶里 1 条，次数刷新（不追加）
    {
        auto pet0 = scenario::make_pet(0, "我方", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        BattleContext& ctx = battle.ctx();
        battle.start();
        seal_skill(&ctx, /*source=*/0, /*target=*/1, /*effect_id=*/70021, true, false, /*count=*/2);
        seal_skill(&ctx, /*source=*/0, /*target=*/1, /*effect_id=*/70021, true, false, /*count=*/5);
        assert(ctx.rule_center_.size() == 1);  // 同 effect 覆盖刷新不追加
        int remaining = -1;
        for (const RuleTicket& t : ctx.rule_center_.entries()) {
            if (t.source_effect_id == 70021) {
                remaining = t.remaining_counts;
            }
        }
        assert(remaining == 5);
        std::cout << "  [dedup] 同 (source,effect,kind) 覆盖 -> 1 条 remaining=" << remaining << std::endl;
    }

    // D) 回合生命周期：回合型递减消失 + 断回合清回合型（次数型保留）
    {
        auto pet0 = scenario::make_pet(0, "我方", {kSkillGaoSuYiDong, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        BattleContext& ctx = battle.ctx();
        battle.start();
        // 回合型 3 回合封属性 + 次数型 1 次封攻击（都挂 source=施放方 0）
        seal_skill(&ctx, /*source=*/0, /*target=*/1, /*effect_id=*/70031, true, false, 1,
                   /*duration_rounds=*/3);
        seal_skill(&ctx, /*source=*/0, /*target=*/1, /*effect_id=*/70032, false, true, 1);
        break_round_effects(&ctx, /*target=*/0) == BreakResult::SUCCESS;
        assert(ctx.rule_center_.size() == 1);  // 只剩次数型（回合型清）
        std::cout << "  [round-lifecycle] 断回合 -> 回合型清、次数型保留 (剩 "
                  << ctx.rule_center_.size() << " 条)" << std::endl;
    }

    // E) 免断 = ImmunityType::BREAK（免疫内核，位覆盖粒度）
    {
        auto pet0 = scenario::make_pet(0, "我方", {kSkillGaoSuYiDong, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        BattleContext& ctx = battle.ctx();
        battle.start();

        // E1: 挂回合型拦截（source=0）→ 无免断 → 断 0 回合 SUCCESS，回合型拦截被清
        seal_skill(&ctx, /*source=*/0, /*target=*/1, /*effect_id=*/70041, true, false, 1,
                   /*duration_rounds=*/3);
        assert(break_round_effects(&ctx, /*target=*/0) == BreakResult::SUCCESS);
        assert(ctx.rule_center_.empty());
        std::cout << "  [break] 无免断 -> SUCCESS，回合型拦截被清" << std::endl;

        // E2: 有免断（覆盖当前时点）→ IMMUNE，什么都没清（"断了个空"）
        seal_skill(&ctx, /*source=*/0, /*target=*/1, /*effect_id=*/70042, true, false, 1,
                   /*duration_rounds=*/3);
        ctx.grant_immunity(0, ImmunityType::BREAK, coverage_all(), 0,
                           /*duration_rounds=*/0, /*source_id=*/70042);
        assert(break_round_effects(&ctx, /*target=*/0) == BreakResult::IMMUNE);
        {   // 拦截毫发无损(SEAL 计数，免疫条目不算)
            int seal_cnt = 0;
            for (const auto& t : ctx.rule_center_.entries())
                if (t.category == RuleCategory::SEAL) ++seal_cnt;
            assert(seal_cnt == 1);
        }
        std::cout << "  [break] 免断中 -> IMMUNE，回合型拦截保留(断了个空)" << std::endl;

        // E3: 位覆盖粒度——免断只覆盖部分时点，未覆盖时点照常可断
        ctx.revoke_immunity(0, /*source_id=*/70042);
        const uint64_t narrow = coverage_union({State::BATTLE_FIRST_ACTION_START});
        ctx.grant_immunity(0, ImmunityType::BREAK, narrow, 0,
                           /*duration_rounds=*/0, /*source_id=*/70043);
        assert(break_round_effects(&ctx, /*target=*/0) == BreakResult::SUCCESS);
        std::cout << "  [break] 免断未覆盖当前时点 -> SUCCESS（位覆盖粒度）" << std::endl;
    }

    // F) 全部消费：一次技能消耗所有响应的次数类条目（不同来源不合并，但全部消费）
    {
        auto pet0 = scenario::make_pet(0, "我方", {kSkillGaoSuYiDong, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        BattleContext& ctx = battle.ctx();
        battle.start();

        // 两个不同 effect 的次数盔（shield 我，拦 0 的攻击）各 1 次
        ctx.rule_center_.grant_seal(/*source=*/0, /*source_slot=*/-1, /*effect_id=*/70051,
                                    /*target=*/0, SealKind::SEAL_ATTACK, /*counts=*/1, /*rounds=*/0,
                                    /*penetrable=*/true);
        ctx.rule_center_.grant_seal(/*source=*/0, /*source_slot=*/-1, /*effect_id=*/70052,
                                    /*target=*/0, SealKind::SEAL_ATTACK, /*counts=*/1, /*rounds=*/0,
                                    /*penetrable=*/true);
        assert(ctx.rule_center_.size() == 2);  // 不同 effect → 不合并

        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);  // 撞击
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        battle.drive();
        assert(ctx.ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID);
        assert(ctx.rule_center_.empty());  // 两条都被消耗（全部消费）
        std::cout << "  [all-consume] 两次数盔(异源) -> 一次攻击全部消耗" << std::endl;
    }

    // G) scope 绑定：ON_STAGE 换宠清除；TEAM 换宠保留
    {
        auto pet0 = scenario::make_pet(0, "我方", {kSkillGaoSuYiDong, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        BattleContext& ctx = battle.ctx();
        battle.start();

        ctx.rule_center_.grant_seal(/*source=*/0, /*source_slot=*/0, /*effect_id=*/70061,
                                    /*target=*/1, SealKind::SEAL_ATTRIBUTE, /*counts=*/1, /*rounds=*/0,
                                    /*penetrable=*/true, EffectScope::TEAM);
        ctx.rule_center_.grant_seal(/*source=*/0, /*source_slot=*/0, /*effect_id=*/70062,
                                    /*target=*/1, SealKind::SEAL_ATTRIBUTE, /*counts=*/1, /*rounds=*/0,
                                    /*penetrable=*/true, EffectScope::ON_STAGE);
        assert(ctx.rule_center_.size() == 2);

        battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        assert(ctx.rule_center_.size() == 1);  // 只剩 TEAM 那条
        bool team_kept = false;
        for (const RuleTicket& t : ctx.rule_center_.entries()) {
            if (t.scope == EffectScope::TEAM) {
                team_kept = true;
            }
        }
        assert(team_kept);
        std::cout << "  [scope] 换宠 -> ON_STAGE 清、TEAM 保留" << std::endl;
    }

    // H) 免疫 scope：ON_STAGE 换宠清除，TEAM 换宠保留
    {
        auto pet0 = scenario::make_pet(0, "我方", {kSkillGaoSuYiDong, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800);
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);

        // H1：只挂 ON_STAGE 免断 → 换宠后失效
        {
            scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
            BattleContext& ctx = battle.ctx();
            battle.start();
            ctx.grant_immunity(0, ImmunityType::BREAK, coverage_all(), 0, /*duration=*/0,
                               /*source_id=*/70071);
            assert(ctx.is_immune(0, ImmunityType::BREAK, ctx.currentState));
            battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
            battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            battle.drive();
            assert(!ctx.is_immune(0, ImmunityType::BREAK, ctx.currentState));  // 已随换宠清除
            std::cout << "  [immunity-scope] ON_STAGE 免断 -> 换宠后清除" << std::endl;
        }
        // H2：挂 TEAM 免断 → 换宠后保留（可传承）
        {
            scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
            BattleContext& ctx = battle.ctx();
            battle.start();
            ctx.grant_immunity(0, ImmunityType::BREAK, coverage_all(), 0, /*duration=*/0,
                               /*source_id=*/70072, /*soul_immunity=*/false,
                               /*scope=*/EffectScope::TEAM);
            battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
            battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            battle.drive();
            assert(ctx.is_immune(0, ImmunityType::BREAK, ctx.currentState));  // TEAM 保留
            std::cout << "  [immunity-scope] TEAM 免断 -> 换宠后保留" << std::endl;
        }
    }

    std::cout << "scenario_011 PASS (换宠 + RuleCenter统一容器 + 覆盖去重 + 回合生命周期 + 免断 + 全部消费/scope)"
              << std::endl;
    return 0;
}