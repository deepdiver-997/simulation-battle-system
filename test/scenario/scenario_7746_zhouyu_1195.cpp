#include <utils/sbs_rand.h>
// 场景 7746 —— K414 effect 1195 周瑜（首登必焚烬 pending / 业火涤尘周期 /
// 体力对照增减伤；soul_lib/k414_dodge_chain_purge.cpp）（2026-11-04，worktree
// ../agents/k414；号段 7746-7749 四块）
//
// 断言口径（本体 row907 args[1,100,5,50,50,1] / 伙伴 row938 args[1,100,5,80,60,1]，
// 行参驱动）：
//   ① 首登焚烬 pending：登场后第 1 次用技（任意系别，M7）→ 审计槽申报 100 + 对手
//     落焚烬（100% 掷点短路，K390 直查形）；pending 申报制即耗（第二刀不重写审计槽
//     ——哨兵值钉死）；属性技首发也耗（M7 无攻击过滤直证）。
//   ② 业火涤尘：entry 锚（rc-entry)%5==0 且 rc>entry（M8）→ 首触发在 R6（rc=5）；
//     双向四清：对手提升被清 + 焚烬落地；无物可清则不焚烬；周期外计数不动。
//     boss 局照常（M3：尾注只在③支）。
//   ③ 高体增伤 ×1.5 / 低体减伤 ÷2（同 seed 对照局整数精确，K397 1139 直系）；
//     伙伴行 80/60 档位差；boss 局双臂全停（节点 boss_invalid）。
//   ⚠️ 测量局必须先白盒关 pending（焚烬 tick 污染掉血测量 + 多回合局对手 tick 死
//     风险）——plugin_storage 哨兵写法，K7500 审计槽同源。
//   ⚠️ 红伤浮动锚：atk300/def100 raw ∈ [85,104]（速查 §五 勘误口径）。
//   ⚠️ 涤尘为 100% 申报 → 全部精确断言，无统计带需求（概率面只在 821）。
#include <array>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;    // 拍打：攻击载体
constexpr int kGaoSu = 20017;   // 高速移动：属性技（首发耗 pending 观测）
constexpr int kMaxHp = 100000;
constexpr int kAtkSlot = 0;
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);  // 22

// 与 k414_dodge_chain_purge.cpp 一致的存储键。
constexpr int kPendingBurn1195 = 991195000 + 30;
constexpr int kAuditFirst1195 = 991195000 + 40;
constexpr int kAuditPurge1195 = 991195000 + 50;
constexpr int kPurgeCnt1195 = 991195000 + 60;
constexpr int kEntryRound1195 = 991195000 + 20;

