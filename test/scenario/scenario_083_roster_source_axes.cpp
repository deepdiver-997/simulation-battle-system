// 场景 083 —— 场下保护 + 源两轴模型：武心婵 4500 / 星皇 903 生命周期 / 咤克斯消逝守卫
//
// 锁的是 2026-09-19 的两轴化（口径见
// docs_local/docs/02-效果系统/魂印机制设计与精灵表现档案.md §4.2.4）＋ 2026-09-21 的
// 星皇生命周期修正（attach：官方前提「自身位于出战背包时」，消逝 = 出列 = 无效化；
// 旧 detach 口径被语料 idx=402/403 + idx=400"一刀切"推翻）：
//   A. find_mark_in_roster：源空间定位**只剔消逝、阵亡照算**（对照 find_pet_with_soulmark 的
//      hp>0 门）；off_field_stats_protected 只护场下、票撤销即失效
//   B. 武心婵 4500「位于出战背包时，双方场下的精灵体力、PP值均不会减少」：
//      授一次 + EVENT_VANISH 推送注销——**死亡保护还在、消逝即没**（用户实测口径）
//   C. 双方各一只武心婵：票取或，一方消逝另一方继续保护
//   D. 咤克斯 2378 连锁归零 × 保护/消逝：场下写入路径先查 off_field_stats_protected；
//      咤自己被消逝 → 连锁不跑 + 秒杀转化位清除（补 v1 遗留的"被消逝则无效"）
//   E. 星皇 903 之赐：阵亡照常提供（阵亡仍在背包占位）；**消逝即停**（2026-09-21 修正，
//      旧"detach 不受消逝影响"废弃）；场域开才挡场下源
//
// 官方依据：effect_icon 1724（武心婵 4500）、622（星皇 903）、语料 idx=149（空间定义）、
// idx=402/403（之录吃掉星皇之赐）+ idx=400（2024.1.12"一刀切"）。

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSkill = 10001;   // 撞击（低威力，只关心生命周期）
constexpr int kWuxinChan = 2115;  // effect_id（2026-09-27 对齐装配层）
constexpr int kStarEmperor = 903;
constexpr int kZha = 2378;

std::array<int, 5> plain_skills() { return {kSkill, kSkill, kSkill, kSkill, kSkill}; }

ElfPet plain_pet(int hp = 2000, int atk = 1) {
    return make_pet(1, "普通", plain_skills(), /*speed=*/100, atk, hp);
}

ElfPet wuxin_pet(int hp = 2000) {
    return make_pet(4500, "武心婵", plain_skills(), 100, 1, hp,
                    SoulMark(kWuxinChan, "武心婵", "场下保护"));
}

ElfPet star_emperor_pet(int hp = 8000) {
    return make_pet(903, "瀚宇星皇", plain_skills(), 100, 1, hp,
                    SoulMark(kStarEmperor, "瀚宇星皇", "之赐之佑"));
}

ElfPet zha_pet(int hp = 2000) {
    return make_pet(2378, "咤克斯", plain_skills(), 100, 1, hp,
                    SoulMark(kZha, "咤克斯", "咒怨族"));
}

void hard_kill(BattleContext& x, int side, int slot) {
    x.seerRobot[side].elfPets[slot].hp = 0;
}

// 消逝必须走原语：vanish_spirit 是 EVENT_VANISH 的唯一发源（直写字段不发事件，
// 推送注销的 watcher 自然不触发——这正是"消逝 ⇔ 走过原语"不变量的另一面）。
void vanish(BattleContext& x, int side, int slot) {
    const VanishResult r = vanish_spirit(&x, side, slot, /*actor=*/-1);
    CHECK(r == VanishResult::VANISHED, "前置：消逝应成立");
}

