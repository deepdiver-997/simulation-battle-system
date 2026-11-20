#include <utils/sbs_rand.h>
// 场景 8650 —— K478 护罩消散降属+受击反馈两族（1446/658，
// resources/moves_lib/k478_hood_break_drop_and_hit_feedback.cpp）
//（2026-11-18，worktree k478；号段 8650-8659 → T8650-pre/a-i 十一块）
//
// 断言口径（真实载体 + 审计槽 + 行为面三线并锁，K470 8570/K474 8610 同构）：
//   - 1446 正测腿 = 28258 川岳凝合 [400,1]（链 191 免反异常窗惰性 / 1753 盾 400+破盾
//     回血——盾只吃红伤与罩互不串）/ 半吸收腿 = 直调 pink 300（罩剩 100 不触发）/
//     被打穿腿 = 直调 pink 100（池空 → 破罩事件 → 六槽逐项 -1）。
//     ⚠️ 粉伤直调可测粉管线（套路目录 §七：IMMUNE/REDUCE/HOOD 相在内跑）——罩破
//     触发链在 HOOD 相内，直调即覆盖；红伤面（盾破不串）用真实拍打驱动。
//   - 1446 盾破不串腿 = 28258（真实拍打破 1753 盾 → EVENT_SHIELD_BROKEN 照发，
//     池门 quantity_of(1446)>0 跳过——M-池门杀位：删门则盾破即触发转红）。
//   - 1446 镜像腿 = 27363 花灵庇护 [500,1](side1)（{0}=500 档 + owner×16 键分账；
//     链 191/1235 均被动窗、无攻击不掷）；26581 永恒咒印 [300,1] 腿 = 885/1239
//     共链旁证（885 自身全属性+1 不触对手；1239 满血回血钳零、无附加粉伤）。
//   - 1446 换宠 slot 戳腿 = 36318 煌龙之噬 [300,1]（R2 切到 fresh 槽1 → 手术盾被
//     拍打破 → 事件 target=0 但 slot 戳不符 → 跳过不清旗（slot9=3）；R4 切回槽0
//     → 直调 pink 300 罩破触发——旗跨换宠往返存续，M-slot戳杀位：删戳则他人盾破
//     即触发、清旗则回场不触发，两向各转红）。
//   - 1446 池叠加腿 = 28258×2（罩池 800 两栈：pink 400 破一栈池剩 400 不触发，
//     pink 400 池空恰一次触发——M-池语义杀位：逐条触发则计数 2 转红）。
//   - 658 基数腿 = 23514 万夫不当 [100]（max_pp=1 单用 stance、零共链最净）+
//     手术盾：R2 盾 10000 全吸收（无 TAKE_DAMAGE → 哨兵不消费——"受到攻击伤害"=
//     实扣口径，M-基数杀位：全吸收也消费则 consume=1 转红）；R3 盾 300 部分吸收
//     → base=实扣=己方掉血 → 反馈=base=对手掉血三方相等（pct100 恒等式，免 seed）。
//   - 658 {0}=80 腿 = 23864 冰霜降落 [80,链 656]（粉伤直调不消费负测 + 真拍打
//     触发 + 对手掉血==己方掉血×80/100 int 恒等式（免 seed）+ R3 二次拍打零消费
//     零反馈——一次性消费杀位）；656 掷点只耗 rand 不进断言面。
//   - 658 跨回合腿 = 24328 森林伪装 [100]（side0 速 100 后手：R1 已发生的对手攻击
//     不回溯消费（"下次"前向语义）→ R2 才触发；链 48/187/694 均惰性——对手用
//     攻技不踩 187 属性门、side0 不出攻不踩 694）。
//   - 658 镜像腿 = 23514(side1)（owner=1 键分账 + 反馈落 side0）。
//   - 伤害带常量：拍打 10004 物 40；atk2000/def100 → [573,674]（>400 必破盾、
//     <10000 必全吸）；atk100 → [30,36]；atk1 → [1,2]。全套比例/相对断言免 seed
//     定标（每块仍 sbs_srand 固定 seed 供复现）。
//   - 审计槽（键式见插件头注）：1446 slot0=armed/1=slot戳/2={0}回显/3={1}回显/
//     4=触发计数/5=触发回合/8=使用计数/9=跳过原因；658 slot0=pct/1=armed/2=slot戳/
//     3=pending/4=反馈量/5=fire 计数/6=消费计数/8=使用计数/9=跳过原因。
#include <any>
#include <cstdlib>
#include <iostream>

