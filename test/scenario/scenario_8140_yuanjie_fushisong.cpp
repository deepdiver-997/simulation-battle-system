// 场景 8140 —— K429 德布比特 4499 收尾：缘结·浮世诵 37021 命中期效果 2132
// （对手持失体 → 本次伤害+100%；否则附加 3 回合失体Ⅰ+害怕）
//   —— moves_lib/k429_yuanjie_fushisong_pool.cpp（切片对参/四查/口径记档见实现头）
//
// 断言口径：
//   - 无随机精确断言：2132 两支全无 rand 消费（增伤支直写 ws；否则支害怕恒 100
//     申报短路 + random_anomaly_duration 常量，K176/J331 口径）→ 分支位/到期回合/
//     落地位/槽值全为确定值。
//   - 伤害翻倍 = **同 seed 对照对**：两局 srand 同值，被测分支是唯一 rand 序差异源
//     （增伤支不掷骰、否则支 100% 短路不掷骰——两局 rand 序逐位一致）→ 37021 首用
//     红伤 A == 2×B 精确成立（浮动掷点在公式内、AMP 之前；AMP = checked_pct_mul
//     ×200/100 = ×2 整数无舍入差）。对手掉血 = 红伤 + 837 首用吸取 200（两局同值，
//     差分扣除）。
//   - ws 结构断言：damage_add_pct 停泊后直读（K417 T7950d 同式）。
//   - 联动/隔离：
//     · 2133（J331，选择期回体槽）同载体联动：R1 双否则支各附加各的标记 → R2/R3
//       双持有支（先制+1 结构断言 + 伤害翻倍）→ R4 双过期回落（写即覆盖）；
//     · K417 生产者联动：诸行无常 28285 的 1968 造失体Ⅰ → 37021 的 2132 增伤支
//       读到（跨 dylib 共槽）；
//     · K417 解放者联动：缘起·苍生聚 36552 的 1971 解放失体Ⅰ → 2132 门回落否则支；
//     · 标记类型隔离：回体持有不触发 2132 增伤（2132 只读失体槽）；失体挂己方侧
//       不触发（side 映射反例，J331 T7994 同式）；害怕(6)/疲惫(7) 两码并存互斥不串。
//   - roundCount 映射：人类第 N 回合处理期间 roundCount == N-1（advanceRound 在
//     ROUND_COMPLETION）——到期回合期望值按此折算（K417/J331 场景同款）。
//   - 场景纪律：无 while(!FINISHED)（单回合 cast 推进）、无概率断言、门闭断言用
//     绝对缺席值（-999）。
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
constexpr int kYuanJie = 37021;   // 缘结·浮世诵 [699,1568,2132,2133,837][200,100,400]
                                  // （特殊攻击 power=160 must_hit=1；本单被测载体）
constexpr int kZhuXing = 28285;   // 诸行无常 [1967,1968,191,971][2,3,3,4,4,3,2]
                                  // （K417 1968 失体Ⅰ生产者载体）
constexpr int kYuanQi  = 36552;   // 缘起·苍生聚 [881,1221,1971,1156][1]
                                  // （K417 1971 失体Ⅰ解放者载体）

// K417 段功能槽（契约源 k417_debubite_pool.cpp；0/缺=未持有、到期残留不判持有）。
constexpr int kHuiTiBase = 4695000;   // +side：回体Ⅰ到期回合
constexpr int kShiTiBase = 4695010;   // +side：失体Ⅰ到期回合

// 本单审计槽（与 k429_yuanjie_fushisong_pool.cpp constexpr 逐键一致）。
constexpr int k2132BranchAudit = 1021320001;   // +owner：分支位（1=增伤支 2=否则支）
constexpr int k2132AttachAudit = 1021320002;   // +owner：否则支失体到期回合
constexpr int k2132FearAudit   = 1021320003;   // +owner：害怕落地位（1=落地 0=被挡）

