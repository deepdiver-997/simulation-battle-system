// 场景 8032 —— 「曝」·被击败效果失效端到端 + 第三方死亡观察者不受影响 + 免死不穿
// + 低段抗性惩罚（含全免）
// —— resources/soul_lib/k421_meiheweite_lyrics.cpp（曝线）+ include/effects/event_center.h
//    （既有「被击败效果失效」压制门：event_center.h owner==target 监听门 +
//    battleContext.cpp DEFEAT_STATUS/AFTER_DEFEATED 桶门，百罗鬼帝 2085 同款）
//
// 语料口径：
//   「曝的封亡语效果就类似于星皇之怒，同时对于免死类的魂印或者技能效果都不受影响」
//   「"被击败、死亡"效果不会受到曝影响，属于第三方效果（赫斯提亚灼火/神罗裂缝/
//     奥菲支援/天尊…）」——即只封倒下方**自己**的亡语，击杀方/第三方观察照常。
//
// 真实装配载体：
//   · 皎明若玉 36731 + 冰骷守护 28274（effect 1882：自己被击败 → 己方下只出战精灵
//     抵挡 N 次攻击——EVENT_DEATH 自有监听面，e_block_family.cpp）→ 压制门监听面。
//   · 幻草灵狮 2618（魂印 309：击败中毒的对手 → 下一个出场的对手也中毒——
//     AFTER_DEFEATED 击杀方桶，k308_kill_poison_inherit.cpp）→ 验证倒下方带曝时
//     **击杀方桶照常投递**（第三方"被击败/死亡"效果不受曝影响）。
//
// ⚠️ 记档（既有门覆盖面，非本单扩大）：死亡结算时点桶（15 FIRST/26 SECOND_MOVER_
//   DEATH，K402/K404 死亡双桶族如 2494 托勒斯）**不在**压制门内——is_defeated_timing_state
//   只盖 DEFEAT_STATUS/AFTER_DEFEATED（百罗 2026-09-22 既有口径）。曝/百罗的官方语义
//   理应盖全部"被击败时"时点；扩门属内核口径变更（影响百罗既有行为面），另单裁决。
//
// 白盒面说明：8031 已用真实永沐端到端挂曝；本文件无梅赫维特在场，用
// meiheweite_bao::attach_bao + set_defeat_effects_suppressed 白盒等价面（= 永沐挂曝
// + ENTER_STAGE 刷新的产物）。
#include <any>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "../../resources/meiheweite_bao_runtime.h"

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;    // 拍打（物攻填充，空链）
constexpr int kBingKu = 28274;  // 冰骷守护 [191,1839,1882]（皎明若玉 36731 真实载体技）
constexpr int kMaxHp = 100000;

// e_block_family.cpp:73 的 1882 待决旗（+side）
constexpr int k1882PendingBase = 990188200;
// k308_kill_poison_inherit.cpp 的 309 传染待决旗（+owner）
constexpr int k309PendingBase = 990808000;

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

bool has_anomaly(ScenarioBattle& b, int side, int id) {
    return b.ctx().abnormal_status_end_round[side][id] > b.ctx().roundCount;
}

