// 场景 8200：K435 性别条件附加固伤 —— 130「对方为{0}则附加{1}点伤害」
//（resources/moves_lib/k435_xingbie_fushang_pool.cpp，2026-11-08；号段 8200-8209）
//
// 考证口径（详见插件头注）：{0} = **性别码**（1=雄性 / 2=雌性；BWIKI 效果:130 实例
// 文案 + 同门 129/131 两页互证 + 链内 898「若对手不是雄性精灵」互补 + 引擎 600
// 同码表先例）——K433 排除注记的「属性码」假设被证伪。
//
// 覆盖（条件 = 对手**本体** gender 枚举相等；落点 = deal_pink_damage(FIXED) 即时档，
// SKILL_EFFECT 桶先于本刀红伤——T9 rider 击杀直证）：
// ── 130（审计段 1001300000）──
//   T1  [2,60] 12297 深蓝利刃 vs 雌性：rider +60 同种子精确差值、审计链全申报
//   T2  同载体 vs 雄性：条件假（reason=1）、rider 不落（slot5 未写 =-1）、计数 0
//   T3  同载体 vs 无性别：门闭（NONE 不属雌——600 同判）
//   T4  [1,100] 12304 深红之光 vs 雄性：码 1→雄直证、rider +100、特攻载体同落
//   T5  双方镜像（0 侧 12297[2,60] / 1 侧 12304[1,100]）：性别对开双落 vs 性别
//       反置双闭，同种子逐位差值（0 侧吃 100 / 1 侧吃 60）、键段 owner×16 隔离
//   T6  19343 净世闭月断 [180,130,898] 混链：rider +250 精确差值；雌性对照局
//       原为 898 未注册 no-op 哨兵，898 注册（K436）后按新行为改判：预扣 500 回 250
//   T7  34612 狂影之掠 [915,130,898] 混链：rider +300、切片 [1,300] 不串位
//       （915 的 [2,50,6] 不入 130 审计）；雌性对照局哨兵翻转同 T6（回 300）
//   T8  免粉边界（PINK_DAMAGE 票）：rider 被拦（hp 与基线逐位相等）；原语返回
//       仍 SUCCESS（"已处理不落地"口径，battle_primitives.cpp:1296-1300）→
//       hp 差值为权威观测、返回码仅辅助
//   T9  rider 击杀边界：对手 hp 预置 30 < 60 → rider 先于红伤落地打死
//       （SKILL_EFFECT 桶序直证）、pink=TARGET_DEFEATED、成功计数 0
//   T10 确定性：T1 同种子两开 → 审计链与对手 hp 全等
//   （生存门"目标已倒跳过"登记前时点场景不可构造，同 K431 184 记档；门本体
//     T9 的 TARGET_DEFEATED 档已覆盖击杀路径。）
//
// 确定性纪律：130 无概率 → 全部断言种子无关；红伤带 0.84~1.0 方差 → 一律
// "性别开 vs 性别关"同种子对照取**精确差值**（变量只剩 rider）。must_hit=1 凭证
// 短路命中率链（8160 同式）；暴击率 harness 默认清零。
#include <array>
#include <iostream>

#include <effects/rule_center.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kShenLan  = 12297;   // 深蓝利刃 [130]|[2,60]：物100（雌性+60）
constexpr int kShenHong = 12304;   // 深红之光 [130]|[1,100]：特100（雄性+100）
constexpr int kJingShi  = 19343;   // 净世闭月断 [180,130,898]|[1,250,250]：物160（混链）
constexpr int kKuangYing = 34612;  // 狂影之掠 [915,130,898]|[2,50,6,1,300,300]：特85（混链）
constexpr int kPaiDa    = 10004;   // 拍打：物40 填充刀

constexpr int kAudit130 = 1001300000;

