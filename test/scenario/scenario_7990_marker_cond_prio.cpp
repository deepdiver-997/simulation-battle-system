// 场景 7990 —— J331 标记门条件先制族：1940/1941（回能Ⅰ/失能Ⅰ门 +2）+
//                2133（回体门 +1 / 否则支附加回体+疲惫）
//                （moves_lib/j331_marker_cond_prio.cpp；号段 7990-7999，
//                2026-11-06，worktree ../j331）
//
// DB 侧前置：custom_skill_selection_routes 32 行态（j4/selection_routes_expansion_
// 3.sql 已 apply——缺扩行补丁时 1940/1941/2133 无路由行 → 本场景全部转红）。
//
// 断言口径（6450 条件先制结构断言 + 7950/7960 标记槽直读同款）：
//   - **先手权结构断言**：先制等级不等时先手权只看等级不看速度（battleFsm
//     handle_BattleFirstMoveRight）→ 己方速度 100 < 对手 300 仍抢到先手 =
//     先制量确实进了 preemptive_level；门闭时等级相等 → 回落速度 → 对手先手。
//   - 审计槽（10<id>000x 段，与实现 constexpr 逐键一致）：
//     1019400001(+self)=1940 授予的先制量（缺席=-999=门闭）；
//     1019410001(+self)=1941 授予的先制量（缺席=门闭）；
//     1021330001(+self)=2133 分支位（1=先制支 2=否则支）；
//     1021330002(+self)=2133 否则支写入的回体到期回合；
//     1021330003(+self)=2133 疲惫落地位（1=落地）。
//   - 标记槽直读（K417/K418 段契约，0/缺=未持有、到期残留不判持有）：
//     回能 4696000+side / 失能 4696010+side（K418）；回体 4695000+side（K417）。
//   - 2133 全流程单局四回合驱动（无随机断言：疲惫恒 100 申报短路不耗 rand、
//     random_anomaly_duration 常量——先制/槽值/异常全为确定值）：
//     ⚠️ roundCount 在选技期（FIRST_MOVE_RIGHT 桶）为本回合 **0 基**序号
//     （R1 选取时=0）——附加 3 回合到期 = roundCount+3-1 = R1+2 = 2（0 基下
//     roundCount 0/1/2 三次选取判持有，与"附加回合在内"语义一致，K417 1970
//     同款公式）：
//     R1 无回体 → 否则支（附加 3 回合到期=2 + 对手疲惫）→ 无先制 → 对手先手；
//     R2 回体持有（2>=1）→ 先制支 +1 → 低速反超先手（槽值不被先制支改动）；
//     R3 回体仍持有（2>=2，含当回合边界）→ 仍先手；
//     R4 到期残留（2<3）→ 不判持有 → 否则支再触（写即覆盖残留，新到期=3+2=5）。
//   - 1941 条件读**对手侧**槽：失能标记挂己方侧不触发（side 映射反例）。
//   - 场景纪律：无 while(!FINISHED)、无概率断言、门闭断言用绝对缺席值（-999）。
#include <iostream>
#include <string>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打：物40（filler，PP35）
constexpr int kShouYi  = 36503;   // 守一 [1940,1941,1922,1776]（1940/1941 载体，PP20）
constexpr int kYuanJie = 37021;   // 缘结·浮世诵 [699,1568,2132,2133,837]（2133 载体，PP5）
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 9999;    // 四回合连战垫够血量

// K417/K418 段功能槽（与 j331_marker_cond_prio.cpp 头注契约一致）。
constexpr int kHuiNengBase = 4696000;   // +side：回能Ⅰ到期回合
constexpr int kShiNengBase = 4696010;   // +side：失能Ⅰ到期回合
constexpr int kHuiTiBase   = 4695000;   // +side：回体Ⅰ到期回合

// J331 审计槽（与实现 constexpr 逐键一致）。
constexpr int k1940Audit        = 1019400001;
constexpr int k1941Audit        = 1019410001;
constexpr int k2133BranchAudit  = 1021330001;
constexpr int k2133AttachAudit  = 1021330002;
constexpr int k2133FatigueAudit = 1021330003;

constexpr int kSlow = 100;   // 己方慢速（先制等级不等时仍抢到先手 = 硬证据）
constexpr int kFast = 300;   // 对手高速

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/kSlow, /*atk=*/300, /*hp=*/kSelfHp);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/kFast, /*atk=*/300, /*hp=*/kFoeHp);
    accuracy_100(p);
    return p;
}

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

