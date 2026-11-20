// 场景 876 —— 哈莫链复刻：表姐先手自爆 × 龙神哈莫固伤救场（2026-09-30 用户实测现象）
//
// 复刻的官方实测现象（用户游戏内）：表姐先手自爆守御八方，对手龙神哈莫——正常应该
// 表姐死亡跳过哈莫出手，实际哈莫出了招、表姐直到回合结束才死，且复活后免死仍剩 1 回合。
//
// 机制链（2026-09-30 用户定稿，四次复核收敛，见交接档"哈莫现象·最终版机制链"）：
//   1. 自爆无视免死：消耗体力=体力修正通道，不发预死亡信号 → 免死不触发（"致命伤害
//      是自己的技能造成的"与"通道不可见"同构）；
//   2. 哈莫魂印③固伤（战斗阶段结束=出手流程结束→死亡检查时点桶，先于死亡裁决）打在
//      0 血表姐身上 → 伤害结算命中 0 血状态 → 范围检测免死激活（信号回调）→ 拉起 1 血；
//   3. 死亡检查看到 1 → 活 → **哈莫照常出招**（不跳后手流程）；
//   4. 自爆的待决死亡印记在后手流程结束后的死亡检查时点收尾：清 0（体力修正，无信号、
//      无新激发）→ 死亡检查看到 0 → 死亡成立（表姐死于哈莫出手之后）。
//
// 引擎落点（本场景锁定的三处改动）：
//   · deal_damage 尸体检测：已倒地目标被伤害结算命中 → 也发预死亡信号（②族检测面）；
//   · 帝皇之御免死迁移②信号回调（拦截器退役）；守御八方自爆改印记形态（清 0+待决
//     印记，SECOND_MOVER_DEATH 收尾）；
//   · 哈莫③固伤时点 = FIRST/SECOND_MOVER_DEATH 桶（旗标消费制）。
//
// 断言组：
//   A 复刻主线：R1 表姐自爆 → 哈莫固伤救场（第一死亡检查看到 1）→ 哈莫出招
//     （skill_use_seq[1]==1，死亡未跳过）→ 印记收尾 → 表姐死于回合内（hp=0+死亡登记）。
//   B 对照（无哈莫魂印）：同配置 plain 3809 → 无固伤救场 → 表姐死于第一检查点
//     → 哈莫被跳过（skill_use_seq[1]==0）——同时证明免死不响应自爆本身。
//   C 传承照常：A 组表姐之死挂传承待决（下只登场 2 回合减伤），自爆印记已消费不残留。
//
// 确定性纪律：固伤 150-300 与掷点不落断言（救场机制由 A1 的"哈莫出招"间接证明——
// 固伤若未落、免死未拉起，哈莫必被跳过）。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kDiwangId = 3512;
constexpr int kHamoId = 3809;
constexpr int kMarkDiwang = 1106;
constexpr int kMarkHamo = 1392;
constexpr int kShouYuBaFang = 31505;
constexpr int kGaoSu = 20017;

// 与 resources/soul_lib/diwang_3512.cpp 共享段对齐。
constexpr int kSelfDestructMarkKey = 35120208;
constexpr int kLegacyPendingKey = 35120205;

ElfPet diwang(int speed = 300) {
    return scenario::make_pet(kDiwangId, "帝皇之御",
                              {kShouYuBaFang, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              speed, 100, 5000,
                              SoulMark(kMarkDiwang, "御", "帝皇之御魂印"));
}

// marked=false = 无魂印哈莫（对照组）。
ElfPet hamos(int speed, bool marked) {
    return scenario::make_pet(kHamoId, "龙神哈莫", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              speed, 100, 1000000,
                              marked ? SoulMark(kMarkHamo, "敖", "龙神哈莫魂印") : SoulMark());
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

    // ══ A 复刻主线：固伤救场 → 哈莫出招 → 印记收尾才死 ══════════════════════
    {
        ScenarioBattle b(scenario::make_party(diwang()),
                         scenario::make_party(hamos(/*speed=*/100, /*marked=*/true)));
        BattleContext& ctx = b.ctx();
        b.start();
        submit(b, 0, 0);   // 表姐守御八方（先手）+ 哈莫高速移动
        b.drive();
        const int slot = ctx.on_stage[0];
        CHECK_EQ(ctx.getPet(0).hp, 0, "A 表姐最终死亡（印记收尾）");
        CHECK(ctx.pet_death_notified[0][slot], "A 死亡登记成立");
        CHECK_EQ(ctx.skill_use_seq[1], 1,
                 "A 哈莫照常出招（固伤+免死在第一死亡检查点救场，未跳后手流程）");
        CHECK_EQ(ctx.skill_use_seq[0], 1, "A 表姐守御八方真实使用");
        std::cout << "  [A] 复刻主线：自爆 → 固伤救场 → 哈莫出招 → 印记收尾才死 ✓"
                  << std::endl;
    }

    // ══ B 对照：无哈莫魂印 → 死于第一检查点、哈莫被跳过 ═════════════════════
    {
        ScenarioBattle b(scenario::make_party(diwang()),
                         scenario::make_party(hamos(/*speed=*/100, /*marked=*/false)));
        BattleContext& ctx = b.ctx();
        b.start();
        submit(b, 0, 0);
        b.drive();
        const int slot = ctx.on_stage[0];
        CHECK_EQ(ctx.getPet(0).hp, 0, "B 表姐死亡（自爆不触发免死——自身通道不可见）");
        CHECK(ctx.pet_death_notified[0][slot], "B 死亡登记成立");
        CHECK_EQ(ctx.skill_use_seq[1], 0, "B 哈莫被死亡跳过（无固伤救场）");
        std::cout << "  [B] 对照：无魂印 → 第一检查点死亡、哈莫被跳过 ✓" << std::endl;
    }

    // ══ C 传承照常 + 印记消费 ══════════════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(diwang()),
                         scenario::make_party(hamos(/*speed=*/100, /*marked=*/true)));
        BattleContext& ctx = b.ctx();
        b.start();
        submit(b, 0, 0);
        b.drive();
        auto legacy = ctx.plugin_storage.find(kLegacyPendingKey);
        CHECK(legacy != ctx.plugin_storage.end()
                  && std::any_cast<int>(legacy->second) == 1,
              "C 表姐之死挂传承待决（下只登场减伤）");
        auto mark = ctx.plugin_storage.find(kSelfDestructMarkKey);
        CHECK(mark == ctx.plugin_storage.end(), "C 自爆印记已消费（不残留、不追杀复活体）");
        std::cout << "  [C] 传承待决挂账、自爆印记消费 ✓" << std::endl;
    }

    std::cout << "scenario_876 PASS (哈莫链复刻：自爆无视免死→固伤激活免死→后手照常→印记收尾)"
              << std::endl;
    return 0;
}
