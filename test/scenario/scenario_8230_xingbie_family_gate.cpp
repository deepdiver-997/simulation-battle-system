// 场景 8230 —— J336 性别码表家族门（129/131 勘误重验 + 130/898 同码表隔离）
//（号段 8230-8239，2026-11-08，J336 单；插件 j_power_cond_double.cpp(129) /
//  k419_meiheweite_pool.cpp(131) / k435_xingbie_fushang_pool.cpp(130 对照)）
//
// 背景：129（J289）/131（K419）原实现把 {0} 当 skill_types 元素 id——K435 定案
// 证伪（{0}=性别码：1=雄 / 2=雌；BWIKI 效果:129/130/131 三页逐载体文案 + 引擎
// 600 同码表 + 898"不是雄性精灵"链内互补），J336 重实现两判据后本场景做家族级
// 回归（既有 6220/7970 号段的宏内正反例已同步重算，本场景管**跨宏面**）。
//
// 载体（DB moves 现查 2026-11-08）：
//   12296 蔚蓝之击 [129]|[2]（雌门 物45）   32098 噩梦袭击 [129]|[1]（雄门 特120）
//   21197 红光幻盾 [131]|[1]（雄门 属性技）  21193 蓝光幻盾 [131]|[2]（雌门 属性技）
//   12297 深蓝利刃 [130]|[2,60]（雌门 rider60）12304 深红之光 [130]|[1,100]（雄门 rider100）
//   19343 净世闭月断 [180,130,898]|[1,250,250]（130 切片雄门 rider250 + 898 哨兵）
//
// 审计键（各实现文件账本；交叉缺席断言 = 键空间隔离）：
//   129 → 991290000+owner（倍率 1/2）    131 → 101310001（匹配位 0/1）
//   130 → 1001300000+owner×16+slot（8 槽审计链）
//
// 覆盖：
//   T8230 129 码2雌门双向：vs FEMALE 翻倍 90 / vs MALE 45（审计 2/1）
//   T8231 129 码1雄门双向：vs MALE 240 / vs FEMALE 120（审计 2/1）
//   T8232 129 无性别门闭：[2] vs NONE → 45（600/130 同判：NONE 不属雄/雌）
//   T8233 131 码1雄门双向：vs MALE 免疫挡伤+次回合过期 / vs FEMALE 伤害落地
//   T8234 131 码2雌门双向：vs FEMALE 免疫挡伤 / vs MALE 伤害落地
//   T8235 131 无性别门闭：[1] vs NONE → 匹配位 0、伤害落地
//   T8236 129×130 同码表隔离：129 局（翻倍）130 键段零写入；130 局（rider+60
//         同种子精确差值）129/131 键零写入——三效果键空间互不串写
//   T8237 131×130 同码表隔离：131 局（免疫）130 键段零写入；130 局（rider+100
//         同种子精确差值）131/129 键零写入
//   T8238 19343 混链同码表 + 898 哨兵：vs MALE → 130 切片 [1,250]
//         rider+250；FEMALE 对照局预扣 500hp 回血 250（898"若对手不是雄性精灵
//         恢复250"条件真——K436 落地后 898 在册，no-op 世界不复存在，哨兵取
//         正向断言；2026-11-09 勘误：原断言误写"不回血"，仅在 898 未注册时碰巧绿）
//   T8239 确定性：T8231 正例同种子两开 → 审计与对手 hp 全等（129/131 全确定性档）
//
// 确定性纪律：129/131 无概率 → 全部断言种子无关；130/红伤带 0.84~1.0 方差 →
// 一律同种子"门开 vs 门关"对照取精确差值。攻击载体 must_hit=1 短路命中率链；
// 暴击率 harness 默认清零。129 威力断言按坑集 #6 读 ws.skill_power_view 字段。
#include <any>
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa    = 10004;   // 拍打：物40 填充刀
constexpr int kWeiLan   = 12296;   // 蔚蓝之击 [129]|[2]：物45 雌门
constexpr int kEMeng    = 32098;   // 噩梦袭击 [129]|[1]：特120 雄门
constexpr int kHongGuang = 21197;  // 红光幻盾 [131]|[1]：属性技 雄门
constexpr int kLanGuang = 21193;   // 蓝光幻盾 [131]|[2]：属性技 雌门
constexpr int kShenLan  = 12297;   // 深蓝利刃 [130]|[2,60]：物100 雌门 rider60
constexpr int kShenHong = 12304;   // 深红之光 [130]|[1,100]：特100 雄门 rider100
constexpr int kJingShi  = 19343;   // 净世闭月断 [180,130,898]|[1,250,250]：物160

