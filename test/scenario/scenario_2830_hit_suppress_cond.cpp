// 场景 2830 —— K6 攻击技命中率压制+条件结算族（1314/1576/1843，
// soul_lib/k_hit_suppress_cond.cpp）（2026-10-04；号段 2830-2839 → T2830-T2835 六块）
//
// 断言口径：
//   - 1314（卡珀莉可）：乘区挂载=读 ws.hit_rate_mod_attack[1]==0.65；命中分支
//     （对手攻击命中 → 回合末回 max/3）；未命中分支用 962 注入极值（0.0 必 miss）
//     → 回合末随机 3 项 -1（恰 3 槽 -1）；
//   - 1576（菲克曼）：对手有任意升降槽 → 乘区 0.5；回合末未变化 → 兜底（随机 1 技
//     pp 归零 + 下回合先制 -1：等级 -1 翻转先手）；
//   - 1843（幕）：B 段 0.8→0.7→0.6 封顶阶梯（受击计数）；A 段高体门 → 对手先制 -1
//     + 用属性技 → 封属下回合（高速移动 R3 被 block、R4 解封可复用）。
//   ⚠️ 魂印行参数场景内不灌 db（SoulMark 手工构造 args 空）→ 实现 fallback 缺省值
//   即权威口径（文件头记档），与行参数一致。
#include <array>
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

constexpr int kPaDa  = 10004;   // 拍打：攻击载体
constexpr int kGaoSu = 20017;   // 高速移动：属（effect 4 已实现，速度+2——封属观察能手）

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 0;   // 命中率族场景：不置必中（必中绕过乘区）
        s.accuracy = 100;
    }
}

template <int MarkId>
ElfPet mark_pet(const std::array<int, 5>& skills, int speed = 300, int atk = 1,
                int hp = 99999) {
    ElfPet p = make_pet(1, "印主", skills, speed, atk, hp,
                        /*soul_mark=*/SoulMark(MarkId, "K6 魂印", ""));
    always_hit(p);
    return p;
}

ElfPet plain_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99999,
                 bool must_hit = false) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = must_hit ? 1 : 0;
        s.accuracy = 100;
    }
    return p;
}

// 962 同款：双操作提交 → debug_step 停泊 → step 过 ROUND_START（reset 已跑完）→ 注入。
void round_with_injection(ScenarioBattle& b, int self_slot, int foe_slot,
                          const std::function<void(BattleContext&)>& inject) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.ctx().debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.ctx().debug_step_mode = false;
    b.step_past(State::BATTLE_ROUND_START);
    inject(b.ctx());
    b.drive();
}

