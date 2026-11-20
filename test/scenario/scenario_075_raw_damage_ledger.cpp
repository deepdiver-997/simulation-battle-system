// 场景 075 —— **裸伤台账**（`ws.raw_attack_damage`）+ effect 422「附加所造成伤害值{0}%的固定伤害」
//
// 背景（用户 2026-09-18 实测）：鬼地行者的"晦能入侵"（38480，[1471,422]）里那条附加固伤，
// 检测的是**裸伤**——"管线后面的挡伤、锁伤所改变的红伤都不是被这个效果记录的"。
// 而本引擎里"按伤害比例"的效果过去统一读 `resolvedDamage.final`（**管线后**的值），
// 所以这一族必须改读新台账。一次攻击在内存里其实有**三个**不同的值：
//   ① 裸伤    = 公式值（含暴击/浮动/连击/次数增伤）+ **变威力重算后的第二次结果**，不含管线
//               → `ws.raw_attack_damage[攻方]`（本场景的被测对象）
//   ② 管线后  = `resolvedDamage.final`（增伤/减伤/保底/锁伤/挡伤都已作用）
//   ③ 实际落血= `EVENT_TAKE_DAMAGE.amount`（再扣护盾/护罩）
//
// 唯一写入点 = `finish_attack_damage` 里 `apply_variable_power_recalc` **之后**、
// `damage_pipeline_.run` **之前**——场景 D 组专门锁"变威力重算后台账必须更新"。
//
// 组：
//   A 基线：无修正 → 台账 == 管线后值；422 的粉伤 == 台账×pct
//   B 锁伤(CAP) / 挡伤(BLOCK)：红伤被改小/归零，**台账与 422 的粉伤不变**
//   C 点数减伤(REDUCE_FLAT)：同上（把"写入点在管线前"钉死；若实测减伤该进，改写入点+本组）
//   D **变威力重算**：先出手威力2倍（effect 40）→ 台账 == 重算后的值（不是被推翻的第一次）

#include <effects/damage_pipeline.h>
#include <entities/pet_factory.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kChuanYunJian = 16413;   // 穿云箭：[422] args=[20] → 附加裸伤 20% 固伤
constexpr int kJiFengZhuiJi = 10260;   // 疾风追击：[40] 先出手时威力2倍（变威力）
constexpr int kZhuangJi = 10001;       // 撞击

std::array<int, 5> skills_with(int first, int second) {
    return {first, second, kZhuangJi, kZhuangJi, kZhuangJi};
}

ElfPet attacker(int speed, int atk = 300) {
    return make_pet(0, "攻方", skills_with(kChuanYunJian, kJiFengZhuiJi), speed, atk,
                    /*hp=*/100000);
}

ElfPet defender(int speed, int hp = 50000) {
    return make_pet(1, "守方", skills_with(kZhuangJi, kZhuangJi), speed, /*atk=*/100, hp);
}

// 本次结算的双值探针：挂在管线 DETECT 阶段（此时台账已写、管线已跑完）。
// 本回合落在守方身上的粉伤累计（422 的附加固伤走粉伤通道）
int& pink_total() {
    static int v = 0;
    return v;
}

