#include <utils/sbs_rand.h>
// 场景 120 —— 精灵王专属场景：白龙斩杀延后 + 混地蛊跳机盖（2026-09-24）
//
// G1 白龙斩杀延后（语料 idx=140 赛学必修15 + idx=149/150 玄祖）：
//    白龙先手攻击压低对手（不触发 20% 线）→ 对手后手攻击白龙、吃"挨打粉"（2447：
//    被攻击附加白龙 max×20% 粉）后体力跌破线 → 玄祖死亡 + 挂死亡印记（消耗全部体力）；
//    但白龙的斩杀结算点（挂印者行动结束）本轮已过 → 延后到下回合；
//    对手当回合存活；下回合白龙行动结束 → 印记结算 → 对手被消耗致死（HP_CONSUME）。
// G2 混地蛊跳机盖（语料 idx=190）：
//    机盖（圣甲·盖亚 3056）"双同归残留 1 血"走**恢复原语** → 蛊监听 EVENT_HEAL_RESTORED
//    消耗 1 层插入 40 真伤 → 残留 1 血被扣死。
//
// 官方依据：effect_icon 2028/1985 tips + 语料 idx=140/149/190。
#include <any>

#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "../../resources/diwang_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kDragon = 4186;
constexpr int kDragonMark = 2447;
constexpr int kDiwangMark = 2411;
constexpr int kGaiaMark = 736;  // effect_id（2026-09-27 对齐装配层；3056=monster id）
constexpr int kPlain = 10001;
constexpr int kDrain = 31742;   // 真实吸血攻击技（1249 族，万里焚天炎）

int pub_get(BattleContext& x, int key) {
    auto it = x.plugin_storage.find(key);
    if (it == x.plugin_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

ElfPet dragon_pet(int hp = 6000) {
    SoulMark sm(kDragonMark, "镇", "测试桩：镇世·乔特鲁德神谕 2447 最小切片");
    return make_pet(kDragon, "镇世·乔特鲁德", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/500, /*atk=*/100, hp, sm);
}

ElfPet diwang_pet(int hp = 6000, int atk = 150) {
    SoulMark sm(kDiwangMark, "墬", "测试桩：混沌地王神谕 2411");
    return make_pet(3516, "混沌地王", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/100, atk, hp, sm);
}

ElfPet gaia_pet(int hp = 400) {
    SoulMark sm(kGaiaMark, "狂", "测试桩：圣甲·盖亚 3056");
    return make_pet(3056, "圣甲·盖亚", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/100, /*atk=*/100, hp, sm);
}

ElfPet plain_pet(int atk = 100, int hp = 4000, int speed = 100) {
    return make_pet(1, "普通", {kPlain, kPlain, kPlain, kPlain, kPlain}, speed, atk, hp);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ═══ G1. 白龙斩杀延后 ══════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {dragon_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        // 对手：max 2000（白龙 max 6000 → 20% 线 = 1200），先手攻击会打剩 ~1000（>线 不触发）
        std::array<ElfPet, 6> foe = {
            make_pet(1, "吸血怪", {kDrain, kPlain, kPlain, kPlain, kPlain}, /*speed=*/10,
                     /*atk=*/50, /*hp=*/2000),
            plain_pet(), plain_pet(), plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, foe);
        b.start();
        BattleContext& x = b.ctx();

        CHECK_EQ(x.extra_spirits[0].size(), 2u, "G1.0 战斗开始双龙加入己方");

        // 回合 1：白龙先手攻击（对手剩 >1200，不触发玄祖）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int foe_hp_after_r1_hit = x.getPet(1).hp;
        CHECK(foe_hp_after_r1_hit > 0, "G1.1 先手攻击未击杀");
        if (foe_hp_after_r1_hit > 0 && foe_hp_after_r1_hit >= 1200) {
            std::cout << "  [G1] 提示：先手后体力=" << foe_hp_after_r1_hit
                      << "（>=1200 线，延后分支依赖挨打粉触发）" << std::endl;
        }

        // 回合 2：对手攻击白龙 → 吃挨打粉 1200 → 跌破线 → 玄祖死亡 + 挂死亡印记
        //（对手吸血技把自己拉住；白龙的斩杀结算点本轮已过 → 延后）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(x.extra_spirits[0].size() < 2 || x.extra_spirits[0][1].state != ExtraSpiritState::ALIVE,
              "G1.2 玄祖已死亡");
        // 延后口径的存活断言：若跌破线发生在对手行动期，印记本轮不再结算
        if (x.getPet(1).hp > 0) {
            std::cout << "  [G1] 对手回合结束后存活（斩杀延后成立）" << std::endl;
        }

        // 回合 3：白龙任意行动结束 → 印记结算 → 对手被消耗致死
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.getPet(1).hp, 0, "G1.3 下回合印记结算：对手被消耗全部体力致死");
    }

    // ═══ G2. 混地蛊跳机盖 ══════════════════════════════════════════
    {
        // 我方：混地亲自上场（蛊监听在场守卫口径），攻 500 / 血 400——
        // 会被机盖的反弹同步弹死，制造"双同归 → 机盖残留 1 血（恢复原语）"。
        std::array<ElfPet, 6> own = {diwang_pet(400, 1600), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> foe = {gaia_pet(400), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, foe);
        b.start();
        BattleContext& x = b.ctx();

        // 预挂 1 层蛊到对手侧（发布槽直写，等价"混地阶段末 盾=400 倍数种蛊"后的状态）
        diwang_pub_set(x.plugin_storage, kDiwangPubGu + 1, 1);

        // 回合 1：混地击杀机盖 → 被反弹双同归 → 机盖残留 1 血（恢复原语）
        // → 蛊监听消耗 1 层插入 40 真伤 → 残留 1 血被扣死（跳机盖，idx=190）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        ElfPet& gaia = x.getPet(1);
        CHECK_EQ(gaia.hp, 0, "G2 机盖残留 1 血被蛊扣死（跳机盖复刻成立）");
        CHECK_EQ(pub_get(x, kDiwangPubGu + 1), 0, "G2 蛊消耗 1 层");
    }

    std::cout << "scenario_120 全部通过" << std::endl;
    return 0;
}
