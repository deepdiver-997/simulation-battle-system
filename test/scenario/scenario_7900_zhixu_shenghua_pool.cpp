#include <utils/sbs_rand.h>
// 场景 7900 —— K416 逐宠定制池首批：至序圣华 4321 专属效果族
// （519 状态回血窗 / 1720 护盾转真伤 / 1476 全属性弱化低体翻倍 / 1757 次免+免疫真伤 /
//   1758 攻击窗 PP 归零+圣剑 / 1759 属性窗被处决精灵 PP / 1760 全属性提升印记翻倍 /
//   1762 低体门异常+未触发先制兜底）
//   —— moves_lib/k416_zhixu_shenghua_pool.cpp（切片对参/四查/口径记档见实现头）
//
// 断言口径：
//   - 概率/触发全部走实现侧审计槽（10<id>000x 段）无随机精确断言；行为断言只做
//     精确量（护盾吸收后的真伤净额、heal 量、PP 差、能力等级槽位）。
//   - 519 首因子 = 光逝 27836（446 Fisher-Yates 三不重复槽）→ 恒 3 个状态槽。
//   - 1757 的异常源 = 剧毒粉 28858（[1605] args[100,1]：100% 中毒，**现代通道**，
//     must_hit=1 属性技）——古早通道无视 Modern 次免票（common.h 通道注），不可用。
//   - 1762 的 100% 档原语短路不耗 rand（K176）；落地支/免疫支由对手是否持异常免疫票
//     区分（9996 测试桩票），无需找 seed。
//   - 1758/1759 的窗口边界用第 5 回合出界对照（InRounds [R1, R1+4)，5 回合局有界步进，
//     绝不 while(!FINISHED)）。
//   - 1762 门关支 = parser 寄生修复的对照证据：实现前"{0}%令对手{1}"主句被裸解析为
//     无条件 100% 疲惫（S22 §3.4 PARTIAL·前文条件丢失），门关支断言无异常+无兜底。
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
constexpr int k519Arm   = 27838;      // 秩序重建 [519][5,100]（cat=4）
constexpr int k519Arm3  = 28638;      // 春台沐风 [519][3,100]（窗口出界对照）
constexpr int k3Up      = 27836;      // 光逝 [446]（随机3项+1 → 恒3状态）
constexpr int k1720Move = 35615;      // 原初圣印 [1720][40]（cat=1 pow=100）
constexpr int k1476Move = 35618;      // 剑决浮云 [683,1476][1,500,1,2]
constexpr int k1757Move = 27840;      // 天命已拟 [1757,1758,1759,1121]
constexpr int k1760Move = 28029;      // 光辉意志 [1760][1]（纯链）
constexpr int k1762Move = 35619;      // 圣御·征伐四方 [881,1279,1360,1762]
constexpr int kPoison   = 28858;      // 剧毒粉 [1605][100,1]（现代通道 100% 中毒，属性技）

// 实现文件账本键（10<id>000x 审计段 + 功能段，与实现文件 constexpr 一致）
constexpr int k519TickAudit     = 1005190001;
constexpr int k1720DmgAudit     = 1017200001;
constexpr int k1476DropAudit    = 1014760001;
constexpr int k1757RemainAudit  = 1017570001;
constexpr int k1757PayloadAudit = 1017570002;
constexpr int k1758SwordAudit   = 1017580001;
constexpr int k1758PpAudit      = 1017580002;
constexpr int k1759PpAudit      = 1017590001;
constexpr int k1760UpAudit      = 1017600001;
constexpr int k1762LandAudit    = 1017620001;
constexpr int k1762PrioAudit    = 1017620002;
constexpr int k1757RemainKey    = 17570;     // on_stage_storage：次免剩余
constexpr int k416SwordBase     = 4694000;   // +side：圣剑
constexpr int k416MarkBase      = 4694010;   // +side*6+slot：处决印记

constexpr int kPoisonId  = static_cast<int>(AbnormalStatusId::Poison);    // 1
constexpr int kFatigueId = static_cast<int>(AbnormalStatusId::Fatigue);   // 7

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

int remain_of(ScenarioBattle& b, int side) {
    const auto it = b.ctx().getPet(side).on_stage_storage.find(k1757RemainKey);
    return it == b.ctx().getPet(side).on_stage_storage.end() ? -1
           : (std::any_cast<int>(&it->second) ? *std::any_cast<int>(&it->second) : -1);
}

