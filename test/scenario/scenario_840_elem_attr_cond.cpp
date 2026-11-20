// 场景 840 —— H6 系别/属性技条件族（65 / 152 / 526，moves_lib/h_elem_attr_cond.cpp）
//（2026-09-30，worktree ../agents/h6；场景段 840-847 一文件覆盖三效果）
//
// 断言组（场景号 → 用例）：
//   840  65 主路径：先手水天一色（[2,2,2] 水系×2）→ R2 水系攻击威力翻倍、R3 过期回落
//        （loss 比值带断言，85%~100% 浮动两侧同消）。
//   841  65 系别条件：窗口内拍打（普通系）不翻倍（R2/R5 自比 ≈1）。
//   842  65 {0}=1 窗口长度：季风（[1,4,2] 1回合）→ R2 攻击已过期不翻（与 840 差分钉
//        {0} 参数）。
//   843  152 主路径：先手熔岩真气（[5,100,2] 100% 确定档）→ 对手用攻击技（拍打）不掷
//        → 无烧伤；对手用属性技（高速移动）→ 烧伤落地。
//   844  152 "使用即触发"语义：对手属性技命中压到 1%（必 miss 档）→ 烧伤照常落地
//        （SKILL_USED 不看命中，469 同判）。
//   845  526 主路径+过期：古剑灭心诀（[3,150]）→ R1/R3 对手属性技命中各挨 150 粉伤
//        （精确值+红伤噪声带）、R2 攻击技不挨、R4 窗口过期不挨。
//   846  526 "成功使用"语义：窗口内对手属性技被 1284 封属（28019 链，SKILL_INVALID
//        无 EVENT_HIT）→ 不挨粉伤（与 844 的"miss 也算使用"成对钉两效果语义分档）。
//   847  526 第二载体：巨爪猛击（[180,526]|[3,200]）→ 200 点档。
//
// 设计口径（720/800 同款）：
//   - 先手用 speed 300 vs 100（65/526 窗口断言全部取先手位，回避 InRounds 后手顺延的
//     起点折算——顺延语义由 register_round_effect/watcher 的既有口径守护，不在本单展开）。
//   - 红伤 85%~100% 浮动 → 比值带断言（[1.6,2.4] 倍 / [0.7,1.3] 平）；粉伤 FIXED 档无
//     浮动 → 精确值 + atk=1 红伤噪声带（atk=1 打任何面板都是 1~2 点，速查 §五）。
//   - 必中全开、暴击率 0（make_pet 默认）；伴生自查：21411 链 4×2（自身特攻/特防+1，
//     正交）、28019 链 191（弹控窗，正交）/1073（未实现 no-op）、17532 链 180（对手无
//     回合类 no-op）、17600/20285/20297/20506/11425/10004 纯净；20017（高速移动）是
//     对手属性技施加器兼速度+2（≤4 层不翻 300 vs 100 的先手序）。
//   - ⚠️ 840/841/842 的 65 窗口观察位在"施放回合的下一回合"（载体全是属性技、施放
//     当回合自身无攻击可翻）——先手位下 InRounds 窗口 = R1..R{0}，R2 是 {0}≥2 的
//     观察位、R{0}+1 起过期。
#include <array>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kShuiTian = 20506;   // 水天一色：属性 [65]|[2,2,2]（2回合水系×2）
constexpr int kJiFeng = 20297;     // 季风：属性 [65]|[1,4,2]（1回合飞行系×2）
constexpr int kShuiLiu = 11425;    // 水流波动：特攻95 水系 无效果（65 观察武器）
constexpr int kPaDa = 10004;       // 拍打：物攻40 普通系 无效果（对照组/填充）
constexpr int kRongYan = 21411;    // 熔岩真气：属性 [4,4,152]|[…,5,100,2]（152 100% 烧伤）
constexpr int kGuJian = 17600;     // 古剑灭心诀：物攻160 [526]|[3,150]
constexpr int kJuZhua = 17532;     // 巨爪猛击：物攻? [180,526]|[3,200]
constexpr int kShuangZhi = 28019;  // 霜之教义：属性 [191,1073,1284] → 1284 封属[3]（846 用）
constexpr int kGaoSu = 20017;      // 高速移动：属性 [4]|[4,100,2]（对手属性技施加器）

