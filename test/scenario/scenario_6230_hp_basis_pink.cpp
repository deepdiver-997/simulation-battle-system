// 场景 6230 —— J290 体力基准粉伤族（1229 / 673 / 1045，moves_lib/j_hp_basis_pink.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6230-6239）
//
// 断言口径：
//   - 三条都是"附加百分比伤害"→ 粉伤落点。红伤噪声用
//     `loss == ctx.resolvedDamage.final + 期望粉伤` 剥离（1440/1380 同法）。
//   - 1229（33517 魔水天倾 / 35350 暗蛇獠牙，纯链 [3]）：自身体力**严格高于**对手才触发；
//     粉伤 = 自身当前体力/3。门闭（低于/相等）绝对值：loss 只剩红伤、审计槽 0。
//   - 673（17238 极电狂龙祭 [0,25] / 36911 星汉玄音 [5,30]）：门 = 对手**不处于**指定
//     异常状态（只判指定码，其它异常不关闸）；粉伤 = (max-cur)×pct/100。
//     门闭用麻痹(0)/冻伤(5)直接施加（不扣血、不污染 loss）。
//   - 1045（30247 暴君狂怒吼 纯链 [3]）：SKILL_EFFECT 注册、AFTER_ACTION 结算；
//     未击败 → 自身 max/3；击败局门关（审计槽缺省 0，loss 只剩红伤）。
//   - 审计槽记录本次实际粉伤/已损失体力（无随机），与 hp 差值双向核对。
#include <any>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物 40（填充刀）
constexpr int kMoShui = 33517;  // 魔水天倾 [1229]|[3] 特140
constexpr int kAnShe = 35350;   // 暗蛇獠牙 [1229]|[3] 物140
constexpr int kJiDian = 17238;  // 极电狂龙祭 [673]|[0,25] 物160（门=麻痹 0）
constexpr int kXingHan = 36911; // 星汉玄音 [673]|[5,30] 特130（门=冻伤 5）
constexpr int kBaoJun = 30247;  // 暴君狂怒吼 [1045]|[3] 物150