int slot(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -999;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -999;
}

void play(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/0);
    b.drive();
}

bool owner_first(BattleContext& ctx) {
    return ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261106);

    // ══ T7990 1940 正例：自身持回能标记（到期 5>=R1）→ 先制+2 → 低速反超先手 ══
    {
        ScenarioBattle b(make_party(mk_self(kShouYi)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[kHuiNengBase + 0] = 5;   // 回能Ⅰ到期回合 5（持有）
        play(b);
        std::cout << "  [T7990] 1940 正例 audit=" << audit(ctx, k1940Audit + 0)
                  << " owner_first=" << owner_first(ctx) << std::endl;
        CHECK_EQ(audit(ctx, k1940Audit + 0), 2, "T7990 1940 审计槽=授予的先制量 2");
        CHECK_EQ(audit(ctx, k1941Audit + 0), -999,
                 "T7990 隔离：对手无失能标记 → 1941 门闭（审计缺席）");
        CHECK(owner_first(ctx), "T7990 1940 正例：先制+2 抢到先手（低速 100 反超 300）");
        CHECK_EQ(slot(ctx, kHuiNengBase + 0), 5,
                 "T7990 1940 对功能槽只读：回能槽值不被路由体改动");
    }

    // ══ T7991 1940 反例：无回能标记 → 门闭 → 对手先手（速度回落）══
    {
        ScenarioBattle b(make_party(mk_self(kShouYi)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        play(b);
        CHECK_EQ(audit(ctx, k1940Audit + 0), -999, "T7991 门闭 → 审计未写（绝对缺席）");
        CHECK_EQ(audit(ctx, k1941Audit + 0), -999, "T7991 1941 亦门闭（无失能标记）");
        CHECK(!owner_first(ctx), "T7991 1940 反例：无标记 → 不加先制 → 对手先手（速度）");
    }

    // ══ T7992 1940 到期残留反例：回能槽=0（0/缺=未持有）→ 门闭 ══
    {
        ScenarioBattle b(make_party(mk_self(kShouYi)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[kHuiNengBase + 0] = 0;   // 残留条目（0=未持有口径）
        play(b);
        CHECK_EQ(audit(ctx, k1940Audit + 0), -999, "T7992 残留条目不判持有 → 审计未写");
        CHECK(!owner_first(ctx), "T7992 1940 到期残留：门闭 → 对手先手");
    }

    // ══ T7993 1941 正例：对手持失能标记（到期 5>=R1）→ 先制+2 → 低速反超 ══
    {
        ScenarioBattle b(make_party(mk_self(kShouYi)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[kShiNengBase + 1] = 5;   // 对手侧失能Ⅰ到期回合 5
        play(b);
        CHECK_EQ(audit(ctx, k1941Audit + 0), 2, "T7993 1941 审计槽=授予的先制量 2");
        CHECK_EQ(audit(ctx, k1940Audit + 0), -999,
                 "T7993 隔离：自身无回能标记 → 1940 门闭（审计缺席）");
        CHECK(owner_first(ctx), "T7993 1941 正例：先制+2 抢到先手（低速反超）");
        CHECK_EQ(slot(ctx, kShiNengBase + 1), 5, "T7993 1941 对失能槽只读");
    }

    // ══ T7994 1941 side 映射反例：失能标记挂**己方**侧 → 条件不成立 ══
    //     （1941 读对手侧槽 kShiNengBase+target；己方侧持有不构成条件。）
    {
        ScenarioBattle b(make_party(mk_self(kShouYi)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[kShiNengBase + 0] = 5;   // 挂错侧：己方失能
        play(b);
        CHECK_EQ(audit(ctx, k1941Audit + 0), -999,
                 "T7994 己方侧失能不触发 1941（side 映射反例）");
        CHECK(!owner_first(ctx), "T7994 1941 反例：条件不成立 → 对手先手");
    }

    // ══ T7995-T7998 2133 单局四回合全流程（否则支 → 先制支 ×2 → 残留再触）══
    {
        ScenarioBattle b(make_party(mk_self(kYuanJie)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();

        // T7995 R1：无回体标记 → 否则支：附加 3 回合（到期=R1+2=3）+ 对手疲惫；
        //           无先制 → 对手先手（速度）。
        CHECK_EQ(slot(ctx, kHuiTiBase + 0), -999, "T7995 前置：自身无回体标记");
        play(b);
        std::cout << "  [T7995] 2133 R1 branch=" << audit(ctx, k2133BranchAudit + 0)
                  << " expiry=" << audit(ctx, k2133AttachAudit + 0)
                  << " fatigue=" << audit(ctx, k2133FatigueAudit + 0)
                  << " owner_first=" << owner_first(ctx) << std::endl;
        CHECK_EQ(audit(ctx, k2133BranchAudit + 0), 2, "T7995 R1 分支位=2（否则支）");
        CHECK_EQ(audit(ctx, k2133AttachAudit + 0), 2,
                 "T7995 R1 回体到期回合=roundCount(0)+3-1=2（附加回合在内，0 基）");
        CHECK_EQ(slot(ctx, kHuiTiBase + 0), 2, "T7995 回体槽已写入到期回合 2");
        CHECK_EQ(slot(ctx, kHuiTiBase + 1), -999, "T7995 对手侧回体槽不被触碰");
        CHECK_EQ(audit(ctx, k2133FatigueAudit + 0), 1, "T7995 疲惫落地位=1（恒 100 申报）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)),
              "T7995 对手已处于疲惫（否则支落地）");
        CHECK(!owner_first(ctx), "T7995 R1 无先制 → 对手先手（速度）");

        // T7996 R2：回体持有（2>=1）→ 先制支 +1 → 低速反超先手；先制支不改槽值。
        play(b);
        std::cout << "  [T7996] 2133 R2 branch=" << audit(ctx, k2133BranchAudit + 0)
                  << " owner_first=" << owner_first(ctx) << std::endl;
        CHECK_EQ(audit(ctx, k2133BranchAudit + 0), 1, "T7996 R2 分支位=1（先制支）");
        CHECK(owner_first(ctx), "T7996 R2 先制+1 抢到先手（低速 100 反超 300）");
        CHECK_EQ(slot(ctx, kHuiTiBase + 0), 2,
                 "T7996 先制支只读标记：回体槽值保持 2（不被重置）");
        CHECK_EQ(audit(ctx, k2133AttachAudit + 0), 2,
                 "T7996 先制支不写附加审计（R1 值保持）");

        // T7997 R3：回体仍持有（2>=2，含当回合边界）→ 仍先手。
        play(b);
        CHECK_EQ(audit(ctx, k2133BranchAudit + 0), 1, "T7997 R3 分支位=1（含当回合持有）");
        CHECK(owner_first(ctx), "T7997 R3 到期回合=当回合仍判持有 → 仍先手");

        // T7998 R4：到期残留（2<3）→ 不判持有 → 否则支再触：写即覆盖，新到期=3+2=5。
        play(b);
        std::cout << "  [T7998] 2133 R4 branch=" << audit(ctx, k2133BranchAudit + 0)
                  << " expiry=" << audit(ctx, k2133AttachAudit + 0)
                  << " owner_first=" << owner_first(ctx) << std::endl;
        CHECK_EQ(audit(ctx, k2133BranchAudit + 0), 2,
                 "T7998 R4 到期残留不判持有 → 否则支再触");
        CHECK_EQ(audit(ctx, k2133AttachAudit + 0), 5,
                 "T7998 R4 写即覆盖残留：新到期回合=roundCount(3)+3-1=5");
        CHECK_EQ(slot(ctx, kHuiTiBase + 0), 5, "T7998 回体槽已刷新为 5");
        CHECK_EQ(audit(ctx, k2133FatigueAudit + 0), 1, "T7998 疲惫再次落地");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)),
              "T7998 对手疲惫仍在（再附加）");
        CHECK(!owner_first(ctx), "T7998 R4 否则支无先制 → 对手先手");
    }

    std::cout << "\n场景 7990 通过：J331 标记门条件先制族——1940 正/反/残留三态、"
                 "1941 正例+side 映射反例（对手侧槽）、2133 单局四回合全流程"
                 "（否则支附加 3 回合+疲惫 → 先制支低速反超 ×2 → 到期残留边界"
                 "写即覆盖），先手权结构断言+审计槽+标记槽直读全数命中。\n";
    return 0;
}