// J331/K417 审计槽（联动断言读；与各自实现 constexpr 逐键一致）。
constexpr int k2133BranchAudit  = 1021330001;   // +self：2133 分支位
constexpr int k2133AttachAudit  = 1021330002;   // +self：2133 回体到期回合
constexpr int k2133FatigueAudit = 1021330003;   // +self：2133 疲惫落地位
constexpr int k1968ModeAudit    = 1019680001;   // K417 1968（1=附加 2=刷新）
constexpr int k1971ReleaseAudit = 1019710001;   // K417 1971 解放位掩码 bit0=回体 bit1=失体

constexpr int kFearId    = static_cast<int>(AbnormalStatusId::Fear);      // 6
constexpr int kFatigueId = static_cast<int>(AbnormalStatusId::Fatigue);   // 7

constexpr int kSelfHp  = 5000;    // 对手 NONE 出手（K417 cast 口径）→ 自身无承伤
constexpr int kFoeHp   = 30000;   // 四回合连战垫够血量（37021×4 + 837 递增吸取 <6k）
constexpr int kFoeHpBig = 90000;  // 联动局（链上 971 吸取窗会吸 1/3×2 次）加垫
constexpr int kSlow = 100;        // 己方慢速（先制支翻转先手 = J331 结构断言同式）
constexpr int kFast = 300;        // 对手高速

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self(const std::vector<int>& carriers) {
    std::array<int, 5> ids{kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa};
    for (std::size_t i = 0; i < carriers.size() && i < 5; ++i) {
        ids[i] = carriers[i];
    }
    ElfPet p = make_pet(1, "己方", ids, /*speed=*/kSlow, /*atk=*/300, /*hp=*/kSelfHp);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(int hp = kFoeHp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/kFast, /*atk=*/300, /*hp=*/hp);
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
    return audit(ctx, key);   // 同一读取面（plugin_storage 直读）
}

