// 场景 W：自爆传承——"牺牲自己，己方**下一只登场**精灵获得一次不可穿盔"。
//
// 机制锚点（用户指定 + effect_info 实查）：**虚空附体（37750，原始黑洞·尤纳斯 4717）**
//   effect 1212："消耗自身全部体力，使己方下只出战精灵令对手下{0}次使用的攻击技能无效
//   且{1}回合内造成的攻击伤害额外提升{2}%"，args=[1,2,100,350,1] → 令对手下 **1** 次
//   攻击技能无效 + 2 回合伤害+100%。
//   - 本场景验证的就是 1212 的"令对手下 1 次攻击技能无效"子句（= 一次不可穿盔）；
//   - "2 回合伤害提升"子句未实现（需要回合级伤害加成 buff 的持久授予，留待）；
//   - 同家族：1182（终末之光 30819，"自爆+下一只获得{1}次奇迹之力"）、
//     1275（守御八方 31505，"消耗自身全部体力，使下只出场精灵前{0}次出手必定先手必定暴击"，
//     见文末未来用例）。
//
// 引擎积木：perform_switch 统一 emit EVENT_ENTER_STAGE（主动中切 + 死亡换宠共同漏斗）→
//   "下一只登场"监控（EVENT_ENTER_STAGE watcher，TEAM scope + 回调内自删）→
//   grant_skill_invalid（内联包装，次数型 1 次、penetrable=false）挂到**对方**桶
//   （条目挂"被拦截方"，见技能判定流程与无效效果体系.md §5.2）。
//
// ⚠️ 监控写法约定：**once=false + 回调内过滤 + 自删**，不能用 once=true——
//   EventCenter 的 once 在回调过滤不命中时也会被消耗（对手先切人 → 监控静默消失）。
//
// 验证点：
//   W1 自爆死亡本身不产生盔；登场时点才挂（死亡 ≠ 传承）
//   W2 继承者登场后，对手下次攻击被不可穿盔挡下（SKILL_INVALID），一次性（响应即耗）
//   W3 盔耗尽后恢复：对手再攻击正常命中
//
// ┌─ 未来用例（用户留言：同属自爆传承家族，实现"必定先手+必定暴击"凭证后加到这里）─────
// │ 守御八方（31505，effect 1275，args[2]=2）："消耗自身全部体力，使下只出场精灵前 2 次
// │ 出手必定先手必定暴击"。与 W 的差异：传承物是 2 次出手凭证（先手权 + 暴击），不是一次
// │ 拦截；需要"次数型先手/暴击凭证"的授予与消费落点（先手权在选择期、暴击在伤害公式前），
// │ 且传承粒度是"前 N 次出手"而非"下次响应"。注意官方文本是"下只出场精灵"而非
// │ "本队下两次选择"——若按后者理解则还需要队伍级计数器，待用户对照游戏内实测定夺。
// │ ✅ 已实现（2026-09-30，resources/moves_lib/diwang_3512_skills.cpp）：粒度按实测档
// │ = 全队共享 2 次（队伍级 plugin_storage 计数器）；嗑药/切换/被控不消耗；另含命中
// │ 修正（accuracy_fix + 条件必中凭证）。断言见 scenario_781_diwang_3512.cpp [B] 组。
// └──────────────────────────────────────────────────────────────────────────
#include <cassert>
#include <iostream>
#include <memory>

#include <entities/pet_factory.h>
#include <effects/continuousEffect.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillGaoSuYiDong = 20017;  // 高速移动（威力 0）
constexpr int kSkillYanFan = 10037;       // 燕返（威力 60，必中——对手攻击用）
constexpr int kSkillXuKongFuTi = 37750;   // 虚空附体（原始黑洞·尤纳斯，must_hit，effect 1212 自爆传承）

constexpr int kArmorEffectId = 1212;      // 传承盔 = effect 1212 的"令对手下1次攻击技能无效"子句

