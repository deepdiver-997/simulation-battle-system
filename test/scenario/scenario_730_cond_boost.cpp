#include <utils/sbs_rand.h>
// 场景 730 —— G3 条件增伤族（999 / 1360 / 609，moves_lib/g_cond_boost.cpp）
//（2026-09-29，worktree ../agents/g3；号段 730-739 一文件覆盖三效果、六载体）
//
// 机制：
//   999  自身每处于{0}(=1)种能力提升状态，造成的伤害**额外**提升{1}(=10)% —— 六槽
//        正等级槽数 ×10 → **乘法通道** damage_add_extra_mul（effect_info 模板带"额外"，
//        官方 L352 非通用增伤；孤立数值与加算相同 → 按 final×(100+N)/100 断言）；
//   1360 自己出手时若自身体力**严格高于**对手 → 通用增伤 damage_add_pct +50%
//        （loss == raw×150/100；相等不算高于——镜像载体镜像侧各一钉）；
//   609  若对手处于{0}(=官方异常码 1=**中毒**) → 威力视图 ×2（132 同款物化 → 变威力
//        重算覆盖）——非中毒异常（烧伤对照）不翻，钉死"指定异常"判据。
//
// 断言口径（常识速查 §五）：
//   - 红伤 85%~100% 浮动 → loss == raw×(100+Σ)/100 比例断言（raw = 裸伤台账，
//     变威力重算**之后**、管线之前的公式值——609 的 raw 已含双倍威力）；
//   - 609 用闭式上下界 red_min/red_max 钉死重算后威力档（(0.84×攻/100×威力+2)
//     ×(217+sbs_rand()%39)/255，攻取面板、防=100、无克制/本系/暴击——make_pet 属性
//     {0,0} 克制 1x、harness 默认清零暴击）；
//   - 毒/烧跳伤 = 行动方自己的 ACTION_START 结算 max(1,max_hp/8)（**确定性**）：
//     对手总掉血 = raw + tick，tick 作已知常量并入断言；
//   - 999 的"提升状态"前置直写本体 ability_levels（视图不动 → 先手权与伤害公式
//     不受污染，650 场景同款）；链上邻接效果（521/928/773/1179/180）在对应构造下
//     均 no-op（见实现文件头），不留随机噪声；
//   - 载体 all must_hit（消 miss 噪声）；filler 用 ActionType::NONE（威力 0 有保底伤）。
#include <algorithm>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kYiCiYuan   = 19782;  // 异次元杀阵：物攻150 [521,999] → 999 切片 [1,10]
constexpr int kShengDan   = 30394;  // 圣诞夜光破：特攻150 必中 [999] → [1,10]
constexpr int kShengYe    = 32280;  // 綪·笙夜凉歌：特攻160 必中 [928,1360,773] → 1360 切片 [50]
constexpr int kBaiMoFa    = 32464;  // 白魔法之语：特攻160 必中 [1179,1360,1230] → [50]
constexpr int kShenQiJiuCai = 16231; // 神奇韭菜：物攻80 先制+2 [609] → 609 切片 [1]=中毒
constexpr int kHuanMo     = 10373;  // 幻魔空牙冲：物攻80 先制+2 [180,609] → [1]
constexpr int kPaDa       = 10004;  // 拍打：物攻40 无效果（filler）
constexpr int kSpeedSlot  = 4;      // 等级槽 4 = 速度（0攻1特攻2防3特防4速5命中）
constexpr int kAtkSlot    = 0;      // 等级槽 0 = 攻击
constexpr int kPoisonId   = static_cast<int>(AbnormalStatusId::Poison);   // 1
constexpr int kBurnId     = static_cast<int>(AbnormalStatusId::Burn);     // 2

// 红伤闭式界（Calculation::calculateDamage 同式，680 场景同款 helper）。
int red_min(int atk, int power) {
    const double b = 0.84 * atk / 100.0 * power + 2;
    return static_cast<int>(b * 217.0 / 255.0);
}
int red_max(int atk, int power) {
    const double b = 0.84 * atk / 100.0 * power + 2;
    return static_cast<int>(b);
}

// 异常跳伤：行动方 ACTION_START 结算 max(1, max_hp×1/8)（battleFsm stage/settle 同式）。
int anomaly_tick(int max_hp) {
    return std::max(1, max_hp / 8);
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk, int hp, int speed = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk = 1, int hp = 9999, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    always_hit(p);
    return p;
}

