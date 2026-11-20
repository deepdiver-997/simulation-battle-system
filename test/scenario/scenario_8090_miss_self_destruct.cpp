// 场景 8090 —— J335 case 72 miss 自灭闭合：72「若没有命中对手则自己失去全部体力」
//   （号段 8090-8099；2026-11-07，worktree ../j335）
//   （内核配套 = src/entities/skills.cpp default_branch_for_effect case 72 →
//   SKILL_INVALID 一行；插件 = moves_lib/j335_miss_self_destruct.cpp——两件套须同发：
//   缺内核行时 72 落 HIT 分支 → miss 趟不注册 → T8090b 转红；缺插件注册时
//   Skills(36506) 构造对 72 走 parser 静默跳过（合法态）→ T8090b 转红。）
//
// 断言口径（8060 T8060f fail-loud 探针的翻正 + 缺口闭合验收；K423 记档续单）：
//   - miss 构造 = 失明（异常 20）+ 载体固有 must_hit=0（36506 无为·魂切 命中50/
//     must_hit=0）→ query_usage 失明分支必定 miss（零掷骰确定性，8060 同款）。
//   - miss 趟：72 无条件自灭（SKILL_INVALID 单桶，分支即条件——900001 同口径）；
//     hp<=0 生存门在先、hp 归零在后、defeat_pet(HP_CONSUME) 死亡漏斗收尾（59/2213/
//     1276 自爆族既定形态；HP_CONSUME 不响应免疫秒杀、穿残留免死）。
//     断言 = 自身槽位 hp==0 + 审计槽（1007200001 执行位=1 / 1007200002 DefeatResult
//     序值==DEFEATED）+ 对手 HP 不动 + 对手无异常（36 秒杀掷骰不跑——miss 趟 HIT
//     分支未注册）+ FSM 进入补位等待（换宠状态，死亡已登记的行为面）。
//   - 命中趟：36506 must_hit 强制开 → 必中 → 36 切片 [100] 秒杀掷骰 100% 档短路
//     （零 rand）→ 对手 hp==0；72 命中趟不注册（HIT/SKILL_INVALID 单桶互斥）→
//     自身 HP 不动、审计槽双键绝对缺席。
//   - 分支结构：Skills(36506) 的 36 在 HIT 分支、72 在 SKILL_INVALID 分支（内核
//     case 72 的直接结构面）。
//   - 场景纪律：无 while(!FINISHED)、无概率断言；seed 固定隔离（失明预置 apply_
//     anomaly 零 rand、36 秒杀 100% 短路零 rand、miss 分支零掷骰——全链确定性）。
#include <any>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <effects/spirit_lifecycle.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打（对手填充，强制必中）
constexpr int kWuWei  = 36506;   // 无为·魂切 [36,72][100]（72 载体，must_hit=0 固有）
constexpr int kMaxHp  = 100000;

// J335 审计槽（与 j335_miss_self_destruct.cpp constexpr 逐键一致）。
constexpr int k72ExecAudit   = 1007200001;   // 1=hp 已归零走漏斗（缺席=未执行）
constexpr int k72ResultAudit = 1007200002;   // DefeatResult 序值

int read_key(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -999;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -999;
}

// 72 载体宠：force_hit=false 保留 36506 固有 must_hit=0/命中 50——失明时确定性
// miss 专用；命中趟用例对单个技能显式开必中。
ElfPet wuwei_pet(int speed = 300, bool force_hit = false) {
    ElfPet p = make_pet(1, "普鲁修托", {kWuWei, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/speed, /*atk=*/300, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        if (force_hit) {
            s.must_hit = 1;
            s.accuracy = 100;
        }
        s.critical_strike_rate = 0.0f;
    }
    return p;
}

ElfPet foe_pet(int speed = 100) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/speed, /*atk=*/300, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
        s.critical_strike_rate = 0.0f;
    }
    return p;
}

