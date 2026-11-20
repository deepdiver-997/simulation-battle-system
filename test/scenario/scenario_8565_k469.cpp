// 场景 8565 —— K469 血量先制族（616，resources/moves_lib/k469_lowhp_prio.cpp +
// tools/db_patches/j4/selection_routes_expansion_6.sql；2026-11-17，worktree k469；
// 号段 8565-8569 → T8565-pre ＋ a-g 七块）
//
// ⚠️ 私有 DB 自备（共享库只读纪律）：616 路由行在**共享库仍是 37 行态**（K469
//   补丁待集成侧 apply_all 重放）→ 本场景开工时把共享库**复制**到
//   build/k469_route_patch.sqlite（本 worktree 私有、git 忽略），幂等应用
//   expansion_6 的 616 行（与补丁文件同文），initialize_runtime_data 用私有副本。
//   共享库零写入（a31-orch 2026-11-17 裁决口径）；集成侧打补丁后本场景自备流程
//   幂等收敛（INSERT OR IGNORE no-op）。
//
// 断言口径（真实载体 + 审计槽 + 先手权真实序三线并锁；K457 acting_side_is_first
//   "必先→先制→速度"序——先制加成后以弱速度抢先手 = 加成真实生效的结构断言）：
//   - 门判据 hp×3 < max_hp（454/1397 严格低于整数直乘；恰 1/3 → 门闭）。
//   - T8565-pre 六载体实际加载器切片核验（load_skill_effects 逐载体，非手算）。
//   - 门闭负测：满血（T8565.a）与恰 1/3 边界（T8565.c）→ 先手权落速度序
//     （弱速方后手），审计门判=0。
//   - 门开正测：hp<1/3（T8565.b）→ 先制+2 → 弱速方（100 vs 300）抢到先手
//     （preemptive_right == SEER_ROBOT_1），审计门判=1 + 加成后 preemptive_level
//     快照=2。
//   - 逐回合在位重判：开→仍低血再开→回血门闭三段（T8565.d，"开→仍低→再开"
//     是 latch 变异杀手——变异轮 M11 幸存教训后补强）、受击后门开
//     （T8565.e R0 闭+真实承伤跌破 1/3→R1 开）。
//   - 镜像同速两侧（T8565.f 双侧 17660 同速同血：双侧 +2、审计独立记账、
//     同先制同速落掷点序——不断言顺序）。
//   - 离场/换技能无残留（T8565.g R1 换拍打：路由节点随选择技能安装，未选载体
//     即无判门无加成，累计判门不再增长）。
//   - 审计槽（键式见插件头注；soul 空间 616 已实现 K303（990803000 段）→
//     moves 侧 +0100 子块拆分）：1016160100 + owner×16 + slot。
//   - 载体：17660 破锐怒冲（兄弟 128 受击转回血窗——门开/门闭/镜像块用，
//     自伤面不观测）、19323 龙怒天火断（无转回血兄弟——受击门开块用；兄弟
//     680 仅先手趟掷 40% 害怕，不涉断言面）。
//   - 变异权威轮（build/k469-mutations/，底本 SHA bbb405fc…48d32；删 .o 强编 +
//     SHA 双守卫）——11 变异 11 真红（s8565 rc=134 首杀各归其位、s8560 恒 0 =
//     特异性），还原后 SHA==底本逐字节一致 + deploy 复绿（86 断言 rc=0）：
//     M7 门边界（< → <=，恰 1/3 也开门）→ 首杀 T8565.c 先手权反转（应闭却开）；
//     M8 出值写死（+= bonus → += 1）→ 首杀 T8565.b 快照=1≠2；
//     M9 键位（audit_key_of 丢 owner）→ 首杀 T8565.f 侧1 门判=0（串账）；
//     M10 通道置换（preemptive_level → guaranteed_first）→ 首杀 T8565.b 快照=0
//       ≠2（先手权断言不红——必先同样抢先手，审计快照面锁通道归属）；
//     M11 生命周期（门开 latch）→ **初版幸存**（覆盖缺口："开→回血→闭"与
//       "closed→damage→open"均不与 latch 分岔）→ T8565.d 补强"开→仍低→再开"
//       三段结构后真红（首杀 T8565.d R1 先手权反转）——K465 M5a 同款教训：
//       生命周期语义的覆盖必须含"同条件连续两回合"分支。
//   - 场景纪律：无 while(!FINISHED)（单回合 submit+drive）、无概率断言、
//     must_hit+acc100+crit0 → 命中/暴击零抽。
#include <algorithm>
#include <any>
#include <array>
#include <filesystem>
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
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 私有 DB 自备（复制共享库 + 幂等应用 616 路由行；见文件头注）──────────────
constexpr const char* kPrivateDbPath = "build/k469_route_patch.sqlite";

