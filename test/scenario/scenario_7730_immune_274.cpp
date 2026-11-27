#include <utils/sbs_rand.h>
// 场景 7730 —— K413 effect 274 将神·吕布（2496，row25 args[100,100,50] / row26
// args[100,100,100]，kind 同 [7]，db[2] 档位差 50/100）
// 「50%概率免疫异常状态」（row25）/「满体力时100%概率免疫异常状态」（row26）
//   —— soul_lib/k413_immune_steal_seal.cpp
//
// 断言口径：
//   - 免疫票 = ANOMALY duration=1 固定句柄（K171 285 同宠直系），门与掷骰在
//     condition 现场判（apply_anomaly 直施探测，apply 100% 档短路不耗 rand——
//     condition 掷点 = 停泊期 ctx 播种后的首个 draw，peek 锚定防分支互换盲区，T7543d 纪律）；
//   - 满体门 = 现场读 hp（掉血即闭门、回血即重开——逐次施加时点判，K171 口径）；
//   - 审计槽 = 掷前写申报概率（992740000，K400 形）：50 档双支断言 =50、100 档 =100；
//   - 免疫落地计数（992740010）= EVENT_ANOMALY_IMMUNED watcher（停泊期直调后手动
//     drain，090/7573 先例）；
//   - 统计带 = 30 独立局 50% 落地带 [7,23]（期望 15）；boss 门 = 行文无尾注 →
//     boss 局照常免疫（与 K400 T7543h 反向对照）；
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

constexpr int kMaxHp = 99999;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);          // 2
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);  // 5

// 审计/侧证键（与 k413_immune_steal_seal.cpp 一致；owner=0）。
constexpr int kAudit = 992740000;    // 免疫申报概率审计槽（掷前写）
constexpr int kImmCnt = 992740010;   // 免疫落地计数（ANOMALY_IMMUNED）

const std::vector<int> kRow50{100, 100, 50};    // row25（无门 50%）
const std::vector<int> kRow100{100, 100, 100};  // row26（满体门 100%）

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(274, "K413 将神·吕布", "test", EffectArgs(args));
    sm.monster_id = 2496;
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "将神·吕布", {10004, 10004, 10004, 10004, 10004},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {10004, 10004, 10004, 10004, 10004},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

// 停泊期直施探测（apply 100% 申报短路不耗 rand → condition 掷点=首个 draw）。
bool probe_immune(const std::vector<int>& args, int seed, bool hurt_first = false) {
    ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
    b.start();
    auto& ctx = b.ctx();
    if (hurt_first) {
        (void)deal_pink_damage(&ctx, 0, 1000, DamageKind::FIXED, /*actor=*/1);
    }
    ctx.seed_rng(seed);
    return apply_anomaly(&ctx, 0, kBurn, 2, /*actor=*/1, /*chance_pct=*/100)
        == ApplyAnomalyResult::TARGET_IMMUNE;
}

// peek：用独立流预演 condition 首掷（draw % 100 < 50 ↔ row25 免疫支）。
bool peek_under50(int seed) {
    SbsRng rng(seed);
    return (rng.next() % 100) < 50;
}

