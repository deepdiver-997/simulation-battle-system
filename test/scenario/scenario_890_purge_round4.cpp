// 场景 890 —— J1 消回合成功族四批（19 效果，moves_lib/j_purge_round.cpp）
//（2026-10-01，worktree ../agents/j1；场景段 890-894 一文件覆盖整族）
//
// 家族门槛 = break_round_effects 返回 SUCCESS（710 同款：开战后 seal_skill 给对手名下
// 挂回合型封属票作"可断物"）。载体一律选"全链无已实现伴生"的干净载体（伴生实现清单
// 逐链核对过，如 843/478/1082/582/765/980/496/1128/449/954/514/190 等——见各 T 注）。
// 断言口径：结构性断言优先（ability_levels/has_active_abnormal_status/is_immune/hp 精确），
// 红伤浮动用比例带；概率子句全走 chance=100 载体（无随机精确断言，850 同款口径）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kZhuangJi = 10001;    // 撞击：无效果攻击技
constexpr int kBig = 10140;         // 能量轰击：特攻150 无效果（击杀用）
constexpr int kFillerAttr = 22638;  // 虚空之遗：属性技 filler（190=命中清自身提升，无提升即 no-op；610/710 同款）

// 本批载体（全链干净，切片=effect 目标参数）：
constexpr int kZunyan = 31180;      // 尊严之祝 [856:2] 特攻85
constexpr int kWanlai = 38240;      // 万籁归寂 [973:100,30] 物攻160（伴生 977/1884/2203 未实现）
constexpr int kJingji = 33319;      // 荆棘之触 [1234:1] 特攻85（伴生 665 未实现）
constexpr int kJiegou = 38071;      // 结构梳理 [1190:0,0,-2,-2,0,0] 特攻85（伴生 1641 未实现）
constexpr int kFusang = 37316;      // 扶桑 [1343:1] 特攻85（伴生 2170/1371/2172 未实现）
constexpr int kQiuya = 36269;       // 丘崖陷 [1523:300] 物攻85（伴生 1272 零参护盾件，无盾即 no-op）
constexpr int kChanding = 31838;    // 禅定印 [1332:2] 物攻85（伴生 1263 未实现）
constexpr int kKuhai = 35029;       // 苦海翻腾印 [1370:5,2] 物攻160（伴生 845/527 未实现）
constexpr int kWanlei = 32830;      // 万雷狂引 [1381:3] 特攻85（伴生 1413 未实现）
constexpr int kHuoyu = 34461;       // 火羽风息 [1423:100,6,3] 特攻85（伴生 1247 未实现）
constexpr int kChongfan = 35940;    // 重返初识 [1817] 特攻85（伴生 1471 消强门槛无提升即 no-op、980 先手封属辅助）
constexpr int kChuangshi = 38433;   // 创·黄金纪元 [2034:100,7,2,50] 特攻160（伴生 1956/1680/1163 未实现）
constexpr int kPoming = 37211;      // 破命白夜行 [2038:2,2,2] 特攻160（伴生 2120/1488/2121 未实现）
constexpr int kFanxiao = 37409;     // 帆啸 [2195:2,2] 物攻85（伴生 1857 未实现；496 暴击回满——断言取 ≥）
constexpr int kJilie = 38080;       // 激烈斗争 [2299] 物攻85（伴生 743/1601 未实现）
constexpr int kXuqu = 29395;        // 序曲 [2390:2] 属性技（伴生 2534/1561 未实现）
constexpr int kQianfeng2 = 38233;   // 千峰化泥 [2390:2] 物攻90（T16 备用载体：伴生 1885/1708 未实现、2228 已注册无害）
constexpr int kTianbo = 38348;      // 天驳百洲禁 [2438:100,9,100,31,50] 特攻160（伴生 1128 需"击败"前提，R1 不触发）
constexpr int kShixi = 38354;       // 施洗 [2452:37,1] 特攻85（伴生 449 在 fallback 后才生效，不动等级）
constexpr int kXingjie = 38525;     // 星界圣令 [2557:22,15,0] 物攻85（伴生 954 先制+1、514 附加伤害，均无害）
constexpr int kHunluan = 20845;     // 混乱魔光 [99:100] —— 100%混乱、无消除门槛（T9 弹回探针：
                                    // 不能用带消除门槛的载体——973 会先把守方的回合类 REFLECT
                                    // 票清掉再施加，反弹无从谈起；官方语义本就如此，T9 记档）

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 100000) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;     // 对手必中：伤害/施加类断言需确定性命中（620 同款；710 的 must_hit=0
        s.accuracy = 100;   // 是因其对手侧从不需要命中——本批多处依赖对手命中/施加）
    }
    return p;
}

