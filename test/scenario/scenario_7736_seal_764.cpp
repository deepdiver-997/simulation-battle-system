#include <utils/sbs_rand.h>
// 场景 7736 —— K413 effect 764 斗天武神·瑞尔斯（3102，row449 本体 args[2,100] /
// row456 伙伴 args[2,100,50,50]，kind 同 [1,5,13]，args 计数互异 2/4）
// 「自身攻击技能命中后则令对手当回合及下回合使用的属性技能失效（该效果为回合类
//  效果）；|回合开始时若自身体力高于对手，则当回合造成的攻击伤害提升100%；回合
//  开始时若自身体力低于对手，则下1次使用技能造成的攻击伤害会恢复自身体力，之后
//  下1次使用技能给对手造成等同于恢复量的固定伤害（boss有效）」（伙伴行 +攻/体
//  各50——划界不实现）—— soul_lib/k413_immune_steal_seal.cpp
//
// 断言口径：
//   - ①封属窗 = own 攻击命中（红伤）→ 高速移动 20017 同回合即被封（R0 等级不变，
//     T7536 RS 窗同回合可断面）+ R1 仍封 + R2 恢复（duration=2 "当回合及下回合"，
//     tick 自灭；高速移动实测每次 +2，7543e 同锚）；封属计数=1（同句柄刷新非追加）；
//   - ②高体门 = ROUND_START 严格档（hp 严格 > 对手）→ ws.damage_add_pct 直读精确
//     （=100 / 门闭重写回 0 不叠加）+ 增伤带红伤 ×2 派生 [174,204]（raw [87,102]×2，
//     速查红伤锚）；等血对照 = 严格 > 不含等号；
//   - boss 门 = "（boss有效）" → 封属/增伤 boss 局照常（与 boss无效族反向对照，
//     K400 T7543h 镜像面）；
//   - 伙伴行 = 范围内行为同本体（db[0]/db[1] 恒同值），db[2]/db[3] 划界无行为差；
//   - 全部 submit_operation + drive() 有限回合，绝不 while(!FINISHED)。
#include <algorithm>
#include <any>
#include <cstdlib>
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

constexpr int kHit = 10004;      // 拍打（物理攻击，priority 0 载体）
constexpr int kGaoSu = 20017;    // 高速移动（属性技：生效则自身速度+2，封属观测）
constexpr int kMaxHp = 99999;

// 审计/侧证键（与 k413_immune_steal_seal.cpp 一致；owner=0）。
constexpr int kSealCnt = 997640000;   // 封属授予计数
constexpr int kGateAmp = 997640010;   // 高体门现行增幅（审计）