ElfPet self_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 对手属性技"几乎必 miss"变体（844 用：accuracy 1 + 不必中）。
ElfPet foe_pet_whiffy_attr(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = foe_pet(skills, speed, atk, hp);
    p.skills[0].must_hit = 0;
    p.skills[0].accuracy = 1;
    return p;
}

void play_round(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

bool has_anomaly(BattleContext& ctx, int side, AbnormalStatusId id) {
    return ctx.has_active_abnormal_status(side, static_cast<int>(id));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // 840  65 主路径：水天一色 → R2 水系×2、R3 过期
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kShuiTian, kShuiLiu, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/50, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：先手施放水天一色（属性技，无红伤）；对手拍打
        const int hp_after_r1 = ctx.getPet(1).hp;
        play_round(b, 1, 0);   // R2：水流波动（水系）——窗口内 → 威力×2
        const int w2 = 100000 - ctx.getPet(1).hp - (100000 - hp_after_r1);
        play_round(b, 1, 0);   // R3：窗口过期 → 回落
        const int w3 = 100000 - ctx.getPet(1).hp - (100000 - hp_after_r1) - w2;
        CHECK(w3 > 0, "840 前提：水系攻击落地");
        const double ratio = static_cast<double>(w2) / static_cast<double>(w3);
        CHECK(ratio > 1.6 && ratio < 2.4,
              "840 核心：窗口内水系威力×2（W2/W3=" << ratio << "，期望≈2，浮动带 [1.6,2.4]）");
        std::cout << "  [840] 65 水系×2 主路径 通过（W2/W3=" << ratio << "）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 841  65 系别条件：窗口内普通系攻击不翻倍
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kShuiTian, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/50, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：施放水天一色
        int prev = ctx.getPet(1).hp;
        play_round(b, 1, 0);   // R2：拍打（普通系，窗口内）→ 不翻
        const int d2 = prev - ctx.getPet(1).hp;
        prev = ctx.getPet(1).hp;
        play_round(b, 1, 0);   // R3/R4：水流打过去（与断言正交，消耗窗口）
        play_round(b, 1, 0);
        prev = ctx.getPet(1).hp;
        play_round(b, 1, 0);   // R5：拍打（窗口已过期）→ 基线
        const int d5 = prev - ctx.getPet(1).hp;
        CHECK(d5 > 0, "841 前提：拍打基线落地");
        const double ratio = static_cast<double>(d2) / static_cast<double>(d5);
        CHECK(ratio > 0.7 && ratio < 1.3,
              "841 核心：普通系不吃加成（D2/D5=" << ratio << "，期望≈1）");
        std::cout << "  [841] 65 系别条件对照 通过（D2/D5=" << ratio << "）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 842  65 {0}=1：季风 1 回合窗 → R2 已过期
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kJiFeng, kShuiLiu, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/50, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：施放季风（[1,4,2] 飞行系 1 回合；我无飞行攻击可翻）
        play_round(b, 1, 0);   // R2：水流波动 → {0}=1 窗口已过期 → 不翻
        const int w2 = 100000 - ctx.getPet(1).hp;
        play_round(b, 1, 0);   // R3：基线
        const int w3 = 100000 - ctx.getPet(1).hp - w2;
        CHECK(w3 > 0, "842 前提：水流波动落地");
        const double ratio = static_cast<double>(w2) / static_cast<double>(w3);
        CHECK(ratio > 0.7 && ratio < 1.3,
              "842 核心：1 回合窗 R2 已过期（W2/W3=" << ratio
 << "；与 840 的 {0}=2 翻倍成差分）");
        std::cout << "  [842] 65 {0}=1 窗口长度差分 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 843  152 主路径：熔岩真气 → 对手攻击技不掷、属性技 100% 烧伤
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kRongYan, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 1);   // R1：先手熔岩真气（152 窗口 5 回合 armed）；对手拍打（攻击技）→ 不掷
        CHECK(!has_anomaly(ctx, 1, AbnormalStatusId::Burn),
              "843 核心：对手用攻击技 → 152 不掷（无烧伤）");
        play_round(b, 1, 0);   // R2：对手高速移动（属性技）→ 100% 烧伤
        CHECK(has_anomaly(ctx, 1, AbnormalStatusId::Burn),
              "843 核心：对手用属性技 → 烧伤落地（[5,100,2] 确定档）");
        std::cout << "  [843] 152 属性技监听主路径 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 844  152 "使用即触发"：miss 的属性技也触发
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kRongYan, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet_whiffy_attr({kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa},
                                                        /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：先手熔岩真气；对手高速移动（accuracy 1 → 大概率 miss）
        CHECK(has_anomaly(ctx, 1, AbnormalStatusId::Burn),
              "844 核心：属性技 miss 仍算'使用'（SKILL_USED 不看命中）→ 烧伤落地");
        std::cout << "  [844] 152 miss 仍触发 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 845  526 主路径：R1/R3 属性技命中 → 对手挨 150 粉伤；R2 攻击技/R4 过期 → 不挨
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kGuJian, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：先手古剑灭心诀（红伤 1~2）+ 对手高速移动（命中）→ +150 粉
        const int d1 = 100000 - ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R2：对手拍打（攻击技）→ 不挨粉
        const int d2 = 100000 - ctx.getPet(1).hp - d1;
        play_round(b, 1, 0);   // R3：窗口末回合 → 再挨 150
        const int d3 = 100000 - ctx.getPet(1).hp - d1 - d2;
        play_round(b, 1, 0);   // R4：过期 → 不挨
        const int d4 = 100000 - ctx.getPet(1).hp - d1 - d2 - d3;
        CHECK(d1 >= 151 && d1 <= 155,
              "845 核心：R1 对手挨 150 粉伤（D1=" << d1 << "，150 粉+atk=1 红伤带）");
        CHECK(d2 <= 3, "845 核心：R2 攻击技不触发（D2=" << d2 << "）");
        CHECK(d3 >= 151 && d3 <= 155,
              "845 核心：R3 窗口末回合仍触发（D3=" << d3 << "）");
        CHECK(d4 <= 3, "845 核心：R4 窗口过期（D4=" << d4 << "）");
        std::cout << "  [845] 526 属性技成功→固伤 主路径 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 846  526 "成功使用"门：窗口内对手属性技被封属（SKILL_INVALID 无 EVENT_HIT）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kShuangZhi, kGuJian, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：先手霜之教义 → 1284 封属 3 回合（R1-R3）
        play_round(b, 1, 0);   // R2：先手古剑灭心诀（526 窗口 R2-R4 armed）；对手高速移动被
                               //     封属（SKILL_INVALID，无 EVENT_HIT）→ 不挨粉
        const int d2 = 100000 - ctx.getPet(1).hp;
        CHECK(d2 <= 3,
              "846 核心：被封属的属性技不算'成功使用'（D2=" << d2 << "；与 844 成对）");
        std::cout << "  [846] 526 成功使用门 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 847  526 第二载体：巨爪猛击（[3,200]）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kJuZhua, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：先手巨爪猛击 + 对手高速移动命中 → +200 粉
        const int d1 = 100000 - ctx.getPet(1).hp;
        CHECK(d1 >= 201 && d1 <= 205,
              "847 核心：第二载体 200 点档（D1=" << d1 << "）");
        std::cout << "  [847] 526 第二载体 200 点档 通过\n";
    }

    std::cout << "scenario_840 PASS (H6 系别/属性技条件族 65/152/526，段 840-847)" << std::endl;
    return 0;
}
