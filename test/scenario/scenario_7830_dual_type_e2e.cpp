// 场景 7830 —— S20 百科机制穿测①：03 章「新双属性聚合口径」在**真实战斗流**的端到端
// （S20，2026-11-05，worktree ../agents/s20；号段 7830-7839 一文件）。
//
// 与场景 7860（J322）的分工：7860 锁死的是聚合/本系/映射的**单元语义**
// （直调 calculateRestraintMultiples / involve + 一场构局）；本场景的价值是把
// 同一批口径放进 INIT→出招→伤害落地的完整对局里，用「同面板对照场 + 浮动带
// 不交叠」证明**真战斗里**聚合生效。所有伤害断言全部经 submit_operation+drive()
// 的真实回合产生，零直调算伤。
//
// 对应百科正文（docs_local/docs/10-游戏百科/03-属性克制与伤害公式.md）：
//   T7830 §2.4 例 1（官方原文 L99）：双系技（神秘+超能，合并 id 51，残月鸦天咒同款
//         组件）打单属性（轮回 224）→ 克制均值 1.5；同面板单系技对照（普通 8 →
//         轮回 = 1.0 格值，现查 types_relation）——两场真实对局伤害带比 1.5 : 1.0。
//   T7831 §2.4 例 2（L100）：双系技（冰+暗影，id 45）打双分量（自然+圣灵，成分对
//         {20,16}）→ avg{1.0,0.5,1.0,0.5}=0.75（被克减伤害）；对照组 45 打圣灵单系
//         =0.5（L101）、45 打普通 =1.0——三档带严格不交叠。
//   T7832 §4.2 本系相交读法：复合技（圣灵+神秘 id 117）组件含攻方属性（{16,0}）
//         → ×1.5 生效；同面板无交集攻方（{4,0}）对照 → ×1.0。守方普通（8）钉住
//         克制恒 1.0（saint/myth→normal 均 1.0，现查）——单测本系因子。
//   T7833 §4.1「四个克制改写口」第一行 / §一 J322 修订注「属性视图语义不变」：
//         SKILL_EFFECT 时点改写 ws.skill_element_view（effect 2210 天忤太虚同款
//         模式：tianqi_skills.cpp 直写双槽）后，**聚合读的是视图后属性**——
//         静态单系技（神秘 14，克制 2.0）被视图改写为复合 51 → 带从 ×2.0 落到
//         ×1.5。两场真实对局互斥可观测。
//   数值锚：atk300/def100/威力40 拍打 raw = 0.84×300/100×40+2 = 102.8，
//         浮动带 [87,102]（百科 §4.3 锚），按克制×本系乘数缩放推导（7860
//         expect_band 同款 ±1 容差；禁拿单次抽样当边界）。
//
// 断言纪律：无概率路径依赖（浮动带上下界覆盖全部 39 档）、无 while(!FINISHED)，
//   每场恰好 submit×2 + drive() 一次。
#include <iostream>
#include <string>

#include <entities/pet_factory.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMythSamsaraType = 51;   // 神秘+超能（L99 载体，skill_types.att=["14 10"]）
constexpr int kIceDarkType = 45;       // 冰+暗影（L100/L101 载体，["9 13"]）
constexpr int kSaintMythType = 117;    // 圣灵+神秘（["16 14"]，T7832 复合技载体）
constexpr int kSamsara = 224;          // 轮回（天启帝君）
constexpr int kSaint = 16;             // 圣灵
constexpr int kNature = 20;            // 自然
constexpr int kNormal = 8;             // 普通
constexpr int kMyth = 14;              // 神秘
constexpr int kFlying = 4;             // 飞行

constexpr int kPaiDa = 10004;          // 拍打：普通系(8) 物 40，无 side_effect（带干净）
constexpr int kPower = 40;
constexpr int kAtk = 300;
constexpr int kDef = 100;              // make_pet 防御面板固定值
constexpr int kBigHp = 99999;