void both_act(ScenarioBattle& b, int self_slot, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

int neg_slot_count(BattleContext& ctx, int side) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] < 0) {
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

    // ══ T2830 1314 乘区挂载 + 命中分支（对手攻击命中 → 回 max/3）══
    {
        ScenarioBattle b(make_party(mark_pet<1314>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                   /*speed=*/300, /*atk=*/1, /*hp=*/99999)),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 6000;
        // 注入 1.0 覆盖 0.65（保证命中分支——乘区挂载本身由 T2833 同款读取断言）。
        round_with_injection(b, 0, 0, [](BattleContext& c) {
            c.ws.hit_rate_mod_attack[1] = 1.0f;
        });
        const int raw = ctx.ws.raw_attack_damage[1];   // 对手红伤
        CHECK(raw > 0, "T2830 前置：对手攻击命中");
        CHECK_EQ(ctx.getPet(0).hp, 6000 - raw + 33333,
                 "T2830 1314：命中 → 回合末回 max/3=33333");
        std::cout << "  [T2830] 1314 乘区/命中回血 通过\n";
    }

    // ══ T2831 1314 未命中分支（962 注入 0.0 → 必 miss → 随机 3 项 -1）══
    {
        ScenarioBattle b(make_party(mark_pet<1314>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        round_with_injection(b, 0, 0, [](BattleContext& c) {
            c.ws.hit_rate_mod_attack[1] = 0.0f;   // 极值：必 miss
        });
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T2831 前置：对手攻击被闪（不掉血）");
        CHECK_EQ(level_sum(ctx, 1), -3, "T2831 1314：未命中 → 随机 3 项 -1（sum=-3）");
        CHECK_EQ(neg_slot_count(ctx, 1), 3, "T2831 1314：恰 3 个负槽");
        std::cout << "  [T2831] 1314 未命中降槽 通过\n";
    }

    // ══ T2832 1576 乘区条件挂载 + 未变化兜底（PP 归零 + 下回合先制 -1）══
    {
        ScenarioBattle b(make_party(mark_pet<1576>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                   /*speed=*/100, /*atk=*/1)),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/101)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：双方攻击（对手无升降槽 → 乘区不挂、回合末未变化 → 兜底）
        both_act(b, 0);
        CHECK_EQ(ctx.ws.hit_rate_mod_attack[1], 1.0f,
                 "T2832 1576：对手无升降槽 → 乘区不挂");
        int pp_total = 0;
        int pp_zero_slots = 0;
        for (const Skills& s : ctx.seerRobot[1].elfPets[0].skills) {
            pp_total += s.pp;
            if (s.pp == 0) {
                ++pp_zero_slots;
            }
        }
        CHECK_EQ(pp_zero_slots, 1, "T2832 1576 兜底：随机 1 技 PP 归零");
        (void)pp_total;
        for (Skills& sk : ctx.seerRobot[1].elfPets[0].skills) {
            if (sk.pp >= 0) {
                sk.pp = 99;   // 兜底 PP 归零会拒掉对手出技（提交校验），回满再驱动 R2
            }
        }
        both_act(b, 0);   // R2：对手下回合先制 -1 生效（pl=-1 < 我 0 → 我先手）
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T2832 1576：下回合所有技能先制 -1（等级翻转先手）");
        std::cout << "  [T2832] 1576 未变化兜底 通过\n";
    }

    // ══ T2833 1576 乘区条件挂载（对手有升降槽 → 0.5）══
    {
        ScenarioBattle b(make_party(mark_pet<1576>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                   /*speed=*/100, /*atk=*/1)),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, 0, 1) == StatChangeResult::SUCCESS);   // 对手攻 +1
        both_act(b, 0);   // R1 快照（含 +1）→ 乘区挂载
        CHECK(ctx.ws.hit_rate_mod_attack[1] < 1.0f,
              "T2833 1576：对手有升降槽 → 乘区挂载（0.5）");
        std::cout << "  [T2833] 1576 条件挂载 通过\n";
    }

    // ══ T2834 1843 B 段阶梯（0.8 → 0.7 → 0.6 封顶）══
    {
        ScenarioBattle b(make_party(mark_pet<1843>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/1, /*hp=*/99999,
                                              /*must_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0);   // R1：对手打我 1 次 → 计数 1（乘区本回合 0.8）
        CHECK(ctx.ws.hit_rate_mod_attack[1] < 0.9f,
              "T2834 R1 1843：登场基础 0.8（<0.9）");
        both_act(b, 0);   // R2：再挨 1 次 → 0.7
        CHECK(ctx.ws.hit_rate_mod_attack[1] < 0.75f, "T2834 R2 1843：受击递减 → 0.7");
        both_act(b, 0);   // R3：0.6
        both_act(b, 0);   // R4：封顶仍 0.6
        CHECK(ctx.ws.hit_rate_mod_attack[1] < 0.65f, "T2834 R4 1843：封顶 0.6（<0.65）");
        std::cout << "  [T2834] 1843 B 段阶梯 通过\n";
    }

    // ══ T2835 1843 A 段高体门（先制 -1 + 封属下回合 + 解封复用）══
    // 对手 slots={高速移动, 拍打×4}：R1 对手放高速移动（+2，门开 → 回合末封属覆盖
    // R2）；R2 对手改放拍打（属性技被封"无法使用"——提交校验拒掉会卡死回合，记档）
    // → R3 封属过期 → 高速移动复用 +2（spd 4）。
    {
        ScenarioBattle b(make_party(mark_pet<1843>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                   /*speed=*/100, /*atk=*/1,
                                                   /*hp=*/99999)),
                         make_party(plain_pet({kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/101, /*atk=*/1, /*hp=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0);   // R1：对手先手放高速移动（+2）；我高体门开 → FMR 对手先制-1
        CHECK_EQ(ctx.ability_levels[1][4], 2, "T2835 R1：对手高速移动 +2");
        both_act(b, 1, 1);   // R2：对手先制-1 生效（我先手）+ 对手拍打（属性技被封）
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T2835 1843：高体门 → 对手先制 -1（我先手）");
        both_act(b, 0, 0);   // R3：封属过期 → 对手高速移动复用
        CHECK_EQ(ctx.ability_levels[1][4], 4,
                 "T2835 1843：R3 封属过期 → 高速移动复用（+4）");
        std::cout << "  [T2835] 1843 A 段高体门/封属 通过\n";
    }

    std::cout << "scenario_2830 PASS (K6 命中压制+条件结算族)" << std::endl;
    return 0;
}
