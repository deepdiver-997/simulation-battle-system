// 场景 8742 —— K487 两族（953/798，resources/moves_lib/k487_regen_prio_and_foeboost_cap.cpp
// ⚠️ 改号记档（2026-11-22 编排）：原号 8740 被主线 expel-switch 线占用（scenario_8740_expel_switch_cause.cpp），段内顺延取 8742；T8742-pre 等标签同批改号。
// + tools/db_patches/j4/selection_routes_expansion_7.sql；2026-11-19，worktree k487；
// 号段 8742-8749 → T8742-pre ＋ a-e/953 五块 + a-d/798 五块）
//
// ⚠️ 私有 DB 自备（共享库只读纪律）：953 路由行在**共享库仍是 38 行态**（K487
//   补丁待集成侧 apply_all 重放）→ 本场景开工时把共享库**复制**到
//   build/k487_route_patch.sqlite（本 worktree 私有、git 忽略），幂等应用
//   expansion_7 的 953 行（与补丁文件同文），initialize_runtime_data 用私有副本。
//   共享库零写入（K469 a31-orch 2026-11-17 裁决口径）；集成侧打补丁后本场景
//   自备流程幂等收敛（INSERT OR IGNORE no-op）。
//
// 断言口径（真实载体 + 审计槽 + 先手权真实序/精确 hp 三线并锁）：
//   - 953 先制半 = 先手权结构断言（先制等级不等时先手权只看等级不看速度——
//     弱速方抢先手 = 加成真实生效；K457"必先→先制→速度"序）+ 审计门判/快照。
//     门 = hp×100 < max_hp×{2} 严格低于整数直乘（恰 {2}% → 门闭）。
//   - 953 回血半 = 回合扣减点 tick（762 直系）精确 hp 断言（半血构造 + 单独压
//     当前 hp——坑集 #4；clamp 实际量按确定性记账断言）+ 累计 tick 计数 +
//     窗起点回显（InRounds 先手当回合/后手顺延 = round_effect_start_round 契约）。
//   - 953 在位重判三段（T8743）：开→(回血)→闭→(压血)→再开——latch 双向变异杀手
//     （K469 M11 幸存教训同款覆盖）。
//   - 798 = 门（对手任一正槽）武装快照 + CAP 精确封顶断言（超/不超两侧：
//     atk300 [87,102]<200 不超侧原值落地、atk900 [261,306]>200 封顶侧恰 200）
//     + 窗过期（门外命中计数）+ 武装快照 vs 每回合重评（T8747：窗内消除对手
//     提升后攻击仍被封顶——快照档锁定）+ 697 凭证穿透 + 属性直伤封顶
//     （deal_attribute_damage 直调真实结算出口，8530 T8530.h 同法）+ 换宠
//     epoch 离场清理 + 重施放代际刷新不叠加。
//   - rand 记账：全技能 crit0+acc100+must_hit → 命中/暴击零抽；953 腿全 cat4
//     载体+对手 NONE → 全程零抽；798 腿攻击伤害各一抽（带断言容忍浮动）。
//   - 场景纪律：无 while(!FINISHED)（单回合 submit+drive、多回合有界 for 循环）、
//     无概率断言（全部确定性/精确值/宽带上界）。
#include <algorithm>
#include <any>
#include <array>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <sqlite3.h>
#include <vector>

#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 私有 DB 自备（复制共享库 + 幂等应用 953 路由行；见文件头注）──────────────
constexpr const char* kPrivateDbPath = "build/k487_route_patch.sqlite";

// 与 tools/db_patches/j4/selection_routes_expansion_7.sql 的 INSERT 同文（证据字段落
// 场景侧幂等重放；补丁文件仍是权威交付物）。
constexpr const char* k953RouteInsertSql =
    "INSERT OR IGNORE INTO custom_skill_selection_routes "
    "(effect_id, phase, state, arg_adapter, schema_version, evidence) VALUES "
    "(953, 'selection', 'FIRST_MOVE_RIGHT', 'plain', 1, "
    "'{0}回合恢复最大体力的1/{1}，体力低于{2}%时先制+{3}（effect_info 原文；5 载体 "
    "24921 坠天断魂曲/25269 地底核心/26577 冰霸霸之魂/28592 福龙临世/28714 风翼护体，"
    "切片[4,3,40,2]x4+[3,3,60,1]逐位自洽；先制半门=hp×100<max_hp×{2} 严格低于整数直乘，"
    "出=ws.preemptive_level+={3} 优先级修改面；回血半=路由 continue 吞普通注册→插件体自挂"
    "当回合 SKILL_EFFECT 武装节点（539 双半直系）→ROUND_REDUCTION InRounds {0} 回合窗"
    "（762 tick 直系）；K469 616 同形，E 板 K487 行 616 先例直系授权；soul 空间 953 仅 "
    "effect_icon 模板行无运行时注册；插件 k487_regen_prio_and_foeboost_cap.cpp）');";

