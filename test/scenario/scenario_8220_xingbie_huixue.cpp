// 场景 8220：K436 性别互补回血族 —— 898「若对手不是雄性精灵，则恢复{0}点体力」
//（resources/moves_lib/k436_xingbie_huixue_pool.cpp，2026-11-08；号段 8220-8229）
//
// 考证口径（详见插件头注）：判据 = 对手 gender != Gender::MALE（**含雌性与无性别**
// ——K435 130「对方为{0}则附加伤害」性别码定案的链内互补对：雄性 130 开/898 闭，
// 非雄性 130 闭/898 开）；恢复通道 = heal_amount 真原语（吃封回血/恢复修正%，
// 满血封顶 actual=0）；生存门 = owner 侧 hp<=0（426 2344 自体门同式，登记前不可
// 构造——同 K435 记档）。
//
// 覆盖（898 落在 SKILL_EFFECT 桶；19343/34612 链内 130 先于 898，互补门保证同回合
// 至多一条开——T1/T2/T6 即"与 K435 130 宏的同场隔离断言"）：
// ── 898（审计段 1008980000）+ 130（审计段 1001300000）──
//   T1  19343 净世闭月断 vs 雌性：预扣 500 → 回 250 逐位精确；a898 链全申报；
//       a130 条件假（reason=1）计数 0——130/898 双键段同场各写各的（隔离直证）
//   T2  同载体 vs 雄性：898 闭（reason=1、不回血——预扣 500 原样）；130 开
//       （计数 1）+ 同种子对照 rider +250 逐位——互补对反演
//   T3  同载体 vs 无性别：130 闭（NONE≠MALE）+ 898 开（"不是雄性"含 NONE）
//       → 回 250——三向门无性别腿直证
//   T4  29168 茸茸蓄力（属性技）vs 雌性：898 切片 [300] → 回 300 逐位；属性技
//       不落伤害（对手 hp 原样）；585[0,1,1,1,1,0]/89[3,1] 不入 898 审计（切片不串位）
//   T5  37894 弧光爪（物85 先制+3）vs 雌性：898 切片 [200] → 回 200；同种子雄性
//       对照差值 200 逐位（743 无提升可反→确定性）
//   T6  双方镜像（双侧 19343，双侧预扣 500，互打红伤同种子相消——(MALE,MALE)
//       对照 C 基线分解）：(MALE,FEMALE) = 0 侧基线+250（回）/ 1 侧基线+250（免骑）；
//       (FEMALE,MALE) 反演；四象限审计（a130 0/1 侧 × a898 0/1 侧）逐槽互补——
//       键段 owner×16 隔离
//   T7  满血封顶：满血 + 条件开 → actual=0、计数 0（不算成功）、返回码 SUCCESS
//       （"已处理不落地"口径，hp 为权威观测——K435 T8 同式）
//   T8  封回血（HEAL_BLOCK 票）：条件开、通道拦 → actual=0、hp 原样（通道内拦证，
//       直写 hp 就此穿帮——通道选型反证）
//   T9  确定性：同种子两开 → a898 八槽 + hp 全等
//   （生存门"owner 已倒跳过"登记前时点不可构造，同 K435/K431 记档。）
//
// 确定性纪律：898 无概率 → 全部断言种子无关；红伤带 0.84~1.0 方差 → 一律
// "门开 vs 门关"同种子对照取**精确差值**（变量只剩 898/130 一条腿）。
// must_hit=1 凭证短路命中率链（8200 同式）；暴击率 harness 默认清零。
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

constexpr int kJingShi  = 19343;   // 净世闭月断 [180,130,898]|[1,250,250]：物160 must_hit
constexpr int kRongRong = 29168;   // 茸茸蓄力 [585,89,898]|[0,1,1,1,1,0,3,1,300]：属性技
constexpr int kHuGuang  = 37894;   // 弧光爪 [743,898]|[1,1,200]：物85 先制+3
constexpr int kPaiDa    = 10004;   // 拍打：物40 填充刀

