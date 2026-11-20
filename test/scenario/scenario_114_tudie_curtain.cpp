// 场景 114 —— 涂蝶 4861 魂印「幕」系统
// （2026-09-22 合并时由 112 改号：112 已被启灵技能组收尾（批次A）占用，113 为百罗鬼帝）（2026-09-22，登场先后批次任务 C）
//
// 官方文本（4399 = DB effect_icon 2046，两源一致）：
//   战斗开始时，为己方添加白幕，若自身的1号位技能名中存在颜色，则将己方的幕染为
//   对应的颜色；自身登场时，若对方没有任何颜色的幕，则为对方添加白幕；任意一方幕
//   的颜色改变时，当回合结束后位于场内的自身恢复最大体力的1/3与所有技能1点PP值。
//
// 锁定判定：
//   A 战斗开始白幕 + 1号位技能名染色（颜色字符 → 幕色枚举；无颜色字符保持白幕）；
//   B 变色 → 当回合结束后场内自身回 1/3 + 全技能 1 PP（无变色不回）；
//   C 对方已有真颜色幕 → 登场子句不覆盖（白幕授予只落无色幕）；
//   D 中切登场子句（次回合提交序）+ 涂蝶在背包时战斗开始照样给己方上幕/染色；
//   E 首回合双切（视频场景）：挑战方先提交、房主侧涂蝶仍先登场先挂幕。
//
// 幕状态读口 = tudie_runtime.h 的 plugin_storage 键（kCurtainColorBase + side），
// 无幕 = 键不存在 = -1；白幕 = 0。染色枚举：赤/红=1 黄=2 蓝=3 黑=4。

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "../../resources/tudie_runtime.h"

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10162;

std::array<int, 5> same5(int id = kHit) { return {id, id, id, id, id}; }

SoulMark tudie_mark() { return SoulMark(2468, "涂蝶", "幕系统"); }

// 涂蝶：1 号位技能名可覆写（幕判定只读名字符串；用 kHit 加载保证技能可用无害），
// 另有 real38228 档直接从 DB 加载真·赤绘（38228）验证名字路径。
ElfPet tudie_pet(const std::string& slot1_name, int speed = 100, int hp = 3000) {
    ElfPet p = make_pet(4861, "涂蝶", same5(), speed, /*atk=*/1, hp, tudie_mark());
    p.skills[0].name = slot1_name;
    return p;
}
ElfPet tudie_pet_real(int speed = 100, int hp = 3000) {
    return make_pet(4861, "涂蝶", {38228, kHit, kHit, kHit, kHit}, speed, /*atk=*/1, hp,
                    tudie_mark());
}
ElfPet plain(int speed = 100) {
    return make_pet(1, "宠", same5(), speed, /*atk=*/1, 9000);
}

int curtain(const BattleContext& ctx, int side) {
    return tudie::curtain_color(ctx.plugin_storage, side);
}

