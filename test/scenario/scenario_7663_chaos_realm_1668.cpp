#include <utils/sbs_rand.h>
// 场景 7663 —— K408 提升转嫁族·1668 混沌界神 4075（2 行同文，db[1]/db[2]/db[3] 档位差）
// 「自身使用技能后吸取对手能力提升状态，之后将自身相应的能力提升状态赋予给对手相应
//   的能力下降状态（幅度镜像），若对手免疫则吸取对手最大体力1/3且令对手随机1/2项
//   技能PP值归零；|回合开始时，自身每处于1种能力提升状态则战斗阶段结束时恢复60/70
//   点体力，对手每处于1种能力下降状态则战斗阶段结束时损失60/70点体力（boss无效）」
//   —— soul_lib/k_six_anomaly_transfer.cpp（口径记档见实现头）
//
// 断言口径：
//   - 吸取 = transfer_stat_boosts 原语（battle_primitives.cpp:1672：from 正等级搬 to、
//     from 清零、to 等量累加钳 +6）；镜像赋降 = 逐槽 stat_drop 对称（幅度=吸取后自身
//     该槽等级）；免疫路行为面判（免消除强化挡吸取=有提升而 moved==0 / 免弱挡赋降=
//     stat_drop IMMUNE）→ drain_hp（max/3=33333 精确）+ pp_zero_slot 循环。
//   - 免疫夹具 = grant_immunity(STAT_CLEAR/STAT_DROP, duration=0 永久、ON_STAGE)
//     （K163 票形；transfer 对 STAT_CLEAR 的查询点 battle_primitives 实锚）。
//   - 尾支：回合开始快照（pre-start ability_levels 直写过 start——T7653a 同款）→
//     战阶末 60×状态数 精确（heal/dmg 侧证键 + hp 白盒）；回合戳防跨回合残留。
//   - 属性技触发：瞪眼 20004（无伤害阶段 → hp 裸断言无红伤污染）。
//   - boss 门：尾支两节点不注册（healLast/dmgLast 缺席 + hp 不动）；主支无尾注 →
//     boss 局吸取/镜像/免疫路照常。
//   - raw 红伤带：atk300/def100 拍打 [87,102]（速查锚），断言外扩 [85,104]；自身
//     攻+2 → [174,204]（等级 ×2，T7652a 同式）。
//   - 全部 submit_operation + drive() 有限推进，绝不 while(!FINISHED)。
#include <any>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;    // 拍打（物理攻击，priority=0 纯链载体）
constexpr int kAttr = 20004;    // 瞪眼（属性技载体，无伤害阶段）
constexpr int kMaxHp = 100000;  // 1/3 = 33333 整除干净

// 实现文件账本键（k_six_anomaly_transfer.cpp；own 恒 side 0 → +owner 取 0）。
constexpr int kSteal = 991668010;
constexpr int kMirror = 991668020;
constexpr int kDrain = 991668030;
constexpr int kPpZero = 991668040;
constexpr int kSnapOwn = 991668050;
constexpr int kSnapOpp = 991668060;
constexpr int kHealLast = 991668080;
constexpr int kDmgLast = 991668090;

const std::vector<int> kRowBody{3, 1, 60, 60, 1};     // row1320 本体
const std::vector<int> kRowPartner{3, 2, 70, 70, 1};  // row1544 伙伴（2 项 PP + 70/70）

SoulMark make_mark(int id, const std::vector<int>& args) {
    SoulMark sm(id, "K408", "test", EffectArgs(args));
    sm.monster_id = 4075;
    sm.kind_tags = {5, 11, 12};
    return sm;
}