// 注入的"自爆"效果（1212 子句①）：牺牲全部体力 + 注册"下一只登场精灵"监控。
// 1212 子句②（{1}回合内伤害+{2}%）未实现，留待回合级伤害 buff 机制。
EffectResult self_destruct_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->getPet(owner).hp = 0;  // 牺牲全部体力

    // "下一只登场"监控：TEAM scope（撑过死亡换宠/断回合的混乱）；once=false + 自删。
    auto watcher_id = std::make_shared<int>(-1);
    *watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE,
        owner,
        ctx->roundCount,
        /*duration_rounds=*/0,  // 永久（自删或战斗结束回收）
        /*once=*/false,         // ⚠️ 不能用 true：过滤不命中也会被 once 消耗
        [owner, watcher_id](BattleContext* wctx, const BattleEvent& event) {
            if (event.actor != owner) {
                return;  // 只认己方登场（对手先切人不算）
            }
            // 传承：给"下一个登场的己方精灵"挂一次不可穿盔。统一语义 source=己方(自爆方)、
            // target=对方（被拦截方，对方下次攻击技能无效）；次数型 1 次、不可穿透。
            // scope=TEAM：跨切换保留（自爆方已死，盔应只随次数消费消失，不因换宠清）。
            wctx->grant_skill_invalid(/*source=*/owner, /*target=*/1 - owner, /*source_slot=*/-1,
                                      /*effect_id=*/kArmorEffectId, SealKind::SEAL_ATTACK,
                                      /*counts=*/1, /*rounds=*/0,
                                      /*penetrable=*/false, EffectScope::TEAM);
            wctx->event_center_.remove_watcher(*watcher_id);  // 自删："下一只"语义
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto bomber = scenario::make_pet(201, "自爆者",
                                     {kSkillXuKongFuTi, kSkillGaoSuYiDong, kSkillGaoSuYiDong,
                                      kSkillGaoSuYiDong, kSkillGaoSuYiDong},
                                     /*speed=*/300, /*atk=*/100, /*hp=*/5000);
    auto heir = scenario::make_pet(202, "继承者",
                                   {kSkillGaoSuYiDong, kSkillGaoSuYiDong, kSkillGaoSuYiDong,
                                    kSkillGaoSuYiDong, kSkillGaoSuYiDong},
                                   /*speed=*/300, /*atk=*/100, /*hp=*/5000);
    auto enemy = scenario::make_pet(20201, "对手",
                                    {kSkillYanFan, kSkillGaoSuYiDong, kSkillGaoSuYiDong,
                                     kSkillGaoSuYiDong, kSkillGaoSuYiDong},
                                    /*speed=*/100, /*atk=*/100, /*hp=*/5000);

    scenario::ScenarioBattle battle({bomber, heir, bomber, bomber, bomber, bomber},
                                    scenario::make_party(enemy));
    BattleContext& ctx = battle.ctx();
    battle.start();

    // 注入自爆效果（ON_SKILL_HIT、once：只引爆一次）
    {
        Effect self_destruct(99030, 0, 0, 0, EffectArgs(std::vector<int>{0}),
                             &self_destruct_effect);  // 全参构造（Effect 无默认构造）
        auto ce = std::make_unique<ContinuousEffect>(
            self_destruct, State::BATTLE_FIRST_ON_SKILL_HIT, /*owner=*/0,
            /*duration=*/-1, ctx.roundCount);
        ce->source_id_ = 99030;
        ce->once_ = true;
        ctx.registerEffect(State::BATTLE_FIRST_ON_SKILL_HIT, 0, std::move(ce),
                            EffectContainer::Skill);
    }
    ctx.seerRobot[0].elfPets[1].hp = 4000;  // 继承者（0 号方 1 号槽，尚未登场）掉点血，
                                            // R2 断言"没被打到"才有分辨力

    // R1：自爆（先手+2）→ 死亡 → 己方死亡换宠
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    battle.drive();  // 停在 CHOOSE_AFTER_DEATH（等 0 号方选替补）

    REQUIRE(ctx.getPet(0).hp <= 0);                    // 自爆者阵亡
    for (const RuleTicket& t : ctx.rule_center_.entries()) {
        std::cerr << "[dbg] cat=" << static_cast<int>(t.category) << " sub=" << t.subtype
                  << " eff=" << t.source_effect_id << " src=" << t.source_owner
                  << " tgt=" << t.target << " rounds=" << t.remaining_rounds << std::endl;
    }
    // W1a：死亡本身不产生盔。桶里唯一合法条目 = 2148 传承次免（虚空附体数据自带，
    // 2026-09-20 实现前被静默跳过），不得出现 SEAL 盔类条目。
    REQUIRE(ctx.rule_center_.size() == 1);
    REQUIRE(ctx.rule_center_.entries()[0].source_effect_id == 2148
           && ctx.rule_center_.entries()[0].category == RuleCategory::IMMUNE);
    REQUIRE(ctx.event_center_.watch_count(EventType::EVENT_ENTER_STAGE) == 1);  // 监控在等
    battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);

    // 继承者登场（死亡换宠路径的 EVENT_ENTER_STAGE）→ 监控触发 → 盔挂上对方桶、监控自删
    REQUIRE(ctx.rule_center_.size() == 2);   // W1b：登场时点挂上（桶内另有 2148 传承次免）
    {   // 那条盔应不可穿透
        bool found_impenetrable = false;
        for (const RuleTicket& t : ctx.rule_center_.entries()) {
            if (t.target == 1 && t.penetrable == false) {
                found_impenetrable = true;
            }
        }
        REQUIRE(found_impenetrable);
    }
    REQUIRE(ctx.event_center_.watch_count(EventType::EVENT_ENTER_STAGE) == 0);  // 已自删
    std::cout << "  [W1] 死亡不传、登场才传：盔挂上对方桶（不可穿透、1 次），监控自删" << std::endl;

    // R2：对手攻击 → 被不可穿盔挡下（SKILL_INVALID），一次性消费
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();

    REQUIRE(ctx.ws.skill_exec_result[1] == SkillExecResult::SKILL_INVALID);  // 被盔挡下
    REQUIRE(ctx.rule_center_.size() == 1);                         // 盔响应即耗（桶内剩 2148 传承次免）
    REQUIRE(ctx.getPet(0).hp == 4000);                                       // 继承者没被打到
    std::cout << "  [W2] 对手燕返被不可穿盔挡下（SKILL_INVALID），盔响应即耗，继承者无伤" << std::endl;

    // R3：盔已耗尽 → 对手攻击正常命中
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();

    REQUIRE(ctx.ws.skill_exec_result[1] != SkillExecResult::SKILL_INVALID);  // 不再被挡
    REQUIRE(ctx.ws.skill_power_view[1] == 60);                               // 燕返正常执行
    REQUIRE(ctx.getPet(0).hp < 4000);                                        // 继承者掉血了
    std::cout << "  [W3] 盔耗尽后恢复：燕返正常命中，继承者 " << 4000 << "->"
              << ctx.getPet(0).hp << std::endl;

    std::cout << "scenario_w PASS (自爆传承：死亡不传/登场挂盔/不可穿盔一次性/耗尽恢复)"
              << std::endl;
    return 0;
}