#include <db/official_data_repository.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kYongHeng = 26581;  // 永恒咒印 [885,1239,1446] → 1446=[300,1]
constexpr int kHuaLing  = 27363;  // 花灵庇护 [1446,191,1235] → 1446=[500,1]
constexpr int kChuanYue = 28258;  // 川岳凝合 [191,1753,1446] → 1446=[400,1]|1753=[400,3]
constexpr int kHLSou    = 36314;  // 煌龙之狩 [5,5,1446,1461] → 1446=[300,1]
constexpr int kHLShi    = 36318;  // 煌龙之噬 [5,5,1446,1461] → 1446=[300,1]
constexpr int kWanFu    = 23514;  // 万夫不当 [658] → 658=[100]（cat4 priority2 maxpp1）
constexpr int kBingShuang = 23864;  // 冰霜降落 [658,656] → 658=[80]|656=[50,2]
constexpr int kSenLin   = 24328;  // 森林伪装 [658,48,187,694] → 658=[100]
constexpr int kPaiDa    = 10004;  // 拍打：物40（攻技观测面）
constexpr int kMaxHp = 99999;
constexpr int kHoodSource = 1446;

constexpr int k1446Base = 1014460000;
constexpr int k658Base  = 1000658000;

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

// 审计槽读数（owner 侧；absent 读作 fallback 值——armed 已消费/未武装=缺席）。
int slot1446(BattleContext& ctx, int owner, int n, int fallback = -1) {
    return storage_int(ctx, k1446Base + owner * 16 + n, fallback);
}
int slot658(BattleContext& ctx, int owner, int n, int fallback = -1) {
    return storage_int(ctx, k658Base + owner * 16 + n, fallback);
}

