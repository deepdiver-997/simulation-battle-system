// 场景 8180 —— R8 连击技 miss 结算勘误：miss 趟零攻击伤害（静态/变威力载体 2×2 矩阵）
//（2026-11-07，worktree r8；号段 8180-8189，主场景本文件 8180）
//
// 勘误本体（K424 记档，2026-11-07 实测隔离：拍打 0/20 vs 手工 combo 界 20/20）：
//   修复前 handle_*_AttackDamage 的无效出口 run_attack_damage_from_invalid_skill
//   （src/fsm/battleFsm.cpp:991）用 variable_power_requested（battleFsm.cpp:819-826）
//   当门，其 `combo_view > 1` 项把**引擎打底的静态连击基数**（resolve_skill_execution
//   materialize，battleFsm.cpp:517，不打 rewritten 标记——battleWorkspace.h:87）也当
//   "靠后的变威力改写"放行 → 连击技 miss（SKILL_INVALID 趟）stage+finish 全额重结算
//   N× 红伤。修复（R8）：无效出口两处 gates（run_attack_damage_from_invalid_skill /
//   run_invalid_skill_damage_hooks 的"已接管"守卫）窄化为
//   variable_power_rewritten_by_effect（只认 rewritten）；命中趟收尾
//   apply_variable_power_recalc 保持全集（静态连击照样走"破防→重算→×N"，044 C4）。
//
// 官方语义（置信度高，三源）：
//   ① effect 72「若没有命中对手则自己失去全部体力」/ 136「若Miss则自己恢复1/{0}体力」
//     ——官方给 miss 专门开补偿模板且只注册在 SKILL_INVALID 分支（skills.h:346），
//     佐证"miss 趟默认零伤害、零效果，唯一后果是补偿分支明说的那件事"；
//   ② 1141 模板自身措辞「每次攻击{2}%令对手{3}」——伤害来自"落地的攻击"，
//     8070 T7 已锁 miss 趟逐段异常钩子全不触发（尝试/landed==0）；
//   ③ 语料 idx452/idx100（连击/变威力专篇）只定义"一次公式×N"与变威力重算，
//     无"miss 结算伤害"表述；引擎既有原则（battleFsm.cpp 无效出口注释）
//     "没有靠后的变威力改写 → 无效就是无效，零伤害"在静态连击载体上自相矛盾，即本勘误。
//   例外保留（2501「乐园之初诞」L95 同构）：效果在 SKILL_INVALID 分支**真的改写**
//   视图 → 照旧补结算（本场景 E 块防过修）。
//
// 断言口径（2×2 矩阵逐格 + 防过修 + 单击对照；全确定性，无概率断言）：
//   P0 J326 不变量面：30762 炽焰狙击 [1172]|[1,2,6,30,…] 静态基数 2~6（DB 规则行驱动）。
//   A 命中×静态连击：seed=3 → MINSTD 首抽 50421 → N=2+(50421%5)=3（8070 T1 同式
//      闭式复核）；hitCount==3、combo_view==3、伤害>0（命中趟行为零变化）。
//   B miss×静态连击：对手 100% 闪避 → hitCount/final==0、raw==0、掉血==0（修复本体）；
//      前提审计：SKILL_INVALID、combo_view==3 且 rewritten==false（静态打底已物化、
//      零效果改写——bug 的确切前提）。
//   C miss×变威力连击载体：HIT 分支注入"威力=1000"改写（只在命中趟注册）→ miss 趟
//      改写不发生 → 掉血==0（修复第二格：变威力载体 miss 同样零伤害）。
//   D 命中×变威力连击：同注入 → 命中趟照常改写+×N：hitCount==3、伤害 > A 局 ×10
//      （威力 30→1000 理论比值 ≈32.7×；同 seed 下公式浮动/连击数同轨，纯比值锁）。
//   E miss×SKILL_INVALID 分支改写（2501 同构，防过修）：miss 趟效果真的改写威力
//      → 照旧补结算：掉血>0、hitCount==3、final>0（L95 例外路径保留）。
//   F miss×单击载体（拍打）：掉血==0（K424 隔离对照 0/20 的既有行为，零变化）。
//   G 命中×静态连击+必暴+守方特防+2：破防后重算 ×N 保持（044 C4 同款口径落在本单
//      载体上——天真修法"从全集判据里删 combo_view>1"在此转红）。
//   ⚠️ 驱动纪律：单回合 submit+drive（禁 while(!FINISHED)）；对手 NONE 空转（8070
//      run_cast 同式）→ 我方零受击；每块独立对局 + 独立钉种子；伤害断言只锁
//      命中趟比值/符号，不锁绝对值（浮动 217~255 与取整不进断言）。
#include <any>
#include <array>
#include <iostream>
#include <vector>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打：物10 威力40 命中100 无 side_effect（单击对照/填充）
constexpr int kChiYan  = 30762;   // 炽焰狙击：特10 威力30 命中100 [1172]|[1,2,6,30,…] → 静态连击 2~6
constexpr int kGaoSu   = 20017;   // 高速移动：属性技能（对手空转配技，本场景实际用 NONE，不出手）