// 对手 NONE 出手（K417 scenario_7950 cast 同式）：被测宏的伤害/标记不受对手回合干扰。
void cast(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

bool self_first(BattleContext& ctx) {
    return ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8140a R1 双否则支自然链（2132 否则支 + 2133/J331 联动同载体）═══════
    {
        sbs_srand(81401);
        ScenarioBattle b(make_party(mk_self({kYuanJie})), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(slot(ctx, kShiTiBase + 1), -999, "T8140a 前置：对手无失体标记");
        cast(b, 0);   // R1 37021：2133 否则支（附加回体+疲惫）→ 2132 否则支（附加失体+害怕）
        CHECK_EQ(audit(ctx, k2132BranchAudit + 0), 2, "T8140a R1 分支位=2（否则支）");
        CHECK_EQ(audit(ctx, k2132AttachAudit + 0), 2,
                 "T8140a R1 失体到期=roundCount(0)+3-1=2（附加回合在内）");
        CHECK_EQ(slot(ctx, kShiTiBase + 1), 2, "T8140a R1 失体Ⅰ槽已写入对手侧=2");
        CHECK_EQ(slot(ctx, kShiTiBase + 0), -999, "T8140a R1 己方侧失体槽不被触碰");
        CHECK_EQ(audit(ctx, k2132FearAudit + 0), 1, "T8140a R1 害怕落地位=1（恒 100 申报）");
        CHECK(ctx.has_active_abnormal_status(1, kFearId), "T8140a R1 对手已处于害怕");
        // J331 联动（同载体选择期 2133 也走了否则支）：两码并存隔离断言
        CHECK_EQ(audit(ctx, k2133BranchAudit + 0), 2, "T8140a R1 2133 分支位=2（否则支联动）");
        CHECK_EQ(audit(ctx, k2133AttachAudit + 0), 2, "T8140a R1 2133 回体到期=2（联动）");
        CHECK_EQ(slot(ctx, kHuiTiBase + 0), 2, "T8140a R1 回体Ⅰ槽=2（2133 附加，联动）");
        CHECK_EQ(audit(ctx, k2133FatigueAudit + 0), 1, "T8140a R1 2133 疲惫落地=1（联动）");
        CHECK(ctx.has_active_abnormal_status(1, kFatigueId), "T8140a R1 对手已处于疲惫");
        CHECK(ctx.has_active_abnormal_status(1, kFearId)
                  && ctx.has_active_abnormal_status(1, kFatigueId),
              "T8140a R1 害怕(6)与疲惫(7)并存（两支各落各码，互斥不串）");
        std::cout << "  [T8140a] R1 双否则支+害怕/疲惫并存 通过\n";
    }

    // ══ T8140b 同 seed 对照对：增伤支精确翻倍 + 支间隔离 ═══════════════════
    {
        // B 局（基准）：对手无失体 → 否则支 → 无增伤
        sbs_srand(81402);
        int foe_hp_b = 0;
        {
            ScenarioBattle b(make_party(mk_self({kYuanJie})), make_party(mk_foe()));
            b.start();
            cast(b, 0);   // R1 37021：否则支
            BattleContext& ctx = b.ctx();
            CHECK_EQ(audit(ctx, k2132BranchAudit + 0), 2, "T8140b B 局分支位=2（否则支）");
            CHECK_EQ(audit(ctx, k2132AttachAudit + 0), 2, "T8140b B 局失体附加照常");
            CHECK(ctx.has_active_abnormal_status(1, kFearId), "T8140b B 局对手害怕在身");
            foe_hp_b = b.pet(1).hp;
        }
        // A 局（实验）：白盒预置对手失体Ⅰ（K417 T7950c 同式）→ 增伤支
        sbs_srand(81402);   // 同 seed：两局 rand 序逐位一致（两支均不掷骰）
        int foe_hp_a = 0;
        {
            ScenarioBattle b(make_party(mk_self({kYuanJie})), make_party(mk_foe()));
            b.start();
            b.ctx().plugin_storage[kShiTiBase + 1] = 2;   // 失体Ⅰ到期 2（R1 rc0 持有）
            cast(b, 0);   // R1 37021：增伤支
            BattleContext& ctx = b.ctx();
            CHECK_EQ(audit(ctx, k2132BranchAudit + 0), 1,
                     "T8140b A 局分支位=1（增伤支：对手持失体）");
            CHECK_EQ(audit(ctx, k2132AttachAudit + 0), -999,
                     "T8140b A 局增伤支不写附加审计（绝对缺席）");
            CHECK_EQ(audit(ctx, k2132FearAudit + 0), -999, "T8140b A 局增伤支不害怕");
            CHECK(!ctx.has_active_abnormal_status(1, kFearId),
                  "T8140b A 局对手无害怕（支间隔离：增伤支无否则支后缀）");
            CHECK_EQ(slot(ctx, kShiTiBase + 1), 2,
                     "T8140b A 局增伤支对失体槽只读（白盒预置值保持）");
            foe_hp_a = b.pet(1).hp;
        }
        // 精确翻倍：掉血差 = 红伤 + 837 首用吸取 200（两局同值）。
        const int red_a = kFoeHp - foe_hp_a - 200;
        const int red_b = kFoeHp - foe_hp_b - 200;
        CHECK(red_b > 0, "T8140b B 局红伤>0（载体正常结算）");
        CHECK_EQ(red_a, 2 * red_b,
                 "T8140b 增伤支精确翻倍：red_A == 2×red_B（同 seed 同 rand 序）");
        std::cout << "  [T8140b] 同 seed 对照对 red_b=" << red_b << " red_a=" << red_a
                  << "（精确 ×2）通过\n";
    }

    // ══ T8140c 单局四回合全流程（否则支 → 持有支×2 含边界 → 残留再触）════════
    {
        sbs_srand(81403);
        ScenarioBattle b(make_party(mk_self({kYuanJie})), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：双无标记 → 双否则支；无先制 → 对手先手（速度）。
        cast(b, 0);
        CHECK_EQ(audit(ctx, k2132BranchAudit + 0), 2, "T8140c R1 2132 分支位=2");
        CHECK_EQ(audit(ctx, k2133BranchAudit + 0), 2, "T8140c R1 2133 分支位=2（联动）");
        CHECK(!self_first(ctx), "T8140c R1 无先制 → 对手先手（速度 100<300）");
        // R2：失体 2>=1 持有 → 增伤支；回体 2>=1 持有 → 2133 先制支 +1 → 低速反超先手。
        cast(b, 0);
        CHECK_EQ(audit(ctx, k2132BranchAudit + 0), 1, "T8140c R2 2132 分支位=1（增伤支）");
        CHECK_EQ(slot(ctx, kShiTiBase + 1), 2, "T8140c R2 增伤支只读：失体槽保持 2");
        CHECK_EQ(audit(ctx, k2133BranchAudit + 0), 1, "T8140c R2 2133 分支位=1（先制支联动）");
        CHECK(self_first(ctx), "T8140c R2 2133 先制+1 → 低速 100 反超 300（联动结构断言）");
        // R3：双标记 2>=2（含当回合边界）→ 双持有支仍走。
        cast(b, 0);
        CHECK_EQ(audit(ctx, k2132BranchAudit + 0), 1,
                 "T8140c R3 到期=当回合仍判持有 → 增伤支（边界）");
        CHECK_EQ(audit(ctx, k2133BranchAudit + 0), 1, "T8140c R3 2133 仍先制支（边界联动）");
        CHECK(self_first(ctx), "T8140c R3 仍先手（边界内持有）");
        // R4：双标记 2<3 到期残留不判持有 → 双否则支再触；写即覆盖，新到期=3+3-1=5。
        cast(b, 0);
        CHECK_EQ(audit(ctx, k2132BranchAudit + 0), 2, "T8140c R4 到期残留 → 否则支再触");
        CHECK_EQ(audit(ctx, k2132AttachAudit + 0), 5,
                 "T8140c R4 写即覆盖残留：新到期=roundCount(3)+3-1=5");
        CHECK_EQ(slot(ctx, kShiTiBase + 1), 5, "T8140c R4 失体槽已刷新为 5");
        CHECK_EQ(audit(ctx, k2132FearAudit + 0), 1, "T8140c R4 害怕再次落地");
        CHECK(ctx.has_active_abnormal_status(1, kFearId), "T8140c R4 对手害怕仍在（再附加）");
        CHECK_EQ(audit(ctx, k2133BranchAudit + 0), 2, "T8140c R4 2133 亦回落否则支（联动）");
        CHECK(!self_first(ctx), "T8140c R4 无先制 → 对手先手（速度）");
        std::cout << "  [T8140c] 四回合全流程（否则支→持有支×2→残留再触）通过\n";
    }

    // ══ T8140d K417 生产者联动：诸行无常 1968 造失体Ⅰ → 2132 增伤支读到 ════
    {
        sbs_srand(81404);
        ScenarioBattle b(make_party(mk_self({kZhuXing, kYuanJie})), make_party(mk_foe(kFoeHpBig)));
        b.start();
        BattleContext& ctx = b.ctx();
        cast(b, 0);    // R1 诸行无常：1968 附加失体Ⅰ（K417 生产者）
        CHECK_EQ(audit(ctx, k1968ModeAudit), 1, "T8140d R1 K417 1968 附加档（生产者前置）");
        CHECK_EQ(slot(ctx, kShiTiBase + 1), 2, "T8140d R1 失体Ⅰ槽=2（K417 段写入）");
        cast(b, 1);    // R2 37021：2132 读 K417 造的标记 → 增伤支（跨 dylib 共槽消费）
        CHECK_EQ(audit(ctx, k2132BranchAudit + 0), 1,
                 "T8140d R2 2132 增伤支读到 K417 1968 造的失体Ⅰ（跨族联动）");
        CHECK_EQ(audit(ctx, k2132AttachAudit + 0), -999, "T8140d R2 增伤支无附加审计");
        CHECK_EQ(slot(ctx, kShiTiBase + 1), 2, "T8140d R2 失体槽值保持（增伤支只读）");
        std::cout << "  [T8140d] K417 生产者联动（1968→2132）通过\n";
    }

    // ══ T8140e K417 解放者联动：1971 解放失体Ⅰ → 2132 门回落否则支 ═════════
    {
        sbs_srand(81405);
        ScenarioBattle b(make_party(mk_self({kYuanQi, kYuanJie})), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[kShiTiBase + 1] = 2;   // 白盒预置对手失体Ⅰ（K417 T7950c 同式）
        cast(b, 0);    // R1 缘起·苍生聚：1971 未击败 → 解放对手失体Ⅰ（掩码 bit1）
        CHECK(b.pet(1).hp > 0, "T8140e R1 对手存活（未击败前提）");
        CHECK_EQ(audit(ctx, k1971ReleaseAudit), 2,
                 "T8140e R1 解放位掩码=2（仅失体Ⅰ；自身无回体）");
        CHECK_EQ(slot(ctx, kShiTiBase + 1), -999, "T8140e R1 失体Ⅰ已解放（槽擦除）");
        cast(b, 1);    // R2 37021：无失体（被 1971 解放）→ 否则支（门回落）
        CHECK_EQ(audit(ctx, k2132BranchAudit + 0), 2,
                 "T8140e R2 2132 门回落否则支（1971 解放清门）");
        CHECK_EQ(audit(ctx, k2132AttachAudit + 0), 3,
                 "T8140e R2 失体到期=roundCount(1)+3-1=3（重新附加）");
        CHECK_EQ(slot(ctx, kShiTiBase + 1), 3, "T8140e R2 失体槽已重写=3");
        CHECK_EQ(audit(ctx, k2132FearAudit + 0), 1, "T8140e R2 害怕落地");
        std::cout << "  [T8140e] K417 解放者联动（1971→2132 门回落）通过\n";
    }

    // ══ T8140f 标记类型隔离：回体持有不触发 2132 增伤（只读失体槽）══════════
    {
        sbs_srand(81406);
        ScenarioBattle b(make_party(mk_self({kYuanJie})), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[kHuiTiBase + 0] = 2;   // 白盒预置自身回体Ⅰ（2133 会消费）
        cast(b, 0);    // R1 37021：2133 先制支（回体在）→ 2132 不读回体 → 否则支
        CHECK_EQ(audit(ctx, k2133BranchAudit + 0), 1,
                 "T8140f 前置有效：2133 先制支吃到白盒回体（预置被消费）");
        CHECK_EQ(audit(ctx, k2132BranchAudit + 0), 2,
                 "T8140f 回体持有不触发 2132 增伤（标记类型隔离：只读失体槽）");
        CHECK_EQ(audit(ctx, k2132AttachAudit + 0), 2, "T8140f 否则支失体附加照常");
        CHECK_EQ(audit(ctx, k2132FearAudit + 0), 1, "T8140f 害怕照常落地");
        std::cout << "  [T8140f] 回体隔离（2132 只读失体槽）通过\n";
    }

    // ══ T8140g side 映射反例：失体挂己方侧 → 条件不成立（J331 T7994 同式）════
    {
        sbs_srand(81407);
        ScenarioBattle b(make_party(mk_self({kYuanJie})), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[kShiTiBase + 0] = 2;   // 挂错侧：己方失体
        cast(b, 0);
        CHECK_EQ(audit(ctx, k2132BranchAudit + 0), 2,
                 "T8140g 己方侧失体不触发增伤支（读对手侧槽 kShiTiBase+target）");
        CHECK_EQ(audit(ctx, k2132AttachAudit + 0), 2, "T8140g 否则支照常附加对手");
        std::cout << "  [T8140g] side 映射反例 通过\n";
    }

    std::cout << "\n场景 8140 通过：2132 增伤支（同 seed 精确 ×2 + 支间隔离）/否则支"
                 "（附加 3 回合+害怕，无随机精确断言）/四回合全流程（边界+残留写即覆盖）/"
                 "K417 生产-解放双向联动/J331 同载体联动（害怕与疲惫并存）/回体隔离+"
                 "side 映射反例，审计槽+标记槽直读+先手权结构断言全数命中。\n";
    return 0;
}
