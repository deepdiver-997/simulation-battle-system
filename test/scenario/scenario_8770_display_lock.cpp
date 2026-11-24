// 场景 8770 —— 效果显示 G2 动态显示锁（规范 docs/07-工具与测试/效果显示gate规范.md §4 G2）
//
// 锁的是"驻留显示态必须能从引擎状态导出"的动态契约：全部走真实技能路径
// （28931/28809/38020 官方切片驱动），手写应显示清单，逐条断言 fullstate/快照含之。
// 场景 179 锁的是字段形状（票面/桶条目增强字段长什么样）；本场景锁的是
// "每类驻留态至少一条真实条目可见可读"——序列化字段被摘除时这里必须转红（MUT 面）。
//
// 应显示清单（手写，规范 §1.1：只锁驻留显示态，一次性效果无此义务）：
//   D1 真实回合桶条目（register_round_effect 族，isRoundEffect=true 且
//      actionOneShot=false 判别——⚠️ G3 考古更正：G2 初版只查 isRoundEffect 会被
//      SKILL_EXEC 一次性在途节点误匹配（它们 duration=1/isRoundEffect=true））：
//      · 28809 魂劫骇咒 → 2076（4回合吸取窗口）/ 843（下2回合先制窗口）
//      · 38020 渎神     → 1814（臣服窗口）
//   D2 规则票：2148 免异常票（category=0 / counts=1 / scope=1 TEAM / expired=false，
//      载体 28931 算力拆解，side_effect_arg=[...|1]）；另 48 古代纯免也是纯票
//      （grant_immunity，不进桶）
//   D3 印记 Mark：2211 蚩庸之锁（id=0 / name="蚩庸之锁" / count=1，载体 28809，
//      side_effect_arg=[...|1,2,1]；挂对手场上宠 pet.marks）
//   D4 异常状态：38020 渎神 → 对手臣服（Submission=31）——fullstate 与快照
//      （getStateJson）两出口都锁
//   D5 JSON 合法性：fullstate/快照写盘交 python -m json.tool 复验
//   D6 快照效果摘要（G3）：快照 effects.round（2076/843/1814，remaining 现算）、
//      effects.tickets（2148/48）、effects.watchers（1151/1311——j5_arm 族的"N回合
//      窗口"住 event_center_ 不住桶，G3 考古新收编的驻留显示态）、marks（蚩庸之锁，
//      场上宠）——摘段转红
//   D7 storage 显示身份（G4）：r2 后 plugin_storage 蚩庸之锁发布槽（4676001，方1=对手）
//      name="蚩庸之锁层数(方1)" value="1"（快照渲染面 + fullstate 审计面 registered:true）；
//      暴击充能（内核种子 990238000/990238001）未挂值时不出现在快照（map 无键）
//   D8 watcher 归属（G4 债务 #8）：watchers 条目带 sourceEffectId（1151=1151/1311=1311，
//      j5_arm 直调族已传；0=未归属存量）
//   （1338 全属性+1 等一次性强化不在清单——规范 §1.1 反例锚点）
//
// 机制口径见 resources/moves_lib/mistike_skills.cpp（2211/2148）、anomaly_channel.cpp（48）、
// j_attack_cond_window.cpp（1151）、k419_meiheweite_pool.cpp（1311）；
// 显示契约见 include/effects/continuousEffect.h（isActionOneShot 判别）与
// src/fsm/battleContext.cpp（getStateJson/getFullStateJson 序列化）。

#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include <abnormal-system/abnormal-types.h>
#include <effects/event_center.h>
#include <effects/display_registry.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kSuanli = 28931;   // 算力拆解：[48, 1151, 1311, 2148]
constexpr int kHunjie = 28809;   // 魂劫骇咒：[1338, 2076, 843, 2211]
constexpr int kDushen = 38020;   // 渎神：1237/43/1814（臣服窗口）
constexpr int kHit = 10004;      // 拍打：物攻 40 必中无效果
constexpr int kSubmission = static_cast<int>(AbnormalStatusId::Submission);  // 31

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

