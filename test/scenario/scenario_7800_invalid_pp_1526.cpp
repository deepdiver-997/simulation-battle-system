// 场景 7800 —— J321 1526 后缀子句「技能无效时减少对手所有技能PP1点」
//（2026-11-05，worktree ../agents/j321；号段 7800-7809，一文件）
// J325 · 数据刀拆分改造（2026-11-05，worktree ../agents/j325）：后缀已从 1526 趟别
//  分支迁出为私有 id **900001**（side_effect 私有段 900000+，db_patch j3/ 链尾追加；
//  内核 case 900001 → SKILL_INVALID 天然桶位，插件 SKILL_EFFECT(900001) 承接）。
//  本场景读法随之改定：无效路径扣 PP = **900001 事件**（SKILL_INVALID 单桶注册执行），
//  不再经由 1526 的趟别判断（ws.skill_exec_result 判读自插件摘除，机制本身保留给
//  2086~2090 等未拆分 INHERENT 家族）。
//
// 断言口径（J316 场景 7560 直系 + PP 槽动态捕获精确差值）：
//   - 1526 链（拆分后）= [.., 1526, 900001]：1526 默认 HIT 桶（命中趟主句 roll 固伤）、
//     900001 SKILL_INVALID 桶（无效趟减对手全技 PP1）——双效果各自进各自桶，T7800.e
//     同局端到端同场验证。
//   - 无效路径注入：seal_skill 龙威（封攻+封属、不可穿透，002 场景同款）——
//     攻击技载体（智眼解析 35732）与属性技载体（菌丝体期 26986）各验一趟。
//   - 无效趟恒等：对手 hp 精确不变（1526 命中趟未注册 → 无固伤）、审计键缺失
//     （1526 不 roll）、对手全技 PP = 前-1（900001 只扣 1 点）、己方已用技 PP = 前-1
//     （"用了但被无效"只扣使用，不吃自己的后缀——后缀目标是对手）。
//   - 钳底：pp=1 的槽扣到 0（pp_reduce 钳底语义），非归零。
//   - 主句回归（本文件内基线）：命中趟审计 ∈ [200,400] + 掉血-固伤 == 046 直系红伤带
//     + 对手 PP 不变（900001 不越趟：SKILL_INVALID 桶在命中趟根本不注册）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kZhiYan   = 35732;  // 智眼解析：物攻130 [1526]|[200,400]（攻击技载体）
constexpr int kJunSi    = 26986;  // 菌丝体期：属性技 [1526]|[100,400,1]（第三参冗余，arg_count=2）
constexpr int kPaiDa    = 10004;  // 拍打：物 40（填充刀）
constexpr int kAuditBase = 990152600;  // 1526 审计槽（实现同款）
constexpr int kBigHp    = 99999;

// ── 046 直系：单段红伤理论区间（def 恒 100；7560 同式）──
void expect_range(int atk, int power, int* lo, int* hi) {
    const double raw = 0.84 * atk / 100.0 * power + 2.0;
    *lo = static_cast<int>(raw * 217.0 / 255.0) - 2;
    *hi = static_cast<int>(raw) + 2;
}

// 双侧钉 restraint=1.0（ROUND_START 桶每回合重写，7560 同款）。
EffectResult pin_restraint_tick(BattleContext* ctx, const EffectArgs&) {
    if (ctx) {
        ctx->ws.restraint_view[0] = 1.0;
        ctx->ws.restraint_view[1] = 1.0;
    }
    return EffectResult::kOk;
}

void register_restraint_pin(BattleContext& ctx, int owner) {
    Effect e(9997800, /*priority=*/0, owner, /*left_round=*/1,
             EffectArgs(std::vector<int>{owner, 1 - owner}), &pin_restraint_tick);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, owner,
                                                 /*duration=*/9, ctx.roundCount);
    ce->source_id_ = 9997800;
    ctx.register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
}

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

int audit_or(BattleContext& ctx, int key, int fallback) {
    auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return fallback;   // 类型不符按缺失语义回退（7560 同款，不抛）
}

