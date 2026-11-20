// 场景 110 —— 启灵元神 3986 魂印全条（神印之外的四条）+ 第五技 神游·众生归渺
//
// 口径来源：语料 idx=439《启灵元神魂印补全》/ idx=460《机制解析—启灵元神》
// / 4399 图鉴 907369（第五技"大量真实伤害"的内置公式）。
//   A [神殇]：回合开始重掷对手放逐集（不在场存活；存活-2 上限）——被放逐不能
//     **主动**切换上场（死亡补位 is_forced 绕过）；位图发布供 1534 读取
//   B [神谕]：己方场下精灵体力 < 最大体力 1/2 → 体力不可削弱（off_field_stats_protected）
//   C [神照]：己方首发精灵每次登场 → 盾 = 启灵最大体力 30%（下场不清，站场才被磨）
//   D [神启]：高体力免控免弱（R1 游戏开始至任一方首次命中 / R2 登场 / R3 每回合开始，
//     都"延续至下回合开始、下场保留"）；低体力（R4）每回合开始自盾 30%
//   E 第五技 33894：1533 伤害 < 350 → 追加真伤 =（350 − 伤害）× 1.2（4399 内置公式）；
//     1534 打被放逐精灵 PP-1，无被放逐 → 打当前精灵全部 PP-1
//
// ⚠️ 队伍构造：**只放一只 1581 holder**（make_party 会整队复制，多 holder 会让
//    deploy/神殇按份数重复结算）。E 组攻击方不带魂印（技能全局共享，无需 holder；
//    且 E4 的手动放逐位图不能被神殇重掷覆盖）。

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "../../resources/qiling_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10162;       // 飞空俯冲（必中攻击 90）
constexpr int kGaoSu = 20017;     // 高速移动（威力 0 属性技，0 伤害 → 盾不被磨）
constexpr int kShenYou = 33894;   // 神游·众生归渺（第五技）
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);
constexpr int kHolderMax = 4000;

std::array<int, 5> same5(int id = kHit) { return {id, id, id, id, id}; }

SoulMark qiling_mark() { return SoulMark(1581, "启灵元神", "神殇/神印/神谕/神照/神启"); }

ElfPet qiling_pet(int speed, int hp = kHolderMax) {
    return make_pet(3986, "启灵元神", same5(), speed, /*atk=*/100, hp, qiling_mark());
}

ElfPet teammate(int speed) {
    return make_pet(3987, "队友", same5(kGaoSu), speed, /*atk=*/1, 9000);
}

ElfPet plain_pet(int speed, int atk) {
    return make_pet(1, "守方", same5(), speed, atk, 9000);
}

// 单 holder 队伍：on_stage 指定 holder 在场还是场下（1 号位），其余队友补位
std::array<ElfPet, 6> holder_party(int speed, bool holder_on_stage) {
    std::array<ElfPet, 6> party{qiling_pet(speed), teammate(speed), teammate(speed),
                                teammate(speed), teammate(speed), teammate(speed)};
    if (!holder_on_stage) {
        std::swap(party[0], party[1]);   // 0=队友（首发），1=启灵（场下）
    }
    return party;
}