// JSON 结构自检（D5 的机器内版本）：串感知的括号深度校验——depth 全程不回 0（除结尾）、
// 不为负、结尾恰好归零。G3 实测教训：子串 contains 断言抓不住括号失衡（effects 段
// 多/少一层 } 时场景照样绿），D5 写盘 + 外部 json.tool 才兜住；此检查把兜底内置。
void check_json_structure(const std::string& json, const std::string& what) {
    int depth = 0;
    bool in_string = false, escaped = false;
    for (const char c : json) {
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == '{' || c == '[') {
            ++depth;
        } else if (c == '}' || c == ']') {
            --depth;
            CHECK(depth >= 0, what + "：括号深度为负（多关了一层）");
        }
    }
    CHECK(depth == 0 && !in_string,
          what + "：括号不配对（结尾深度 " + std::to_string(depth) + "）");
}

// 桶条目邻域判别：`"effectId":<id>,` 的**任一**出现位置（同 id 可有多条：SKILL_EXEC
// 一次性节点 + 真窗口并存）后 window 字节内出现 `needle` 即算命中；全部不中才算失败
// （防 `"effectId":48,` 被 480/4812 等前缀误匹配、防字段顺序漂移、防一次性节点遮蔽真窗口）。
void check_entry_field(const std::string& dump, int effect_id, const std::string& needle,
                       const std::string& what) {
    const std::string key = "\"effectId\":" + std::to_string(effect_id) + ",";
    std::size_t pos = dump.find(key);
    bool hit = false;
    while (pos != std::string::npos) {
        if (contains(dump.substr(pos, 400), needle)) {
            hit = true;
            break;
        }
        pos = dump.find(key, pos + 1);
    }
    CHECK(hit, what + "：" + key + " 的任何条目邻域 400 字节内均未见 " + needle);
}