int audit_of(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

ElfPet mark_pet(int mark_id, int monster_id, std::vector<int> args,
                bool first_gaoshu = false) {
    const std::array<int, 5> skills =
        first_gaoshu ? std::array<int, 5>{kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa}
                     : std::array<int, 5>{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    ElfPet p = make_pet(1, "周瑜", skills,
                        /*speed=*/300, /*atk=*/300, kMaxHp,
                        SoulMark(mark_id, "K414 魂印", "test", EffectArgs(std::move(args))));
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(1, "对照", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void pass_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int holder_attack(ScenarioBattle& b) {
    const int hp0 = b.ctx().getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp0 - b.ctx().getPet(1).hp;
}

int foe_attack(ScenarioBattle& b) {
    const int hp0 = b.ctx().getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp0 - b.ctx().getPet(0).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7746.a ①首登焚烬 pending：首刀 100% 落焚烬 + 申报制即耗 ══
    {
        sbs_srand(7746);
        ScenarioBattle b(make_party(mark_pet(1195, 3617, {1, 100, 5, 50, 50, 1})),
                         make_party(foe_pet()));
        b.start();
        holder_attack(b);   // R1：登场后第 1 次用技 → pending 消耗
        CHECK(b.ctx().has_active_abnormal_status(1, kIncinerate),
              "T7746.a 首刀 → 100% 焚烬对手（申报短路，K390 直查形）");
        CHECK_EQ(audit_of(b.ctx(), kAuditFirst1195), 100,
                 "T7746.a 首登焚烬审计槽=100（掷前申报）");
        // 申报制即耗：哨兵改槽后第二刀不得重写（重武装/重申报都会覆盖哨兵）。
        b.ctx().plugin_storage[kAuditFirst1195 + 0] = -7;
        holder_attack(b);
        CHECK_EQ(audit_of(b.ctx(), kAuditFirst1195), -7,
                 "T7746.a 第二刀：pending 已耗 → 审计槽不重写（哨兵 -7 保留）");
        std::cout << "  [T7746.a] 首登焚烬 pending 通过\n";
    }

    // ══ T7746.b "下1次技能"含属性技（M7 无攻击过滤直证）══
    {
        sbs_srand(7747);
        ScenarioBattle b(make_party(mark_pet(1195, 3617, {1, 100, 5, 50, 50, 1},
                                          /*first_gaoshu=*/true)),
                         make_party(foe_pet()));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 高速移动
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(b.ctx().has_active_abnormal_status(1, kIncinerate),
              "T7746.b 属性技首发 → pending 照常消耗（行文无攻击限定）");
        CHECK_EQ(audit_of(b.ctx(), kAuditFirst1195), 100,
                 "T7746.b 审计槽=100（属性技路径申报）");
        std::cout << "  [T7746.b] 属性技首发耗 pending 通过\n";
    }

    // ══ T7747.a ②业火涤尘：R6（rc=5）首触发——清对手提升 + 焚烬；R7 不再触发 ══
    //（全程 pass 局：pending 不耗、③无攻击不涉——纯净②观测。）
    {
        sbs_srand(7748);
        ScenarioBattle b(make_party(mark_pet(1195, 3617, {1, 100, 5, 50, 50, 1})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& x = b.ctx();
        core_api().stat_change(&x, 1, kAtkSlot, 2);   // 对手攻击+2（待涤尘清）
        CHECK_EQ(audit_of(x, kEntryRound1195), 1,
                 "T7747.a 登场锚=1（+1 偏移编码，解码后=0 号回合）");
        for (int r = 0; r < 4; ++r) {   // R1-R4（rc=0..3）
            pass_round(b);
        }
        CHECK_EQ(audit_of(x, kPurgeCnt1195), -1,
                 "T7747.a R1-R4：涤尘计数不写（周期未到，缺席=-1）");
        pass_round(b);                  // R5（rc=4）：4%5≠0
        CHECK_EQ(audit_of(x, kPurgeCnt1195), -1, "T7747.a R5（rc=4）：仍未写");
        pass_round(b);                  // R6（rc=5）：5%5==0 → 触发
        CHECK_EQ(audit_of(x, kPurgeCnt1195), 1, "T7747.a R6（rc=5）：涤尘触发计数=1");
        CHECK_EQ(x.ability_levels[1][kAtkSlot], 0,
                 "T7747.a 涤尘清除对手能力提升（+2 → 0，实测 " +
                     std::to_string(x.ability_levels[1][kAtkSlot]) + "）");
        CHECK(x.has_active_abnormal_status(1, kIncinerate),
              "T7747.a 消除任一成功 → 对手焚烬（100% 申报）");
        CHECK_EQ(audit_of(x, kAuditPurge1195), 100, "T7747.a 涤尘焚烬审计槽=100");
        pass_round(b);                  // R7（rc=6）：6%5≠0
        CHECK_EQ(audit_of(x, kPurgeCnt1195), 1, "T7747.a R7：计数仍=1（周期外）");
        std::cout << "  [T7747.a] 业火涤尘周期+双向清 通过\n";
    }

    // ══ T7747.b 涤尘无物可清 → 不焚烬（"消除任意一项成功"门）══
    {
        sbs_srand(7749);
        ScenarioBattle b(make_party(mark_pet(1195, 3617, {1, 100, 5, 50, 50, 1})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& x = b.ctx();
        b.ctx().plugin_storage[kAuditPurge1195 + 0] = -7;   // 哨兵
        for (int r = 0; r < 6; ++r) {   // R1-R6：rc=5 触发一次（无物可清）
            pass_round(b);
        }
        CHECK_EQ(audit_of(x, kPurgeCnt1195), 1, "T7747.b 涤尘触发=1");
        CHECK_EQ(audit_of(x, kAuditPurge1195), -7,
                 "T7747.b 无物可清 → 焚烬申报不写（哨兵 -7 保留）");
        CHECK(!x.has_active_abnormal_status(1, kIncinerate),
              "T7747.b 消除全败 → 不焚烬");
        std::cout << "  [T7747.b] 涤尘空清门 通过\n";
    }

    // ══ T7748.a ③高体增伤 ×1.5（本体行 db[3]=50，同 seed 整数精确；白盒关 pending
    //    ——防焚烬 tick 污染测量）══
    {
        int d0 = -1, d1 = -1;
        {
            sbs_srand(7750);
            ScenarioBattle b(make_party(plain_pet()), make_party(foe_pet()));
            b.start();
            d0 = holder_attack(b);
        }
        {
            sbs_srand(7750);
            ScenarioBattle b(make_party(mark_pet(1195, 3617, {1, 100, 5, 50, 50, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().plugin_storage[kPendingBurn1195 + 0] = 0;   // 白盒关 pending
            b.ctx().getPet(0).hp = 90000;   // 高体档
            b.ctx().getPet(1).hp = 50000;
            d1 = holder_attack(b);
        }
        CHECK(d0 >= 85 && d0 <= 104,
              "T7748.a 对照局 raw 带 [85,104]（实测 " + std::to_string(d0) + "）");
        CHECK_EQ(d1, d0 * 150 / 100,
                 "T7748.a 1195 高体：攻击伤害提高 50%（db[3]，整数精确；对照 " +
                     std::to_string(d0) + " → 实测 " + std::to_string(d1) + "）");
        std::cout << "  [T7748.a] 高体增伤通过（d0=" << d0 << " d1=" << d1 << "）\n";
    }

    // ══ T7748.b ③低体减伤 ÷2（db[4]=50，截断精确；holder 全程 NONE → pending 不耗）══
    {
        int d0 = -1, d_low = -1;
        {
            sbs_srand(7751);
            ScenarioBattle b(make_party(mark_pet(1195, 3617, {1, 100, 5, 50, 50, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;   // 等号档对照
            b.ctx().getPet(1).hp = 50000;
            d0 = foe_attack(b);
        }
        {
            sbs_srand(7751);
            ScenarioBattle b(make_party(mark_pet(1195, 3617, {1, 100, 5, 50, 50, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 1000;    // 低体档
            b.ctx().getPet(1).hp = 50000;
            d_low = foe_attack(b);
        }
        CHECK_EQ(d_low, d0 / 2,
                 "T7748.b 1195 低体：受到的攻击伤害减少 50%（db[4]，截断精确；对照 " +
                     std::to_string(d0) + " → 实测 " + std::to_string(d_low) + "）");
        std::cout << "  [T7748.b] 低体减伤通过（d0=" << d0 << " d_low=" << d_low << "）\n";
    }

    // ══ T7748.c 伙伴行 args[1,100,5,80,60,1]：增伤 ×1.8 / 减伤 ×0.6（档位差）══
    {
        int d0 = -1, d_hi = -1, d_low = -1;
        {
            sbs_srand(7752);
            ScenarioBattle b(make_party(mark_pet(1195, 3617, {1, 100, 5, 80, 60, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().plugin_storage[kPendingBurn1195 + 0] = 0;   // 白盒关 pending
            b.ctx().getPet(0).hp = 50000;
            b.ctx().getPet(1).hp = 50000;
            d0 = foe_attack(b);   // 等号：无分支基线
        }
        {
            sbs_srand(7752);
            ScenarioBattle b(make_party(mark_pet(1195, 3617, {1, 100, 5, 80, 60, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().plugin_storage[kPendingBurn1195 + 0] = 0;
            b.ctx().getPet(0).hp = 90000;
            b.ctx().getPet(1).hp = 50000;
            d_hi = holder_attack(b);
        }
        {
            sbs_srand(7752);
            ScenarioBattle b(make_party(mark_pet(1195, 3617, {1, 100, 5, 80, 60, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().plugin_storage[kPendingBurn1195 + 0] = 0;
            b.ctx().getPet(0).hp = 1000;
            b.ctx().getPet(1).hp = 50000;
            d_low = foe_attack(b);
        }
        CHECK_EQ(d_hi, d0 * 180 / 100,
                 "T7748.c 伙伴行高体：增伤 80%（db[3] 档位差；对照 " +
                     std::to_string(d0) + " → 实测 " + std::to_string(d_hi) + "）");
        CHECK_EQ(d_low, d0 * 40 / 100,
                 "T7748.c 伙伴行低体：受到伤害减少 60% → ×40%（db[4] 档位差；对照 " +
                     std::to_string(d0) + " → 实测 " + std::to_string(d_low) + "）");
        std::cout << "  [T7748.c] 伙伴行档位差通过（d0=" << d0 << " d_hi=" << d_hi
                  << " d_low=" << d_low << "）\n";
    }

    // ══ T7749.a boss 门：③双臂全停（d==d0 / 全额带），①②照常（M3 拆分实证）——
    //    拆三局：伤害测量局关 pending；①②各独立局（防焚烬 tick 死亡扰动）══
    {
        // 局 1：③停——增伤测量（关 pending）。
        int d0 = -1, d_amp = -1, d_red = -1;
        {
            sbs_srand(7753);
            ScenarioBattle b(make_party(plain_pet()), make_party(foe_pet()));
            b.start();
            d0 = holder_attack(b);
        }
        {
            sbs_srand(7753);
            ScenarioBattle b(make_party(mark_pet(1195, 3617, {1, 100, 5, 50, 50, 1})),
                             make_party(foe_pet()));
            b.ctx().is_boss_challenge = true;   // start 前置位（注册口判据）
            b.start();
            b.ctx().plugin_storage[kPendingBurn1195 + 0] = 0;
            b.ctx().getPet(0).hp = 90000;
            b.ctx().getPet(1).hp = 50000;
            d_amp = holder_attack(b);
            CHECK_EQ(d_amp, d0, "T7749.a boss 局：③增伤停（实测 " +
                                    std::to_string(d_amp) + " vs 对照 " +
                                    std::to_string(d0) + "）");
            // ③减伤停：低体挨打全额（同局续 R2）
            b.ctx().getPet(0).hp = 1000;
            d_red = foe_attack(b);
            CHECK(d_red >= 85 && d_red <= 104,
                  "T7749.a boss 局：③减伤停（全额带 [85,104]，实测 " +
                      std::to_string(d_red) + "）");
        }
        // 局 2：①照常——boss 局首刀焚烬（无尾注）。
        {
            sbs_srand(7754);
            ScenarioBattle b(make_party(mark_pet(1195, 3617, {1, 100, 5, 50, 50, 1})),
                             make_party(foe_pet()));
            b.ctx().is_boss_challenge = true;
            b.start();
            holder_attack(b);
            CHECK(b.ctx().has_active_abnormal_status(1, kIncinerate),
                  "T7749.a boss 局：①首登焚烬照常");
            CHECK_EQ(audit_of(b.ctx(), kAuditFirst1195), 100,
                     "T7749.a boss 局：首登焚烬审计槽=100");
        }
        // 局 3：②照常——boss 局涤尘周期（R6 触发：清提升 + 焚烬）。
        // ⚠️ ③减伤停旁证放 R2（焚烬 R6 才落地——焚烬在 is_control_abnormal_status
        //    控制表内=禁行动（abnormal-types.h:252），落地后对手攻击直接跳过）。
        {
            sbs_srand(7755);
            ScenarioBattle b(make_party(mark_pet(1195, 3617, {1, 100, 5, 50, 50, 1})),
                             make_party(foe_pet()));
            b.ctx().is_boss_challenge = true;
            b.start();
            b.ctx().plugin_storage[kPendingBurn1195 + 0] = 0;   // 关 pending（纯净②③）
            b.ctx().getPet(0).hp = 1000;   // 低体档：boss 局 ③减伤停 → R2 挨打全额
            pass_round(b);                 // R1（rc=0：周期未到）
            d_red = foe_attack(b);         // R2：③停旁证（焚烬尚未落地）
            CHECK(d_red >= 85 && d_red <= 104,
                  "T7749.a boss 局：③减伤停（全额带 [85,104]，实测 " +
                      std::to_string(d_red) + "）");
            core_api().stat_change(&b.ctx(), 1, kAtkSlot, 2);   // 对手攻击+2（待涤尘清）
            for (int r = 0; r < 4; ++r) {  // R3-R6（rc=2..5）：R6 触发
                pass_round(b);
            }
            CHECK_EQ(audit_of(b.ctx(), kPurgeCnt1195), 1,
                     "T7749.a boss 局：涤尘照常触发");
            CHECK_EQ(b.ctx().ability_levels[1][kAtkSlot], 0,
                     "T7749.a boss 局：对手提升被涤尘清除");
            CHECK(b.ctx().has_active_abnormal_status(1, kIncinerate),
                  "T7749.a boss 局：涤尘焚烬照常");
        }
        std::cout << "  [T7749.a] boss 门（③停①②转）通过\n";
    }

    std::cout << "scenario_7746 (1195) all checks passed" << std::endl;
    return 0;
}
