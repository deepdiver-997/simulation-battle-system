// 场景 8740 —— 驱逐/切出成因线（2026-11-21，worktree feat/expel-switch；号段 8740-8749）
//
// 覆盖面（内核 battleFsm/rule_center + 玄武 soul_lib/zhenwu_2553.cpp）：
//   A [玄武重分类 × 神印压制] 1581 神印挂层 = EVENT_SWAP watcher（对方主动切换）——
//     玄武 2553「双方的主动下场均视为驱逐下场」在场时，对方主动中切被改写为 EXPEL
//     （不发 EVENT_SWAP）→ 神印零挂层零扣血；对照（无玄武）照常挂层扣血。
//     语料点名压制对象含启灵元神（all_posts_final.jsonl 行 489）。
//   B [事件成因载荷] ENTER_STAGE switch_cause：主动切换=ACTIVE(0)、驱逐=EXPELLED(2)；
//     驱逐档 EVENT_SWAP 记录为空（"主动切换未发生"——事件不存在，不是 watcher 自滤）。
//   C [朝曦获得面压制] 2456 唱词获得 = EVENT_SWAP watcher（自身主动切换入场）——玄武
//     在场时梅赫维特中切入场零获得；对照获得 1 句。施放面（9901006 旧链路）的压制
//     由 roundChoice 不再是 CHOOSE_PET 成立（同机制，未在本场景重复搭建）。
//   D [天尊强制驱逐] forced_expel_pending 置位（模拟插件触发——插件契约就是置位）：
//     锁面板收包校验（SELECT_SKILL 被拒、NONE 收）→ ROUND_START 锚点机器填 EXPEL 随机
//     补位（**排除 is_locked 放逐锁与阵亡**——effect_des 402 + 神殇位 2026-11-21 用户
//     口径）→ 物理切场 + ENTER_STAGE(EXPELLED) + 无 EVENT_SWAP + 标志清零。
//   E [402 守卫失败] 无合法替补（全阵亡）→ 不填 op 不切场，标志清零。
//
// 断言全部行为面（HP / on_stage / plugin_storage 键 / 事件记录），无随机依赖
// （D 组候选集 {1,2} 两者皆合法，断言"∈ 集合"覆盖随机分支；sbs_srand 播种保序）。
#include <any>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打（物攻填充，空链）
constexpr int kZhuRi   = 38171;   // 逐日长诗 [2382,2034,1347,2384,2372]（2372 标签——朝曦获得面探针）
constexpr int kCountBase = 4697000;  // 2456 唱词总句数键（k421_meiheweite_lyrics.cpp 账本）

constexpr int kHolderMaxHp = 4000;   // 启灵元神最大体力（神印基数）
constexpr int kVictimMaxHp = 2500;   // 被挂方最大体力

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

void force_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

// 玄武（魂印 2553 绑定）
ElfPet zhenwu_pet(int speed = 100) {
    SoulMark sm(2553, "镇世神岳·玄武", "测试桩：驱逐重分类魂印 2553（effect_icon 行 2144）");
    ElfPet p = make_pet(4995, "镇世神岳·玄武", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, 100, 99999, sm);
    force_hit(p);
    return p;
}

// 启灵元神（魂印 1581 绑定——真插件程序，神印压制面测的就是 soul_plugin 加载链）
ElfPet qiling_pet(int slot_hp) {
    SoulMark sm(1581, "启灵元神", "神印：对方主动切换登场附加神印");
    ElfPet p = make_pet(3986, "启灵元神", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        100, 100, slot_hp, sm);
    force_hit(p);
    return p;
}

ElfPet plain_pet(int id, const char* name, int speed, int hp) {
    ElfPet p = make_pet(id, name, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed, 100, hp);
    force_hit(p);
    return p;
}

