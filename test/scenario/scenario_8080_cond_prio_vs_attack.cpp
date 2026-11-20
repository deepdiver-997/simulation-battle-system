// 场景 8080 —— J335 选择期路由扩行三批：1774「若对手当回合使用的技能为攻击技能
//   则自身额外先制+1」（号段 8080-8089，K425 分配段续用，K425 未占用；2026-11-07，
//   worktree ../j335）（moves_lib/j335_cond_prio_vs_attack.cpp +
//   tools/db_patches/j4/selection_routes_expansion_5.sql 36→37 行态——缺扩行补丁时
//   1774 无路由行 → 结构断言 T8080a 转红；缺插件 dylib 时 Skills 构造 fail-closed
//   「clone 无实现」直接 throw）
//
// 断言口径（7990/8000 同款先手权结构断言 + 审计槽直读）：
//   - **先手权结构断言**（硬于数值，§七点五口径）：先制等级不等时先手权只看等级
//     不看速度（battleFsm handle_BattleFirstMoveRight）→ 己方速度 100 < 对手 300
//     仍先手 = 先制量确实进了 preemptive_level；门闭时等级相等 → 回落速度 → 对手
//     先手。镜像用（对手持 1774、速度低）反向同式。
//   - ws.preemptive_level 直读（8000 同款）+ 审计槽（10<id>000x 段，与实现 constexpr
//     逐键一致；缺席=-999 绝对缺席值）：1017740001(+owner)=授予量（1=门开）。
//   - 门判据 = 对手本回合所选技能（roundChoice，选择期写定、MOVE_RIGHT 桶保留）：
//     攻击技（物理/特殊）开门；属性技/嗑药（USE_MEDICINE）门闭。嗑药反例同时锁
//     "medicine 规则让嗑药方后手"与"门闭审计缺席"两个正交面。
//   - 1395 联动面（35652 链上兄弟"若先出手则必定打出致命一击"）：1774 抢到的先手
//     会让 1395 落地（crit_happened 置位）——门闭对照下 crit 不置位，构成
//     "先制+1 真的改变了出手顺序"的**下游行为面**证据（hard 于仅断等级数值）。
//   - 载体代表 3/8：36277 雾停枝（纯单载体 [1774]）、28051 启明之辰（属性技链
//     [1774,191,847,1342]——路由随效果号不随技别）、35652 几曾识干戈（物理链
//     [697,1568,1774,1395] 全零参）。载体静态与切片见实现头行清单。
//   - 场景纪律：无 while(!FINISHED)、无概率断言（全部技能 must_hit/accuracy 强制
//     100 或固有必中；剑舞 100% 档 roll_percent 短路不耗 rand；seed 固定隔离）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打（物理攻击填充，[ ]无链）
constexpr int kJianWu  = 20010;   // 剑舞（属性技填充：[4][0,100,2]，100% 档零 rand）
constexpr int kWuTing  = 36277;   // 雾停枝 [1774]（特殊 p130，纯单载体）
constexpr int kQiMing  = 28051;   // 启明之辰 [1774,191,847,1342]（属性 p0 链载体）
constexpr int kJiGan   = 35652;   // 几曾识干戈 [697,1568,1774,1395]（物理 p150 全零参链）
constexpr int kMaxHp   = 100000;

// J335 审计槽（与 j335_cond_prio_vs_attack.cpp constexpr 逐键一致）。
constexpr int k1774Audit = 1017740001;   // +owner：授予的先制量（缺席=门闭）

constexpr int kSlow = 100;   // 己方慢速（先制等级不等时仍先手 = 硬证据）
constexpr int kFast = 300;   // 对手高速

int audit(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -999;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -999;
}

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
        s.critical_strike_rate = 0.0f;
    }
}

// 载体宠（35652 固有必中 + 全体强制必中/零暴击：全部确定性）。
ElfPet carrier_pet(int carrier, int speed) {
    ElfPet p = make_pet(1, "载体侧", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/speed, /*atk=*/300, /*hp=*/kMaxHp);
    accuracy_100(p);
    return p;
}

// 对手宠（攻击技 = 拍打；属性技 = 剑舞——slot 0/1 由用例选）。
ElfPet foe_pet(int speed) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kJianWu, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/speed, /*atk=*/300, /*hp=*/kMaxHp);
    accuracy_100(p);
    return p;
}

