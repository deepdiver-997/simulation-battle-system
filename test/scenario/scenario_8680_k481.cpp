// 场景 8680 —— K481 两族（2035/728，resources/moves_lib/k481_pp_drain_allstat_and_boost_roundend.cpp）
//（2026-11-18，worktree k481；号段 8680-8689 → T8680-pre/a-m 十四块）
//
// 断言口径（真实载体 + 审计槽 + 同种子对照三线并锁）：
//   - 2035 = plugin_storage 次数票（K461 1645 直系）+ EVENT_HIT watcher
//     （skills.cpp:1293/1302 HIT 终点 emit、ON_SKILL_HIT 桶末 drain——武装当击
//     结构性收不到 = "下{0}次"排除自身；属性技命中照发 → executed_skill_is_attack
//     攻击过滤）+ pp_zero_slot 原语全量消耗（常规 1 点消耗在 AFTER_ACTION——
//     回调时点读到的就是剩余全量）+ stat_drop 六槽阶梯（每 {1} 点一档，档数
//     = 消耗/{1} 整数除）。
//   - 728 = SKILL_EFFECT 施放时点提升门在位判读（K289 任一正槽直系；互补腿
//     763/1152 结构证据）+ ROUND_END 单发 tick + deal_pink_damage FIXED 固定档
//     （量 = max_hp/{0} 整数除，99999 → 1/5=19999、1/4=24999、1/3=33333）。
//     同种子对照局：提升门用命中槽（slot5，不进伤害公式+必中绕过精度）→
//     攻击伤害逐位一致 → HP 差 == 扣血量（精确等式）。
//   - rand 记账：全载体链零掷（28355：979 消不成/87 补 PP/1558+702 暴击窗休眠
//     crit0；28743：2189 100% 申报制短路；29349：191/1226 窗武装零抽）——攻击
//     只剩伤害浮动 1 抽，全部断言走 PP/等级/审计槽，不依赖伤害面。
#include <algorithm>
#include <any>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;          // 拍打：物40 无副作用（观测面/填充）
constexpr int kSuoTou = 20001;         // 缩头：属性技 [4] 自身防+1（"攻击命中"过滤面零 rand 填充）
constexpr int kZhiLi = 28355;          // 止戾·飒空龙心 [979,87,1558,702,2035]|[1,1,1] cat4 —— 2035 主载体
constexpr int kNieYuan = 28743;        // 孽冤悲哭 [191,2035,2189,843]|[2,1,1] cat4 —— {0}=2 载体
constexpr int kXuShi = 29349;          // 蓄势回击 [191,1226,2035]|[1,3,1] cat4 —— {1}=3 阶梯载体
constexpr int kFeiXu = 38114;          // 飞絮空澜没 [1234,1895,2035,1821]|[1,1,2] cat2 攻160 —— 攻击技载体
constexpr int kHeiYe = 17104;          // 黑夜灭魂决 [728,440]|[5] cat2 特160 —— 728 主载体
constexpr int kYingHun = 17291;        // 英魂圣裁决 [728,763]|[4] cat1 物160 —— 门时点甄别载体
constexpr int kShengZhang = 19214;     // 圣杖叱喝斩 [180,728]|[4] cat1 物160 —— {0}=4 载体
constexpr int kDuoPo = 31464;          // 夺魄魅魂击 [1152,728]|[3] cat1 物150 —— {0}=3 载体
constexpr int kMaxHp = 99999;

constexpr int k2035Base = 1020350000;
constexpr int k2035BridgeBase = 1020350100;   // 2035 降属桥子块（pending 排空账）
constexpr int k728Base = 1000728000;

int audit_int(BattleContext& ctx, int key, int def) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : def;
}