struct Probe {
    int raw = -1;     // ws.raw_attack_damage（裸伤台账）
    int pre = -1;     // 管线**首阶段**（GUARD_DETECT）抓到的 final == 公式值（同笔结算的基准）
    int post = -1;    // 管线末段（DETECT）的 final == 管线后值
};
// ⚠️ 不要拿 `resolvedDamage.base` 当基准：挡伤类阶段会把它一并清零（B2 踩到）。
// ⚠️ Probe 必须由**调用方**持有：lambda 按引用捕获它，若捕获的是本函数内的局部量，
//    函数一返回就成了悬垂引用（写进去也读不到，还会是 UB）。
void install_probe(BattleContext& ctx, int attacker_side, Probe& p) {
    // 首阶段：此刻还没人改过 final → 抓到的就是公式值（与台账同源）
    ctx.register_damage_effect(
        DamagePhase::GUARD_DETECT, attacker_side, DamageEffectCategory::DETECT,
        [&p, attacker_side](BattleContext* c, int) {
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId == attacker_side && d.isRed && d.final > 0) {
                p.pre = d.final;
                p.raw = c->ws.raw_attack_damage[attacker_side];
            }
        });
    ctx.register_damage_effect(
        DamagePhase::DETECT, attacker_side, DamageEffectCategory::DETECT,
        [&p, attacker_side](BattleContext* c, int) {
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != attacker_side || !d.isRed) {
                return;
            }
            if (d.final > 0 || c->ws.raw_attack_damage[attacker_side] > 0) {
                p.post = d.final;
            }
        });
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto make_battle = [](int atk_speed, int def_speed) {
        return ScenarioBattle(make_party(attacker(atk_speed)), make_party(defender(def_speed)));
    };

    // ════════════════════════════════════════════════════════════════
    // A. 基线：无任何修正 → 台账 == 管线后值；422 粉伤 == 台账 × 20%
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b = make_battle(/*atk=*/300, /*def=*/100);   // 攻方先手
        b.start();
        BattleContext& ctx = b.ctx();
        Probe p;
        install_probe(ctx, 0, p);
        pink_total() = 0;
        ctx.event_center_.register_watcher(
            EventType::EVENT_TAKE_PINK_DAMAGE, 0, ctx.roundCount, 0, false,
            [](BattleContext*, const BattleEvent& ev) {
                if (ev.target == 1 && ev.amount > 0) {
                    pink_total() += ev.amount;
                }
            },
            WatcherScope::TEAM);

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 穿云箭（含 422）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK(p.raw > 0, "A) 台账应被写入（>0）");
        CHECK_EQ(p.raw, p.pre, "A) 台账 == 公式值（管线首阶段的值）");
        CHECK_EQ(p.raw, p.post, "A) 无修正时：裸伤 == 管线后值");
        CHECK_EQ(pink_total(), p.raw * 20 / 100, "A) 422 附加固伤 == 裸伤 × 20%");
        std::cout << "  [A] 基线：裸伤=" << p.raw << " 粉伤=" << pink_total() << " 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B1. **锁伤 (CAP)**：红伤被锁到 50 → 台账/422 粉伤不受影响
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        Probe p;
        install_probe(ctx, 0, p);
        pink_total() = 0;
        ctx.event_center_.register_watcher(
            EventType::EVENT_TAKE_PINK_DAMAGE, 0, ctx.roundCount, 0, false,
            [](BattleContext*, const BattleEvent& ev) {
                if (ev.target == 1 && ev.amount > 0) pink_total() += ev.amount;
            },
            WatcherScope::TEAM);
        // 守方桶的 CAP 阶段：把最终伤害锁到 50
        ctx.register_damage_effect(
            DamagePhase::CAP, /*owner=*/1, DamageEffectCategory::MITIGATE,
            [](BattleContext* c, int) {
                if (c->resolvedDamage.defenderId == 1 && c->resolvedDamage.final > 50) {
                    c->resolvedDamage.final = 50;
                }
            });

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK_EQ(p.post, 50, "B1) 锁伤生效：管线后红伤 == 50");
        CHECK(p.post != p.raw, "B1) 前置：管线后值已被锁伤改变");
        CHECK_EQ(p.raw, p.pre, "B1) 台账**不受锁伤影响**（== 公式值）");
        CHECK_EQ(pink_total(), p.raw * 20 / 100,
                 "B1) 422 粉伤按**裸伤**算（若误读管线后值会得到 50×20%=10）");
        std::cout << "  [B1] 锁伤 50：红伤 " << p.post << " 但台账 " << p.raw
                  << "/粉伤 " << pink_total() << " 不变 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B2. **挡伤 (BLOCK，免红伤)**：红伤归零 → 台账/422 粉伤仍不受影响
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        Probe p;
        install_probe(ctx, 0, p);
        pink_total() = 0;
        ctx.event_center_.register_watcher(
            EventType::EVENT_TAKE_PINK_DAMAGE, 0, ctx.roundCount, 0, false,
            [](BattleContext*, const BattleEvent& ev) {
                if (ev.target == 1 && ev.amount > 0) pink_total() += ev.amount;
            },
            WatcherScope::TEAM);
        // 守方"免红伤"票（IMMUNE DAMAGE 在管线 BLOCK 阶段消费）
        ctx.grant_immunity(/*owner=*/1, ImmunityType::DAMAGE, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/1, /*source_id=*/90075);

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK_EQ(p.post, 0, "B2) 挡伤生效：管线后红伤 == 0");
        CHECK_EQ(p.raw, p.pre, "B2) 台账**不受挡伤影响**（== 公式值）");
        CHECK_EQ(pink_total(), p.raw * 20 / 100,
                 "B2) 挡伤归零也照打 422 的附加固伤（按裸伤）");
        std::cout << "  [B2] 挡伤归零：红伤 0 但台账 " << p.raw << "/粉伤 " << pink_total()
                  << " 不变 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. **点数减伤**：红伤变小 → 台账/422 粉伤仍不受影响（把写入点钉在管线前）
    //    ⚠️ 若将来实测发现"减伤会减少附加固伤"，则应把写入点移到管线 REDUCE_PCT 之后，
    //       本组断言随之调整（写入点 = 机制定义，见 battleWorkspace.h）。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b = make_battle(300, 100);
        b.start();
        BattleContext& ctx = b.ctx();
        Probe p;
        install_probe(ctx, 0, p);
        pink_total() = 0;
        ctx.event_center_.register_watcher(
            EventType::EVENT_TAKE_PINK_DAMAGE, 0, ctx.roundCount, 0, false,
            [](BattleContext*, const BattleEvent& ev) {
                if (ev.target == 1 && ev.amount > 0) pink_total() += ev.amount;
            },
            WatcherScope::TEAM);
        ctx.register_damage_effect(
            DamagePhase::REDUCE_FLAT, /*owner=*/1, DamageEffectCategory::MITIGATE,
            [](BattleContext* c, int) {
                if (c->resolvedDamage.defenderId == 1) {
                    c->resolvedDamage.final = std::max(0, c->resolvedDamage.final - 100);
                }
            });

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK_EQ(p.post, std::max(0, p.raw - 100), "C) 点数减伤生效（管线后 = 公式值 - 100）");
        CHECK_EQ(p.raw, p.pre, "C) 台账不受点数减伤影响");
        CHECK_EQ(pink_total(), p.raw * 20 / 100, "C) 422 粉伤不受点数减伤影响（按裸伤）");
        std::cout << "  [C] 点数减伤：红伤 " << p.post << " 但台账 " << p.raw << " 不变 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. **变威力重算**：疾风追击（[40] 先出手威力2倍）
    //    台账必须在**重算之后**写——否则记的是被推翻的第一次结果。
    //    D1 攻方后手（条件不成立、无重算）取基准；D2 攻方先手（威力2倍 + 重算）。
    // ════════════════════════════════════════════════════════════════
    {
        // D1 攻方速度 50 < 守方 200 → 后手 → effect 40 条件不成立 → 无重算
        ScenarioBattle b1 = make_battle(/*atk=*/50, /*def=*/200);
        b1.start();
        Probe p1;
        install_probe(b1.ctx(), 0, p1);
        b1.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 疾风追击
        b1.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b1.drive();
        CHECK(p1.raw > 0, "D1) 后手：台账已写入");
        CHECK_EQ(p1.raw, p1.pre, "D1) 台账 == 公式值");
        CHECK_EQ(p1.raw, p1.post, "D1) 无重算时台账 == 管线后值");

        // D2 攻方速度 300 > 守方 100 → 先手 → 威力2倍 → **变威力重算**
        ScenarioBattle b2 = make_battle(/*atk=*/300, /*def=*/100);
        b2.start();
        Probe p2;
        install_probe(b2.ctx(), 0, p2);
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK(p2.raw > 0, "D2) 先手：台账已写入");
        CHECK_EQ(p2.raw, p2.pre, "D2) 台账 == 公式值（重算后的第二次结果）");
        CHECK_EQ(p2.raw, p2.post,
                 "D2 ★ 台账 == 重算后的值（若写在重算之前，这里会拿到被推翻的第一次结果）");
        CHECK(p2.raw > p1.raw * 3 / 2,
              "D2) 先出手威力2倍 → 台账显著大于后手基准（证明重算结果被记录下来）");
        std::cout << "  [D] 变威力重算：后手 " << p1.raw << " → 先手(×2) " << p2.raw
                  << "，台账取重算后值 通过\n";
    }

    std::cout << "scenario_075 PASS（裸伤台账：挡伤/锁伤/减伤都不进；变威力重算后更新）\n";
    return 0;
}
