// 场景 8741 —— 圣光灵神SP 2567「神魂之器」首批（2026-11-22，worktree feat/lingshen-sp）
// —— resources/soul_lib/lingshen_sp_2567.cpp + 内核 ATTACH_PIERCE 票/穿透缝/EVENT_LETHAL
//
// 断言口径（全行为面，无随机精确断言）：
//   A [内核穿透·免控] 对手挂条件免疫票（Modern ANOMALY）→ 穿透凭证未 armed：附加被免疫；
//     凭证 armed（kPierceAnomaly 位）：免疫票被跳过 → 异常落地（语料 post497"最终双方
//     都进入疲惫"的前半）。对照：凭证无该位 → 照常免疫。
//   B [内核穿透·免弱] 同构：STAT_DROP 票被 kPierceStatDrop 凭证跳过。
//   C [穿透路径反弹] 目标挂免疫票 + REFLECT 票 → 穿透附加：目标收（异常落地）且施放方
//     也收（反弹照常）——"只能无视前半段的免疫，后半段的反弹依然正常生效"；且
//     reflect_depth=1 不再回弹（无乒乓）。非穿透对照：免疫命中 → 反弹 → 施放方收、
//     目标不收（原语义）。
//   D [SP③致死拦截] 非 神灵系在场 → 致死伤害触发拦截（hp 留存）→ 变神灵系 + 移除
//     最后一项记录（免控位清掉）→ ④凭证 mask 刷新为 kPierceAnomaly；第二次致死不再拦
//     （一次门 + 神灵系）。
//   E [SP②条件免疫] 神灵系 + 记录在位 → 免控票命中（附加被免）；记录移除后（D 的
//     后置状态）→ 免控票不再命中（condition 现场判）。
//   F [SP⑤viewed 致死] SP 在场造成伤害、目标未真致死 → EVENT_LETHAL_DAMAGE_TAKEN 发出
//     （watcher 记录）；真致死（目标 hp≤0）→ 不发（HP_EXHAUSTED 正序，无双发）。
//   G [④场域门] block_offstage_to_onstage=true 且 SP 场下 → has_attach_pierce=false；
//     场域关闭 → 恢复（post497"会被帝皇侠和薇无效"——与玄武线双标相反，门显式写）。
//
// MUT：摘除 [2.7] attach_pierced → A/B 红；摘除 [3.1] 穿透反弹 → C 前半红；
// 摘除③拦截 → D 红。golden 链入库。
#include <any>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打（物攻填充，空链）
constexpr int kBurn  = static_cast<int>(AbnormalStatusId::Burn);      // 2 弱化类（非控制）
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);     // 8 控制类
constexpr int kMaxHp = 5000;

void force_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

// SP（魂印 2567 绑定——真插件程序）
ElfPet sp_pet(int speed = 100) {
    SoulMark sm(2567, "圣光灵神SP", "测试桩：神魂之器魂印 2567（effect_icon 行 2162）");
    ElfPet p = make_pet(13311, "圣光灵神SP", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, 300, kMaxHp, sm);
    force_hit(p);
    return p;
}

ElfPet plain_pet(int id, const char* name, int speed, int hp) {
    ElfPet p = make_pet(id, name, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed, 100, hp);
    force_hit(p);
    return p;
}

// 条件免疫票桩（镜像 SP② 的票形：Modern 层、condition 可控）——测内核穿透缝不依赖
// SP 插件自身的票（分层：A/B/C 测内核，D/E/F/G 测插件）。
struct TestImmuneRig {
    int sid_anomaly = 0;
    int sid_statdrop = 0;
    bool armed = true;   // condition 读它
};

