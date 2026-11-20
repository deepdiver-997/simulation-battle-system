#include <utils/sbs_rand.h>
// 场景 7950 —— K417 逐宠定制池二批：德布比特 4499 回体Ⅰ/失体Ⅰ标记族
// （1966 消强+双增伤下回合窗 / 1967 回体标记→回合类免消除窗 / 1968 失体Ⅰ附加/刷新 /
//   1969 失体标记→强化保护窗 / 1970 回体Ⅰ附加/刷新 / 1971 未击败→解放双标记+锁切+先制失效）
//   —— moves_lib/k417_debubite_pool.cpp（切片对参/四查/口径记档见实现头）
//
// 断言口径：
//   - 标记生命周期全部走实现侧审计槽（10<id>000x 段）+ 功能槽 4695xxx 段直读，
//     无随机精确断言；行为断言只做精确量（清除项数、到期回合、粉伤实放额、
//     on_stage 锁切前后、ws 结构字段）。
//   - 回体Ⅰ/失体Ⅰ标记槽（4695000+side / 4695010+side）是本批实现细节，场景直读
//     既是断言面也是后续任务（2132/2133 缘结·浮世诵）的对接验收面。
//   - 1967/1969 免疫断言用**原语直调**（scenario_090 F3 remove_round_effects==IMMUNE
//     同式）：1967 → remove_round_effects(自身)=IMMUNE；1969 → clear_stat_boosts/
//     transfer_stat_boosts 被票挡下返回 0 且槽位不变。
//   - "后出手则延续1回合"用慢速载体局（speed 50 vs 300）审计槽精确断言 2+1=3。
//   - 1966 攻击增伤走 ws.damage_add_pct 停泊后结构断言（威力改写族伤害比例断言
//     不可对齐——SOP §五.1）；粉伤增幅在 R4 窗内 debug 停泊后 deal_pink_damage
//     直调（粉管线直调可测——套路目录 §七），审计槽断言实放额 500。
//   - 1971 未击败门：KO 支对手压血 1 点真杀（行为走真实技能结算），审计槽保持
//     未写 = 门关；未击败支由 AFTER_ACTION 延迟节点（57 形双桶）落地。
//   - roundCount 映射：人类第 N 回合处理期间 roundCount == N-1（advanceRound 在
//     ROUND_COMPLETION）——所有到期回合期望值按此折算。
#include <any>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;          // 拍打（物理攻击填充）
constexpr int kMaxHp = 100000;

// 载体技（链与切片见实现文件头）
constexpr int kZhuFaWuWo = 28286;     // 诸法无我 [1969,1970,1283,843][2,3,3,1,2,2]
constexpr int kZhuXingWuChang = 28285;  // 诸行无常 [1967,1968,191,971][2,3,3,4,4,3,2]
constexpr int kGuiYi = 36548;         // 归一 [1138,1966][100,7,100,6,100,50]
constexpr int kYuanQi = 36552;        // 缘起·苍生聚 [881,1221,1971,1156][1]

// 实现文件账本键（10<id>000x 审计段 + 4695xxx 功能段，与实现 constexpr 一致）
constexpr int k1966ClearAudit   = 1019660001;
constexpr int k1966AtkAudit     = 1019660002;
constexpr int k1966PinkAudit    = 1019660003;
constexpr int k1966PinkApplied  = 1019660004;
constexpr int k1967Audit        = 1019670001;
constexpr int k1968ModeAudit    = 1019680001;
constexpr int k1968ExpiryAudit  = 1019680002;
constexpr int k1969Audit        = 1019690001;
constexpr int k1970ModeAudit    = 1019700001;
constexpr int k1970ExpiryAudit  = 1019700002;
constexpr int k1971ReleaseAudit = 1019710001;
constexpr int k1971LockAudit    = 1019710002;
constexpr int k1971NullifyAudit = 1019710003;
constexpr int kHuiTiBase        = 4695000;   // +side：回体Ⅰ到期回合
constexpr int kShiTiBase        = 4695010;   // +side：失体Ⅰ到期回合