ElfPet attacker(int hp) {
    ElfPet p = scenario::make_pet(0, "显示锁壳", {kSuanli, kHunjie, kDushen, kHit, kHit},
                                  /*speed=*/300, /*atk=*/50, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;      // 凭证短路：必中不掷命中（场景 179 ds001 同款）
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_defender(int hp) {
    return scenario::make_pet(1, "对手", {kHit, kHit, kHit, kHit, kHit},
                              /*speed=*/100, /*atk=*/1, hp);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // 回合 1：算力拆解（D2 票 + D1a 三个回合窗口）
    std::string dump_r1;
    std::string snap_r1;
    {
        ScenarioBattle b(scenario::make_party(attacker(6000)),
                         scenario::make_party(plain_defender(9000)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 算力拆解
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 拍打
        b.drive();
        dump_r1 = b.dump();
        // D2 规则票：2148 免异常票（TEAM 传承，1 次）
        // 锚票对象起始 `{`（category 在 sourceEffectId 之前，往前找对象头）
        CHECK(contains(dump_r1, "\"ruleTickets\":["), "D2 fullstate 应含 ruleTickets 数组");
        const auto tp = dump_r1.find("\"sourceEffectId\":2148");
        CHECK(tp != std::string::npos, "D2 免异常票（sourceEffectId=2148）可见");
        const auto obj = dump_r1.rfind('{', tp);
        CHECK(obj != std::string::npos, "D2 票对象定位");
        const std::string ticket = dump_r1.substr(obj, 400);
        CHECK(contains(ticket, "\"category\":0"), "D2 票类别=免疫（category=0）");
        CHECK(contains(ticket, "\"counts\":1"), "D2 免疫次数=1（side_effect_arg 尾参）");
        CHECK(contains(ticket, "\"scope\":1"), "D2 TEAM 传承（scope=1）");
        CHECK(contains(ticket, "\"expired\":false"), "D2 窗口内不过期");
        // D1a（G3 更正）：48/1151/1311 的桶条目全是 SKILL_EXEC 一次性在途节点
        // （duration=1/actionOneShot=true），真正的窗口分别住在票（48）与 watcher
        // （1151/1311）里——本回合没有真桶窗口，桶断言移至 r2/r3 的真窗口载体。
        // D6 快照效果摘要（G3）：tickets 含 2148/48；watchers 含 1151/1311（j5_arm
        // 族窗口住 event_center_）；round 数组存在即可（本回合无真桶窗口）。
        snap_r1 = b.ctx().getStateJson();
        CHECK(contains(snap_r1, "\"effects\":{\"round\":["), "D6 快照应含 effects.round 数组");
        CHECK(contains(snap_r1, "\"watchers\":[{"), "D6 快照应含 effects.watchers 数组");
        {
            const std::string skill_used = "\"event\":"
                + std::to_string(static_cast<int>(EventType::EVENT_SKILL_USED));
            const auto wp = snap_r1.find(skill_used + ",");
            CHECK(wp != std::string::npos,
                  "D6 快照 watchers 含 EVENT_SKILL_USED 监听（1151/1311 窗口）");
            CHECK(contains(snap_r1.substr(wp, 300), "\"rounds\":3"),
                  "D6 快照 watcher 窗口 3 回合（1151/1311 side_effect_arg）");
            // D8 watcher 归属（G4）：1151 的窗口条目带 sourceEffectId=1151
            CHECK(contains(snap_r1, "\"sourceEffectId\":1151"),
                  "D8 快照 watcher 1151 归属效果 1151");
        }
        {
            const auto tk = snap_r1.find("\"tickets\":[");
            CHECK(tk != std::string::npos, "D6 快照应含 effects.tickets 数组");
            const std::string tsec = snap_r1.substr(tk, 400);
            CHECK(contains(tsec, "\"sourceEffectId\":2148"), "D6 快照 tickets 含 2148 免异常票");
            CHECK(contains(tsec, "\"sourceEffectId\":48,"), "D6 快照 tickets 含 48 古代纯免票");
        }
        std::cout << "  [r1] D2 免异常票 + D6 快照摘要（票/watcher） 通过\n";
    }

    // 回合 2：魂劫骇咒（D3 蚩庸之锁 + D1b 吸取/先制窗口）
    std::string dump_r2;
    {
        ScenarioBattle b(scenario::make_party(attacker(6000)),
                         scenario::make_party(plain_defender(9000)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 算力拆解（铺垫）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 魂劫骇咒
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        dump_r2 = b.dump();
        // D3 印记 Mark：对手场上宠 pet.marks 挂 蚩庸之锁（id=0，count=1）
        CHECK(contains(dump_r2, "\"name\":\"蚩庸之锁\",\"count\":1"),
              "D3 蚩庸之锁印记（id=0/count=1）在 party marks 可见");
        // D1b 真实回合桶条目（G3 更正判别）：2076 吸取窗口 / 843 先制窗口——
        // actionOneShot=false 才是真窗口（SKILL_EXEC 一次性节点会被 isRoundEffect 误匹配）
        check_entry_field(dump_r2, 2076, "\"actionOneShot\":false", "D1b 2076 吸取窗口（真窗口）");
        check_entry_field(dump_r2, 843, "\"actionOneShot\":false", "D1b 843 先制窗口（真窗口）");
        // D6 快照效果摘要：marks 段含蚩庸之锁 + round 段含 2076（锁挂对手场上宠，
        // 快照 marks 只出场上宠——2211 挂的正是防守方 on-stage，槽位一致）
        const std::string snap_r2 = b.ctx().getStateJson();
        CHECK(contains(snap_r2, "\"marks\":[{\"id\":0,\"name\":\"蚩庸之锁\",\"count\":1}]"),
              "D6 快照 marks 含蚩庸之锁（id=0/count=1）");
        check_entry_field(snap_r2, 2076, "\"remaining\":", "D6 快照 2076 条目带 remaining");
        // D7 storage 显示身份（G4）：对手（方1）的蚩庸之锁发布槽在快照渲染面可见可读
        CHECK(contains(snap_r2, "\"name\":\"蚩庸之锁层数(方1)\",\"kind\":\"custom\",\"value\":\"1\""),
              "D7 快照 storage 蚩庸之锁层数(方1)=1（STATE_DISPLAY 登记 + 通用解码）");
        // fullstate 审计面：registered:true（G4 前 key 是裸黑盒）
        CHECK(contains(dump_r2, "\"key\":4676001,\"container\":\"battle\",\"name\":\"蚩庸之锁层数(方1)\""),
              "D7 fullstate storage 蚩庸之锁发布槽 registered:true");
        std::cout << "  [r2] D3 蚩庸之锁 + D1b 两条回合窗口 + D6 快照摘要 + D7 storage 通过\n";
    }

    // 回合 3：渎神（D1c 臣服窗口 1814 + D4 异常两出口）
    {
        ScenarioBattle b(scenario::make_party(attacker(6000)),
                         scenario::make_party(plain_defender(9000)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);   // 渎神
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        BattleContext& ctx = b.ctx();
        CHECK(ctx.has_active_abnormal_status(1, kSubmission),
              "D4 机制面先确认：对手确已臣服（场景 139 同款口径）");
        const std::string dump = b.dump();
        // D1c 臣服窗口（register_round_effect 族，真窗口判别）
        check_entry_field(dump, 1814, "\"actionOneShot\":false", "D1c 1814 臣服窗口（真窗口）");
        // D4 异常状态——fullstate 出口
        CHECK(contains(dump, "\"id\":" + std::to_string(kSubmission)),
              "D4 fullstate abnormalStates 含臣服（id=31）");
        // D4 异常状态——快照出口（"立刻可渲染"层，G3 扩段不得回退）
        const std::string snapshot = ctx.getStateJson();
        CHECK(contains(snapshot, "\"abnormalStates\":["),
              "D4 快照应含 abnormalStates 数组");
        CHECK(contains(snapshot, "\"id\":" + std::to_string(kSubmission)),
              "D4 快照 abnormalStates 含臣服");
        // D6 快照效果摘要：臣服窗口 1814 进 round 段（窗口条目 remaining 现算）
        check_entry_field(snapshot, 1814, "\"remaining\":", "D6 快照 1814 条目带 remaining");
        std::cout << "  [r3] D1c 臣服窗口 + D4 异常两出口 + D6 快照摘要 通过\n";

        // D5 JSON 合法性：结构自检（内置）+ 写盘（外部 python -m json.tool 复验，
        // SBS_FULLSTATE_OUT 可改）
        for (const auto& [label, content] : std::initializer_list<std::pair<const char*, const std::string&>>{
                 {"r1 fullstate", dump_r1}, {"r1 snapshot", snap_r1},
                 {"r2 fullstate", dump_r2}, {"r3 fullstate", dump},
                 {"r3 snapshot", snapshot}}) {
            check_json_structure(content, std::string("D5 ") + label);
        }
        const char* out = std::getenv("SBS_FULLSTATE_OUT");
        const std::string base = out ? out : "/tmp/sbs_fullstate_8770.json";
        for (const auto& [path, content] : std::initializer_list<std::pair<std::string, const std::string&>>{
                 {base, dump}, {base + ".snapshot.json", snapshot},
                 {base + ".r1.json", dump_r1}, {base + ".r1.snapshot.json", snap_r1},
                 {base + ".r2.json", dump_r2}}) {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            file << content;
            file.close();
            CHECK(static_cast<bool>(file), "D5 写盘成功：" + path);
        }
        std::cout << "  [D5] fullstate/快照已写 " << base << "（python -m json.tool 复验）\n";
    }

    std::cout << "场景 8770（显示锁 G2-G4：桶/票/Mark/异常/快照/storage/watcher 归属）全部通过\n";
    return 0;
}