void cast(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int count_branch_nodes(const Skills& s, int eid) {
    int n = 0;
    for (const auto& [result, nodes] : s.effectBranches) {
        (void)result;
        for (const SkillEffectNode& node : nodes) {
            if (node.effect.id == eid) {
                ++n;
            }
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8090a 分支结构：36 → HIT 分支、72 → SKILL_INVALID 分支（内核 case 72 面）═
    {
        const Skills s(kWuWei);
        CHECK_EQ(count_branch_nodes(s, 36), 1,
                 "T8090a 36（命中趟秒杀掷骰）在 HIT 分支恰 1 个");
        CHECK_EQ(count_branch_nodes(s, 72), 1,
                 "T8090a 72（miss 趟自灭）在 SKILL_INVALID 分支恰 1 个"
                 "（缺内核 case 72 时落 HIT 分支 → 本断言转红）");
        int hit_72 = 0, invalid_36 = 0;
        for (const auto& [result, nodes] : s.effectBranches) {
            if (result == SkillExecResult::HIT) {
                for (const SkillEffectNode& node : nodes) {
                    if (node.effect.id == 72) {
                        ++hit_72;
                    }
                }
            }
            if (result == SkillExecResult::SKILL_INVALID) {
                for (const SkillEffectNode& node : nodes) {
                    if (node.effect.id == 36) {
                        ++invalid_36;
                    }
                }
            }
        }
        CHECK_EQ(hit_72, 0, "T8090a 72 不在 HIT 分支（单桶互斥）");
        CHECK_EQ(invalid_36, 0, "T8090a 36 不在 SKILL_INVALID 分支（单桶互斥）");
        std::cout << "  [T8090a] 36→HIT / 72→SKILL_INVALID 分支结构断言 通过\n";
    }

    // ══ T8090b miss 趟自灭：失明 + 固有非必中 → 确定性 miss → 失去全部体力 ═══
    {
        sbs_srand(80901);
        ScenarioBattle b(make_party(wuwei_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 失明（异常 20）：非必中技能必定 miss（query_usage 失明分支，零掷骰）
        const ApplyAnomalyResult blind = apply_anomaly(
            &ctx, /*target=*/0, static_cast<int>(AbnormalStatusId::Blind),
            /*duration_rounds=*/3, /*actor=*/1);
        CHECK(blind == ApplyAnomalyResult::SUCCESS
                  || blind == ApplyAnomalyResult::REPLACED_EXISTING
                  || blind == ApplyAnomalyResult::DURATION_EXTENDED,
              "T8090b 失明预置落地");
        cast(b, 0);   // R1 无为·魂切：必 miss → 72 自灭
        CHECK_EQ(read_key(ctx, k72ExecAudit + 0), 1,
                 "T8090b miss 趟 72 执行位=1（hp 已归零走漏斗）");
        CHECK_EQ(read_key(ctx, k72ResultAudit + 0),
                 static_cast<int>(DefeatResult::DEFEATED),
                 "T8090b 死亡漏斗登记 DEFEATED（HP_CONSUME 自爆族口径）");
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0,
                 "T8090b 失去全部体力：倒下者槽位 hp==0（官方语义「自己倒下」）");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp, "T8090b 对手 HP 不动（miss 无伤害）");
        CHECK(!ctx.has_active_abnormal_status(1, 0),
              "T8090b 对手无异常（36 秒杀掷骰不跑——miss 趟 HIT 分支未注册）");
        // 死亡已登记的行为面：提交 CHOOSE_PET 完成补位（6100 自爆族同款流程）。
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 1, "T8090b 补位完成：下一只出战宠登场");
        CHECK(ctx.getPet(0).hp > 0, "T8090b 补位宠存活（hp>0）");
        std::cout << "  [T8090b] miss 趟自灭（失明+非必中确定性构造） 通过\n";
    }

    // ══ T8090c 命中趟不触发：必中开 → 36 秒杀落地（100% 短路零 rand）、72 不跑 ═
    {
        sbs_srand(80902);
        ScenarioBattle b(make_party(wuwei_pet(300, /*force_hit=*/true)),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        cast(b, 0);   // R1 无为·魂切：必中 → HIT 分支（36 秒杀），72 不注册
        CHECK_EQ(ctx.getPet(1).hp, 0,
                 "T8090c 命中趟 36 切片 [100] 秒杀掷骰 100% 档短路 → 对手 hp==0");
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                 "T8090c 命中趟 72 不注册（「没有命中」恒假）→ 自身 HP 不动");
        CHECK_EQ(read_key(ctx, k72ExecAudit + 0), -999,
                 "T8090c 72 执行位绝对缺席（审计槽双键零写入）");
        CHECK_EQ(read_key(ctx, k72ResultAudit + 0), -999,
                 "T8090c 漏斗结果键绝对缺席");
        std::cout << "  [T8090c] 命中趟不触发（36 秒杀 / 72 缺席） 通过\n";
    }

    // ══ T8090d 侧别键对照：对手侧 72 载体 miss 自灭 → 审计写 +1 键 ═══════════
    // （镜像对局：对手普鲁修托造型失明，我方速度高先手拍打；对手后手 miss 自灭。）
    {
        sbs_srand(80903);
        ScenarioBattle b(make_party(foe_pet(/*speed=*/400)),
                         make_party(wuwei_pet(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        const ApplyAnomalyResult blind = apply_anomaly(
            &ctx, /*target=*/1, static_cast<int>(AbnormalStatusId::Blind),
            /*duration_rounds=*/3, /*actor=*/0);
        CHECK(blind == ApplyAnomalyResult::SUCCESS
                  || blind == ApplyAnomalyResult::REPLACED_EXISTING
                  || blind == ApplyAnomalyResult::DURATION_EXTENDED,
              "T8090d 失明预置落地（对手侧）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // 我方先手拍打（命中），对手后手魂切 miss → 自灭
        CHECK_EQ(read_key(ctx, k72ExecAudit + 1), 1,
                 "T8090d 对手侧 miss 自灭 → 审计写 +1 侧别键");
        CHECK_EQ(read_key(ctx, k72ExecAudit + 0), -999,
                 "T8090d 我方侧键未被覆盖（侧别键隔离）");
        CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 0,
                 "T8090d 对手（72 载体方）倒下者槽位 hp==0");
        CHECK(ctx.getPet(0).hp > 0, "T8090d 我方存活（未受 72 影响）");
        std::cout << "  [T8090d] 侧别审计键隔离（对手侧自灭） 通过\n";
    }

    std::cout << "\n场景 8090 通过：case 72 miss 自灭闭合——分支结构（36→HIT/"
                 "72→SKILL_INVALID 单桶互斥）、miss 趟确定性自灭（失明+非必中，"
                 "HP_CONSUME 漏斗 DEFEATED）、命中趟不触发（36 秒杀/72 双键缺席）、"
                 "侧别键隔离，全数命中。\n";
    return 0;
}