constexpr int kFearId = static_cast<int>(AbnormalStatusId::Fear);   // 6（1138 未触发支）

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

// 德布比特载体（属性技为主），技能全 must_hit=1/accuracy=100 防 miss 摇随机序。
ElfPet debbit_pet(const std::vector<int>& skill_ids, int speed = 300) {
    std::array<int, 5> ids{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    for (std::size_t i = 0; i < skill_ids.size() && i < 5; ++i) {
        ids[i] = skill_ids[i];
    }
    ElfPet p = make_pet(1, "德布比特", ids, speed, 300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed = 100) {
    std::array<int, 5> ids{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    ElfPet p = make_pet(2, "对手", ids, speed, 300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void cast(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// 白盒回满双方 HP：诸行无常链上已注册的 971 吸取窗（每回合吸对手最大体力 1/3，
// 窗 4 回合）在多回合局里会把 100k 对手吸死 → 死亡流程打断后续回合的标记断言。
// 本单被测宏均不读 HP（除 1971 击败门），回满只隔离链上邻位宏的副作用。
void top_up(ScenarioBattle& b) {
    b.ctx().getPet(0).hp = kMaxHp;
    b.ctx().getPet(1).hp = kMaxHp;
}

// 对手出技能 idx（后手延续局：对手先手出招、德布比特后手出载体技）。
void round_both(ScenarioBattle& b, int foe_index, int debbit_index) {
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_index);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, debbit_index);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7950a 1970/1968/1967/1969 标记生命周期全链（先手局）═══════════════
    {
        sbs_srand(79501);
        ScenarioBattle b(make_party(debbit_pet({kZhuFaWuWo, kZhuXingWuChang})),
                         make_party(foe_pet()));
        b.start();
        // R1 诸法无我：1969 无失体不触发；1970 附加回体Ⅰ [3,3] → 到期 rc0+3-1=2
        cast(b, 0);
        CHECK_EQ(read_key(b, k1969Audit), -1, "T7950a R1 对手无失体标记 → 1969 不触发");
        CHECK_EQ(read_key(b, k1970ModeAudit), 1, "T7950a R1 回体Ⅰ附加档");
        CHECK_EQ(read_key(b, k1970ExpiryAudit), 2, "T7950a R1 回体Ⅰ到期=rc0+3-1=2");
        CHECK_EQ(read_key(b, kHuiTiBase + 0), 2, "T7950a R1 回体Ⅰ槽=2");
        // R2 诸行无常：1967 持回体 → BREAK 窗 2 回合（先手无延续）；1968 附加失体Ⅰ
        cast(b, 1);
        CHECK_EQ(read_key(b, k1967Audit), 2, "T7950a R2 免断窗=2（先手无延续）");
        CHECK_EQ(read_key(b, k1968ModeAudit), 1, "T7950a R2 失体Ⅰ附加档");
        CHECK_EQ(read_key(b, k1968ExpiryAudit), 3, "T7950a R2 失体Ⅰ到期=rc1+3-1=3");
        CHECK_EQ(read_key(b, kShiTiBase + 1), 3, "T7950a R2 失体Ⅰ槽=3");
        const RemoveRoundEffectsResult br = remove_round_effects(&b.ctx(), 0);
        CHECK_EQ(static_cast<int>(br), static_cast<int>(RemoveRoundEffectsResult::IMMUNE),
                 "T7950a R2 免断窗内消自身回合类效果 = IMMUNE（821 同式直调）");
        top_up(b);
        // R3 诸法无我：1969 持失体（3>=rc2 边界）→ 保护窗 2；1970 持回体（2>=2 边界）
        // → 刷新档到期 rc2+3-1=4
        cast(b, 0);
        CHECK_EQ(read_key(b, k1969Audit), 2, "T7950a R3 强化保护窗=2（先手无延续）");
        CHECK_EQ(read_key(b, k1970ModeAudit), 2, "T7950a R3 回体Ⅰ刷新档");
        CHECK_EQ(read_key(b, k1970ExpiryAudit), 4, "T7950a R3 回体Ⅰ刷新到期=rc2+3-1=4");
        CHECK_EQ(read_key(b, kHuiTiBase + 0), 4, "T7950a R3 回体Ⅰ槽=4");
        // 1969 双票直调：消除/吸取均被挡、槽位不变（场景链 sim_core，原语直调）
        const int lvl_before = b.ctx().ability_levels[0][2];
        const int cleared = clear_stat_boosts(&b.ctx(), 0);
        CHECK_EQ(cleared, 0, "T7950a R3 STAT_CLEAR 票挡下消除（返回 0）");
        CHECK_EQ(b.ctx().ability_levels[0][2], lvl_before, "T7950a R3 消除被挡槽位不变");
        const int moved = transfer_stat_boosts(&b.ctx(), 0, 1);
        CHECK_EQ(moved, 0, "T7950a R3 STAT_BOOST 票挡下吸取/搬走（返回 0）");
        CHECK_EQ(b.ctx().ability_levels[0][2], lvl_before, "T7950a R3 吸取被挡槽位不变");
        // R4 诸行无常：1968 刷新档（失体 3>=rc3 边界）→ 到期 rc3+3-1=5；1967 再授
        cast(b, 1);
        CHECK_EQ(read_key(b, k1968ModeAudit), 2, "T7950a R4 失体Ⅰ刷新档");
        CHECK_EQ(read_key(b, k1968ExpiryAudit), 5, "T7950a R4 失体Ⅰ刷新到期=rc3+3-1=5");
        CHECK_EQ(read_key(b, k1967Audit), 2, "T7950a R4 免断窗再授=2");
        top_up(b);
        // R5 拍打过渡（无标记写入）
        cast(b, 2);
        top_up(b);
        // R6 诸法无我：回体Ⅰ已过期（4<rc5，残留不算持有）→ 附加档；失体Ⅰ仍持
        // （5>=5 边界）→ 1969 照触发
        cast(b, 0);
        CHECK_EQ(read_key(b, k1970ModeAudit), 1, "T7950a R6 过期残留不算持有 → 附加档");
        CHECK_EQ(read_key(b, k1970ExpiryAudit), 7, "T7950a R6 回体Ⅰ附加到期=rc5+3-1=7");
        CHECK_EQ(read_key(b, k1969Audit), 2, "T7950a R6 失体Ⅰ边界内仍持 → 保护窗照授");
        std::cout << "  [T7950a] 标记生命周期+1967/1969 窗 通过\n";
    }

    // ══ T7950b 1967 反例：无回体标记 → 免断窗不授、回合类效果照清 ═════════
    {
        sbs_srand(79502);
        ScenarioBattle b(make_party(debbit_pet({kZhuXingWuChang})), make_party(foe_pet()));
        b.start();
        cast(b, 0);                    // R1 诸行无常（链首 1967 无标记早退；191 照挂）
        CHECK_EQ(read_key(b, k1967Audit), -1, "T7950b R1 无回体标记 → 免断窗不授");
        CHECK_EQ(read_key(b, k1968ModeAudit), 1, "T7950b R1 失体Ⅰ附加照常（同链独立宏）");
        const RemoveRoundEffectsResult br = remove_round_effects(&b.ctx(), 0);
        CHECK_EQ(static_cast<int>(br), static_cast<int>(RemoveRoundEffectsResult::SUCCESS),
                 "T7950b R1 无免断窗 → 消自身回合类效果（191）= SUCCESS");
        std::cout << "  [T7950b] 1967 无标记反例 通过\n";
    }

    // ══ T7950c 1967/1969 后出手延续（慢速载体局 + 白盒预置标记）═════════════
    // ⚠️ 不能靠"先诸法无我造标记、后诸行无常吃后手"走自然链：诸法无我链上 843
    // （下2回合先制+2，已注册）会让其后两回合的德布比特恒先手，速度无法翻越先制
    // 等级——后手形态只能白盒预置标记槽（K416 处决印记槽同款）+ 载体单出。
    {
        sbs_srand(79503);
        ScenarioBattle b(make_party(debbit_pet({kZhuXingWuChang, kZhuFaWuWo}, /*speed=*/50)),
                         make_party(foe_pet(/*speed=*/300)));
        b.start();
        // R1：对手拍打先手，德布比特后手诸行无常；白盒预置回体Ⅰ（到期 rc2 ≥ rc0）
        b.ctx().plugin_storage[kHuiTiBase + 0] = 2;
        round_both(b, 0, 0);
        CHECK_EQ(read_key(b, k1967Audit), 3, "T7950c R1 后出手延续 → 免断窗=2+1=3");
        // R2：后手诸法无我；R1 的 1968 已自然附加失体Ⅰ（到期 rc2 ≥ rc1 持有）
        round_both(b, 0, 1);
        CHECK_EQ(read_key(b, k1969Audit), 3, "T7950c R2 后出手延续 → 保护窗=2+1=3");
        std::cout << "  [T7950c] 后出手延续 通过\n";
    }

    // ══ T7950d 1966 正例：双标记持 + 消强成功 → 双增伤下回合窗 ═════════════
    {
        sbs_srand(79504);
        ScenarioBattle b(make_party(debbit_pet({kZhuFaWuWo, kZhuXingWuChang, kGuiYi})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().ability_levels[1][0] = 3;   // 对手攻槽 +3（白盒铺消强靶子）
        cast(b, 0);                    // R1 诸法无我 → 附加回体Ⅰ
        cast(b, 1);                    // R2 诸行无常 → 附加失体Ⅰ
        cast(b, 2);                    // R3 归一：1138 消对手回合类（无→未触发 100% 害怕）
                                       //       1966 消强成功 + 双标记 → 双窗授予
        CHECK(b.ctx().has_active_abnormal_status(1, kFearId),
              "T7950d R3 对手害怕在身（1138 未触发支，对手无回合类效果）");
        CHECK_EQ(read_key(b, k1966ClearAudit), 1, "T7950d R3 消强清除 1 项（攻槽 +3）");
        CHECK_EQ(b.ctx().ability_levels[1][0], 0, "T7950d R3 对手攻槽清零");
        CHECK_EQ(read_key(b, k1966AtkAudit), 100, "T7950d R3 持回体 → 攻击增伤窗授予 100%");
        CHECK_EQ(read_key(b, k1966PinkAudit), 50, "T7950d R3 持失体 → 粉伤增伤窗授予 50%");
        top_up(b);                 // 隔离 971 吸取窗（R4 的 ROUND_START 会再吸一轮）
        // R4：攻击增伤走 ROUND_START tick 写 ws（结构断言）；粉伤窗内直调实放
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 3);   // 拍打
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);   // 停泊在 R4 内（rc==3）
        b.ctx().debug_step_mode = false;
        CHECK_EQ(b.ctx().roundCount, 3, "T7950d R4 停泊 rc=3（窗 [3,4) 内）");
        const FixedDamageResult pink = deal_pink_damage(&b.ctx(), 1, 1000,
                                                        DamageKind::FIXED, 0);
        (void)pink;
        CHECK_EQ(read_key(b, k1966PinkApplied), 500,
                 "T7950d R4 粉伤 1000×50% 实放 500（AMP 窗内）");
        b.drive();
        CHECK_EQ(b.ctx().ws.damage_add_pct[0], 100.0f,
                 "T7950d R4 ws 攻击增伤=+100%（ROUND_START tick 落盘）");
        std::cout << "  [T7950d] 1966 双窗正例 通过\n";
    }

    // ══ T7950e 1966 门关反例×2：无标记（消强成功也无窗）/ 有标记无提升（门槛关）══
    {
        sbs_srand(79505);
        ScenarioBattle b(make_party(debbit_pet({kGuiYi})), make_party(foe_pet()));
        b.start();
        b.ctx().ability_levels[1][0] = 3;
        cast(b, 0);                    // R1 归一：无双标记 → 消强成功也不授窗
        CHECK_EQ(read_key(b, k1966ClearAudit), 1, "T7950e1 消强照常成功（1 项）");
        CHECK_EQ(read_key(b, k1966AtkAudit), -1, "T7950e1 无回体标记 → 攻击窗不授");
        CHECK_EQ(read_key(b, k1966PinkAudit), -1, "T7950e1 无失体标记 → 粉伤窗不授");
        cast(b, 0);                    // R2 再归一：仍无标记；ws 增伤无写入
        CHECK_EQ(b.ctx().ws.damage_add_pct[0], 0.0f, "T7950e1 R2 ws 攻击增伤=0（无窗）");
        std::cout << "  [T7950e1] 1966 标记门反例 通过\n";
    }
    {
        sbs_srand(79506);
        ScenarioBattle b(make_party(debbit_pet({kZhuFaWuWo, kZhuXingWuChang, kGuiYi})),
                         make_party(foe_pet()));
        b.start();
        cast(b, 0);                    // R1 附加回体Ⅰ
        cast(b, 1);                    // R2 附加失体Ⅰ
        cast(b, 2);                    // R3 归一：对手无提升 → 消强门槛关（0）→ 无窗
        CHECK_EQ(read_key(b, k1966ClearAudit), 0, "T7950e2 对手无提升 → 消除数=0 门槛关");
        CHECK_EQ(read_key(b, k1966AtkAudit), -1, "T7950e2 消强失败 → 攻击窗不授");
        CHECK_EQ(read_key(b, k1966PinkAudit), -1, "T7950e2 消强失败 → 粉伤窗不授");
        std::cout << "  [T7950e2] 1966 消强门反例 通过\n";
    }

    // ══ T7950f 1971 未击败 → 解放双标记 + 锁切 + 先制失效（先手局 FIRST 桶）═══
    {
        sbs_srand(79507);
        ScenarioBattle b(make_party(debbit_pet({kZhuFaWuWo, kZhuXingWuChang, kYuanQi})),
                         make_party(foe_pet()));
        b.start();
        cast(b, 0);                    // R1 附加回体Ⅰ
        cast(b, 1);                    // R2 附加失体Ⅰ
        cast(b, 2);                    // R3 缘起·苍生聚：未击败 → AFTER_ACTION 解放双标记
        top_up(b);                     // 隔离 971 吸取窗（R4 的 ROUND_START 还会吸一轮）
        CHECK(b.pet(1).hp > 0, "T7950f R3 对手存活（未击败前提）");
        CHECK_EQ(read_key(b, k1971ReleaseAudit), 3, "T7950f R3 解放位掩码=3（双标记齐解）");
        CHECK_EQ(read_key(b, k1971LockAudit), 1, "T7950f R3 解放成功 → 锁切+先制失效授予");
        CHECK_EQ(read_key(b, kHuiTiBase + 0), -1, "T7950f 回体Ⅰ已解放（槽擦除）");
        CHECK_EQ(read_key(b, kShiTiBase + 1), -1, "T7950f 失体Ⅰ已解放（槽擦除）");
        CHECK(b.ctx().rule_center_.is_switch_locked(1, b.ctx().roundCount),
              "T7950f R4 输入期对手在锁切窗内（NextRounds 起点 [3,4)）");
        // 锁切回合对手主动切换被拒：CHOOSE_PET 被锁拒绝后 FSM 仍停在输入态等合法
        // 操作 → 被拒后必须补交一个 NONE 让回合推进（否则 R4 整回合不跑）。
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(b.ctx().on_stage[1], 0, "T7950f R4 锁切回合对手切换被拒（未换人）");
        CHECK_EQ(read_key(b, k1971NullifyAudit), 3,
                 "T7950f R4 先制失效 tick 在 FMR 执行（回合戳=3）");
        CHECK_EQ(b.ctx().ws.priority_nullified[1], true,
                 "T7950f R4 先制失效区置位（FMR tick 落盘）");
        // R5 窗过期：切换放行、先制失效复位
        CHECK(!b.ctx().rule_center_.is_switch_locked(1, b.ctx().roundCount),
              "T7950f R5 锁切窗已过期（单回合窗 [3,4)）");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 3);
        b.drive();
        CHECK_EQ(b.ctx().on_stage[1], 1, "T7950f R5 窗过期后对手切换放行");
        CHECK_EQ(b.ctx().ws.priority_nullified[1], false, "T7950f R5 先制失效复位（ws 重置）");
        std::cout << "  [T7950f] 1971 解放+锁切+先制失效 通过\n";
    }

    // ══ T7950g 1971 击败支：真杀对手 → 不解放（审计保持未写）══════════════
    {
        sbs_srand(79508);
        ScenarioBattle b(make_party(debbit_pet({kYuanQi})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(1).hp = 1;      // 压对手到 1 点（真实技能结算击杀）
        cast(b, 0);                    // R1 缘起·苍生聚 → 击败 → 门关
        CHECK(b.pet(1).hp <= 0, "T7950g R1 对手被击败（真实攻击结算）");
        CHECK_EQ(read_key(b, k1971ReleaseAudit), -1, "T7950g 击败支不解放（审计未写）");
        CHECK_EQ(read_key(b, k1971LockAudit), -1, "T7950g 击败支无锁切");
        std::cout << "  [T7950g] 1971 击败支门关 通过\n";
    }

    // ══ T7950h 1971 局部解放（仅回体）+ SECOND 桶（慢速载体局）══════════════
    {
        sbs_srand(79509);
        ScenarioBattle b(make_party(debbit_pet({kZhuFaWuWo, kYuanQi}, /*speed=*/50)),
                         make_party(foe_pet(/*speed=*/300)));
        b.start();
        round_both(b, 0, 0);           // R1：后手诸法无我 → 附加回体Ⅰ（失体Ⅰ无）
        round_both(b, 0, 1);           // R2：后手缘起·苍生聚 → SECOND 桶延迟判定
        CHECK(b.pet(1).hp > 0, "T7950h R2 对手存活（未击败前提）");
        CHECK_EQ(read_key(b, k1971ReleaseAudit), 1, "T7950h 仅解放回体Ⅰ（掩码=1）");
        CHECK_EQ(read_key(b, k1971LockAudit), 1, "T7950h 解放成功任意一项 → 锁切授予");
        CHECK(b.ctx().rule_center_.is_switch_locked(1, b.ctx().roundCount),
              "T7950h R3 输入期对手在锁切窗内");
        std::cout << "  [T7950h] 1971 局部解放+SECOND 桶 通过\n";
    }

    // ══ T7950i 1971 双无标记 → 解放失败 → 无锁切 ═══════════════════════════
    {
        sbs_srand(79510);
        ScenarioBattle b(make_party(debbit_pet({kYuanQi})), make_party(foe_pet()));
        b.start();
        cast(b, 0);                    // R1 缘起·苍生聚：无标记 → mask=0
        CHECK(b.pet(1).hp > 0, "T7950i R1 对手存活");
        CHECK_EQ(read_key(b, k1971ReleaseAudit), 0, "T7950i 解放位掩码=0（双无）");
        CHECK_EQ(read_key(b, k1971LockAudit), -1, "T7950i 解放全败 → 无锁切");
        CHECK(!b.ctx().rule_center_.is_switch_locked(1, b.ctx().roundCount),
              "T7950i R2 输入期对手无锁切");
        std::cout << "  [T7950i] 1971 解放失败支 通过\n";
    }

    std::cout << "scenario_7950 全部通过" << std::endl;
    return 0;
}
