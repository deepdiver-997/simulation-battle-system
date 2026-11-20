#include <utils/sbs_rand.h>
// 场景 7110 —— K366 概率异常+低伤免疫阈值族（1210，soul_lib/k_anomaly_threshold_1210.cpp）
//（2026-10-24，worktree ../agents/k366；号段 7110-7119 → T7110-T7116 七块）
//
// 断言口径（真实攻击走伤害管线；85%~100% 浮动 → 比例带；概率走审计槽精确计数）：
//   - T7110 row924 ① 40% 害怕：审计槽（990366800+owner*2）读申报概率/ApplyAnomalyResult，
//     20 回合拍打计"落地次数"∈ [2,14]（p=0.4/n=20 → 期望 8；上界拦 100% 误档、
//     下界拦 0%/4% 误档）；
//   - T7111 row924 ② 低伤免疫：atk=1 拍打（伤 1~2 < 300）→ 印主 hp 不动 + 对手六槽全 0
//     （低伤不走分叉）；
//   - T7112 row924 ③ 高伤分叉+减免旗：刀1 loss>300 不免、对手六槽全-1、旗置位；重置
//     对手等级后刀2 = fresh×0.6（减免消费）→ ratio ∈ (0.48, 0.75)（无减免世界
//     [0.85,1.18] 不相交）、六槽再次全-1（分叉重触发）、旗再置位；
//   - T7113 row1476 伙伴行 50/50 同构互锚：同 T7112、减免带 (0.40, 0.62)；
//   - T7114 row1812 ① 100% 烧伤：一次拍打 → 对手必带烧伤（短路档，确定）；
//   - T7115 row1812 降攻门+免疫段关：对手 atk=1 拍打 → 印主掉血 1~2（db[2]=0 免疫段关）
//     + 对手六槽全-1（攻击技触发）；下回合对手用缩头（属性技）→ 六槽 = [-1,0,-1,-1,-1,-1]
//     （属性技不触发降攻门；缩头行参 arg[0]=1 直映引擎槽 1，实测定档）；
//   - T7116 row1812 减免段关：两刀高伤（刀间重置等级）ratio ∈ [0.83,1.20] 无减免、
//     六槽 -1→-1 仅由降攻门驱动。
//   ⚠️ 刀间重置对手等级（直写 ability_levels，回合初视图重基）消除六槽-1 的伤害
//   公式耦合——比例带只测减免，不混等级乘数。伤害损失一律取当刀差值。
#include <any>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;    // 拍打：物理攻击 filler（PP35/acc100/无附带）
constexpr int kSuoTou = 20001;   // 缩头：属性技 filler（自身防+1，效果4）
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);    // 6
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);    // 2
constexpr int kMaxHp = 800000;
constexpr int kSlots = BattleContext::kAbilityLevelSlotCount;      // 6
constexpr int kReduceFlagKey = 990366500;   // 减免旗（+owner，见实现文件）

SoulMark make_mark(const std::vector<int>& db_args, int monster_id) {
    SoulMark sm(/*effect_id=*/1210, "K1210 魂印", "test",
                EffectArgs(std::vector<int>(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
}

ElfPet mark_pet(const std::vector<int>& db_args, int monster_id) {
    ElfPet p = make_pet(1, "印主", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp,
                        make_mark(db_args, monster_id));
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(int atk) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, kMaxHp);
    accuracy_100(p);
    return p;
}

ElfPet foe_pet_attr(int atk) {   // 对手格 5 带缩头（属性技负控用）
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kSuoTou},
                        /*speed=*/100, atk, kMaxHp);
    accuracy_100(p);
    return p;
}

// 印主 NONE 先手、对手拍打后手打印主；返回对手当刀伤害（差值，非累计）
int foe_strike(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int hp_before = ctx.getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp_before - ctx.getPet(0).hp;
}

void reset_foe_levels(BattleContext& ctx) {   // 消除六槽-1 的伤害公式耦合
    for (int i = 0; i < kSlots; ++i) {
        ctx.ability_levels[1][i] = 0;
    }
}

// 对手六槽等级是否全等于 expect
bool all_levels_eq(BattleContext& ctx, int side, int expect) {
    for (int i = 0; i < kSlots; ++i) {
        if (ctx.ability_levels[side][i] != expect) {
            return false;
        }
    }
    return true;
}

std::string levels_str(BattleContext& ctx, int side) {
    std::string s = "[";
    for (int i = 0; i < kSlots; ++i) {
        s += std::to_string(ctx.ability_levels[side][i]);
        s += (i + 1 < kSlots) ? ',' : ']';
    }
    return s;
}