void play(ScenarioBattle& b, int my_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, my_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

bool owner_first(BattleContext& ctx) {
    return ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1;
}

bool foe_first(BattleContext& ctx) {
    return ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2;
}

// 路由节点结构断言：1774 在 selection_effects_、不在普通分支（路由消费后 push 后
// continue——DB 行缺失时 1774 会落普通分支，本断言即转红）。
int count_route_nodes(const Skills& s, int eid) {
    int n = 0;
    for (const SkillEffectNode& node : s.selection_effects_) {
        if (node.effect.id == eid) {
            ++n;
        }
    }
    return n;
}
int count_branch_nodes(const Skills& s, int eid) {
    int n = 0;
    for (const auto& [result, nodes] : s.effectBranches) {
        (void)result;
        for (const SkillEffectNode& node : nodes) {
            if (node.effect.id == eid) {
                ++n;
            }
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

    // ══ T8080a 结构断言：1774 是选择期路由节点，不在普通分支 ════════════════
    {
        for (const int carrier : {kWuTing, kQiMing, kJiGan}) {
            const Skills s(carrier);
            CHECK_EQ(count_route_nodes(s, 1774), 1,
                     "T8080a 载体 " + std::to_string(carrier)
                         + " 选择期恰 1 个 1774 路由节点（DB 扩行 + 插件注册缺一即红）");
            CHECK_EQ(count_branch_nodes(s, 1774), 0,
                     "T8080a 载体 " + std::to_string(carrier)
                         + " 普通分支零 1774（路由行消费后 push 后 continue）");
            for (const SkillEffectNode& node : s.selection_effects_) {
                if (node.effect.id == 1774) {
                    CHECK(node.registerState == State::BATTLE_FIRST_MOVE_RIGHT,
                          "T8080a 载体 " + std::to_string(carrier)
                              + " 1774 节点 registerState=FIRST_MOVE_RIGHT");
                    CHECK((node.effect.args.owned_int_args.size() == 2),
                          "T8080a 载体 " + std::to_string(carrier)
                              + " 零参 plain 喂参 [owner,target]");
                }
            }
        }
        std::cout << "  [T8080a] 3 载体路由结构断言（selection 恰 1 / 普通分支 0） 通过\n";
    }

    // ══ T8080b 正例（结构断言）：对手选拍打（攻击技）→ 先制+1 → 低速反超先手 ══
    {
        sbs_srand(80801);
        ScenarioBattle b(make_party(carrier_pet(kWuTing, kSlow)),
                         make_party(foe_pet(kFast)));
        b.start();
        BattleContext& ctx = b.ctx();
        play(b, /*my_slot=*/0, /*foe_slot=*/0);   // 我方雾停枝 vs 对手拍打
        CHECK_EQ(audit(ctx, k1774Audit + 0), 1,
                 "T8080b 对手选攻击技 → 门开 → 审计=授予量 1");
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T8080b 先制等级 0→1（+1 模板文案量）");
        CHECK_EQ(ctx.ws.preemptive_level[1], 0, "T8080b 对手无先制来源 → 等级 0");
        CHECK(owner_first(ctx),
              "T8080b 结构断言：速度 100<300 仍先手 = 等级不等时只看等级");
        std::cout << "  [T8080b] 攻击技门正例（低速反超先手） 通过\n";
    }

    // ══ T8080c 属性技反例：对手选剑舞（属性技）→ 门闭 → 回落速度对手先手 ═════
    // （35652 兼作 crit 联动对照：门闭 → 非先手 → 1395 不落地 → crit 不置位）
    {
        sbs_srand(80802);
        ScenarioBattle b(make_party(carrier_pet(kJiGan, kSlow)),
                         make_party(foe_pet(kFast)));
        b.start();
        BattleContext& ctx = b.ctx();
        play(b, /*my_slot=*/0, /*foe_slot=*/1);   // 我方几曾识干戈 vs 对手剑舞
        CHECK_EQ(audit(ctx, k1774Audit + 0), -999,
                 "T8080c 对手选属性技 → 门闭 → 审计绝对缺席");
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "T8080c 门闭 → 先制等级 0");
        CHECK_EQ(ctx.ws.preemptive_level[1], 0, "T8080c 对手等级 0（剑舞无先制半句）");
        CHECK(foe_first(ctx),
              "T8080c 门闭 → 等级相等 → 回落速度 → 高速对手先手");
        CHECK(!ctx.crit_happened[0],
              "T8080c 1395 联动对照：非先手 → 必暴不落地（crit 未置位）");
        std::cout << "  [T8080c] 属性技门反例（速度回落 + crit 对照） 通过\n";
    }

    // ══ T8080d 嗑药反例：对手嗑药（USE_MEDICINE）→ 门闭；medicine 规则嗑药方后手 ═
    // （满血嗑药钳 0 跳过出手；roundChoice[1][0]=USE_MEDICINE ≠ SELECT_SKILL）
    {
        const MedicineStock foe_meds = {{300011, 3}};   // 初级体力药剂 ×3（满血钳 0）
        sbs_srand(80803);
        ScenarioBattle b(make_party(carrier_pet(kWuTing, kSlow)),
                         make_party(foe_pet(kFast)), {}, foe_meds);
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        CHECK_EQ(audit(ctx, k1774Audit + 0), -999,
                 "T8080d 对手嗑药（非选技动作）→ 门闭 → 审计绝对缺席");
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "T8080d 门闭 → 先制等级 0");
        CHECK(owner_first(ctx),
              "T8080d medicine 规则：嗑药方后手（本方先手非来自 1774）");
        CHECK(ctx.getPet(1).hp < kMaxHp,
              "T8080d 我方攻击照常落地（对手掉血 = 先手出手面成立）");
        std::cout << "  [T8080d] 嗑药门反例（规则后手 + 审计缺席） 通过\n";
    }

    // ══ T8080e 镜像正例：对手持 1774 低速 → 门开抢先手（反向结构断言）════════
    {
        sbs_srand(80804);
        ScenarioBattle b(make_party(carrier_pet(kPaiDa, kFast)),
                         make_party(carrier_pet(kWuTing, kSlow)));
        b.start();
        BattleContext& ctx = b.ctx();
        play(b, /*my_slot=*/0, /*foe_slot=*/0);   // 我方拍打 vs 对手雾停枝
        CHECK_EQ(audit(ctx, k1774Audit + 1), 1,
                 "T8080e 对手侧门开 → 审计写 +1 侧别键");
        CHECK_EQ(audit(ctx, k1774Audit + 0), -999,
                 "T8080e 我方拍打无 1774 → 侧别键互不覆盖");
        CHECK_EQ(ctx.ws.preemptive_level[1], 1, "T8080e 对手先制等级 1");
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "T8080e 我方等级 0");
        CHECK(foe_first(ctx),
              "T8080e 结构断言：对手速度 100<300 仍先手（镜像硬证据）");
        std::cout << "  [T8080e] 镜像正例（对手低速持 1774 抢先手） 通过\n";
    }

    // ══ T8080f 双持镜像：双方均选攻击技 → 双门开 → 等级相等回落速度 ══════════
    {
        sbs_srand(80805);
        ScenarioBattle b(make_party(carrier_pet(kWuTing, kSlow)),
                         make_party(carrier_pet(kWuTing, kFast)));
        b.start();
        BattleContext& ctx = b.ctx();
        play(b, /*my_slot=*/0, /*foe_slot=*/0);
        CHECK_EQ(audit(ctx, k1774Audit + 0), 1, "T8080f 我方门开（对手选拍打系攻击技）");
        CHECK_EQ(audit(ctx, k1774Audit + 1), 1, "T8080f 对手门开（我方选攻击技）");
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T8080f 双方等级 1（+1 对称）");
        CHECK_EQ(ctx.ws.preemptive_level[1], 1, "T8080f 双方等级 1");
        CHECK(foe_first(ctx),
              "T8080f 等级相等 → 回落速度 → 高速对手先手（先制不改写速度规则）");
        std::cout << "  [T8080f] 双持镜像等价对照 通过\n";
    }

    // ══ T8080g 属性链载体 28051：路由随效果号不随技别 → 属性技也能抢先手 ═════
    {
        sbs_srand(80806);
        ScenarioBattle b(make_party(carrier_pet(kQiMing, kSlow)),
                         make_party(foe_pet(kFast)));
        b.start();
        BattleContext& ctx = b.ctx();
        play(b, /*my_slot=*/0, /*foe_slot=*/0);   // 启明之辰（属性） vs 拍打
        CHECK_EQ(audit(ctx, k1774Audit + 0), 1,
                 "T8080g 属性链载体门开（门判据看对手不看己方技别）");
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T8080g 先制等级 1");
        CHECK(owner_first(ctx), "T8080g 低速反超先手");
        std::cout << "  [T8080g] 属性链载体 28051 正例 通过\n";
    }

    // ══ T8080h 物理链载体 35652 + 1395 联动：先制+1 → 先手 → 链上必暴落地 ════
    {
        sbs_srand(80807);
        ScenarioBattle b(make_party(carrier_pet(kJiGan, kSlow)),
                         make_party(foe_pet(kFast)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_before = ctx.getPet(1).hp;
        play(b, /*my_slot=*/0, /*foe_slot=*/0);   // 几曾识干戈 vs 拍打
        CHECK_EQ(audit(ctx, k1774Audit + 0), 1, "T8080h 物理链载体门开");
        CHECK(owner_first(ctx), "T8080h 低速反超先手");
        CHECK(ctx.crit_happened[0],
              "T8080h 1395 联动：1774 抢到的先手让链上「若先出手则必暴」落地");
        CHECK(ctx.getPet(1).hp < foe_before, "T8080h 对手掉血（攻击结算面成立）");
        std::cout << "  [T8080h] 物理链载体 35652 + 1395 先手必暴联动 通过\n";
    }

    std::cout << "\n场景 8080 通过：J335 扩行三批——1774 当回合条件先制（对手选攻击技"
                 "→先制+1）：路由结构 3 载体、攻击技正例（低速反超）、属性技/嗑药双反例"
                 "（回落速度/规则后手 + 审计缺席）、镜像双向结构断言、双持等价、1395"
                 " 先手必暴联动，审计槽直读全数命中。\n";
    return 0;
}
