// 场景 7980 —— K420 逐宠定制池五批：慈辉永颂·瑞亚 4888 消除→封禁/免疫族
// （2433 消除双方回合类→封对手属性技 / 2434 消除双方能力提升→封对手攻击技 /
//   2435 解除双方异常→己方免疫异常窗 / 2437 无视能力提升→成功则断对手回合；
//   2436 选技期自先制 = 内核门槛遗留不产码，见实现文件头"落地范围裁决"）
//   —— moves_lib/k420_ruiya_pool.cpp（切片对参/四查/口径记档见实现头）
//
// 断言口径：
//   - 四宏全为一次性战时效果：行为断言（封禁真拦技能、免疫真挡异常、断回合真清窗）
//     + 实现侧审计槽（10<id>000x 段）精确值（成功位/计数/行参回合数）双面。
//   - 技能槽位约定：载体技 = 槽 0、拍打填充 = 槽 1；对手槽位见各 block 注释。
//   - 封禁票存活口径 = SEAL 条目存在且 remaining_rounds>0（720 场景记档：has_seal_kind
//     对 register_round=0 的活票会误报过期，禁用；本场景按 source_effect_id 过滤）。
//   - 窗口回合模型（rule_center tick_rounds 在回合末扣减，battleFsm.cpp:2169）：
//     "N回合内"含当回合（2009 口径）→ R2 授予 2 回合 = R2/R3 生效、R4 放行；到期 =
//     行为翻转（窗口内被封技能真失效、过期后同一技能真生效）。
//   - 先制窗（843）会翻转行动顺序（R2 起对手先手）——对手在封禁授予前的行动照常落地，
//     断言全部落在"整轮 drive() 之后的终态"，不依赖轮内时序；封禁授予后的对手行动
//     才是被拦面（R3 探针轮）。
//   - 无随机精确断言（纪律：概率类禁 while(!FINISHED) 步进）；所有施加/消除链路
//     chance=100 或必中，无掷骰依赖（剧毒粉 1605 [100,1] 申报 100）。
//   - roundCount 映射：人类第 N 回合处理期间 roundCount == N-1（advanceRound 在
//     ROUND_COMPLETION）。
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;          // 拍打（物理攻击填充）
constexpr int kMaxHp = 100000;

// 载体技（链与切片见实现文件头）
constexpr int kYuShen = 29305;        // 予神繁祉 [2433,2434,2435][2,2,2]
constexpr int kYuTian = 29303;        // 予天繁星 [2437,1687,2196,2196,2196][4,...]

// setup 载体（全已注册技能；剑指风云=强化+双回合窗、超速闪身=纯先制窗、剧毒粉=100%施加）
constexpr int kJianZhi = 24950;       // 剑指风云 [962,707,843][1,4,3,2,2,2]
constexpr int kChaoSu  = 28469;       // 超速闪身 [843][4,2]
constexpr int kJuDu    = 28858;       // 剧毒粉 [1605][100,1]（100%令对手中毒）

// 实现文件账本键（10<id>000x 审计段，与实现 constexpr 一致）
constexpr int k2433BreakAudit   = 1024330001;
constexpr int k2433RoundsAudit  = 1024330002;
constexpr int k2434ClearAudit   = 1024340001;
constexpr int k2434RoundsAudit  = 1024340002;
constexpr int k2435DispelAudit  = 1024350001;
constexpr int k2435RoundsAudit  = 1024350002;
constexpr int k2437PierceAudit  = 1024370001;
constexpr int k2437SuccessAudit = 1024370002;

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

// 封禁票存活口径（720 场景同式；按 source_effect_id 过滤到本族宏）。
bool seal_ticket_alive(ScenarioBattle& b, int target, SealKind kind, int effect_id) {
    for (const RuleTicket& t : b.ctx().rule_center_.entries()) {
        if (t.category == RuleCategory::SEAL && t.target == target
            && static_cast<SealKind>(t.subtype) == kind
            && t.source_effect_id == effect_id && t.remaining_rounds > 0) {
            return true;
        }
    }
    return false;
}