// 己方出技能 + 对手 NONE（不出招；跨回合无 watcher，零 rand 消费者）。
void play_round(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 对手出技能 + 己方 NONE（镜像方向：验证效果落在**出手侧**槽）。
void play_round_foe_attacks(ScenarioBattle& b, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // 999 异次元杀阵（物攻150）
    // ════════════════════════════════════════════════════════════════

    // T1 无提升对照 → 不增伤（loss == raw）
    {
        ScenarioBattle b(make_party(self_pet({kYiCiYuan, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK(raw > 0, "T1 前提：伤害台账非零");
        CHECK_EQ(loss, raw, "T1 核心：无提升状态 → 不增伤（loss == raw）");
        std::cout << "  [T1] 999 无提升对照 通过（raw " << raw << "）\n";
    }

    // T2 1 种提升（本体速度槽=1，视图不动）→ 额外提升 10%（loss == raw×110/100）
    {
        ScenarioBattle b(make_party(self_pet({kYiCiYuan, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][kSpeedSlot] = 1;
        play_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK_EQ(loss, raw * 110 / 100, "T2 核心：1 种提升 → +10%（乘法通道单档）");
        std::cout << "  [T2] 999 一档 +10% 通过\n";
    }

    // T3 2 种提升（速度+攻击各=1）→ 计数线性 ×2 档（loss == raw×120/100）
    {
        ScenarioBattle b(make_party(self_pet({kYiCiYuan, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][kSpeedSlot] = 1;
        ctx.ability_levels[0][kAtkSlot] = 1;
        play_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK_EQ(loss, raw * 120 / 100, "T3 核心：2 种提升 → 计数×10 = +20%（线性计档）");
        std::cout << "  [T3] 999 两档 +20% 通过\n";
    }

    // T4 圣诞夜光破（特攻150 第二载体）：2 种提升 → +20%
    {
        ScenarioBattle b(make_party(self_pet({kShengDan, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][kSpeedSlot] = 1;
        ctx.ability_levels[0][kAtkSlot] = 1;
        play_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK_EQ(loss, raw * 120 / 100, "T4 核心：第二载体同模板 → +20%");
        std::cout << "  [T4] 999 第二载体（圣诞夜光破）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 1360 綪·笙夜凉歌 / 白魔法之语（特攻160，切片 [50]）
    // ════════════════════════════════════════════════════════════════

    // T5 綪·笙夜凉歌：体力高于对手（9999 > 5000）→ +50%（loss == raw×150/100）
    {
        ScenarioBattle b(make_party(self_pet({kShengYe, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1, /*hp=*/5000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 5000 - ctx.getPet(1).hp;
        CHECK(raw > 0, "T5 前提：伤害台账非零");
        CHECK_EQ(loss, raw * 150 / 100, "T5 核心：体力严格高于对手 → 通用增伤 +50%");
        std::cout << "  [T5] 1360 高于对手 +50% 通过\n";
    }

    // T6 綪·笙夜凉歌：体力相等（9999 == 9999）→ 严格比较不成立（loss == raw）
    {
        ScenarioBattle b(make_party(self_pet({kShengYe, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/9999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK_EQ(loss, raw, "T6 核心：相等不算'高于'（严格 >）→ 不增伤");
        std::cout << "  [T6] 1360 相等对照 通过\n";
    }

    // T7 白魔法之语：体力低于对手（100 < 9999）→ 不增伤（第二载体负例；
    //    链上 1179 消对手回合类必失败（NONE 无回合类）→ 无 1179 增伤污染）
    {
        ScenarioBattle b(make_party(self_pet({kBaiMoFa, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/100, /*speed=*/300)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0);   // 己方先手出手（对手 NONE，自身 100 hp 不受击）
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK_EQ(loss, raw, "T7 核心：体力低于对手 → 不增伤（第二载体）");
        std::cout << "  [T7] 1360 低于对照（白魔法之语）通过\n";
    }

    // T8 白魔法之语：体力高于对手 → +50%（第二载体正例）
    {
        ScenarioBattle b(make_party(self_pet({kBaiMoFa, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1, /*hp=*/5000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 5000 - ctx.getPet(1).hp;
        CHECK_EQ(loss, raw * 150 / 100, "T8 核心：高于对手 → +50%（第二载体）");
        std::cout << "  [T8] 1360 高于正例（白魔法之语）通过\n";
    }

    // T9 镜像方向：对手持綪·笙夜凉歌（对手 20000 > 己方 9999，对手先手）→
    //    增伤落在**出手侧**槽（我方承伤 == raw1×150/100，raw1 = 对手台账）。
    //    ⚠️ 己方血必须低于对手：链上 773 只在"使用者体力低于对手"时互换——
    //    对手更高则 773 no-op，增伤干净可断。
    {
        ScenarioBattle b(make_party(self_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/1, /*hp=*/9999, /*speed=*/100)),
                         make_party(foe_pet({kShengYe, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/2000, /*hp=*/20000, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_foe_attacks(b, 0);
        const int raw1 = ctx.ws.raw_attack_damage[1];
        const int loss = 9999 - ctx.getPet(0).hp;
        CHECK(raw1 > 0, "T9 前提：对手伤害台账非零");
        CHECK_EQ(loss, raw1 * 150 / 100,
                 "T9 核心：效果写出手侧（owner=1）槽 → 我方承伤 +50%（方向纪律钉子）");
        std::cout << "  [T9] 1360 镜像方向 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 609 神奇韭菜 / 幻魔空牙冲（物攻80，切片 [1]=中毒）
    // ════════════════════════════════════════════════════════════════
    // 对手总掉血 = raw（重算后威力档）+ tick（毒/烧 ACTION_START 跳伤，确定性常量）。

    // T10 神奇韭菜：对手中毒 → 威力视图 80→160（raw 落在威力 160 闭式界内）
    {
        ScenarioBattle b(make_party(self_pet({kShenQiJiuCai, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1, /*hp=*/1000000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const ApplyAnomalyResult r =
            apply_anomaly(&ctx, 1, kPoisonId, /*duration_rounds=*/3, /*actor=*/0);
        REQUIRE(r == ApplyAnomalyResult::SUCCESS);
        play_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 1000000 - ctx.getPet(1).hp;
        const int tick = anomaly_tick(1000000);
        CHECK_EQ(static_cast<int>(ctx.ws.skill_power_view[0].value), 160,
                 "T10 核心：对手中毒 → 威力视图翻倍（80→160）");
        CHECK(raw >= red_min(2000, 160), "T10：重算后 raw ≥ 威力160 下界");
        CHECK(raw <= red_max(2000, 160), "T10：重算后 raw ≤ 威力160 上界");
        CHECK_EQ(loss, raw + tick, "T10：总掉血 = 红伤 + 中毒跳伤（确定性）");
        std::cout << "  [T10] 609 中毒威力翻倍（raw " << raw << " ∈ ["
                  << red_min(2000, 160) << "," << red_max(2000, 160) << "]）通过\n";
    }

    // T11 神奇韭菜：对手无异常 → 视图保持 80（对照）
    {
        ScenarioBattle b(make_party(self_pet({kShenQiJiuCai, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1, /*hp=*/1000000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 1000000 - ctx.getPet(1).hp;
        CHECK_EQ(static_cast<int>(ctx.ws.skill_power_view[0].value), 80,
                 "T11 核心：对手无异常 → 威力视图保持 80");
        CHECK(raw >= red_min(2000, 80), "T11：raw ≥ 威力80 下界");
        CHECK(raw <= red_max(2000, 80), "T11：raw ≤ 威力80 上界");
        CHECK_EQ(loss, raw, "T11：无异常无跳伤 → 总掉血 == raw");
        std::cout << "  [T11] 609 无异常对照 通过\n";
    }

    // T12 神奇韭菜：对手处于**非中毒**异常（烧伤）→ 不翻（钉死"指定异常"判据，
    //     不是 671 式"任一异常"；烧伤照常跳伤作活性对照）
    {
        ScenarioBattle b(make_party(self_pet({kShenQiJiuCai, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1, /*hp=*/1000000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const ApplyAnomalyResult r =
            apply_anomaly(&ctx, 1, kBurnId, /*duration_rounds=*/3, /*actor=*/0);
        REQUIRE(r == ApplyAnomalyResult::SUCCESS);
        play_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 1000000 - ctx.getPet(1).hp;
        const int tick = anomaly_tick(1000000);
        CHECK_EQ(static_cast<int>(ctx.ws.skill_power_view[0].value), 80,
                 "T12 核心：对手烧伤（非 {0}=1）→ 威力不翻");
        CHECK(raw >= red_min(2000, 80) && raw <= red_max(2000, 80), "T12：威力 80 界内");
        CHECK_EQ(loss, raw + tick, "T12：烧伤跳伤照常（异常确实在身）");
        std::cout << "  [T12] 609 非中毒异常对照 通过\n";
    }

    // T13 幻魔空牙冲（第二载体，链 [180,609]）：对手中毒 → 视图 160
    {
        ScenarioBattle b(make_party(self_pet({kHuanMo, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1, /*hp=*/1000000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const ApplyAnomalyResult r =
            apply_anomaly(&ctx, 1, kPoisonId, /*duration_rounds=*/3, /*actor=*/0);
        REQUIRE(r == ApplyAnomalyResult::SUCCESS);
        play_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(static_cast<int>(ctx.ws.skill_power_view[0].value), 160,
                 "T13 核心：第二载体同模板 → 视图 160");
        CHECK(raw >= red_min(2000, 160) && raw <= red_max(2000, 160),
              "T13：重算后威力 160 界内");
        std::cout << "  [T13] 609 第二载体（幻魔空牙冲）通过\n";
    }

    std::cout << "scenario_730_cond_boost all passed" << std::endl;
    return 0;
}