void play_round_all_attack(ScenarioBattle& b) {
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

    // ════════════════════════════════════════════════════════════════
    // A [神殇]：放逐重掷 + 被放逐不能主动切换上场
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(holder_party(/*speed=*/100, /*holder_on_stage=*/true),
                         make_party(plain_pet(100, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_all_attack(b);
        const int mask = qiling::exile_mask(ctx.plugin_storage, /*side=*/1);
        CHECK(mask != 0, "A1) ★ 神殇放逐了对手场下精灵（6 存活 ≥5 → 放逐 1-3 个，位图非零）");
        int exiled = -1;
        int free_slot = -1;
        for (int slot = 0; slot < 6; ++slot) {
            if (slot == ctx.on_stage[1]) {
                continue;
            }
            if (mask & (1 << slot)) {
                exiled = slot;
            } else if (ctx.seerRobot[1].elfPets[slot].hp > 0 && free_slot < 0) {
                free_slot = slot;
            }
        }
        CHECK(exiled >= 0 && free_slot >= 0, "A) 前置：放逐槽与非放逐槽都存在");
        const int prev_stage = ctx.on_stage[1];
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, exiled);
        CHECK_EQ(ctx.on_stage[1], prev_stage,
                 "A2) ★ 被放逐的精灵**不能主动切换上场**（is_locked 位）");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, free_slot);
        CHECK_EQ(ctx.on_stage[1], free_slot, "A3) 未被放逐的精灵照常切换（对照）");
        std::cout << "  [A] 神殇·放逐与主动切换封锁 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B [神谕]：己方场下精灵半血以下 → 体力不可削弱
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(holder_party(100, true), make_party(plain_pet(100, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ElfPet& bench = ctx.seerRobot[0].elfPets[1];
        const int bench_max = bench.numericalBase[NumericalPropertyIndex::HP];   // 9000
        bench.hp = bench_max * 2 / 5;   // 40% < 1/2
        CHECK(ctx.off_field_stats_protected(0, 1),
              "B1) ★ 场下半血以下 → 体力不可削弱（防炸背包）");
        bench.hp = bench_max * 4 / 5;   // 80% ≥ 1/2
        CHECK(!ctx.off_field_stats_protected(0, 1), "B2) 半血以上不保护（条件门）");
        CHECK(!ctx.off_field_stats_protected(0, ctx.on_stage[0]),
              "B3) 在场精灵不归神谕管（只护场下）");
        std::cout << "  [B] 神谕·场下半血免削 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C [神照]：首发精灵登场获得启灵最大体力 30% 的盾；再登场再获得（下场不清）
    // ⚠️ deploy 节点在 PROTECTION_1（每回合链里、两次 submit 之后）→ 断言在首轮后
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(holder_party(100, /*holder_on_stage=*/false),
                         make_party(make_pet(1, "守方", same5(kGaoSu), 100, 1, 9000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int expected = kHolderMax * 30 / 100;   // 1200
        play_round_all_attack(b);   // 双方 0 伤害属性技 → 盾不被磨
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].shield_bank_.total(), expected,
                 "C1) ★ 首发登场即获得盾（启灵最大体力 4000 × 30% = 1200）");
        // 首发切走再切回：盾下场不清 + 再登场再获得一个 → 2400
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 2);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].shield_bank_.total(), expected * 2,
                 "C2) ★ 盾下场不清 + 再登场再获得（小草王式盾逻辑，idx=439）");
        std::cout << "  [C] 神照·首发登场盾 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D [神启]：高体力免控免弱（R1/R3）；低体力自盾（R4）
    // 口径（2026-09-26 用户拍板）：免控**只保 holder 本人**——R3 票为 TEAM + condition
    // 在场守卫；holder 场下时在场的队友【不】享受（旧 side 级查询会误保护队友，已修）。
    // ════════════════════════════════════════════════════════════════
    {
        // holder 场下站（不挨打 → 神照盾不干扰数值断言；同时锁"下场不护队友"反向断言）
        ScenarioBattle b(holder_party(/*speed=*/300, /*holder_on_stage=*/false),
                         make_party(plain_pet(100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_all_attack(b);   // 有技能命中 → R1 撤销
        // R3 的窗口 = "延续至下回合的回合开始时"：上一轮的票恰在本回合开始时到期、
        // 本回合 ROUND_START 重授 → 断言必须落在 ROUND_START 之后（回合间的 CHOOSE
        // 停泊点是"已过期未重授"的死区）。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;   // 夹在两次 submit 之间
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(!ctx.is_immune(0, ImmunityType::ANOMALY, ctx.currentState),
              "D1) R3 只保本体：holder 场下 → 在场队友不享受免控（免误保护）");
        CHECK(apply_anomaly(&ctx, /*target=*/0, kCurse, /*dur=*/3, /*actor=*/1)
                  == ApplyAnomalyResult::SUCCESS, "D1) 诅咒落到队友身上（旧口径对照）");
        b.drive();                  // 跑完本回合
        // holder 换上场 → 下一回合 ROUND_START 重授后，本体在场被保护
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(ctx.is_immune(0, ImmunityType::ANOMALY, ctx.currentState),
              "D2) ★ R3：每回合开始高体力重授免控免弱（holder 在场 → 本体被保护）");
        CHECK(apply_anomaly(&ctx, /*target=*/0, kCurse, /*dur=*/3, /*actor=*/1)
                  != ApplyAnomalyResult::SUCCESS, "D2) 免疫确生效：诅咒施加被挡");
        b.drive();                  // 跑完本回合
        ctx.seerRobot[0].elfPets[1].hp = 1000;    // 4000 的 1/4 < 1/2 → 转 R4 档
        play_round_all_attack(b);   // 本回合 ROUND_START：低体力 → R4 自盾、不授免控
        CHECK(!ctx.is_immune(0, ImmunityType::ANOMALY, ctx.currentState),
              "D3) ★ 低体力 → 不再免控（R1 已被首次命中撤销 + R3/R4 互斥）");
        // R4 盾 1200 在本回合 ROUND_START 落账后，对手攻击当回合打在场上 holder、
        // 已被盾吸收 ~2 点 → 用小容差断言（holder 在场是 D1 新口径的必然结果）。
        const int shield_total = ctx.seerRobot[0].elfPets[1].shield_bank_.total();
        CHECK(shield_total <= kHolderMax * 30 / 100
                  && shield_total >= kHolderMax * 30 / 100 - 4,
              "D4) ★ R4：回合开始低体力 → 自盾 = 自身最大体力 30%（当回合已被吸收少量）");
        std::cout << "  [D] 神启·免控只保本体（新口径）/低体力自盾 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E 第五技 33894：1533 低伤补真（内置公式）+ 1534 放逐扣 PP
    // （攻击方不带魂印——技能全局共享；E4 的手动位图不能被神殇重掷覆盖）
    // ════════════════════════════════════════════════════════════════
    {
        auto make_low = [] {
            return make_pet(3986, "启灵低攻", same5(kShenYou), 300, /*atk=*/30, 9000);
        };
        auto make_high = [] {
            return make_pet(3987, "启灵高攻", same5(kShenYou), 300, /*atk=*/3000, 9000);
        };
        {
            ScenarioBattle b(make_party(make_low()), make_party(plain_pet(100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            ctx.debug_step_mode = true;   // 夹在两次 submit 之间
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.step_past(State::BATTLE_FIRST_AFTER_ACTION);   // 1533 在 AFTER_ACTION 结算
            const int raw = ctx.ws.raw_attack_damage[0];
            CHECK(raw > 0 && raw < 350, "E1) 前置：低攻使伤害落在 (0, 350)");
            const int expected_extra = (350 - raw) * 12 / 10;   // （350 − 伤害）× 1.2
            CHECK_EQ(9000 - ctx.getPet(1).hp, raw + expected_extra,
                     "E1) ★ 伤害 <350 → 追加真伤 =（350 − 伤害）× 1.2（4399 内置公式）");
            b.drive();
            std::cout << "  [E1] 1533 低伤补真（公式）通过\n";
        }
        {
            ScenarioBattle b(make_party(make_high()), make_party(plain_pet(100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            ctx.debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.step_past(State::BATTLE_FIRST_AFTER_ACTION);
            const int raw = ctx.ws.raw_attack_damage[0];
            CHECK(raw >= 350, "E2) 前置：高攻使伤害达到 350");
            CHECK_EQ(9000 - ctx.getPet(1).hp, raw,
                     "E2) ★ 伤害 ≥350 → 不追加真伤（阈值门）");
            b.drive();   // 后手（守方）这 时才出手 → PP 断言放在整回合结束后
            // 1534 无被放逐 → 对手**当前精灵**全部 PP-1（0 号槽另有自身使用的 -1）
            const auto& foe_skills = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].skills;
            for (int slot = 0; slot < 5; ++slot) {
                CHECK_EQ(foe_skills[slot].pp, foe_skills[slot].maxPP - (slot == 0 ? 2 : 1),
                         "E3) ★ 无被放逐 → 对手当前精灵所有技能 PP-1");
            }
            std::cout << "  [E2/E3] 1533 阈值门 + 1534 当前精灵扣 PP 通过\n";
        }
        {
            ScenarioBattle b(make_party(make_low()), make_party(plain_pet(100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            qiling::publish_exile_mask(ctx.plugin_storage, /*side=*/1, /*mask=*/1 << 2);
            play_round_all_attack(b);
            CHECK_EQ(ctx.seerRobot[1].elfPets[2].skills[0].pp
                         - ctx.seerRobot[1].elfPets[2].skills[0].maxPP,
                     -1,
                     "E4) ★ 有被放逐 → 打被放逐的场下精灵 PP-1（1534 与神殇联动）");
            std::cout << "  [E4] 1534 放逐联动 通过\n";
        }
    }

    std::cout << "scenario_110 PASS（启灵元神：神殇/神谕/神照/神启 + 第五技 1533/1534）"
              << std::endl;
    return 0;
}