bool ensure_private_db() {
    std::error_code ec;
    const std::filesystem::path src(official_data::kDefaultOfficialDatabasePath);
    const std::filesystem::path dst(kPrivateDbPath);
    std::filesystem::create_directories(dst.parent_path(), ec);
    // 每次运行重建副本（跟随共享库当前态，幂等；集成侧打补丁后自动收敛）。
    std::filesystem::copy_file(src, dst,
                               std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "ensure_private_db: copy failed: " << ec.message() << std::endl;
        return false;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open(dst.string().c_str(), &db) != SQLITE_OK) {
        std::cerr << "ensure_private_db: open failed" << std::endl;
        return false;
    }
    char* err = nullptr;
    if (sqlite3_exec(db, k953RouteInsertSql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "ensure_private_db: insert failed: " << (err ? err : "?") << std::endl;
        sqlite3_free(err);
        sqlite3_close(db);
        return false;
    }
    sqlite3_free(err);
    sqlite3_close(db);
    return true;
}

// ── 载体与审计键 ──────────────────────────────────────────────────────────
constexpr int kPaiDa = 10004;      // 拍打：物40 无副作用（填充/观测面）
constexpr int kSuoTou = 20001;     // 缩头 [4]|[1,100,1] 属性技——自身防御+1（提升门源）
constexpr int kBingLing = 10151;   // 冰凌之光 [33] 属性技——消对手提升（快照甄别腿）
constexpr int kFuLong = 28592;     // 福龙临世 [48,953,557]|[4,4,3,40,2,4,100,20]
                                   //   cat4 p0——953 主载体（rounds=4/denom3/40%/+2）
constexpr int kFengYi = 28714;     // 风翼护体 [138,48,953]|[1,3,3,3,3,60,1]
                                   //   cat4 p0——953 差分载体（rounds=3/60%/+1）
constexpr int kXieLi = 35691;      // 卸力毒针 [798]|[3,200] cat1 物140 p0——798 主载体（纯链）
constexpr int kYinMing = 36229;    // 银鸣 [913,798]|[100,2,280] cat1 物85 prio3——798 rounds=2
constexpr int kGuangZhen = 31199;  // 光震粉碎机 [1180,697,696]|[2,40,20,100] cat1 物160——697 载体
constexpr int kMaxHp = 30000;      // 953 腿最大体力（tick = 1/3 = 10000，阈值 40% = 12000）

constexpr int k953Base = 1009530000;    // moves 953（soul 953 无实现免拆）
constexpr int k798Base = 1007980100;    // moves 798（soul 798=K245 → +0100 子块拆分）

int stor_int(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}
int a953(const BattleContext& ctx, int owner, int slot) {
    return stor_int(ctx, k953Base + owner * 16 + slot);
}
int a798(const BattleContext& ctx, int owner, int slot) {
    return stor_int(ctx, k798Base + owner * 16 + slot);
}

// ── 宠构造（K469/K480 同式：must_hit+acc100+crit0 → 命中/暴击零抽）────────
ElfPet carrier_pet(int id, const char* name, int carrier, int speed, int atk, int hp) {
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
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

// 双特技宠（798 腿：slot0=主载体/slot1=副技）。
ElfPet dual_pet(int id, const char* name, int skill0, int skill1, int speed, int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(skill0), Skills(skill1), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
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

ElfPet foe_pet(int id, const char* name, int speed, int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
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

// 提升门源宠：slot0=缩头（自身防御+1 属性技）、slot1=拍打（可改 31199 测 697）。
ElfPet booster_pet(int id, const char* name, int attack_skill, int speed, int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(kSuoTou), Skills(attack_skill), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
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

// 红伤带（atk300/def100 拍打 raw ∈ [87,102]，随攻击面板线性缩放，外扩 ±3 余量；
// 2026-10-13 勘误口径——禁止拿单次抽样当边界）。
struct DamageBand {
    int lo;
    int hi;
};
DamageBand band_for(int atk, int power) {
    const int k = atk * power / (300 * 40);   // atk300+power40 → k=1
    return {87 * k - 3, 102 * k + 3};
}

int hp_of(const BattleContext& ctx, int side) {
    return ctx.seerRobot[side].elfPets[ctx.on_stage[side]].hp;
}

// 中途压当前 hp（最大体力不动——坑集 #4 口径；K473/K474/K475/K476 同款 helper）。
void set_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
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
                           dead == 0 ? 1 : 0);
        b.drive();
    }
}

}  // namespace

int main() {
    if (!ensure_private_db()) {
        std::cerr << "failed to provision private db" << std::endl;
        return 1;
    }
    if (!PetFactory::initialize_runtime_data(kPrivateDbPath)) {
        std::cerr << "failed to init runtime data (private db)" << std::endl;
        return 1;
    }

    // ── 私有 DB 路由行在位核验（fail-closed：自备失败即红，不带病测试）──────
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        CHECK(repo.load_selection_routes(),
              "T8742-pre 私有 DB 路由表装载成功（typed loader；find 前须显式装载——"
              "缓存按 DB 连接生命周期，loadSkills 内才懒加载）");
        const official_data::SelectionRouteRecord* rec = repo.find_selection_route(953);
        CHECK(rec != nullptr, "T8742-pre 私有 DB 953 路由行在位（自备流程生效）");
        CHECK(rec->arg_adapter == official_data::kSelectionRouteAdapterPlain,
              "T8742-pre 953 adapter=plain");
        CHECK_EQ(rec->schema_version, official_data::kSelectionRoutesSchemaVersion,
                 "T8742-pre 953 schema_version=1");
        CHECK(repo.find_selection_route(798) == nullptr,
              "T8742-pre 798 无路由行（798 全 SKILL_EFFECT 桶——无先制面）");
    }

    // ══ T8742-pre 10 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 953 五载体（[4,3,40,2]×4 + [3,3,60,1]）──
                {24921, {{952, {1}}, {953, {4, 3, 40, 2}}, {776, {2}}}},
                {25269, {{585, {1, 1, 1, 1, 1, 1}}, {953, {4, 3, 40, 2}}, {843, {2, 1}}}},
                {26577, {{4, {0, 100, 2}}, {4, {1, 100, 2}}, {4, {3, 100, 2}},
                          {953, {4, 3, 40, 2}}, {780, {3, 50, 1}}}},
                {28592, {{48, {4}}, {953, {4, 3, 40, 2}}, {557, {4, 100, 20}}}},
                {28714, {{138, {1, 3}}, {48, {3}}, {953, {3, 3, 60, 1}}}},
                // ── 798 五载体（[3,200]×2 + [3,300] + [2,280] + [3,300]）──
                {17772, {{180, {}}, {798, {3, 200}}}},
                {33362, {{1117, {}}, {798, {3, 300}}}},
                {35691, {{798, {3, 200}}}},
                {36229, {{913, {100}}, {798, {2, 280}}}},
                {37618, {{1230, {1, 2}}, {458, {100}}, {1736, {5}}, {798, {3, 300}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8742-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8742-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8742-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8742-pre] 10 载体实际加载器切片核验 + 私有 DB 路由行在位 通过\n";
    }

    // ══ T8742-pre-b 39 行态引用面下限（同 7930 B 段；固定载体在上段精确核验）══
    {
        sqlite3* db = nullptr;
        CHECK(sqlite3_open_v2(kPrivateDbPath, &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK,
              "T8742-pre-b 私有 DB 只读句柄");
        int ref_total = -1, carrier_total = -1;
        struct Rc {
            int* r;
            int* c;
        } rc{&ref_total, &carrier_total};
        const char* kSql =
            "SELECT COUNT(*), COUNT(DISTINCT m.id) FROM moves m, "
            "json_each(m.side_effect) j WHERE j.value IN "
            "(SELECT effect_id FROM custom_skill_selection_routes);";
        char* err = nullptr;
        CHECK(sqlite3_exec(db, kSql,
                           +[](void* u, int, char** v, char**) -> int {
                               auto* out = static_cast<Rc*>(u);
                               *out->r = std::atoi(v[0]);
                               *out->c = std::atoi(v[1]);
                               return 0;
                           },
                           &rc, &err) == SQLITE_OK,
              "T8742-pre-b 引用面查询执行");
        sqlite3_free(err);
        sqlite3_close(db);
        CHECK(ref_total >= 484, "T8742-pre-b 39 行态引用总数不低于历史下限 484");
        CHECK(carrier_total >= 480, "T8742-pre-b 39 行态载体总数不低于历史下限 480");
        std::cout << "  [T8742-pre-b] 39 行态引用面 " << ref_total << '/' << carrier_total
                  << "（历史下限 484/480）\n";
    }

    // ══ T8742.a 953 门闭负测 + 回血首跳正测（28592 主载体；满血压 15000>40%×30000）══
    // rand 记账：全程零抽（cat4 载体 + 对手 NONE + must_hit）。
    {
        sbs_srand(41);
        auto p0 = carrier_pet(0, "福龙", kFuLong, /*speed=*/300, /*atk=*/1, kMaxHp);
        auto p1 = foe_pet(1, "对手", /*speed=*/100, /*atk=*/1, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        set_hp(b, 0, 15000);   // 15000 > 12000（40% 阈值）→ 门闭；半血观测回血
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T8742.a 门闭 → 先手权落速度序（300 vs 100 己方先手）");
        CHECK_EQ(a953(ctx, 0, 4), 0, "T8742.a 审计门判=0（闭）");
        CHECK_EQ(a953(ctx, 0, 5), 1, "T8742.a 累计判门=1");
        CHECK_EQ(a953(ctx, 0, 6), 0, "T8742.a 累计门开=0");
        CHECK_EQ(a953(ctx, 0, 2), 40, "T8742.a {2} 行参回显=40");
        CHECK_EQ(a953(ctx, 0, 3), 2, "T8742.a {3} 行参回显=2");
        // 回血子句无条件（门闭不影响）：先手 InRounds 当回合即首跳。
        CHECK_EQ(a953(ctx, 0, 9), 0, "T8742.a 窗起点=0（先手 InRounds 含当回合）");
        CHECK_EQ(a953(ctx, 0, 10), 1, "T8742.a 累计回血 tick=1");
        CHECK_EQ(a953(ctx, 0, 11), 10000, "T8742.a 首跳实际恢复=10000（max/3）");
        CHECK_EQ(a953(ctx, 0, 12), 0, "T8742.a 末次回血回合=0");
        CHECK_EQ(hp_of(ctx, 0), 25000, "T8742.a hp=15000+10000=25000");
        std::cout << "  [T8742.a] 门闭负测+回血首跳（先手当回合结算） 通过\n";
    }

    // ══ T8742.b 953 门开正测 + 先手权结构断言（hp=11000<12000；弱速抢先手）══
    {
        sbs_srand(42);
        auto p0 = carrier_pet(0, "福龙", kFuLong, /*speed=*/100, /*atk=*/1, kMaxHp);
        auto p1 = foe_pet(1, "对手", /*speed=*/300, /*atk=*/1, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        set_hp(b, 0, 11000);   // 11000 < 12000 → 门开
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T8742.b 门开先制+2 → 弱速方（100 vs 300）抢到先手（等级不等只看等级）");
        CHECK_EQ(a953(ctx, 0, 4), 1, "T8742.b 审计门判=1（开）");
        CHECK_EQ(a953(ctx, 0, 6), 1, "T8742.b 累计门开=1");
        CHECK_EQ(a953(ctx, 0, 8), 2, "T8742.b 出值后 preemptive_level 快照=2");
        CHECK_EQ(a953(ctx, 0, 10), 1, "T8742.b 回血 tick=1（门开不影响回血半）");
        CHECK_EQ(hp_of(ctx, 0), 21000, "T8742.b hp=11000+10000=21000");
        std::cout << "  [T8742.b] 门开正测+先手权结构断言 通过\n";
    }

    // ══ T8742.c 边界恰 40% 门闭（hp×100 == max×40 → 严格低于门闭）══
    {
        sbs_srand(43);
        auto p0 = carrier_pet(0, "福龙", kFuLong, /*speed=*/100, /*atk=*/1, kMaxHp);
        auto p1 = foe_pet(1, "对手", /*speed=*/300, /*atk=*/1, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        set_hp(b, 0, 12000);   // 恰 40%：12000×100 == 30000×40 → 门闭
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T8742.c 恰 40% 门闭（严格低于）→ 对手（速300）先手");
        CHECK_EQ(a953(ctx, 0, 4), 0, "T8742.c 审计门判=0（边界闭）");
        CHECK_EQ(a953(ctx, 0, 6), 0, "T8742.c 累计门开=0");
        // 门闭不关回血子句：施放方=第二手 → cast 当回合零 tick、R1 起跳（顺延再证）。
        CHECK_EQ(a953(ctx, 0, 9), 1, "T8742.c 窗起点=1（第二手顺延）");
        CHECK_EQ(a953(ctx, 0, 10), 0, "T8742.c R0 零 tick（后手 cast 当回合不结算）");
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a953(ctx, 0, 10), 1, "T8742.c R1 回血照常（门闭不关回血子句）");
        CHECK_EQ(hp_of(ctx, 0), 22000, "T8742.c hp=12000+10000=22000");
        std::cout << "  [T8742.c] 边界恰 40% 门闭+回血子句独立 通过\n";
    }

    // ══ T8741 InRounds 窗多回合 + 窗过期（28592 rounds=4：tick R0..R3，R4 零）══
    {
        sbs_srand(44);
        auto p0 = carrier_pet(0, "福龙", kFuLong, /*speed=*/300, /*atk=*/1, kMaxHp);
        auto p1 = foe_pet(1, "对手", /*speed=*/100, /*atk=*/1, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        set_hp(b, 0, 9000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a953(ctx, 0, 10), 1, "T8741 R0 tick=1");
        CHECK_EQ(a953(ctx, 0, 11), 10000, "T8741 R0 实际=10000（9000→19000）");
        // R1..R3：双方 NONE（carrier 不再选择——窗 tick 独立于选择）。
        for (int r = 1; r <= 3; ++r) {
            run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        }
        CHECK_EQ(a953(ctx, 0, 10), 4, "T8741 R1..R3 累计 tick=4（窗 [0,4) 全跳）");
        CHECK_EQ(a953(ctx, 0, 12), 3, "T8741 末次 tick 回合=3");
        CHECK_EQ(hp_of(ctx, 0), 30000,
                 "T8741 hp 钳满记账（9000→19000→29000→[+1000]30000→[+0]30000）");
        CHECK_EQ(a953(ctx, 0, 11), 0, "T8741 R3 实际=0（满血钳零——K473 内生面）");
        // R4：窗过期零 tick。
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a953(ctx, 0, 10), 4, "T8741 R4 窗过期 tick 不增");
        CHECK_EQ(a953(ctx, 0, 12), 3, "T8741 R4 末次 tick 回合仍=3");
        std::cout << "  [T8741] InRounds 窗多回合+窗过期 通过\n";
    }

    // ══ T8742 后手顺延（第二手施放：窗起点 R+1，cast 当回合零 tick）══
    // ⚠️ hp 必须在门闭区间（22000>12000）：门开会 +2 抢先手、施放方变先手
    //     （InRounds 不顺延）——本腿专测后手顺延，门开面归 T8742.b。
    {
        sbs_srand(45);
        auto p0 = carrier_pet(0, "福龙", kFuLong, /*speed=*/100, /*atk=*/1, kMaxHp);
        auto p1 = foe_pet(1, "对手", /*speed=*/300, /*atk=*/1, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        set_hp(b, 0, 22000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T8742 门闭 → 速度序（对手 300 先手，施放方=第二手）");
        CHECK_EQ(a953(ctx, 0, 4), 0, "T8742 门判=0（闭——回血半不受门影响）");
        CHECK_EQ(a953(ctx, 0, 9), 1, "T8742 窗起点=1（后手 round_effect_start_round 顺延）");
        CHECK_EQ(a953(ctx, 0, 10), 0, "T8742 cast 当回合零 tick（后手不结算）");
        CHECK_EQ(hp_of(ctx, 0), 22000, "T8742 R0 hp 未动");
        for (int r = 1; r <= 4; ++r) {
            run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        }
        CHECK_EQ(a953(ctx, 0, 10), 4, "T8742 R1..R4 累计 tick=4（窗 [1,5)）");
        CHECK_EQ(a953(ctx, 0, 12), 4, "T8742 末次 tick 回合=4");
        CHECK_EQ(hp_of(ctx, 0), 30000,
                 "T8742 hp 钳满记账（22000→[+8000]30000→[+0]×3）");
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a953(ctx, 0, 10), 4, "T8742 R5 窗过期不增（4 回合窗恰 {0} 跳）");
        std::cout << "  [T8742] 后手顺延（cast 当回合零 tick、R+1 起算） 通过\n";
    }

    // ══ T8743 选择期在位重判三段（开→(回血)→闭→(压血)→再开——latch 双向杀手）══
    {
        sbs_srand(46);
        auto p0 = carrier_pet(0, "福龙", kFuLong, /*speed=*/100, /*atk=*/1, kMaxHp);
        auto p1 = foe_pet(1, "对手", /*speed=*/300, /*atk=*/1, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        set_hp(b, 0, 11000);
        // R0：门开（+2 → 弱速抢先手）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1, "T8743 R0 门开抢先手");
        CHECK_EQ(a953(ctx, 0, 6), 1, "T8743 R0 门开=1");
        // R1：回血后 hp=21000>阈值 → 再选择门闭（速度序）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T8743 R1 回血后门闭 → 速度序（对手先手）——latch『开后恒开』被杀");
        CHECK_EQ(a953(ctx, 0, 4), 0, "T8743 R1 门判=0（闭）");
        CHECK_EQ(a953(ctx, 0, 6), 1, "T8743 R1 累计门开仍=1");
        // 压血（K473 同款 helper）→ R2 再选择门再开。
        set_hp(b, 0, 8000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T8743 R2 压血后门再开 → 弱速抢先手——latch『闭后恒闭』被杀");
        CHECK_EQ(a953(ctx, 0, 4), 1, "T8743 R2 门判=1（开）");
        CHECK_EQ(a953(ctx, 0, 6), 2, "T8743 R2 累计门开=2");
        std::cout << "  [T8743] 选择期在位重判三段（latch 双向杀手） 通过\n";
    }

    // ══ T8744 镜像同速双开 + 换宠离场清理（epoch 作废 → 离场方窗死）══
    {
        sbs_srand(47);
        auto p0 = carrier_pet(0, "福龙A", kFuLong, /*speed=*/200, /*atk=*/1, kMaxHp);
        auto p1 = carrier_pet(1, "福龙B", kFuLong, /*speed=*/200, /*atk=*/1, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        set_hp(b, 0, 11000);
        set_hp(b, 1, 11000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        // 双侧门开：镜像独立记账（owner×16 分账）；同先制同速不断言顺序（K469 同款）。
        CHECK_EQ(a953(ctx, 0, 4), 1, "T8744 side0 门开");
        CHECK_EQ(a953(ctx, 1, 4), 1, "T8744 side1 门开");
        CHECK_EQ(a953(ctx, 0, 8), 2, "T8744 side0 快照=2");
        CHECK_EQ(a953(ctx, 1, 8), 2, "T8744 side1 快照=2（独立记账）");
        // R0 回血：同先制同速落掷点序 → 恰一侧（先手）tick、后手侧顺延零 tick。
        CHECK_EQ(a953(ctx, 0, 10) + a953(ctx, 1, 10), 1,
                 "T8744 R0 恰先手侧 tick=1（后手 cast 当回合零 tick）");
        // R1：双侧 NONE → 先手侧（R0 已 tick）再 tick=2、后手侧首跳=1（合计 3）。
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        const int s0_r1 = a953(ctx, 0, 10);
        const int s1_r1 = a953(ctx, 1, 10);
        CHECK_EQ(s0_r1 + s1_r1, 3, "T8744 R1 合计=3（先手窗 [0,4) 已 2 跳 + 后手窗 [1,5) 1 跳）");
        CHECK((s0_r1 == 1 || s0_r1 == 2) && (s1_r1 == 1 || s1_r1 == 2),
              "T8744 R1 两侧各在 {1,2}（顺序掷点序不定）");
        // R2：side0 换宠（离场）→ 窗 epoch 作废；side1 不动照常 +1。
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a953(ctx, 0, 10), s0_r1, "T8744 R2 side0 离场后窗死（tick 冻结）");
        CHECK_EQ(a953(ctx, 1, 10), s1_r1 + 1, "T8744 R2 side1 照常 +1");
        CHECK_EQ(a953(ctx, 0, 5), 1, "T8744 R2 side0 累计判门冻结=1（换宠回合无选择）");
        std::cout << "  [T8744] 镜像双开+换宠离场清理（epoch 作废） 通过\n";
    }

    // ══ T8745.a 798 门闭负测 + 封顶不超侧（对手无提升 → 不武装；[87,102]<200 原值）══
    // rand 记账：R0 35691 攻击 1 抽；R1 对手拍打 1 抽。
    {
        sbs_srand(51);
        auto p0 = carrier_pet(0, "卸力", kXieLi, /*speed=*/300, /*atk=*/300, kMaxHp);
        auto p1 = foe_pet(1, "对手", /*speed=*/100, /*atk=*/300, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a798(ctx, 0, 2), 0, "T8745.a 无提升 → 门判=0");
        CHECK_EQ(a798(ctx, 0, 11), 0, "T8745.a 累计武装=0（门闭不武装）");
        CHECK_EQ(a798(ctx, 0, 15), 1, "T8745.a 跳过原因=1（门闭不武装）");
        CHECK_EQ(a798(ctx, 0, 0), 3, "T8745.a {0} 行参回显=3");
        CHECK_EQ(a798(ctx, 0, 1), 200, "T8745.a {1} 行参回显=200");
        // R1：对手拍打（无提升、atk300 → [87,102] < 200 不超侧原值落地）。
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int loss = kMaxHp - hp_of(ctx, 0);
        const DamageBand bd = band_for(300, 40);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8745.a 不超侧原值落地（带断言）");
        CHECK_EQ(a798(ctx, 0, 5), 0, "T8745.a 累计封顶=0（无票无封顶）");
        std::cout << "  [T8745.a] 798 门闭负测+不超侧原值 通过\n";
    }

    // ══ T8745.b 798 门开正测 + 封顶超侧（缩头后武装；atk900 [261,306] → 恰 200）══
    {
        sbs_srand(52);
        auto p0 = carrier_pet(0, "卸力", kXieLi, /*speed=*/300, /*atk=*/300, kMaxHp);
        auto p1 = booster_pet(1, "对手", kPaiDa, /*speed=*/100, /*atk=*/900, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        // R0：对手缩头（自身防御+1 → 有提升）；己方 NONE。
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        // R1：己方 35691 → 门开武装（快照档）。35691 物140 攻击对手（带断言）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a798(ctx, 0, 2), 1, "T8745.b 门判=1（对手有提升）");
        CHECK(a798(ctx, 0, 3) >= 1, "T8745.b 对手正槽计数>=1");
        CHECK_EQ(a798(ctx, 0, 11), 1, "T8745.b 累计武装=1");
        CHECK_EQ(a798(ctx, 0, 4), 1, "T8745.b 窗起点=1（先手 InRounds 含施放回合）");
        CHECK_EQ(a798(ctx, 0, 12), 4, "T8745.b 窗终点=4（[1,4) rounds=3）");
        // R2：对手拍打 atk900 → [261,306] 超封顶 → 恰 200。
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(kMaxHp - hp_of(ctx, 0), 200, "T8745.b 封顶侧恰 200（超侧锁定）");
        CHECK_EQ(a798(ctx, 0, 5), 1, "T8745.b 累计封顶=1");
        CHECK_EQ(a798(ctx, 0, 8), 200, "T8745.b post-cap=200");
        const int raw7 = a798(ctx, 0, 7);
        const DamageBand bd = band_for(900, 40);
        CHECK(raw7 >= bd.lo && raw7 <= bd.hi, "T8745.b 原伤带（封顶前值审计）");
        std::cout << "  [T8745.b] 798 门开武装+封顶超侧恰 200 通过\n";
    }

    // ══ T8746 窗过期（rounds=3：R1..R3 窗内封顶、R4 门外原值+门外计数）══
    {
        sbs_srand(53);
        auto p0 = carrier_pet(0, "卸力", kXieLi, /*speed=*/300, /*atk=*/300, kMaxHp);
        auto p1 = booster_pet(1, "对手", kPaiDa, /*speed=*/100, /*atk=*/900, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a798(ctx, 0, 4), 1, "T8746 武装窗 [1,4)");
        // R2/R3：窗内两击全封顶。
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(kMaxHp - hp_of(ctx, 0), 200, "T8746 R2 封顶 200");
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(a798(ctx, 0, 5), 2, "T8746 R3 累计封顶=2");
        // R4：窗外（roundCount=4 >= 4）→ 原值落地 + 门外计数。
        const int hp_before = hp_of(ctx, 0);
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 1);
        const int loss = hp_before - hp_of(ctx, 0);
        const DamageBand bd = band_for(900, 40);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8746 R4 窗过期原值落地（带断言）");
        CHECK_EQ(a798(ctx, 0, 5), 2, "T8746 R4 累计封顶仍=2");
        CHECK_EQ(a798(ctx, 0, 13), 1, "T8746 门外命中计数=1");
        std::cout << "  [T8746] 窗过期（[1,4) 边界 + 门外计数） 通过\n";
    }

    // ══ T8747 武装快照甄别（窗内消除对手提升后攻击仍封顶——快照档锁定）+ rounds=2 差分 ══
    // 36229 银鸣 [913,798]|[100,2,280]：{0}=2 → 窗 [1,3)。
    {
        sbs_srand(54);
        auto p0 = dual_pet(0, "银鸣", kYinMing, kBingLing, /*speed=*/300, /*atk=*/300, kMaxHp);
        auto p1 = booster_pet(1, "对手", kPaiDa, /*speed=*/100, /*atk=*/900, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        // R0：对手缩头（提升）。
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        // R1：己方 36229 武装（窗 [1,3)；36229 prio3 先手；913 对手无回合类效果 inert）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a798(ctx, 0, 2), 1, "T8747 门判=1");
        CHECK_EQ(a798(ctx, 0, 4), 1, "T8747 窗起点=1");
        CHECK_EQ(a798(ctx, 0, 12), 3, "T8747 窗终点=3（rounds=2 差分载体）");
        CHECK_EQ(a798(ctx, 0, 1), 280, "T8747 {1}=280 行参回显");
        // R2：己方冰凌之光消对手提升（33 → clear_stat_boosts）；对手拍打攻击——
        // 武装快照档：提升已消除但窗内仍封顶（每回合重评变异在此转红）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(kMaxHp - hp_of(ctx, 0), 280, "T8747 R2 提升已消除仍封顶恰 280（快照档）");
        CHECK_EQ(a798(ctx, 0, 5), 1, "T8747 R2 累计封顶=1");
        // R3：窗外（rounds=2 → [1,3)）→ 原值 + 门外计数。
        const int hp_before = hp_of(ctx, 0);
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 1);
        const int loss = hp_before - hp_of(ctx, 0);
        const DamageBand bd = band_for(900, 40);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8747 R3 窗过期原值落地");
        CHECK_EQ(a798(ctx, 0, 13), 1, "T8747 门外命中=1");
        std::cout << "  [T8747] 武装快照甄别+rounds=2 差分 通过\n";
    }

    // ══ T8748.a 697 凭证穿透（31199 [1180,697,696] → 无视伤害限制 → 不封顶）══
    {
        sbs_srand(55);
        auto p0 = carrier_pet(0, "卸力", kXieLi, /*speed=*/300, /*atk=*/300, kMaxHp);
        auto p1 = booster_pet(1, "对手", kGuangZhen, /*speed=*/100, /*atk=*/900, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a798(ctx, 0, 11), 1, "T8748.a 武装=1");
        // R2：对手 31199（slot1，携 697 凭证）→ 穿锁伤不封顶。
        const int hp_before = hp_of(ctx, 0);
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 1);
        const int loss = hp_before - hp_of(ctx, 0);
        CHECK(loss > 500,
              "T8748.a 697 凭证穿透 → 远超 cap 落地（若封顶则恰 200；31199 物160+696 "
              "暴击浮动宽容——断言只锁『未被 200 封住』）");
        CHECK_EQ(a798(ctx, 0, 9), 1, "T8748.a 697 穿透计数=1");
        CHECK_EQ(a798(ctx, 0, 5), 0, "T8748.a 累计封顶=0（凭证压过封顶）");
        std::cout << "  [T8748.a] 798 697 凭证穿透 通过\n";
    }

    // ══ T8748.b 属性直伤封顶（deal_attribute_damage 直调真实结算出口——8530 T8530.h 同法；
    //     798 模板无"攻击"限定 → isRed 面含属性直伤，766 口径）══
    {
        sbs_srand(56);
        auto p0 = carrier_pet(0, "卸力", kXieLi, /*speed=*/300, /*atk=*/1, kMaxHp);
        auto p1 = booster_pet(1, "对手", kPaiDa, /*speed=*/100, /*atk=*/1, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a798(ctx, 0, 11), 1, "T8748.b 武装=1");
        // 直调属性伤：elem {1,0} vs {0,0} → 倍率 1.0 → 5000 全额进 CAP 相。
        const int hp_before = hp_of(ctx, 0);
        const int elem[2] = {1, 0};
        (void)deal_attribute_damage(&ctx, /*target=*/0, /*points=*/5000, elem, /*actor=*/1);
        CHECK_EQ(hp_before - hp_of(ctx, 0), 200, "T8748.b 属性直伤 5000 封顶恰 200（isRed 面）");
        CHECK_EQ(a798(ctx, 0, 5), 1, "T8748.b 累计封顶=1（属性伤计入）");
        CHECK_EQ(a798(ctx, 0, 7), 5000, "T8748.b 原伤=5000");
        std::cout << "  [T8748.b] 798 属性直伤封顶 通过\n";
    }

    // ══ T8748.c 镜像（双侧武装互封——独立记账）══
    {
        sbs_srand(57);
        auto p0 = dual_pet(0, "卸力A", kXieLi, kSuoTou, /*speed=*/200, /*atk=*/900, kMaxHp);
        auto p1 = dual_pet(1, "卸力B", kXieLi, kSuoTou, /*speed=*/200, /*atk=*/900, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        // R0：双方缩头（互有提升）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        // R1：双方 35691 武装（各自读对手提升）。⚠️ 时序：先动方先武装——后动方的
        //     35691 击落在先动方武装之后（被封 200），先动方承的后动方击落在其武装
        //     之前（未封 [913,1071] 物140 带）——两侧伤情由顺序定、断言按双侧或。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(a798(ctx, 0, 2), 1, "T8748.c side0 门开（对手有提升）");
        CHECK_EQ(a798(ctx, 1, 2), 1, "T8748.c side1 门开（对手有提升）");
        const int d0_r1 = kMaxHp - hp_of(ctx, 0);
        const int d1_r1 = kMaxHp - hp_of(ctx, 1);
        const DamageBand bd35691 = band_for(900, 140);
        CHECK((d0_r1 == 200 && d1_r1 >= bd35691.lo && d1_r1 <= bd35691.hi) ||
                  (d1_r1 == 200 && d0_r1 >= bd35691.lo && d0_r1 <= bd35691.hi),
              "T8748.c R1 恰一侧被封 200、另一侧吃未武装 35691 全带（先动先武装时序）");
        // R2：双方拍打互攻 → 各自承伤恰 200（双侧均已武装）。
        const int hp0_pre_r2 = hp_of(ctx, 0);
        const int hp1_pre_r2 = hp_of(ctx, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::SELECT_SKILL, 2);
        CHECK_EQ(hp0_pre_r2 - hp_of(ctx, 0), 200, "T8748.c R2 side0 承伤封顶 200");
        CHECK_EQ(hp1_pre_r2 - hp_of(ctx, 1), 200, "T8748.c R2 side1 承伤封顶 200");
        CHECK_EQ(a798(ctx, 0, 5) + a798(ctx, 1, 5), 3,
                 "T8748.c 合计封顶=3（先动方 2 + 后动方 1；R1 未武装击不封）");
        CHECK(a798(ctx, 0, 5) >= 1 && a798(ctx, 1, 5) >= 1,
              "T8748.c 双侧封顶各>=1（独立记账不串账）");
        std::cout << "  [T8748.c] 镜像互封独立记账 通过\n";
    }

    // ══ T8749.a 换宠离场清理（CAP 条目 pipeline epoch 作废 → 新宠承伤不封顶）══
    {
        sbs_srand(58);
        auto p0 = carrier_pet(0, "卸力", kXieLi, /*speed=*/300, /*atk=*/300, kMaxHp);
        auto p1 = booster_pet(1, "对手", kPaiDa, /*speed=*/100, /*atk=*/900, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a798(ctx, 0, 11), 1, "T8749.a 武装=1");
        // R2：side0 换宠 → 条目 epoch 作废；对手攻击新宠 → 原值落地。
        const int hp_before = hp_of(ctx, 0);
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        const int loss = hp_before - hp_of(ctx, 0);
        const DamageBand bd = band_for(900, 40);
        CHECK(loss >= bd.lo && loss <= bd.hi,
              "T8749.a 换宠后新宠承伤原值落地（epoch 作废=离场清理结构性成立）");
        CHECK_EQ(a798(ctx, 0, 5), 0, "T8749.a 累计封顶=0（旧条目已死）");
        std::cout << "  [T8749.a] 换宠离场清理（pipeline epoch） 通过\n";
    }

    // ══ T8749.b 重施放代际刷新（跨载体 36229→35691：gen1 cap=280 / gen2 cap=200，
    //     攻击 raw [345,411] 恒超两档 → 旧条目复活必在封顶计数显形；同载体同 cap
    //     的封顶写幂等、计数不可见——M10 幸存实测后按 K469 M11 先例改造）══
    {
        sbs_srand(59);
        auto p0 = dual_pet(0, "银鸣", kYinMing, kXieLi, /*speed=*/300, /*atk=*/300, kMaxHp);
        auto p1 = booster_pet(1, "对手", kPaiDa, /*speed=*/100, /*atk=*/1200, kMaxHp);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        // R1：36229 首次武装（gen=1，cap=280，rounds=2 → 窗 [1,3)）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a798(ctx, 0, 10), 1, "T8749.b 首次代际=1");
        CHECK_EQ(a798(ctx, 0, 1), 280, "T8749.b {1} 回显=280（36229）");
        CHECK_EQ(a798(ctx, 0, 4), 1, "T8749.b 窗起点=1");
        CHECK_EQ(a798(ctx, 0, 12), 3, "T8749.b 窗终点=3（rounds=2）");
        // R2：35691 重施放（gen=2，cap=200，窗刷新 [2,5)）+对手 atk1200 拍打
        //     raw [345,411]：gen1 窗 [1,3) 仍在但代际死 → 恰一次封顶 200
        //     （守卫摘除则 gen1 写 280→gen2 写 200，计数=2——叠加变异在此转红）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(a798(ctx, 0, 10), 2, "T8749.b 重施放代际=2");
        CHECK_EQ(a798(ctx, 0, 11), 2, "T8749.b 累计武装=2");
        CHECK_EQ(a798(ctx, 0, 4), 2, "T8749.b 窗起点刷新=2");
        CHECK_EQ(a798(ctx, 0, 12), 5, "T8749.b 窗终点刷新=5");
        CHECK_EQ(a798(ctx, 0, 1), 200, "T8749.b {1} 回显刷新=200（35691）");
        CHECK_EQ(kMaxHp - hp_of(ctx, 0), 200, "T8749.b R2 封顶恰 200");
        CHECK_EQ(a798(ctx, 0, 5), 1, "T8749.b R2 封顶计数=1（旧代际自灭不双计——叠加变异在此转红）");
        // R3：gen1 已过期（[1,3)）、gen2 窗内 → 照常封顶（回合差值断言）。
        const int hp_pre_r3 = hp_of(ctx, 0);
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(hp_pre_r3 - hp_of(ctx, 0), 200, "T8749.b R3 封顶恰 200");
        CHECK_EQ(a798(ctx, 0, 5), 2, "T8749.b R3 累计封顶=2");
        // R4：gen2 窗末回合（[2,5) 含 4）→ 仍封顶。
        const int hp_pre_r4 = hp_of(ctx, 0);
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(hp_pre_r4 - hp_of(ctx, 0), 200, "T8749.b R4 窗末封顶恰 200");
        CHECK_EQ(a798(ctx, 0, 5), 3, "T8749.b R4 累计封顶=3");
        // R5：窗外原值落地（窗外计数面已由 T8746/T8747 单代代锁定，此处不叠断言）。
        const int hp_before = hp_of(ctx, 0);
        run_round(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 1);
        const int loss = hp_before - hp_of(ctx, 0);
        const DamageBand bd = band_for(1200, 40);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8749.b R5 窗过期原值落地");
        CHECK_EQ(a798(ctx, 0, 5), 3, "T8749.b R5 累计封顶仍=3");
        std::cout << "  [T8749.b] 重施放代际刷新不叠加（跨载体封顶差分） 通过\n";
    }

    std::cout << "\n场景 8742 通过：K487 两族 953（选择期路由双半：血量门先制+InRounds "
                 "回血窗）+798（提升门武装快照+CAP 封顶）——真实载体切片核验+先手权结构"
                 "断言+精确 hp/封顶断言+窗界/镜像/离场清理全绿。\n";
    return 0;
}
