// 场景 8033 —— 神谕水王沧岚（2343）「对手附加的下场保留印记」× 非正常击杀交互
// —— resources/soul_lib/canglan_pink.cpp（怒涛·沧岚 2343 蚀砚之泪/永恒之水 + 被击败清零 watcher）
// —— resources/meiheweite_bao_runtime.h（曝压制面，8031/8032 同线）
//
// 用户口径（2026-11-07）：
//   「神谕后的水王沧岚，会给对手附加下场保留的印记（蚀砚之泪，4 泪队友共享——
//     nutao_runtime.h 口径），但是自身被击败会清除对手全队的这种印记；
//     如果被非正常击杀导致跳过（消逝）或者死亡效果不触发（曝·封亡语），
//     印记反而会保留并且继续生效。」
//
// 引擎对应面：
//   · 蚀砚之泪/永恒之水 = 沧岚方 team 计数（nutao_runtime 4690 段，下场保留）；
//     「清除对手全队的这种印记」= 死亡清零 watcher（nutao_clear_tears_on_death，
//     自有 EVENT_DEATH 监听）一次清 Tears/Water/WaterLock 三槽。
//   · 死亡清零 watcher 是**自有监听**（watcher.owner == 被击败方）→ 曝压制门
//     （event_center.h owner==target 门）天然拦下 → 印记保留；
//   · 消逝（vanish_spirit）不发 EVENT_DEATH → watcher 根本不见 → 印记保留。
//   · 「继续生效」= 保留的水增伤（+25%/道）在重新登场后的红伤管线 AMP 仍然到账
//     （BLOCK 相一次性探针断言 resolvedDamage.addPct）。
//
// 装配/白盒口径：
//   · 神谕沧岚 = make_pet(3740) + SoulMark(2343)（051 同款；神谕行选择只影响 DB
//     自动装配，手工 SoulMark 直接绑 2343）；泪水真实累计路径已有既有场景守护，
//     本文件白盒档位只做「首滴真实挂上 + 尾段档位预设」（8010 白盒纪律）。
//   · 登场 400 盾用 ShieldBank::consume 白盒清掉（清盾不是受击 → 不触发
//     「受攻伤→抵挡1次」票链，击杀轮无票可挡，死亡路径干净）。
//   · 曝 = attach_bao + set_defeat_effects_suppressed 白盒等价面（8032 同款：
//     真实永沐挂曝已在 8031 端到端覆盖）。
#include <any>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/shield_bank.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "../../resources/meiheweite_bao_runtime.h"
#include "../../resources/nutao_runtime.h"

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打（物攻填充，空链）
constexpr int kMaxHp = 100000;

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