constexpr int kRewrittenPower = 1000;   // 注入改写的威力值（30 → 1000，理论比值 ≈32.7×）
constexpr int kFakeIdHitRewrite     = 818001;   // 测试假效果 id（场景号前缀，无注册冲突）
constexpr int kFakeIdInvalidRewrite = 818002;

// 审计槽（坑集 §0：前提真的成立才能断言——改写函数是否真的跑了，用槽记次数）。
constexpr int kAuditHitRewriteRuns     = 8180010;   // HIT 分支改写函数执行次数
constexpr int kAuditInvalidRewriteRuns = 8180020;   // SKILL_INVALID 分支改写函数执行次数

void audit_bump(BattleContext* ctx, int key) {
    int n = 0;
    if (const auto it = ctx->plugin_storage.find(key); it != ctx->plugin_storage.end()) {
        if (const int* v = std::any_cast<int>(&it->second)) {
            n = *v;
        }
    }
    ctx->plugin_storage[key] = n + 1;
}

int audit_count(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

// 威力改写函数：`operator=` 写入即置 rewritten（battleWorkspace.h:81-82，变威力标记）。
EffectResult rewrite_power_hit_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.skill_power_view[owner] = kRewrittenPower;
    audit_bump(ctx, kAuditHitRewriteRuns);
    return EffectResult::kOk;
}

EffectResult rewrite_power_invalid_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.skill_power_view[owner] = kRewrittenPower;
    audit_bump(ctx, kAuditInvalidRewriteRuns);
    return EffectResult::kOk;
}

// 白盒注入：把"威力改写"节点塞进指定技能的指定结果分支（register_branch 逐节点
// 注册到 BATTLE_FIRST_SKILL_EFFECT 桶；HIT 分支只在命中趟注册，SKILL_INVALID 分支
// 只在 miss/无效趟注册——skills.cpp execute 三个注册点）。
void inject_branch_rewrite(ElfPet& pet, SkillExecResult branch, int fake_id) {
    const EffectFn fn = (branch == SkillExecResult::HIT) ? &rewrite_power_hit_effect
                                                         : &rewrite_power_invalid_effect;
    pet.skills[0].effectBranches[branch].push_back(SkillEffectNode{
        Effect(fake_id, /*priority=*/0, /*owner=*/0, /*left_round=*/0,
               EffectArgs(std::vector<int>{0, 1}), fn),
        State::BATTLE_FIRST_SKILL_EFFECT});
}

// 连击载体攻方（非必中：miss 块靠对手闪避魂印制造真 miss；命中块 accuracy=100 恒中）。
ElfPet combo_attacker(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "连击方", skills, /*speed=*/300, /*atk=*/400, /*hp=*/999999);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
    return p;
}