constexpr int kAudit130 = 1001300000;   // K435 130 键段（同场隔离断言用）
constexpr int kAudit898 = 1008980000;   // K436 898 键段

int audit_int(BattleContext& ctx, int base, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(base + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a130(BattleContext& ctx, int owner, int slot, int def = 0) {
    return audit_int(ctx, kAudit130, owner, slot, def);
}
int a898(BattleContext& ctx, int owner, int slot, int def = 0) {
    return audit_int(ctx, kAudit898, owner, slot, def);
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
// self_debit > 0 → 预扣我方当前体力（回血断言基线）。
void run_cast(ScenarioBattle& b, int skill_index, int seed, Gender foe_gender,
              int self_debit = 0) {
    b.ctx().getPet(1).gender = foe_gender;
    if (self_debit > 0) {
        b.ctx().getPet(0).hp = kMyMax - self_debit;
    }
    sbs_srand(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 双方各自出 19343（镜像）。g0/g1 = 双方 gender；双侧预扣 500。
void run_mirror(ScenarioBattle& b, int seed, Gender g0, Gender g1) {
    b.ctx().getPet(0).gender = g0;
    b.ctx().getPet(1).gender = g1;
    b.ctx().getPet(0).hp = kMyMax - 500;
    b.ctx().getPet(1).hp = kFoeMax - 500;
    sbs_srand(static_cast<unsigned>(seed));
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

    //===== T1 19343 vs 雌性：回 250 逐位 + a898 链全申报 + 130/898 双段隔离 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261108, Gender::FEMALE, /*self_debit=*/500);
        CHECK_EQ(ctx.getPet(0).hp, kMyMax - 250,
                 "T1 预扣 500 → 回 250 逐位（雌性对手=非雄性，898 开）");
        CHECK_EQ(a898(ctx, 0, 0), 250, "T1 申报恢复量=250");
        CHECK_EQ(a898(ctx, 0, 1), static_cast<int>(Gender::FEMALE), "T1 对手 gender 观测=FEMALE");
        CHECK_EQ(a898(ctx, 0, 2), 1, "T1 条件=开（非雄性）");
        CHECK_EQ(a898(ctx, 0, 3), static_cast<int>(HealResult::SUCCESS), "T1 heal_amount=SUCCESS");
        CHECK_EQ(a898(ctx, 0, 4), 250, "T1 实际恢复=250");
        CHECK_EQ(a898(ctx, 0, 5), 1, "T1 成功计数=1");
        CHECK_EQ(a898(ctx, 0, 6), 0, "T1 拒绝原因=0");
        CHECK_EQ(a898(ctx, 0, 7), kMyMax - 250, "T1 恢复后 hp 观测=kMyMax-250");
        // 同场隔离：130 键段照常申报自身切片、条件假计数 0——两段互不覆写。
        CHECK_EQ(a130(ctx, 0, 0), 1, "T1 130 申报性别码=1（[1,250] 切片归 130）");
        CHECK_EQ(a130(ctx, 0, 1), 250, "T1 130 申报固伤量=250");
        CHECK_EQ(a130(ctx, 0, 4), 0, "T1 130 条件=假（对手雌性）");
        CHECK_EQ(a130(ctx, 0, 6), 0, "T1 130 成功计数=0");
        CHECK_EQ(a130(ctx, 0, 7), 1, "T1 130 拒绝原因=1（条件假）");
        const int foe_loss_on = kFoeMax - ctx.getPet(1).hp;
        std::cout << "  [T1] 898 雌性门开 回250 + 双段隔离 通过\n";
    }

    //===== T2 19343 vs 雄性：898 闭不回血 + 130 开 rider+250（互补反演）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261108, Gender::MALE, /*self_debit=*/500);
        CHECK_EQ(ctx.getPet(0).hp, kMyMax - 500,
                 "T2 雄性对手：898 闭、预扣 500 原样（不回血）");
        CHECK_EQ(a898(ctx, 0, 1), static_cast<int>(Gender::MALE), "T2 对手 gender 观测=MALE");
        CHECK_EQ(a898(ctx, 0, 2), 0, "T2 条件=闭（是雄性）");
        CHECK_EQ(a898(ctx, 0, 4), 0, "T2 实际恢复=0（未调 heal——slot4 缺省读）");
        CHECK_EQ(a898(ctx, 0, 5), 0, "T2 成功计数=0");
        CHECK_EQ(a898(ctx, 0, 6), 1, "T2 拒绝原因=1（对手是雄性）");
        // 130 同场开：互补对反演直证。
        CHECK_EQ(a130(ctx, 0, 4), 1, "T2 130 条件=真（对手雄性）");
        CHECK_EQ(a130(ctx, 0, 6), 1, "T2 130 成功计数=1");
        // 同种子对照取 rider 精确差值（T1 的雌性别名基线，同 seed 同载体）。
        ScenarioBattle b2(scenario::make_party(caster({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b2.start();
        BattleContext& c2 = b2.ctx();
        run_cast(b2, 0, 20261108, Gender::FEMALE);
        const int foe_loss_off = kFoeMax - c2.getPet(1).hp;
        const int foe_loss_on = kFoeMax - ctx.getPet(1).hp;
        CHECK_EQ(foe_loss_on - foe_loss_off, 250,
                 "T2 同种子精确差值：雄性对手多扣 250（130 rider 逐位）"
                 "（on=" + std::to_string(foe_loss_on) +
                 " off=" + std::to_string(foe_loss_off) + "）");
        std::cout << "  [T2] 898 雄性门闭 + 130 rider+250 互补 通过\n";
    }

    //===== T3 19343 vs 无性别：130 闭 + 898 开（三向门 NONE 腿）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261108, Gender::NONE, /*self_debit=*/500);
        CHECK_EQ(ctx.getPet(0).hp, kMyMax - 250,
                 "T3 无性别对手=「不是雄性精灵」→ 回 250（三向门 NONE 开腿）");
        CHECK_EQ(a898(ctx, 0, 1), static_cast<int>(Gender::NONE), "T3 对手 gender 观测=NONE");
        CHECK_EQ(a898(ctx, 0, 2), 1, "T3 条件=开（NONE≠MALE）");
        CHECK_EQ(a898(ctx, 0, 5), 1, "T3 成功计数=1");
        CHECK_EQ(a130(ctx, 0, 4), 0, "T3 130 条件=假（NONE 不属雄，600 同判）");
        CHECK_EQ(a130(ctx, 0, 6), 0, "T3 130 成功计数=0");
        std::cout << "  [T3] 898 无性别门开 通过\n";
    }

    //===== T4 29168 茸茸蓄力（属性技）vs 雌性：切片 [300] 回 300、不落伤害 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kRongRong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261113, Gender::FEMALE, /*self_debit=*/500);
        CHECK_EQ(a898(ctx, 0, 0), 300, "T4 申报恢复量=300（9 参切片尾参归 898，585/89 不串位）");
        CHECK_EQ(a898(ctx, 0, 4), 300, "T4 实际恢复=300");
        CHECK_EQ(ctx.getPet(0).hp, kMyMax - 200, "T4 预扣 500 → 回 300 逐位");
        CHECK_EQ(ctx.getPet(1).hp, kFoeMax,
                 "T4 属性技不落伤害：对手 hp 原样（585 提升窗/89 吸血窗不动血）");
        std::cout << "  [T4] 29168 属性技 回300 切片隔离 通过\n";
    }

    //===== T5 37894 弧光爪（物85）vs 雌性：切片 [200] 回 200 + 同种子差值 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kHuGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261114, Gender::FEMALE, /*self_debit=*/500);
        CHECK_EQ(a898(ctx, 0, 0), 200, "T5 申报恢复量=200（743 的 [1,1] 不入 898 审计）");
        CHECK_EQ(a898(ctx, 0, 4), 200, "T5 实际恢复=200");
        const int self_hp_on = ctx.getPet(0).hp;
        // 同种子雄性对照：898 关，差值即回血量（743 无提升可反→无 rand 消耗）。
        ScenarioBattle b2(scenario::make_party(caster({kHuGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b2.start();
        BattleContext& c2 = b2.ctx();
        run_cast(b2, 0, 20261114, Gender::MALE, /*self_debit=*/500);
        CHECK_EQ(self_hp_on - c2.getPet(0).hp, 200,
                 "T5 同种子精确差值：雌性对手多回 200");
        CHECK_EQ(a898(c2, 0, 6), 1, "T5 对照局拒绝原因=1（对手是雄性）");
        std::cout << "  [T5] 37894 回200 通过\n";
    }

    //===== T6 双方镜像：四象限互补 + 键段 owner×16 隔离 =====
    {
        // 开局双侧 19343、双侧预扣 500、双侧互打（红伤方差同种子相消——对照组 C 分解）。
        // Run C (MALE, MALE)：双侧 130 开（各吃 rider 250）、双侧 898 闭 —— 基线。
        ScenarioBattle bc(scenario::make_party(caster({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        bc.start();
        BattleContext& cc = bc.ctx();
        run_mirror(bc, 20261110, Gender::MALE, Gender::MALE);
        const int base0 = cc.getPet(0).hp;   // -500 -250(rider) -红伤(1→0)
        const int base1 = cc.getPet(1).hp;   // -500 -250(rider) -红伤(0→1)
        CHECK_EQ(a898(cc, 0, 5), 0, "T6C 0 侧 898 计数=0（双雄性全闭）");
        CHECK_EQ(a130(cc, 0, 6), 1, "T6C 0 侧 130 计数=1");
        CHECK_EQ(a130(cc, 1, 6), 1, "T6C 1 侧 130 计数=1");
        // Run A (MALE, FEMALE)：0 侧 898 开（回 250）、0 侧 130 闭；1 侧 130 开
        // （rider+250 打 0 侧）、1 侧 898 闭 → 0 侧 = 基线 +250（回 250 恰抵吃进的
        // rider 250）、1 侧 = 基线 +250（0 侧 130 闭、少骑 250）。
        ScenarioBattle b(scenario::make_party(caster({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_mirror(b, 20261110, Gender::MALE, Gender::FEMALE);
        CHECK_EQ(ctx.getPet(0).hp, base0 + 250, "T6A 0 侧 = 基线+250（898 回 250 逐位）");
        CHECK_EQ(ctx.getPet(1).hp, base1 + 250, "T6A 1 侧 = 基线+250（0 侧 130 闭、少骑 250）");
        CHECK_EQ(a898(ctx, 0, 5), 1, "T6A 0 侧 898 计数=1");
        CHECK_EQ(a898(ctx, 1, 5), 0, "T6A 1 侧 898 计数=0（对手 MALE）");
        CHECK_EQ(a130(ctx, 0, 6), 0, "T6A 0 侧 130 计数=0（对手 FEMALE）");
        CHECK_EQ(a130(ctx, 1, 6), 1, "T6A 1 侧 130 计数=1（对手 MALE）");
        // Run B (FEMALE, MALE)：反演 —— 0 侧 898 闭、0 侧 130 开（rider 打 1 侧）；
        // 1 侧 898 开（回 250）、1 侧 130 闭 → 0 侧 = 基线 +250（没吃 rider 也没回）；
        // 1 侧 = 基线 +250（回 250 恰抵吃进的 rider 250——与 A 的 0 侧同构镜像）。
        ScenarioBattle b2(scenario::make_party(caster({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b2.start();
        BattleContext& c2 = b2.ctx();
        run_mirror(b2, 20261110, Gender::FEMALE, Gender::MALE);
        CHECK_EQ(c2.getPet(0).hp, base0 + 250,
                 "T6B 0 侧 = 基线+250（898 闭不回、也没吃 rider——rider 打到 1 侧）");
        CHECK_EQ(c2.getPet(1).hp, base1 + 250,
                 "T6B 1 侧 = 基线+250（898 回 250 恰抵吃进的 0 侧 rider 250）");
        CHECK_EQ(a898(c2, 0, 5), 0, "T6B 0 侧 898 计数=0");
        CHECK_EQ(a898(c2, 1, 5), 1, "T6B 1 侧 898 计数=1");
        CHECK_EQ(a130(c2, 0, 6), 1, "T6B 0 侧 130 计数=1");
        CHECK_EQ(a130(c2, 1, 6), 0, "T6B 1 侧 130 计数=0");
        std::cout << "  [T6] 898 双方镜像四象限互补（对照 C 红伤相消）通过\n";
    }

    //===== T7 满血封顶：条件开但 actual=0、不算成功 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261108, Gender::FEMALE);   // 满血（不预扣）
        CHECK_EQ(ctx.getPet(0).hp, kMyMax, "T7 满血回不上（clamp 封顶）");
        CHECK_EQ(a898(ctx, 0, 2), 1, "T7 条件=开");
        CHECK_EQ(a898(ctx, 0, 3), static_cast<int>(HealResult::SUCCESS),
                 "T7 heal_amount 返回仍 SUCCESS（clamp 在原语内，只查参数）");
        CHECK_EQ(a898(ctx, 0, 4), 0, "T7 实际恢复=0");
        CHECK_EQ(a898(ctx, 0, 5), 0, "T7 成功计数=0（actual>0 才计）");
        CHECK_EQ(a898(ctx, 0, 6), 0, "T7 拒绝原因=0（门开、非拒绝）");
        std::cout << "  [T7] 898 满血封顶 通过\n";
    }

    //===== T8 封回血（HEAL_BLOCK）：条件开、通道拦、hp 原样 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all());  // 永久封我方回血
        run_cast(b, 0, 20261108, Gender::FEMALE, /*self_debit=*/500);
        CHECK_EQ(a898(ctx, 0, 2), 1, "T8 条件=开（票在通道内拦）");
        CHECK_EQ(ctx.getPet(0).hp, kMyMax - 500,
                 "T8 封回血：预扣 500 原样（heal_impl HEAL_BLOCK 档，battle_primitives.cpp:1487）");
        CHECK_EQ(a898(ctx, 0, 4), 0, "T8 实际恢复=0");
        CHECK_EQ(a898(ctx, 0, 5), 0, "T8 成功计数=0");
        std::cout << "  [T8] 898 封回血边界 通过\n";
    }

    //===== T9 确定性：同种子两开 → a898 八槽 + hp 全等 =====
    {
        int hp_a = 0, hp_b = 0;
        int audit_a[8] = {}, audit_b[8] = {};
        for (int rep = 0; rep < 2; ++rep) {
            ScenarioBattle b(scenario::make_party(caster({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            run_cast(b, 0, 777, Gender::FEMALE, /*self_debit=*/500);
            for (int s = 0; s <= 7; ++s) {
                if (rep == 0) audit_a[s] = a898(ctx, 0, s, -999);
                else audit_b[s] = a898(ctx, 0, s, -999);
            }
            if (rep == 0) hp_a = ctx.getPet(0).hp;
            else hp_b = ctx.getPet(0).hp;
        }
        for (int s = 0; s <= 7; ++s) {
            CHECK_EQ(audit_b[s], audit_a[s],
                     std::string("T9 审计槽 ") + std::to_string(s) + " 两开全等");
        }
        CHECK_EQ(hp_b, hp_a, "T9 我方 hp 两开全等");
        std::cout << "  [T9] 898 确定性 通过\n";
    }

    std::cout << "scenario_8220 PASS (K436 性别互补回血族 898)" << std::endl;
    return 0;
}