// 梅赫维特（魂印 2456 绑定；1 号位=逐日长诗 2372 标签 → 朝曦获得面探针）
ElfPet me_pet(int speed = 400) {
    SoulMark sm(2456, "梅赫维特", "测试桩：唱词魂印 2456（effect_icon 行 2038）");
    ElfPet p = make_pet(4853, "梅赫维特", {kZhuRi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, 300, 99999, sm);
    force_hit(p);
    return p;
}

// 事件记录 watcher：挂 ENTER_STAGE/SWAP 记录器（TEAM，永久），返回键用完即弃。
struct EventRecorder {
    std::vector<int> enter_causes;
    std::vector<int> swap_causes;
};

void attach_recorder(ScenarioBattle& b, EventRecorder& rec) {
    BattleContext& ctx = b.ctx();
    ctx.event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, 0, ctx.roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [&rec](BattleContext*, const BattleEvent& ev) {
            rec.enter_causes.push_back(ev.switch_cause);
        },
        WatcherScope::TEAM);
    ctx.event_center_.register_watcher(
        EventType::EVENT_SWAP, 0, ctx.roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [&rec](BattleContext*, const BattleEvent& ev) {
            rec.swap_causes.push_back(ev.switch_cause);
        },
        WatcherScope::TEAM);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A+B 神印压制 + 事件成因载荷（同一对局内对照）
    // ════════════════════════════════════════════════════════════════
    {
        sbs_srand(87401);
        // 0 号方：前排 + 场下 1 号槽启灵元神（神印提供方）；1 号方：前排 + 场下 1 号槽玄武
        std::array<ElfPet, 6> party0{plain_pet(3987, "前排0", 100, kVictimMaxHp),
                                     qiling_pet(kHolderMaxHp),
                                     plain_pet(3987, "前排0", 100, kVictimMaxHp),
                                     plain_pet(3987, "前排0", 100, kVictimMaxHp),
                                     plain_pet(3987, "前排0", 100, kVictimMaxHp),
                                     plain_pet(3987, "前排0", 100, kVictimMaxHp)};
        std::array<ElfPet, 6> party1{plain_pet(4001, "前排1", 100, 99999),
                                     zhenwu_pet(),
                                     plain_pet(4001, "前排1", 100, 99999),
                                     plain_pet(4001, "前排1", 100, 99999),
                                     plain_pet(4001, "前排1", 100, 99999),
                                     plain_pet(4001, "前排1", 100, 99999)};
        ScenarioBattle b(party0, party1);
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.find_pet_with_soulmark(0, 1581), 1, "A 前置：元神（1581）在 0 号方 1 号槽");
        CHECK_EQ(ctx.find_pet_with_soulmark(1, 2553), 1, "A 前置：玄武（2553）在 1 号方 1 号槽");
        CHECK(ctx.rule_center_.has_leave_cause_reclass(&ctx, ctx.roundCount),
              "A 前置：玄武 deploy 授票 → LEAVE_CAUSE_RECLASS armed");

        EventRecorder rec;
        attach_recorder(b, rec);

        // R1 双方原地切（同槽 = 无切换），R2 1 号方主动中切 → 被玄武改写为驱逐
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();

        CHECK_EQ(ctx.on_stage[1], 1, "A1 前置：1 号方实际切场（slot1 登场——动作面不变）");
        CHECK_EQ(ctx.seerRobot[1].elfPets[1].hp, 99999,
                 "A1 ★ 玄武在场：1 号方中切视为驱逐 → 神印零挂层零扣血"
                 "（若有 bug 挂层，阶段末扣 4000/8=500）");
        bool saw_expelled_enter = false;
        bool saw_active_enter = false;
        for (int c : rec.enter_causes) {
            if (c == static_cast<int>(SwitchCause::EXPELLED)) {
                saw_expelled_enter = true;
            }
            if (c == static_cast<int>(SwitchCause::ACTIVE)) {
                saw_active_enter = true;
            }
        }
        CHECK(saw_expelled_enter,
              "B1 ★ ENTER_STAGE switch_cause=EXPELLED(2)（驱逐成因随事件下发）");
        CHECK(rec.swap_causes.empty(),
              "B2 ★ 驱逐档零 EVENT_SWAP——主动键控 watcher 的触发源被掐（事件不存在）");
        std::cout << "  [A/B-玄武] 神印压制 + 成因载荷 + 零 SWAP 通过\n";
        // saw_active_enter 此刻应为 false（本局唯一真实切换就是驱逐档；同槽"切"不产事件）
        CHECK(!saw_active_enter, "B3 同局内无主动档登场（唯一切换已被改写）");

        // ── 对照：无玄武 → 神印照常挂层扣血 + ACTIVE 成因 ──
        sbs_srand(87402);
        std::array<ElfPet, 6> foe_plain{plain_pet(4001, "前排1", 100, 99999),
                                        plain_pet(4001, "前排1", 100, 99999),
                                        plain_pet(4001, "前排1", 100, 99999),
                                        plain_pet(4001, "前排1", 100, 99999),
                                        plain_pet(4001, "前排1", 100, 99999),
                                        plain_pet(4001, "前排1", 100, 99999)};
        ScenarioBattle b2(party0, foe_plain);
        b2.start();
        EventRecorder rec2;
        attach_recorder(b2, rec2);
        b2.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b2.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);
        b2.drive();
        b2.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b2.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b2.drive();
        CHECK_EQ(b2.ctx().seerRobot[1].elfPets[1].hp, 99999 - kHolderMaxHp / 8,
                 "A2 ★ 对照（无玄武）：神印照常挂层 → 阶段末扣 4000/8=500");
        CHECK(!rec2.swap_causes.empty() && rec2.swap_causes.back()
                      == static_cast<int>(SwitchCause::ACTIVE),
              "B4 对照：主动档 EVENT_SWAP 照发且带 ACTIVE(0) 成因");
        std::cout << "  [A2/B4-对照] 无玄武 → 神印照常 + ACTIVE 成因 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C 朝曦获得面压制（2456 = EVENT_SWAP watcher，自身主动切换入场）
    // ════════════════════════════════════════════════════════════════
    {
        // ── C1 对照（无玄武）：中切入场 → 获得 1 句 ──
        sbs_srand(87403);
        std::array<ElfPet, 6> my{plain_pet(4854, "前排", 200, 99999),
                                 me_pet(),
                                 plain_pet(4854, "前排", 200, 99999),
                                 plain_pet(4854, "前排", 200, 99999),
                                 plain_pet(4854, "前排", 200, 99999),
                                 plain_pet(4854, "前排", 200, 99999)};
        std::array<ElfPet, 6> foe{plain_pet(4001, "对手", 100, 99999),
                                  plain_pet(4001, "对手", 100, 99999),
                                  plain_pet(4001, "对手", 100, 99999),
                                  plain_pet(4001, "对手", 100, 99999),
                                  plain_pet(4001, "对手", 100, 99999),
                                  plain_pet(4001, "对手", 100, 99999)};
        std::array<ElfPet, 6> foe_zhenwu = foe;
        foe_zhenwu[1] = zhenwu_pet();   // 玄武进 1 号方背包
        {
            ScenarioBattle b(my, foe);
            b.start();
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 中切入场梅赫维特
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(b.ctx().on_stage[0], 1, "C1 前置：梅赫维特登场");
            CHECK_EQ(read_key(b, kCountBase + 0), 1,
                     "C1 ★ 对照（无玄武）：朝曦获得 1 句（1 号位 2372 标签）");
            std::cout << "  [C1-对照] 朝曦照常获得 通过\n";
        }
        // ── C2 玄武在场：同一套动作 → 零获得 ──
        {
            ScenarioBattle b(my, foe_zhenwu);
            b.start();
            CHECK(b.ctx().rule_center_.has_leave_cause_reclass(&b.ctx(), b.ctx().roundCount),
                  "C2 前置：改写票 armed");
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(b.ctx().on_stage[0], 1, "C2 前置：梅赫维特照常登场（动作面不变）");
            CHECK_EQ(read_key(b, kCountBase + 0), -1,
                     "C2 ★ 玄武在场：0 号方主动切出视为驱逐 → 零 EVENT_SWAP → 零获得");
            std::cout << "  [C2-玄武] 朝曦获得面压制 通过\n";
        }
    }

    // ════════════════════════════════════════════════════════════════
    // D 天尊强制驱逐（插件契约 = 置 pending 位；本场景直写模拟）
    // ════════════════════════════════════════════════════════════════
    {
        sbs_srand(87404);
        // 0 号方：slot0 受驱逐者在场；slot1/2 合法替补；slot3 放逐锁（必须排除）；
        // slot4/5 阵亡（必须排除）。1 号方：全同款填充。
        ElfPet victim  = plain_pet(5001, "受驱逐者", 100, 99999);
        ElfPet cand_a  = plain_pet(5002, "替补A", 100, 99999);
        ElfPet cand_b  = plain_pet(5003, "替补B", 100, 99999);
        ElfPet locked  = plain_pet(5004, "被放逐", 100, 99999);
        locked.is_locked = true;   // 神殇放逐位（神殇线同款字段）
        ElfPet dead    = plain_pet(5005, "阵亡", 100, 99999);
        dead.hp = 0;
        std::array<ElfPet, 6> party0{victim, cand_a, cand_b, locked, dead, dead};
        std::array<ElfPet, 6> party1{plain_pet(4001, "对手", 100, 99999),
                                     plain_pet(4001, "对手", 100, 99999),
                                     plain_pet(4001, "对手", 100, 99999),
                                     plain_pet(4001, "对手", 100, 99999),
                                     plain_pet(4001, "对手", 100, 99999),
                                     plain_pet(4001, "对手", 100, 99999)};
        ScenarioBattle b(party0, party1);
        b.start();
        BattleContext& ctx = b.ctx();

        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.drive();
        // 模拟插件触发（混元天尊线"辛死亡驱逐对手"的插件侧全部职责 = 置位）
        ctx.forced_expel_pending[0] = 1;

        EventRecorder rec;
        attach_recorder(b, rec);

        // R2：锁面板——SELECT_SKILL 必被拒（收包校验只认 NONE，暴露集与接受集同源）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.operation_collected[0], 0,
                 "D1 ★ 面板锁：SELECT_SKILL 被拒（未收集，要求重交）");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();

        CHECK_EQ(ctx.forced_expel_pending[0], 0, "D2 ★ 标志消费即清（无跨回合残留）");
        const int stage = ctx.on_stage[0];
        CHECK(stage == 1 || stage == 2,
              "D3 ★ 随机补位 ∈ {替补A, 替补B}——放逐锁(slot3)与阵亡(slot4/5)被排除"
              "（2026-11-21 用户口径：补位候选排除神殇 is_locked）");
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 99999, "D4 前置：受驱逐者下场存活（不是死亡）");
        CHECK(ctx.seerRobot[0].elfPets[stage].hp > 0, "D5 前置：新登场者存活");
        bool d_expelled = false;
        for (int c : rec.enter_causes) {
            if (c == static_cast<int>(SwitchCause::EXPELLED)) {
                d_expelled = true;
            }
        }
        CHECK(d_expelled, "D6 ★ ENTER_STAGE(EXPELLED)：强制驱逐走同一成因档");
        CHECK(rec.swap_causes.empty(), "D7 ★ 强制驱逐零 EVENT_SWAP（不是主动切换）");
        std::cout << "  [D] 面板锁 + 随机补位排除放逐锁/阵亡 + 成因载荷 通过\n";

        // ── E 402 守卫失败：无合法替补 → 不切场、标志清零 ──
        sbs_srand(87405);
        ElfPet last = plain_pet(5001, "独苗", 100, 99999);
        ElfPet corpse = plain_pet(5005, "阵亡", 100, 99999);
        corpse.hp = 0;
        std::array<ElfPet, 6> solo{last, corpse, corpse, corpse, corpse, corpse};
        ScenarioBattle b2(solo, party1);
        b2.start();
        b2.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b2.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);
        b2.drive();
        b2.ctx().forced_expel_pending[0] = 1;
        b2.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b2.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b2.drive();
        CHECK_EQ(b2.ctx().on_stage[0], 0,
                 "E1 ★ 无合法替补（effect_des 402：对方背包中不存在其他存活精灵）→ 不驱逐");
        CHECK_EQ(b2.ctx().forced_expel_pending[0], 0, "E2 守卫失败也清标志");
        std::cout << "  [E] 402 守卫失败（无替补） 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // F 场域管辖开关（薇尔诗 2513 场下源抑制 × 驱逐重分类，2026-11-21 用户裁决）
    //   F1 默认豁免（官方双标现状）：场域开启 + 玄武在场下 → 重分类照常生效
    //   F2 非豁免票受场域：同条件下票被抑制（刷新豁免位 = 一视同仁后的授予形态；
    //     全量切换 = 翻 rule_center.h 的 kReclassSubjectToOfffieldBlock 常量）
    //   F3 场域关闭 → 非豁免票恢复
    // ════════════════════════════════════════════════════════════════
    {
        sbs_srand(87406);
        std::array<ElfPet, 6> p0{plain_pet(4001, "甲", 100, 99999),
                                 plain_pet(4001, "甲", 100, 99999),
                                 plain_pet(4001, "甲", 100, 99999),
                                 plain_pet(4001, "甲", 100, 99999),
                                 plain_pet(4001, "甲", 100, 99999),
                                 plain_pet(4001, "甲", 100, 99999)};
        std::array<ElfPet, 6> p1{plain_pet(4001, "乙", 100, 99999),
                                 zhenwu_pet(),
                                 plain_pet(4001, "乙", 100, 99999),
                                 plain_pet(4001, "乙", 100, 99999),
                                 plain_pet(4001, "乙", 100, 99999),
                                 plain_pet(4001, "乙", 100, 99999)};
        ScenarioBattle b(p0, p1);
        b.start();
        BattleContext& ctx = b.ctx();
        auto gate = [](BattleContext* c, int owner, int mark_id) {
            return c->reclass_offfield_gate_pass(owner, mark_id);
        };
        CHECK(ctx.find_pet_with_soulmark(1, 2553) == 1, "F 前置：玄武在场下 1 号槽");
        CHECK(ctx.rule_center_.has_leave_cause_reclass(&ctx, ctx.roundCount, gate),
              "F 前置：票 armed");
        // 模拟薇尔诗 2513 场域开启（roster_field.cpp 2513 程序的置位面）
        ctx.block_offstage_to_onstage = true;
        CHECK(ctx.rule_center_.has_leave_cause_reclass(&ctx, ctx.roundCount, gate),
              "F1 ★ 默认豁免（官方双标）：场域开启 + 源在场下 → 重分类照常生效");
        // 刷新 2553 票豁免位 = false（一视同仁后的授予形态；覆盖键刷新不追加）
        ctx.rule_center_.grant_leave_cause_reclass(
            /*source_owner=*/1, /*source_effect_id=*/2553,
            [](BattleContext* c, int, int, bool) -> bool {
                if (!c) {
                    return false;
                }
                const int h = c->find_mark_in_roster(1, 2553);
                return h >= 0 && c->seerRobot[1].elfPets[h].hp > 0;
            },
            EffectScope::TEAM, /*outfield_block_exempt=*/false);
        CHECK(!ctx.rule_center_.has_leave_cause_reclass(&ctx, ctx.roundCount, gate),
              "F2 ★ 非豁免票：场域开启 + 源在场下 → 本票被抑制（薇尔诗 2513）");
        ctx.block_offstage_to_onstage = false;
        CHECK(ctx.rule_center_.has_leave_cause_reclass(&ctx, ctx.roundCount, gate),
              "F3 场域关闭 → 非豁免票恢复生效");
        std::cout << "  [F] 场域管辖开关（默认豁免/非豁免抑制/场域关闭恢复） 通过\n";
    }

    std::cout << "scenario_8740 PASS（驱逐/切出成因线：重分类压制/成因载荷/强制驱逐/守卫/场域开关）"
              << std::endl;
    return 0;
}