bool foe_has_round_state(ScenarioBattle& b) {
    return b.ctx().has_round_effects(1) || b.ctx().rule_center_.has_round_type(1);
}

ElfPet ruiya_pet(const std::vector<int>& skill_ids, int speed = 300) {
    std::array<int, 5> ids{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    for (std::size_t i = 0; i < skill_ids.size() && i < 5; ++i) {
        ids[i] = skill_ids[i];
    }
    ElfPet p = make_pet(1, "慈辉永颂·瑞亚", ids, speed, 300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(const std::vector<int>& skill_ids, int speed = 100) {
    std::array<int, 5> ids{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    for (std::size_t i = 0; i < skill_ids.size() && i < 5; ++i) {
        ids[i] = skill_ids[i];
    }
    ElfPet p = make_pet(2, "对手", ids, speed, 300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 双方各出一个技能（me_idx/foe_idx 为各自技能槽位；载体=0、拍打填充=1）。
void cast(ScenarioBattle& b, int me_idx, int foe_idx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, me_idx);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_idx);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7980a 2433/2434 正例：双侧消除成功 → 双封禁 + 封属行为 + 到期 ═══════
    // 对手技槽：0=剑指风云 1=超速闪身 2=拍打
    {
        ScenarioBattle b(make_party(ruiya_pet({kYuShen})),
                         make_party(foe_pet({kJianZhi, kChaoSu, kPaDa})));
        b.start();
        // R1 对手剑指风云：+2 全六槽（962 无提升翻倍）+ 707 恢复窗 + 843 先制窗
        cast(b, 1, 0);
        CHECK(foe_has_round_state(b), "T7980a R1 setup 对手回合类效果在身");
        // R2 我方予神繁祉（对手凭 843 先手，超速闪身先落地、随即被断）：
        //   2433 断双方（对手 SUCCESS）→ 封属2回合；2434 清六槽（计6）→ 封攻2回合；
        //   2435 零异常不触发
        cast(b, 0, 1);
        CHECK_EQ(read_key(b, k2433BreakAudit), 1, "T7980a 2433 断回合成功位=1");
        CHECK_EQ(read_key(b, k2433RoundsAudit), 2, "T7980a 2433 封属回合数=2（行参消费钉）");
        CHECK_EQ(read_key(b, k2434ClearAudit), 6, "T7980a 2434 清除提升数=6（六槽含命中）");
        CHECK_EQ(read_key(b, k2434RoundsAudit), 2, "T7980a 2434 封攻回合数=2（行参消费钉）");
        CHECK_EQ(read_key(b, k2435DispelAudit), 0, "T7980a 2435 无异常 → 解除数=0");
        CHECK(!foe_has_round_state(b), "T7980a 2433 断后对手回合类效果全清");
        CHECK(seal_ticket_alive(b, 1, SealKind::SEAL_ATTRIBUTE, 2433),
              "T7980a 2433 封属票在身");
        CHECK(seal_ticket_alive(b, 1, SealKind::SEAL_ATTACK, 2434),
              "T7980a 2434 封攻票在身");
        // R3（窗口内）对手超速闪身（属性技）→ 封属拦截 → 不产生 843 新窗
        cast(b, 1, 1);
        CHECK(!foe_has_round_state(b), "T7980a R3 封属行为：超速闪身被拦（无新窗）");
        // R4（到期）对手超速闪身 → 真生效 → 843 新窗出现
        cast(b, 1, 1);
        CHECK(!seal_ticket_alive(b, 1, SealKind::SEAL_ATTRIBUTE, 2433),
              "T7980a R4 封属票到期注销（R2 授 2 回合 = R2/R3）");
        CHECK(!seal_ticket_alive(b, 1, SealKind::SEAL_ATTACK, 2434),
              "T7980a R4 封攻票同步到期注销");
        CHECK(foe_has_round_state(b), "T7980a R4 到期行为：超速闪身生效（843 新窗）");
        // R5 对手拍打 → 封攻已到期 → 我方血量下降（封攻到期行为面）
        const int hp_before = b.ctx().getPet(0).hp;
        cast(b, 1, 2);
        CHECK(b.ctx().getPet(0).hp < hp_before, "T7980a R5 到期行为：拍打落地（血量下降）");
        std::cout << "  [T7980a] 2433/2434 正例+封属行为+到期 通过\n";
    }

    // ══ T7980a2 2434 封攻窗口内行为：拍打被拦（血量不动）════════════════════
    // 对手技槽：0=剑指风云 1=拍打
    {
        ScenarioBattle b(make_party(ruiya_pet({kYuShen})),
                         make_party(foe_pet({kJianZhi, kPaDa})));
        b.start();
        cast(b, 1, 0);   // R1 setup：对手剑指风云
        cast(b, 0, 1);   // R2 予神繁祉（对手凭先手拍打先落地，随即封攻授予）
        CHECK(seal_ticket_alive(b, 1, SealKind::SEAL_ATTACK, 2434),
              "T7980a2 R2 封攻票在身");
        const int hp_after_r2 = b.ctx().getPet(0).hp;
        cast(b, 1, 1);   // R3（窗口内）对手拍打 → 封攻拦截
        CHECK_EQ(b.ctx().getPet(0).hp, hp_after_r2,
                 "T7980a2 R3 封攻行为：拍打被拦（血量不动）");
        cast(b, 1, 1);   // R4（到期）对手拍打 → 落地
        CHECK(b.ctx().getPet(0).hp < hp_after_r2, "T7980a2 R4 到期行为：拍打落地");
        std::cout << "  [T7980a2] 2434 封攻行为+到期 通过\n";
    }

    // ══ T7980b 2433/2434/2435 反例：三无所 → 零成功、零封禁、攻击落地 ════════
    // 对手技槽：0=拍打
    {
        ScenarioBattle b(make_party(ruiya_pet({kYuShen})),
                         make_party(foe_pet({kPaDa})));
        b.start();
        cast(b, 0, 0);   // R1 予神繁祉 vs 拍打：无回合类/无提升/无异常
        CHECK_EQ(read_key(b, k2433BreakAudit), 0, "T7980b 2433 无回合类 → 成功位=0");
        CHECK_EQ(read_key(b, k2434ClearAudit), 0, "T7980b 2434 无提升 → 清除数=0");
        CHECK_EQ(read_key(b, k2435DispelAudit), 0, "T7980b 2435 无异常 → 解除数=0");
        CHECK(!seal_ticket_alive(b, 1, SealKind::SEAL_ATTRIBUTE, 2433),
              "T7980b 未成功 → 无封属票");
        CHECK(!seal_ticket_alive(b, 1, SealKind::SEAL_ATTACK, 2434),
              "T7980b 未成功 → 无封攻票");
        CHECK(b.ctx().getPet(0).hp < kMaxHp, "T7980b 无封攻 → 对手拍打落地");
        std::cout << "  [T7980b] 2433/2434/2435 三无反例 通过\n";
    }

    // ══ T7980c 2435 正例：中毒 → 双侧解除 → 免疫窗 → 到期 ══════════════════
    // 对手技槽：0=剧毒粉 1=拍打
    {
        ScenarioBattle b(make_party(ruiya_pet({kYuShen})),
                         make_party(foe_pet({kJuDu, kPaDa})));
        b.start();
        // R1 对手剧毒粉（1605 [100,1]）→ 我方中毒
        cast(b, 1, 0);
        CHECK(b.ctx().has_active_abnormal_status(0, 1), "T7980c R1 我方中毒在身");
        // R2 予神繁祉：2433/2434 零成功（隔离证据），2435 解除数=1 → 免疫 2 回合
        cast(b, 0, 1);
        CHECK_EQ(read_key(b, k2433BreakAudit), 0, "T7980c 2433 隔离：无回合类 → 0");
        CHECK_EQ(read_key(b, k2434ClearAudit), 0, "T7980c 2434 隔离：无提升 → 0");
        CHECK_EQ(read_key(b, k2435DispelAudit), 1, "T7980c 2435 解除数=1（我方中毒）");
        CHECK_EQ(read_key(b, k2435RoundsAudit), 2, "T7980c 2435 免疫回合数=2（行参消费钉）");
        CHECK(!b.ctx().has_active_abnormal_status(0, 1), "T7980c 解除后我方无异常");
        CHECK(!seal_ticket_alive(b, 1, SealKind::SEAL_ATTRIBUTE, 2433),
              "T7980c 2435 不产封属票（宏间隔离）");
        CHECK(!seal_ticket_alive(b, 1, SealKind::SEAL_ATTACK, 2434),
              "T7980c 2435 不产封攻票（宏间隔离）");
        // R3（窗口内）对手剧毒粉 → Modern 层免疫挡下
        cast(b, 1, 0);
        CHECK(!b.ctx().has_active_abnormal_status(0, 1), "T7980c R3 免疫行为：中毒未落地");
        // R4（到期）对手剧毒粉 → 中毒落地
        cast(b, 1, 0);
        CHECK(b.ctx().has_active_abnormal_status(0, 1), "T7980c R4 到期行为：中毒落地");
        std::cout << "  [T7980c] 2435 解除+免疫+到期 通过\n";
    }

    // ══ T7980d 2437 正例：对手有提升 → 穿透写入 + 成功断对手回合 ════════════
    // 对手技槽：0=剑指风云 1=拍打
    {
        ScenarioBattle b(make_party(ruiya_pet({kYuTian})),
                         make_party(foe_pet({kJianZhi, kPaDa})));
        b.start();
        cast(b, 1, 0);   // R1 对手剑指风云（+2 六槽 + 回合类双窗）
        CHECK(foe_has_round_state(b), "T7980d R1 setup 对手回合类效果在身");
        cast(b, 0, 1);   // R2 予天繁星：穿透 + 无视成功 → 断对手回合
        CHECK_EQ(read_key(b, k2437PierceAudit), 1, "T7980d 2437 穿透旗标写入位=1");
        CHECK_EQ(read_key(b, k2437SuccessAudit), 1, "T7980d 2437 无视成功位=1（view 六槽 +2）");
        CHECK(!foe_has_round_state(b), "T7980d 成功分支：对手回合类效果被断清");
        std::cout << "  [T7980d] 2437 正例 通过\n";
    }

    // ══ T7980e 2437 反例：对手无提升（仅有回合类）→ 不断回合 ════════════════
    // 对手技槽：0=超速闪身 1=拍打
    {
        ScenarioBattle b(make_party(ruiya_pet({kYuTian})),
                         make_party(foe_pet({kChaoSu, kPaDa})));
        b.start();
        cast(b, 1, 0);   // R1 对手超速闪身（只有 843 先制窗，无强化）
        CHECK(foe_has_round_state(b), "T7980e R1 setup 对手回合类效果在身");
        cast(b, 0, 1);   // R2 予天繁星：无提升可无视 → 成功位=0 → 不断
        CHECK_EQ(read_key(b, k2437PierceAudit), 1, "T7980e 2437 穿透旗标恒写入=1");
        CHECK_EQ(read_key(b, k2437SuccessAudit), 0, "T7980e 2437 无提升 → 无视成功位=0");
        CHECK(foe_has_round_state(b), "T7980e 失败分支：对手回合类效果保留（未断）");
        std::cout << "  [T7980e] 2437 反例 通过\n";
    }

    std::cout << "scenario_7980 全部通过" << std::endl;
    return 0;
}