// 神谕沧岚（魂印 2343；技能必中；暴击清零）。
ElfPet canglan_pet(int speed = 400) {
    SoulMark sm(2343, "怒涛·沧岚", "测试桩：神谕沧岚 2343（effect_icon 行 1932）");
    ElfPet p = make_pet(3740, "沧岚", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed, 300, kMaxHp, sm);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet dummy_pet(int speed = 100) {
    ElfPet p = make_pet(3, "队友", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed, 1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 重击手（击杀轮用；atk 3000 → 一刀穿 400 登场盾余量打空 1 血）。
ElfPet heavy_pet(int speed = 100) {
    ElfPet p = make_pet(4, "重击手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed, 3000, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 标准对局：我方=沧岚在场+替补；对方=重击手+替补。
ScenarioBattle std_battle() {
    return ScenarioBattle({canglan_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet()},
                          {heavy_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet()});
}

// 公共铺垫：白盒清 400 登场盾 → 沧岚真实攻击挂首滴泪 → 白盒补档 tears=4 / water=2。
void preset_marks(ScenarioBattle& b) {
    b.ctx().seerRobot[0].elfPets[0].shield_bank_.consume(999);
    CHECK_EQ(b.ctx().seerRobot[0].elfPets[0].shield_bank_.total(), 0, "铺垫：白盒清登场盾");
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 沧岚攻击（无盾 → +1 泪）
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    CHECK_EQ(read_key(b, kNutaoPubTears + 0), 1, "铺垫：无盾攻击 → 对手附加 1 滴蚀砚之泪（真实路径）");
    nutao_pub_set(b.ctx().plugin_storage, kNutaoPubTears + 0, 4);    // 白盒补档（泪 4 滴档）
    nutao_pub_set(b.ctx().plugin_storage, kNutaoPubWater + 0, 2);    // 白盒补档（本次在场水 2 道）
    nutao_pub_set(b.ctx().plugin_storage, kNutaoPubWaterLock + 0, 8); // 白盒补档（8 道锁定=下场保留）
    CHECK_EQ(read_key(b, kNutaoPubTears + 0), 4, "铺垫：泪档位=4");
    CHECK_EQ(read_key(b, kNutaoPubWater + 0), 2, "铺垫：在场水档位=2");
    CHECK_EQ(read_key(b, kNutaoPubWaterLock + 0), 8, "铺垫：水锁定=8（下场保留档）");
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8033a 对照：沧岚被正常击杀 → 自有死亡清零 watcher 触发 → 印记全清 ════
    {
        sbs_srand(80331);
        ScenarioBattle b(std_battle());
        b.start();
        preset_marks(b);
        b.ctx().seerRobot[0].elfPets[0].hp = 1;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 重击手收头
        b.drive();
        CHECK_EQ(b.ctx().pet_death_notified[0][0], true, "T8033a 前置：沧岚已被击败登记");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 死亡补位
        b.drive();
        CHECK_EQ(read_key(b, kNutaoPubTears + 0), 0,
                 "T8033a 被击败 → 蚀砚之泪清零（对手全队的印记被清除）");
        CHECK_EQ(read_key(b, kNutaoPubWater + 0), 0, "T8033a 永恒之水清零");
        CHECK_EQ(read_key(b, kNutaoPubWaterLock + 0), 0, "T8033a 水锁定槽清零");
        std::cout << "  [T8033a] 正常击杀 → 印记全清 通过\n";
    }
    // ══ T8033b 曝·封亡语：死亡效果不触发 → 印记保留，重新登场后继续生效 ══════
    {
        sbs_srand(80332);
        ScenarioBattle b(std_battle());
        b.start();
        preset_marks(b);
        meiheweite_bao::attach_bao(&b.ctx(), 0, b.ctx().on_stage[0]);   // 白盒挂曝
        b.ctx().set_defeat_effects_suppressed(0, true);
        CHECK_EQ(b.ctx().is_defeat_effects_suppressed(0), true, "T8033b 前置：压制位武装");
        b.ctx().seerRobot[0].elfPets[0].hp = 1;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().pet_death_notified[0][0], true, "T8033b 死亡照常登记（曝不拦死亡本身）");
        CHECK_EQ(read_key(b, kNutaoPubTears + 0), 4,
                 "T8033b 死亡效果不触发 → 蚀砚之泪保留");
        CHECK_EQ(read_key(b, kNutaoPubWater + 0), 2, "T8033b 本次在场水保留");
        CHECK_EQ(read_key(b, kNutaoPubWaterLock + 0), 8, "T8033b 8 道锁定槽（下场保留档）保留");
        // ── 继续生效：补位 → 复活沧岚 → 重新登场（魂印再激活）→ 锁定水增伤到账 ──
        // （重新登场会按官方口径清"本次在场"水（<8 道下场不保留），锁定槽不清 →
        //   有效水 = max(0, 8) = 8 → 增伤 +200%。）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 补位（离开 37 时点）
        b.drive();
        CHECK_EQ(revive_pet(&b.ctx(), 0, 0, 1000), 1000, "T8033b 前置：复活沧岚（背包位）");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);   // 主动切回（魂印再激活）
        b.drive();
        CHECK_EQ(b.ctx().on_stage[0], 0, "T8033b 前置：沧岚重新登场");
        b.ctx().seerRobot[0].elfPets[0].shield_bank_.consume(999);   // 再入场盾白盒清掉
        // 一次性 AMP 观测探针：本次红伤的加算增伤 = 25×锁定水道数（8 道 → +200%）。
        bool probed = false;
        b.ctx().register_damage_effect(
            DamagePhase::BLOCK, 0, DamageEffectCategory::BLOCK,
            [&](BattleContext* c, int /*owner*/) {
                auto& d = c->resolvedDamage;
                if (d.attackerId == 0 && d.isRed && !probed) {
                    probed = true;
                    CHECK_EQ(d.addPct, 200, "T8033b 保留的 8 道锁定水在管线继续生效（+200%）");
                }
            });
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 沧岚攻击
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(probed, true, "T8033b 探针已观测（红伤管线确有到账）");
        std::cout << "  [T8033b] 曝压制 → 印记保留并继续生效 通过\n";
    }
    // ══ T8033c 消逝（跳过时点）：不发 EVENT_DEATH → 清零 watcher 不见 → 印记保留 ═
    {
        sbs_srand(80333);
        ScenarioBattle b(std_battle());
        b.start();
        preset_marks(b);
        CHECK_EQ(vanish_spirit(&b.ctx(), 0, b.ctx().on_stage[0], 1) == VanishResult::VANISHED,
                 true, "T8033c 前置：沧岚被消逝");
        CHECK_EQ(b.ctx().pet_death_notified[0][0], true, "T8033c 消逝蕴含阵亡登记");
        CHECK_EQ(read_key(b, kNutaoPubTears + 0), 4,
                 "T8033c 消逝跳过死亡时点 → 蚀砚之泪保留");
        CHECK_EQ(read_key(b, kNutaoPubWater + 0), 2, "T8033c 永恒之水保留");
        std::cout << "  [T8033c] 消逝跳过 → 印记保留 通过\n";
    }

    std::cout << "scenario_8033 全部通过" << std::endl;
    return 0;
}