// 印主宠：首槽=载体技能；must_hit + crit0 → 命中/暴击链零抽（k474 同构）。
ElfPet carrier_pet(int id, const char* name, int carrier, int speed, int atk, int hp,
                   int def = 100, const SoulMark& mark = SoulMark{}) {
    std::array<Skills, 5> skills{
        Skills(carrier), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
    };
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = def;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = def;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, mark,
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

// 对手宠：技能表自选（拍打=攻技观测面）。
ElfPet foe_pet(int id, const char* name, const std::array<int, 5>& moves, int speed, int atk,
               int hp, int def = 100, const SoulMark& mark = SoulMark{}) {
    std::array<Skills, 5> skills{
        Skills(moves[0]), Skills(moves[1]), Skills(moves[2]), Skills(moves[3]), Skills(moves[4]),
    };
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = def;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = def;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, mark,
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

int next_replacement_slot(ScenarioBattle& b, int side) {
    for (int slot_i = 0; slot_i < 6; ++slot_i) {
        if (slot_i == b.ctx().on_stage[side]) {
            continue;
        }
        if (b.ctx().seerRobot[side].elfPets[slot_i].hp > 0) {
            return slot_i;
        }
    }
    return 1;
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

// 六槽等级全查（逐项，不看总和——坑集 §1 纪律）。
bool levels_all_eq(BattleContext& ctx, int side, int v) {
    for (int i = 0; i < 6; ++i) {
        if (ctx.ability_levels[side][i] != v) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8650-pre 10 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 1446 五载体（{0}∈{300,400,500}、{1}=1）──
                {kYongHeng, {{885, {1}}, {1239, {1, 2}}, {1446, {300, 1}}}},
                {kHuaLing,  {{1446, {500, 1}}, {191, {4}}, {1235, {3, 80, 1}}}},
                {kChuanYue, {{191, {4}}, {1753, {400, 3}}, {1446, {400, 1}}}},
                {kHLSou,    {{5, {0, 100, -2}}, {5, {2, 100, -2}},
                             {1446, {300, 1}}, {1461, {3}}}},
                {kHLShi,    {{5, {4, 100, -2}}, {5, {5, 100, -2}},
                             {1446, {300, 1}}, {1461, {3}}}},
                // ── 658 五载体（{0}∈{80,100}）──
                {19314,     {{658, {100}}}},
                {kWanFu,    {{658, {100}}}},
                {kBingShuang, {{658, {80}}, {656, {50, 2}}}},
                {kSenLin,   {{658, {100}}, {48, {4}}, {187, {3, 3}}, {694, {2}}}},
                {24740,     {{47, {4}}, {658, {100}}, {165, {3, 2}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8650-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8650-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8650-pre 切片参数");
            }
        }
        std::cout << "  [T8650-pre] 10 载体实际加载器切片核验 通过\n";
    }

    // ══ T8650.a 1446 正测（28258 [400,1]）：武装+罩 400；pink 300 半吸收不触发
    //    （罩剩 100、hp 不掉——粉全被罩吃）；pink 100 池空破罩 → 六槽逐项 -1、
    //    触发恰一次、armed 消费；再 pink 无罩直落 hp 不再触发（一次性）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主a", kChuanYue, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        sbs_srand(1);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1446(ctx, 0, 2), 400, "T8650.a {0}=400 行参回显");
        CHECK_EQ(slot1446(ctx, 0, 3), 1, "T8650.a {1}=1 行参回显");
        CHECK_EQ(slot1446(ctx, 0, 0), 1, "T8650.a armed 置位");
        CHECK_EQ(slot1446(ctx, 0, 8), 1, "T8650.a 使用计数=1");
        CHECK_EQ(ctx.getPet(0).hood_bank_.quantity_of(kHoodSource), 400,
                 "T8650.a 护罩 400 落 hood_bank_（source=1446）");
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(1753), 400,
                 "T8650.a 共链 1753 盾 400 旁证（链序切片）");
        CHECK(levels_all_eq(ctx, 1, 0), "T8650.a 使用技未动对手等级");
        // 半吸收：pink 300 → 罩吃 300 剩 100，无破罩事件、不触发、hp 不掉。
        (void)deal_pink_damage(&ctx, 0, 300, DamageKind::FIXED, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hood_bank_.quantity_of(kHoodSource), 100,
                 "T8650.a 半吸收后罩剩 100");
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T8650.a 粉伤全被罩吸（hp 不掉）");
        CHECK_EQ(slot1446(ctx, 0, 4), 0, "T8650.a 半吸收不触发（触发计数仍 0，武装清账口径）");
        CHECK(levels_all_eq(ctx, 1, 0), "T8650.a 半吸收不降属");
        // 被打穿：pink 100 → 池空 → 破罩事件 → 六槽逐项 -1（不看总和）。
        (void)deal_pink_damage(&ctx, 0, 100, DamageKind::FIXED, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hood_bank_.quantity_of(kHoodSource), 0,
                 "T8650.a 罩池清空");
        CHECK_EQ(slot1446(ctx, 0, 4), 1, "T8650.a 破罩触发恰一次");
        CHECK_EQ(slot1446(ctx, 0, 0), -1, "T8650.a armed 已消费（缺席）");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(ctx.ability_levels[1][i], -1, "T8650.a 对手六槽逐项 -1");
        }
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T8650.a 罩全吸 hp 不掉（破罩腿）");
        // 一次性：再 pink 100 → 无罩直落 hp，不重触发。
        (void)deal_pink_damage(&ctx, 0, 100, DamageKind::FIXED, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp - 100, "T8650.a 无罩粉伤直落 hp");
        CHECK_EQ(slot1446(ctx, 0, 4), 1, "T8650.a 一次性：不再触发");
        CHECK(levels_all_eq(ctx, 1, -1), "T8650.a 等级保持 -1（不二次降）");
        std::cout << "  [T8650.a] 1446 武装/半吸收负/破罩六槽逐项/一次性 通过\n";
    }

    // ══ T8650.b 1446 盾破不串（28258）：真实拍打（atk2000→[573,674]）破 1753 盾
    //    → EVENT_SHIELD_BROKEN 照发，池门 quantity_of(1446)>0 跳过（M-池门杀位）；
    //    盾破后 1753 回血旁证；随后 pink 400 罩破才触发 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主b", kChuanYue, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 2000, kMaxHp)));
        b.start();
        sbs_srand(2);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(1753), 400, "T8650.b 盾 400 前置");
        CHECK_EQ(ctx.getPet(0).hood_bank_.quantity_of(kHoodSource), 400, "T8650.b 罩 400 前置");
        // R2：真实红伤拍打 → 盾 400 全破（remaining 落 hp）→ 盾破事件 → 1446 池门跳过。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(1753), 0, "T8650.b 盾被红伤打破");
        CHECK(ctx.getPet(0).hp > kMaxHp - 700, "T8650.b 盾后实扣 <700（573-674 带内）");
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                 "T8650.b 共链 1753 破盾回血 1/3 钳顶旁证（红伤破盾前提成立）");
        CHECK_EQ(slot1446(ctx, 0, 4), 0, "T8650.b 盾破不触发罩子句（池门，M 杀位；武装清账=0）");
        CHECK(levels_all_eq(ctx, 1, 0), "T8650.b 盾破不降属");
        CHECK_EQ(slot1446(ctx, 0, 0), 1, "T8650.b armed 保留（池未空）");
        // R3：pink 400 → 罩破 → 触发（盾已不在，事件归因面只剩罩）。
        (void)deal_pink_damage(&ctx, 0, 400, DamageKind::FIXED, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1446(ctx, 0, 4), 1, "T8650.b 罩破触发恰一次");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(ctx.ability_levels[1][i], -1, "T8650.b 对手六槽逐项 -1");
        }
        std::cout << "  [T8650.b] 1446 盾破不串（池门）+罩破触发 通过\n";
    }

    // ══ T8650.c1 1446 镜像 side1（27363 [500,1]）：side1 用技武装 → pink 500 罩破
    //    → side0 六槽 -1；owner=1 键分账（M-键位杀位：步长错则计数串账）══
    {
        ScenarioBattle b(make_party(foe_pet(1, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)),
                         make_party(carrier_pet(2, "印主c1", kHuaLing, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(3);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot1446(ctx, 1, 0), 1, "T8650.c1 side1 armed（owner=1 键段）");
        CHECK_EQ(slot1446(ctx, 1, 2), 500, "T8650.c1 {0}=500 行参回显");
        CHECK_EQ(slot1446(ctx, 1, 8), 1, "T8650.c1 side1 使用计数=1（键分账）");
        CHECK_EQ(slot1446(ctx, 0, 8), -1, "T8650.c1 side0 键段零串账");
        CHECK_EQ(ctx.getPet(1).hood_bank_.quantity_of(kHoodSource), 500, "T8650.c1 罩 500");
        (void)deal_pink_damage(&ctx, 1, 500, DamageKind::FIXED, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1446(ctx, 1, 4), 1, "T8650.c1 side1 触发恰一次");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], -1, "T8650.c1 side0 六槽逐项 -1（镜像）");
        }
        CHECK(levels_all_eq(ctx, 1, 0), "T8650.c1 side1 自身等级不动");
        std::cout << "  [T8650.c1] 1446 镜像 side1+{0}=500 键分账 通过\n";
    }

    // ══ T8650.c2 1446 26581 [300,1] 共链旁证：885 自身全属性+1 不触对手（负测）、
    //    1239 满血钳零无附加粉伤；罩 300 破 → 对手 -1 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主c2", kYongHeng, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        sbs_srand(4);
        BattleContext& ctx = b.ctx();
        const int opp_hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1446(ctx, 0, 2), 300, "T8650.c2 {0}=300 行参回显");
        CHECK_EQ(ctx.getPet(0).hood_bank_.quantity_of(kHoodSource), 300, "T8650.c2 罩 300");
        CHECK(levels_all_eq(ctx, 0, 1), "T8650.c2 共链 885 自身全属性+1（等体不翻倍）");
        CHECK(levels_all_eq(ctx, 1, 0), "T8650.c2 885 不触对手（作用方向负测）");
        CHECK_EQ(ctx.getPet(1).hp, opp_hp0, "T8650.c2 1239 满血无附加粉伤（对手不掉血）");
        (void)deal_pink_damage(&ctx, 0, 300, DamageKind::FIXED, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1446(ctx, 0, 4), 1, "T8650.c2 罩破触发");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(ctx.ability_levels[1][i], -1, "T8650.c2 对手六槽逐项 -1");
        }
        std::cout << "  [T8650.c2] 1446 26581 共链旁证+罩破 通过\n";
    }

    // ══ T8650.d 1446 换宠 slot 戳（36318 [300,1]）：R2 切 fresh 槽1 → 手术盾 100
    //    被拍打破 → 事件 target=0 但 slot 戳不符 → 跳过不清旗（slot9=3）；R4 切回
    //    槽0 → pink 300 罩破触发（旗跨换宠往返存续）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主d", kHLShi, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 2000, kMaxHp)));
        b.start();
        sbs_srand(5);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1446(ctx, 0, 1), 0, "T8650.d armed slot 戳=0（获罩时在场槽）");
        CHECK_EQ(ctx.getPet(0).hood_bank_.quantity_of(kHoodSource), 300, "T8650.d 罩 300");
        // R2：主动换到 fresh 槽1（同宠副本——罩/旗留槽0 实例上）。
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.on_stage[0], 1, "T8650.d 已切到槽1");
        ctx.getPet(0).shield_bank_.add(0, 100, /*source_id=*/990478, 0);   // 手术盾
        // R3：对手拍打破手术盾 → 事件 target=0 → slot 戳不符跳过（不清旗）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(990478), 0, "T8650.d 手术盾被破");
        CHECK_EQ(slot1446(ctx, 0, 9), 3, "T8650.d slot 戳不符跳过（slot9=3）");
        CHECK_EQ(slot1446(ctx, 0, 0), 1, "T8650.d armed 旗保留（不清旗）");
        CHECK_EQ(slot1446(ctx, 0, 4), 0, "T8650.d 他人盾破不触发（M-slot戳杀位；武装清账=0）");
        // R4：切回槽0（原实例罩仍在）→ pink 300 罩破 → 触发。
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.on_stage[0], 0, "T8650.d 已切回槽0");
        std::vector<int> before;
        for (int i = 0; i < 6; ++i) {
            before.push_back(ctx.ability_levels[1][i]);
        }
        (void)deal_pink_damage(&ctx, 0, 300, DamageKind::FIXED, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1446(ctx, 0, 4), 1, "T8650.d 回场后罩破触发恰一次");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(ctx.ability_levels[1][i], before[i] - 1,
                     "T8650.d 对手六槽相对 captured 逐项 -1");
        }
        std::cout << "  [T8650.d] 1446 换宠 slot 戳（跳过+存续） 通过\n";
    }

    // ══ T8650.e 1446 重施放池叠加（28258×2）：罩池 800 两栈 → pink 400 破一栈
    //    （池剩 400 不触发——M-池语义杀位：逐条触发则计数 2）→ pink 400 池空恰一次 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主e", kChuanYue, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        sbs_srand(6);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1446(ctx, 0, 8), 2, "T8650.e 使用计数=2（重施放）");
        CHECK_EQ(ctx.getPet(0).hood_bank_.quantity_of(kHoodSource), 800, "T8650.e 罩池 800");
        (void)deal_pink_damage(&ctx, 0, 400, DamageKind::FIXED, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hood_bank_.quantity_of(kHoodSource), 400, "T8650.e 一栈被破");
        CHECK_EQ(slot1446(ctx, 0, 4), 0, "T8650.e 池未空不触发（M-池语义杀位）");
        (void)deal_pink_damage(&ctx, 0, 400, DamageKind::FIXED, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hood_bank_.quantity_of(kHoodSource), 0, "T8650.e 池空");
        CHECK_EQ(slot1446(ctx, 0, 4), 1, "T8650.e 池空触发恰一次");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(ctx.ability_levels[1][i], -1, "T8650.e 对手六槽逐项 -1");
        }
        std::cout << "  [T8650.e] 1446 重施放池叠加语义 通过\n";
    }

    // ══ T8650.f 658 基数=实扣（23514 [100] + 手术盾）：R2 盾 10000 全吸收 → 无
    //    TAKE_DAMAGE → 哨兵不消费（M-基数杀位）；R3 盾 300 部分吸收 → base=实扣=
    //    己方掉血 → 反馈=base → 三方相等（对手掉血==己方掉血==slot4）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主f", kWanFu, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 2000, kMaxHp)));
        b.start();
        sbs_srand(7);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot658(ctx, 0, 0), 100, "T8650.f {0}=100 行参回显");
        CHECK_EQ(slot658(ctx, 0, 1), 1, "T8650.f armed 置位");
        CHECK_EQ(slot658(ctx, 0, 8), 1, "T8650.f 使用计数=1");
        ctx.getPet(0).shield_bank_.add(0, 10000, /*source_id=*/990478, 0);   // 手术大盾
        // R2：拍打 [573,674] < 10000 → 全吸收 → 无 TAKE_DAMAGE → 不消费。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.getPet(0).shield_bank_.quantity_of(990478) > 9000, "T8650.f R2 盾未破");
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T8650.f R2 全吸收 hp 不掉");
        CHECK_EQ(slot658(ctx, 0, 6), 0, "T8650.f R2 哨兵未消费（M-基数杀位；武装清账=0）");
        CHECK_EQ(slot658(ctx, 0, 1), 1, "T8650.f R2 armed 保留");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp, "T8650.f R2 无反馈（对手不掉血）");
        // R3：换小盾 300 → 拍打破盾 → base=实扣=己方掉血 → 反馈恰等。
        ctx.getPet(0).shield_bank_.clear();
        ctx.getPet(0).shield_bank_.add(0, 300, /*source_id=*/990478, 0);
        const int opp_hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int my_loss = kMaxHp - ctx.getPet(0).hp;
        const int opp_drop = opp_hp0 - ctx.getPet(1).hp;
        CHECK(ctx.getPet(0).shield_bank_.quantity_of(990478) == 0, "T8650.f R3 盾破");
        CHECK(my_loss > 0, "T8650.f R3 实扣 >0（部分吸收前提）");
        CHECK_EQ(slot658(ctx, 0, 6), 1, "T8650.f R3 哨兵消费恰一次");
        CHECK_EQ(slot658(ctx, 0, 5), 1, "T8650.f R3 桥 fire 恰一次");
        CHECK_EQ(slot658(ctx, 0, 4), my_loss, "T8650.f R3 反馈量==实扣基数（slot4 对账）");
        CHECK_EQ(opp_drop, my_loss, "T8650.f R3 对手掉血==己方掉血（pct100 恒等）");
        CHECK_EQ(opp_drop, slot658(ctx, 0, 4), "T8650.f R3 行为面==审计面");
        CHECK_EQ(slot658(ctx, 0, 3), -1, "T8650.f R3 pending 已清账");
        std::cout << "  [T8650.f] 658 基数=实扣（全吸收负+部分吸收恒等） 通过\n";
    }

    // ══ T8650.g 658 {0}=80（23864 [80]）：粉伤直调不消费（damage_kind 过滤负测）
    //    → 真拍打触发 → 对手掉血==己方掉血×80/100（int 恒等式免 seed）→ R3 二次
    //    拍打零消费零反馈（一次性消费杀位）；656 掷点只耗 rand 不进断言面 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主g", kBingShuang, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 100, kMaxHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot658(ctx, 0, 0), 80, "T8650.g {0}=80 行参回显");
        CHECK_EQ(slot658(ctx, 0, 1), 1, "T8650.g armed 置位");
        // 粉伤负测：FIXED 档 TAKE_DAMAGE 投递但被 damage_kind 过滤 → 不消费。
        (void)deal_pink_damage(&ctx, 0, 100, DamageKind::FIXED, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.getPet(0).hp < kMaxHp, "T8650.g 粉伤落地前提（无罩直落 hp）");
        CHECK_EQ(slot658(ctx, 0, 6), 0, "T8650.g 粉伤不消费哨兵（M-通道杀位；武装清账=0）");
        CHECK_EQ(slot658(ctx, 0, 1), 1, "T8650.g armed 保留");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp, "T8650.g 粉伤不反馈");
        // R2：真拍打（atk100 → [30,36]）→ 消费 → 反馈 = 实扣×80/100。
        // ⚠️ my_loss 用 R2 前快照相对值——负测粉伤已扣过 hp，不能从 kMaxHp 起算。
        const int my_hp0 = ctx.getPet(0).hp;
        const int opp_hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int my_loss = my_hp0 - ctx.getPet(0).hp;
        const int opp_drop = opp_hp0 - ctx.getPet(1).hp;
        CHECK(my_loss > 0, "T8650.g R2 红伤落地前提");
        CHECK_EQ(slot658(ctx, 0, 6), 1, "T8650.g R2 消费恰一次");
        CHECK_EQ(slot658(ctx, 0, 5), 1, "T8650.g R2 fire 恰一次");
        CHECK_EQ(opp_drop, my_loss * 80 / 100, "T8650.g {0}=80 反馈恒等式（int 同式）");
        CHECK_EQ(opp_drop, slot658(ctx, 0, 4), "T8650.g 行为面==审计面");
        // R3：二次拍打 → armed 已消费 → 不再反馈（一次性）。
        const int opp_hp1 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(kMaxHp - ctx.getPet(0).hp > my_loss, "T8650.g R3 二次红伤落地前提");
        CHECK_EQ(slot658(ctx, 0, 6), 1, "T8650.g R3 不再消费（一次性）");
        CHECK_EQ(slot658(ctx, 0, 5), 1, "T8650.g R3 不再 fire");
        CHECK_EQ(ctx.getPet(1).hp, opp_hp1, "T8650.g R3 零反馈（M-一次性杀位）");
        std::cout << "  [T8650.g] 658 {0}=80 档+粉伤负测+一次性 通过\n";
    }

    // ══ T8650.h 658 跨回合后手 arm（24328 [100]，side0 速 100 后手）：R1 对手先手
    //    拍打发生在武装前 → 不回溯消费（"下次"前向语义）→ R1 末武装 → R2 对手再拍
    //    → 消费+反馈（TEAM 哨兵跨回合存续——duration=1 误写则 R2 不消费转红）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h", kSenLin, 100, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            300, 100, kMaxHp)));
        b.start();
        sbs_srand(9);
        BattleContext& ctx = b.ctx();
        // R1：side0 后手（速 100 < 300）——对手先拍打（未武装），随后武装。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(slot658(ctx, 0, 1), 1, "T8650.h R1 末 armed 置位");
        CHECK_EQ(slot658(ctx, 0, 6), 0, "T8650.h R1 武装前的攻击不回溯消费（清账=0）");
        CHECK_EQ(slot658(ctx, 0, 5), -1, "T8650.h R1 零 fire");
        CHECK(ctx.getPet(0).hp < kMaxHp, "T8650.h R1 对手拍打落地前提");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp, "T8650.h R1 无反馈");
        // R2：对手再拍打 → 消费 → 反馈恰等（pct100）。my_loss2 取 R2 前快照相对值
        //（R1 已扣过 hp，不能从 kMaxHp 起算）。
        const int my_hp1 = ctx.getPet(0).hp;
        const int opp_hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int my_loss2 = my_hp1 - ctx.getPet(0).hp;
        const int opp_drop2 = opp_hp0 - ctx.getPet(1).hp;
        CHECK(my_loss2 > 0, "T8650.h R2 红伤落地前提");
        CHECK_EQ(slot658(ctx, 0, 6), 1, "T8650.h R2 消费恰一次（跨回合哨兵）");
        CHECK_EQ(slot658(ctx, 0, 5), 1, "T8650.h R2 fire 恰一次");
        CHECK_EQ(opp_drop2, my_loss2, "T8650.h R2 反馈==实扣（pct100 恒等）");
        std::cout << "  [T8650.h] 658 跨回合后手 arm+前向语义 通过\n";
    }

    // ══ T8650.i 658 镜像 side1（23514）：side1 priority2 先手武装 → R2 side0 拍打
    //    → side1 消费 → 反馈落 side0（owner=1 键分账）══
    {
        ScenarioBattle b(make_party(foe_pet(1, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 100, kMaxHp)),
                         make_party(carrier_pet(2, "印主i", kWanFu, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(10);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot658(ctx, 1, 1), 1, "T8650.i side1 armed（owner=1 键段）");
        CHECK_EQ(slot658(ctx, 1, 8), 1, "T8650.i side1 使用计数=1");
        CHECK_EQ(slot658(ctx, 0, 8), -1, "T8650.i side0 键段零串账");
        // R2：side0 拍打 side1 → side1 哨兵消费 → 反馈落 side0。
        const int s0_hp0 = ctx.getPet(0).hp;
        const int s1_hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int s1_loss = s1_hp0 - ctx.getPet(1).hp;
        const int s0_drop = s0_hp0 - ctx.getPet(0).hp;
        CHECK(s1_loss > 0, "T8650.i R2 side1 受击前提");
        CHECK_EQ(slot658(ctx, 1, 6), 1, "T8650.i side1 消费恰一次");
        CHECK_EQ(slot658(ctx, 1, 5), 1, "T8650.i side1 fire 恰一次");
        CHECK_EQ(s0_drop, s1_loss, "T8650.i 反馈落 side0==side1 实扣（镜像恒等）");
        std::cout << "  [T8650.i] 658 镜像 side1 键分账 通过\n";
    }

    std::cout << "[T8650] 全部块通过（1446×5 载体 + 658×5 载体，11 块）" << std::endl;
    return 0;
}
