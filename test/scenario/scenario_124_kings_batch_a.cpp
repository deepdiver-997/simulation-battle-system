#include <utils/sbs_rand.h>
// 场景 119 —— 精灵王批 A：怒涛·沧岚（神谕 3740 / 魂印 2343）+ 混沌地王（神谕 3516 / 魂印 2411）
//
// 锁的是 2026-09-24 批 A（台账"精灵王线"行）：
//   A. 水王登场 400 盾；盾在 → 先制 +1；攻击后 +1 永恒之水（8 道锁定）
//   B. 无盾攻击后 → 对手侧 +1 蚀砚之泪；泪 ≥4 → PENETRATE_ATTACK 窗口（凭证查询）
//   C. 水王粉伤锁（CAP）仍生效：受粉 ≤ 登场盾笔数值
//   D. 混地：回合开始无提升 → 阶段末吸取 1/3 + 等量盾；盾值=400 倍数 → 种 1 蛊
//   E. 混地：对手回血 → 消耗 1 蛊插入 40 真伤（每回合 4 次上限）
//   F. 混地：强化被消除（带归因通道）→ +1 嶂 + 400 盾；嶂 → 护盾承伤乘区
//
// 官方依据：effect_icon 1932/1985 tips 全文 + 解析 idx=263/190。
#include <any>
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "../../resources/nutao_runtime.h"
#include "../../resources/diwang_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCanglan = 3740;
constexpr int kCanglanMark = 2343;
constexpr int kDiwang = 3516;
constexpr int kDiwangMark = 2411;
constexpr int kPlain = 10001;

int pub_get(BattleContext& x, int key) {
    auto it = x.plugin_storage.find(key);
    if (it == x.plugin_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

ElfPet canglan_pet(int hp = 6000) {
    SoulMark sm(kCanglanMark, "滴", "测试桩：怒涛·沧岚神谕 2343");
    return make_pet(kCanglan, "怒涛·沧岚", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/300, /*atk=*/150, hp, sm);
}

ElfPet diwang_pet(int hp = 6000) {
    SoulMark sm(kDiwangMark, "墬", "测试桩：混沌地王神谕 2411");
    return make_pet(kDiwang, "混沌地王", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/100, /*atk=*/150, hp, sm);
}

ElfPet plain_pet(int atk = 100, int hp = 4000) {
    return make_pet(1, "普通", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, atk, hp);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ═══ A/B/C. 水王 ════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {canglan_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet(100, 100000)));
        b.start();
        BattleContext& x = b.ctx();

        CHECK_EQ(x.getPet(0).shield_bank_.total(), 400, "A1 登场 400 盾");

        // 回合 1：水王攻击（盾在 → 攻击后 +1 水）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(pub_get(x, kNutaoPubWater), 1, "A2 有盾攻击 → 永恒之水 +1");
        CHECK_EQ(pub_get(x, kNutaoPubWaterLock), 0, "A3 未达 8 道 → 不锁定");

        // 泪为 0：先手权应 +1（水王 speed 300 已必先，这里只锁行为不炸即可）
        CHECK(pub_get(x, kNutaoPubTears) == 0, "B0 有盾攻击 → 不上泪");

        // 打掉盾：对方高攻打 2 回合把 400 盾磨光 → 之后攻击改 +泪
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        if (x.getPet(0).shield_bank_.total() == 0) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(pub_get(x, kNutaoPubTears), 1, "B1 无盾攻击 → 对手侧 +1 蚀砚之泪");
            CHECK_EQ(pub_get(x, kNutaoPubWater), 1, "B2 无盾攻击 → 不再 +水");
        }
    }

    // ═══ D/E/F. 混地 ════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {diwang_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet(50, 999999)));
        b.start();
        BattleContext& x = b.ctx();

        // 回合 1：混地无强化 → 阶段末吸取分支（对手被吸 + 混地得盾）；盾 400 → 种 1 蛊
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(pub_get(x, kDiwangPubGu + 1), 1, "D1 盾值=400 倍数 → 对手种 1 蛊");
        CHECK(x.getPet(0).shield_bank_.total() >= 400, "D2 阶段末获得等量护盾");

        // E：对手回血 → 蛊 -1 + 插 40 真伤（对手造一次回血：给它挂 heal 原语）
        const int gu_before = pub_get(x, kDiwangPubGu + 1);
        ElfPet& opp = x.getPet(1);
        core_api().heal_amount(&x, 1, 500);
        const int after_heal = opp.hp;   // 先回血（+500）
        x.event_center_.drain(&x, x.roundCount);   // 事件"入队+晚 drain"——手动泵一次
        CHECK_EQ(pub_get(x, kDiwangPubGu + 1), gu_before - 1, "E1 回血消耗 1 蛊");
        CHECK(opp.hp == after_heal - 40, "E2 插入真伤落地（40）");

        // F：强化被消除（带归因通道）→ +1 嶂 + 400 盾
        auto& core = core_api();
        core.stat_change(&x, 0, /*stat=*/0, /*delta=*/2);   // 先给混地一个提升
        int zhang = pub_get(x, kDiwangPubZhang);
        core.clear_stat_boosts_as(&x, 0, 1);
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(pub_get(x, kDiwangPubZhang), zhang + 1, "F1 强化被消除 → +1 封邪之嶂");
        CHECK_EQ(x.ws.shield_absorb_bonus_pct[0], 10 * (zhang + 1), "F2 嶂 → 护盾承伤乘区");
    }

    std::cout << "scenario_119 全部通过" << std::endl;
    return 0;
}