constexpr int k1229Pink = 991229000;
constexpr int k673Pink = 996730000;
constexpr int k673Lost = 996730100;
constexpr int k1045Pink = 991045000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int skill, int hp, int atk = 0) {
    ElfPet p = make_pet(1, "己方", {skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(int max_hp = 10000, int cur_hp = -1, int atk = 1) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, /*hp=*/max_hp);
    if (cur_hp >= 0) {
        p.hp = cur_hp;   // make_pet 的 hp 同时是 max；单独压当前体力（坑集 §4）
    }
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

void run(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6230 1229 自身体力高于对手 → 自身当前体力 1/{0} 粉伤 ══
    {
        {   // (a) 自身 4000 > 对手 3000 → 粉伤 4000/3=1333
            ScenarioBattle b(make_party(mk_self(kMoShui, /*hp=*/5000)),
                             make_party(mk_foe(/*max=*/10000, /*cur=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4000;
            run(b);
            CHECK_EQ(stor(ctx, k1229Pink), 1333, "T6230.a 1229：自身 4000/3 粉伤 1333");
            CHECK_EQ(3000 - ctx.getPet(1).hp, ctx.resolvedDamage.final + 1333,
                     "T6230.a 1229：对手 loss = 红 + 1333");
        }
        {   // (b) 门闭：自身 2000 < 对手 3000 → 无粉伤
            ScenarioBattle b(make_party(mk_self(kMoShui, /*hp=*/5000)),
                             make_party(mk_foe(/*max=*/10000, /*cur=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 2000;
            run(b);
            CHECK_EQ(stor(ctx, k1229Pink), 0, "T6230.b 1229 门：体力低于对手 → 无粉伤");
            CHECK_EQ(3000 - ctx.getPet(1).hp, ctx.resolvedDamage.final,
                     "T6230.b 1229 门：对手 loss 只剩红伤");
        }
        {   // (c) 边界：双方体力相等 → "高于"严格不成立
            ScenarioBattle b(make_party(mk_self(kMoShui, /*hp=*/5000)),
                             make_party(mk_foe(/*max=*/10000, /*cur=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 3000;
            run(b);
            CHECK_EQ(stor(ctx, k1229Pink), 0, "T6230.c 1229 边界：体力相等不触发（严格高于）");
            CHECK_EQ(3000 - ctx.getPet(1).hp, ctx.resolvedDamage.final,
                     "T6230.c 1229 边界：对手 loss 只剩红伤");
        }
        {   // (d) 第二载体 35350（同宏同参）：自身 4500/3=1500
            ScenarioBattle b(make_party(mk_self(kAnShe, /*hp=*/5000)),
                             make_party(mk_foe(/*max=*/10000, /*cur=*/2000)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4500;
            run(b);
            CHECK_EQ(stor(ctx, k1229Pink), 1500, "T6230.d 1229 载体 35350：4500/3=1500");
            CHECK_EQ(2000 - ctx.getPet(1).hp, ctx.resolvedDamage.final + 1500,
                     "T6230.d 1229 载体 35350：对手 loss = 红 + 1500");
        }
        std::cout << "  [T6230] 1229 体力优势门粉伤 通过\n";
    }

    // ══ T6231 673 对手不处于{0}状态 → 已损失体力{1}% 粉伤 ══
    {
        {   // (a) 对手 6000/10000 无异常 → lost 4000×25%=1000
            ScenarioBattle b(make_party(mk_self(kJiDian, /*hp=*/5000)),
                             make_party(mk_foe(/*max=*/10000, /*cur=*/6000)));
            b.start();
            BattleContext& ctx = b.ctx();
            run(b);
            CHECK_EQ(stor(ctx, k673Lost), 4000, "T6231.a 673：已损失体力基数 4000");
            CHECK_EQ(stor(ctx, k673Pink), 1000, "T6231.a 673：4000×25%=1000 粉伤");
            CHECK_EQ(6000 - ctx.getPet(1).hp, ctx.resolvedDamage.final + 1000,
                     "T6231.a 673：对手 loss = 红 + 1000");
        }
        {   // (b) 门闭：对手麻痹(0) → 指定状态命中，无粉伤
            ScenarioBattle b(make_party(mk_self(kJiDian, /*hp=*/5000)),
                             make_party(mk_foe(/*max=*/10000, /*cur=*/6000)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.apply_abnormal_status_for_rounds(
                1, static_cast<int>(AbnormalStatusId::Paralysis), 3);   // 绕抗性，确定落地
            run(b);
            CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
                  "T6231.b 前置：对手确处于麻痹");
            CHECK_EQ(stor(ctx, k673Pink), 0, "T6231.b 673 门：对手处于麻痹 → 无粉伤");
            CHECK_EQ(6000 - ctx.getPet(1).hp, ctx.resolvedDamage.final,
                     "T6231.b 673 门：对手 loss 只剩红伤");
        }
        {   // (c) 只判指定码：对手睡眠(8) ≠ 麻痹(0) → 门仍开，粉伤 1000
            ScenarioBattle b(make_party(mk_self(kJiDian, /*hp=*/5000)),
                             make_party(mk_foe(/*max=*/10000, /*cur=*/6000)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.apply_abnormal_status_for_rounds(
                1, static_cast<int>(AbnormalStatusId::Sleep), 3);
            run(b);
            CHECK_EQ(stor(ctx, k673Pink), 1000, "T6231.c 673：其它异常不关闸（只判 {0}）");
        }
        {   // (d) 第二载体 36911 [5,30]：lost 4000×30%=1200
            ScenarioBattle b(make_party(mk_self(kXingHan, /*hp=*/5000)),
                             make_party(mk_foe(/*max=*/10000, /*cur=*/6000)));
            b.start();
            BattleContext& ctx = b.ctx();
            run(b);
            CHECK_EQ(stor(ctx, k673Pink), 1200, "T6231.d 673 载体 36911：4000×30%=1200");
            CHECK_EQ(6000 - ctx.getPet(1).hp, ctx.resolvedDamage.final + 1200,
                     "T6231.d 673 载体 36911：对手 loss = 红 + 1200");
        }
        {   // (e) 36911 门闭：对手冻伤(5) → 无粉伤
            ScenarioBattle b(make_party(mk_self(kXingHan, /*hp=*/5000)),
                             make_party(mk_foe(/*max=*/10000, /*cur=*/6000)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.apply_abnormal_status_for_rounds(
                1, static_cast<int>(AbnormalStatusId::Frostbite), 3);
            run(b);
            CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Frostbite)),
                  "T6231.e 前置：对手确处于冻伤");
            CHECK_EQ(stor(ctx, k673Pink), 0, "T6231.e 673 门：对手处于冻伤 → 无粉伤");
            // 冻伤有回合末自伤 tick，loss 不宜与红伤等式对齐；门闭以审计槽 0 为准
            // （同参开闸局 T6231.d 实测 1200，构成对照）。
        }
        {   // (f) 满血 → 已损失 0 → 粉伤 0
            ScenarioBattle b(make_party(mk_self(kJiDian, /*hp=*/5000)),
                             make_party(mk_foe(/*max=*/10000)));
            b.start();
            BattleContext& ctx = b.ctx();
            run(b);
            CHECK_EQ(stor(ctx, k673Lost), 0, "T6231.f 673：满血 → 已损失体力 0");
            CHECK_EQ(stor(ctx, k673Pink), 0, "T6231.f 673：满血 → 无粉伤");
        }
        std::cout << "  [T6231] 673 非异常门已损失体力% 粉伤 通过\n";
    }

    // ══ T6232 1045 未击败对手 → 自身最大体力 1/{0} 粉伤（AFTER_ACTION） ══
    {
        {   // (a) max 5000，对手存活 → 5000/3=1666
            ScenarioBattle b(make_party(mk_self(kBaoJun, /*hp=*/5000)),
                             make_party(mk_foe(/*max=*/10000)));
            b.start();
            BattleContext& ctx = b.ctx();
            run(b);
            CHECK(ctx.getPet(1).hp > 0, "T6232.a 前置：对手存活（未击败）");
            CHECK_EQ(stor(ctx, k1045Pink), 1666, "T6232.a 1045：5000/3=1666 粉伤");
            CHECK_EQ(10000 - ctx.getPet(1).hp, ctx.resolvedDamage.final + 1666,
                     "T6232.a 1045：对手 loss = 红 + 1666");
        }
        {   // (b) max 9999 → 3333（取整向下），验证读的是最大体力
            ScenarioBattle b(make_party(mk_self(kBaoJun, /*hp=*/9999)),
                             make_party(mk_foe(/*max=*/20000)));
            b.start();
            BattleContext& ctx = b.ctx();
            run(b);
            CHECK_EQ(stor(ctx, k1045Pink), 3333, "T6232.b 1045：9999/3=3333（最大体力分母）");
            CHECK_EQ(20000 - ctx.getPet(1).hp, ctx.resolvedDamage.final + 3333,
                     "T6232.b 1045：对手 loss = 红 + 3333");
        }
        {   // (c) 击杀局：门关 → 无粉伤，审计槽缺省 0
            ScenarioBattle b(make_party(mk_self(kBaoJun, /*hp=*/5000, /*atk=*/3000)),
                             make_party(mk_foe(/*max=*/2000)));
            b.start();
            BattleContext& ctx = b.ctx();
            run(b);
            CHECK(ctx.getPet(1).hp <= 0, "T6232.c 前置：对手被击败");
            CHECK_EQ(stor(ctx, k1045Pink), 0, "T6232.c 1045 门：击败局不触发粉伤");
            CHECK(ctx.resolvedDamage.final >= 2000,
                  "T6232.c 1045 门：击杀红伤 ≥ 对手最大体力");
        }
        std::cout << "  [T6232] 1045 未击败门最大体力 1/{0} 粉伤 通过\n";
    }

    std::cout << "scenario_6230_hp_basis_pink PASS" << std::endl;
    return 0;
}
