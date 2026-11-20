// 场景 7550 —— J315 effect 179（若属性相同则技能威力提升{0}，moves_lib/j_cond_power_rewrite.cpp）
//
// 断言口径：**SKILL_EFFECT 时点字段断言**（K9 2861 同款）——debug_step 停泊到
// FIRST_SKILL_EFFECT 跑完后读 ws.skill_power_view[0].value/.rewritten。不走伤害比例
// 对齐：rewritten 重算会重掷 85%~100% 浮点（K9 2862 首跑实证 / §8.7）。
//
//   A1 本系门开（雷纳尔多{6,0}+机械能量炮12981 type6 [20]）：view=150+20=170、rewritten
//   A2 门关对照（布布种子{1,0}+12981）：view=150、未改写
//   A3 判据读**属性视图**非本体（本体{6,0}、视图直改{3,0}）：不触发（785 防自污染口径）
//   A4 反向（本体{3,0}、视图直改{6,0}）：触发 —— 视图是唯一判据输入
//   A5 参数面（SEK{6,0}+无情横扫34166 [60] 物140）：view=140+60=200
//   A6 参数面二（艾斯普鲁{104,0}+电光骤闪35515 [100] 特130）：view=130+100=230
//   A7 威力0载体平加（痴愚13739 power0 [220] type8）：门关 view=0；门开 view=0+220=220
//      ——平直口径的依据面（无 view>0 守卫；百分比口径下 0×1.7 仍 0 = 死技）
//   B 伤害方向（同种子 atk300）：门开−门关 ∈ [150,340]
//      （(2.52×170+2)×1.5×[0.85,1] − (2.52×150+2)×[0.85,1] = 548.8..645.6 − 323..380；
//      机械 vs 草 restraint=1.0（types_relation steel|grass 实查）；make_pet 清零暴击）
//
// 元素构造：make_pet 恒 {0,0} → 门开用例在 start() 前**设本体元素**（首回合
// sync_workspace_from_on_stage 重基进视图）；视图直改用例在 start() 后同时改
// elf_element_view + ws.view（半持久层兜住任何重同步次序）。
// A7 附带核：13739 链上 700/1083/1257/1248/456 是解析器兄弟单元，均不写威力视图
// （PowerBoost 仅 179 一家）——字段断言不受链激活平移影响。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物40（填充）
constexpr int kJiXie   = 12981;  // 机械能量炮 [179]|[20]：特150 type6（纯链）
constexpr int kHengSao = 34166;  // 无情横扫 [179]|[60]：物140 type6（纯链）
constexpr int kDianDian= 35515;  // 电光骤闪 [179]|[100]：特130 type104（纯链）
constexpr int kChiYu   = 13739;  // 痴愚 [179,700,1083,1257,1248,456]|[220,…]：物0 type8

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = true;
    }
}

// 痴愚 13739/无相谛族 max_pp=0（官方 PP"--"）→ 测试层补 PP（009 用 ignore_pp 魂印同因）。
void pp_top_up(ElfPet& p) {
    p.skills[0].maxPP = 5;
    p.skills[0].pp = 5;
}

