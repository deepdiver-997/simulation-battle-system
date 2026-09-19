#ifndef SERVER_LEGAL_ACTIONS_H
#define SERVER_LEGAL_ACTIONS_H

// 合法动作集：此刻这个玩家能做什么。
//
// 为什么值得单独做一层：内核里"这个技能能不能点"的判断**早就有了**
// （Skills::skill_usable / query_selectable、ElfPet::is_locked、medicines 数量），
// 只是从来没往客户端发过。发出去之后前端只渲染合法按钮，玩家验证时不会把
// "我点了没反应"当成本身是 bug —— 而这类误报在开放给玩家验证时是最耗时的噪声。
//
// ⚠️ 本文件里的函数必须**只读**：它们会在战斗状态被查询时调用，
//    一旦有副作用就会改变战斗结果。所用的内核接口（skill_usable / query_selectable）
//    已确认无副作用。

#include <string>
#include <vector>

class BattleContext;

namespace server {

struct LegalSkill {
    int index = -1;
    int id = -1;
    std::string name;
    int pp = 0;
    int max_pp = 0;
    bool usable = false;
    std::string reason;  // usable=false 时说明原因（pp_empty / locked / ...）
};

struct LegalPet {
    int slot = -1;
    int id = -1;
    std::string name;
    int hp = 0;
    int max_hp = 0;
    bool on_stage = false;
    bool usable = false;
    std::string reason;  // fainted / locked
};

struct LegalMedicine {
    int index = -1;
    int type = -1;  // MedicineType 整数值
    int count = 0;
    bool usable = false;
};

struct LegalActions {
    // can_act=false 时下表为空，reason 说明为什么轮不到他（not_waiting / not_your_turn）。
    bool can_act = false;
    std::string reason;

    // 当前是否处于"必须换宠"（CHOOSE_AFTER_DEATH）而不是"选技能"。
    // 前端据此切界面 —— 这两种状态下合法动作集完全不同。
    bool must_choose_pet = false;

    std::vector<LegalSkill> skills;
    std::vector<LegalPet> pets;
    std::vector<LegalMedicine> medicines;
};

// 该玩家此刻是否"必须换宠"（CHOOSE_AFTER_DEATH 且他自己的场上精灵倒了）。
// 与 BattleContext::getStateJson 的 needDeathSwitch 同口径，供服务端决定该提示谁。
bool player_must_switch_pet(BattleContext& ctx, int player);

// 该玩家此刻是否存在**任何**合法动作。
// false 意味着引擎把对局卡在了一个无法满足的输入要求上（已知场景：一方精灵全灭后
// 仍要求在"死后选择"里换宠，而没有任何可换的精灵）。服务端据此兜底，不把玩家挂死。
bool has_any_legal_action(const LegalActions& legal);

// 只读查询。player 取 0/1。
//
// 签名取非 const 引用：内核的 skill_usable / query_selectable 是历史非 const 签名
// （内部只读，不改任何状态）。与其为它们加一层 const_cast 包装，不如把"只读"
// 这件事写在这里的约定上，并靠"合法动作集不改变战斗结果"的回归测试守住。
LegalActions compute_legal_actions(BattleContext& ctx, int player);

}  // namespace server

#endif  // SERVER_LEGAL_ACTIONS_H