constexpr int k129Audit = 991290000;   // +owner：倍率（1=未触发 2=翻倍）
constexpr int k131Audit = 101310001;   // 匹配位（1=真 0=假）
constexpr int k130Audit = 1001300000;  // +owner×16+slot：8 槽审计链

int stor(const BattleContext& ctx, int key, int def = 0) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

bool key_absent(const BattleContext& ctx, int key) {
    return ctx.plugin_storage.find(key) == ctx.plugin_storage.end();
}

// 130 键段 owner 侧 8 槽全部缺席（隔离断言用）。
bool keys_130_absent(const BattleContext& ctx, int owner) {
    for (int slot = 0; slot < 8; ++slot) {
        if (!key_absent(ctx, k130Audit + owner * 16 + slot)) return false;
    }
    return true;
}

constexpr int kMyMax = 400000;
constexpr int kFoeMax = 600000;

ElfPet caster(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = scenario::make_pet(0, "施加方", skills, speed, /*atk=*/300, kMyMax);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(Gender g = Gender::NONE, int speed = 100) {
    ElfPet p = scenario::make_pet(1, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  speed, /*atk=*/1, kFoeMax);
    p.gender = g;
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 我方出载体、对手静默（8200 run_cast 同式；130 rider 差值对照与 898 哨兵靠它）。
void run_cast(ScenarioBattle& b, int skill_index, int seed, Gender foe_gender) {
    b.ctx().getPet(1).gender = foe_gender;
    b.ctx().seed_rng(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// 双方各出一刀（对手拍打反击；131 免疫挡伤/过期行为断言靠它，7970 round_drive 同式）。
void round_drive(ScenarioBattle& b, int my_idx, Gender foe_gender) {
    b.ctx().getPet(1).gender = foe_gender;
    b.ctx().seed_rng(static_cast<unsigned>(20261108));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, my_idx);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 停泊到己方 FIRST_SKILL_EFFECT 之后，返回威力视图（129 效果已执行；6220 同式）。
int power_after_skill_effect(ScenarioBattle& b, BattleContext& ctx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    return ctx.ws.skill_power_view[0].value;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T8230 129 码2雌门双向：[2] vs 雌翻倍 / vs 雄不翻 ==========
    {
        int view_on = 0, view_off = 0;
        {
            ScenarioBattle b(scenario::make_party(caster({kWeiLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe(Gender::FEMALE)));
            b.start();
            BattleContext& ctx = b.ctx();
            view_on = power_after_skill_effect(b, ctx);
            CHECK_EQ(view_on, 90, "T8230 [2]雌门 vs 雌性 → 45×2=90");
            CHECK_EQ(stor(ctx, k129Audit + 0), 2, "T8230 审计倍率=2");
        }
        {
            ScenarioBattle b(scenario::make_party(caster({kWeiLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe(Gender::MALE)));
            b.start();
            BattleContext& ctx = b.ctx();
            view_off = power_after_skill_effect(b, ctx);
            CHECK_EQ(view_off, 45, "T8230 反例：[2]雌门 vs 雄性 → 45 不翻");
            CHECK_EQ(stor(ctx, k129Audit + 0), 1, "T8230 反例：审计倍率=1");
        }
        CHECK_EQ(view_on - view_off, 45, "T8230 双向差值：翻倍增量=DB 静态威力 45");
        std::cout << "  [T8230] 129 码2雌门双向 通过\n";
    }

    //========== T8231 129 码1雄门双向：[1] vs 雄翻倍 / vs 雌不翻 ==========
    {
        int hp_on = 0;
        {
            ScenarioBattle b(scenario::make_party(caster({kEMeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe(Gender::MALE)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int view = power_after_skill_effect(b, ctx);
            CHECK_EQ(view, 240, "T8231 [1]雄门 vs 雄性 → 120×2=240");
            CHECK_EQ(stor(ctx, k129Audit + 0), 2, "T8231 审计倍率=2");
            hp_on = ctx.getPet(1).hp;
        }
        {
            ScenarioBattle b(scenario::make_party(caster({kEMeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe(Gender::FEMALE)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int view = power_after_skill_effect(b, ctx);
            CHECK_EQ(view, 120, "T8231 反例：[1]雄门 vs 雌性 → 120 不翻");
            CHECK_EQ(stor(ctx, k129Audit + 0), 1, "T8231 反例：审计倍率=1");
        }
        (void)hp_on;   // T8239 复用同口径种子做确定性对照
        std::cout << "  [T8231] 129 码1雄门双向 通过\n";
    }

    //========== T8232 129 无性别门闭：[2] vs NONE → 45 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kWeiLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe(Gender::NONE)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int view = power_after_skill_effect(b, ctx);
        CHECK_EQ(view, 45, "T8232 [2]雌门 vs 无性别 → 45 不翻（NONE 不属雌，600/130 同判）");
        CHECK_EQ(stor(ctx, k129Audit + 0), 1, "T8232 审计倍率=1");
        std::cout << "  [T8232] 129 无性别门闭 通过\n";
    }

    //========== T8233 131 码1雄门双向：红光幻盾 [1] vs 雄免疫 / vs 雌落地 ==========
    {
        {
            ScenarioBattle b(scenario::make_party(caster({kHongGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe(Gender::MALE)));
            b.start();
            round_drive(b, 0, Gender::MALE);   // R1 红光幻盾 vs 拍打
            BattleContext& ctx = b.ctx();
            CHECK_EQ(stor(ctx, k131Audit), 1, "T8233 [1]雄门 vs 雄性 → 匹配位=1");
            CHECK_EQ(ctx.getPet(0).hp, kMyMax, "T8233 免疫当前回合伤害（HP 不变）");
            round_drive(b, 1, Gender::MALE);   // R2 拍打过渡（票 InRounds=1 已过期）
            CHECK(ctx.getPet(0).hp < kMyMax, "T8233 次回合票过期（伤害恢复落地）");
        }
        {
            ScenarioBattle b(scenario::make_party(caster({kHongGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe(Gender::FEMALE)));
            b.start();
            round_drive(b, 0, Gender::FEMALE);
            BattleContext& ctx = b.ctx();
            CHECK_EQ(stor(ctx, k131Audit), 0, "T8233 反例：[1]雄门 vs 雌性 → 匹配位=0");
            CHECK(ctx.getPet(0).hp < kMyMax, "T8233 反例：无免疫 → 对手拍打落地");
        }
        std::cout << "  [T8233] 131 码1雄门双向 通过\n";
    }

    //========== T8234 131 码2雌门双向：蓝光幻盾 [2] vs 雌免疫 / vs 雄落地 ==========
    {
        {
            ScenarioBattle b(scenario::make_party(caster({kLanGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe(Gender::FEMALE)));
            b.start();
            round_drive(b, 0, Gender::FEMALE);
            BattleContext& ctx = b.ctx();
            CHECK_EQ(stor(ctx, k131Audit), 1, "T8234 [2]雌门 vs 雌性 → 匹配位=1（码2→雌直证）");
            CHECK_EQ(ctx.getPet(0).hp, kMyMax, "T8234 免疫当前回合伤害（HP 不变）");
        }
        {
            ScenarioBattle b(scenario::make_party(caster({kLanGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe(Gender::MALE)));
            b.start();
            round_drive(b, 0, Gender::MALE);
            BattleContext& ctx = b.ctx();
            CHECK_EQ(stor(ctx, k131Audit), 0, "T8234 反例：[2]雌门 vs 雄性 → 匹配位=0");
            CHECK(ctx.getPet(0).hp < kMyMax, "T8234 反例：无免疫 → 对手拍打落地");
        }
        std::cout << "  [T8234] 131 码2雌门双向 通过\n";
    }

    //========== T8235 131 无性别门闭：红光幻盾 [1] vs NONE → 落地 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kHongGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe(Gender::NONE)));
        b.start();
        round_drive(b, 0, Gender::NONE);
        BattleContext& ctx = b.ctx();
        CHECK_EQ(stor(ctx, k131Audit), 0, "T8235 [1]雄门 vs 无性别 → 匹配位=0");
        CHECK(ctx.getPet(0).hp < kMyMax, "T8235 无免疫 → 对手拍打落地");
        std::cout << "  [T8235] 131 无性别门闭 通过\n";
    }

    //========== T8236 129×130 同码表隔离：键空间互不串写 + 130 rider60 照常 ==========
    {
        {   // 129 局：翻倍照常，130 键段与 131 键零写入。
            ScenarioBattle b(scenario::make_party(caster({kWeiLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe(Gender::FEMALE)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int view = power_after_skill_effect(b, ctx);
            CHECK_EQ(view, 90, "T8236 129 局：雌门翻倍照常");
            CHECK(keys_130_absent(ctx, 0), "T8236 129 局：130 键段（owner0 8 槽）零写入");
            CHECK(key_absent(ctx, k131Audit), "T8236 129 局：131 匹配位键零写入");
        }
        {   // 130 局：深蓝利刃 [2,60] vs 雌 → rider+60（同种子 vs 雄差值）；129/131 键零写入。
            ScenarioBattle b_on(scenario::make_party(caster({kShenLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                                scenario::make_party(foe(Gender::FEMALE)));
            b_on.start();
            run_cast(b_on, 0, 20261108, Gender::FEMALE);
            BattleContext& ctx_on = b_on.ctx();
            CHECK_EQ(stor(ctx_on, k130Audit + 0 * 16 + 0), 2, "T8236 130 局：申报码=2（同码表照常）");
            CHECK_EQ(stor(ctx_on, k130Audit + 0 * 16 + 6), 1, "T8236 130 局：成功计数=1");
            const int foe_hp_on = ctx_on.getPet(1).hp;
            CHECK(key_absent(ctx_on, k129Audit + 0), "T8236 130 局：129 倍率键零写入");
            CHECK(key_absent(ctx_on, k131Audit), "T8236 130 局：131 匹配位键零写入");
            ScenarioBattle b_off(scenario::make_party(caster({kShenLan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                                 scenario::make_party(foe(Gender::MALE)));
            b_off.start();
            run_cast(b_off, 0, 20261108, Gender::MALE);
            CHECK_EQ(kFoeMax - b_off.ctx().getPet(1).hp, kFoeMax - foe_hp_on - 60,
                     "T8236 130 局同种子精确差值：雌性对手多扣 60（rider 逐位）");
        }
        std::cout << "  [T8236] 129×130 同码表隔离 通过\n";
    }

    //========== T8237 131×130 同码表隔离：免疫门与 rider 门键空间互不串写 ==========
    {
        {   // 131 局：红光幻盾 [1] vs 雄免疫；130 键段与 129 键零写入。
            ScenarioBattle b(scenario::make_party(caster({kHongGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe(Gender::MALE)));
            b.start();
            round_drive(b, 0, Gender::MALE);
            BattleContext& ctx = b.ctx();
            CHECK_EQ(stor(ctx, k131Audit), 1, "T8237 131 局：雄门免疫照常");
            CHECK(keys_130_absent(ctx, 0), "T8237 131 局：130 键段零写入");
            CHECK(key_absent(ctx, k129Audit + 0), "T8237 131 局：129 倍率键零写入");
        }
        {   // 130 局：深红之光 [1,100] vs 雄 → rider+100（同种子 vs 雌差值）；131/129 键零写入。
            ScenarioBattle b_on(scenario::make_party(caster({kShenHong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                                scenario::make_party(foe(Gender::MALE)));
            b_on.start();
            run_cast(b_on, 0, 20261109, Gender::MALE);
            BattleContext& ctx_on = b_on.ctx();
            CHECK_EQ(stor(ctx_on, k130Audit + 0 * 16 + 0), 1, "T8237 130 局：申报码=1");
            CHECK_EQ(stor(ctx_on, k130Audit + 0 * 16 + 6), 1, "T8237 130 局：成功计数=1");
            const int foe_hp_on = ctx_on.getPet(1).hp;
            CHECK(key_absent(ctx_on, k131Audit), "T8237 130 局：131 匹配位键零写入");
            CHECK(key_absent(ctx_on, k129Audit + 0), "T8237 130 局：129 倍率键零写入");
            ScenarioBattle b_off(scenario::make_party(caster({kShenHong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                                 scenario::make_party(foe(Gender::FEMALE)));
            b_off.start();
            run_cast(b_off, 0, 20261109, Gender::FEMALE);
            CHECK_EQ(kFoeMax - b_off.ctx().getPet(1).hp, kFoeMax - foe_hp_on - 100,
                     "T8237 130 局同种子精确差值：雄性对手多扣 100（rider 逐位）");
        }
        std::cout << "  [T8237] 131×130 同码表隔离 通过\n";
    }

    //========== T8238 19343 混链同码表 + 898 前向兼容哨兵 ==========
    {
        ScenarioBattle b_on(scenario::make_party(caster({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe(Gender::MALE)));
        b_on.start();
        run_cast(b_on, 0, 20261111, Gender::MALE);
        BattleContext& ctx_on = b_on.ctx();
        CHECK_EQ(stor(ctx_on, k130Audit + 0 * 16 + 0), 1, "T8238 申报码=1（250 切片归 130，雄门）");
        CHECK_EQ(stor(ctx_on, k130Audit + 0 * 16 + 6), 1, "T8238 成功计数=1");
        const int foe_hp_on = ctx_on.getPet(1).hp;
        // 雌性对照：130 门闭 + 898 哨兵（预扣 500 不自愈）。
        ScenarioBattle b_off(scenario::make_party(caster({kJingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe(Gender::FEMALE)));
        b_off.start();
        BattleContext& ctx_off = b_off.ctx();
        ctx_off.getPet(0).hp = kMyMax - 500;
        run_cast(b_off, 0, 20261111, Gender::FEMALE);
        CHECK_EQ(kFoeMax - ctx_off.getPet(1).hp, kFoeMax - foe_hp_on - 250,
                 "T8238 同种子精确差值：雄性对手多扣 250（130 切片性别门）");
        CHECK_EQ(ctx_off.getPet(0).hp, kMyMax - 250,
                 "T8238 898 哨兵：对手雌性时「不是雄性恢复250」条件真，预扣 500 回血 250"
                 "（2026-11-09 勘误：原断言误写不回血、依赖 898 no-op 旧世界，K436 正确实现落地后必红）");
        std::cout << "  [T8238] 19343 混链 + 898 哨兵 通过\n";
    }

    //========== T8239 确定性：T8231 正例同种子两开 → 审计与对手 hp 全等 ==========
    {
        int hp_a = 0, hp_b = 0;
        int audit_a = 0, audit_b = 0;
        for (int rep = 0; rep < 2; ++rep) {
            ScenarioBattle b(scenario::make_party(caster({kEMeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe(Gender::MALE)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int view = power_after_skill_effect(b, ctx);
            CHECK_EQ(view, 240, "T8239 威力视图两开全等（240）");
            if (rep == 0) {
                audit_a = stor(ctx, k129Audit + 0);
                hp_a = ctx.getPet(1).hp;
            } else {
                audit_b = stor(ctx, k129Audit + 0);
                hp_b = ctx.getPet(1).hp;
            }
        }
        CHECK_EQ(audit_b, audit_a, "T8239 审计倍率两开全等");
        CHECK_EQ(hp_b, hp_a, "T8239 对手 hp 两开全等");
        std::cout << "  [T8239] 129 确定性 通过\n";
    }

    std::cout << "scenario_8230_xingbie_family_gate PASS (J336 性别码表家族门)" << std::endl;
    return 0;
}