// 审计槽读 int（插件侧写入为 int；065 同款 any_cast）
int audit_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

// 审计槽判"本次异常申报是否落地"（免疫/弹控等非 ROLL_FAILED 档在干净场景不出现）
bool anomaly_landed(BattleContext& ctx, int owner) {
    const int res = audit_int(ctx, 990366801 + owner * 2);
    return res == static_cast<int>(ApplyAnomalyResult::SUCCESS)
        || res == static_cast<int>(ApplyAnomalyResult::DURATION_EXTENDED)
        || res == static_cast<int>(ApplyAnomalyResult::REPLACED_EXISTING);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7110 row924 [40,6,300,1,1,40,1] ①：20 回合 40% 害怕，审计槽精确计数 ══
    {
        sbs_srand(20261024);
        ScenarioBattle b(make_party(mark_pet({40, 6, 300, 1, 1, 40, 1}, 3632)),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        int landed = 0;
        for (int i = 0; i < 20 && ctx.getPet(1).hp > 0; ++i) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            if (anomaly_landed(ctx, 0)) {
                ++landed;
            }
        }
        CHECK(audit_int(ctx, 990366800) == 40,
              "T7110 审计槽申报概率应=40（实测 " << audit_int(ctx, 990366800)
                  << "）");
        CHECK(landed >= 2 && landed <= 14,
              "T7110 row924 ①：20 回合 40% 害怕落地次数 ∈ [2,14]（期望 8，实测 "
                  + std::to_string(landed) + "）");
        std::cout << "  [T7110] 1210 row924 40% 害怕（审计槽计数 " << landed
                  << "/20）通过\n";
    }

    // ══ T7111 row924 ②：低伤免疫（atk=1 → 伤 1~2 < 300）═════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({40, 6, 300, 1, 1, 40, 1}, 3632)),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();
        const int loss = foe_strike(b);
        CHECK_EQ(loss, 0, "T7111 低伤被免疫（印主 hp 不动）");
        CHECK(all_levels_eq(b.ctx(), 1, 0),
              "T7111 低伤不触发六槽-1（实测 " + levels_str(b.ctx(), 1) + "）");
        std::cout << "  [T7111] 1210 row924 低伤免疫 通过\n";
    }

    // ══ T7112 row924 ③：高伤分叉（六槽-1）+ 下次受伤 -40% 旗 ═════════════════
    {
        ScenarioBattle b(make_party(mark_pet({40, 6, 300, 1, 1, 40, 1}, 3632)),
                         make_party(foe_pet(/*atk=*/5000)));
        b.start();
        const int s1 = foe_strike(b);
        CHECK(s1 > 300 && s1 < 2000,
              "T7112 刀1 高伤落地前提不成立：s1=" << s1);
        CHECK(all_levels_eq(b.ctx(), 1, -1),
              "T7112 刀1 高伤→对手六槽全-1（实测 " + levels_str(b.ctx(), 1)
                  + "）");
        CHECK(b.ctx().plugin_storage.count(kReduceFlagKey) == 1,
              "T7112 刀1 高伤应置减免旗");
        reset_foe_levels(b.ctx());
        const int s2 = foe_strike(b);
        const double ratio = static_cast<double>(s2) / static_cast<double>(s1);
        CHECK(ratio > 0.48 && ratio < 0.75,
              "T7112 刀2 应为 fresh×0.6（减免旗消费）：s1=" << s1 << " s2=" << s2
                  << " ratio=" << ratio);
        CHECK(all_levels_eq(b.ctx(), 1, -1),
              "T7112 刀2 分叉重触发→六槽（重置后）再全-1（实测 "
                  + levels_str(b.ctx(), 1) + "）");
        CHECK(b.ctx().plugin_storage.count(kReduceFlagKey) == 1,
              "T7112 刀2 高伤再次置旗（下1次语义可持续）");
        std::cout << "  [T7112] 1210 row924 高伤分叉+减免40%（s1=" << s1
                  << " s2=" << s2 << "）通过\n";
    }

    // ══ T7113 row1476 [50,6,300,1,1,50,1] 伙伴行 50/50 同构互锚 ══════════════
    {
        ScenarioBattle b(make_party(mark_pet({50, 6, 300, 1, 1, 50, 1}, 3632)),
                         make_party(foe_pet(/*atk=*/5000)));
        b.start();
        const int s1 = foe_strike(b);
        CHECK(s1 > 300, "T7113 刀1 高伤落地前提不成立：s1=" << s1);
        CHECK(all_levels_eq(b.ctx(), 1, -1),
              "T7113 刀1 六槽全-1（实测 " + levels_str(b.ctx(), 1) + "）");
        reset_foe_levels(b.ctx());
        const int s2 = foe_strike(b);
        const double ratio = static_cast<double>(s2) / static_cast<double>(s1);
        CHECK(ratio > 0.40 && ratio < 0.62,
              "T7113 刀2 应为 fresh×0.5：s1=" << s1 << " s2=" << s2
                  << " ratio=" << ratio);
        CHECK(all_levels_eq(b.ctx(), 1, -1),
              "T7113 刀2 分叉重触发（实测 " + levels_str(b.ctx(), 1) + "）");
        std::cout << "  [T7113] 1210 row1476 伙伴行 50 档（ratio=" << ratio
                  << "）通过\n";
    }

    // ══ T7114 row1812 [100,2,0,1,0,0,1] ①：100% 烧伤（确定档）═══════════════
    {
        ScenarioBattle b(make_party(mark_pet({100, 2, 0, 1, 0, 0, 1}, 4607)),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(b.ctx().has_active_abnormal_status(1, kBurn),
              "T7114 row1812 100% 烧伤应必落地");
        std::cout << "  [T7114] 1210 row1812 100% 烧伤 通过\n";
    }

    // ══ T7115 row1812 降攻门+免疫段关：攻击技触发 / 属性技不触发 ═════════════
    {
        ScenarioBattle b(make_party(mark_pet({100, 2, 0, 1, 0, 0, 1}, 4607)),
                         make_party(foe_pet_attr(/*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int loss1 = foe_strike(b);
        CHECK(loss1 >= 1 && loss1 <= 2,
              "T7115 db[2]=0 免疫段关：atk=1 拍打应正常掉血 1~2（实测 " << loss1
                  << "）");
        CHECK(all_levels_eq(ctx, 1, -1),
              "T7115 对手攻击技→六槽全-1（实测 " + levels_str(ctx, 1) + "）");
        // 对手改用属性技（缩头，格 4）：自身防+1，但不触发降攻门
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 4);
        b.drive();
        bool attr_shape = true;
        for (int i = 0; i < kSlots; ++i) {
            // 缩头行参 arg[0]=1 直映引擎槽 1（实测定档）：+1 叠在 -1 基线上 → 槽1=0
            const int expect = (i == 1) ? 0 : -1;
            if (ctx.ability_levels[1][i] != expect) {
                attr_shape = false;
            }
        }
        CHECK(attr_shape, "T7115 属性技不触发降攻门（实测 " + levels_str(ctx, 1)
                  + "，应=[-1,0,-1,-1,-1,-1]）");
        CHECK(hp_before - ctx.getPet(0).hp == 0,
              "T7115 属性技当回合印主无伤");
        std::cout << "  [T7115] 1210 row1812 降攻门+段关 通过\n";
    }

    // ══ T7116 row1812 减免段关：高伤两刀无免无减（ratio≈1）══════════════════
    {
        ScenarioBattle b(make_party(mark_pet({100, 2, 0, 1, 0, 0, 1}, 4607)),
                         make_party(foe_pet(/*atk=*/5000)));
        b.start();
        const int s1 = foe_strike(b);
        CHECK(s1 > 300, "T7116 刀1 高伤落地前提不成立：s1=" << s1);
        CHECK(all_levels_eq(b.ctx(), 1, -1),
              "T7116 刀1 降攻门六槽全-1（实测 " + levels_str(b.ctx(), 1) + "）");
        CHECK(b.ctx().plugin_storage.count(kReduceFlagKey) == 0,
              "T7116 db[4]=0 减免段关：不应置旗");
        reset_foe_levels(b.ctx());
        const int s2 = foe_strike(b);
        const double ratio = static_cast<double>(s2) / static_cast<double>(s1);
        CHECK(ratio >= 0.83 && ratio <= 1.20,
              "T7116 db[4]=0 减免段关：刀2 应无减免（ratio≈1）：s1=" << s1
                  << " s2=" << s2 << " ratio=" << ratio);
        CHECK(all_levels_eq(b.ctx(), 1, -1),
              "T7116 刀2 降攻门六槽再全-1（实测 " + levels_str(b.ctx(), 1)
                  + "）");
        std::cout << "  [T7116] 1210 row1812 减免段关（ratio=" << ratio
                  << "）通过\n";
    }

    std::cout << "scenario_7110 PASS (K366 概率异常+低伤免疫阈值族)" << std::endl;
    return 0;
}