const std::vector<int> kRowBase{2, 100};          // row449 本体
const std::vector<int> kRowPartner{2, 100, 50, 50};  // row456 伙伴（面板划界）

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(764, "K413 斗天武神·瑞尔斯", "test", EffectArgs(args));
    sm.monster_id = 3102;
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int speed = 300) {
    ElfPet p = make_pet(1, "斗天武神·瑞尔斯", {kHit, kHit, kHit, kHit, kHit},
                        speed, /*atk=*/300, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed = 100) {
    ElfPet p = make_pet(2, "对手", {kHit, kGaoSu, kHit, kHit, kHit}, speed,
                        /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// own 攻击 + 对手同回合出属性技（own speed 300 先手 → 命中封属后对手才行动）。
void own_hit_foe_attr_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
}

// 对手单出属性技（封属窗观测）→ 返回对手速度等级。
int foe_attr_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
    return b.ctx().ability_levels[1][4];
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7736a ②高体门：严格档 ws 直读 + 增伤 ×2 带 + 门闭重写回 0 ══════════
    {
        sbs_srand(7737);
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        auto& ctx = b.ctx();
        idle_round(b);   // R0 RS：hp 相等 → 严格 > 不成立 → 门闭
        CHECK_EQ(read_key(b, kGateAmp), 0, "T7736a 等血门闭（严格 > 不含等号）");
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 0,
                 "T7736a 等血 ws 增幅=0");
        ctx.getPet(1).hp = 50000;   // 对手压血 → 自身高于对手
        idle_round(b);   // R1 RS：门开
        CHECK_EQ(read_key(b, kGateAmp), 100, "T7736a 高体门审计=100");
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 100,
                 "T7736a ws.damage_add_pct=100（当回合全攻击红伤）");
        const int hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int drop = hp0 - ctx.getPet(1).hp;
        CHECK(drop >= 174 && drop <= 204, "T7736a 增伤带 [174,204]（raw×2，实测 " +
                                              std::to_string(drop) + "）");
        ctx.getPet(0).hp = 1000;   // 自身压血 → 门闭
        idle_round(b);   // R2 RS：重写回 0（不叠加）
        CHECK_EQ(read_key(b, kGateAmp), 0, "T7736a 低体门闭");
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 0,
                 "T7736a ws 重写回 0（回合窗语义不跨回合叠加）");
        std::cout << "  [T7736a] 高体门 通过 (drop=" << drop << ")\n";
    }

    // ══ T7736b ①封属窗：命中即封（同回合+下回合）/ 第 3 回合恢复 ════════════
    {
        sbs_srand(7738);
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.start();   // R0
        own_hit_foe_attr_round(b);   // R0：own 命中 → 封属；对手同回合属性技被废
        CHECK_EQ(b.ctx().ability_levels[1][4], 0, "T7736b 当回合属性技失效（速度不变）");
        CHECK_EQ(read_key(b, kSealCnt), 1, "T7736b 封属授予计数=1");
        const int lv_r1 = foe_attr_round(b);   // R1：下回合仍封
        CHECK_EQ(lv_r1, 0, "T7736b 下回合属性技仍失效");
        const int lv_r2 = foe_attr_round(b);   // R2：窗过恢复（实测每次 +2）
        CHECK_EQ(lv_r2, 2, "T7736b 第 3 回合窗过恢复");
        std::cout << "  [T7736b] 封属 2 回合窗 通过 (R0/R1=" << lv_r1
                  << " R2=" << lv_r2 << ")\n";
    }

    // ══ T7736c boss 门："（boss有效）" → 封属+增伤 boss 局照常 ═══════════════
    {
        sbs_srand(7739);
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(1).hp = 50000;   // 高体门素材
        idle_round(b);   // R1 RS：门开（boss 局照常）
        CHECK_EQ(read_key(b, kGateAmp), 100, "T7736c boss：高体门照常开");
        own_hit_foe_attr_round(b);   // R1：命中 → 封属；对手属性技被废
        CHECK_EQ(b.ctx().ability_levels[1][4], 0, "T7736c boss：当回合封属照常");
        CHECK_EQ(read_key(b, kSealCnt), 1, "T7736c boss：封属计数=1");
        const int lv = foe_attr_round(b);   // R2：仍封
        CHECK_EQ(lv, 0, "T7736c boss：下回合仍封");
        std::cout << "  [T7736c] boss 有效 通过\n";
    }

    // ══ T7736d 伙伴行（args 4 位）：范围内行为同本体（db[2]/db[3] 划界）══════
    {
        sbs_srand(7740);
        ScenarioBattle b(make_party(mark_pet(kRowPartner)), make_party(foe_pet()));
        b.start();
        own_hit_foe_attr_round(b);   // R0：命中即封；对手同回合属性技被废
        CHECK_EQ(b.ctx().ability_levels[1][4], 0, "T7736d 伙伴：当回合封属（同本体）");
        CHECK_EQ(read_key(b, kSealCnt), 1, "T7736d 伙伴：封属计数=1");
        const int lv_r1 = foe_attr_round(b);   // R1：下回合仍封（db[0] 恒 2）
        CHECK_EQ(lv_r1, 0, "T7736d 伙伴：下回合仍封");
        b.ctx().getPet(1).hp = 50000;
        idle_round(b);   // R2 RS：高体门同本体（db[1] 恒 100）
        CHECK_EQ(read_key(b, kGateAmp), 100, "T7736d 伙伴：高体门同本体");
        const int lv_r2 = foe_attr_round(b);   // R3：窗过恢复
        CHECK_EQ(lv_r2, 2, "T7736d 伙伴：窗过恢复（+2）");
        std::cout << "  [T7736d] 伙伴行范围内同形 通过\n";
    }

    std::cout << "scenario_7736: all green" << std::endl;
    return 0;
}