int find_anchored(const std::vector<int>& args, bool want_immune) {
    for (int s = 1; s < 500; ++s) {
        if (peek_under50(s) == want_immune
            && probe_immune(args, s, /*hurt_first=*/true) == want_immune) {
            return s;
        }
    }
    return -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7730a row26 满体门 100%：满血免疫 / 掉血闭门 / 回血重开 ═════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow100)), make_party(foe_pet()));
        b.start();
        auto& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, /*actor=*/1, /*chance_pct=*/100),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T7730a 满血免疫异常");
        ctx.event_center_.drain(&ctx, ctx.roundCount);
        CHECK(!ctx.has_active_abnormal_status(0, kBurn), "T7730a 无烧伤在身");
        CHECK_EQ(read_key(b, kAudit), 100, "T7730a 审计=100（100 档申报）");
        CHECK_EQ(read_key(b, kImmCnt), 1, "T7730a 免疫落地计数=1");
        // 掉血 → 满体门闭（现场判，K171 口径）。
        (void)deal_pink_damage(&ctx, 0, 1000, DamageKind::FIXED, 1);
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, 1, 100), ApplyAnomalyResult::SUCCESS,
                 "T7730a 掉血后门闭落地");
        CHECK(ctx.has_active_abnormal_status(0, kBurn), "T7730a 闭门支烧伤在身");
        CHECK_EQ(read_key(b, kImmCnt), 1, "T7730a 闭门支无免疫计数");
        CHECK_EQ(read_key(b, kAudit), 100, "T7730a 门闭支不重写审计（门判先于掷点）");
        // 回血 → 门重开（免疫窗未过期——ROUND_START 重授票仍在）。
        ctx.getPet(0).hp = kMaxHp;
        CHECK_EQ(apply_anomaly(&ctx, 0, kFrostbite, 2, 1, 100),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T7730a 回血门重开");
        ctx.event_center_.drain(&ctx, ctx.roundCount);
        CHECK_EQ(read_key(b, kImmCnt), 2, "T7730a 免疫落地计数=2");
        std::cout << "  [T7730a] row26 满体门 通过\n";
    }

    // ══ T7730b row25 无门 50%：非满血照掷 + peek 锚定双支 + 审计=50 ═════════
    {
        const int s_hit = find_anchored(kRow50, true);
        const int s_miss = find_anchored(kRow50, false);
        CHECK(s_hit > 0 && s_miss > 0, "T7730b peek 锚定找到双支种子");
        {   // 免疫支（掷点 <50）
            ScenarioBattle b(make_party(mark_pet(kRow50)), make_party(foe_pet()));
            b.start();
            auto& ctx = b.ctx();
            (void)deal_pink_damage(&ctx, 0, 1000, DamageKind::FIXED, 1);   // 非满血
            ctx.seed_rng(s_hit);
            CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, 1, 100),
                     ApplyAnomalyResult::TARGET_IMMUNE, "T7730b 免疫支（<50）");
            ctx.event_center_.drain(&ctx, ctx.roundCount);
            CHECK_EQ(read_key(b, kAudit), 50, "T7730b 免疫支审计=50（档位差位）");
            CHECK_EQ(read_key(b, kImmCnt), 1, "T7730b 免疫支落地计数=1");
        }
        {   // 落地支（掷点 >=50）
            ScenarioBattle b(make_party(mark_pet(kRow50)), make_party(foe_pet()));
            b.start();
            auto& ctx = b.ctx();
            (void)deal_pink_damage(&ctx, 0, 1000, DamageKind::FIXED, 1);
            ctx.seed_rng(s_miss);
            CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, 1, 100), ApplyAnomalyResult::SUCCESS,
                     "T7730b 落地支（>=50）");
            CHECK(ctx.has_active_abnormal_status(0, kBurn), "T7730b 落地支烧伤在身");
            CHECK_EQ(read_key(b, kAudit), 50, "T7730b 落地支审计照写（掷前写）");
            CHECK_EQ(read_key(b, kImmCnt), -1, "T7730b 落地支无免疫计数（键未写）");
        }
        std::cout << "  [T7730b] row25 无门 50% 双支 通过\n";
    }

    // ══ T7730c 统计带：30 独立局 50% 免疫带 [7,23]（期望 15）════════════════
    {
        int immune = 0;
        for (int s = 7731; s < 7761; ++s) {   // bounded for，一局一刀
            if (probe_immune(kRow50, s, /*hurt_first=*/true)) {
                ++immune;
            }
        }
        CHECK(immune >= 7 && immune <= 23, "T7730c 50% 免疫带 [7,23]（实测 " +
                                               std::to_string(immune) + "/30）");
        std::cout << "  [T7730c] 统计带 通过 (" << immune << "/30)\n";
    }

    // ══ T7730d boss 门：行文无尾注 → boss 局照常免疫（与 boss无效族反向）════
    {
        ScenarioBattle b(make_party(mark_pet(kRow100)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        auto& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, 1, 100),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T7730d boss 局照常免疫");
        ctx.event_center_.drain(&ctx, ctx.roundCount);
        CHECK_EQ(read_key(b, kAudit), 100, "T7730d boss 局审计照写");
        CHECK_EQ(read_key(b, kImmCnt), 1, "T7730d boss 局免疫计数=1");
        CHECK(!ctx.has_active_abnormal_status(0, kBurn), "T7730d boss 局无烧伤");
        std::cout << "  [T7730d] boss 无尾注照常 通过\n";
    }

    // ══ T7730e FSM 外直施只消费对局流，非对局流不参与 ═══════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow50)), make_party(foe_pet()));
        b.start();
        auto& ctx = b.ctx();
        ctx.seed_rng(1);  // 首掷 16807，07 < 50 → 免疫。
        sbs_srand(8);     // 首掷 134456，56 >= 50 → 若误用非对局流则不免疫。
        const auto result = apply_anomaly(&ctx, 0, kBurn, 2, 1, 100);
        CHECK_EQ(result, ApplyAnomalyResult::TARGET_IMMUNE, "T7730e FSM 外直施使用对局流");
        CHECK_EQ(ctx.rng().state(), 16807ULL, "T7730e 仅消耗一次对局掷点");
        const int non_battle_draw = sbs_rand();
        CHECK_EQ(non_battle_draw, 134456, "T7730e 非对局流仍在首掷");
    }

    std::cout << "scenario_7730: all green" << std::endl;
    return 0;
}