ElfPet self_pet(int skill0, int speed, int atk, int hp) {
    ElfPet p = make_pet(1, "己方", {skill0, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    set_crit_sixteenths(p, 0);
    return p;
}

ElfPet foe_pet(int atk, int hp, int speed = 100) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    set_crit_sixteenths(p, 0);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7800.a 攻击技载体 + 龙威无效：900001 扣对手全技 PP1、无固伤、审计键缺失 ══
    {
        ScenarioBattle b(make_party(self_pet(kZhiYan, /*speed=*/300, /*atk=*/300, kBigHp)),
                         make_party(foe_pet(/*atk=*/1, kBigHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 注入龙威：对手(施放方1)封己方(被封方0)的攻击+属性技能，不可穿透（002 同款）
        seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/70001,
                   /*attribute=*/true, /*attack=*/true, /*count=*/1,
                   /*duration_rounds=*/0, /*penetrable=*/false);
        int foe_pp[5], self_pp[5];
        for (int i = 0; i < 5; ++i) {
            foe_pp[i] = ctx.getPet(1).skills[i].pp;
            self_pp[i] = ctx.getPet(0).skills[i].pp;
        }
        const int foe_before = ctx.getPet(1).hp;
        const int self_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 智眼解析（将被封）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID,
                 "T7800.a 前置：龙威下智眼解析走 SKILL_INVALID 趟（900001 桶的注册前提）");
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(ctx.getPet(1).skills[i].pp, foe_pp[i] - 1,
                     "T7800.a 900001：对手全技 PP 各-1（槽 " + std::to_string(i) +
                         "，实测 " + std::to_string(ctx.getPet(1).skills[i].pp) +
                         "，应 " + std::to_string(foe_pp[i] - 1) + "）");
        }
        CHECK_EQ(ctx.getPet(0).skills[0].pp, self_pp[0] - 1,
                 "T7800.a 被封技只扣使用 PP（实测 " + std::to_string(ctx.getPet(0).skills[0].pp) +
                     "，应 " + std::to_string(self_pp[0] - 1) + "）");
        for (int i = 1; i < 5; ++i) {
            CHECK_EQ(ctx.getPet(0).skills[i].pp, self_pp[i],
                     "T7800.a 900001 目标=对手，己方未用技 PP 不动（槽 " + std::to_string(i) + "）");
        }
        CHECK_EQ(ctx.getPet(1).hp, foe_before,
                 "T7800.a 无效趟无固伤（1526 只挂 HIT 桶未注册）：对手 hp 精确不变（实测 " +
                     std::to_string(ctx.getPet(1).hp) + "，前 " + std::to_string(foe_before) + "）");
        CHECK_EQ(ctx.getPet(0).hp, self_before, "T7800.a 对手 NONE + 拦截 → 己方 hp 不变");
        CHECK(ctx.plugin_storage.find(kAuditBase + 0) == ctx.plugin_storage.end(),
              "T7800.a 无效趟 1526 不注册不 roll：审计键 990152600 缺失");
        std::cout << "  [T7800.a] 攻击技载体打盔：900001 扣对手全技 PP-1、零固伤、零 roll ✓\n";
    }

    // ══ T7800.b 主句回归（命中趟基线）：审计 roll + 红伤恒等 + 对手 PP 不动 ══════
    {
        ScenarioBattle b(make_party(self_pet(kZhiYan, /*speed=*/300, /*atk=*/300, kBigHp)),
                         make_party(foe_pet(/*atk=*/1, kBigHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        register_restraint_pin(ctx, 0);
        register_restraint_pin(ctx, 1);
        int foe_pp[5];
        for (int i = 0; i < 5; ++i) {
            foe_pp[i] = ctx.getPet(1).skills[i].pp;
        }
        const int foe_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 智眼解析（命中）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::HIT,
                 "T7800.b 前置：无拦截智眼解析命中趟");
        const int amount = audit_or(ctx, kAuditBase + 0, -1);
        CHECK(amount >= 200 && amount <= 400,
              "T7800.b 主句审计抽取 ∈ [200,400]（实测 " + std::to_string(amount) + "）");
        int lo = 0, hi = 0;
        expect_range(300, 130, &lo, &hi);
        const int red_part = (foe_before - ctx.getPet(1).hp) - amount;
        CHECK(red_part >= lo && red_part <= hi,
              "T7800.b 掉血-固伤 == 红伤带 [" + std::to_string(lo) + "," + std::to_string(hi) +
                  "]（实测红份 " + std::to_string(red_part) + "，总掉 " +
                  std::to_string(foe_before - ctx.getPet(1).hp) + "，审计 " +
                  std::to_string(amount) + "）");
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(ctx.getPet(1).skills[i].pp, foe_pp[i],
                     "T7800.b 命中趟 900001 不注册（SKILL_INVALID 单桶）：对手 PP 不动（槽 " +
                         std::to_string(i) + "）");
        }
        std::cout << "  [T7800.b] 命中趟主句回归：roll=" << amount << " + 红伤恒等 + PP 不动 ✓\n";
    }

    // ══ T7800.c 属性技载体 + 龙威无效：同样走 900001（属性技本无伤，hp 恒等）════
    {
        ScenarioBattle b(make_party(self_pet(kJunSi, /*speed=*/300, /*atk=*/1, kBigHp)),
                         make_party(foe_pet(/*atk=*/1, kBigHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/70001,
                   /*attribute=*/true, /*attack=*/true, /*count=*/1,
                   /*duration_rounds=*/0, /*penetrable=*/false);
        int foe_pp[5];
        for (int i = 0; i < 5; ++i) {
            foe_pp[i] = ctx.getPet(1).skills[i].pp;
        }
        const int foe_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 菌丝体期（将被封）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID,
                 "T7800.c 前置：龙威封属性技 → SKILL_INVALID 趟");
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(ctx.getPet(1).skills[i].pp, foe_pp[i] - 1,
                     "T7800.c 属性技载体 900001：对手全技 PP-1（槽 " + std::to_string(i) + "）");
        }
        CHECK_EQ(ctx.getPet(1).hp, foe_before,
                 "T7800.c 无效趟无固伤：对手 hp 精确不变");
        CHECK(ctx.plugin_storage.find(kAuditBase + 0) == ctx.plugin_storage.end(),
              "T7800.c 无效趟不 roll：审计键缺失");
        std::cout << "  [T7800.c] 属性技载体打盔：900001 照扣、零固伤 ✓\n";
    }

    // ══ T7800.d 钳底与单点精确：pp=1 的槽扣到 0，其余只-1（非归零语义）══════════
    {
        ElfPet foe = foe_pet(/*atk=*/1, kBigHp);
        foe.skills[2].pp = 1;   // 直改（make_party 前变异 → 入场生效）
        ScenarioBattle b(make_party(self_pet(kZhiYan, /*speed=*/300, /*atk=*/300, kBigHp)),
                         make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/70001,
                   /*attribute=*/true, /*attack=*/true, /*count=*/1,
                   /*duration_rounds=*/0, /*penetrable=*/false);
        int foe_pp[5];
        for (int i = 0; i < 5; ++i) {
            foe_pp[i] = ctx.getPet(1).skills[i].pp;
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0,
                 "T7800.d 钳底：pp=1 的槽后缀后=0（实测 " +
                     std::to_string(ctx.getPet(1).skills[2].pp) + "）");
        for (int i = 0; i < 5; ++i) {
            if (i == 2) continue;
            CHECK_EQ(ctx.getPet(1).skills[i].pp, foe_pp[i] - 1,
                     "T7800.d 只-1 不归零（槽 " + std::to_string(i) +
                         "，实测 " + std::to_string(ctx.getPet(1).skills[i].pp) + "）");
        }
        std::cout << "  [T7800.d] 钳底 + 非-PP 技能只扣 1 点 ✓\n";
    }

    // ══ T7800.e 端到端同场（J325 新增）：同一局双回合，链上双效果各自进各自桶 ════
    // R1 无拦截：1526 命中趟主句 roll 固伤、对手掉血、900001 未注册（PP 不动）；
    // R2 注入龙威：智眼解析走 SKILL_INVALID 趟，只注册 900001——零固伤（1526 的
    // HIT 桶不注册）+ 对手全技 PP-1（900001 执行）+ 审计槽不被覆盖（1526 未执行）。
    {
        ScenarioBattle b(make_party(self_pet(kZhiYan, /*speed=*/300, /*atk=*/300, kBigHp)),
                         make_party(foe_pet(/*atk=*/1, kBigHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        register_restraint_pin(ctx, 0);
        register_restraint_pin(ctx, 1);
        int foe_pp0[5], self_pp0[5];
        for (int i = 0; i < 5; ++i) {
            foe_pp0[i] = ctx.getPet(1).skills[i].pp;
            self_pp0[i] = ctx.getPet(0).skills[i].pp;
        }
        // R1：命中趟（主句固伤）
        const int foe_before_r1 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 智眼解析（命中）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::HIT,
                 "T7800.e R1 前置：无拦截命中趟");
        const int amount_r1 = audit_or(ctx, kAuditBase + 0, -1);
        CHECK(amount_r1 >= 200 && amount_r1 <= 400,
              "T7800.e R1 主句 roll ∈ [200,400]（实测 " + std::to_string(amount_r1) + "）");
        CHECK(ctx.getPet(1).hp < foe_before_r1,
              "T7800.e R1 主句固伤落地：对手掉血（前 " + std::to_string(foe_before_r1) +
                  "，后 " + std::to_string(ctx.getPet(1).hp) + "）");
        int foe_pp_r1[5];
        for (int i = 0; i < 5; ++i) {
            foe_pp_r1[i] = ctx.getPet(1).skills[i].pp;
            CHECK_EQ(foe_pp_r1[i], foe_pp0[i],
                     "T7800.e R1 命中趟 900001 未注册：对手 PP 全不动（槽 " + std::to_string(i) + "）");
        }
        // R2：注入龙威（次数型 1 次，70002 与 R1 无键冲突），同技能再选 → 无效趟
        seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/70002,
                   /*attribute=*/true, /*attack=*/true, /*count=*/1,
                   /*duration_rounds=*/0, /*penetrable=*/false);
        const int foe_after_r1 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 智眼解析（将被封）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID,
                 "T7800.e R2 前置：龙威下同技能走 SKILL_INVALID 趟");
        CHECK_EQ(ctx.getPet(1).hp, foe_after_r1,
                 "T7800.e R2 无效趟零固伤：1526 未执行（对手 hp 与 R1 结束值精确相等）");
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(ctx.getPet(1).skills[i].pp, foe_pp_r1[i] - 1,
                     "T7800.e R2 900001 扣对手全技 PP1（槽 " + std::to_string(i) +
                         "，实测 " + std::to_string(ctx.getPet(1).skills[i].pp) + "，应 " +
                         std::to_string(foe_pp_r1[i] - 1) + "）");
        }
        CHECK_EQ(audit_or(ctx, kAuditBase + 0, -1), amount_r1,
                 "T7800.e R2 审计槽不被覆盖：1526 未执行（900001 无 roll 行为）");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, self_pp0[0] - 2,
                 "T7800.e 己方槽0 两回合各扣使用 PP 一次（实测 " +
                     std::to_string(ctx.getPet(0).skills[0].pp) + "，应 " +
                     std::to_string(self_pp0[0] - 2) + "）");
        std::cout << "  [T7800.e] 端到端同场：R1 主句固伤（HIT 桶）→ R2 零固伤+900001 扣 PP"
                     "（SKILL_INVALID 桶），双桶同局各自触发 ✓\n";
    }

    std::cout << "scenario_7800 all green" << std::endl;
    return 0;
}