int a130(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAudit130 + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

constexpr int kMyMax = 400000;
constexpr int kFoeMax = 600000;

ElfPet caster(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = scenario::make_pet(0, "施加方", skills, speed, /*atk=*/300, kMyMax);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, /*atk=*/1, kFoeMax);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// R1 骨架：我方出载体、对手静默。foe_gender 显式置位（harness make_pet 恒 NONE）；
// foe_hp > 0 → 预置对手当前体力（T9 击杀边界）。
void run_cast(ScenarioBattle& b, int skill_index, int seed, Gender foe_gender,
              int foe_hp = -1) {
    b.ctx().getPet(1).gender = foe_gender;
    if (foe_hp > 0) {
        b.ctx().getPet(1).hp = foe_hp;
    }
    b.ctx().seed_rng(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 双方各自出载体（镜像）。g0/g1 = 双方 gender；开 = (MALE, FEMALE) 双落，
// 关 = (FEMALE, MALE) 双闭（对方的性别码都对不上）。
void run_mirror(ScenarioBattle& b, int seed, Gender g0, Gender g1) {
    b.ctx().getPet(0).gender = g0;
    b.ctx().getPet(1).gender = g1;
    b.ctx().seed_rng(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 [2,60] vs 雌性：rider +60 同种子精确差值 + 审计链 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kShenLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261108, Gender::FEMALE);
        CHECK_EQ(a130(ctx, 0, 0), 2, "T1 申报性别码=2（雌）");
        CHECK_EQ(a130(ctx, 0, 1), 60, "T1 申报固伤量=60");
        CHECK_EQ(a130(ctx, 0, 2), static_cast<int>(Gender::FEMALE), "T1 映射 Gender=FEMALE");
        CHECK_EQ(a130(ctx, 0, 3), static_cast<int>(Gender::FEMALE), "T1 对手 gender 观测=FEMALE");
        CHECK_EQ(a130(ctx, 0, 4), 1, "T1 条件=真");
        CHECK_EQ(a130(ctx, 0, 5), static_cast<int>(FixedDamageResult::SUCCESS),
                 "T1 deal_pink_damage=SUCCESS");
        CHECK_EQ(a130(ctx, 0, 6), 1, "T1 成功计数=1");
        CHECK_EQ(a130(ctx, 0, 7), 0, "T1 拒绝原因=0");
        const int foe_hp_on = ctx.getPet(1).hp;
        // T2 同载体同种子雄性对照（rider 关）。
        ScenarioBattle b2(scenario::make_party(caster({kShenLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b2.start();
        BattleContext& c2 = b2.ctx();
        run_cast(b2, 0, 20261108, Gender::MALE);
        CHECK_EQ(kFoeMax - c2.getPet(1).hp, kFoeMax - foe_hp_on - 60,
                 "T1/T2 同种子精确差值：雌性对手多扣 60（rider 逐位）"
                 "（实扣 on=" + std::to_string(kFoeMax - foe_hp_on) +
                 " off=" + std::to_string(kFoeMax - c2.getPet(1).hp) + "）");
        std::cout << "  [T1] 130 雌性门开 rider+60 通过\n";
    }

    //========== T2 [2,60] vs 雄性：条件假、rider 不落 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kShenLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261108, Gender::MALE);
        CHECK_EQ(a130(ctx, 0, 3), static_cast<int>(Gender::MALE), "T2 对手 gender 观测=MALE");
        CHECK_EQ(a130(ctx, 0, 4), 0, "T2 条件=假");
        CHECK_EQ(a130(ctx, 0, 5, -1), -1, "T2 deal_pink_damage 未调用（slot5 未写，-1 哨兵读）");
        CHECK_EQ(a130(ctx, 0, 6), 0, "T2 成功计数=0");
        CHECK_EQ(a130(ctx, 0, 7), 1, "T2 拒绝原因=1（条件假）");
        std::cout << "  [T2] 130 雄性门闭 通过\n";
    }

    //========== T3 [2,60] vs 无性别：NONE 不属雌 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kShenLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261108, Gender::NONE);
        CHECK_EQ(a130(ctx, 0, 3), static_cast<int>(Gender::NONE), "T3 对手 gender 观测=NONE");
        CHECK_EQ(a130(ctx, 0, 4), 0, "T3 条件=假（无性别≠雌性，600 同判）");
        CHECK_EQ(a130(ctx, 0, 7), 1, "T3 拒绝原因=1（条件假）");
        std::cout << "  [T3] 130 无性别门闭 通过\n";
    }

    //========== T4 [1,100] vs 雄性：码 1→雄直证 + rider+100 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kShenHong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261109, Gender::MALE);
        CHECK_EQ(a130(ctx, 0, 0), 1, "T4 申报性别码=1（雄）");
        CHECK_EQ(a130(ctx, 0, 1), 100, "T4 申报固伤量=100");
        CHECK_EQ(a130(ctx, 0, 2), static_cast<int>(Gender::MALE), "T4 映射 Gender=MALE");
        CHECK_EQ(a130(ctx, 0, 4), 1, "T4 条件=真");
        CHECK_EQ(a130(ctx, 0, 6), 1, "T4 成功计数=1");
        const int foe_hp_on = ctx.getPet(1).hp;
        ScenarioBattle b2(scenario::make_party(caster({kShenHong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b2.start();
        BattleContext& c2 = b2.ctx();
        run_cast(b2, 0, 20261109, Gender::FEMALE);
        CHECK_EQ(kFoeMax - c2.getPet(1).hp, kFoeMax - foe_hp_on - 100,
                 "T4/T2' 同种子精确差值：雄性对手多扣 100（码1→雄+量直证）");
        std::cout << "  [T4] 130 雄性门开 rider+100 通过\n";
    }

    //========== T5 双方镜像：性别对开双落 vs 反置双闭（键段隔离）==========
    {
        // 开局：0 侧 深蓝利刃[2,60] / 1 侧 深红之光[1,100]；gender (MALE, FEMALE)
        // → 0 侧条件（对手 FEMALE）✓、1 侧条件（对手 MALE）✓ 双落。
        ScenarioBattle b(scenario::make_party(caster({kShenLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kShenHong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_mirror(b, 20261110, Gender::MALE, Gender::FEMALE);
        CHECK_EQ(a130(ctx, 0, 0), 2, "T5 0 侧申报码=2");
        CHECK_EQ(a130(ctx, 1, 0), 1, "T5 1 侧申报码=1（键段隔离、参不串）");
        CHECK_EQ(a130(ctx, 0, 6), 1, "T5 0 侧成功计数=1");
        CHECK_EQ(a130(ctx, 1, 6), 1, "T5 1 侧成功计数=1");
        const int loss0_on = kMyMax - ctx.getPet(0).hp;
        const int loss1_on = kFoeMax - ctx.getPet(1).hp;
        // 反置（FEMALE, MALE）：双侧条件都闭 → 同种子基线。
        ScenarioBattle b2(scenario::make_party(caster({kShenLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kShenHong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b2.start();
        BattleContext& c2 = b2.ctx();
        run_mirror(b2, 20261110, Gender::FEMALE, Gender::MALE);
        CHECK_EQ(a130(c2, 0, 6), 0, "T5 反置 0 侧计数=0");
        CHECK_EQ(a130(c2, 1, 6), 0, "T5 反置 1 侧计数=0");
        const int loss0_off = kMyMax - c2.getPet(0).hp;
        const int loss1_off = kFoeMax - c2.getPet(1).hp;
        CHECK_EQ(loss0_on - loss0_off, 100,
                 "T5 镜像逐位：0 侧多吃 1 侧的 rider 100");
        CHECK_EQ(loss1_on - loss1_off, 60,
                 "T5 镜像逐位：1 侧多吃 0 侧的 rider 60"
                 "（实际 loss0 on/off=" + std::to_string(loss0_on) + "/" +
                 std::to_string(loss0_off) + " loss1=" + std::to_string(loss1_on) + "/" +
                 std::to_string(loss1_off) + "）");
        std::cout << "  [T5] 130 双方镜像 通过\n";
    }

    //========== T6 19343 混链 [180,130,898]：rider+250 + 898 no-op 哨兵 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261111, Gender::MALE);
        CHECK_EQ(a130(ctx, 0, 0), 1, "T6 申报码=1（250 切片归 130）");
        CHECK_EQ(a130(ctx, 0, 1), 250, "T6 申报量=250");
        CHECK_EQ(a130(ctx, 0, 6), 1, "T6 成功计数=1");
        const int foe_hp_on = ctx.getPet(1).hp;
        // 雌性对照：rider 关 + 898 已注册（K436）——预扣 500 实测回 250（哨兵翻转，
        // 原断言"预扣 500 不回血"随 898 落地转红后按新行为改判，2026-11-08）。
        ScenarioBattle b2(scenario::make_party(caster({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b2.start();
        BattleContext& c2 = b2.ctx();
        c2.getPet(0).hp = kMyMax - 500;   // 898"不是雄性恢复250"→ 应 +250
        run_cast(b2, 0, 20261111, Gender::FEMALE);
        CHECK_EQ(kFoeMax - c2.getPet(1).hp, kFoeMax - foe_hp_on - 250,
                 "T6 同种子精确差值：雄性对手多扣 250");
        CHECK_EQ(c2.getPet(0).hp, kMyMax - 500 + 250,
                 "T6 898 已注册（K436）：雌性对手回 250——预扣 500 实测 +250（哨兵翻转）");
        std::cout << "  [T6] 19343 混链 rider+250 + 898 哨兵 通过\n";
    }

    //========== T7 34612 混链 [915,130,898]：rider+300 + 切片不串位 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kKuangYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261112, Gender::MALE);
        CHECK_EQ(a130(ctx, 0, 0), 1, "T7 申报码=1（915 的 [2,50,6] 不入 130 审计）");
        CHECK_EQ(a130(ctx, 0, 1), 300, "T7 申报量=300");
        CHECK_EQ(a130(ctx, 0, 6), 1, "T7 成功计数=1");
        const int foe_hp_on = ctx.getPet(1).hp;
        ScenarioBattle b2(scenario::make_party(caster({kKuangYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b2.start();
        BattleContext& c2 = b2.ctx();
        c2.getPet(0).hp = kMyMax - 500;   // 898"不是雄性恢复300"→ 应 +300（哨兵翻转同 T6）
        run_cast(b2, 0, 20261112, Gender::FEMALE);
        CHECK_EQ(kFoeMax - c2.getPet(1).hp, kFoeMax - foe_hp_on - 300,
                 "T7 同种子精确差值：雄性对手多扣 300");
        CHECK_EQ(c2.getPet(0).hp, kMyMax - 500 + 300,
                 "T7 898 已注册（K436）：雌性对手回 300——预扣 500 实测 +300（哨兵翻转）");
        std::cout << "  [T7] 34612 混链 rider+300 + 切片隔离 通过\n";
    }

    //========== T8 免粉边界：PINK_DAMAGE 票拦 rider（hp 与基线逐位相等）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kShenLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all());  // 永久免粉
        run_cast(b, 0, 20261108, Gender::FEMALE);
        CHECK_EQ(a130(ctx, 0, 4), 1, "T8 条件=真（门开、票在原语内拦）");
        const int loss_immune = kFoeMax - ctx.getPet(1).hp;
        // 同种子无票基线（T1 的 b 局口径重开）。
        ScenarioBattle b2(scenario::make_party(caster({kShenLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b2.start();
        BattleContext& c2 = b2.ctx();
        run_cast(b2, 0, 20261108, Gender::FEMALE);
        const int loss_free = kFoeMax - c2.getPet(1).hp;
        CHECK_EQ(loss_immune + 60, loss_free,
                 "T8 免粉：rider 60 被拦（免粉局扣血 = 自由局 - 60，逐位）"
                 "（immune=" + std::to_string(loss_immune) +
                 " free=" + std::to_string(loss_free) + "）");
        std::cout << "  [T8] 130 免粉边界 通过\n";
    }

    //========== T9 rider 击杀边界：先于红伤落地 → TARGET_DEFEATED ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kShenLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261108, Gender::FEMALE, /*foe_hp=*/30);
        CHECK(ctx.getPet(1).hp <= 0, "T9 对手已被 rider 击倒");
        CHECK_EQ(a130(ctx, 0, 5), static_cast<int>(FixedDamageResult::TARGET_DEFEATED),
                 "T9 deal_pink_damage=TARGET_DEFEATED（rider 先于红伤，SKILL_EFFECT 桶序直证）");
        CHECK_EQ(a130(ctx, 0, 6), 0, "T9 成功计数=0（击杀档不计成功）");
        std::cout << "  [T9] 130 rider 击杀边界 通过\n";
    }

    //========== T10 确定性：同种子两开 → 审计链与对手 hp 全等 ==========
    {
        int hp_a = 0, hp_b = 0;
        int audit_a[8] = {}, audit_b[8] = {};
        for (int rep = 0; rep < 2; ++rep) {
            ScenarioBattle b(scenario::make_party(caster({kShenHong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            run_cast(b, 0, 777, Gender::MALE);
            for (int s = 0; s <= 7; ++s) {
                if (rep == 0) audit_a[s] = a130(ctx, 0, s, -999);
                else audit_b[s] = a130(ctx, 0, s, -999);
            }
            if (rep == 0) hp_a = ctx.getPet(1).hp;
            else hp_b = ctx.getPet(1).hp;
        }
        for (int s = 0; s <= 7; ++s) {
            CHECK_EQ(audit_b[s], audit_a[s],
                     std::string("T10 审计槽 ") + std::to_string(s) + " 两开全等");
        }
        CHECK_EQ(hp_b, hp_a, "T10 对手 hp 两开全等");
        std::cout << "  [T10] 130 确定性 通过\n";
    }

    std::cout << "scenario_8200 PASS (K435 性别条件附加固伤 130)" << std::endl;
    return 0;
}
