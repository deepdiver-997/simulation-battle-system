// 场景 3730 —— K229 血量对照减伤+随机吸取族（1422，soul_lib/k229_hpcmp_mitigate_drain.cpp）
//（2026-10-08，夜跑 k-prod-6；号段 3730-3739 → T3730 一块六例）
//
// 断言口径（K226 3700 同款：F9 审计槽无随机断言 + 比例带侧证双保险）：
//   - a/b/c 血量对照减伤（审计槽精确选档 + 红伤比例带）：高于→30（比例带 (0.55,0.84)）/
//     低于→40（(0.45,0.72)）/ 相等→0（(0.83,1.25)）。比例带按 85%~100% 浮动最坏界
//     计算（treated/control 跨对局比值）：30% 档真值域 [0.595,0.824]、40% 档 [0.51,0.706]、
//     无减伤 [0.85,1.176]。
//     ⚠️ 承伤重构口径：ROUND_END 吸取会给印主回血污染净差值——真实承伤 =
//     净差值 + 审计槽实吸（纯算术；HP 拉大避开回复封顶）。pet.cover 字段直写**不是**
//     粉伤吸收通道（护罩走管线桶注册，直改字段无效——本场景实测教训，勿再踩）。
//   - d 吸取+阈值门正例（row1 真实行参，对手 HP=149<阈值150）：实吸=min(rand,149)
//     ≤149 恒<150 → 门必触发恰 3 槽 -1；roll≥149 的吸死分支照常落地（stat_drop 无
//     存活守卫，FSM 短路不回卷本桶）；回复等式=实吸。
//   - e row2 全真实行参端到端（对手满血 9 万必存活）：实吸 ∈[100,300]
//     （db[2]=100 下限、db[3]=300 上限消费证据）；回复等式；实吸 <200 → 恰 3 槽 -1，
//     否则 0 槽（门两分支一致性）。
//   - f 账本每回合重估（回合开始快照口径）：R1 高于→30，翻血量后 R2→40。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：filler
constexpr int kLedgerBase = 991421000;   // +owner：减伤%账本（实现文件同锚）
constexpr int kDrainBase = 991422000;    // +owner：实吸量审计槽

SoulMark make_mark(const std::vector<int>& row_args) {
    SoulMark sm(1422, "K229 魂印", "test", EffectArgs(row_args));
    sm.monster_id = 3833;   // 魂王·暗翼影
    return sm;
}

// 印主：魂印宠（hp 同时是 max），快攻先手；拍打无威胁。
ElfPet mark_pet(const std::vector<int>& row_args, int hp) {
    return make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                    /*speed=*/300, /*atk=*/300, hp, make_mark(row_args));
}

// 无魂印对照宠（同面板）。
ElfPet plain_pet(int hp) {
    return make_pet(1, "对照", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                    /*speed=*/300, /*atk=*/300, hp);
}

// 对手：攻击手（atk 可调）。
ElfPet foe_pet(int atk, int hp) {
    return make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                    /*speed=*/100, atk, hp);
}

int audit_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

void round2(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
            BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
}

int dropped_slots(BattleContext& ctx) {
    int neg = 0;
    for (int i = 0; i < 6; ++i) {
        const int lv = ctx.ability_levels[1][i];
        CHECK(lv == 0 || lv == -1, "T3730 对手等级槽只能是 0/-1");
        if (lv == -1) {
            ++neg;
        }
    }
    return neg;
}