// 通用宠构造：技能表自选；全技能 crit0+acc100+must_hit（rand 记账前提：必中跳过
// 命中掷、crit0 短路跳过暴击掷、攻击只剩伤害浮动一抽）。
ElfPet skill_pet(int id, const char* name, const std::array<int, 5>& skill_ids, int speed,
                 int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(skill_ids[0]), Skills(skill_ids[1]), Skills(skill_ids[2]),
        Skills(skill_ids[3]), Skills(skill_ids[4]),
    };
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

// 5 槽 PP 设成自选值（消耗读面从 pp 状态直读）。
void set_pp(ScenarioBattle& b, int side, const std::array<int, 5>& v) {
    for (int i = 0; i < 5; ++i) {
        b.ctx().getPet(side).skills[static_cast<std::size_t>(i)].pp =
            v[static_cast<std::size_t>(i)];
    }
}

// 提升态注入（场景 setup 惯例，scenario_001/002/003/024 直系）：开战后写
// ability_levels（开战清零、回合流转不清、换宠即清）。
void set_boost(ScenarioBattle& b, int side, int slot, int level) {
    b.ctx().ability_levels[side][slot] = level;
}

// 死切补位：第一个存活且非当前在场槽（K470/K474 同款）。
int next_replacement_slot(ScenarioBattle& b, int side) {
    for (int s = 0; s < 6; ++s) {
        if (s != b.ctx().on_stage[side]
            && b.ctx().seerRobot[side].elfPets[s].hp > 0) {
            return s;
        }
    }
    return -1;
}

// 一回合：双方提交操作（死切自动补位）。
void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET,
                           next_replacement_slot(b, dead));
        b.drive();
    }
}

