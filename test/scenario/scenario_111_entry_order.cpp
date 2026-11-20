// 场景 111 —— 登场先后规则（2026-09-22，涂蝶砥砺 vs 百鬼转化视频 + 语料 idx=128/349）
//
// 官方规则（用户实测口径）：
//   ① 首回合双方中切**强制房主先登场**，不看点击/提交顺序（视频：无论怎么切，
//      房主侧涂蝶先挂砥砺 → 挑战方百鬼登场转化才吃得到）；
//   ② 第二回合起双方中切按**提交顺序**（先切先登场）——百鬼只有后切才能转化
//      先登场者挂的砥砺（它的转化只判定一次）；
//   ③ 部署/PROTECTION_1 等共享桶按 房主→挑战方 序执行（角色位 host_side 可翻转）。
//
// 探针：测试侧直接挂 EVENT_ENTER_STAGE watcher（记 ENTER 顺序）+
// OPERATION_ENTER_EXIT_STAGE / OPERATION_PROTECTION_MECHANISM_1 桶效果（记执行顺序）。

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10162;

std::array<int, 5> same5(int id = kHit) { return {id, id, id, id, id}; }

// 顺序探针（deploy/PROTECTION_1 桶效果用）
static std::vector<int> g_bucket_seq;
EffectResult probe_effect(BattleContext* ctx, const EffectArgs& args) {
    if (ctx && args.int_args && args.int_count >= 1) {
        g_bucket_seq.push_back(args.int_args[0]);
    }
    return EffectResult::kOk;
}

// ENTER 顺序探针（watcher 用）
static std::vector<int> g_enter_seq;

void register_probe_effects(BattleContext& ctx) {
    g_bucket_seq.clear();
    for (int owner = 0; owner <= 1; ++owner) {
        for (State state : {State::OPERATION_ENTER_EXIT_STAGE,
                            State::OPERATION_PROTECTION_MECHANISM_1}) {
            Effect e(991100, 0, 1, 2, EffectArgs(std::vector<int>{owner}), &probe_effect);
            auto ce = std::make_unique<ContinuousEffect>(e, state, owner, 2, ctx.roundCount);
            ce->source_id_ = 991100 + owner;
            ctx.register_skill_effect(state, owner, std::move(ce));
        }
    }
}

void register_enter_watchers(BattleContext& ctx) {
    g_enter_seq.clear();
    for (int side = 0; side <= 1; ++side) {
        ctx.event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, side, ctx.roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [side](BattleContext*, const BattleEvent& ev) {
                if (ev.actor == side) {
                    g_enter_seq.push_back(side);
                }
            },
            WatcherScope::TEAM);
    }
}

ElfPet plain(int speed = 100) {
    return make_pet(1, "宠", same5(), speed, /*atk=*/1, 9000);
}

void play_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A 部署桶/PROTECTION_1 按 房主→挑战方（角色位可翻转）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(plain()), make_party(plain()));
        register_probe_effects(b.ctx());
        b.start();
        // PROTECTION_1 在首轮 CHOOSE 之后 → drive 一轮
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(g_bucket_seq.size(), 4u, "A) 前置：两个桶各两条探针");
        CHECK(g_bucket_seq[0] == 0 && g_bucket_seq[1] == 1,
              "A1) ★ 部署桶（登场时效果）按 房主→挑战方 执行");
        CHECK(g_bucket_seq[2] == 0 && g_bucket_seq[3] == 1,
              "A2) ★ PROTECTION_1 同样 房主→挑战方");
        std::cout << "  [A] 部署/PROTECTION_1 角色序 通过\n";
    }
    {
        // 角色翻转：host_side=1 → 序反转
        ScenarioBattle b(make_party(plain()), make_party(plain()));
        b.ctx().host_side = 1;   // 1 号方当房主
        register_probe_effects(b.ctx());
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(g_bucket_seq.size() == 4 && g_bucket_seq[0] == 1 && g_bucket_seq[1] == 0,
              "A3) ★ host_side=1（1 号方房主）→ 部署桶先跑 1 号方的登场效果");
        std::cout << "  [A'] 角色位翻转 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B 首回合双中切：挑战方先提交 → 仍强制房主先登场
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(plain()), make_party(plain()));
        b.start();
        register_enter_watchers(b.ctx());
        // 挑战方（1 号）先提交切换，房主（0 号）后提交
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(g_enter_seq.size(), 2u, "B) 前置：两次登场");
        CHECK(g_enter_seq[0] == 0 && g_enter_seq[1] == 1,
              "B1) ★ 首回合双中切**强制房主先登场**（不看提交顺序——挑战方先点也被缓冲）");
        std::cout << "  [B] 首回合双切·房主先 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C 首回合房主先提交 → 自然房主先（对照）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(plain()), make_party(plain()));
        b.start();
        register_enter_watchers(b.ctx());
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(g_enter_seq.size(), 2u, "C) 前置：两次登场");
        CHECK(g_enter_seq[0] == 0 && g_enter_seq[1] == 1, "C1) 房主先提交 → 房主先登场");
        std::cout << "  [C] 首回合房主先提交 对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D 第二回合起双中切按提交顺序：挑战方先切 → 挑战方先登场（百鬼后切规则）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(plain()), make_party(plain()));
        b.start();
        register_enter_watchers(b.ctx());
        play_round(b);   // R1：普攻回合，把首回合过掉
        // R2：双中切，挑战方（1 号）先提交 → 先切先登场
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(g_enter_seq.size(), 2u, "D) 前置：两次登场");
        CHECK(g_enter_seq[0] == 1 && g_enter_seq[1] == 0,
              "D1) ★ 第二回合起按**提交顺序**（先切先登场——百鬼须后切才转化砥砺）");
        std::cout << "  [D] 次回合双切·提交序 通过\n";
    }

    std::cout << "scenario_111 PASS（登场先后：首回合强制房主先 / 次回合提交序 / 角色位翻转）"
              << std::endl;
    return 0;
}