// 与 tools/db_patches/j4/selection_routes_expansion_6.sql 的 INSERT 同文（证据字段落
// 场景侧幂等重放；补丁文件仍是权威交付物）。
constexpr const char* k616RouteInsertSql =
    "INSERT OR IGNORE INTO custom_skill_selection_routes "
    "(effect_id, phase, state, arg_adapter, schema_version, evidence) VALUES "
    "(616, 'selection', 'FIRST_MOVE_RIGHT', 'plain', 1, "
    "'当自身血量少于1/3时先制+2（effect_info 原文；6 载体 17660 破锐怒冲/19323 龙怒天火断/"
    "23413 海之契约/23836 轮回天生/24822 心怀苍生/38016 异怪波动，切片恒[3,2]逐位自洽；"
    "门=hp×3<max_hp 严格低于（454/1397 整数直乘口径，恰1/3门闭），出=ws.preemptive_level+=2 "
    "优先级修改面；J53 454 同形（j_cond_priority.cpp:36），a31-orch 2026-11-17 裁决扩行授权；"
    "soul 空间 616=K303 已实现（双空间同号甄别，K303 不受影响）；插件 k469_lowhp_prio.cpp）');";

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
    if (sqlite3_exec(db, k616RouteInsertSql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "ensure_private_db: insert failed: " << (err ? err : "?") << std::endl;
        sqlite3_free(err);
        sqlite3_close(db);
        return false;
    }
    sqlite3_free(err);
    sqlite3_close(db);
    return true;
}

constexpr int kPaiDa = 10004;       // 拍打：物40 无副作用（填充/观测面）
constexpr int kPoRui = 17660;       // 破锐怒冲 [128,616]|[3,2]（p0 物140，兄弟128 转回血）
constexpr int kLongNu = 19323;      // 龙怒天火断 [180,680,616]|[3,2]（p0 物150，无转回血）

constexpr int k616Base = 1016160100;   // moves 空间 616 子块（soul 616=K303 已实现拆分）

