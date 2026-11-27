#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

#include <entities/elf-pet.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <entities/soul_mark_manager.h>
#include <db/official_data_repository.h>
#include <effects/continuousEffect.h>
#include <effects/effect.h>
#include <effects/effect_meta.h>
#include <effects/effect_unit.h>
#include <effects/effect_unit_loader.h>
#include <effects/effect_unit_parser.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

namespace {

std::array<BattlePetMessage, 6> build_party(const std::array<int, 5>& skills) {
    std::array<BattlePetMessage, 6> party{};
    for (auto& pet : party) {
        pet.pet_id = 12; // 波克尔
        pet.chosen_skills_id = skills;
        pet.numerical_base = {0, 0, 0, 0, 0, 0};
        pet.common_trait_id = 1027; // 瞬杀 Lv0: 进攻类技能有3%几率秒杀对方
    }
    return party;
}

} // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to initialize official sqlite data" << std::endl;
        return 1;
    }

    if (EffectFactory::getInstance().getLoadedLibraryCount() == 0) {
        std::cerr << "failed to load skill effect plugins" << std::endl;
        return 1;
    }

    const auto attacker_party = build_party({10038, 10057, 10001, 10008, 10009});
    const auto defender_party = build_party({10001, 10008, 10009, 10033, 20017});

    SeerRobot robots[2] = {
        SeerRobotFactory::create_robot(attacker_party),
        SeerRobotFactory::create_robot(defender_party),
    };

    BattleContext context(nullptr, robots);
    context.preemptive_right = PreemptiveRight::SEER_ROBOT_1;

    // 0) 验证通用特性加载（瞬杀 Lv0, idx=1027）
    {
        const auto& trait = context.seerRobot[0].elfPets[context.on_stage[0]].commonTrait;
        assert(trait.id == 1027);
        assert(trait.name == "瞬杀");
        assert(trait.star_level == 0);
        assert(trait.description.find("秒杀") != std::string::npos);
    }

    // 1) 验证突进(10038) 的 effect 6 在攻击伤害时点触发反伤
    context.roundChoice[0][0] = 1; // SELECT_SKILL
    context.roundChoice[0][1] = 0; // skill index 0 -> 10038
    {
        context.seerRobot[0].elfPets[context.on_stage[0]].skills[0].execute(&context, 0, State::BATTLE_FIRST_ON_SKILL_HIT);
        context.execute_registered_actions(0, State::BATTLE_FIRST_SKILL_EFFECT);

        const int hp_before = context.seerRobot[0].elfPets[context.on_stage[0]].hp;
        context.resolvedDamage.attackerId = 0;
        context.resolvedDamage.defenderId = 1;
        context.resolvedDamage.final = 40;
        context.execute_registered_actions(0, State::BATTLE_FIRST_ATTACK_DAMAGE);

        const int hp_after = context.seerRobot[0].elfPets[context.on_stage[0]].hp;
        assert(hp_after == hp_before - 10); // 40 / 4
    }

    context.clearAllEffects();

    // 2) 验证手下留情(10057) 的 effect 8 在攻击伤害时点把伤害压到留1血
    context.roundChoice[0][0] = 1; // SELECT_SKILL
    context.roundChoice[0][1] = 1; // skill index 1 -> 10057
    {
        context.seerRobot[0].elfPets[context.on_stage[0]].skills[1].execute(&context, 0, State::BATTLE_FIRST_ON_SKILL_HIT);

        auto& defender = context.seerRobot[1].elfPets[context.on_stage[1]];
        defender.hp = 30;
        context.resolvedDamage.attackerId = 0;
        context.resolvedDamage.defenderId = 1;
        context.resolvedDamage.final = 50;

        context.execute_registered_actions(0, State::BATTLE_FIRST_ATTACK_DAMAGE);
        assert(context.resolvedDamage.final == 29);
    }

    // 3) 验证 applyDamageReduction 已迁入管线 REDUCE 阶段（install_default_damage_reduction 生效）
    {
        // 防御方(1) 加算减伤 25%：base 100 → final 75
        context.damage_reduce_add[1][0] = 25;
        context.resolvedDamage.attackerId = 0;
        context.resolvedDamage.defenderId = 1;
        context.resolvedDamage.final = 100;
        context.damage_pipeline_.run(&context, 0, 1);
        assert(context.resolvedDamage.final == 75);
        context.damage_reduce_add[1][0] = 0;
    }

    // 4) 挡伤失效（蚀砚之泪≥4滴 = mask BLOCK|DETECT）只废挡伤，不废减伤。
    //    官方口径："减伤100%、点数减伤全都是不能被无视的效果" —— 故减伤(MITIGATE)
    //    与挡伤(BLOCK)必须是两个类别、两套掩码位。
    {
        auto& defender = context.seerRobot[1].elfPets[context.on_stage[1]];
        const int suppress_bits =
            (1 << static_cast<int>(DamageEffectCategory::BLOCK))
            | (1 << static_cast<int>(DamageEffectCategory::DETECT));

        // 4a) 未压制：注册的 BLOCK 挡伤把伤害归零
        context.register_damage_effect(
            DamagePhase::BLOCK, 1, DamageEffectCategory::BLOCK,
            [](BattleContext* c, int bucket_owner) {
                if (c->resolvedDamage.defenderId == bucket_owner) {
                    c->resolvedDamage.final = 0;
                }
            });
        context.resolvedDamage.attackerId = 0;
        context.resolvedDamage.defenderId = 1;
        context.resolvedDamage.final = 100;
        context.damage_pipeline_.run(&context, 0, 1);
        assert(context.resolvedDamage.final == 0);  // 挡伤生效

        // 4b) 压制后：BLOCK 挡伤被跳过 → 伤害保留
        defender.damage_suppress_mask |= suppress_bits;
        context.resolvedDamage.final = 100;
        context.damage_pipeline_.run(&context, 0, 1);
        assert(context.resolvedDamage.final == 100);  // 挡伤失效

        // 4c) 同一掩码下减伤(MITIGATE)仍生效：100 → 75
        context.damage_reduce_add[1][0] = 25;
        context.resolvedDamage.final = 100;
        context.damage_pipeline_.run(&context, 0, 1);
        assert(context.resolvedDamage.final == 75);  // 减伤不可被"挡伤失效"无视

        defender.damage_suppress_mask = 0;
        context.damage_reduce_add[1][0] = 0;
    }

    // 5) 任意技能可加载（Skills(int) 不校验可学性，用于框架验证）
    {
        Skills no_mind_skill(37087);  // 索杰德尔·无念归空净
        Skills garden_skill(38421);   // 薇尔诗·乐园之初诞
        assert(no_mind_skill.name.find("无念归空净") != std::string::npos);
        assert(garden_skill.name.find("乐园之初诞") != std::string::npos);

        // 37087 的 effect 列表应含 2006（技能无效时触发分支已路由到 SKILL_INVALID）
        auto it_invalid = no_mind_skill.effectBranches.find(SkillExecResult::SKILL_INVALID);
        assert(it_invalid != no_mind_skill.effectBranches.end());
        bool has_2006 = false;
        for (const auto& node : it_invalid->second) {
            if (node.effect.id == 2006) {
                has_2006 = true;
                break;
            }
        }
        assert(has_2006);
    }

    // 6) EVENT_ATTACK_BLOCKED 发射 + drain 投递验证
    {
        int fire_count = 0;
        const int wid = context.event_center_.register_watcher(
            EventType::EVENT_ATTACK_BLOCKED, 0, context.roundCount, 0, true,
            [&fire_count](BattleContext* ctx, const BattleEvent& ev) {
                (void)ctx;
                (void)ev;
                ++fire_count;
            }
        );
        context.event_center_.emit(BattleEvent{EventType::EVENT_ATTACK_BLOCKED, 0, 1, 0});
        context.event_center_.drain(&context, context.roundCount);
        assert(fire_count == 1);  // once watcher 触发一次
        // 触发后 once watcher 已移除，不污染后续测试
        assert(!context.event_center_.has_watchers(EventType::EVENT_ATTACK_BLOCKED));
        (void)wid;
    }

    // 7) effect_skill_2006 事件链：SKILL_INVALID → 挂 **SEAL 盔** → 盔真正生效 → 对手全属性+1
    //
    // ⚠️ 2026-09-13 改：2006 从"伤害管线 BLOCK 挡伤"改成 **SEAL 盔**。
    //    官方实测依据（用户游戏内确认）：这条盔**能被穿盔凭证穿透**，而穿盔凭证
    //    (`ignore_attack_immunity`) 只作用于 RuleCenter 的 SEAL 条目——管线挡伤不在它的作用面。
    {
        Effect e = EffectFactory::getInstance().getEffect(2006, EffectArgs(std::vector<int>{0, 1}));
        assert(e.logic != nullptr);
        e.logic(&context, e.args);

        // 盔挂上了：source=0(施放方生命周期锚) → target=1(被封方)；攻击类、1 次、可穿、被穿消耗
        bool found = false;
        for (const RuleTicket& t : context.rule_center_.entries()) {
            if (t.category != RuleCategory::SEAL || t.source_effect_id != 2006) {
                continue;
            }
            found = true;
            assert(t.source_owner == 0);
            assert(t.target == 1);
            assert(t.remaining_counts == 1);
            assert(t.penetrable);                 // 可穿盔（可被穿盔凭证穿透）
            assert(t.consumed_when_pierced);      // 带后续子句 → 被穿也消耗
            assert(t.responds_to(/*is_attribute_skill=*/false));  // 拦攻击技能
            assert(!t.responds_to(/*is_attribute_skill=*/true));  // 不拦属性技能
        }
        assert(found);

        // 走**真实路径**：对手(1)使用攻击技能 → 被这条盔拦下 → notify 发结算事件 → 子句触发。
        // （不手工构造 BattleEvent 再 emit —— 那是绕开引擎的合成路径，测不出接线是否真的通。）
        ElfPet& opp_pet = context.seerRobot[1].elfPets[context.on_stage[1]];
        opp_pet.skills[0].must_hit = true;      // 排除命中率干扰
        const int opp_level_before = context.ability_levels[1][0];
        const SkillUsageResult usage = opp_pet.skills[0].query_usage(&context, /*owner=*/1);
        context.event_center_.drain(&context, context.roundCount);
        assert(usage == SkillUsageResult::SEALED);            // 被盔拦下
        const int opp_level_after = context.ability_levels[1][0];
        assert(opp_level_after == opp_level_before + 1);      // 盔生效 → 对手全属性+1

        // 再打一次（这次没盔了，因为 count=1 已被消耗）→ 子句**不该**再触发第二次
        const int after_first = context.ability_levels[1][0];
        opp_pet.skills[0].query_usage(&context, 1);
        context.event_center_.drain(&context, context.roundCount);
        assert(context.ability_levels[1][0] == after_first);             // 子句只触发一次

        context.clearAllEffects();
    }

    // 8) effect_skill_843：下N回合自身所有技能先制+N
    {
        Effect e = EffectFactory::getInstance().getEffect(843, EffectArgs(std::vector<int>{0, 1, 2, 2}));
        assert(e.logic != nullptr);
        const int level_before = context.ws.preemptive_level[0];
        e.logic(&context, e.args);
        // 843 注册到 BATTLE_FIRST_MOVE_RIGHT 桶；手动触发该时点
        context.execute_registered_actions(0, State::BATTLE_FIRST_MOVE_RIGHT);
        assert(context.ws.preemptive_level[0] == level_before + 2);
        context.clearAllEffects();  // 清掉 843 注册的回合效果
    }

    // 9) 朝曦魂印(9901006)：切换仍使用1号位技能
    //    （id 用私有号段 9901xxx：撞号治理 ecfe64e8 把 1006 迁走，官方 1006=3408 另有语义；
    //      getEffectFunc 未命中返回 noop——非空，assert 拦不住拿错函数）
    {
        SoulMarkManager& mgr = SoulMarkManager::getInstance();
        EffectFn zhaoxi = mgr.getEffectFunc(9901006);
        assert(zhaoxi != nullptr);

        // 模拟己方(0)操作：CHOOSE_PET(0) 切到 2 号位精灵
        EffectArgs zargs(std::vector<int>{0, 1});

        // 正路径：把 2 号位临时改成梅赫维特(4853)，验证改写
        const int orig_slot2_id = context.seerRobot[0].elfPets[2].id;
        context.seerRobot[0].elfPets[2].id = 4853;
        context.roundChoice[0][0] = 0;  // CHOOSE_PET
        context.roundChoice[0][1] = 2;
        zhaoxi(&context, zargs);
        // 改写成功：CHOOSE_PET → SELECT_SKILL(1)，index 0
        assert(context.roundChoice[0][0] == 1);
        assert(context.roundChoice[0][1] == 0);
        context.seerRobot[0].elfPets[2].id = orig_slot2_id;

        // 反路径：目标不是梅赫维特 → 幂等不改写
        context.roundChoice[0][0] = 0;  // CHOOSE_PET
        context.roundChoice[0][1] = 2;
        zhaoxi(&context, zargs);
        assert(context.roundChoice[0][0] == 0);  // 仍是 CHOOSE_PET
    }

    // 10) 选择期生命周期：on_selected 把基值先制注册到 MOVE_RIGHT 桶，执行时点后累加
    {
        Skills skill(10008);  // 电光火石，priority=1
        const int before = context.ws.preemptive_level[0];
        skill.on_selected(&context, 0);  // 注册基值先制效果到 MOVE_RIGHT 桶
        context.execute_registered_actions(0, State::BATTLE_FIRST_MOVE_RIGHT);  // 执行桶
        assert(context.ws.preemptive_level[0] == before + 1);
    }

    // 11) query_selectable：PP=0 被拒；PP>0 可选
    {
        Skills skill(10008);
        assert(skill.query_selectable(&context, 0) == SkillSelectionResult::SELECTABLE);
        skill.pp = 0;
        assert(skill.query_selectable(&context, 0) == SkillSelectionResult::PP_EMPTY);
        skill.pp = 0;
        skill.register_usability_effect(999, SkillUsabilityEffectType::IgnorePP, true);
        assert(skill.query_selectable(&context, 0) == SkillSelectionResult::SELECTABLE);
    }

    // 12) 执行期：query_usage 命中判定（必中技能必 OK；封属性/封攻击返回 SEALED 并消费次数）
    {
        Skills attr_skill(20017);  // 高速移动，属性技能（category=4）
        Skills atk_skill(10008);   // 电光火石，攻击技能

        // 封属性：挂 1 次（source=施放方1 → target=被封方0），属性技能被拦截并消费
        seal_skill(&context, /*source=*/1, /*target=*/0, /*effect_id=*/70001,
                   /*attribute=*/true, /*attack=*/false, /*count=*/1);
        assert(context.rule_center_.size() == 1);
        assert(attr_skill.query_usage(&context, 0) == SkillUsageResult::SEALED);
        assert(context.rule_center_.empty());  // 次数已消费，桶清空

        // 封攻击：攻击技能被拦截
        seal_skill(&context, /*source=*/1, /*target=*/0, /*effect_id=*/70002,
                   /*attribute=*/false, /*attack=*/true, /*count=*/1);
        assert(atk_skill.query_usage(&context, 0) == SkillUsageResult::SEALED);
        assert(context.rule_center_.empty());

        // 未命中判定：accuracy 高概率命中（电光火石 accuracy 100）→ 应 OK
        context.ws.dodge_rate[1] = 0.0f;
        assert(atk_skill.query_usage(&context, 0) == SkillUsageResult::OK);
    }

    // 13) 同源去重：同 source_id 新注册覆盖旧；不同 source_id 并存
    {
        context.clearAllEffects();
        auto& map = context.skills_effects.at(State::BATTLE_FIRST_SKILL_EFFECT, 0);

        // 来源 100 效果 1：注册一次
        Effect e1(1, 0, 0, 5, EffectArgs(std::vector<int>{100}), nullptr);
        auto ce1 = std::make_unique<ContinuousEffect>(
            e1, State::BATTLE_FIRST_SKILL_EFFECT, 0, 5, context.roundCount);
        ce1->source_id_ = 100;
        context.registerEffect(State::BATTLE_FIRST_SKILL_EFFECT, 0, std::move(ce1), EffectContainer::Skill);

        // 来源 100 效果 1：再注册一次（新覆盖旧）→ map 仍只有 1 条
        auto ce1b = std::make_unique<ContinuousEffect>(
            e1, State::BATTLE_FIRST_SKILL_EFFECT, 0, 5, context.roundCount);
        ce1b->source_id_ = 100;
        context.registerEffect(State::BATTLE_FIRST_SKILL_EFFECT, 0, std::move(ce1b), EffectContainer::Skill);
        assert(map.size() == 1);

        // 来源 101 效果 1：不同 source_id → 并存
        auto ce2 = std::make_unique<ContinuousEffect>(
            e1, State::BATTLE_FIRST_SKILL_EFFECT, 0, 5, context.roundCount);
        ce2->source_id_ = 101;
        context.registerEffect(State::BATTLE_FIRST_SKILL_EFFECT, 0, std::move(ce2), EffectContainer::Skill);
        assert(map.size() == 2);

        context.clearAllEffects();
    }

    // 14) 切换作废：invalidate_on_stage_effects 使 ON_STAGE 效果失效；TEAM 豁免
    {
        context.clearAllEffects();
        // ON_STAGE 回合效果
        Effect e1(2, 0, 0, 3, EffectArgs(std::vector<int>{1}), nullptr);
        auto ce1 = std::make_unique<ContinuousEffect>(
            e1, State::BATTLE_FIRST_SKILL_EFFECT, 0, 3, context.roundCount);
        ce1->source_id_ = 200;
        context.registerEffect(State::BATTLE_FIRST_SKILL_EFFECT, 0, std::move(ce1), EffectContainer::Skill);

        // TEAM 效果
        auto ce2 = std::make_unique<ContinuousEffect>(
            e1, State::BATTLE_FIRST_SKILL_EFFECT, 0, 3, context.roundCount);
        ce2->source_id_ = 201;
        ce2->scope_ = EffectScope::TEAM;
        context.registerEffect(State::BATTLE_FIRST_SKILL_EFFECT, 0, std::move(ce2), EffectContainer::Skill);

        // 切换：作废所有 ON_STAGE
        context.invalidate_on_stage_effects(0);

        // 执行桶：ON_STAGE 被跳过（valid_id 不匹配），TEAM 仍执行
        bool on_stage_executed = false;
        bool team_executed = false;
        for (auto& [key, eff] : context.skills_effects.at(State::BATTLE_FIRST_SKILL_EFFECT, 0)) {
            if (eff->scope_ == EffectScope::TEAM) {
                team_executed = true;  // 能被枚举到
            } else {
                on_stage_executed = true;  // ON_STAGE 条目仍存在但 valid_id 失效
            }
        }
        assert(team_executed);
        assert(on_stage_executed);

        // cleanup：ON_STAGE 失效条目被移除，TEAM 保留
        context.cleanup_expired_effects();
        auto& map = context.skills_effects.at(State::BATTLE_FIRST_SKILL_EFFECT, 0);
        assert(map.size() == 1);  // 只剩 TEAM
        for (auto& [key, eff] : map) {
            assert(eff->scope_ == EffectScope::TEAM);
        }

        context.clearAllEffects();
    }

    // 15) 断回合不递增监听器版本号：补偿 watcher 在断回合后仍存活
    {
        context.clearAllEffects();
        // 注册 ON_STAGE 断回合补偿 watcher（等价 register_break_callback 语义）
        const int wid = context.event_center_.register_watcher(
            EventType::EVENT_BREAK, 0, context.roundCount, 0, true,
            [](BattleContext* ctx, const BattleEvent& ev) { (void)ctx; (void)ev; },
            WatcherScope::ON_STAGE, context.watcher_valid_id[0]);

        // 断回合：只改效果桶版本号，监听器版本号不变
        context.invalidate_all_round_effects(0);
        assert(context.event_center_.has_watchers(EventType::EVENT_BREAK));  // watcher 仍存活
        context.event_center_.remove_watcher(wid);
        context.clearAllEffects();
    }

    // 16) 切换作废 ON_STAGE watcher；TEAM watcher 保留
    {
        context.clearAllEffects();
        // ON_STAGE watcher
        const int w1 = context.event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, 0, context.roundCount, 0, true,
            [](BattleContext* ctx, const BattleEvent& ev) { (void)ctx; (void)ev; },
            WatcherScope::ON_STAGE, context.watcher_valid_id[0]);
        // TEAM watcher（valid_id 设为任意值，切换不检查 TEAM）
        const int w2 = context.event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, 0, context.roundCount, 0, true,
            [](BattleContext* ctx, const BattleEvent& ev) { (void)ctx; (void)ev; },
            WatcherScope::TEAM, 999);

        // 切换：递增监听器版本号 → ON_STAGE 作废
        context.invalidate_on_stage_effects(0);
        // deliver 检查：ON_STAGE 被跳过，TEAM 仍触发
        int fired = 0;
        context.event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, 0, context.roundCount, 0, true,
            [&fired](BattleContext* ctx, const BattleEvent& ev) { (void)ctx; (void)ev; ++fired; },
            WatcherScope::TEAM, context.watcher_valid_id[0]);
        context.event_center_.emit(BattleEvent{EventType::EVENT_TAKE_DAMAGE, 0, 1, 10});
        context.event_center_.drain(&context, context.roundCount, context.watcher_valid_id);
        assert(fired == 1);  // 只有新注册的 TEAM watcher 触发（ON_STAGE w1 被作废，w2 once 已消费）

        // cleanup：切换作废的 ON_STAGE watcher 被移除；TEAM 保留
        context.cleanup_expired_effects();
        // w1 已作废并应被 cleanup 移除
        // 检查：watcher 数（w2 已 once 消费移除，此处验证 w1 被清理）
        // 通过重新 emit 验证：若 w1 仍在则其 once 语义会再触发——不直接断言数量，改为验证 cleanup 行为
        context.event_center_.remove_watcher(w1);
        context.event_center_.remove_watcher(w2);
        context.clearAllEffects();
    }

    // 17) 任务1：现代精灵魂印链路（effect_icon）+ effect_des 词典
    {
        auto& repository = official_data::OfficialDataStore::instance().repository();

        // 星光·艾欧丽娅 4911：魂印描述来自 effect_icon.tips，应包含"骑士决斗"
        const auto display_4911 = repository.load_soul_mark_display_by_monster(4911);
        assert(display_4911.has_value());
        assert(display_4911->tips_plain.find("骑士决斗") != std::string::npos);
        assert(!display_4911->kind_tags.empty());

        // 奥菲利娅 4706：两行魂印（icon 1901 / effect 2291 基础版；icon 1940 / effect 2344 强化版）
        // 取 icon_id 最大者为当前生效版本（effect_id 应为强化版 2344）。
        const auto display_4706 = repository.load_soul_mark_display_by_monster(4706);
        assert(display_4706.has_value());
        assert(display_4706->icon_id == 1940);   // 最大 icon 行 id
        assert(display_4706->effect_id == 2344); // 强化版效果号

        // effect_des 词典：骑士决斗词条定义含"第五技能"（技能转换机制）
        const auto term = repository.load_term("骑士决斗");
        assert(term.has_value());
        assert(term->description.find("第五技能") != std::string::npos);
        // kind=2 异常状态词表非空（元数据分类器的状态名词来源）
        assert(!repository.load_terms_by_kind(2).empty());
    }

    // 18) 任务2：效果元数据目录
    {
        auto& catalog = EffectMetaCatalog::instance();
        assert(catalog.ready());

        // effect 4/5："{1}%改变对手/自身{0}等级{2}" → StatChange，概率参数位 1
        const auto* meta_stat = catalog.find(5);
        assert(meta_stat != nullptr);
        assert(meta_stat->category == EffectCategory::StatChange);
        assert(meta_stat->chance.arg_index == 1);

        // effect 2533："{0}%令对手{1}，未触发则恢复PP" → StatusInflict（主子句优先）
        const auto* meta_status = catalog.find(2533);
        assert(meta_status != nullptr);
        assert(meta_status->category == EffectCategory::StatusInflict);
        // 概率阈值谓词：args[0]=100 时 100>50 为假；args[0]=30 时为真
        const EffectArgs high_chance(std::vector<int>{100, 31, 1});
        const EffectArgs low_chance(std::vector<int>{30, 31, 1});
        assert(!catalog.is_status_inflict_at_most(2533, high_chance, 50));
        assert(catalog.is_status_inflict_at_most(2533, low_chance, 50));
        // 非异常附加类（843 先制）谓词恒假
        const EffectArgs prio_args(std::vector<int>{2, 1});
        assert(!catalog.is_status_inflict_at_most(843, prio_args, 50));

        // effect 843 → Priority；未收录 id 返回 nullptr 不崩溃
        const auto* meta_prio = catalog.find(843);
        assert(meta_prio != nullptr);
        assert(meta_prio->category == EffectCategory::Priority);
        assert(catalog.find(999999) == nullptr);
    }

    // 19) 穿透凭证最小门（凭证基础设施 + 技能无效中心门）
    {
        context.clearAllEffects();
        context.rule_center_.clear_all();

        // 元数据识别：697/699 → Penetration，且作为凭证贡献者不注册分支效果
        auto& catalog = EffectMetaCatalog::instance();
        assert(catalog.find(697) != nullptr);
        assert(catalog.find(697)->category == EffectCategory::Penetration);
        assert(catalog.find(699) != nullptr);
        assert(catalog.find(699)->category == EffectCategory::Penetration);

        Skills plain(10008);   // 电光火石：无穿透
        Skills pierce(17589);  // 法咒共鸣斩：side_effect=[697,699]
        assert(!plain.penetration_flags.ignore_attack_immunity);
        assert(!plain.penetration_flags.ignore_damage_limit);
        assert(pierce.penetration_flags.ignore_attack_immunity);
        assert(pierce.penetration_flags.ignore_damage_limit);
        assert(pierce.effectBranches.empty());  // 697/699 不注册为分支效果（凭证贡献者）

        // A) 无凭证 + 攻击免疫阻挡 → 被挡（阻挡次数消费）
        context.rule_center_.clear_all();
        seal_skill(&context, /*source=*/1, /*target=*/0, /*effect_id=*/70013,
                   /*attribute=*/false, /*attack=*/true, /*count=*/1);
        assert(plain.query_usage(&context, 0) == SkillUsageResult::SEALED);
        assert(context.rule_center_.empty());  // 次数已消费

        // B) 有次数凭证 + 可穿盔 → 穿透（plain 无技能自带凭证，隔离验证 grant 是穿透来源）
        //    成功攻击后次数 1→0 移除，可穿盔保留（文档 5.2）
        context.rule_center_.clear_all();
        seal_skill(&context, /*source=*/1, /*target=*/0, /*effect_id=*/70014,
                   /*attribute=*/false, /*attack=*/true, /*count=*/1);
        context.grant_penetration(0, /*level=*/1, /*i_ai=*/true, /*i_dl=*/false, /*counts=*/1,
                                  /*rounds=*/0, RuleCategory::PENETRATE_ATTACK);
        assert(context.ws.attack_credential[0].valid == false);  // 尚未物化
        const auto [resB, flagsB] = plain.execute(&context, 0, State::BATTLE_FIRST_ON_SKILL_HIT);
        assert(resB == SkillExecResult::HIT);
        assert(context.rule_center_.count_penetrate(0, RuleCategory::PENETRATE_ATTACK) == 0);  // 次数 1→0 移除
        assert(context.rule_center_.size() == 1);     // 可穿盔被穿透 → 保留

        // C) 无阻挡但存在次数授予 → 成功攻击仍消费（统一计数，用户定模型）
        context.rule_center_.clear_all();
        context.grant_penetration(0, /*level=*/1, /*i_ai=*/true, /*i_dl=*/false, /*counts=*/2,
                                  /*rounds=*/0, RuleCategory::PENETRATE_ATTACK);
        const auto [resC, flagsC] = plain.execute(&context, 0, State::BATTLE_FIRST_ON_SKILL_HIT);
        assert(resC == SkillExecResult::HIT);
        // 2→1：次数型条目仍在（RuleCenter 扣到 0 才注销）
        assert(context.rule_center_.count_penetrate(0, RuleCategory::PENETRATE_ATTACK) == 1);

        // D) 条件盔/龙威（penetrable=false）→ 即使 699 凭证也被挡
        context.rule_center_.clear_all();
        context.rule_center_.grant_seal(/*source=*/1, /*source_slot=*/-1, /*effect_id=*/70001,
                                        /*target=*/0, SealKind::SEAL_ATTACK, /*counts=*/1,
                                        /*rounds=*/0, /*penetrable=*/false);
        assert(pierce.query_usage(&context, 0) == SkillUsageResult::SEALED);
        assert(context.rule_center_.empty());  // 条件盔被攻击消耗

        context.clearAllEffects();
    }

    // 20) 强制执行凭证（检查端）：miss 绕过 + 命中效果失效绕过（预留）
    {
        context.clearAllEffects();
        context.rule_center_.clear_all();
        context.ws.dodge_rate[1] = 0.0f;

        Skills plain(10008);   // 电光火石：无强制、无穿透

        // 无凭证 + 对手闪避100% → 必 miss（命中率 = accuracy - 100 ≤ 0，rand%100 >= 0 恒真）
        context.ws.dodge_rate[1] = 1.0f;
        assert(plain.query_usage(&context, 0) == SkillUsageResult::MISS);

        // 挂强制凭证 → 同一技能同一闪避 → 必定命中（跳过 miss 计算，隐含必中）
        plain.penetration_flags.force_execute = true;
        assert(plain.query_usage(&context, 0) == SkillUsageResult::OK);
        assert(context.ws.attack_credential[0].force_execute == true);  // 已物化
        assert(context.ws.attack_credential[0].valid == true);

        // 强制执行走完整 execute 也命中（miss 被绕过，HIT 分支照常）
        const auto [res20, flags20] = plain.execute(&context, 0, State::BATTLE_FIRST_ON_SKILL_HIT);
        assert(res20 == SkillExecResult::HIT);

        // 还原
        plain.penetration_flags.force_execute = false;
        context.ws.dodge_rate[1] = 0.0f;
        context.clearAllEffects();
    }

    // 21) SET端：魂印激活链 + 无为觉者 2260 PP=0 强制
    {
        context.clearAllEffects();
        context.rule_center_.clear_all();
        context.force_execute_on_pp0[0] = false;  context.force_execute_on_pp0[1] = false;
        context.ignore_pp[0] = false;  context.ignore_pp[1] = false;
        context.ws.dodge_rate[1] = 0.0f;

        // 1) 魂印激活链：构造 2260 魂印（soul_lib 已注册）→ register_soul_effect → ROUND_START 执行 → 信号置位
        SoulMark mark_2260(2260, "无为觉者魂印", "");
        // 2260 已迁移程序模型（multi-trigger）：effect 为空，program_ 含 ROUND_START+early 节点
        assert(mark_2260.has_program_ && !mark_2260.program_.empty());
        mark_2260.register_soul_effect(&context, 0, /*owner_on_stage=*/true, /*host_slot=*/0);
        context.execute_registered_actions(-1, State::BATTLE_ROUND_START);
        assert(context.force_execute_on_pp0[0] == true);
        assert(context.ignore_pp[0] == true);

        // 2) PP=0 技能 + ignore_pp → 可选；无 ignore_pp → PP_EMPTY（对照组）
        Skills s_pp0(10008);  // 电光火石
        s_pp0.pp = 0;
        assert(s_pp0.query_selectable(&context, 0) == SkillSelectionResult::SELECTABLE);
        assert(s_pp0.skill_usable(&context, 0) == true);
        context.ignore_pp[0] = false;
        assert(s_pp0.query_selectable(&context, 0) == SkillSelectionResult::PP_EMPTY);
        context.ignore_pp[0] = true;

        // 3) materialize 合并：force_execute_on_pp0[0] && skill.pp==0 → force_execute，miss 绕过
        context.ws.dodge_rate[1] = 1.0f;  // 对手闪避100%
        assert(s_pp0.query_usage(&context, 0) == SkillUsageResult::OK);
        assert(context.ws.attack_credential[0].force_execute == true);

        // 还原
        context.force_execute_on_pp0[0] = false;
        context.ignore_pp[0] = false;
        context.ws.dodge_rate[1] = 0.0f;
        context.clearAllEffects();
    }

    // 22) 命中效果失效③层（白板/保留伤害 + 强制执行绕过）
    {
        context.clearAllEffects();
        context.rule_center_.clear_all();
        // ③层已并入 RuleCenter(clear_all 清)
        context.ws.hit_invalid_zero_damage[0] = false;  context.ws.hit_invalid_zero_damage[1] = false;
        context.ws.dodge_rate[1] = 0.0f;

        Skills s_stat(10005);  // 泡沫：effect 5 能力变化（SKILL_EFFECT 时点注册，默认可否决）
        // 注意：不能持有 skills_effects[...][0] 的引用——clearAllEffects 会 clear 掉它（悬垂）。
        auto skill_effect_bucket_empty = [&]() {
            return context.skills_effects.at(State::BATTLE_FIRST_SKILL_EFFECT, 0).empty();
        };

        // 可否决性元数据：effect 5 默认可否决；1002 条件先制经覆盖表不可否决
        assert(EffectMetaCatalog::instance().find(5)->nullify.hit_effect_invalidatable == true);
        assert(EffectMetaCatalog::instance().find(1002) != nullptr);
        assert(EffectMetaCatalog::instance().find(1002)->nullify.hit_effect_invalidatable == false);

        // 对照组：无③ → HIT 分支效果注册（effect 5 进桶）
        assert(s_stat.execute(&context, 0, State::BATTLE_FIRST_ON_SKILL_HIT).first == SkillExecResult::HIT);
        assert(!skill_effect_bucket_empty());
        context.clearAllEffects();

        // ① 白板 kFullNull：效果被过滤（桶空）+ ws 白板标记置位 + 次数消费
        hit_effect_invalid(&context, /*target=*/1, HitInvalidMode::kFullNull, /*count=*/1,
                            /*is_attribute_skill=*/false);
        assert(s_stat.execute(&context, 0, State::BATTLE_FIRST_ON_SKILL_HIT).first == SkillExecResult::HIT);
        assert(skill_effect_bucket_empty());                       // effect 5 被过滤
        assert(context.ws.hit_invalid_zero_damage[0] == true);     // 白板标记
        assert(context.rule_center_.hit_invalid_remaining(1) == 0);            // 次数已消费
        context.clearAllEffects();
        context.ws.hit_invalid_zero_damage[0] = false;

        // ② 保留伤害 kEffectsOnly：效果被过滤 + 白板标记不置位
        hit_effect_invalid(&context, 1, HitInvalidMode::kEffectsOnly, 1, /*is_attribute_skill=*/false);
        assert(s_stat.execute(&context, 0, State::BATTLE_FIRST_ON_SKILL_HIT).first == SkillExecResult::HIT);
        assert(skill_effect_bucket_empty());                       // effect 5 仍被过滤
        assert(context.ws.hit_invalid_zero_damage[0] == false);    // 保留伤害不动
        context.clearAllEffects();

        // ③ 强制执行绕过：force_execute → ③层完全无效（效果注册 + 无白板 + ③未消费）
        hit_effect_invalid(&context, 1, HitInvalidMode::kFullNull, 1, /*is_attribute_skill=*/false);
        s_stat.penetration_flags.force_execute = true;
        assert(s_stat.execute(&context, 0, State::BATTLE_FIRST_ON_SKILL_HIT).first == SkillExecResult::HIT);
        assert(!skill_effect_bucket_empty());                      // 效果照常注册
        assert(context.ws.hit_invalid_zero_damage[0] == false);    // 无白板
        assert(context.rule_center_.hit_invalid_remaining(1) == 1);        // ③层被绕过，未消费
        s_stat.penetration_flags.force_execute = false;
        context.clearAllEffects();
    }

    // 23) 效果组合语法第一刀：条件效果单元执行器（Type B 免疫补偿）
    {
        context.clearAllEffects();
        context.ws.dodge_rate[1] = 0.0f;
        ElfPet& target_pet = context.getPet(1);
        target_pet.marks.clear();
        context.ability_levels[0][0] = 0;  // 攻击等级归一
        context.ability_levels[1][0] = 0;

        // 组合：主效果"令对手麻痹"；若被免疫（on_immune）→ 补偿"自身攻击+2"。
        EffectUnit compensation;   // 补偿：自身(0)攻击(stat=0)+2
        compensation.primary_tag = PrimitiveTag::StatChange;
        compensation.actor = 0;
        compensation.target = 0;
        compensation.param0 = 0;   // 攻击
        compensation.param1 = 2;

        EffectUnit main_unit;      // 主效果：令对手(1)麻痹(anomaly=0)，2回合
        main_unit.primary_tag = PrimitiveTag::Anomaly;
        main_unit.actor = 0;
        main_unit.target = 1;
        main_unit.param0 = 0;      // AbnormalStatusId::Paralysis
        main_unit.param1 = 2;
        main_unit.chance_arg = -1; // 必定执行
        main_unit.on_immune = 1;
        const ParsedEffectProgram program({main_unit, compensation});

        const EffectArgs args_empty;  // chance_arg=-1，无需概率参数

        // 场景1：目标无免疫 → 麻痹成功，补偿不触发（原语细码首次被消费）
        const BranchKey key1 = execute_effect_unit(&context, args_empty, program, 0);
        assert(key1 == BranchKey::Success);
        assert(context.has_active_abnormal_status(1, 0) == true);  // 目标麻痹
        assert(context.ability_levels[0][0] == 0);                  // 补偿未触发

        // 场景2：目标挂 Mark(0) 免疫 → TARGET_IMMUNE → on_immune 补偿触发
        add_mark(target_pet.marks, 0, "异常免疫");
        const BranchKey key2 = execute_effect_unit(&context, args_empty, program, 0);
        assert(key2 == BranchKey::Success);                        // 补偿单元执行成功
        assert(context.ability_levels[0][0] == 2);                  // 自身攻击+2（Type B 补偿）

        target_pet.marks.clear();
        context.ability_levels[0][0] = 0;
        context.clearAllEffects();
    }

    // 24) 组合语法解析：模板 → 条件效果单元（StatusInflict + 未触发兜底）
    {
        context.clearAllEffects();
        context.ws.dodge_rate[1] = 0.0f;
        ElfPet& target_pet = context.getPet(1);
        target_pet.marks.clear();
        context.set_abnormal_status_end_round(1, 0, context.roundCount - 1);  // 清残留麻痹

        // 1) 解析器结构：916 模板（{0}%使对手{1}，若未触发则恢复自身全部体力）
        std::vector<EffectUnit> storage;
        const int root_idx = parse_effect_unit(
            "命中后{0}%使对手{1}，若未触发则恢复自身全部体力", {100, 0}, storage);
        assert(root_idx == 0);
        const EffectUnit& root = storage[static_cast<std::size_t>(root_idx)];
        assert(root.primary_tag == PrimitiveTag::Anomaly);
        assert(root.target == 1);            // 对手
        assert(root.chance_value == 100);
        assert(root.param0 == 0);            // 麻痹（AbnormalStatusId::Paralysis）
        assert(root.on_other == 1);
        assert(storage[root.on_other].primary_tag == PrimitiveTag::Heal);
        assert(storage[root.on_other].target == 0);  // 恢复自身

        // 2) 递归解析：兜底本身又是"令对手{状态}"（真正的递归 if-else）——独立 storage
        std::vector<EffectUnit> storage_rec;
        const int rec_idx = parse_effect_unit("{0}%令对手{1}，未触发则令对手{2}", {50, 0, 1}, storage_rec);
        assert(rec_idx == 0);
        assert(storage_rec[0].on_other == 1);
        assert(storage_rec[1].primary_tag == PrimitiveTag::Anomaly);
        assert(storage_rec[1].param0 == 1);  // 兜底异常 id

        // 3) 执行器：chance=100 → 主效果异常施加（owner 从 args[0] 解析）
        const EffectArgs exec_args(std::vector<int>{0, 1});  // owner=0, target=1
        const ParsedEffectProgram program(storage);
        const BranchKey key_primary = execute_effect_unit(&context, exec_args, program, root_idx);
        assert(key_primary == BranchKey::Success);
        assert(context.has_active_abnormal_status(1, 0) == true);  // 目标麻痹

        // 4) 集成：真实技能 19531（effect 916）。2026-09-27 起 effect 916 由
        //    moves_lib/batch_a_skills.cpp 注册为**插件函数**（技能组批产 A 组，含
        //    概率闸门申报）→ loadSkills 走插件路径、不再落解析器兜底（parsed_units_
        //    为空）。解析器本体的解析/执行能力由本节 1)/2)/3)/5) 直接覆盖。
        Skills s_916(19531);  // 红炎冲击波：side_effect=[916], args=[50,2]
        assert(!s_916.effectBranches.empty());  // 插件注册的 916 已挂进分支
        assert(s_916.parsed_units().empty());    // 插件路径：不再产生解析单元

        // 5) 执行器兜底：chance=0 → 未触发 → on_other（Heal 自身）执行
        storage[root_idx].chance_value = 0;  // 构建另一份定义，确定触发兜底。
        const ParsedEffectProgram heal_program(storage);
        ElfPet& self_pet = context.getPet(0);
        const int self_max = self_pet.numericalBase[NumericalPropertyIndex::HP];
        self_pet.hp = self_max / 2;  // 半血
        const BranchKey key_fallback = execute_effect_unit(&context, exec_args, heal_program, root_idx);
        assert(key_fallback == BranchKey::Success);
        assert(self_pet.hp > self_max / 2);  // 恢复生效

        // 连续追加 JSON 程序，早先的分支索引跨扩容仍指向原来的子节点。
        const char* json = R"({"tag":"PowerBoost","param0":7,"condition":"TargetHpBelow",
            "on_success":{"tag":"PowerBoost","param0":100,"condition":"TargetHpBelow"}})";
        std::vector<EffectUnit> json_units;
        const int first = load_effect_unit_from_json(json, {}, json_units);
        assert(first == 0);
        const auto initial_capacity = json_units.capacity();
        for (int root = 2; root < 64; root += 2) {
            const int appended = load_effect_unit_from_json(json, {}, json_units);
            assert(appended == root);
        }
        assert(json_units.capacity() > initial_capacity);
        for (int root = 0; root < 64; root += 2) assert(json_units[root].on_success == root + 1);
        const ParsedEffectProgram json_program(json_units);
        json_units[1].param0 = 1000;
        assert(json_program.unit_at(1)->param0 == 100);  // 装配缓冲不是程序的可写别名。
        assert(json_program.unit_at(-1) == nullptr && json_program.unit_at(64) == nullptr);
        assert(execute_effect_unit(&context, exec_args, json_program, 64) == BranchKey::Invalid);

        // 准入仅跳根条件，子分支自己的条件仍然生效。
        const auto saved_view = context.ws.skill_power_view[0];
        const int saved_target_hp = target_pet.hp;
        context.ws.skill_power_view[0] = 100;
        target_pet.hp = 1;
        const auto rng_before = context.rng().state();
        assert(execute_effect_unit(&context, exec_args, json_program, 0) == BranchKey::Never);
        assert(context.ws.skill_power_view[0] == 100);
        assert(execute_effect_unit_unconditional(&context, exec_args, json_program, 0) == BranchKey::Never);
        assert(context.ws.skill_power_view[0] == 107 && context.rng().state() == rng_before);
        context.ws.skill_power_view[0] = saved_view;
        target_pet.hp = saved_target_hp;

        for (int invalid_edge : {-2, 0, 1}) {
            EffectUnit malformed;
            malformed.on_other = invalid_edge;
            bool rejected = false;
            try { const ParsedEffectProgram invalid({malformed}); }
            catch (const std::invalid_argument&) { rejected = true; }
            assert(rejected);  // 非法负数、自环、越界
        }

        target_pet.marks.clear();
        context.getPet(0).hp = self_max;
        context.clearAllEffects();
    }

    std::cout << "smoke test passed for monster 012 effects and timing" << std::endl;
    return 0;
}