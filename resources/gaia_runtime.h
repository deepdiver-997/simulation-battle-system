#ifndef GAIA_RUNTIME_H
#define GAIA_RUNTIME_H

// 圣甲·盖亚 3056 运行时 —— 魂印"狂"（soul_lib/gaia_kuang_3056.cpp）与技能侧共享的
// 键约定。放独立头：场景测试 096 也要按同一类型读盘面（any 按类型存取）。
//
// 官方魂印（4399 图鉴 606894）：
//   狂：受到致命攻击时反弹对手造成的攻击伤害（双方同时死亡时自己残留1点体力）；
//       每回合恢复1/3最大HP。
// 关键口径（用户 2026-09-20 拍板 + 语料 opus 965872689626480658《机制解析—圣甲盖亚》）：
//   ★ 弹伤由**致死检测**触发，在死亡判定（defeat_pet 拦截器层）里结算：先给对面造成
//     等量自己刚受到的红伤数值的**真实伤害**，再检测对面死没死；死了 → 自己保留 1 点体力。
//   ★ 弹的档位：攻击红伤（NORMAL）+ 属性直伤（ATTRIBUTE）；粉伤/真伤斩杀**不弹**
//     （语料：回合结束粉刚好斩杀机盖不会弹；克雷/空元的二段粉伤不被弹）。
//   ★ 弹伤时点随攻击方出手位自然成立：死亡漏斗在 FIRST/SECOND_MOVER_DEATH 两个时点跑，
//     攻击方先手 → 先手方出手流程结束判死；攻击方后手 → 第二时点（回合结束段）判死。
//   ★ 封回血翻转：盖亚身上有封回血（HEAL_BLOCK 免疫覆盖当前时点）→ "保留 1 点体力"
//     拉不起来，盖亚先死；随后**对方的死亡判定**里反同归机制给对方拉 1（同样走
//     heal 通道、同样被封回血约束）——两次触发都是同一个拦截器。
//   ★ 每回合恢复 1/3 最大 HP：需要**本次在场出手过**（SELECT_SKILL 才算，切换/嗑药/
//     被控不算）才激发；走 heal_amount 原语 → 天然吃封回血/恢复修正。
#include <any>

#include <entities/elf-pet.h>

// ── ElfPet::soulmark_storage（下场保留）：拦截器只挂一次的守卫句柄（int）──
constexpr int kGaiaInterceptorGuardKey = 3056;

// ── ElfPet::on_stage_storage（本次上场，换宠清空）：出手过梄（int，1=出手过）──
constexpr int kGaiaOnStageActedKey = 3057;

// ── BattleContext::plugin_storage：反同归待决旗（int 槽，跨拦截器调用可见）──
// 键 = kGaiaPubReflectBase + 被弹方 side，值 = 盖亚方 side。
// 语义："该方被盖亚的死亡弹伤弹到 0 血、且盖亚的保留-1-点未成功"——等它的死亡判定
// 来询问时给拉 1。盖亚留 1 成功或对面没被弹死时立即撤销。
constexpr int kGaiaPubReflectBase = 3056100;
// 路径 A 同步死亡判定求解中标记（+盖亚方 side）：期间路径 B 不抢答，对面的免死先应答。
constexpr int kGaiaResolvingKey = 3056200;

// ── 魂印/系别常量 ──
constexpr int kGaiaPetId = 3056;
// ⚠️ 注册键 = effect_icon.effect_id = 736（装配层 SoulMark.id 按 effect_id 查程序表）。
//    3056 是 monster id（kGaiaPetId），不能作注册键——错键会让程序在真实对局挂不上
//    （2026-09-27 修复；见 unity 库 effect_icon 410 行 [3056]→736）。
constexpr int kGaiaMarkId = 736;

inline int gaia_interceptor_guard(const ElfPet& pet) {
    auto it = pet.soulmark_storage.find(kGaiaInterceptorGuardKey);
    return it == pet.soulmark_storage.end() ? 0 : std::any_cast<int>(it->second);
}

inline void gaia_set_interceptor_guard(ElfPet& pet, int id) {
    pet.soulmark_storage[kGaiaInterceptorGuardKey] = id;
}

inline bool gaia_has_acted_on_stage(const ElfPet& pet) {
    auto it = pet.on_stage_storage.find(kGaiaOnStageActedKey);
    return it != pet.on_stage_storage.end() && std::any_cast<int>(it->second) != 0;
}

inline void gaia_set_acted_on_stage(ElfPet& pet) {
    pet.on_stage_storage[kGaiaOnStageActedKey] = 1;
}

#endif  // GAIA_RUNTIME_H