// 减伤比例测量：treated（有印）/ control（无印）同配置各跑一回合对手攻击。
// 返回承伤比值；treated 侧用审计槽把吸取回血从净差值里加回去（真实承伤）。
double mitigate_ratio(const std::vector<int>& row, int holder_hp, int foe_hp, int foe_atk) {
    const int dmg_t = [&] {
        ScenarioBattle b(make_party(mark_pet(row, holder_hp)), make_party(foe_pet(foe_atk, foe_hp)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(0).hp;
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int net = hp0 - ctx.getPet(0).hp;
        const int drained = audit_int(ctx, kDrainBase + 0);
        CHECK(drained >= 0, "T3730 实吸审计槽必须在（含 0）");
        return net + drained;   // 重构：净承伤 + 回血 = 真实承伤
    }();
    const int dmg_c = [&] {
        ScenarioBattle b(make_party(plain_pet(holder_hp)), make_party(foe_pet(foe_atk, foe_hp)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(0).hp;
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        return hp0 - ctx.getPet(0).hp;
    }();
    CHECK(dmg_c > 0, "T3730 对照承伤必须为正");
    return static_cast<double>(dmg_t) / static_cast<double>(dmg_c);
}

constexpr int kRow1High = 30, kRow1Low = 40;   // 行参语义速查（断言消息用）

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<int> row1{30, 40, 1, 300, 150, 3, 1, 1};
    const std::vector<int> row2{30, 40, 100, 300, 200, 3, 1, 1};

    // ══ T3730.a 体力高于对手 → 减伤 30%（审计槽 30 + 比例带）══
    {
        ScenarioBattle b(make_party(mark_pet(row1, 999990)), make_party(foe_pet(300, 990000)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, kLedgerBase + 0), kRow1High, "T3730.a 高于对手 → 账本 30");
        const double r = mitigate_ratio(row1, 999990, 990000, 3000);
        CHECK(r > 0.55 && r < 0.84, "T3730.a 承伤比例带 (0.55,0.84) 实测");
        std::cout << "  [T3730.a] 高于→30% 通过\n";
    }

    // ══ T3730.b 体力低于对手 → 减伤 40% ══
    {
        ScenarioBattle b(make_party(mark_pet(row1, 50000)), make_party(foe_pet(300, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, kLedgerBase + 0), kRow1Low, "T3730.b 低于对手 → 账本 40");
        const double r = mitigate_ratio(row1, 50000, 99999, 3000);
        CHECK(r > 0.45 && r < 0.72, "T3730.b 承伤比例带 (0.45,0.72) 实测");
        std::cout << "  [T3730.b] 低于→40% 通过\n";
    }

    // ══ T3730.c 体力相等 → 两门都不满足，无减伤 ══
    {
        ScenarioBattle b(make_party(mark_pet(row1, 999990)), make_party(foe_pet(300, 999990)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, kLedgerBase + 0), 0, "T3730.c 相等 → 账本 0");
        const double r = mitigate_ratio(row1, 999990, 999990, 3000);
        CHECK(r > 0.83 && r < 1.25, "T3730.c 无减伤比例带 (0.83,1.25) 实测");
        std::cout << "  [T3730.c] 相等→不减伤 通过\n";
    }

    // ══ T3730.d 吸取+阈值门正例（row1 真实行参）：对手 HP=149<阈值150 →
    //     实吸=min(rand,149) 恒<150 门必触发恰 3 槽 -1；roll≥149 的吸死分支
    //     照常落地（stat_drop 无存活守卫）══
    {
        ScenarioBattle b(make_party(mark_pet(row1, 99000)), make_party(foe_pet(1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[1].elfPets[0].hp = 149;
        ctx.seerRobot[0].elfPets[0].hp = 98500;   // 压到上限下（满血回复会封顶，等式断言需要余量）
        const int hp0 = ctx.getPet(0).hp;
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        const int drained = audit_int(ctx, kDrainBase + 0);
        CHECK(drained >= 1 && drained <= 149, "T3730.d 实吸(HP差真值) ∈[1,149]");
        CHECK_EQ(ctx.getPet(0).hp - hp0, drained, "T3730.d 回复等式 = 实吸");
        CHECK_EQ(dropped_slots(ctx), 3, "T3730.d 实吸<150 → 恰 3 槽 -1");
        std::cout << "  [T3730.d] 吸取+降属门正例 通过（实吸=" << drained << "）\n";
    }

    // ══ T3730.e row2 全真实行参端到端（对手满血必存活）══
    {
        ScenarioBattle b(make_party(mark_pet(row2, 99000)), make_party(foe_pet(1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 98500;   // 压到上限下（回复封顶防护，同 T.d）
        const int hp0 = ctx.getPet(0).hp;
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        const int drained = audit_int(ctx, kDrainBase + 0);
        CHECK(drained >= 100 && drained <= 300, "T3730.e row2 实吸 ∈[100,300]（db[2]/db[3] 消费）");
        CHECK_EQ(ctx.getPet(0).hp - hp0, drained, "T3730.e 回复等式 = 实吸");
        if (drained < 200) {
            CHECK_EQ(dropped_slots(ctx), 3, "T3730.e 实吸<200 → 恰 3 槽 -1");
        } else {
            CHECK_EQ(dropped_slots(ctx), 0, "T3730.e 实吸≥200 → 不降属");
        }
        std::cout << "  [T3730.e] row2 下限+门一致性 通过（实吸=" << drained << "）\n";
    }

    // ══ T3730.f 账本每回合重估（回合开始快照口径）══
    {
        ScenarioBattle b(make_party(mark_pet(row1, 999990)), make_party(foe_pet(1, 990000)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(audit_int(ctx, kLedgerBase + 0), kRow1High, "T3730.f R1 高于 → 30");
        ctx.seerRobot[0].elfPets[0].hp = 100;   // 翻血量（回合间直铺）
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(audit_int(ctx, kLedgerBase + 0), kRow1Low, "T3730.f R2 重估 → 40");
        std::cout << "  [T3730.f] 账本每回合重估 通过\n";
    }

    // ══ T3730.g 吸死分支（对手 HP=1：实吸恒=1=击杀）：死亡不短路本桶——
    //     回复/降属照常落地（stat_drop 无存活守卫；此分支每次运行必走，防 rand 序漂移）══
    {
        ScenarioBattle b(make_party(mark_pet(row1, 99000)), make_party(foe_pet(1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[1].elfPets[0].hp = 1;
        ctx.seerRobot[0].elfPets[0].hp = 98500;
        const int hp0 = ctx.getPet(0).hp;
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        const int drained = audit_int(ctx, kDrainBase + 0);
        CHECK_EQ(drained, 1, "T3730.g 对手仅 1 血 → 实吸恒 1（击杀）");
        CHECK_EQ(ctx.getPet(0).hp - hp0, drained, "T3730.g 击杀分支回复等式");
        CHECK_EQ(dropped_slots(ctx), 3, "T3730.g 击杀分支降属照常落地");
        std::cout << "  [T3730.g] 吸死分支 通过\n";
    }

    std::cout << "scenario_3730 PASS (K229 血量对照减伤+随机吸取族)" << std::endl;
    return 0;
}