ElfPet plain_target(const char* name, int id,
                    SoulMark soul_mark = {}) {
    return make_pet(id, name, {kPaiDa, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                    /*speed=*/100, /*atk=*/1, /*hp=*/999999, std::move(soul_mark));
}

// 标准局：我方先手出 slot0 技，对手 NONE 空转（零受击）。seed 钉死 → 连击数闭式确定。
void run_cast(ScenarioBattle& b, int seed) {
    b.ctx().seed_rng(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

int foe_hp(BattleContext& ctx) { return ctx.getPet(1).hp; }

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ P0 J326 不变量面：静态连击基数仍由 DB 规则行驱动（7910 B 表 1172 行同源）══
    {
        Skills s(kChiYan);
        CHECK_EQ(s.combo_min, 2, "P0) 30762 静态连击下限==2（custom_skill_combo_rules 1172 行）");
        CHECK_EQ(s.combo_max, 6, "P0) 30762 静态连击上限==6");
    }

    // ══ A 命中趟 × 静态连击：一次公式 ×N，行为零变化 ══
    int dmg_static_hit = 0;
    {
        ScenarioBattle b(make_party(combo_attacker({kChiYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(plain_target("目标", 2)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = foe_hp(ctx);
        run_cast(b, /*seed=*/3);   // MINSTD 首抽 50421 → N = 2 + 50421%5 = 3
        dmg_static_hit = hp0 - foe_hp(ctx);
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::HIT,
                 "A) 命中趟前提：技能 HIT");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 3,
                 "A) seed=3 静态连击 N==3（resolvedDamage 权威记录）");
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 3,
                 "A) combo_view 物化值==3（引擎打底）");
        CHECK(dmg_static_hit > 0, "A) 命中趟伤害>0（静态连击照常结算）");
        std::cout << "  [A] 命中×静态连击: N=3 伤害=" << dmg_static_hit << "\n";
    }

    // ══ B miss 趟 × 静态连击：零伤害（本勘误修复本体）══
    {
        ScenarioBattle b(make_party(combo_attacker({kChiYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(plain_target("闪避目标", 2, make_dodge_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = foe_hp(ctx);
        run_cast(b, /*seed=*/3);
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID,
                 "B) miss 前提：技能 SKILL_INVALID（对手 100% 闪避 + 非必中）");
        // 前提审计（坑集 §0）：静态打底确实物化 N>1 且**零效果改写**——
        // 这正是修复前 bug 的触发条件（variable_power_requested 的 combo_view>1 项）。
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 3,
                 "B) 前提：miss 趟静态连击基数已物化 N==3");
        CHECK(!ctx.ws.combo_view[0].rewritten,
              "B) 前提：连击视图 rewritten==false（引擎物化，非效果改写）");
        CHECK(!ctx.ws.skill_power_view[0].rewritten,
              "B) 前提：威力视图 rewritten==false（无效果改写）");
        CHECK_EQ(hp0 - foe_hp(ctx), 0, "B) miss 趟零伤害（修复本体：不再全额重结算）");
        CHECK_EQ(ctx.resolvedDamage.final, 0, "B) resolvedDamage.final==0（无效出口清快照）");
        CHECK_EQ(ctx.resolvedDamage.attackerId, -1,
                 "B) 快照未 staged（attackerId==NSDMI -1；hitCount 的 NSDMI 是 1，不作判据）");
        CHECK_EQ(ctx.ws.raw_attack_damage[0], 0, "B) 裸伤台账==0（未结算）");
        std::cout << "  [B] miss×静态连击: 零伤害（修复前为 N=3 全额重结算）\n";
    }

    // ══ C miss 趟 × 变威力连击载体（HIT 分支改写只在命中趟注册）：零伤害 ══
    {
        ElfPet atk = combo_attacker({kChiYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
        inject_branch_rewrite(atk, SkillExecResult::HIT, kFakeIdHitRewrite);
        ScenarioBattle b(make_party(atk),
                         make_party(plain_target("闪避目标", 2, make_dodge_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = foe_hp(ctx);
        run_cast(b, /*seed=*/3);
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID,
                 "C) miss 前提：技能 SKILL_INVALID");
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 3,
                 "C) 前提：静态连击基数已物化 N==3");
        CHECK_EQ(audit_count(ctx, kAuditHitRewriteRuns), 0,
                 "C) 前提：HIT 分支改写在 miss 趟未注册（执行次数==0）");
        CHECK(!ctx.ws.skill_power_view[0].rewritten,
              "C) 前提：威力视图未改写");
        CHECK_EQ(hp0 - foe_hp(ctx), 0,
                 "C) miss 趟零伤害（变威力载体同样：命中趟效果不参与 miss 趟）");
        CHECK_EQ(ctx.ws.raw_attack_damage[0], 0, "C) 裸伤台账==0");
        std::cout << "  [C] miss×变威力连击载体: 零伤害\n";
    }

    // ══ D 命中趟 × 变威力连击：改写落地 + ×N（命中趟行为零变化）══
    {
        ElfPet atk = combo_attacker({kChiYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
        inject_branch_rewrite(atk, SkillExecResult::HIT, kFakeIdHitRewrite);
        ScenarioBattle b(make_party(atk),
                         make_party(plain_target("目标", 2)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = foe_hp(ctx);
        run_cast(b, /*seed=*/3);
        const int dmg = hp0 - foe_hp(ctx);
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::HIT,
                 "D) 命中趟前提：技能 HIT");
        CHECK_EQ(audit_count(ctx, kAuditHitRewriteRuns), 1,
                 "D) 前提：HIT 分支改写执行一次（命中趟照常注册）");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 3, "D) seed=3 连击 N==3");
        // 同 seed 下 A/D 两局：连击数、命中 roll、公式浮动（两次结算两掷）同轨，
        // 唯一差异 = 威力视图 30 → 1000 → 伤害比值 ≈32.7×（42×1000×0.08+2 vs 42×30×0.08+2）。
        // 变威力"第二次结算覆盖"照常发生（rewritten → recalc），比值锁死改写+×N 双通路。
        CHECK(dmg > dmg_static_hit * 10,
              "D) 改写落地：伤害 > A 局 ×10（理论 ≈32.7×）");
        std::cout << "  [D] 命中×变威力连击: N=3 伤害=" << dmg << "（A 局 " << dmg_static_hit << "）\n";
    }

    // ══ G 命中趟 × 静态连击的"破防→重算→×N"两趟流水线（防过修面）══
    // 若有人把修复写成"从 variable_power_requested 里删掉 combo_view>1 项"（天真修法），
    // 命中趟静态连击就不再走第二次结算（第一次公式在暴击破防前算完）→ 本块转红。
    // 正确修复（本单）只窄化无效出口，命中趟收尾 apply_variable_power_recalc 保持全集。
    // 口径同 044 C4（必暴 + 守方特防+2：破防后重算读到归零特防 → 伤害 ≈2×（破防）×3（N）；
    // 不重算则 ≈2×（暴击）×0.5（特防 200 减伤）×3 ≈ 1× 无增益）。
    {
        ElfPet atk = combo_attacker({kChiYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
        scenario::set_crit_sixteenths(atk, 16);   // 必定暴击（确定性档）
        ScenarioBattle b(make_party(atk), make_party(plain_target("特防目标", 2)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][3] = 2;   // 守方特防 +2（特殊技走槽 3，crit_defense_break 同映射）
        ctx.ws.view_levels[1][3] = 2;
        const int hp0 = foe_hp(ctx);
        run_cast(b, /*seed=*/3);
        const int dmg = hp0 - foe_hp(ctx);
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::HIT, "G) 命中趟前提：技能 HIT");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 3, "G) seed=3 连击 N==3");
        // 带宽推演（浮动 [217,255]/255 = [0.851,1.0]）：正确两趟 dmg/dmg_A ∈ [1.70, 2.35]；
        // 天真修法（不重算，特防 200 减半）比值 ∈ [0.87, 1.20]。取 1.45 分界。
        CHECK(dmg * 100 > dmg_static_hit * 145,
              "G) 命中趟两趟流水线保持：破防后重算 ×N（天真删 combo>1 项会转红）");
        std::cout << "  [G] 命中×静态连击(必暴+特防2): 伤害=" << dmg << "（A 局 " << dmg_static_hit
                  << "，比值≈" << (dmg * 100 / (dmg_static_hit ? dmg_static_hit : 1)) / 100.0
                  << "）\n";
    }

    // ══ E miss 趟 × SKILL_INVALID 分支改写（2501 乐园之初诞同构，防过修）══
    // 修复只窄化"引擎静态基数"那条路；效果在无效趟**真的改写**视图 → 照旧补结算
    // （官方 L95：技能无效时重新进行伤害结算，"技能效果还是无效，只有攻击伤害"）。
    {
        ElfPet atk = combo_attacker({kChiYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
        inject_branch_rewrite(atk, SkillExecResult::SKILL_INVALID, kFakeIdInvalidRewrite);
        ScenarioBattle b(make_party(atk),
                         make_party(plain_target("闪避目标", 2, make_dodge_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = foe_hp(ctx);
        run_cast(b, /*seed=*/3);
        const int dmg = hp0 - foe_hp(ctx);
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID,
                 "E) 前提：miss 趟");
        CHECK_EQ(audit_count(ctx, kAuditInvalidRewriteRuns), 1,
                 "E) 前提：SKILL_INVALID 分支改写执行一次（无效趟照常注册）");
        CHECK(dmg > 0, "E) 2501 同构：无效趟效果改写 → 照旧补结算（例外路径保留）");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 3, "E) 补结算走一次公式×N：hitCount==3");
        CHECK_EQ(ctx.resolvedDamage.final, dmg, "E) final 与掉血一致（真实红伤落地）");
        std::cout << "  [E] miss×无效趟改写(2501 同构): 伤害=" << dmg << "（例外路径保留）\n";
    }

    // ══ F miss 趟 × 单击载体（拍打）：零伤害（既有行为，K424 隔离对照）══
    {
        ScenarioBattle b(make_party(combo_attacker({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(plain_target("闪避目标", 2, make_dodge_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = foe_hp(ctx);
        run_cast(b, /*seed=*/3);
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID,
                 "F) miss 前提：技能 SKILL_INVALID");
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 1, "F) 单击载体连击基数==1");
        CHECK_EQ(hp0 - foe_hp(ctx), 0, "F) 单击 miss 零伤害（既有行为零变化）");
        std::cout << "  [F] miss×单击: 零伤害\n";
    }

    std::cout << "scenario_8180 PASS (R8 连击 miss 结算勘误：2x2 矩阵 + 2501 例外 + 单击对照)\n";
    return 0;
}