// 视图改写探针（T7833）：SKILL_EFFECT 时点把技能系别视图改写为复合 51。
// 与 effect 2210（天忤太虚，tianqi_skills.cpp:55-59）同款时点与同款双槽直写——
// 2210 需要星赐/星哲异常前置，构造困难，这里按派发允许用「同模式测试效果」替位，
// 语义等价（SKILL_EFFECT 晚于 ON_SKILL_HIT 物化 → 改写能存活到 calculateDamage）。
constexpr int kViewTarget = kMythSamsaraType;
EffectResult view_rewrite_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.skill_element_view[owner][0] = kViewTarget;
    ctx->ws.skill_element_view[owner][1] = 0;
    return EffectResult::kOk;
}

SoulMark make_view_rewrite_mark() {
    SoulMark sm;
    sm.id = 9993;
    sm.name = "视图改写(测试)";
    sm.description = "SKILL_EFFECT 时点改写 skill_element_view=复合51（effect 2210 同模式）";
    sm.args = EffectArgs(std::vector<int>{0, 1});
    // 攻方速 300 恒先手 → FIRST 桶；补 SECOND 桶防侧位（2343 AFTER_ACTION 双桶同款）。
    sm.program_ = {SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &view_rewrite_effect),
                   SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT, &view_rewrite_effect)};
    sm.has_program_ = true;
    return sm;
}

// 攻方：面板 atk300/def100/速300，技能槽全拍打；skill_element 覆写为 element_id
// （0 = 保持拍打静态普通系）。attrs = 攻方精灵属性对（参与本系判定，不参与克制）。
// J322 载体注释（7860:77-79）：直改 element 槽等价 loadSkills 的 type_id 直存路径。
ElfPet attacker_pet(const std::array<int, 2>& attrs, int skill_element = 0,
                    SoulMark mark = {}) {
    ElfPet p = make_pet(1, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, kAtk, kBigHp, std::move(mark));
    p.elementalAttributes = attrs;
    if (skill_element > 0) {
        p.skills[0].element[0] = skill_element;
        p.skills[0].element[1] = 0;
        p.skills[0].power = kPower;
    }
    return p;
}