// 槽位布局：0=拍打(物理) 1=瞪眼(属性) 2=水枪(特殊) 3/4=拍打。
ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "混沌界神", {kPaDa, kAttr, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark(1668, args));
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void own_use(ScenarioBattle& b, int idx = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

bool in_band(int v, int lo, int hi) { return v >= lo && v <= hi; }

int pp_zero_count(ScenarioBattle& b, int side) {
    int n = 0;
    for (const Skills& s : b.pet(side).skills) {
        if (s.pp == 0) {
            ++n;
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

    // ══ T7663a 主链：吸提升（+2/+1 搬自家）+ 镜像赋降（-2/-1 对称）+ 无免疫路 ══
    {
        sbs_srand(76631);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.ctx().ability_levels[1][0] = 2;   // start 前置：对手攻+2（T7653a 同款）
        b.ctx().ability_levels[1][4] = 1;   // 速+1
        b.start();
        const int hp0 = b.pet(1).hp;
        own_use(b);
        CHECK_EQ(b.ctx().ability_levels[0][0], 2, "T7663a 吸取：自身攻+2");
        CHECK_EQ(b.ctx().ability_levels[0][4], 1, "T7663a 吸取：自身速+1");
        CHECK_EQ(b.ctx().ability_levels[1][0], -2, "T7663a 镜像：对手攻-2（幅度对称）");
        CHECK_EQ(b.ctx().ability_levels[1][4], -1, "T7663a 镜像：对手速-1");
        CHECK_EQ(read_key(b, kSteal), 2, "T7663a 吸取计数=2 项");
        CHECK_EQ(read_key(b, kMirror), 2, "T7663a 镜像落地=2 槽");
        CHECK_EQ(read_key(b, kDrain), -1, "T7663a 无免疫路");
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 174, 204),
              "T7663a 输出带（自身攻+2 等级 ×2，实测 " + std::to_string(d) + "）");
        std::cout << "  [T7663a] 吸取+镜像主链 通过 (d=" << d << ")\n";
    }

    // ══ T7663b 免疫路 A：免消除强化挡吸取（有提升而吸不进）→ 吸 33333 + 1 项PP归零 ══
    {
        sbs_srand(76632);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.ctx().ability_levels[1][0] = 2;
        b.start();
        b.pet(0).hp = 50000;   // 留回复空间（吸取等量回）
        b.ctx().grant_immunity(1, ImmunityType::STAT_CLEAR, coverage_all(),
                               /*anomaly_mask=*/0, /*duration_rounds=*/0,
                               /*source_id=*/778, /*soul_immunity=*/false,
                               EffectScope::ON_STAGE, /*counts=*/0,
                               /*source_effect_id=*/-1);
        const int hp0 = b.pet(1).hp;
        own_use(b);
        CHECK_EQ(b.ctx().ability_levels[1][0], 2,
                 "T7663b 免消除强化：对手提升保有（无法被吸取）");
        CHECK_EQ(b.ctx().ability_levels[0][0], 0, "T7663b 自身未获得提升");
        CHECK_EQ(read_key(b, kSteal), -1, "T7663b 吸取计数未写（moved=0）");
        CHECK_EQ(read_key(b, kDrain), 1, "T7663b 免疫路吸取=1");
        CHECK_EQ(read_key(b, kPpZero), 1, "T7663b PP 归零=1 项（本体行 db[1]=1）");
        CHECK_EQ(pp_zero_count(b, 1), 1, "T7663b 对手恰 1 个技能 PP=0");
        CHECK_EQ(b.pet(0).hp, 83333, "T7663b 自身回 33333 精确（50000+33333）");
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 33420, 33435),
              "T7663b 对手失血=红伤[87,102]+33333（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7663b] 免疫路A（免消除强化） 通过 (d=" << d << ")\n";
    }

    // ══ T7663c 免疫路 B：免弱挡镜像赋降 → 吸 33333 + 1 项PP归零 ════════════════
    {
        sbs_srand(76633);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.ctx().ability_levels[0][0] = 2;   // 自身攻+2（镜像源）
        b.start();
        b.pet(0).hp = 50000;
        b.ctx().grant_immunity(1, ImmunityType::STAT_DROP, coverage_all(),
                               0, /*duration_rounds=*/0, /*source_id=*/779,
                               false, EffectScope::ON_STAGE, 0, -1);
        const int hp0 = b.pet(1).hp;
        own_use(b);
        CHECK_EQ(b.ctx().ability_levels[1][0], 0, "T7663c 免弱：赋降被挡（等级不变）");
        CHECK_EQ(b.ctx().ability_levels[0][0], 2, "T7663c 自身提升保留");
        CHECK_EQ(read_key(b, kMirror), -1, "T7663c 镜像落地未写（IMMUNE）");
        CHECK_EQ(read_key(b, kDrain), 1, "T7663c 免疫路吸取=1");
        CHECK_EQ(pp_zero_count(b, 1), 1, "T7663c 对手恰 1 个技能 PP=0");
        // ⚠️ 尾支并结：自身攻+2 在 R1 快照=1 种提升状态 → 战阶末另回 60（主支
        //    33333 + 尾支 60 = 83393；与 T7663b 的差异即尾支有无快照源）。
        CHECK_EQ(read_key(b, kHealLast), 60, "T7663c 尾支并结回复=60（快照源=攻+2）");
        CHECK_EQ(b.pet(0).hp, 83393, "T7663c 自身回 33333+尾支 60 精确");
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 33507, 33537),
              "T7663c 对手失血=红伤[174,204]+33333（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7663c] 免疫路B（免弱） 通过 (d=" << d << ")\n";
    }

    // ══ T7663d 伙伴行档位差：随机 2 项 PP 归零（db[1]=2；70/70 位本例不涉尾支）══
    {
        sbs_srand(76634);
        ScenarioBattle b(make_party(mark_pet(kRowPartner)), make_party(foe_pet()));
        b.ctx().ability_levels[1][0] = 2;
        b.start();
        b.pet(0).hp = 50000;
        b.ctx().grant_immunity(1, ImmunityType::STAT_CLEAR, coverage_all(),
                               0, 0, 780, false, EffectScope::ON_STAGE, 0, -1);
        own_use(b);
        CHECK_EQ(read_key(b, kPpZero), 2, "T7663d 伙伴行 PP 归零=2 项（档位差）");
        CHECK_EQ(pp_zero_count(b, 1), 2, "T7663d 对手恰 2 个技能 PP=0");
        CHECK_EQ(read_key(b, kDrain), 1, "T7663d 免疫路吸取（分母 db[0]=3 恒）");
        CHECK_EQ(b.pet(0).hp, 83333, "T7663d 吸取量 1/3 恒（33333 精确）");
        std::cout << "  [T7663d] 伙伴行 2 项 PP 归零 通过\n";
    }

    // ══ T7663e 尾支：回合开始快照 → 战阶末 60×状态数（连续两回合累加）══════════
    {
        sbs_srand(76635);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.ctx().ability_levels[0][0] = 2;   // 自身 1 种提升状态
        b.ctx().ability_levels[1][2] = -1;  // 对手 1 种下降状态
        b.start();
        b.pet(0).hp = 50000;
        idle_round(b);
        CHECK_EQ(read_key(b, kSnapOwn), 1, "T7663e 快照：自身提升状态数=1");
        CHECK_EQ(read_key(b, kSnapOpp), 1, "T7663e 快照：对手下降状态数=1");
        CHECK_EQ(b.pet(0).hp, 50060, "T7663e R1 回 60×1 精确");
        CHECK_EQ(b.pet(1).hp, 99940, "T7663e R1 对手损 60×1 精确");
        CHECK_EQ(read_key(b, kHealLast), 60, "T7663e 回复侧证=60");
        CHECK_EQ(read_key(b, kDmgLast), 60, "T7663e 损失侧证=60");
        idle_round(b);
        CHECK_EQ(b.pet(0).hp, 50120, "T7663e R2 再回 60（逐回合重复）");
        CHECK_EQ(b.pet(1).hp, 99880, "T7663e R2 对手再损 60");
        std::cout << "  [T7663e] 尾支快照回复/损失 通过\n";
    }

    // ══ T7663f 属性技也触发（"使用技能"无系别过滤）：瞪眼无伤害 → hp 裸断言 ════
    {
        sbs_srand(76636);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.ctx().ability_levels[1][0] = 2;
        b.start();
        own_use(b, /*idx=*/1);   // 瞪眼（属性技）
        CHECK_EQ(read_key(b, kSteal), 1, "T7663f 属性技触发吸取（夹具仅攻+2 一槽）");
        CHECK_EQ(b.ctx().ability_levels[1][0], -2, "T7663f 镜像照落");
        CHECK_EQ(b.pet(1).hp, kMaxHp,
                 "T7663f 属性技无伤害阶段+快照无下降 → hp 分毫不动");
        std::cout << "  [T7663f] 属性技触发 通过\n";
    }

    // ══ T7663g boss 门：主支照常 + 尾支不注册（heal/dmg 缺席 + hp 不动）═════════
    {
        sbs_srand(76637);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.ctx().ability_levels[1][0] = 2;
        b.ctx().ability_levels[1][2] = -1;
        b.start();
        const int hp0 = b.pet(1).hp;
        own_use(b);
        CHECK_EQ(read_key(b, kSteal), 1, "T7663g boss：主支吸取照常（攻+2 一槽）");
        CHECK_EQ(b.ctx().ability_levels[1][0], -2, "T7663g boss：镜像照落");
        const int d = hp0 - b.pet(1).hp;
        // ⚠️ 自身攻+2(×2) × 对手防-1(受伤×1.5，夹具尾支下降源) = ×3——带按
        //    [87,102]×3 推导再外扩 ±2（速查比例带口径）。
        CHECK(in_band(d, 259, 308), "T7663g boss：输出带 ×3（实测 " + std::to_string(d) +
                                        "）");
        CHECK_EQ(read_key(b, kHealLast), -1, "T7663g boss：尾支无回复");
        CHECK_EQ(read_key(b, kDmgLast), -1, "T7663g boss：尾支无损失");
        const int own_hp = b.pet(0).hp;
        const int opp_hp = b.pet(1).hp;
        idle_round(b);
        CHECK_EQ(b.pet(0).hp, own_hp, "T7663g boss：R2 自身 hp 不动（尾支哑）");
        CHECK_EQ(b.pet(1).hp, opp_hp, "T7663g boss：R2 对手 hp 不动（尾支哑）");
        std::cout << "  [T7663g] boss 门 通过 (d=" << d << ")\n";
    }

    // ══ T7663h 未触发对照：无提升无免疫 → 全链静默 ═════════════════════════════
    {
        sbs_srand(76638);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        const int hp0 = b.pet(1).hp;
        own_use(b);
        CHECK_EQ(read_key(b, kSteal), -1, "T7663h 无提升：吸取未写");
        CHECK_EQ(read_key(b, kMirror), -1, "T7663h 无提升：镜像未写");
        CHECK_EQ(read_key(b, kDrain), -1, "T7663h 无免疫：免疫路未落");
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 85, 104), "T7663h 纯红伤带（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7663h] 未触发对照 通过 (d=" << d << ")\n";
    }

    // ══ T7663i 无印对照 ═══════════════════════════════════════════════════════
    {
        sbs_srand(76639);
        ElfPet plain = make_pet(1, "无印者", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 300, 300,
                                kMaxHp);
        set_crit_sixteenths(plain, 0);
        for (Skills& s : plain.skills) {
            s.must_hit = 1;
            s.accuracy = 100;
        }
        ScenarioBattle b(make_party(plain), make_party(foe_pet()));
        b.start();
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 85, 104), "T7663i 无印对照纯红伤带（实测 " + std::to_string(d) +
                                       "）");
        std::cout << "  [T7663i] 无印对照 通过 (d=" << d << ")\n";
    }

    std::cout << "scenario_7663 全部通过" << std::endl;
    return 0;
}
