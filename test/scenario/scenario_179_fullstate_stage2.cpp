// 场景 179 —— 控制台状态显示 Stage 2 数据面：时点桶条目增强 + RuleCenter 票导出
// （开工文档 docs_local/docs/07-工具与测试/控制台状态显示-开工.md §三 Stage 2）
//
// 锁的是 getFullStateJson 的数据面契约（§2.1 只传 id/数值，名字客户端解析）：
//   A RuleCenter 票全量导出：亮节闸门票（category=15 / thresholdPct=50 / belowResult=0 /
//     sourceEffectId=2436 / target=持有方 / expired=false）——场景 103 H1 的数据面镜像；
//   B 桶条目增强字段：sourceId（§2.2 分组折叠锚）/ scope / once / actionOneShot /
//     invalidated / duration——渎神臣服窗口（1814）在桶里"可见可读"（场景 139 T7a 镜像）；
//   C 全量 JSON 合法性：写盘（SBS_FULLSTATE_OUT 可改，默认 /tmp）给 python -m json.tool 复验。
//
// 机制口径见 include/effects/rule_center.h（CHANCE_GATE 段）、
// include/effects/continuousEffect.h（isActionOneShot 段）。

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "../../resources/chance_gate_lib.h"
#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kDushenSkill = 38020;  // 极渊·渎神⑤：1237 双方 PP 归零 + 43 回复 + 1814 臣服窗口
constexpr int kHit = 10004;          // 拍打：物攻 40 必中无效果
constexpr int kSubmission = static_cast<int>(AbnormalStatusId::Submission);

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

// DS-001 白板壳（不挂魂印插件，隔离被测面；同场景 139 的 ds001 助手）。
ElfPet ds001(std::array<int, 5> skills, int atk = 300, int hp = 3000) {
    ElfPet p = scenario::make_pet(0, "极渊DS-001", skills, /*speed=*/200, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;      // 凭证短路：必中不掷命中
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(int id, const char* name, int speed, int hp) {
    return scenario::make_pet(id, name, {kHit, kHit, kHit, kHit, kHit}, speed, 1, hp);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== A 亮节闸门票：票面 id+数值在 fullstate 可见可读 ==========
    std::string dump;
    std::string window_dump;   // B 组采样（含桶条目），供 C 组写盘复验
    {
        ScenarioBattle b(scenario::make_party(plain_pet(0, "攻", 200, 3000)),
                         scenario::make_party(plain_pet(1, "守", 100, 4000)));
        b.start();
        chance_gate::grant_liangjie(&b.ctx(), 1, 0);   // 守方戴亮节（永久窗，授权方 revoke）
        dump = b.dump();
        CHECK(contains(dump, "\"ruleTickets\":["), "A fullstate 应含 ruleTickets 数组");
        CHECK(contains(dump, "\"category\":15"), "A 应含 CHANCE_GATE（category=15）票");
        CHECK(contains(dump, "\"thresholdPct\":50"), "A 亮节阈值 50");
        CHECK(contains(dump, "\"belowResult\":0"), "A 亮节 ≤50% → 0");
        CHECK(contains(dump, "\"aboveResult\":-1"), "A 亮节只压不抬（>50 不改写）");
        CHECK(contains(dump, "\"sourceEffectId\":2436"), "A 来源效果=无极圣武魂印 2436");
        CHECK(contains(dump, "\"target\":1"), "A 闸门挂被保护方（target=1）");
        CHECK(contains(dump, "\"expired\":false"), "A 窗口内不过期");
        std::cout << "  [A] 亮节票导出 通过\n";
    }

    //========== B 渎神臣服窗口：桶条目增强字段 + 1814 可见可读 ==========
    {
        ScenarioBattle b(scenario::make_party(ds001({kDushenSkill, kHit, kHit, kHit, kHit})),
                         scenario::make_party(plain_pet(1, "对手", 100, 2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 停在对手选技能
        // 手动灌对手操作后单步推进到"先手技能效果已执行"的下一时点（红伤阶段起）：
        // 此刻渎神 1237/43/1814 刚在 SKILL_EFFECT 注册进桶，在途/窗口条目正可观察。
        // ⚠️ 不能用 submit_operation(1)+step_until：submit 会把整回合非单步跑完，
        // step_until 会在等待输入的状态上无限空转（need_input 防护也一并加上）。
        {
            ctx.debug_step_mode = true;
            const int buf[4] = {1, static_cast<int>(BattleFsm::ActionType::SELECT_SKILL), 0, 0};
            ctx.m_buffer.assign(reinterpret_cast<const char*>(buf),
                                reinterpret_cast<const char*>(buf) + 4 * sizeof(int));
            ctx.is_empty = false;
            while (ctx.currentState != State::BATTLE_FIRST_ATTACK_DAMAGE
                   && ctx.currentState != State::FINISHED
                   && !ctx.need_input()) {
                b.step();
            }
        }
        const std::string mid = b.dump();
        // 窗口语义本体照常（场景 139 T7a 同款断言）：臣服已挂上
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, kSubmission), "B 1814 窗口首回合照常：100% 令对手臣服");
        const std::string post = b.dump();
        const std::string& view = contains(mid, "\"effectId\":1814") ? mid : post;
        CHECK(contains(view, "\"effectId\":1814"), "B 渎神臣服窗口（1814）在桶里可见");
        CHECK(contains(view, "\"sourceId\":"), "B 桶条目带 sourceId（§2.2 分组锚）");
        CHECK(contains(view, "\"actionOneShot\":"), "B 桶条目带 actionOneShot（在途节点标记）");
        CHECK(contains(view, "\"invalidated\":"), "B 桶条目带 invalidated（作废态）");
        CHECK(contains(view, "\"duration\":"), "B 桶条目带 duration（剩余回合客户端自算）");
        CHECK(contains(view, "\"scope\":"), "B 桶条目带 scope（ON_STAGE/TEAM）");
        // 客户端渲染路径演练：在 1814 条目邻域取片断，确认 sourceId/invalidated 成对出现
        const auto pos = view.find("\"effectId\":1814");
        CHECK(pos != std::string::npos, "B 1814 条目定位");
        const std::string around = view.substr(pos, 400);
        CHECK(contains(around, "\"sourceId\":"), "B 1814 条目邻域含 sourceId");
        CHECK(contains(around, "\"invalidated\":"), "B 1814 条目邻域含 invalidated");
        window_dump = view;
        std::cout << "  [B] 渎神臣服窗口 + 桶条目增强 通过\n";
    }

    //========== C fullstate 写盘：交 python -m json.tool 验 JSON 合法性 ==========
    {
        const char* out = std::getenv("SBS_FULLSTATE_OUT");
        const std::string base = out ? out : "/tmp/sbs_fullstate_179.json";
        for (const auto& [path, content] : std::initializer_list<std::pair<std::string, const std::string&>>{
                 {base, dump}, {base + ".window.json", window_dump}}) {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            file << content;
            file.close();
            CHECK(static_cast<bool>(file), "C fullstate 写盘成功：" + path);
        }
        std::cout << "  [C] fullstate 已写 " << base << "（python -m json.tool 复验）\n";
    }

    std::cout << "场景 179（Stage 2 数据面：桶增强 + RuleCenter 票）全部通过\n";
    return 0;
}