int stor_int(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

int a616(const BattleContext& ctx, int owner, int slot) {
    return stor_int(ctx, k616Base + owner * 16 + slot);
}

// 印主宠：首槽=载体技能，余拍打；全技能 must_hit+acc100+crit0（rand 记账前提）。
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

// 对手宠：全拍打（观测面）。
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

// 红伤带（atk300/def100 锚 raw ∈ [87,102] @拍打威力40，2026-10-13 勘误口径；
// 随威力线性缩放：×N×(power/40)，再外扩 ±3 余量——禁止拿单次抽样当边界）。
struct DamageBand {
    int lo;
    int hi;
};
DamageBand band_for(int n, int power) {
    return {87 * n * power / 40 - 3, 102 * n * power / 40 + 3};
}

int hp_of(const BattleContext& ctx, int side) {
    return ctx.seerRobot[side].elfPets[ctx.on_stage[side]].hp;
}

// 一回合驱动（K463 run_round 同款）：双提交 + drive；若停在 CHOOSE_AFTER_DEATH
// （本回合有宠倒下），给死亡方补 CHOOSE_PET 再驱动。
void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0, BattleFsm::ActionType a1,
               int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET,
                           /*slot=*/0);
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
              "T8565-pre 私有 DB 路由表装载成功（typed loader；find 前须显式装载——"
              "缓存按 DB 连接生命周期，loadSkills 内才懒加载）");
        const official_data::SelectionRouteRecord* rec = repo.find_selection_route(616);
        CHECK(rec != nullptr, "T8565-pre 私有 DB 616 路由行在位（自备流程生效）");
        CHECK(rec->arg_adapter == official_data::kSelectionRouteAdapterPlain,
              "T8565-pre 616 adapter=plain");
        CHECK_EQ(rec->schema_version, official_data::kSelectionRoutesSchemaVersion,
                 "T8565-pre 616 schema_version=1");
    }

    // ══ T8565-pre 六载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 616 六载体（切片恒 [3,2]）──
                {kPoRui, {{128, {1}}, {616, {3, 2}}}},
                {kLongNu, {{180, {}}, {680, {40, 6, 3}}, {616, {3, 2}}}},
                {23413, {{616, {3, 2}}, {43, {2}}, {57, {3, 4}}, {5, {5, 100, -1}}}},
                {23836, {{705, {}}, {616, {3, 2}}}},
                {24822, {{616, {3, 2}}, {902, {1}}, {749, {100, 1}}}},
                {38016, {{616, {3, 2}}, {1627, {1, 7, 15}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8565-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8565-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8565-pre 切片参数");
            }
        }
        std::cout << "  [T8565-pre] 六载体实际加载器切片核验 + 私有 DB 路由行在位 通过\n";
    }

    // ══ T8565.a 门闭负测（满血）：门闭 → 先制 0 → 先手权落速度序（弱速后手）══
    {
        sbs_srand(31);
        auto p0 = carrier_pet(0, "破锐", kPoRui, /*speed=*/100, /*atk=*/300, /*hp=*/30000);
        auto p1 = foe_pet(1, "对手", /*speed=*/300, /*atk=*/300, /*hp=*/99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 17660
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T8565.a 满血门闭 → 对手（速300）先手");
        CHECK_EQ(a616(ctx, 0, 2), 0, "T8565.a 审计门判=0（闭）");
        CHECK_EQ(a616(ctx, 0, 3), 0, "T8565.a 累计门开=0");
        CHECK_EQ(a616(ctx, 0, 4), 1, "T8565.a 累计判门=1");
        // 自身仍出手（后手）：17660 物140 → 对手掉血带（兄弟 128 在对手承伤面无涉）。
        const int loss = 99999 - hp_of(ctx, 1);
        const DamageBand bd = band_for(1, 140);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8565.a 对手承伤带 ×1（power140）");
        std::cout << "  [T8565.a] 门闭负测（满血 → 速度序先手） 通过\n";
    }

    // ══ T8565.b 门开正测：hp=9999 < 1/3 → 先制+2 → 弱速方抢先手 ══
    {
        sbs_srand(32);
        auto p0 = carrier_pet(0, "破锐", kPoRui, /*speed=*/100, /*atk=*/300, /*hp=*/30000);
        auto p1 = foe_pet(1, "对手", /*speed=*/300, /*atk=*/300, /*hp=*/99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        ctx.getPet(0).hp = 9999;   // 满血构造后压当前体力（坑集 #4 口径）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T8565.b 门开先制+2 → 弱速方（100 vs 300）抢到先手（K457 真实序）");
        CHECK_EQ(a616(ctx, 0, 2), 1, "T8565.b 审计门判=1（开）");
        CHECK_EQ(a616(ctx, 0, 3), 1, "T8565.b 累计门开=1");
        CHECK_EQ(a616(ctx, 0, 6), 2, "T8565.b 加成后 preemptive_level 快照=2");
        const int loss = 99999 - hp_of(ctx, 1);
        const DamageBand bd = band_for(1, 140);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8565.b 对手承伤带 ×1（先手趟）");
        std::cout << "  [T8565.b] 门开正测（先制+2 → 弱速抢先手） 通过\n";
    }

    // ══ T8565.c 恰 1/3 边界负测：hp=10000（恰 max/3）→ hp×3==max → 门闭 ══
    {
        sbs_srand(33);
        auto p0 = carrier_pet(0, "破锐", kPoRui, /*speed=*/100, /*atk=*/300, /*hp=*/30000);
        auto p1 = foe_pet(1, "对手", /*speed=*/300, /*atk=*/300, /*hp=*/99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        ctx.getPet(0).hp = 10000;   // 恰 1/3（严格低于口径 → 不触发）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T8565.c 恰 1/3 门闭 → 对手先手");
        CHECK_EQ(a616(ctx, 0, 2), 0, "T8565.c 审计门判=0（恰 1/3 闭）");
        CHECK_EQ(a616(ctx, 0, 0), 3, "T8565.c {0}回显=3");
        CHECK_EQ(a616(ctx, 0, 1), 2, "T8565.c {1}回显=2");
        std::cout << "  [T8565.c] 恰 1/3 边界负测（hp×3==max → 门闭） 通过\n";
    }

    // ══ T8565.d 回血后门闭 + 连续低血再开（逐回合在位重判，无 latch）：
    //   R0 开（hp 9999）→ R1 不回血仍低血再选载体 → **再开**（latch 变异杀手：
    //   "开→仍低→再开"与"开→latch 闭"在此分岔）→ 回血 → R2 闭。══
    {
        sbs_srand(34);
        auto p0 = carrier_pet(0, "破锐", kPoRui, /*speed=*/100, /*atk=*/300, /*hp=*/30000);
        auto p1 = foe_pet(1, "对手", /*speed=*/300, /*atk=*/300, /*hp=*/99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        ctx.getPet(0).hp = 9999;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T8565.d R0 门开 → 自身先手");
        CHECK_EQ(a616(ctx, 0, 4), 1, "T8565.d R0 累计判门=1");
        CHECK_EQ(a616(ctx, 0, 3), 1, "T8565.d R0 累计门开=1");
        // R1：血量未动（对手 NONE、自身承伤面无涉）仍 < 1/3 → 再选载体必须再开
        //（latch 变异在此转红：误 latch 会门闭 → 速度序 → 对手先手）。
        CHECK(hp_of(ctx, 0) < 10000, "T8565.d R1 前置：血量仍低于阈值（未被回血/转回血扰动）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T8565.d R1 连续低血再选 → 门再开（逐回合重判、无 latch）");
        CHECK_EQ(a616(ctx, 0, 3), 2, "T8565.d R1 累计门开=2（第二次开）");
        CHECK_EQ(a616(ctx, 0, 4), 2, "T8565.d R1 累计判门=2");
        // R2 前回血到阈值上方（场景手术：满血构造单独压当前体力的逆操作）。
        ctx.getPet(0).hp = 20000;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T8565.d R2 回血后门闭 → 对手先手（加成无 latch）");
        CHECK_EQ(a616(ctx, 0, 2), 0, "T8565.d R2 末次门判=0（闭）");
        CHECK_EQ(a616(ctx, 0, 3), 2, "T8565.d R2 累计门开仍=2（R2 未再开）");
        CHECK_EQ(a616(ctx, 0, 4), 3, "T8565.d R2 累计判门=3（逐回合在位重判）");
        std::cout << "  [T8565.d] 开→仍低再开→回血门闭（逐回合重判、无 latch） 通过\n";
    }

    // ══ T8565.e 受击后门开：R0 满血门闭+真实承伤跌破 1/3 → R1 门开抢先手 ══
    // 载体 19323（无转回血兄弟——伤害真实落血）；max_hp=120 → 阈值 40 < 承伤带下限
    // 87 → 一击必破线（R1 血量 ∈ [18,33] 全 < 40，确定性门开）。
    {
        sbs_srand(35);
        auto p0 = carrier_pet(0, "龙怒", kLongNu, /*speed=*/100, /*atk=*/300, /*hp=*/120);
        auto p1 = foe_pet(1, "对手", /*speed=*/300, /*atk=*/300, /*hp=*/99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        // R0：门闭 → 对手先手真打 → 自身掉血 [87,102] → hp ∈ [18,33]。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T8565.e R0 满血门闭 → 对手先手");
        const int hp_r0 = hp_of(ctx, 0);
        CHECK(hp_r0 > 0 && hp_r0 < 40, "T8565.e R0 承伤后血量跌破阈值（<40 且存活）");
        CHECK_EQ(a616(ctx, 0, 2), 0, "T8565.e R0 门判=0（选择时满血）");
        // 自身 R0 后手趟：19323 物150 → 对手掉血带。
        const int loss = 99999 - hp_of(ctx, 1);
        const DamageBand bd = band_for(1, 150);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8565.e 对手承伤带 ×1（power150）");
        // R1：血量 < 1/3 → 门开 → 自身先手（对手 NONE，害怕掷点无观测面影响）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T8565.e R1 受击后门开 → 弱速方抢到先手");
        CHECK_EQ(a616(ctx, 0, 2), 1, "T8565.e R1 门判=1（开）");
        CHECK_EQ(a616(ctx, 0, 3), 1, "T8565.e R1 累计门开=1");
        CHECK_EQ(a616(ctx, 0, 4), 2, "T8565.e R1 累计判门=2");
        CHECK_EQ(a616(ctx, 0, 6), 2, "T8565.e R1 加成后 preemptive_level 快照=2");
        std::cout << "  [T8565.e] 受击后门开（真实承伤 → 次回合门开抢先手） 通过\n";
    }

    // ══ T8565.f 镜像同速两侧：双侧 17660 同速同血 → 双侧 +2、审计独立记账 ══
    {
        sbs_srand(36);
        auto p0 = carrier_pet(0, "破锐0", kPoRui, /*speed=*/200, /*atk=*/300, /*hp=*/30000);
        auto p1 = carrier_pet(1, "破锐1", kPoRui, /*speed=*/200, /*atk=*/300, /*hp=*/30000);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        ctx.getPet(0).hp = 9999;
        ctx.getPet(1).hp = 9999;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 双侧同先制(+2)同速 → 先手落掷点序（不断言顺序）；双侧加成与审计对称。
        CHECK_EQ(ctx.ws.preemptive_level[0], 2, "T8565.f 侧0 preemptive_level=2");
        CHECK_EQ(ctx.ws.preemptive_level[1], 2, "T8565.f 侧1 preemptive_level=2");
        CHECK_EQ(a616(ctx, 0, 2), 1, "T8565.f 侧0 门判=1");
        CHECK_EQ(a616(ctx, 1, 2), 1, "T8565.f 侧1 门判=1");
        CHECK_EQ(a616(ctx, 0, 3), 1, "T8565.f 侧0 累计门开=1（键位含 owner 维度）");
        CHECK_EQ(a616(ctx, 1, 3), 1, "T8565.f 侧1 累计门开=1");
        std::cout << "  [T8565.f] 镜像同速两侧（双侧 +2 独立记账） 通过\n";
    }

    // ══ T8565.g 换技能趟无评估：R0 开 → R1 换拍打 → 路由节点随选择技能安装 ══
    {
        sbs_srand(37);
        auto p0 = carrier_pet(0, "破锐", kPoRui, /*speed=*/100, /*atk=*/300, /*hp=*/30000);
        auto p1 = foe_pet(1, "对手", /*speed=*/300, /*atk=*/300, /*hp=*/99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        ctx.getPet(0).hp = 9999;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T8565.g R0 门开 → 自身先手");
        // R1：回满血 + 改点拍打（槽1，无 616）→ 无判门无加成 → 速度序。
        ctx.getPet(0).hp = 30000;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, -1);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T8565.g R1 未选载体 → 无加成 → 对手先手");
        CHECK_EQ(a616(ctx, 0, 4), 1, "T8565.g R1 累计判门仍=1（未选载体不判门）");
        CHECK_EQ(a616(ctx, 0, 2), 1, "T8565.g R1 末次门判保持 R0 的开（无新判）");
        std::cout << "  [T8565.g] 换技能趟无评估（路由随选择技能安装） 通过\n";
    }

    std::cout << "\n场景 8565 通过：K469 血量先制族 616——满血/恰 1/3 门闭负测、门开弱速抢先手"
                 "正测（K457 真实序）、回血门闭/受击门开逐回合重判、镜像同速双侧记账、"
                 "换技能趟无评估。\n";
    return 0;
}