// 高速攻击手（先手拍打；只用于把 1 血目标打倒）。
ElfPet attacker_pet(int speed = 900) {
    ElfPet p = make_pet(4, "攻击手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed, 300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet dummy_pet(int speed = 100) {
    ElfPet p = make_pet(3, "队友", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed, 1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 皎明若玉（1882 真实载体：skill 28274，无魂印）。
ElfPet ruoyu_pet() {
    ElfPet p = make_pet(36731, "皎明若玉", {kBingKu, kPaDa, kPaDa, kPaDa, kPaDa}, 100, 1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 幻草灵狮（309 真实宿主：魂印 args=[1,0] 中毒码，4640 同款装配）。
ElfPet lingshi_pet() {
    SoulMark sm(309, "幻草灵狮魂印", "test", EffectArgs(std::vector<int>{1, 0}));
    sm.monster_id = 2618;
    ElfPet p = make_pet(2618, "幻草灵狮", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 900, 300, kMaxHp, sm);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 白盒挂曝（= 永沐挂曝 + 刷新的产物；无梅赫维特在场时的等价面）。
void preset_bao(ScenarioBattle& b, int side) {
    meiheweite_bao::attach_bao(&b.ctx(), side, b.ctx().on_stage[side]);
    b.ctx().set_defeat_effects_suppressed(side, true);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8032a 1882 自有 EVENT_DEATH 监听面：对照触发 / 带曝被门掉 ════════════
    //  皎明若玉用冰骷守护（1882 布网）→ 被击倒：
    //  对照：自有监听投递 → 待决旗置位（补位宠获得抵挡票）；
    //  带曝：自有监听被门（watcher.owner == event.target）→ 待决旗不置位；
    //        击杀方探针（第三方）照常收到 EVENT_DEATH。
    auto t8032a = [&](bool with_bao, const char* tag) {
        sbs_srand(with_bao ? 80321 : 80322);
        ScenarioBattle b({attacker_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet()},
                         {ruoyu_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet()});
        b.start();
        bool third_party_saw_death = false;
        b.ctx().event_center_.register_watcher(
            EventType::EVENT_DEATH, 0, b.ctx().roundCount, /*duration_rounds=*/0, /*once=*/false,
            [&](BattleContext*, const BattleEvent&) { third_party_saw_death = true; },
            WatcherScope::TEAM);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 皎明若玉布网
        b.drive();
        CHECK_EQ(read_key(b, k1882PendingBase + 1), -1, "T8032a 前置：未倒地无待决旗");
        b.ctx().seerRobot[1].elfPets[0].hp = 1;
        if (with_bao) {
            preset_bao(b, 1);
            CHECK_EQ(b.ctx().is_defeat_effects_suppressed(1), true, "T8032a 前置：压制位武装");
        }
        // 我方先手攻击手击倒 1 血皎明若玉 → 死亡漏斗登记。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(b.ctx().pet_death_notified[1][0], true, "T8032a 死亡照常登记（曝不拦死亡本身）");
        CHECK_EQ(third_party_saw_death, true, "T8032a 第三方死亡探针照常收到 EVENT_DEATH");
        if (with_bao) {
            CHECK_EQ(read_key(b, k1882PendingBase + 1), -1, "T8032a 带曝：自有监听被门 → 待决旗不置位");
        } else {
            CHECK_EQ(read_key(b, k1882PendingBase + 1), 1, "T8032a 对照：自有监听投递 → 待决旗置位");
            // 补位宠登场 → 1882 授抵挡票（is_immune 查询面）。
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.drive();
            CHECK_EQ(b.ctx().is_immune(1, ImmunityType::DAMAGE, State::BATTLE_FIRST_ATTACK_DAMAGE), true,
                     "T8032a 对照：补位宠获得抵挡票");
        }
        std::cout << "  [T8032a] " << tag << " 通过\n";
    };
    t8032a(false, "1882 对照（无曝）");
    t8032a(true, "1882 带曝封亡语");

    // ══ T8032b 309 击杀方桶：倒下方带曝，击杀方的"被击败/死亡"观察照常触发 ════
    //  （语料第三方清单口径：只有倒下方**自己**的亡语被封，击杀方结算不受影响。）
    {
        sbs_srand(80323);
        ScenarioBattle b({lingshi_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet()},
                         {dummy_pet(100), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet()});
        b.start();
        // 对手在场宠：挂曝 + 中毒（309 归因门的"被击败者带异常"前提）。
        b.ctx().seerRobot[1].elfPets[0].hp = 1;
        preset_bao(b, 1);
        const ApplyAnomalyResult p = apply_anomaly(&b.ctx(), 1, 1, /*duration=*/3, /*actor=*/0);
        CHECK_EQ(p == ApplyAnomalyResult::SUCCESS, true, "T8032b 前置：对手中毒");
        // 幻草灵狮先手击倒中毒+带曝的对手 → 309 击杀方桶（我方未被压制）→ 挂传染旗。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(b.ctx().pet_death_notified[1][0], true, "T8032b 前置：带曝载体已倒地");
        CHECK_EQ(read_key(b, k309PendingBase + 0), 1,
                 "T8032b 击杀方 309 桶照常执行（倒下方曝不封第三方）");
        // 对手补位登场 → 传染落地：下一个出场的对手也中毒。
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(has_anomaly(b, 1, 1), true, "T8032b 下一个出场的对手进入中毒状态");
        std::cout << "  [T8032b] 309 第三方死亡观察不受曝影响 通过\n";
    }
    // ══ T8032c 免死拦截器不受曝影响：带曝载体免死成功，曝不注销 ══════════════
    {
        sbs_srand(80324);
        ScenarioBattle b({attacker_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet()},
                         {dummy_pet(100), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet()});
        b.start();
        preset_bao(b, 1);
        // 探针免死（残留体力层）：写回 500 血拦下死亡。
        DeathInterceptor ic;
        ic.source_effect_id = 1;
        ic.owner = -1;
        ic.sees_hp_consume = false;
        ic.fn = [](BattleContext* c, int side, int slot, int, DefeatCause) {
            c->seerRobot[side].elfPets[slot].hp = 500;
            return true;
        };
        b.ctx().death_interceptors.push_back(ic);
        b.ctx().seerRobot[1].elfPets[0].hp = 1;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(b.ctx().seerRobot[1].elfPets[0].hp, 500, "T8032c 免死拦截器照常拦下");
        CHECK_EQ(b.ctx().pet_death_notified[1][0], false, "T8032c 死亡未登记（拦截成立）");
        CHECK_EQ(meiheweite_bao::has_bao(&b.ctx(), 1, b.ctx().on_stage[1]), true,
                 "T8032c 未死 → 曝保留（消逝只在真死亡）");
        std::cout << "  [T8032c] 免死不穿曝 通过\n";
    }
    // ══ T8032d 低段抗性惩罚：≤50% 段（含全免%）被扣穿；>50% 段不受影响 ════════
    {
        sbs_srand(80325);
        ScenarioBattle b({attacker_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet()},
                         {dummy_pet(100), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet()});
        b.start();
        ElfPet& carrier = b.ctx().seerRobot[1].elfPets[b.ctx().on_stage[1]];
        carrier.resistance.setCompleteImmunity(5);         // 5% 全免（≤50% 段）
        carrier.resistance.setUncontrolSlot(0, 2, 10);     // 烧伤抗性 10%（≤50% 段）
        carrier.resistance.setUncontrolSlot(1, 3, 60);     // 异常3 抗性 60%（>50% 段）
        preset_bao(b, 1);
        // ① 带曝：两段 ≤50% 全部扣穿 → 无论种子永不抵抗（确定性断言）。
        for (int i = 0; i < 50; ++i) {
            CHECK_EQ(carrier.resistance.isResistantTo(2, meiheweite_bao::kBaoResistLowPenalty),
                     false, "T8032d 5%全免+10%烧伤抗性被 10 点扣穿 → 永不抵抗");
        }
        // ② 同种子双跑：>50% 段不受惩罚影响（两次消费同一随机序列 → 结果一致）。
        sbs_srand(80326);
        const bool no_penalty = carrier.resistance.isResistantTo(3, 0);
        sbs_srand(80326);
        const bool with_penalty = carrier.resistance.isResistantTo(3, meiheweite_bao::kBaoResistLowPenalty);
        CHECK_EQ(no_penalty, with_penalty, "T8032d 60%（>50%）抗性不受惩罚影响");
        // ③ 端到端：带曝载体吃烧伤 → 抗性段已扣穿，永不 RESISTED（无随机项）。
        for (int i = 0; i < 3; ++i) {
            const ApplyAnomalyResult r = apply_anomaly(&b.ctx(), 1, 2, /*duration=*/3, /*actor=*/0);
            CHECK(r != ApplyAnomalyResult::RESISTED_BY_RESISTANCE,
                  "T8032d 端到端：带曝载体不被低段抗性抵抗");
        }
        CHECK_EQ(has_anomaly(b, 1, 2), true, "T8032d 烧伤在册");
        std::cout << "  [T8032d] 低段抗性惩罚/全免扣穿/高段豁免 通过\n";
    }

    std::cout << "scenario_8032 全部通过" << std::endl;
    return 0;
}