// 守方：atk1/速100，属性对即守方系别视图（真实战斗里经 view_elementalAttributes
// 进 calculateRestraintMultiples——7860 T7867 同通道实证）。
ElfPet defender_pet(const std::array<int, 2>& attrs) {
    ElfPet p = make_pet(2, "守方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kBigHp);
    p.elementalAttributes = attrs;
    return p;
}

// 打一整回合，返回守方掉血（守方还手打攻方，不影响本侧读数）。
int damage_dealt(const ElfPet& att, const ElfPet& def) {
    ScenarioBattle battle(make_party(att), make_party(def));
    battle.start();
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();
    return kBigHp - battle.ctx().getPet(1).hp;
}

// 046/7860 直系：单段红伤理论带 = raw×克制×本系 ×[217/255, 1.0]，±1 浮点容差。
// raw = 0.84×300/100×40+2 = 102.8（atk300/def100 锚，百科 §4.3 浮动带 [87,102]）。
void expect_band(double restraint, double involve_mul, int* lo, int* hi) {
    const double raw = 0.84 * kAtk / static_cast<double>(kDef) * kPower + 2.0;
    const double scaled = raw * restraint * involve_mul;
    *lo = static_cast<int>(scaled * 217.0 / 255.0) - 1;
    *hi = static_cast<int>(scaled) + 1;
}

// 断言实测落带并打印（带出实测值便于人工复核）。
void check_in_band(int dmg, int lo, int hi, const std::string& msg) {
    CHECK(dmg >= lo && dmg <= hi,
          msg + "，实测 " + std::to_string(dmg) + "，带 [" + std::to_string(lo) + "," +
              std::to_string(hi) + "]");
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7830 03 章 §2.4 例1（L99）：双系技打单属性 → 实战红伤带 = raw×1.5 ══
    //    同面板单系技对照 = raw×1.0（锚带 [87,102]）——两带不交叠即 1.5:1.0 比例
    //    精确成立（39 档浮动全被覆盖，无抽样歧义）。
    {
        int lo = 0, hi = 0;
        expect_band(1.5, 1.0, &lo, &hi);
        const int dmg_dual = damage_dealt(attacker_pet({0, 0}, kMythSamsaraType),
                                          defender_pet({kSamsara, 0}));
        check_in_band(dmg_dual, lo, hi,
                      "T7830) 双系技(神秘+超能 51)打帝君(轮回)：avg(2.0,1.0)=1.5 真战斗带");
        const int dual_lo = lo, dual_hi = hi;

        expect_band(1.0, 1.0, &lo, &hi);
        const int dmg_single = damage_dealt(attacker_pet({0, 0}),          // 拍打静态普通系
                                            defender_pet({kSamsara, 0}));
        check_in_band(dmg_single, lo, hi,
                      "T7830) 对照：单系普通技同守方 = 1.0 锚带 [87,102] 缩放");
        CHECK(dual_lo > hi,
              "T7830) ★ 比例精确断言：×1.5 带下界(" + std::to_string(dual_lo) +
                  ")必须高于 ×1.0 带上界(" + std::to_string(hi) +
                  ")——真实战斗流里克制均值真实生效（旧相乘口径 2.0 / 未分解 1.0 均落不进带）");
        std::cout << "  [T7830] 双系带[" << dual_lo << "," << dual_hi << "] 实测 "
                  << dmg_dual << " ｜ 对照带[" << lo << "," << hi << "] 实测 "
                  << dmg_single << "（1.5 : 1.0）" << std::endl;
    }

    // ══ T7831 03 章 §2.4 例2/例3（L100/L101）：双系技打双分量 → 0.75 / 0.5 ══
    //    守方用真实成分对 {20,16}（宠物侧形态）——经战斗同步进聚合，四对
    //    {1.0,0.5,1.0,0.5} 均值 0.75；对照组圣灵单系 0.5、普通（守方空槽缺省
    //    1.0，T7864 同款语义）——三档带严格递减不交叠。
    {
        int lo = 0, hi = 0;
        expect_band(0.75, 1.0, &lo, &hi);
        const int dmg_075 = damage_dealt(attacker_pet({0, 0}, kIceDarkType),
                                         defender_pet({kNature, kSaint}));
        check_in_band(dmg_075, lo, hi,
                      "T7831) 冰+暗影打自然+圣灵（成分对）：avg 四对 =0.75 被克减带");
        const int lo075 = lo, hi075 = hi;

        expect_band(0.5, 1.0, &lo, &hi);
        const int dmg_05 = damage_dealt(attacker_pet({0, 0}, kIceDarkType),
                                        defender_pet({kSaint, 0}));
        check_in_band(dmg_05, lo, hi, "T7831) 同技打圣灵单系：avg(0.5,0.5)=0.5 带");
        CHECK(hi < lo075,
              "T7831) ★ 0.5 带上界 < 0.75 带下界（双分量口径可分——旧 1v2 相乘 0.25/"
              "旧 2v2 相乘 0.5 均落错带）");

        expect_band(1.0, 1.0, &lo, &hi);
        const int dmg_10 = damage_dealt(attacker_pet({0, 0}, kIceDarkType),
                                        defender_pet({kNormal, 0}));
        check_in_band(dmg_10, lo, hi, "T7831) 同技打普通：平割 1.0 锚带");
        const int lo10 = lo;
        CHECK(hi075 < lo10,
              "T7831) ★ 0.75 带上界(" + std::to_string(hi075) + ") < 1.0 带下界(" +
                  std::to_string(lo10) + ")（被克减方向正确：三档 1.0>0.75>0.5 "
                  "带值严格递减、两两不交叠）");
        std::cout << "  [T7831] 1.0 实测 " << dmg_10 << " ＞ 0.75 实测 " << dmg_075
                  << " ＞ 0.5 实测 " << dmg_05 << "（三带互斥）" << std::endl;
    }

    // ══ T7832 03 章 §4.2 本系相交读法：复合技组件含自身属性 → ×1.5 生效 ══
    //    守方普通（saint/myth→normal 双格均 1.0，现查）钉住克制恒 1.0，
    //    带差**纯**来自本系因子——复合技(117={圣灵,神秘}) × 攻方{16,0} 相交命中
    //    ×1.5；攻方{4,0} 无交集对照 ×1.0。两带不交叠（7860 T7867 是克制+本系
    //    双因子合流，这里单因子隔离证）。
    {
        int lo = 0, hi = 0;
        expect_band(1.0, 1.5, &lo, &hi);
        const int dmg_involve = damage_dealt(attacker_pet({kSaint, 0}, kSaintMythType),
                                             defender_pet({kNormal, 0}));
        check_in_band(dmg_involve, lo, hi,
                      "T7832) 复合技(圣灵+神秘)×圣灵攻方：相交命中 → ×1.5 本系带");
        const int inv_lo = lo;

        expect_band(1.0, 1.0, &lo, &hi);
        const int dmg_plain = damage_dealt(attacker_pet({kFlying, 0}, kSaintMythType),
                                           defender_pet({kNormal, 0}));
        check_in_band(dmg_plain, lo, hi,
                      "T7832) 对照：飞行攻方无交集 → 仅锚带 ×1.0（旧全等读法两场同带）");
        CHECK(inv_lo > hi,
              "T7832) ★ 本系带下界 > 对照带上界——真实战斗里复合本系 ×1.5 真实生效"
              "（本系因子单因子隔离）");
        std::cout << "  [T7832] 本系带 实测 " << dmg_involve << " ｜ 对照 实测 "
                  << dmg_plain << "（克制钉 1.0，1.5 : 1.0）" << std::endl;
    }

    // ══ T7833 03 章 §4.1 改写口第一行 + §一 视图语义不变：聚合读的是视图后属性 ══
    //    同一静态单系技（神秘 14，神秘→轮回 2.0）两场真实对局：
    //      无改写 → ×2.0 带；SKILL_EFFECT 视图改写为复合 51（2210 同模式）→
    //      聚合经 componentsOf(51)={14,10} 得 avg(2.0,1.0)=1.5 带。两带互斥。
    {
        int lo = 0, hi = 0;
        expect_band(2.0, 1.0, &lo, &hi);
        const int dmg_no_view = damage_dealt(attacker_pet({0, 0}, kMyth),
                                             defender_pet({kSamsara, 0}));
        check_in_band(dmg_no_view, lo, hi,
                      "T7833) 基线：静态神秘技打轮回 = 2.0 带（物化打底值）");
        const int base_hi = hi;

        expect_band(1.5, 1.0, &lo, &hi);
        const int dmg_view = damage_dealt(
            attacker_pet({0, 0}, kMyth, make_view_rewrite_mark()),
            defender_pet({kSamsara, 0}));
        check_in_band(dmg_view, lo, hi,
                      "T7833) ★ 视图改写后：聚合读视图后复合 51 → avg=1.5 带"
                      "（若仍读物化打底的 14 应落 ×2.0 带而转红）");
        CHECK(hi < base_hi,
              "T7833) ★ ×1.5 带上界(" + std::to_string(hi) + ") < ×2.0 带上界(" +
                  std::to_string(base_hi) + ")——SKILL_EFFECT 视图改写真实进入聚合"
                  "（2210 天忤太虚同通道语义，聚合读视图后属性）");
        std::cout << "  [T7833] ×2.0 实测 " << dmg_no_view << " → 视图后 ×1.5 实测 "
                  << dmg_view << "（聚合吃的是改写后的技能系别视图）" << std::endl;
    }

    std::cout << "scenario_7830 PASS（S20 穿测①：03 章新双属性聚合口径真实战斗流端到端——"
                 "L99 1.5 / L100 0.75 / L101 0.5 / 复合本系 ×1.5 / 属性视图改写交互，"
                 "全部以同面板对照场浮动带不交叠锁死）" << std::endl;
    return 0;
}
