// 场景 877 —— 表姐 × 机盖 双向交叉（2026-09-30 用户游戏实测点名的场景）
//
// 实测口径（用户 2026-09-30 游戏验证："表姐高红击杀机盖，机盖真的死亡"）：
//   表姐首登窗口内高红打死机盖 → 机盖死亡漏斗 → 盖亚拦截器弹 last_landed_hit 等量
//   真伤 → 表姐被弹致死 → 免死（②信号回调）拉 1 活 → 机盖死亡成立。
//   本场景同时锁定"机盖弹的真伤也是致命伤害会激活表姐免死"（用户实测确认）——
//   真伤信号由 deal_damage TRUE 通道发出。
//   注意：表姐的反弹不触发——弹伤=真伤，kind 不在表姐反弹门（红伤/属性直伤）内。
//
// 引擎锚点：机盖 = resources/soul_lib/gaia_kuang_3056.cpp（死亡拦截器弹真伤 +
// heal(1)）；表姐免死 = diwang_3512.cpp 的 EVENT_HP_EXHAUSTED 信号回调。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kDiwangId = 3512;
constexpr int kGaiaId = 3056;
constexpr int kMarkDiwang = 1106;
constexpr int kMarkGaia = 736;
constexpr int kYanFan = 10037;   // 燕返：flying 60 必中
constexpr int kGaoSu = 20017;

void must_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
    }
}

ElfPet diwang(int speed, int atk, int hp) {
    ElfPet p = scenario::make_pet(kDiwangId, "帝皇之御", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                  speed, atk, hp, SoulMark(kMarkDiwang, "御", "帝皇之御魂印"));
    must_hit(p);
    return p;
}

ElfPet gaia(int hp) {
    ElfPet p = scenario::make_pet(kGaiaId, "圣甲·盖亚", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                  50, 100, hp, SoulMark(kMarkGaia, "狂", "机盖"));
    must_hit(p);
    return p;
}

void submit(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ 表姐高红击杀机盖：机盖死、表姐被等量真伤弹致死 → 免死拉 1 ═══════════
    {
        // 表姐 atk 足以一击 > 机盖体力（3000）；表姐自身 3000 HP < 反弹真伤量
        // → 必然进入"弹致死 → 免死拉 1"路径。
        ScenarioBattle b(scenario::make_party(diwang(/*speed=*/300, /*atk=*/20000, /*hp=*/3000)),
                         scenario::make_party(gaia(/*hp=*/3000)));
        BattleContext& ctx = b.ctx();
        b.start();
        submit(b, 0, 0);   // 表姐燕返（高红）vs 机盖高速移动
        b.drive();
        const int gaia_slot = ctx.on_stage[1];
        CHECK(ctx.pet_death_notified[1][gaia_slot], "机盖死亡成立（高红击杀）");
        CHECK_EQ(ctx.getPet(1).hp, 0, "机盖 hp=0");
        CHECK_EQ(ctx.getPet(0).hp, 1, "表姐被等量真伤弹致死 → 免死拉 1（真伤激活免死）");
        CHECK(!ctx.pet_death_notified[0][ctx.on_stage[0]], "表姐死亡未成立（活）");
        std::cout << "  [T1] 表姐高红杀机盖：机盖死、表姐真伤弹致死免死拉1 ✓" << std::endl;
    }

    std::cout << "scenario_877 PASS (表姐×机盖：高红杀机盖/真伤弹致死激活免死)" << std::endl;
    return 0;
}