// 打完一整回合（双方普攻）
void play_round(ScenarioBattle& b) {
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
    // A 战斗开始：己方白幕 + 1号位技能名染色
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(tudie_pet("陶醉")), make_party(plain()));
        CHECK_EQ(curtain(b.ctx(), 0), tudie::kWuMu, "A) 前置：开战前双方无幕");
        b.start();
        play_round(b);
        // 1号位无颜色字符 → 己方白幕（0）；涂蝶首发登场 → 对方无幕 → 白幕授予
        CHECK_EQ(curtain(b.ctx(), 0), tudie::kWhite, "A1) ★ 战斗开始为己方添加白幕（1号位无颜色）");
        CHECK_EQ(curtain(b.ctx(), 1), tudie::kWhite, "A1) ★ 首发登场 → 对方无色幕 → 白幕授予");
    }
    {
        // 颜色字符 → 染色（赤=1 黄=2 蓝=3 黑=4）；对方只拿到白幕、不被染色
        const std::pair<const char*, int> cases[] = {
            {"赤绘", tudie::kRed}, {"黄印", tudie::kYellow},
            {"蓝染", tudie::kBlue}, {"玄黑", tudie::kBlack}};
        for (const auto& [name, want] : cases) {
            ScenarioBattle b(make_party(tudie_pet(name)), make_party(plain()));
            b.start();
            play_round(b);
            CHECK_EQ(curtain(b.ctx(), 0), want, "A2) ★ 1号位技能名含颜色 → 己方幕染色");
            CHECK_EQ(curtain(b.ctx(), 1), tudie::kWhite, "A2) 对方仍是白幕（授予≠染色）");
        }
        std::cout << "  [A] 战斗开始白幕/染色（赤黄蓝黑四色）通过\n";
    }
    {
        // 名字路径 e2e：真·赤绘 38228 从 DB 加载，Skills::name 自带颜色字符
        ScenarioBattle b(make_party(tudie_pet_real()), make_party(plain()));
        b.start();
        play_round(b);
        CHECK_EQ(curtain(b.ctx(), 0), tudie::kRed, "A3) ★ 真·赤绘(38228) DB 名 → 染红（Skills::name 可见）");
        std::cout << "  [A'] DB 技能名路径 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B 变色 → 当回合结束后：场内自身回 1/3 + 全技能 1 PP
    // （对方整回合换宠不出手 → 我方无受击，回合末 hp/PP 差值即回复量）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(tudie_pet("赤绘", 100, 3000)), make_party(plain()));
        b.start();
        ElfPet& me = b.pet(0);
        me.hp = 1000;   // 预伤，留足回复空间（make_pet HP=3000 → 回复 = 3000/3 = 1000）
        me.skills[1].pp -= 3;   // 预扣：满 PP 的技能 +1 会被 maxPP 钳住，看不出变化
        me.skills[2].pp = 0;    // 极端档：0 → 1
        const int pp1 = me.skills[1].pp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 我方普攻
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);     // 对方换宠（不出手）
        b.drive();   // 跑完整回合 → 停在次回合 CHOOSE
        CHECK_EQ(me.hp, 2000, "B1) ★ 变色当回合结束后回最大体力 1/3（1000 + 3000/3）");
        CHECK_EQ(me.skills[1].pp, pp1 + 1, "B1) ★ 未使用技能回 1 PP");
        CHECK_EQ(me.skills[2].pp, 1, "B1) ★ 0 PP 技能回 1 点（钳 maxPP 前可观测）");
        std::cout << "  [B] 变色回合末回复 通过\n";
    }
    {
        // 对照：无颜色字符 → 无变色 → 不回血不加 PP
        ScenarioBattle b(make_party(tudie_pet("陶醉", 100, 3000)), make_party(plain()));
        b.start();
        ElfPet& me = b.pet(0);
        me.hp = 1000;
        const int pp1 = me.skills[1].pp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(me.hp, 1000, "B2) ★ 无变色 → 不回血");
        CHECK_EQ(me.skills[1].pp, pp1, "B2) ★ 无变色 → 不回 PP");
        std::cout << "  [B'] 无变色对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C 对方已有真颜色幕 → 登场子句不覆盖
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(tudie_pet("赤绘")), make_party(plain()));
        b.ctx().plugin_storage[tudie::kCurtainColorBase + 1] = tudie::kBlue;  // 预置对方蓝幕
        b.start();
        play_round(b);
        CHECK_EQ(curtain(b.ctx(), 0), tudie::kRed, "C) 前置：己方染红");
        CHECK_EQ(curtain(b.ctx(), 1), tudie::kBlue, "C) ★ 对方已有真颜色幕 → 白幕授予不动它");
        std::cout << "  [C] 有色幕保护 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D 中切登场子句 + 在背包时战斗开始照样上幕/染色
    // ════════════════════════════════════════════════════════════════
    {
        // 队伍：首发 plain，涂蝶在 2 号位。战斗开始：己方上幕/染色照常（ROSTER），
        // 但涂蝶未登场 → 对方无白幕（无幕 -1）。
        std::array<ElfPet, 6> party = make_party(plain());
        party[1] = tudie_pet("赤绘");
        ScenarioBattle b(party, make_party(plain()));
        b.start();
        play_round(b);
        CHECK_EQ(curtain(b.ctx(), 0), tudie::kRed, "D1) ★ 涂蝶在背包：战斗开始照样给己方染色");
        CHECK_EQ(curtain(b.ctx(), 1), tudie::kWuMu, "D1) ★ 未登场 → 对方无幕（-1）");
        // 次回合涂蝶中切登场 → 对方白幕授予（-1 → 0 可观测）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(curtain(b.ctx(), 1), tudie::kWhite, "D1) ★ 涂蝶中切登场 → 对方无色幕获白幕");
        std::cout << "  [D] 中切登场子句/背包上幕 通过\n";
    }
    {
        // 中切登场时对方已有真颜色幕 → 不覆盖
        std::array<ElfPet, 6> party = make_party(plain());
        party[1] = tudie_pet("赤绘");
        ScenarioBattle b(party, make_party(plain()));
        b.ctx().plugin_storage[tudie::kCurtainColorBase + 1] = tudie::kYellow;
        b.start();
        play_round(b);
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(curtain(b.ctx(), 1), tudie::kYellow, "D2) ★ 中切登场同理：真颜色幕不被白幕覆盖");
        std::cout << "  [D'] 中切·有色幕保护 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E 首回合双切（视频场景）：挑战方先提交，房主侧涂蝶仍先登场先挂幕
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> host_party = make_party(plain());
        host_party[1] = tudie_pet("赤绘");   // 涂蝶在房主方背包
        ScenarioBattle b(host_party, make_party(plain()));
        b.start();
        // 挑战方（1 号）先提交切换，房主（0 号）后提交（场景 111B：仍强制房主先登场）
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(curtain(b.ctx(), 0), tudie::kRed, "E) 前置：房主方涂蝶染红（战斗开始，背包位）");
        CHECK_EQ(curtain(b.ctx(), 1), tudie::kWhite,
                 "E) ★ 视频场景：房主侧涂蝶先登场 → 登场子句先结算（挑战方先提交也拦不住）");
        std::cout << "  [E] 首回合双切·房主侧涂蝶先挂幕 通过\n";
    }

    std::cout << "scenario_114 PASS（涂蝶幕系统：白幕/染色/登场授予/变色回合末回复/双切房主先）"
              << std::endl;
    return 0;
}
