#include <utils/sbs_rand.h>
// G 组 · G6 PP 受击族 —— 780×29（2026-09-29，worktree ../agents/g6）
//
// 780（args_num=3）"{0}回合内受到攻击则{1}%令对手随机{2}个技能PP值归零"：
//   - 直系 = F10 716（f_misc_family.cpp effect_716_pp_zero_window）：受击 watcher
//    （EVENT_TAKE_DAMAGE + damage_kind==NORMAL 红伤判据 + TEAM scope——817/716 同款）
//     → **roll_percent({1}) 概率门**（与 716 的唯一差异）→ 随机 {2} 个技能槽 pp 归零
//    （pp_zero_slot 原语 + 部分 Fisher-Yates 抽不重复槽位，716 逐字复用）。
//   - 官方字面省略"自身"主语，窗口挂在自身上（对手的攻击打过来即触发）；与 716 一样
//     载体几乎全是属性技（自身不掉血也照样挂窗——"受到攻击"= 挨红伤，属性技对手打不动
//     自己时本效果本就不触发，符合字面）。唯一攻击技载体 35816 降维光线同 handler。
//   - 粉伤 tick/真伤/属性直伤不算"受到攻击"（E12 damage_kind 口径，817/716 同判据；
//     与 scenario_660 T1 同一过滤分支，粉伤对照记档不单测）。
//
// 载体切片（DB moves.side_effect/side_effect_arg，side_effect.arg_count 切分，
// 2026-09-29 查库；全 29 条切片精确耗尽 arg 数组——4399 原文交叉验证逐字吻合，
// 如 24119 不灭天尊 [57,780,60]|[4,3,3,50,1,5,120] → 780 段 [3,50,1]：
// 战神·盖亚/王·盖亚"3回合内若受到攻击则有50%概率使对手携带的1个技能PP归零"）：
//   24119/24409 不灭天尊 [3,50,1]、24855 不休禁锢 [3,100,1]、25264 协奏灵曲 [3,50,1]、
//   25402 疾风劲草 [3,50,1]、25546 源生动力 [3,100,1]、25599 万物生长 [3,100,2]、
//   25997 兽魂觉醒 [3,100,1]、26241 不朽剑神 [3,100,1]、26339 天蝎守护 [3,100,1]、
//   26491 扣人心弦 [3,100,2]、26577 冰霸霸之魂 [3,50,1]、26682 赛米星之灵 [3,100,2]、
//   26772 覆影迷境 [3,100,2]、26903 俗尘清梦 [3,100,1]、27153 清溪泾流 [3,100,1]、
//   27219 盛勇无畏 [3,100,1]、27231 粽叶飘香 [3,100,1]、27307 山菊信风 [3,100,1]、
//   27400 静养生息 [4,100,1]、27440 美好憧憬 [3,100,1]、27602 稻香之息 [3,100,1]、
//   27813 万棱腹甲 [3,100,2]、27845 翡翠碧绿 [3,100,2]、27880 柱神祈愿 [3,100,1]、
//   28194 皑皑之影 [4,50,2]、28348 踉踉跄跄 [3,50,1]、28626 机械原理 [4,100,1]、
//   35816 降维光线 [3,50,1]（物攻 120，唯一非属性载体）
//   （35743 鬼焰·焚身术是 side_effect LIKE '%780%' 子串误报，实际 1780——非载体）
//
// 动工前三查（2026-09-29）：①内核 grep 无 780 硬编码（src/ include/ case 分支零命中）
// ②effect_meta 无牵连分类、无 custom_effect_overrides ③moves_lib 无既有注册
//（SKILL_EFFECT/CARRY_EFFECT(780) 零命中；F10 716 是不同 id、注册不冲突）——真缺口。
//
// 场景回归：test/scenario/scenario_760_pp_onhit.cpp（100% 切片 25599 精确计数 +
// 50% 门 24119 多局种子扫描统计带 + 不受击对照）。

#include "common.h"

#include <numeric>
#include <vector>

namespace {

// ════════════════════════════════════════════════════════════════════
// 780 —— {0}回合内受到攻击则{1}%令对手随机{2}个技能PP值归零
// 挂靠：受击 watcher（F10 716 逐字 + roll_percent({1}) 概率门）。清 PP 走
// pp_zero_slot 原语（emit EVENT_PP_REDUCED；直写 pp 会绕过事件）；随机 N 项 =
// 部分 Fisher-Yates 抽不重复槽位（对手在场宠技能槽数 < {2} 时取全部）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_780_onhit_pp_zero_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 780, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 780, args,
                     "780 布局=[owner,target,rounds,chance,count]（int_count>=5）");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int count = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && rounds > 0 && count > 0, 780, args,
                     "需要 rounds>0 且 count>0");
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, chance, count](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner) {
                return;
            }
            // "受到攻击" = 红伤（E12 口径；粉伤 tick/真伤/属性直伤不算）。
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;
            }
            const int attacker = ev.actor;
            if (attacker < 0 || attacker > 1 || attacker == owner) {
                return;
            }
            if (!roll_skill_percent(*c, 780, 0, owner, attacker, chance)) {
                return;   // {1}% 概率门（与 716 的唯一差异）
            }
            if (!g_core || !g_core->pp_zero_slot) {
                return;
            }
            const int slots = static_cast<int>(c->getPet(attacker).skills.size());
            if (slots <= 0) {
                return;
            }
            std::vector<int> idx(static_cast<std::size_t>(slots));
            std::iota(idx.begin(), idx.end(), 0);
            const int n = std::min(count, slots);
            for (int i = 0; i < n; ++i) {
                const int j = choose_skill_range(*c, 780, 1, owner, attacker, i, slots - 1, ChanceSingleton::SkipDraw);
                std::swap(idx[static_cast<std::size_t>(i)],
                          idx[static_cast<std::size_t>(j)]);
                (void)g_core->pp_zero_slot(c, attacker, idx[static_cast<std::size_t>(i)],
                                           owner);
            }
        },
        WatcherScope::TEAM);   // 跨回合 watcher 必须 TEAM（常识速查 §五）
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(780, effect_780_onhit_pp_zero_window);