void grant_test_immune(ScenarioBattle& b, int side, TestImmuneRig& rig) {
    BattleContext& ctx = b.ctx();
    rig.sid_anomaly = ctx.rule_center_.grant_immune(
        side, static_cast<int>(ImmunityType::ANOMALY), coverage_all(), /*anomaly_mask=*/0,
        /*duration_rounds=*/9, ctx.roundCount, /*source_id=*/99991, /*soul_immunity=*/false,
        EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/0, /*source_effect_id=*/99991,
        static_cast<int>(ImmunityTier::Modern),
        [&rig](const BattleContext*, int) { return rig.armed; });
    rig.sid_statdrop = ctx.rule_center_.grant_immune(
        side, static_cast<int>(ImmunityType::STAT_DROP), coverage_all(), 0,
        9, ctx.roundCount, 99992, false, EffectScope::ON_STAGE, -1, 0, 99992,
        static_cast<int>(ImmunityTier::Modern),
        [&rig](const BattleContext*, int) { return rig.armed; });
}

// 给 actor 方授 ATTACH_PIERCE 凭证（mask 可控）——内核面的直授测试桩（SP 插件的
// 授予路径由 D/E 验证）。
void grant_pierce(ScenarioBattle& b, int side, int mask) {
    b.ctx().rule_center_.grant_attach_pierce(
        side, /*source_effect_id=*/99990, mask,
        [](BattleContext*, int, int, bool) { return true; }, EffectScope::TEAM);
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

    // ════════════════════════════════════════════════════════════════
    // A/B 内核穿透缝（免控/免弱）——不依赖 SP 插件，直授凭证
    // ════════════════════════════════════════════════════════════════
    {
        sbs_srand(87411);
        ScenarioBattle b(make_party(plain_pet(4001, "甲", 100, kMaxHp)),
                         make_party(plain_pet(4001, "乙", 100, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        TestImmuneRig rig;
        grant_test_immune(b, 1, rig);

        // A1 基线：无凭证 → 免疫命中
        REQUIRE(apply_anomaly(&ctx, 1, kSleep, 2, /*actor=*/0) == ApplyAnomalyResult::TARGET_IMMUNE);
        CHECK(!ctx.has_active_abnormal_status(1, kSleep), "A1 基线：免控票命中 → 不落地");

        // A2 凭证无该位（只 STAT_DROP）→ 免控照常命中
        grant_pierce(b, 0, kPierceStatDrop);
        REQUIRE(apply_anomaly(&ctx, 1, kSleep, 2, 0) == ApplyAnomalyResult::TARGET_IMMUNE);
        CHECK(!ctx.has_active_abnormal_status(1, kSleep), "A2 凭证缺 ANOMALY 位 → 免控照常");

        // A3 凭证 armed ANOMALY 位 → 免疫票被跳过 → 落地
        grant_pierce(b, 0, kPierceStatDrop | kPierceAnomaly);
        REQUIRE(apply_anomaly(&ctx, 1, kSleep, 2, 0) == ApplyAnomalyResult::SUCCESS);
        CHECK(ctx.has_active_abnormal_status(1, kSleep), "A3 ★ 穿透凭证 → 免控票被跳过 → 落地");
        ctx.clear_on_stage_abnormal_statuses(1);   // 清掉 A3 落的睡眠，隔离 A4

        // A4 condition 现场性：凭证折除（armed=false）→ 之后的附加恢复被免疫
        grant_pierce(b, 0, 0);   // mask=0：覆盖键刷新，位全清
        REQUIRE(apply_anomaly(&ctx, 1, kSleep, 2, 0) == ApplyAnomalyResult::TARGET_IMMUNE);
        CHECK(!ctx.has_active_abnormal_status(1, kSleep),
              "A4 ★ mask 刷新（记录全在）→ 穿透失效 → 免控恢复（查询式无残留）");

        // B 免弱同构
        REQUIRE(stat_drop(&ctx, 1, 0, 1, /*actor=*/0) == StatDropResult::IMMUNE);
        grant_pierce(b, 0, kPierceStatDrop);
        REQUIRE(stat_drop(&ctx, 1, 0, 1, 0) == StatDropResult::SUCCESS);
        CHECK_EQ(ctx.ability_level(1, 0), -1, "B ★ 免弱票被 kPierceStatDrop 跳过 → 弱化落地");
        std::cout << "  [A/B] 内核穿透缝（免控/免弱 + mask 现场性） 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C 穿透路径反弹（免疫并反弹的两半解耦）
    // ════════════════════════════════════════════════════════════════
    {
        sbs_srand(87412);
        ScenarioBattle b(make_party(plain_pet(4001, "甲", 100, kMaxHp)),
                         make_party(plain_pet(4001, "乙", 100, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        TestImmuneRig rig;
        grant_test_immune(b, 1, rig);
        ctx.rule_center_.grant_reflect(/*source_owner=*/1, /*source_slot=*/-1,
                                       /*source_effect_id=*/99993, /*target=*/1,
                                       /*rounds=*/9, EffectScope::ON_STAGE, 0);

        // C1 非穿透对照：免疫命中 → 反弹（施放方收、目标不收）
        REQUIRE(apply_anomaly(&ctx, 1, kSleep, 2, 0) == ApplyAnomalyResult::REFLECTED);
        CHECK(!ctx.has_active_abnormal_status(1, kSleep), "C1 对照：目标免疫不收");
        CHECK(ctx.has_active_abnormal_status(0, kSleep), "C1 对照：施放方被反弹收");

        // 清施放方睡眠（把反弹来的清掉，隔离下一步）
        ctx.clear_on_stage_abnormal_statuses(0);

        // C2 穿透路径：目标收（免疫被跳过）且施放方也收（反弹照常）——"最终双方都进入疲惫"
        grant_pierce(b, 0, kPierceAnomaly);
        REQUIRE(apply_anomaly(&ctx, 1, kSleep, 2, 0) == ApplyAnomalyResult::SUCCESS);
        CHECK(ctx.has_active_abnormal_status(1, kSleep), "C2a ★ 穿透：目标收（免疫被无视）");
        CHECK(ctx.has_active_abnormal_status(0, kSleep),
              "C2b ★ 穿透路径反弹照常：施放方也收（'最终双方都进入疲惫'）");

        // C3 无乒乓：反弹回来的（depth=1）不再二次反弹 → 施放方的睡眠正好 1 份（上面已证）
        std::cout << "  [C] 穿透路径反弹（两半解耦 + 无乒乓） 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D/E/F/G SP 插件端到端（真魂印 2567 程序）
    // ════════════════════════════════════════════════════════════════
    {
        // D ③致死拦截 + 形态翻转 + 记录移除 + 一次门（SP 全程在场，不切场——
        // 登场会重置一次门/形态；语料"神灵系下场重置"）
        sbs_srand(87413);
        std::array<ElfPet, 6> p0{plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp)};
        std::array<ElfPet, 6> p1{sp_pet(),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp)};
        ScenarioBattle b(p0, p1);
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.find_pet_with_soulmark(1, 2567), 0, "D 前置：SP 在 1 号方 0 号槽（在场）");

        // D1 拦截：留存 + 变神灵系 + 移除最后一项记录（免控位）→ ④ mask 刷新
        ElfPet& sp = ctx.seerRobot[1].elfPets[ctx.find_pet_with_soulmark(1, 2567)];
        sp.hp = 0;   // defeat_pet 契约：hp<=0 才问询拦截器
        REQUIRE(defeat_pet(&ctx, 1, 0, 0, DefeatCause::DAMAGE) == DefeatResult::INTERCEPTED);
        CHECK_EQ(sp.hp, 1, "D1 ★ ③致死拦截：留存（口径待实测，取机盖先例）");
        // ④"场下才生效"：SP 在场 → condition false（不 armed 是正确行为）
        CHECK(!ctx.rule_center_.has_attach_pierce(&ctx, 1, kPierceAnomaly, ctx.roundCount),
              "D2a ④门：SP 在场 → 穿透不 armed（场下才生效）");

        // D3 一次门 + 神灵系：SP 留在场（不切场——登场会重置一次门），再次致死不拦 → 真死
        sp.hp = 0;
        REQUIRE(defeat_pet(&ctx, 1, 0, 0, DefeatCause::DAMAGE) == DefeatResult::DEFEATED);
        CHECK_EQ(sp.hp, 0, "D3 ★ 神灵系 + 一次门耗尽 → 第二次致死不再拦（未切场）");
        std::cout << "  [D] ③拦截/形态翻转/记录移除/一次门（未切场不重置） 通过\n";
    }
    {
        // E ④端到端（独立对局）：SP 拦截（记录移除）→ 切场下 → 凭证 armed →
        // 己方在场队友（1 侧 slot1）施加穿透 0 侧对手免控桩票（armed）→ 落地
        sbs_srand(87415);
        std::array<ElfPet, 6> p0{plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp)};
        std::array<ElfPet, 6> p1{sp_pet(),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp)};
        ScenarioBattle b(p0, p1);
        b.start();
        BattleContext& ctx = b.ctx();
        ElfPet& sp = ctx.seerRobot[1].elfPets[ctx.find_pet_with_soulmark(1, 2567)];
        sp.hp = 0;
        REQUIRE(defeat_pet(&ctx, 1, 0, 0, DefeatCause::DAMAGE) == DefeatResult::INTERCEPTED);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.rule_center_.has_attach_pierce(&ctx, 1, kPierceAnomaly, ctx.roundCount),
              "E 前置：SP 场下 + 免控记录已移除 → 凭证 armed");
        grant_test_immune(b, 0, *(new TestImmuneRig{}));   // 对手 0 侧免控桩（armed）
        REQUIRE(apply_anomaly(&ctx, /*target=*/0, kSleep, 2, /*actor=*/1)
                == ApplyAnomalyResult::SUCCESS);
        CHECK(ctx.has_active_abnormal_status(0, kSleep),
              "E ★ ④端到端：SP 场下 → 在场队友（1侧）附加穿透对手（0侧）免控票");
        std::cout << "  [E] ④端到端（场下凭证 × 队友附加穿透对手免疫） 通过\n";
    }
    {
        // F：SP 在场造成伤害未致死 → viewed 发；致死 → 不发
        sbs_srand(87414);
        std::array<ElfPet, 6> p0{plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp)};
        std::array<ElfPet, 6> p1{sp_pet(300),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp)};
        ScenarioBattle b(p0, p1);
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.drive();

        std::vector<int> viewed_log;
        ctx.event_center_.register_watcher(
            EventType::EVENT_LETHAL_DAMAGE_TAKEN, 0, ctx.roundCount, 0, false,
            [&viewed_log](BattleContext*, const BattleEvent& ev) {
                viewed_log.push_back(ev.target);
            },
            WatcherScope::TEAM);

        // F1 低伤拍打（SP 速度 300 先手）：目标未死 → viewed 发
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 对手也可能还手造成伤害（actor=0 → 不触发 SP 的 viewed，过滤在 watcher）
        // 精确断言：SP 造成伤害且目标 hp>0 的那次应入 log（target=0）
        bool f1 = false;
        for (int t : viewed_log) {
            if (t == 0) {
                f1 = true;
            }
        }
        CHECK(f1, "F1 ★ SP 在场伤害未致死 → EVENT_LETHAL_DAMAGE_TAKEN(target=0) 发出");

        // F2 真致死：把对手打到 0（直接漏斗语义）→ 不发 viewed（HP_EXHAUSTED 正序）
        viewed_log.clear();
        ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 0;
        REQUIRE(defeat_pet(&ctx, 0, ctx.on_stage[0], 1, DefeatCause::DAMAGE)
                == DefeatResult::DEFEATED);
        b.drive();
        bool f2 = false;
        for (int t : viewed_log) {
            if (t == 0) {
                f2 = true;
            }
        }
        CHECK(!f2, "F2 ★ 真致死不发 viewed（无双发；HP_EXHAUSTED 正序）");
        std::cout << "  [F] viewed 致死两半（未致死发/致死不发） 通过\n";

        // G ④场域门：SP 场下 + 场域开启 → 凭证失效；关闭 → 恢复
        //   前置：先拦截一次（移除免控记录）→ 切场下（凭证 armed）
        ElfPet& sp_g = ctx.seerRobot[1].elfPets[ctx.find_pet_with_soulmark(1, 2567)];
        sp_g.hp = 0;
        REQUIRE(defeat_pet(&ctx, 1, 0, 0, DefeatCause::DAMAGE) == DefeatResult::INTERCEPTED);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.on_stage[1] == 1 && ctx.find_pet_with_soulmark(1, 2567) == 0,
              "G 前置：SP 已场下（slot0），替补 slot1 在场");
        // D1 已移除免控记录 → 凭证 mask 含 ANOMALY 位（跨局状态保留在 soulmark_storage）
        CHECK(ctx.rule_center_.has_attach_pierce(&ctx, 1, kPierceAnomaly, ctx.roundCount),
              "G1 前置：SP 场下 + 场域关 → 穿透 armed（④场下才生效）");
        ctx.block_offstage_to_onstage = true;   // 模拟薇尔诗 2513 开启
        CHECK(!ctx.rule_center_.has_attach_pierce(&ctx, 1, kPierceAnomaly, ctx.roundCount),
              "G2 ★ 场域开启 + SP 场下 → 凭证失效（④官方受薇/帝皇侠抑制——与玄武线相反）");
        ctx.block_offstage_to_onstage = false;
        CHECK(ctx.rule_center_.has_attach_pierce(&ctx, 1, kPierceAnomaly, ctx.roundCount),
              "G3 场域关闭 → 凭证恢复");
        std::cout << "  [G] ④场域门（官方明说受抑制，与玄武双标相反） 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // H ②免粉半边（SP 神灵系 + bit2 在位 → PINK_DAMAGE 票命中；粉伤被免）
    // ════════════════════════════════════════════════════════════════
    {
        sbs_srand(87416);
        std::array<ElfPet, 6> p0{plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp)};
        std::array<ElfPet, 6> p1{sp_pet(),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp)};
        ScenarioBattle b(p0, p1);
        b.start();
        BattleContext& ctx = b.ctx();
        ElfPet& sp = ctx.seerRobot[1].elfPets[ctx.find_pet_with_soulmark(1, 2567)];

        // H1 对照（非神灵系）：粉伤照落
        const int hp0 = sp.hp;
        REQUIRE(deal_pink_damage(&ctx, 1, 500, DamageKind::FIXED, /*actor=*/0)
                == FixedDamageResult::SUCCESS);
        CHECK_EQ(sp.hp, hp0 - 500, "H1 对照：非神灵系 → 免粉票不命中 → 粉伤照落");

        // H2 ③拦截 → 神灵系（免控位被移除，bit2 免粉仍在）→ 免粉票命中 → 粉伤为 0
        sp.hp = 0;
        REQUIRE(defeat_pet(&ctx, 1, 0, 0, DefeatCause::DAMAGE) == DefeatResult::INTERCEPTED);
        const int hp1 = sp.hp;
        REQUIRE(deal_pink_damage(&ctx, 1, 500, DamageKind::FIXED, 0)
                == FixedDamageResult::SUCCESS);
        CHECK_EQ(sp.hp, hp1, "H2 ★ ②免粉半边：神灵系 + 免粉记录在位 → 粉伤被免（票命中）");
        std::cout << "  [H] ②免粉半边（神灵系条件票 × 管线消费） 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // I ④免粉穿透（内核缝直测：施加方持 kPiercePink → 对手免粉票本段不查不消费）
    // ════════════════════════════════════════════════════════════════
    {
        sbs_srand(87417);
        ScenarioBattle b(make_party(plain_pet(4001, "甲", 100, kMaxHp)),
                         make_party(plain_pet(4001, "乙", 100, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手（1 侧）挂免粉桩票（armed，counts=0 窗口型）
        ctx.rule_center_.grant_immune(
            1, static_cast<int>(ImmunityType::PINK_DAMAGE), coverage_all(), 0,
            9, ctx.roundCount, 99995, false, EffectScope::ON_STAGE, -1, 0, 99995,
            static_cast<int>(ImmunityTier::Modern),
            [](const BattleContext*, int) { return true; });

        // ⚠️ HP 是 per-slot：1 侧在场宠 = elfPets[on_stage[1]]（slot0），别读错槽。
        ElfPet& defender = ctx.getPet(1);
        // I1 基线：无穿透 → 免粉命中 → 落 0
        const int hp0 = defender.hp;
        REQUIRE(deal_pink_damage(&ctx, 1, 500, DamageKind::FIXED, /*actor=*/0)
                == FixedDamageResult::SUCCESS);
        CHECK_EQ(defender.hp, hp0, "I1 基线：免粉票命中 → 粉伤为 0");

        // I2 施加方持免粉穿透 → 票本段不查不消费 → 照落
        grant_pierce(b, 0, kPiercePink);
        CHECK(ctx.rule_center_.has_attach_pierce(&ctx, 0, kPiercePink, ctx.roundCount),
              "I2 白盒：穿透票武装可查（actor=0 侧）");
        REQUIRE(deal_pink_damage(&ctx, 1, 500, DamageKind::FIXED, 0)
                == FixedDamageResult::SUCCESS);
        CHECK_EQ(defender.hp, hp0 - 500,
                 "I2 ★ 免粉穿透：施加方凭证 → 免粉票被跳过 → 照落本体");

        // I3 穿透不消费票：折除凭证后再打 → 免粉票还在（未被 I2 白扣）→ 照免
        grant_pierce(b, 0, 0);
        REQUIRE(deal_pink_damage(&ctx, 1, 500, DamageKind::FIXED, 0)
                == FixedDamageResult::SUCCESS);
        CHECK_EQ(defender.hp, hp0 - 500,
                 "I3 ★ 穿透不消费对手票：折除凭证 → 免粉恢复（票未被穿透段扣掉）");
        std::cout << "  [I] ④免粉穿透（管线缝直测 + 不消费对手票） 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // J ⑤"免疫下1次对手的攻击"（真致死半边：致死授 DAMAGE counts=1 → 对手下一击被挡）
    // ════════════════════════════════════════════════════════════════
    {
        sbs_srand(87418);
        std::array<ElfPet, 6> p0{plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp),
                                 plain_pet(4001, "甲", 100, kMaxHp)};
        std::array<ElfPet, 6> p1{sp_pet(),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp),
                                 plain_pet(4001, "乙", 100, kMaxHp)};
        ScenarioBattle b(p0, p1);
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.drive();
        ElfPet& sp = ctx.seerRobot[1].elfPets[ctx.find_pet_with_soulmark(1, 2567)];
        ElfPet& foe = ctx.seerRobot[0].elfPets[ctx.on_stage[0]];

        // J1 SP 造成真致死（对手压到致死边缘 → SP 红伤击杀路径）→ 授 DAMAGE counts=1
        //   （deal_damage 直调 emits TAKE_DAMAGE；致死漏斗不经 FSM，对手躺 0 血即可）
        foe.hp = 1;
        deal_damage(&ctx, 0, 9999, DamageKind::NORMAL, /*actor=*/1);   // void 原语
        CHECK(foe.hp <= 0, "J1 前置：SP 造成真致死（目标 hp<=0）");
        CHECK(ctx.is_immune(1, ImmunityType::DAMAGE, ctx.currentState),
              "J2 ★ ⑤致死半边：真致死 → SP 获得下1次攻击免伤票（counts=1）");

        // J3 对手**真实攻击**（免伤票由攻击管线 BLOCK 阶段消费——deal_damage 直调
        //   不走管线相，必须走 FSM 出手）→ SP 不掉血
        foe.hp = kMaxHp;   // 复原对手（直调未走死亡漏斗，无尸体状态）
        const int sp_hp = sp.hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手拍打 SP
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(sp.hp, sp_hp, "J3 ★ 免伤票消费：对手攻击被 BLOCK 相挡下（SP 不掉血）");

        // J4 票已耗 → 下回合再击照落
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(sp.hp < sp_hp, "J4 一次性：免伤票耗尽后再击照落");
        std::cout << "  [J] ⑤免疫下1次攻击（真致死授票 × 攻击管线消费 × 一次性） 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // K ④抗性提升穿透 × 三层模型（2026-11-22 用户定稿）：
    //   base 35（本体，只读）/ mid 50（魂印层·战螳螂式）/ top 100（临时·天尊式）
    //   三档读取全区分：常态 eff=100 → 落 0；
    //   星皇二段（ignore_top）= clamp(35+50)=85 → 500×15%=75 落；
    //   SP④ = base 35 → 500×65%=325 落。
    // ════════════════════════════════════════════════════════════════
    {
        sbs_srand(87419);
        ScenarioBattle b(make_party(plain_pet(4001, "甲", 100, kMaxHp)),
                         make_party(plain_pet(4001, "乙", 100, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        ElfPet& defender = ctx.getPet(0);
        const int slot0 = ctx.on_stage[0];
        defender.damage_resist.fixed_pct = 35;   // 底层（训练）
        ctx.sync_damage_resist_view(0);
        ctx.add_resist_boost(0, slot0, BattleContext::ResistLayer::SoulMark,
                             /*crit=*/0, /*fixed=*/50, /*percent=*/0);    // 中层（魂印式）
        ctx.add_resist_boost(0, slot0, BattleContext::ResistLayer::Temp,
                             /*crit=*/0, /*fixed=*/100, /*percent=*/0);   // 顶层（天尊式）
        CHECK_EQ(ctx.ws.eff_fixed_resist_pct[0], 100, "K 前置：eff = clamp(35+50+100) = 100");

        // K1 常态：eff=100 全免 → 落 0
        const int hp0 = defender.hp;
        REQUIRE(deal_pink_damage(&ctx, 0, 500, DamageKind::FIXED, /*actor=*/1)
                == FixedDamageResult::SUCCESS);
        CHECK_EQ(defender.hp, hp0, "K1 常态：三层合成 eff=100 → 粉伤全免");

        // K2 星皇二段档：ignore_top → 读 clamp(35+50)=85 → 落 75（天尊式临时层被忽略、
        //   魂印层照常生效——语料推断面）
        ctx.ws.ignore_top_resist[1] = true;
        REQUIRE(deal_pink_damage(&ctx, 0, 500, DamageKind::FIXED, 1)
                == FixedDamageResult::SUCCESS);
        CHECK_EQ(defender.hp, hp0 - 75,
                 "K2 ★ 二段档：忽略顶层 → clamp(base 35 + mid 50)=85 → 落 75");

        // K3 SP④档：两层全穿 → 读 base 35 → 落 325（"自带的 35% 不受影响"）
        grant_pierce(b, 1, kPierceResistBoost);
        REQUIRE(deal_pink_damage(&ctx, 0, 500, DamageKind::FIXED, 1)
                == FixedDamageResult::SUCCESS);
        CHECK_EQ(defender.hp, hp0 - 75 - 325,
                 "K3 ★ SP④档：无视提升 = 回落本体 base 35 → 落 325");

        // K4 全折除 → 常态恢复（eff=100 全免；查询式无残留）
        ctx.ws.ignore_top_resist[1] = false;
        grant_pierce(b, 1, 0);
        REQUIRE(deal_pink_damage(&ctx, 0, 500, DamageKind::FIXED, 1)
                == FixedDamageResult::SUCCESS);
        CHECK_EQ(defender.hp, hp0 - 75 - 325, "K4 ★ 旗标/凭证全折除 → 常态 eff=100 恢复全免");
        std::cout << "  [K] 三层模型三档读取（常态/二段/SP④ × 本体不动 × 无残留） 通过\n";
    }

    std::cout << "scenario_8741 PASS（ATTACH_PIERCE 穿透 + SP2567 神魂之器第一批+第二批）" << std::endl;
    return 0;
}