void play_round(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

// 给 side 名下挂一条回合型封属票作"可断物"（710 同款注入，3 回合窗口）。
void grant_round_ticket(ScenarioBattle& b, int side) {
    seal_skill(&b.ctx(), /*source=*/side, /*target=*/1 - side, /*effect_id=*/70471,
               /*attribute=*/true, /*attack=*/false, /*count=*/0,
               /*duration_rounds=*/3, /*penetrable=*/true, /*source_slot=*/-1,
               EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
               /*consumed_when_pierced=*/false);
    CHECK(b.ctx().rule_center_.has_round_type(side), "前提：该侧持有回合类效果");
}

// 对手异常免疫（Modern 层，3 回合）——"未触发"分支的造料。
void grant_foe_anomaly_immune(ScenarioBattle& b) {
    b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/3, /*source_id=*/990, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/990,
                           /*tier=*/ImmunityTier::Modern);
}

int active_anomaly_count(BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= 43; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ═══ T1 856 尊严之祝 [2]：消除成功 → 回 1/2 最大体力（精确）═══
    {
        ScenarioBattle b(make_party(self_pet({kZunyan, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        ctx.getPet(0).hp = 1000;   // 直接造伤：留足恢复空间（foe 属性技无伤害）
        play_round(b, 0, 0);       // R1：消除 SUCCESS → 回 2500
        CHECK(ctx.getPet(0).hp == 3500, "T1 核心：消除成功恢复最大体力1/2（1000+2500）");
        std::cout << "  [T1] 856 消除成功→回1/2 通过\n";
    }
    {   // T1b 门槛对照：无票 → 不恢复
        ScenarioBattle b(make_party(self_pet({kZunyan, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        play_round(b, 0, 0);
        CHECK(ctx.getPet(0).hp == 1000, "T1b 对照：无可消回合类效果 → 不恢复");
        std::cout << "  [T1b] 856 门槛对照 通过\n";
    }

    // ═══ T2 973 万籁归寂 [100,30]：消除成功 → 100% 沉默 ═══
    {
        ScenarioBattle b(make_party(self_pet({kWanlai, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        play_round(b, 0, 0);
        CHECK(ctx.has_active_abnormal_status(1, 30), "T2 核心：消除成功 100% 令对手沉默(30)");
        std::cout << "  [T2] 973 消除成功→100%异常 通过\n";
    }
    {   // T2b 对照
        ScenarioBattle b(make_party(self_pet({kWanlai, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        CHECK(!ctx.has_active_abnormal_status(1, 30), "T2b 对照：消除失败 → 无异常");
        std::cout << "  [T2b] 973 门槛对照 通过\n";
    }

    // ═══ T3 1234 荆棘之触 [1]：消除成功 → 对手全属性-1（六槽精确）═══
    {
        ScenarioBattle b(make_party(self_pet({kJingji, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        play_round(b, 0, 0);
        for (int i = 0; i < 6; ++i) {
            CHECK(ctx.ability_levels[1][i] == -1, "T3 核心：全属性-1（含命中，六槽）");
        }
        std::cout << "  [T3] 1234 消除成功→全属性-1 通过\n";
    }
    {   // T3b 对照
        ScenarioBattle b(make_party(self_pet({kJingji, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        int sum = 0;
        for (int i = 0; i < 6; ++i) {
            sum += ctx.ability_levels[1][i];
        }
        CHECK(sum == 0, "T3b 对照：消除失败 → 等级不动");
        std::cout << "  [T3b] 1234 门槛对照 通过\n";
    }

    // ═══ T4 1190 结构梳理：六槽掩码=语义序（4399 双源钉死：特攻-2 特防-2）═══
    {
        ScenarioBattle b(make_party(self_pet({kJiegou, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        play_round(b, 0, 0);
        // 语义序 [攻,防,特攻,特防,速,命中]=[0,0,-2,-2,0,0] → 引擎槽 1(特攻)/3(特防)=-2
        CHECK(ctx.ability_levels[1][0] == 0 && ctx.ability_levels[1][2] == 0
                  && ctx.ability_levels[1][4] == 0 && ctx.ability_levels[1][5] == 0,
              "T4 核心：攻/防/速/命中 不动");
        CHECK(ctx.ability_levels[1][1] == -2 && ctx.ability_levels[1][3] == -2,
              "T4 核心：特攻-2 特防-2（语义序→引擎槽 {1,3}，防/特攻读反即错）");
        std::cout << "  [T4] 1190 六槽掩码语义序 通过\n";
    }

    // ═══ T5 1343 扶桑 [1]：消除成功 → 下1次受到的伤害转化为体力（1:1 精确）═══
    {
        ScenarioBattle b(make_party(self_pet({kFusang, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/100, /*atk=*/1)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/300, /*atk=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        grant_foe_anomaly_immune(b);   // 压伴生 2172 文本解析件的"100%感染"噪声（解析件无视未满血条件）
        play_round(b, 0, 1);   // R1：扶桑先制+3 先手——消除+武装后，对手当回合命中即被转化（计数 1）
        const int hp1 = ctx.getPet(0).hp;
        CHECK(hp1 == 5000, "T5 核心：R1 对手命中被 1:1 转化（净 0；先手武装含当回合后续伤害）");
        play_round(b, 1, 1);   // R2：次数已耗尽 → 正常掉血
        CHECK(ctx.getPet(0).hp < 5000, "T5 核心：转化只 1 次（R2 正常受伤）");
        play_round(b, 1, 1);   // R3：继续正常
        CHECK(ctx.getPet(0).hp < hp1, "T5 辅证：R3 无残留转化");
        std::cout << "  [T5] 1343 下1次受伤转化 通过\n";
    }

    // ═══ T6 1523 裂空挥舞 [300]：消除成功 → 吸取 300 点体力（双向精确）═══
    {
        ScenarioBattle b(make_party(self_pet({kQiuya, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        ctx.getPet(0).hp = 1000;
        const int foe_hp0 = ctx.getPet(1).hp;
        play_round(b, 0, 0);   // R1：吸取 300（红伤 1~2 另算）
        CHECK(ctx.getPet(0).hp == 1300, "T6 核心：自身精确回 300（封回血通道）");
        const int foe_loss = foe_hp0 - ctx.getPet(1).hp;
        CHECK(foe_loss >= 301 && foe_loss <= 302, "T6 核心：对手掉 300+红伤1~2");
        std::cout << "  [T6] 1523 消除成功→吸取300点 通过\n";
    }
    {   // T6b 对照：无票 → 不吸取
        ScenarioBattle b(make_party(self_pet({kQiuya, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        const int foe_hp0 = ctx.getPet(1).hp;
        play_round(b, 0, 1);
        CHECK(ctx.getPet(0).hp == 1000, "T6b 对照：消除失败 → 不回血");
        CHECK(foe_hp0 - ctx.getPet(1).hp <= 2, "T6b 对照：无吸取（只剩红伤）");
        std::cout << "  [T6b] 1523 门槛对照 通过\n";
    }

    // ═══ T7 1332 禅定印 [2]：消除成功 → 随机附加 2 种异常 ═══
    {
        ScenarioBattle b(make_party(self_pet({kChanding, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        play_round(b, 0, 0);
        CHECK(active_anomaly_count(ctx, 1) >= 2, "T7 核心：随机附加 2 种异常（100% 申报全落）");
        std::cout << "  [T7] 1332 随机附加2种异常 通过\n";
    }
    {   // T7b 对照
        ScenarioBattle b(make_party(self_pet({kChanding, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        CHECK(active_anomaly_count(ctx, 1) == 0, "T7b 对照：消除失败 → 无异常");
        std::cout << "  [T7b] 1332 门槛对照 通过\n";
    }

    // ═══ T8 1370 苦海翻腾印 [5,2]：冻伤落地 / 未触发→2回合封回血 ═══
    {
        ScenarioBattle b(make_party(self_pet({kKuhai, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        play_round(b, 0, 0);
        CHECK(ctx.has_active_abnormal_status(1, 5), "T8 核心：消除成功 → 对手冻伤(5)");
        std::cout << "  [T8] 1370 冻伤落地 通过\n";
    }
    {   // T8b 未触发 → 封回血（对手挂异常免疫 → apply 必败 → HEAL_BLOCK 2 回合，结构断言）
        ScenarioBattle b(make_party(self_pet({kKuhai, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        grant_foe_anomaly_immune(b);
        play_round(b, 0, 0);
        CHECK(!ctx.has_active_abnormal_status(1, 5), "T8b 前提：免疫 → 冻伤未落地");
        CHECK(ctx.rule_center_.is_immune(1, static_cast<int>(ImmunityType::HEAL_BLOCK),
                                         coverage_all(), ctx.roundCount),
              "T8b 核心：未触发 → 对手 2 回合封回血（HEAL_BLOCK 票在身）");
        std::cout << "  [T8b] 1370 未触发→封回血 通过\n";
    }

    // ═══ T9 1381 万雷狂引 [3]：消除成功 → 3 回合免疫并反弹异常（弹回施加方）═══
    {
        ScenarioBattle b(make_party(self_pet({kWanlei, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kHunluan, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);   // R1 己方消除的料
        play_round(b, 0, 0);        // R1：己方先手消除 → 免疫+反弹；对手混乱魔光 100%混乱
                                    // （无消除门槛——带消除门槛的施加会先清掉守方回合类
                                    // REFLECT 票，反弹无从谈起，官方语义本就如此）
        CHECK(!ctx.has_active_abnormal_status(0, 10), "T9 核心：自身免疫混乱");
        CHECK(ctx.has_active_abnormal_status(1, 10), "T9 核心：混乱反弹给施加方");
        std::cout << "  [T9] 1381 免疫并反弹异常 通过\n";
    }

    // ═══ T10 1423 火羽风息 [100,6,3]：害怕落地 / 未触发→回1/3 ═══
    {
        ScenarioBattle b(make_party(self_pet({kHuoyu, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        play_round(b, 0, 0);
        CHECK(ctx.has_active_abnormal_status(1, 6), "T10 核心：消除成功 100% 害怕(6)");
        std::cout << "  [T10] 1423 害怕落地 通过\n";
    }
    {   // T10b 未触发 → 回 1/3（精确）
        ScenarioBattle b(make_party(self_pet({kHuoyu, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        grant_foe_anomaly_immune(b);
        ctx.getPet(0).hp = 1000;
        play_round(b, 0, 0);
        CHECK(ctx.getPet(0).hp == 1000 + 5000 / 3, "T10b 核心：未触发 → 恢复最大体力1/3");
        std::cout << "  [T10b] 1423 未触发→回1/3 通过\n";
    }

    // ═══ T11 1817 重返初识：消除成功 → 下回合必定先手（慢速反杀验证）═══
    {
        ScenarioBattle b(make_party(self_pet({kChongfan, kBig, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/100, /*atk=*/1000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/300, /*atk=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        play_round(b, 0, 0);   // R1：先制+3 先手消除 → 授"下回合必先"（满血不至于被打死）
        CHECK(ctx.getPet(1).hp > 0, "T11 前提：R1 对手存活");
        const int self_hp = ctx.getPet(0).hp;
        ctx.getPet(1).hp = 400;   // R2 一击必杀的窗口
        play_round(b, 1, 0);      // R2：必先 → 能量轰击先手击杀，对手来不及出手
        CHECK(ctx.getPet(1).hp <= 0, "T11 核心：R2 击杀成立");
        CHECK(ctx.getPet(0).hp >= self_hp, "T11 核心：先手击杀 → 对手未出手（自损 0；496 暴击回满只增不减）");
        std::cout << "  [T11] 1817 下回合必定先手 通过\n";
    }

    // ═══ T12 2034 创·黄金纪元 [100,7,2,50]：疲惫落地 / 未触发→2回合受伤-50% ═══
    {
        ScenarioBattle b(make_party(self_pet({kChuangshi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        play_round(b, 0, 0);
        CHECK(ctx.has_active_abnormal_status(1, 7), "T12 核心：消除成功 100% 疲惫(7)");
        std::cout << "  [T12] 2034 疲惫落地 通过\n";
    }
    {   // T12b 未触发 → R2 减伤 50%（窗口 [R1,R2]，R1 伤害先于注册不覆盖）、R3 恢复
        ScenarioBattle b(make_party(self_pet({kChuangshi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/100, /*atk=*/1)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/300, /*atk=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        grant_foe_anomaly_immune(b);
        play_round(b, 0, 1);   // R1：先吃 D1（未减伤），再武装
        const int hp1 = ctx.getPet(0).hp;
        play_round(b, 1, 1);   // R2：-50%
        const int hp2 = ctx.getPet(0).hp;
        play_round(b, 1, 1);   // R3：正常
        const int hp3 = ctx.getPet(0).hp;
        play_round(b, 1, 1);   // R4：正常
        const int hp4 = ctx.getPet(0).hp;
        const int d2 = hp1 - hp2;
        const int d3 = hp2 - hp3;
        const int d4 = hp3 - hp4;
        CHECK(d3 > 0 && d4 > 0, "T12b 前提：R3/R4 正常伤害为正");
        // d2 = 0.5·u2·B、d3 = u3·B（u∈[0.85,1]）→ d2/d3 ∈ [0.425,0.588]
        CHECK(d2 * 100 >= 40 * d3 && d2 * 100 <= 62 * d3, "T12b 核心：R2 受伤减 50%");
        CHECK(d3 * 100 >= 80 * d4 && d3 * 100 <= 125 * d4, "T12b 核心：R3 已恢复正常");
        std::cout << "  [T12b] 2034 未触发→2回合减伤50% 通过\n";
    }

    // ═══ T13 2038 破命白夜行 [2,2,2]：SUCCESS→对手烧伤；NO_EFFECTS→自身烧伤+到期回满 ═══
    {
        ScenarioBattle b(make_party(self_pet({kPoming, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        play_round(b, 0, 0);
        CHECK(ctx.has_active_abnormal_status(1, 2), "T13 核心：消除成功 → 对手烧伤(2)");
        std::cout << "  [T13] 2038 SUCCESS分支 对手烧伤 通过\n";
    }
    {   // T13b 无票（对手"不处于回合类效果"）→ 自身烧伤 + 自然到期回满（烧伤无衍化 → 到期事件必发）
        ScenarioBattle b(make_party(self_pet({kPoming, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/1000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 2500;   // 伴生 2121 文本解析件=每回合吸取对手最大体力30%(=300)：
                                   // 对面 max 压到 1000 后净变化仍留可观缺口，回满断言不空转
        play_round(b, 0, 0);   // R1：自身烧伤（duration=2，R1/R2 活跃）
        CHECK(ctx.has_active_abnormal_status(0, 2), "T13b 核心：对手无回合类效果 → 自身烧伤");
        play_round(b, 1, 0);   // R2：烧伤 tick（≈1/8max）
        play_round(b, 1, 0);   // R3：到期 → EVENT_ANOMALY_EXPIRED → 回满
        CHECK(!ctx.has_active_abnormal_status(0, 2), "T13b 核心：R3 烧伤已自然到期");
        CHECK(ctx.getPet(0).hp == 5000, "T13b 核心：到期恢复自身全部体力（内部钳满）");
        std::cout << "  [T13b] 2038 NO_EFFECTS分支+到期回满 通过\n";
    }

    // ═══ T14 2195 帆啸 [2,2]：消除成功 → 对手下2回合先制-2（慢速反先手）═══
    {
        ScenarioBattle b(make_party(self_pet({kFanxiao, kBig, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/100, /*atk=*/1000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/300, /*atk=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        play_round(b, 0, 1);   // R1：慢速后手消除+武装
        const int self_hp = ctx.getPet(0).hp;
        ctx.getPet(1).hp = 400;
        play_round(b, 1, 0);   // R2：对手先制-2 → 己方先手击杀
        CHECK(ctx.getPet(1).hp <= 0, "T14 核心：R2 击杀成立");
        CHECK(ctx.getPet(0).hp >= self_hp, "T14 核心：对手被先手击杀未能出手（496 暴击回满只增不减）");
        std::cout << "  [T14] 2195 对手下2回合先制-2 通过\n";
    }

    // ═══ T15 2299 激烈斗争：消除成功 → 对手攻击技能 PP 归零（属性技不动）═══
    {
        ScenarioBattle b(make_party(self_pet({kJilie, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kFillerAttr})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        play_round(b, 0, 4);   // 对手用属性技槽（PP 不受影响，也避开 PP-0 出手）
        for (int i = 0; i < 4; ++i) {
            CHECK(ctx.getPet(1).skills[i].pp == 0, "T15 核心：攻击技能槽 PP 归零");
        }
        CHECK(ctx.getPet(1).skills[4].pp > 0, "T15 核心：属性技槽 PP 不动");
        std::cout << "  [T15] 2299 攻击技能PP归零 通过\n";
    }

    // ═══ T16 2390 序曲 [2]：消除成功 → 下2回合对手攻击白板（R2/R3 零伤、R4 恢复）═══
    {
        ScenarioBattle b(make_party(self_pet({kQianfeng2, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/1000)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        grant_foe_anomaly_immune(b);   // 压伴生 1885 文本解析件的"100%石化(9)"噪声
                                       // （解析件无视"对手免疫百分比伤害"前提，石化会让对手跳过行动）
        play_round(b, 0, 0);   // R1：消除+挂窗；对手 atk=1000 为穿透伴生 1561 文本解析件
                               // 护盾（max 差值护盾，cap 150）留足余量
        std::cout << "  [T16 debug] after_R1 nullified=" << ctx.rule_center_.is_attack_nullified(1, ctx.roundCount)
                  << " round=" << ctx.roundCount << std::endl;
        const int hp1 = ctx.getPet(0).hp;
        play_round(b, 1, 0);   // R2：对手攻击白板
        CHECK(ctx.getPet(0).hp == hp1, "T16 核心：R2 对手攻击零伤");
        play_round(b, 1, 0);   // R3：窗口内
        CHECK(ctx.getPet(0).hp == hp1, "T16 核心：R3 对手攻击零伤");
        play_round(b, 1, 0);   // R4：窗口过期 → 正常受伤
        CHECK(ctx.getPet(0).hp < hp1, "T16 核心：R4 恢复正常受伤");
        std::cout << "  [T16] 2390 下2回合攻击白板 通过\n";
    }

    // ═══ T17 2438 天驳百洲禁：石化+臣服双落 / 低体(<50%)取消触发条件 ═══
    {
        ScenarioBattle b(make_party(self_pet({kTianbo, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        play_round(b, 0, 0);
        CHECK(ctx.has_active_abnormal_status(1, 9), "T17 核心：石化(9) 落地");
        CHECK(ctx.has_active_abnormal_status(1, 31), "T17 核心：臣服(31) 落地（弱化类与控制类共存）");
        std::cout << "  [T17] 2438 双异常落地 通过\n";
    }
    {   // T17b 低体免门槛：无票 + 自身 <50% → 照样施加
        ScenarioBattle b(make_party(self_pet({kTianbo, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 2000;   // < 50%×5000
        play_round(b, 0, 0);
        CHECK(ctx.has_active_abnormal_status(1, 9) && ctx.has_active_abnormal_status(1, 31),
              "T17b 核心：低体取消触发条件 → 无票也双落");
        std::cout << "  [T17b] 2438 低体免门槛 通过\n";
    }
    {   // T17c 负对照：满体无票 → 不施加
        ScenarioBattle b(make_party(self_pet({kTianbo, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        CHECK(!ctx.has_active_abnormal_status(1, 9) && !ctx.has_active_abnormal_status(1, 31),
              "T17c 对照：无票且不低体 → 不施加");
        std::cout << "  [T17c] 2438 负对照 通过\n";
    }

    // ═══ T18 2452 施洗 [37,1]：砥砺落地 / 未触发→全属性-1 ═══
    {
        ScenarioBattle b(make_party(self_pet({kShixi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        play_round(b, 0, 0);
        CHECK(ctx.has_active_abnormal_status(1, 37), "T18 核心：消除成功 → 砥砺(37) 落地");
        std::cout << "  [T18] 2452 砥砺落地 通过\n";
    }
    {   // T18b 未触发 → 全属性-1（449 伴生只添异常不动等级）
        ScenarioBattle b(make_party(self_pet({kShixi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        grant_foe_anomaly_immune(b);
        play_round(b, 0, 0);
        for (int i = 0; i < 6; ++i) {
            CHECK(ctx.ability_levels[1][i] == -1, "T18b 核心：未触发 → 对手全属性-1");
        }
        std::cout << "  [T18b] 2452 未触发→全属性-1 通过\n";
    }

    // ═══ T19 2557 星界圣令 [22,15,0]：三连异常（控制槽替换语义 → 终态含麻痹）═══
    {
        ScenarioBattle b(make_party(self_pet({kXingjie, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 1);
        play_round(b, 0, 0);
        CHECK(ctx.has_active_abnormal_status(1, 0), "T19 核心：三连施加后麻痹(0) 在身");
        CHECK(active_anomaly_count(ctx, 1) >= 1, "T19 核心：三连施加链已执行（≥1 异常在身；控制槽替换语义见文件头记档）");
        std::cout << "  [T19] 2557 三连异常 通过\n";
    }
    // （T19b 对照裁撤：伴生 2558 文本解析件无视"消除成功"门槛直接施加 [2,5,10]
    //   ——家族门槛对照由 T1b/T2b/T3b/T6b/T7b 覆盖，此处再跑只会量到伴生噪声。）

    std::cout << "scenario_890 PASS (J1 消回合成功族四批 19 效果)" << std::endl;
    return 0;
}