// 属性技载体（无魂印），技能全 must_hit=1/accuracy=100 防 miss 摇随机序。
ElfPet carrier_pet(const std::vector<int>& skill_ids, int speed = 300, int atk = 300,
                   int hp = kMaxHp) {
    std::array<int, 5> ids{10004, 10004, 10004, 10004, 10004};
    for (std::size_t i = 0; i < skill_ids.size() && i < 5; ++i) {
        ids[i] = skill_ids[i];
    }
    ElfPet p = make_pet(1, "K416载体", ids, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(const std::vector<int>& skill_ids = {10004, 10004, 10004, 10004, 10004},
               int speed = 100, int hp = kMaxHp, SoulMark mark = {}) {
    std::array<int, 5> ids{10004, 10004, 10004, 10004, 10004};
    for (std::size_t i = 0; i < skill_ids.size() && i < 5; ++i) {
        ids[i] = skill_ids[i];
    }
    ElfPet p = make_pet(2, "对手", ids, speed, 300, hp, std::move(mark));
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

// 对手出技能 idx、己方 NONE（测"对手使用X技能"触发窗）。
void foe_use(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.drive();
}

// 对手第一个 PP>0 的技能槽（1758 归零后仍要能出手——0 PP 技能不能选）。
int foe_alive_skill(ScenarioBattle& b) {
    for (int i = 0; i < static_cast<int>(b.pet(1).skills.size()); ++i) {
        if (b.pet(1).skills[i].pp > 0) {
            return i;
        }
    }
    return 0;
}

// 异常免疫票测试桩（harness 9997 免弱桩同款，换 ANOMALY + Modern 层）。
EffectResult anomaly_immune_soul_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/9, /*source_id=*/9996,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/9996);
    return EffectResult::kOk;
}

SoulMark make_anomaly_immune_mark() {
    SoulMark sm;
    sm.id = 9996;
    sm.name = "免异常";
    sm.description = "自身免疫异常状态（测试桩，9 回合窗）";
    sm.effect = &anomaly_immune_soul_effect;
    sm.args = EffectArgs(std::vector<int>{0, 1});
    return sm;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7900a 519：R1 无状态不回 → R2 光逝 3 态回 300（审计+行为精确）═════════
    {
        sbs_srand(79001);
        ScenarioBattle b(make_party(carrier_pet({k519Arm, k3Up})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = kMaxHp - 1000;   // 留回复空间
        cast(b, 0);                              // R1 秩序重建：窗 [R1..R5]，0 态不回
        CHECK_EQ(read_key(b, k519TickAudit), -1, "T7900a R1 零状态不回复（审计不写）");
        cast(b, 1);                              // R2 光逝：3 态 → 回 3×100=300
        CHECK_EQ(read_key(b, k519TickAudit), 300, "T7900a R2 审计=3 态×100=300");
        CHECK_EQ(b.pet(0).hp, kMaxHp - 1000 + 300, "T7900a R2 hp 精确 +300");
        std::cout << "  [T7900a] 519 三态回 300 通过\n";
    }
    // ══ T7900b 519：R2 光逝后状态恒 3，R3-R5 拍打各回 300；R6 出界不回 ═════════
    // （两次光逝的随机槽可重叠 → 状态数不恒定；多回合 tick 用拍打保持 3 态不变。）
    {
        sbs_srand(79002);
        ScenarioBattle b(make_party(carrier_pet({k519Arm, k3Up})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = kMaxHp - 5000;
        cast(b, 0);                    // R1
        cast(b, 1);                    // R2 光逝 → 3 态 → 300
        CHECK_EQ(b.pet(0).hp, kMaxHp - 5000 + 300, "T7900b R2 +300");
        for (int r = 0; r < 3; ++r) {
            cast(b, 2);                // R3/R4/R5 拍打（状态仍 3 → 各 +300，窗 [R1..R5]）
        }
        CHECK_EQ(b.pet(0).hp, kMaxHp - 5000 + 1200, "T7900b R5 累计 +1200（窗 [R1..R5]）");
        const int hp_before = b.pet(0).hp;
        cast(b, 2);                    // R6：窗已过期，拍打照常出手
        CHECK_EQ(b.pet(0).hp, hp_before, "T7900b R6 出界不回复（hp 不变）");
        std::cout << "  [T7900b] 519 窗界出界 通过\n";
    }
    // ══ T7900c 519：3 回合档（春台沐风 [3,100]）R4 出界 ══════════════════════
    {
        sbs_srand(79003);
        ScenarioBattle b(make_party(carrier_pet({k519Arm3, k3Up})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = kMaxHp - 1000;
        cast(b, 0);                    // R1 [3,100]
        cast(b, 1);                    // R2 光逝 → 300
        cast(b, 2);                    // R3 拍打 → 300（窗 [R1..R3]）
        CHECK_EQ(b.pet(0).hp, kMaxHp - 1000 + 600, "T7900c 3 回合档 R3 累计 +600");
        const int hp_before = b.pet(0).hp;
        cast(b, 2);                    // R4：出界（行参档位差 [3,100] vs [5,100]）
        CHECK_EQ(b.pet(0).hp, hp_before, "T7900c R4 出界不回复（3 回合档）");
        std::cout << "  [T7900c] 519 三回合档出界 通过\n";
    }

    // ══ T7901a 1720：护盾 5000 → 附加 40%=200 真伤（红伤被盾吃 → hp 净额恰 200）══
    {
        sbs_srand(79011);
        ScenarioBattle b(make_party(carrier_pet({k1720Move})), make_party(foe_pet()));
        b.start();
        b.pet(1).shield_bank_.add(/*priority=*/0, /*quantity=*/5000, /*source_id=*/777,
                                  /*duration=*/9);
        const int hp0 = b.pet(1).hp;
        cast(b, 0);                    // R1 原初圣印（红伤先被 5000 盾吸收）
        CHECK_EQ(read_key(b, k1720DmgAudit), 2000, "T7901a 审计=护盾 5000×40%=2000");
        CHECK_EQ(hp0 - b.pet(1).hp, 2000,
                 "T7901a hp 净额恰 2000（真伤直通 + 红伤全额进盾）");
        std::cout << "  [T7901a] 1720 护盾真伤 2000 通过\n";
    }
    // ══ T7901b 1720：无护盾门关（审计不写）════════════════════════════════════
    {
        sbs_srand(79012);
        ScenarioBattle b(make_party(carrier_pet({k1720Move})), make_party(foe_pet()));
        b.start();
        cast(b, 0);
        CHECK_EQ(read_key(b, k1720DmgAudit), -1, "T7901b 无盾门关（审计不写）");
        std::cout << "  [T7901b] 1720 无盾门关 通过\n";
    }

    // ══ T7902a 1476：满体 → 全属性 -1（六槽逐槽断言）═════════════════════════
    {
        sbs_srand(79021);
        ScenarioBattle b(make_party(carrier_pet({k1476Move})), make_party(foe_pet()));
        b.start();
        cast(b, 0);                    // R1 剑决浮云（683 随机固伤 + 1476）
        CHECK_EQ(read_key(b, k1476DropAudit), 1, "T7902a 满体档每槽 -1");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(b.ctx().ability_levels[1][i], -1, "T7902a 对手六槽全 -1");
        }
        std::cout << "  [T7902a] 1476 满体 -1 通过\n";
    }
    // ══ T7902b 1476：低体门（hp<1/2 严格档）→ 每槽 -2；恰等不翻 ═══════════════
    {
        sbs_srand(79022);
        ScenarioBattle b(make_party(carrier_pet({k1476Move})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 49999;  // 49999×2=99998 < 100000 → 门内
        cast(b, 0);
        CHECK_EQ(read_key(b, k1476DropAudit), 2, "T7902b 低体门翻倍每槽 -2");
        CHECK_EQ(b.ctx().ability_levels[1][0], -2, "T7902b 对手攻槽 -2");
        std::cout << "  [T7902b] 1476 低体 -2 通过\n";
    }
    {
        sbs_srand(79023);
        ScenarioBattle b(make_party(carrier_pet({k1476Move})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 50000;  // 恰 1/2：50000×2=100000 不小于 → 不翻
        cast(b, 0);
        CHECK_EQ(read_key(b, k1476DropAudit), 1, "T902c 恰等 1/2 不翻倍（严格档）");
        std::cout << "  [T7902b] 1476 恰等不翻 通过\n";
    }
    // ══ T7902c 1476：免弱对照（对手持 STAT_DROP 票 → 弱化被挡、宏照跑）═════════
    {
        sbs_srand(79024);
        ScenarioBattle b(make_party(carrier_pet({k1476Move})),
                         make_party(foe_pet({}, 100, kMaxHp, make_stat_drop_immunity_soul_mark())));
        b.start();
        cast(b, 0);
        CHECK_EQ(read_key(b, k1476DropAudit), 1, "T7902c 免弱下宏照跑（审计=1）");
        CHECK_EQ(b.ctx().ability_levels[1][0], 0, "T7902c 免弱挡下（攻槽不变）");
        std::cout << "  [T7902c] 1476 免弱对照 通过\n";
    }

    // ══ T7903a 1757：授票 counts=2（审计+记账）═══════════════════════════════
    int r2_opp_hp = 0;
    {
        sbs_srand(79031);
        ScenarioBattle b(make_party(carrier_pet({k1757Move, k3Up})),
                         make_party(foe_pet({kPoison, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        cast(b, 0);                    // R1 天命已拟：授票 + 挂 1758/1759 watcher
        CHECK_EQ(read_key(b, k1757RemainAudit), 2, "T7903a 授票 counts=2");
        CHECK_EQ(remain_of(b, 0), 2, "T7903a 剩余记账=2");
        r2_opp_hp = b.pet(1).hp;
        foe_use(b, 0);                 // R2 剧毒粉（现代 100% 中毒）→ 免疫 #1
        CHECK_EQ(remain_of(b, 0), 1, "T7903a R2 免疫后剩余 1");
        CHECK(!b.ctx().has_active_abnormal_status(0, kPoisonId), "T7903a R2 中毒被挡");
        CHECK_EQ(read_key(b, k1757PayloadAudit), 150, "T7903a R2 回合末附加 150 真伤");
        CHECK_EQ(r2_opp_hp - b.pet(1).hp, 150,
                 "T7903a 对手 hp 净额恰 -150（己方光逝无伤害）");
        // ── T7903b（同局续）第 2 次免疫 → 票尽；第 3 次中毒落地 ──
        r2_opp_hp = b.pet(1).hp;
        foe_use(b, 0);                 // R3 → 免疫 #2
        CHECK_EQ(remain_of(b, 0), 0, "T7903b R3 票尽（剩余 0）");
        CHECK_EQ(r2_opp_hp - b.pet(1).hp, 150, "T7903b R3 再附加 150");
        foe_use(b, 0);                 // R4 → 无票 → 中毒落地
        CHECK(b.ctx().has_active_abnormal_status(0, kPoisonId), "T7903b R4 中毒落地");
        CHECK_EQ(read_key(b, k1757PayloadAudit), 150, "T7903b R4 无新附加（审计仍 150）");
        std::cout << "  [T7903a/b] 1757 次免+真伤 通过\n";
    }

    // ══ T7904 1758：攻击窗 PP 归零 2 技 + 圣剑 +1/次；第 5 回合出界 ═══════════
    {
        sbs_srand(79041);
        ScenarioBattle b(make_party(carrier_pet({k1757Move})),
                         make_party(foe_pet()));
        b.start();
        cast(b, 0);                    // R1 天命已拟（1758 窗 [R1..R4]，圣剑段 1758[4,2,1]）
        CHECK_EQ(read_key(b, k1758SwordAudit), -1, "T7904 R1 无触发（审计不写）");
        foe_use(b, 0);                 // R2 对手拍打（攻击技）→ 归零 2 技 + 圣剑 1
        int zeroed = 0;
        for (const Skills& s : b.pet(1).skills) {
            if (s.pp == 0) {
                ++zeroed;
            }
        }
        CHECK_EQ(zeroed, 2, "T7904 R2 恰 2 技 PP 归零");
        CHECK_EQ(read_key(b, k1758PpAudit), 2, "T7904 R2 审计归零技数=2");
        CHECK_EQ(read_key(b, k1758SwordAudit), 1, "T7904 R2 圣剑=1");
        CHECK_EQ(read_key(b, k416SwordBase + 0), 1, "T7904 圣剑资源槽=1");
        foe_use(b, foe_alive_skill(b));   // R3 → 圣剑 2（R2 被归零槽不能再用）
        CHECK_EQ(read_key(b, k1758SwordAudit), 2, "T7904 R3 圣剑=2");
        foe_use(b, foe_alive_skill(b));   // R4 → 圣剑 3（窗内最后回合）
        CHECK_EQ(read_key(b, k1758SwordAudit), 3, "T7904 R4 圣剑=3");
        foe_use(b, foe_alive_skill(b));   // R5：窗 [R1..R4] 已出界
        CHECK_EQ(read_key(b, k1758SwordAudit), 3, "T7904 R5 出界圣剑不再累加");
        std::cout << "  [T7904] 1758 PP 归零+圣剑 通过\n";
    }

    // ══ T7905a 1759：被处决精灵（场上持印记）→ 属性技触发全技能 PP-1 ══════════
    {
        sbs_srand(79051);
        ScenarioBattle b(make_party(carrier_pet({k1757Move})),
                         make_party(foe_pet({kPoison, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        cast(b, 0);                    // R1 天命已拟（1759 窗 [4,1]）
        const int slot = b.ctx().on_stage[1];
        b.ctx().plugin_storage[k416MarkBase + 1 * 6 + slot] = 1;   // 白盒注入处决印记
        std::vector<int> pp_before;
        for (const Skills& s : b.pet(1).skills) {
            pp_before.push_back(s.pp);
        }
        foe_use(b, 0);                 // R2 剧毒粉（属性技）→ 被处决精灵全技 PP-1
        CHECK_EQ(read_key(b, k1759PpAudit), 1, "T7905a 审计 PP 削减=1");
        for (std::size_t i = 0; i < b.pet(1).skills.size(); ++i) {
            const int delta = pp_before[i] - b.pet(1).skills[i].pp;
            CHECK_EQ(delta, i == 0 ? 2 : 1,
                     "T7905a 槽 " + std::to_string(i) + " PP 差（用技 -1 + 削减 -1）");
        }
        std::cout << "  [T7905a] 1759 场上印记 PP-1 通过\n";
    }
    // ══ T7905b 1759：无印记 → 属性技不触发 PP 削减（生产端未实现的忠实空转）════
    {
        sbs_srand(79052);
        ScenarioBattle b(make_party(carrier_pet({k1757Move})),
                         make_party(foe_pet({kPoison, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        cast(b, 0);
        std::vector<int> pp_before;
        for (const Skills& s : b.pet(1).skills) {
            pp_before.push_back(s.pp);
        }
        foe_use(b, 0);
        CHECK_EQ(read_key(b, k1759PpAudit), -1, "T7905b 无印记不触发（审计不写）");
        for (std::size_t i = 0; i < b.pet(1).skills.size(); ++i) {
            CHECK_EQ(pp_before[i] - b.pet(1).skills[i].pp, i == 0 ? 1 : 0,
                     "T7905b 槽 " + std::to_string(i) + " 仅用技消耗");
        }
        std::cout << "  [T7905b] 1759 无印记空转 通过\n";
    }
    // ══ T7905c 1759：印记在背包（非上场槽）→ 不触发 ══════════════════════════
    {
        sbs_srand(79053);
        ScenarioBattle b(make_party(carrier_pet({k1757Move})),
                         make_party(foe_pet({kPoison, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        cast(b, 0);
        b.ctx().plugin_storage[k416MarkBase + 1 * 6 + 3] = 1;   // 背包槽 3
        std::vector<int> pp_before;
        for (const Skills& s : b.pet(1).skills) {
            pp_before.push_back(s.pp);
        }
        foe_use(b, 0);
        CHECK_EQ(read_key(b, k1759PpAudit), -1, "T7905c 背包印记不触发");
        CHECK_EQ(pp_before[2] - b.pet(1).skills[2].pp, 0, "T7905c 非用技槽 PP 不变");
        std::cout << "  [T7905c] 1759 背包印记不触发 通过\n";
    }

    // ══ T7906a 1760：无印记 → 全属性 +1（纯链 光辉意志）══════════════════════
    {
        sbs_srand(79061);
        ScenarioBattle b(make_party(carrier_pet({k1760Move})), make_party(foe_pet()));
        b.start();
        cast(b, 0);
        CHECK_EQ(read_key(b, k1760UpAudit), 1, "T7906a 无印记每槽 +1");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(b.ctx().ability_levels[0][i], 1, "T7906a 自身六槽全 +1");
        }
        std::cout << "  [T7906a] 1760 无印记 +1 通过\n";
    }
    // ══ T7906b 1760：对手场上持处决印记 → 翻倍 +2 ════════════════════════════
    {
        sbs_srand(79062);
        ScenarioBattle b(make_party(carrier_pet({k1760Move})), make_party(foe_pet()));
        b.start();
        const int slot = b.ctx().on_stage[1];
        b.ctx().plugin_storage[k416MarkBase + 1 * 6 + slot] = 2;   // 2 层印记
        cast(b, 0);
        CHECK_EQ(read_key(b, k1760UpAudit), 2, "T7906b 印记翻倍每槽 +2");
        CHECK_EQ(b.ctx().ability_levels[0][0], 2, "T7906b 自身攻槽 +2");
        std::cout << "  [T7906b] 1760 印记翻倍 通过\n";
    }

    // ══ T7907a 1762：门关（体力不低于对手）→ 无异常无兜底（parser 寄生修复对照）═
    {
        sbs_srand(79071);
        ScenarioBattle b(make_party(carrier_pet({k1762Move})), make_party(foe_pet()));
        b.start();
        cast(b, 0);                    // 双方满体力（相等）→ 门关
        CHECK_EQ(read_key(b, k1762LandAudit), -1, "T7907a 门关不掷骰（落地审计不写）");
        CHECK_EQ(read_key(b, k1762PrioAudit), -1, "T7907a 门关无兜底（先制审计不写）");
        CHECK(!b.ctx().has_active_abnormal_status(1, kFatigueId),
              "T7907a 门关对手无疲惫（parser 寄生已灭：旧通道无条件 100% 疲惫）");
        std::cout << "  [T7907a] 1762 门关对照 通过\n";
    }
    // ══ T7907b 1762：门开+对手免异常 → 未触发 → 下回合全技先制 +2（FMR 结构断言）═
    {
        sbs_srand(79072);
        ScenarioBattle b(make_party(carrier_pet({k1762Move})),
                         make_party(foe_pet({}, 100, kMaxHp, make_anomaly_immune_mark())));
        b.start();
        b.ctx().getPet(0).hp = 40000;  // 40000 < 100000 → 门开
        cast(b, 0);                    // R1：100% 申报被免疫票挡下 → 兜底
        CHECK_EQ(read_key(b, k1762LandAudit), 0, "T7907b 免疫支未落地（审计=0）");
        CHECK_EQ(read_key(b, k1762PrioAudit), 2, "T7907b 兜底先制量=2");
        CHECK(!b.ctx().has_active_abnormal_status(1, kFatigueId), "T7907b 对手无疲惫");
        // 停泊读法（7080 同式）：先开 debug 步进再提交，否则 run 会跑完回合到下一
        // CHOOSE（缓冲已空），step_past 在等输入态空转。
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);   // R2 FMR 桶已跑
        b.ctx().debug_step_mode = false;
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 2,
                 "T7907b R2 FMR 先制等级=+2（先手权结构断言）");
        b.drive();
        std::cout << "  [T7907b] 1762 未触发兜底 通过\n";
    }
    // ══ T7907c 1762：门开+无免疫 → 100% 疲惫落地、无兜底 ═════════════════════
    {
        sbs_srand(79073);
        ScenarioBattle b(make_party(carrier_pet({k1762Move})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 40000;
        cast(b, 0);
        CHECK_EQ(read_key(b, k1762LandAudit), 1, "T7907c 100% 档落地（审计=1）");
        CHECK(b.ctx().has_active_abnormal_status(1, kFatigueId), "T7907c 对手疲惫在身");
        CHECK_EQ(read_key(b, k1762PrioAudit), -1, "T7907c 落地无兜底");
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        b.ctx().debug_step_mode = false;
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 0, "T7907c R2 FMR 无先制加成");
        b.drive();
        std::cout << "  [T7907c] 1762 落地支 通过\n";
    }

    std::cout << "scenario_7900 全部通过" << std::endl;
    return 0;
}