// 门开攻击方：本体元素 = 技能系别（start() 前设，首回合 sync 重基进视图）。
ElfPet same_elem_attacker(int skill_id, int elem, int atk) {
    ElfPet p = make_pet(1, "进攻方", {skill_id, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/100000);
    p.elementalAttributes[0] = elem;
    p.elementalAttributes[1] = 0;
    accuracy_100(p);
    return p;
}

// 门关对照方：本体元素 ≠ 技能系别（默认 {0,0} 亦异于一切真实系别，显式给值更醒目）。
ElfPet diff_elem_attacker(int skill_id, int elem, int atk) {
    ElfPet p = make_pet(1, "对照方", {skill_id, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/100000);
    p.elementalAttributes[0] = elem == 8 ? 1 : 8;   // 恒取与技能系别不同的真实系别
    p.elementalAttributes[1] = 0;
    accuracy_100(p);
    return p;
}

ElfPet dummy_pet() {
    return make_pet(2, "木桩", {10005, 10005, 10005, 10005, 10005},
                    /*speed=*/50, /*atk=*/10, /*hp=*/1000000);
}

// 停泊到 FIRST_SKILL_EFFECT 跑完，读进攻方威力视图。before_submit：视图直改钩子（可空）。
int probe_power_view(const ElfPet& attacker, void (*before_submit)(BattleContext*),
                     bool* rewritten_out) {
    ScenarioBattle b(make_party(attacker), make_party(dummy_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261102);
    b.start();
    if (before_submit) {
        before_submit(&ctx);
    }
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    ctx.debug_step_mode = false;
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    const int view = ctx.ws.skill_power_view[0].value;
    if (rewritten_out) {
        *rewritten_out = ctx.ws.skill_power_view[0].rewritten;
    }
    b.drive();
    return view;
}

void force_view(BattleContext* ctx, int slot0, int slot1) {
    // 半持久层 + 工作区同改：先钉住绑定槽位（防双方提交后 sync 的首跑从 pet 重基
    // 冲掉改动），视图取值在执行期被钉死。
    ctx->elf_element_view_bound_slot[0] = ctx->on_stage[0];
    ctx->elf_element_view[0][0] = slot0;
    ctx->elf_element_view[0][1] = slot1;
    ctx->ws.view_elementalAttributes[0][0] = slot0;
    ctx->ws.view_elementalAttributes[0][1] = slot1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A1 本系门开：{6,0} + 机械能量炮（type6，150，[20]）→ 150+20=170
    {
        bool rw = false;
        const int v = probe_power_view(same_elem_attacker(kJiXie, 6, 300), nullptr, &rw);
        CHECK_EQ(v, 170, "A1 本系门开：view=150+20（平直口径，见插件头注记档）");
        CHECK(rw, "A1 rewritten 置位（§8.7 变威力重算入口）");
    }
    // A2 门关对照：{1,0} + 12981 → 不触发
    {
        bool rw = true;
        const int v = probe_power_view(diff_elem_attacker(kJiXie, 6, 300), nullptr, &rw);
        CHECK_EQ(v, 150, "A2 异系门关：view=物化基准 150");
        CHECK(!rw, "A2 未改写");
    }
    // A3 判据读视图非本体：本体{6,0}、视图直改{3,0} → 不触发
    {
        bool rw = true;
        const int v = probe_power_view(same_elem_attacker(kJiXie, 6, 300),
                                       [](BattleContext* c) { force_view(c, 3, 0); }, &rw);
        CHECK_EQ(v, 150, "A3 本体同系但视图异系 → 门关（785 口径：判据读视图）");
        CHECK(!rw, "A3 未改写");
    }
    // A4 反向：本体{3,0}、视图直改{6,0} → 触发
    {
        bool rw = false;
        const int v = probe_power_view(diff_elem_attacker(kJiXie, 6, 300),
                                       [](BattleContext* c) { force_view(c, 6, 0); }, &rw);
        CHECK_EQ(v, 170, "A4 本体异系但视图同系 → 门开（视图是唯一判据输入）");
        CHECK(rw, "A4 rewritten 置位");
    }
    // A5 参数面二：{6,0} + 无情横扫（140，[60]）→ 200
    {
        const int v = probe_power_view(same_elem_attacker(kHengSao, 6, 300), nullptr, nullptr);
        CHECK_EQ(v, 200, "A5 物理载体：view=140+60");
    }
    // A6 参数面三：{104,0} + 电光骤闪（130，[100]）→ 230
    {
        const int v = probe_power_view(same_elem_attacker(kDianDian, 104, 300), nullptr, nullptr);
        CHECK_EQ(v, 230, "A6 特攻载体：view=130+100");
    }
    // A7 威力0载体平加（痴愚 power0 [220] type8，max_pp=0 测试层补 PP）
    {
        bool rw = true;
        ElfPet off = diff_elem_attacker(kChiYu, 8, 300);
        pp_top_up(off);
        const int v_off = probe_power_view(off, nullptr, &rw);
        CHECK_EQ(v_off, 0, "A7.a 威力0+异系：view 物化为 0、不触发");
        CHECK(!rw, "A7.a 未改写");
        ElfPet on = same_elem_attacker(kChiYu, 8, 300);
        pp_top_up(on);
        const int v_on = probe_power_view(on, nullptr, &rw);
        CHECK_EQ(v_on, 220, "A7.b 威力0+同系：view=0+220（平直口径依据面，无 >0 守卫）");
        CHECK(rw, "A7.b rewritten 置位");
    }
    // B 伤害方向（同种子）：门开（170+STAB×1.5）− 门关（150）∈ [150,340]
    {
        auto run = [&](const ElfPet& attacker) {
            ScenarioBattle b(make_party(attacker), make_party(dummy_pet()));
            BattleContext& ctx = b.ctx();
            sbs_srand(20261102);
            b.start();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            return 1000000 - ctx.getPet(1).hp;
        };
        const int d_same = run(same_elem_attacker(kJiXie, 6, 300));
        const int d_diff = run(diff_elem_attacker(kJiXie, 6, 300));
        CHECK(d_diff > 0, "B 前置：基准伤 > 0（实测 " + std::to_string(d_diff) + "）");
        const int diff = d_same - d_diff;
        CHECK(diff >= 150 && diff <= 340,
              "B 方向+带宽：门开−门关 = " + std::to_string(diff) +
                  "（同 " + std::to_string(d_same) + " / 异 " + std::to_string(d_diff) +
                  "；带=功率170+STAB1.5 与 150 的浮动包络）");
    }

    std::cout << "scenario_7550 PASS (J315 effect179 本系条件威力改写·字段断言)" << std::endl;
    return 0;
}