int hp_of(BattleContext& x, int side, int slot) {
    return x.seerRobot[side].elfPets[slot].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A. 谓词基础：find_mark_in_roster（只剔消逝）+ off_field_stats_protected
    // ════════════════════════════════════════════════════════════════
    {
        // 手工魂印（不激活也能查 id；无 FSM 驱动）
        auto marked = make_pet(2, "带印", plain_skills(), 100, 1, 1000,
                               SoulMark(7777, "测试印", "谓词用"));
        ScenarioBattle a(scenario::make_party(plain_pet()), scenario::make_party(plain_pet()));
        a.start();
        BattleContext& x = a.ctx();
        x.on_stage[0] = 0;
        x.seerRobot[0].elfPets[3] = marked;

        CHECK_EQ(x.find_mark_in_roster(0, 7777), 3, "A1 场下带印宠可定位");
        CHECK_EQ(x.find_mark_in_roster(0, 999), -1, "A1 无关 id 找不到");

        hard_kill(x, 0, 3);
        CHECK_EQ(x.find_mark_in_roster(0, 7777), 3, "A2 阵亡（未消逝）仍算在场下空间里");
        CHECK_EQ(x.find_pet_with_soulmark(0, 7777), -1, "A2 对照：find_pet_with_soulmark 的 hp>0 门查不到");

        vanish(x, 0, 3);
        CHECK_EQ(x.find_mark_in_roster(0, 7777), -1, "A3 消逝后从空间里剔除");

        // 写入门：无票不保护；有票护双方场下、不护场上；revoke 即失效
        CHECK(!x.off_field_stats_protected(0, 1), "A4 无票：场下不受保护");
        const int sid = x.rule_center_.grant_off_field_protect(0, 9999, /*target=*/-1);
        CHECK(x.off_field_stats_protected(0, 1), "A5 授票后己方场下受保护");
        CHECK(x.off_field_stats_protected(1, 3), "A5 target=-1 保护双方场下");
        CHECK(!x.off_field_stats_protected(0, x.on_stage[0]), "A5 场上宠不归这条规则");
        x.rule_center_.revoke(sid);
        CHECK(!x.off_field_stats_protected(1, 3), "A6 revoke 后立即失效");

        std::cout << "  [A] 谓词基础通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B. 武心婵 4500：授一次 + EVENT_VANISH 推送注销；死亡不清、消逝即没
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {plain_pet(), wuxin_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 0;

        CHECK(x.off_field_stats_protected(0, 2), "B1 战斗开始已授票（武心婵场下 slot1）");
        CHECK(x.off_field_stats_protected(1, 3), "B1 双方场下都受保护");
        CHECK(!x.off_field_stats_protected(0, x.on_stage[0]), "B1 场上不护");

        hard_kill(x, 0, 1);
        CHECK(x.off_field_stats_protected(1, 3), "B2 宿主死亡保护还在（官方：背包=消逝除外）");

        vanish(x, 0, 1);
        x.event_center_.drain(&x, x.roundCount);
        CHECK(x.is_vanished(0, 1), "B3 前置：宿主已消逝");
        CHECK(!x.off_field_stats_protected(1, 3), "B3 EVENT_VANISH 推送注销 → 保护消失");

        std::cout << "  [B] 武心婵保护生命周期通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. 双方各一只：票取或，一方消逝另一方继续保护
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {plain_pet(), wuxin_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {plain_pet(), wuxin_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle c(own, opp);
        c.start();
        BattleContext& x = c.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 0;

        CHECK(x.off_field_stats_protected(0, 2), "C1 双方都授票");
        vanish(x, 0, 1);
        x.event_center_.drain(&x, x.roundCount);
        CHECK(x.off_field_stats_protected(0, 2), "C2 一方消逝，另一方的票继续保护双方场下");
        vanish(x, 1, 1);
        x.event_center_.drain(&x, x.roundCount);
        CHECK(!x.off_field_stats_protected(0, 2), "C3 两只都消逝 → 保护消失");

        std::cout << "  [C] 多提供方取或通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. 咤克斯连锁归零 × 保护 / 消逝（场下写入路径的唯一生产用户）
    // ════════════════════════════════════════════════════════════════
    {
        // 对方：slot0 场上 hp=500（被秒杀对象），slot1 场下 hp=500（连锁目标），其余 999。
        auto opp_party = [](bool with_wuxin) {
            std::array<ElfPet, 6> own = {zha_pet(), with_wuxin ? wuxin_pet() : plain_pet(),
                                         plain_pet(), plain_pet(), plain_pet(), plain_pet()};
            std::array<ElfPet, 6> opp = {plain_pet(500), plain_pet(500), plain_pet(999),
                                         plain_pet(999), plain_pet(999), plain_pet(999)};
            return std::pair<std::array<ElfPet, 6>, std::array<ElfPet, 6>>{own, opp};
        };

        // D1 基线：无武心婵 → 连锁把对方场下同体力宠归零
        {
            auto [own, opp] = opp_party(false);
            ScenarioBattle d(own, opp);
            d.start();
            BattleContext& x = d.ctx();
            x.on_stage[0] = 0;
            x.on_stage[1] = 0;
            CHECK_EQ(force_hp_to_zero(&x, 1, 0), HpZeroResult::EXECUTED, "D1 前置：秒杀成立");
            x.event_center_.drain(&x, x.roundCount);
            CHECK_EQ(hp_of(x, 1, 1), 0, "D1 连锁归零生效（基线）");
        }
        // D2 武心婵活着 → 对方场下体力不减，连锁被挡
        {
            auto [own, opp] = opp_party(true);
            ScenarioBattle d(own, opp);
            d.start();
            BattleContext& x = d.ctx();
            x.on_stage[0] = 0;
            x.on_stage[1] = 0;
            CHECK_EQ(force_hp_to_zero(&x, 1, 0), HpZeroResult::EXECUTED, "D2 前置：秒杀成立");
            x.event_center_.drain(&x, x.roundCount);
            CHECK_EQ(hp_of(x, 1, 1), 500, "D2 场下体力受保护，连锁跳过");
        }
        // D3 武心婵消逝 → 保护消失，连锁恢复
        {
            auto [own, opp] = opp_party(true);
            ScenarioBattle d(own, opp);
            d.start();
            BattleContext& x = d.ctx();
            x.on_stage[0] = 0;
            x.on_stage[1] = 0;
            vanish(x, 0, 1);
            x.event_center_.drain(&x, x.roundCount);
            CHECK_EQ(force_hp_to_zero(&x, 1, 0), HpZeroResult::EXECUTED, "D3 前置：秒杀成立");
            x.event_center_.drain(&x, x.roundCount);
            CHECK_EQ(hp_of(x, 1, 1), 0, "D3 消逝后保护消失，连锁生效");
        }
        // D4 咤自己被消逝 → 连锁不跑（火时守卫）+ 秒杀转化位清除（v1 遗留补齐）
        {
            auto [own, opp] = opp_party(false);
            ScenarioBattle d(own, opp);
            d.start();
            BattleContext& x = d.ctx();
            x.on_stage[0] = 0;
            x.on_stage[1] = 0;
            CHECK(x.hp_zero_converted[0], "D4 前置：转化位已开");
            vanish(x, 0, 0);
            x.event_center_.drain(&x, x.roundCount);
            CHECK(!x.hp_zero_converted[0], "D4 消逝推送清转化位");
            CHECK_EQ(force_hp_to_zero(&x, 1, 0), HpZeroResult::EXECUTED, "D4 前置：秒杀成立");
            x.event_center_.drain(&x, x.roundCount);
            CHECK_EQ(hp_of(x, 1, 1), 500, "D4 咤消逝后连锁不再执行");
        }

        std::cout << "  [D] 咤克斯连锁 × 保护/消逝通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E. 星皇 903 之赐两轴化：空间轴（场域）与生命周期轴（detach）分离
    // ════════════════════════════════════════════════════════════════
    {
        const int kMaxHp = 8000;
        const int kHeal = kMaxHp / 8;   // 之赐 = 最大体力 1/8 = 1000

        // E1~E3 无场域：存活场下 / 阵亡 / 消逝 → 之赐都照常（detach，游戏实测）
        {
            std::array<ElfPet, 6> own = {plain_pet(kMaxHp), star_emperor_pet(kMaxHp), plain_pet(kMaxHp),
                                         plain_pet(kMaxHp), plain_pet(kMaxHp), plain_pet(kMaxHp)};
            ScenarioBattle e(own, scenario::make_party(plain_pet(kMaxHp)));
            e.start();
            BattleContext& x = e.ctx();
            x.on_stage[0] = 0;
            x.on_stage[1] = 0;
            // 前置：行动宠压掉 2000 体力，保证 1000 的之赐不会被上限封顶
            //（last_heal_amount 记**实际**恢复量，满血时是 2 不是 1000）。
            auto open_room = [&]() {
                x.seerRobot[0].elfPets[x.on_stage[0]].hp = kMaxHp - 2000;
                x.ws.last_heal_amount[0] = 0;
            };
            // E1 存活场下：首回合行动结束回 1/8
            open_room();
            e.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            e.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            e.drive();
            CHECK_EQ(x.ws.last_heal_amount[0], kHeal, "E1 星皇存活场下 → 之赐回 1/8");

            // E2 阵亡（未消逝）：之赐照常（生命周期 detach + 官方"场下=消逝除外"）
            hard_kill(x, 0, 1);
            open_room();
            e.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            e.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            e.drive();
            CHECK_EQ(x.ws.last_heal_amount[0], kHeal, "E2 星皇阵亡 → 之赐照常（旧实现会停，已修正）");

            // E3 消逝：之赐**停止**（2026-09-21 修正口径：官方前提「自身位于出战背包时」，
            // 消逝 = 出列 → 背包辅助魂印无效化——语料 idx=402/403 + idx=400"一刀切"；
            // 旧 detach 口径"回血不受消逝影响"随本断言一并废弃）
            vanish(x, 0, 1);
            x.event_center_.drain(&x, x.roundCount);
            open_room();
            e.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            e.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            e.drive();
            CHECK_EQ(x.ws.last_heal_amount[0], 0, "E3 星皇消逝 → 之赐停止（背包前提不成立，一刀切）");
        }
        // E4~E6 场域开：存活场下被挡 / 星皇自标放行 / 消逝后放行
        {
            std::array<ElfPet, 6> own = {plain_pet(kMaxHp), star_emperor_pet(kMaxHp), plain_pet(kMaxHp),
                                         plain_pet(kMaxHp), plain_pet(kMaxHp), plain_pet(kMaxHp)};
            ScenarioBattle e(own, scenario::make_party(plain_pet(kMaxHp)));
            e.start();
            BattleContext& x = e.ctx();
            x.on_stage[0] = 0;
            x.on_stage[1] = 0;
            x.block_offstage_to_onstage = true;   // 薇尔诗场域（手工置位，无 2513 宿主）
            auto open_room = [&]() {
                x.seerRobot[0].elfPets[x.on_stage[0]].hp = kMaxHp - 2000;
                x.ws.last_heal_amount[0] = 0;
            };

            open_room();
            e.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            e.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            e.drive();
            CHECK_EQ(x.ws.last_heal_amount[0], 0, "E4 场域开 + 星皇场下 → 之赐被挡（空间轴）");

            x.on_stage[0] = 1;   // 星皇自己上场（自标 → 天然放行，U3 回归）
            open_room();
            e.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            e.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            e.drive();
            CHECK_EQ(x.ws.last_heal_amount[0], kHeal, "E5 场域开 + 星皇场上 → 之赐放行");

            x.on_stage[0] = 0;   // 星皇回场下并消逝
            vanish(x, 0, 1);
            x.event_center_.drain(&x, x.roundCount);
            open_room();
            e.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            e.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            e.drive();
            CHECK_EQ(x.ws.last_heal_amount[0], 0,
                     "E6 场域开 + 星皇消逝 → 之赐停止（源不在背包，与场域无关——2026-09-21 一刀切口径）");
        }

        std::cout << "  [E] 星皇之赐两轴化通过\n";
    }

    std::cout << "scenario_083 全部通过\n";
    return 0;
}