// 六槽等级全断（2035"全属性"面）。
void check_all_slots(ScenarioBattle& b, int side, int want, const std::string& tag) {
    for (int s = 0; s < 6; ++s) {
        CHECK_EQ(b.ctx().ability_levels[side][s], want,
                 tag + " 槽 " + std::to_string(s));
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8680-pre 10 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 2035 五载体（{0}/{1}/{2} 差分面）──
                {28355, {{979, {2, 3}}, {87, {}}, {1558, {2, 100}}, {702, {2}},
                         {2035, {1, 1, 1}}}},
                {28501, {{1389, {3, 100, 0}}, {1852, {1}}, {2035, {1, 1, 1}}}},
                {28743, {{191, {4}}, {2035, {2, 1, 1}}, {2189, {100, 23}}, {843, {2, 2}}}},
                {29349, {{191, {4}}, {1226, {3, 300, 300}}, {2035, {1, 3, 1}}}},
                {38114, {{1234, {1}}, {1895, {100, 100, 15}}, {2035, {1, 1, 1}},
                         {1821, {2}}}},
                // ── 728 五载体（{0}∈{5,4,4,4,3}）──
                {17104, {{728, {5}}, {440, {3, 2}}}},
                {17291, {{728, {4}}, {763, {1, 0, 0, 0, 1, 0}}}},
                {19214, {{180, {}}, {728, {4}}}},
                {31324, {{777, {1}}, {728, {4}}}},
                {31464, {{1152, {50}}, {728, {3}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8680-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8680-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8680-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8680-pre] 10 载体实际加载器切片核验 通过\n";
    }

    // ══ T8680.a 2035 正测 {0}=1,{1}=1,{2}=1（28355 属性技载体；seed 11）══
    // R1 属性技命中（EVENT_HIT 照发但非攻击+武装在 SKILL_EFFECT 桶=结构性收不到）
    // → 票 1 未消费；R2 拍打（pp=4）命中 → 全量消耗 4、六槽各 -4、SUCCESS 掩码
    // 0x3F；票耗尽自拆 → R3 拍打（pp=5）仅常规 1 点消耗。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kZhiLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(11);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 0, -1), 1, "T8680.a {0} 回显=1");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 3, -1), 1, "T8680.a R1 剩余票=1（属性技命中不消费）");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 4, 0), 0, "T8680.a R1 累计消费=0（缺键=0）");
        set_pp(b, 0, {9, 4, 9, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 5, -1), 4, "T8680.a 消耗=全量 4（常规消耗前读面）");
        CHECK_EQ(ctx.getPet(0).skills[1].pp, 0, "T8680.a 拍打 PP 归零");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 9, -1), 1, "T8680.a 消耗槽号=1（roundChoice 口径）");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 3, -1), 0, "T8680.a 票耗尽");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 4, -1), 1, "T8680.a 累计消费=1");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 6, -1), 4, "T8680.a 降属总档=4（消耗4/{1}1×{2}1）");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 10, -1), 0x3f, "T8680.a SUCCESS 掩码=六槽");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 11, -1), 0, "T8680.a IMMUNE 掩码=0");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 12, -1), 0, "T8680.a AT_FLOOR 掩码=0");
        check_all_slots(b, 1, -4, "T8680.a 对手全属性");
        // R3：票已耗尽自拆 → 无消费、仅常规消耗（M-once/生命周期杀点面）。
        set_pp(b, 0, {9, 5, 9, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).skills[1].pp, 4, "T8680.a R3 拍打仅常规 -1（watcher 已自拆）");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 4, -1), 1, "T8680.a R3 累计消费停 1");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 14, -1), 0, "T8680.a watcher id 已清（自拆）");
        std::cout << "  [T8680.a] 2035 正测 {0}=1 全量消耗+六槽-4+耗尽自拆 通过\n";
    }

    // ══ T8680.b 2035 阶梯档差分 {1}=3（29349；seed 12）══
    // 消耗 7 → steps=7/3=2（余 1 不足档不降）→ 总档 2 → 六槽 -2（不是 -7 也不是 -1）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kXuShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(12);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 1, -1), 3, "T8680.b {1} 回显=3（真实载体 29349）");
        set_pp(b, 0, {9, 7, 9, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 5, -1), 7, "T8680.b 消耗=7");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 6, -1), 2, "T8680.b 阶梯档=7/3 整数除=2");
        check_all_slots(b, 1, -2, "T8680.b 对手全属性");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 10, -1), 0x3f, "T8680.b SUCCESS 掩码=六槽");
        std::cout << "  [T8680.b] 2035 阶梯档差分 {1}=3（消耗7→-2） 通过\n";
    }

    // ══ T8680.c 2035 {0}=2 逐次消耗（28743；seed 13）══
    // R1 票=2（2189 诅咒 100% 申报制短路零抽；191/843 窗武装零抽）→ R2 攻击消费
    // 一票 → R3 缩头（属性技命中——"攻击命中"过滤面：不消费、降属不动）→ R4 攻击
    // 消费第二票（降属累计 -3 → -5）→ R5 无票仅常规消耗。镜像独立分账随 T8680.e。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kNieYuan, kPaiDa, kSuoTou, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 3, -1), 2, "T8680.c R1 剩余票=2（{0}=2）");
        set_pp(b, 0, {9, 3, 9, 2, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 5, -1), 3, "T8680.c R2 消耗=3");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 3, -1), 1, "T8680.c R2 剩票=1");
        check_all_slots(b, 1, -3, "T8680.c R2 对手全属性");
        // R3 属性技命中（缩头：EVENT_HIT 照发但非攻击技）→ 票不掉、无消耗。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 3, -1), 1, "T8680.c R3 属性技命中票不掉");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 4, 0), 1, "T8680.c R3 累计消费停 1（非攻击不消费）");
        check_all_slots(b, 1, -3, "T8680.c R3 对手全属性不动");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 3, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 5, -1), 2, "T8680.c R4 消耗=2（换槽照消费）");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 9, -1), 3, "T8680.c R4 槽号=3");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 3, -1), 0, "T8680.c R4 票耗尽");
        check_all_slots(b, 1, -5, "T8680.c R4 对手全属性（-3 再 -2 累计）");
        set_pp(b, 0, {9, 4, 9, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).skills[1].pp, 3, "T8680.c R5 无票仅常规 -1");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 4, -1), 2, "T8680.c R5 累计消费停 2");
        std::cout << "  [T8680.c] 2035 {0}=2 逐次消耗+属性技不消费+累计降属 通过\n";
    }

    // ══ T8680.d 2035 攻击技载体自身排除（38114 {0}=1,{1}=1,{2}=1；seed 14）══
    // 武装当击=自身攻击：EVENT_HIT 在 ON_SKILL_HIT 桶末 drain、武装在 SKILL_EFFECT
    // 桶（更晚）→ 结构性收不到 → R1 票 1 未消费、槽 0 PP 未清（1821 +2 补回面
    // 不进断言，只锁审计）；R2 拍打（pp=2）消费 2 → 总档 2（{2}=1）→ 六槽 -2。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kFeiXu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(14);
        BattleContext& ctx = b.ctx();
        set_pp(b, 0, {2, 9, 9, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 3, -1), 1, "T8680.d R1 武装后票=1（自身攻击未消费）");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 4, 0), 0, "T8680.d R1 累计消费=0（缺键=0）");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 5, 0), 0, "T8680.d R1 消耗量=0（自身 PP 未清）");
        set_pp(b, 0, {9, 2, 9, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 5, -1), 2, "T8680.d R2 消耗=2");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 6, -1), 2, "T8680.d 总档=2（{2}=1）");
        check_all_slots(b, 1, -2, "T8680.d 对手全属性");
        std::cout << "  [T8680.d] 2035 攻击技载体自身排除 通过\n";
    }

    // ══ T8680.d2 2035 {2}=2 变档腿（28355+4401 EffectParamOverride；seed 14 复用）══
    // {2} 无真实差分载体（五载体全=1）→ 装配期官方覆盖基建变档（K480 {0}=5 腿同款）：
    // 消耗 2 → 总档 4 → 六槽 -4（行参消费面 M-杀点）。
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const auto monster = repo.load_monster(4401);   // 破世之拳·特罗斯（28355 学习者）
        if (!monster) {
            std::cerr << "override monster 4401 load failed" << std::endl;
            std::abort();
        }
        std::vector<EffectParamOverride> overrides;
        overrides.push_back({kZhiLi, /*effect_id=*/2035, /*arg_index=*/2, /*value=*/2});
        std::array<Skills, 5> skills{
            Skills(kZhiLi, *monster, overrides), Skills(kPaiDa), Skills(kPaiDa),
            Skills(kPaiDa), Skills(kPaiDa),
        };
        for (Skills& s : skills) {
            s.critical_strike_rate = 0.0f;
            s.accuracy = 100;
            s.must_hit = 1;
        }
        numerical_properties base{};
        base[NumericalPropertyIndex::PHYSICAL_ATTACK] = 1;
        base[NumericalPropertyIndex::SPECIAL_ATTACK] = 1;
        base[NumericalPropertyIndex::DEFENSE] = 100;
        base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
        base[NumericalPropertyIndex::SPEED] = 300;
        base[NumericalPropertyIndex::HP] = kMaxHp;
        ElfPet p1(1, "变档", {0, 0}, 0, Gender::NONE, SoulMark{}, CommonTrait{}, base,
                  kMaxHp, 0, 0, false, std::move(skills));
        ScenarioBattle b(make_party(p1),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(14);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 2, -1), 2, "T8680.d2 {2} 回显=2（override 生效）");
        set_pp(b, 0, {9, 2, 9, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 5, -1), 2, "T8680.d2 消耗=2");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 6, -1), 4, "T8680.d2 总档=2×{2}2=4");
        check_all_slots(b, 1, -4, "T8680.d2 对手全属性");
        std::cout << "  [T8680.d2] 2035 {2}=2 变档腿（override） 通过\n";
    }

    // ══ T8680.e 2035 镜像 per-owner 分账（双边 28355；seed 15）══
    // 双边各自武装+各自攻击各消费自己的票：side0 先手降 side1、side1 后手降 side0；
    // 审计键 owner×16 步长分账（M10 同型杀点面）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主0", {kZhiLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "印主1", {kZhiLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(15);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 3, -1), 1, "T8680.e side0 票=1");
        CHECK_EQ(audit_int(ctx, k2035Base + 1 * 16 + 3, -1), 1, "T8680.e side1 票=1");
        set_pp(b, 0, {9, 3, 9, 9, 9});
        set_pp(b, 1, {9, 3, 9, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 5, -1), 3, "T8680.e side0 消耗=3");
        CHECK_EQ(audit_int(ctx, k2035Base + 1 * 16 + 5, -1), 3, "T8680.e side1 消耗=3");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 9, -1), 1, "T8680.e side0 槽号=1");
        CHECK_EQ(audit_int(ctx, k2035Base + 1 * 16 + 9, -1), 1, "T8680.e side1 槽号=1");
        check_all_slots(b, 0, -3, "T8680.e side0 被降");
        check_all_slots(b, 1, -3, "T8680.e side1 被降");
        std::cout << "  [T8680.e] 2035 镜像 per-owner 分账 通过\n";
    }

    // ══ T8680.f 2035 miss 不消费票（28355；seed 16）══
    // R2 拍打改 acc0+非必中 → miss（无 EVENT_HIT）→ 票不掉、PP 仅常规 -1；
    // R3 换槽命中 → 票仍在照消费（票随命中不随出招）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kZhiLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(16);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 3, -1), 1, "T8680.f 票=1");
        ctx.getPet(0).skills[2].accuracy = 0;    // 必 miss（无必中）
        ctx.getPet(0).skills[2].must_hit = 0;
        set_pp(b, 0, {9, 9, 6, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 3, -1), 1, "T8680.f miss 票不掉");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 4, 0), 0, "T8680.f miss 累计消费=0（缺键=0）");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 5, 0), 0, "T8680.f miss 消耗量=0");
        CHECK_EQ(ctx.getPet(0).skills[2].pp, 5, "T8680.f miss 常规 PP -1（miss 也耗 PP）");
        set_pp(b, 0, {9, 4, 9, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 5, -1), 4, "T8680.f R3 命中消费=4（票存活）");
        check_all_slots(b, 1, -4, "T8680.f R3 对手全属性");
        std::cout << "  [T8680.f] 2035 miss 不消费+票跨回合存活 通过\n";
    }

    // ══ T8680.g 2035 击杀回合：降属延后至行动末、目标已倒不降（28355 大攻；seed 17）══
    // 消费在命中时点照走（成本面），降属桥在本行动 AFTER_ACTION——彼时目标已被
    // 本击击杀（死亡裁决在 SecondMoverDeath/换宠补位，晚于 AFTER_ACTION）→
    // 生存门排空不降（掩码 0、桥 pending 清零）；换宠后本体等级回 0。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kZhiLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 500000, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(17);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        set_pp(b, 0, {9, 4, 9, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 4, -1), 1, "T8680.g 击杀回合消费=1");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 6, -1), 4, "T8680.g 总档=4（命中时点已算）");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 10, 0), 0, "T8680.g SUCCESS 掩码=0（目标已倒不降）");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 11, 0), 0, "T8680.g IMMUNE 掩码=0");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 12, 0), 0, "T8680.g AT_FLOOR 掩码=0");
        CHECK_EQ(audit_int(ctx, k2035BridgeBase, 0), 0, "T8680.g 桥 pending 已排空");
        CHECK(ctx.seerRobot[1].elfPets[0].hp <= 0, "T8680.g 前置：对手原在场宠已被击杀（死后换宠补位）");
        check_all_slots(b, 1, 0, "T8680.g 换宠后 fresh 等级 0");
        std::cout << "  [T8680.g] 2035 击杀回合降属不施加（桥生存门） 通过\n";
    }

    // ══ T8680.h 728 正测 {0}=5 精确量（17104；同种子对照局）══
    // 提升门用命中槽 slot5=1（不进伤害公式+必中绕过精度 → 对照局伤害逐位一致）
    // → HP 差 == 19999（99999/5 整数除）。
    int control_loss = -1;
    int boosted_loss = -1;
    for (int rep = 0; rep < 2; ++rep) {
        ScenarioBattle b(make_party(skill_pet(1, rep == 0 ? "提升" : "对照",
                                              {kHeiYe, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(21);
        if (rep == 0) {
            set_boost(b, 0, 5, 1);   // 命中槽提升：门开、伤害面零扰动
        }
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int loss = hp0 - ctx.getPet(1).hp;
        if (rep == 0) {
            CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 0, -1), 5, "T8680.h {0} 回显=5");
            CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 1, -1), 1, "T8680.h 提升门开（命中槽算提升）");
            CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 6, -1), 1, "T8680.h 武装计数=1");
            CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 5, -1), 1, "T8680.h tick 到达=1");
            CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 3, -1), 19999, "T8680.h 计算量=99999/5");
            CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 4, -1), 19999, "T8680.h 实际扣血=19999（固定档落地）");
            CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 7, -1), 1, "T8680.h 落地次数=1");
        } else {
            CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 1, -1), 0, "T8680.h 对照局门闭");
            CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 5, 0), 0, "T8680.h 对照局零 tick");
            control_loss = loss;
        }
        if (rep == 0) {
            boosted_loss = loss;
        }
    }
    CHECK_EQ(boosted_loss - control_loss, 19999,
             "T8680.h 同种子 HP 差 == 扣血量（攻击伤害逐位一致）");
    std::cout << "  [T8680.h] 728 正测 {0}=5 同种子对照 精确 19999 通过\n";

    // ══ T8680.i 728 门负测：无提升 / 仅负等级（17104；seed 22）══
    // 无提升 → 门闭零 tick；纯负等级（-2）也不算"能力提升状态"（任一 >0 判据）。
    for (int rep = 0; rep < 2; ++rep) {
        ScenarioBattle b(make_party(skill_pet(1, "负测", {kHeiYe, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(22);
        if (rep == 1) {
            set_boost(b, 0, 0, -2);   // 纯负等级：不是提升态
        }
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 1, -1), 0,
                 "T8680.i 局" + std::to_string(rep) + " 门闭");
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 5, 0), 0,
                 "T8680.i 局" + std::to_string(rep) + " 零 tick");
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 6, 0), 0,
                 "T8680.i 局" + std::to_string(rep) + " 零武装");
        CHECK_EQ(hp0 - ctx.getPet(1).hp, control_loss,
                 "T8680.i 局" + std::to_string(rep) + " 掉血=纯攻击伤害（同 seed 对照）");
    }
    std::cout << "  [T8680.i] 728 门负测（无提升/纯负等级） 通过\n";

    // ══ T8680.j 728 门时点甄别（17291 [728,763]；seed 23）══
    // 施放时无提升 → 728 门闭 + 763 自升（官方序 row[1,0,0,0,1,0]=攻+1 速+1 →
    // 引擎槽 0/4）→ 回合末自身**已处于提升态**但零 tick——门若改回合末判则必开
    // （互补腿结构证据，M-杀点面）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kYingHun, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(23);
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 1, "T8680.j 763 攻+1（互补腿已开火）");
        CHECK_EQ(ctx.ability_levels[0][4], 1, "T8680.j 763 速+1");
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 1, -1), 0, "T8680.j 728 门闭（施放时点判读）");
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 5, 0), 0, "T8680.j 零 tick（回合末有提升不补触发）");
        CHECK_EQ(hp0 - ctx.getPet(1).hp, control_loss,
                 "T8680.j 掉血=纯攻击伤害（同 seed 对照——无 1/4 抽血）");
        std::cout << "  [T8680.j] 728 门时点甄别（施放快照、回合末不补触发） 通过\n";
    }

    // ══ T8680.k 728 {0} 差分：1/3 与 1/4（31464/19214；seed 24/25）══
    {
        ScenarioBattle b(make_party(skill_pet(1, "三分之一", {kDuoPo, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(24);
        set_boost(b, 0, 5, 1);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 3, -1), 33333, "T8680.k {0}=3 计算量=33333");
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 4, -1), 33333, "T8680.k {0}=3 实际扣血");
        std::cout << "  [T8680.k1] 728 {0}=3 精确 33333 通过\n";
    }
    {
        ScenarioBattle b(make_party(skill_pet(1, "四分之一", {kShengZhang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(25);
        set_boost(b, 0, 5, 1);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 3, -1), 24999, "T8680.k {0}=4 计算量=24999（整数除）");
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 4, -1), 24999, "T8680.k {0}=4 实际扣血");
        std::cout << "  [T8680.k2] 728 {0}=4 精确 24999 通过\n";
    }

    // ══ T8680.l 728 单发无残留（17104；seed 26）══
    // R1 提升攻击 → R1 末扣一次；R2 平打（无新施放）→ tick 停 1、无二次扣血。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kHeiYe, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(26);
        set_boost(b, 0, 5, 1);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 5, -1), 1, "T8680.l R1 tick=1");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 5, -1), 1, "T8680.l R2 tick 停 1（单发窗过期）");
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 7, -1), 1, "T8680.l 落地次数停 1");
        std::cout << "  [T8680.l] 728 单发无残留 通过\n";
    }

    // ══ T8680.m 728 镜像 per-owner 分账（双边 17104 双提升；seed 27）══
    // ROUND_END 桶双边条目各自跑（后手侧先执行序不影响 per-owner 断言面）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主0", {kHeiYe, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "印主1", {kHeiYe, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(27);
        set_boost(b, 0, 5, 1);
        set_boost(b, 1, 5, 1);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 1, -1), 1, "T8680.m side0 门开");
        CHECK_EQ(audit_int(ctx, k728Base + 1 * 16 + 1, -1), 1, "T8680.m side1 门开");
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 4, -1), 19999, "T8680.m side0 扣血=19999");
        CHECK_EQ(audit_int(ctx, k728Base + 1 * 16 + 4, -1), 19999, "T8680.m side1 扣血=19999");
        CHECK_EQ(audit_int(ctx, k728Base + 0 * 16 + 5, -1), 1, "T8680.m side0 tick=1");
        CHECK_EQ(audit_int(ctx, k728Base + 1 * 16 + 5, -1), 1, "T8680.m side1 tick=1");
        std::cout << "  [T8680.m] 728 镜像 per-owner 分账 通过\n";
    }

    // ══ T8680.n 2035 离场清理（28355；seed 28）══
    // R1 武装 → R2 印主换宠（ON_STAGE epoch 作废 watcher）→ R3 新宠攻击不消费。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kZhiLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(28);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 3, -1), 1, "T8680.n 票=1（已武装）");
        const int wid = audit_int(ctx, k2035Base + 0 * 16 + 14, -1);
        CHECK(wid > 0, "T8680.n watcher 已注册");
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        set_pp(b, 0, {9, 6, 9, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).skills[1].pp, 5, "T8680.n 换宠后攻击仅常规 -1（离场作废）");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 4, 0), 0, "T8680.n 累计消费=0（缺键=0）");
        CHECK_EQ(audit_int(ctx, k2035Base + 0 * 16 + 5, 0), 0, "T8680.n 消耗量=0");
        std::cout << "  [T8680.n] 2035 离场清理（换宠 epoch 作废） 通过\n";
    }

    std::cout << "SBS_SCENARIO_K481 ALL PASS" << std::endl;
    return 0;
}
