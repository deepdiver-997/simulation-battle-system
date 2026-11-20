#ifndef LEICHEN_RUNTIME_H
#define LEICHEN_RUNTIME_H

// K448 · 雷辰点亮机运行时 —— soul 侧引擎（soul_lib/k448_leichen_engine.cpp，魂印 2376）
// 与 moves 侧两宏（moves_lib/k448_leichen_pool.cpp，2289/2290）共享的存储布局 + 键约定。
// 放独立头：场景 8360 也要按同一类型读盘面（跨 dylib 只走 fundamental type——
// int 的 typeinfo 全局唯一；crawford_runtime.h:8-13 同款纪律）。
//
// ── 官方口径（effect_icon row 1966 / effect 2376，args "3 1 1"，尾注（boss无效）全句覆盖）──
//   tips① 登场后 1~4 号位技能 ↔ 雷辰·东/南/西/北；使用以"雷"冠名技 → 点亮对应未亮雷辰。
//   tips② 每次点亮 = 与上一颗连接 + 令对手麻痹 + 自身所有技能 PP+1；
//         四颗全亮时：连线呈"ㄣ"形 → 回合结束解自身异常+解能力下降+觉醒 3 回合；
//         否则 → 回合结束回满体力 + 重置雷辰进度（重置=清位图/序列）。
//   tips③ 回合开始时自身麻痹 → 解除麻痹且下回合所有技能先制+1（tips 权威口径"下回合"；
//         analyze 段作"当回合"——两文出入按 SOP §六.1 tips 权威落地，头注留仲裁注记）。
//   tips④ 觉醒状态七子句 + 效果结束时重置雷辰进度。
//
// ── BWIKI/语料复核结论（2026-11-10，留档）──
//   · "连击"疑点：4399 pets_full.jsonl 4765 页「雷神·天明闪」原文与 BWIKI 页面均无任何
//     连击表述（"威力提升160点"为定点加算 + 未点亮保底两句）——J 组"连击变体"记档
//     系转述走样（调研文档 §1.4 判断成立），不涉 F10 段数原语。
//   · "以雷冠名"判定：BWIKI 未给细则 → 保守建模 = 技名**首字为"雷"**（"冠名"字面 =
//     冠在名前；4765 推荐套 1~4 号位 37906/29043/29044/37907 与第五技 37908 全部
//     雷字开头，机制自洽；嵌"雷"技（炽雷光刃/千羽雷葬/瞬影天雷斩）与无雷技
//     （神怒霆闪）不点亮。若有后续实测推翻，只改 leichen_named_lei() 一个函数）。
//   · "ㄣ"形枚举：BWIKI/4399 无几何定义 → 按 TapTap 配置帖实证序
//     「觉醒方式：1423或3241或4312或2134」（1-based 槽序，恰好 2 条无向路径 × 2 方向
//     = 单一手性族，与"连成单一 ㄣ 形"自洽；镜像族 1324/2143/3412/4231 非 ㄣ）。
//     仲裁口子 = kZhenAwakenOrders 单表，后续证据推翻时只改表。
//
// ── 存储布局（crawford_runtime.h 同构：跨 dylib 只存 int；自定义结构不跨插件）──
//   ★ 雷辰状态 → `ElfPet::on_stage_storage`（"本次上场"槽：perform_switch 引擎自动清
//     = "上下场刷新从头开始"零内核支持；主动中切与死亡换宠共同漏斗全覆盖）。
//     - kLeiChenStateKey（4440）→ int：bit0-3 = 东/南/西/北 点亮位图；
//       bit8..15 = 点亮顺序（每颗 2bit 低位在前，条数 = popcount(bit0-3)）。
//     - kLeiChenAwakenEndKey（4441）→ int：觉醒窗最后有效回合（0 = 无觉醒）。
//       进入回合 R 的回合结束触发 → 窗 = [R+1, R+3]，本值 = R+3。
//     - kLeiChenFlagsKey（4442）→ int：bit0 = 下回合先制+1 待消费旗（tips③）。
//   ★ 辰律（effect_des 568「辰律额外视为点亮的雷辰；上限4点」）→ plugin_storage
//     （战斗级，按 side：辰律是"己方获得"的方级资源，非单宠槽）。
//     kChenLvBase（4443000）+ side：0..4（cap 4）。
//     生产端 = 4946 魂印 2548（row 2146，**已落地** resources/soul_lib/k448_wanshen_2548.cpp：
//     ①场下低概率附加攒辰律 / ②场上体%麻痹或吸取汲取 / ③回合开始转化+双分支 /
//     ④低体回满限次）+ moves 2572（击败，k448_leilian.cpp）；
//     消费端 = 2289/2290 计数面按 568 文字并入（见 k448_leichen_pool.cpp 头注）。
//   ★ 审计段（K437-K446 同构 10^9+id×10^4+owner*32+slot，plugin_storage；*32 =
//     K448 验收修正——旧 *16 下 slot 扩展段与邻 owner 槽 0 相撞，各插件 audit_key_of 注）：
//     - 2289 → 1022890000；2290 → 1022900000；soul 2376 → 1023760000。
//   ★ 2289 保底武装键（FLOOR 相消费）→ plugin_storage k2289FloorArmBase+owner。

#include <any>
#include <map>
#include <string>

// ── ElfPet::on_stage_storage（本次上场）──────────────────────────────────
constexpr int kLeiChenStateKey = 4440;       // int：点亮位图 bit0-3 + 顺序 bit8..15
constexpr int kLeiChenAwakenEndKey = 4441;   // int：觉醒窗末回合（0=无）
constexpr int kLeiChenFlagsKey = 4442;       // int：bit0 = 当回合先制+1 待消费（武装回合 bit8..15）

// ── plugin_storage（战斗级）────────────────────────────────────────────
constexpr int kChenLvBase = 4443000;         // +side：辰律层数（cap 4；568 伪雷辰）
constexpr int k2289FloorArmBase = 1022891000;  // +owner：本刀保底武装（FLOOR 相消费即清）
// ── K448 接续段（2576/2591，k448_leiyi_2576_2591.cpp）─────────────────────
constexpr int k2576WinBase = 1025761000;      // +owner：2576 粉伤 cap 窗末回合（含）
constexpr int k2576ArmBase = 1025761100;      // +owner：2576 CAP 条目安装旗
constexpr int k2591BoostArmBase = 1025911000; // +owner：2591 增伤窗安装旗（预留）
constexpr int kLeiYiCapArmKey = 4444;         // on_stage_storage：2576 CAP 条目安装旗（per-pet）
//   （2576/2591 窗的换宠失效由 register_pink_damage_effect / register_skill_effect 的
//    ON_STAGE scope + epoch 内建——perform_switch 递增 epoch 条目自动失效，零插件代码；
//    安装旗必须 per-pet：side 级旗会让换宠回场重施失去重装机会，T18 实测发现）。
// 审计段：2576 → 1025760000；2591 → 1025910000（+owner*32+slot，布局见各文件头注）。

// ── 审计基段（+owner*32+slot）─────────────────────────────────────────
constexpr int k2289AuditBase = 1022890000;
constexpr int k2290AuditBase = 1022900000;
constexpr int k2376AuditBase = 1023760000;

// ── 槽位 ↔ 方位（tips① 顺序：1~4 号位 = 东/南/西/北）────────────────────
//   bit0=东(槽0) bit1=南(槽1) bit2=西(槽2) bit3=北(槽3)。
constexpr int kLeiChenSlotCount = 4;         // 只有 1~4 号位有雷辰（第五技无对应）

// ── "ㄣ"形点亮序枚举（0-based 槽序；= 1-based 1423/3241/4312/2134）──────
//   证据与仲裁口子见文件头"BWIKI/语料复核"段。镜像族（1324/2143/3412/4231）非 ㄣ。
constexpr int kZhenAwakenOrders[4][4] = {
    {0, 3, 1, 2},   // 1423：东→北→南→西
    {2, 1, 3, 0},   // 3241：西→南→北→东（1423 逆向）
    {3, 2, 0, 1},   // 4312：北→西→东→南
    {1, 0, 2, 3},   // 2134：南→东→西→北（4312 逆向）
};

// ── 状态读写（指针式 any_cast：类型不符按缺失回退，绝不抛——soul_lib/common.h
//    any_int_or 同款治理口径；本头自含，不依赖任一插件 common.h）──────────────
inline int leichen_int_or(const std::any& a, int fallback = 0) {
    if (const int* p = std::any_cast<int>(&a)) {
        return *p;
    }
    return fallback;
}
inline int leichen_state_of(const ElfPet& pet) {
    auto it = pet.on_stage_storage.find(kLeiChenStateKey);
    return it == pet.on_stage_storage.end() ? 0 : leichen_int_or(it->second);
}
inline void leichen_set_state(ElfPet& pet, int state) {
    pet.on_stage_storage[kLeiChenStateKey] = state;
}
inline int leichen_awaken_end_of(const ElfPet& pet) {
    auto it = pet.on_stage_storage.find(kLeiChenAwakenEndKey);
    return it == pet.on_stage_storage.end() ? 0 : leichen_int_or(it->second);
}
inline void leichen_set_awaken_end(ElfPet& pet, int end_round) {
    pet.on_stage_storage[kLeiChenAwakenEndKey] = end_round;
}
inline int leichen_flags_of(const ElfPet& pet) {
    auto it = pet.on_stage_storage.find(kLeiChenFlagsKey);
    return it == pet.on_stage_storage.end() ? 0 : leichen_int_or(it->second);
}
inline void leichen_set_flags(ElfPet& pet, int flags) {
    pet.on_stage_storage[kLeiChenFlagsKey] = flags;
}

// 点亮位图（bit0-3）/ 序列长度（= 已点亮数）/ 第 i 次点亮的槽位（0-3）。
inline int leichen_bitmap(int state) { return state & 0x0F; }
inline int leichen_seq_len(int state) {
    int n = 0;
    for (int b = leichen_bitmap(state); b; b &= b - 1) {
        ++n;
    }
    return n;
}
inline int leichen_seq_slot(int state, int i) {
    return (state >> (8 + 2 * i)) & 0x03;
}
// 点亮槽 slot（未亮前提由调用方保证）：置位图位 + 追加顺序位（bit8+2*len 起 2bit）。
inline int leichen_light_slot(int state, int slot) {
    const int len = leichen_seq_len(state);
    const int bitmap = leichen_bitmap(state) | (1 << slot);
    int seq = state & 0xFF00;   // bit8..15 = 顺序区
    seq |= (slot & 0x03) << (8 + 2 * len);
    return bitmap | seq;
}

// 重置雷辰进度 = 写回 0（清位图 + 清序列；effect_des 444/445「重置自身的雷辰进度」）。
inline constexpr int kLeiChenResetState = 0;

// 消耗末位点亮的雷辰（effect 2571「消耗自身末位的点亮的雷辰」）：弹出最后一条序列项
// 并清其位图位。返回被弹的槽位（0-3）；无点亮返回 -1（调用方如实零消耗档）。
inline int leichen_pop_last(ElfPet& pet) {
    const int state = leichen_state_of(pet);
    const int len = leichen_seq_len(state);
    if (len <= 0) {
        return -1;
    }
    const int slot = leichen_seq_slot(state, len - 1);
    int bitmap = leichen_bitmap(state) & ~(1 << slot);
    int seq = state & 0xFF00;
    seq &= ~(0x03 << (8 + 2 * (len - 1)));   // 截断最后一条序列项
    pet.on_stage_storage[kLeiChenStateKey] = bitmap | seq;
    return slot;
}

// 觉醒窗激活判定（当前回合落在 [进入+1, 窗末] 内；窗值 0 = 无觉醒）。
inline bool leichen_awaken_active(const ElfPet& pet, int round_count) {
    const int end_round = leichen_awaken_end_of(pet);
    return end_round > 0 && round_count <= end_round;
}

// 辰律读/写（cap 4 在写入侧钳；effect_des 568「上限4点」）。
inline int chenlv_of(const std::map<int, std::any>& storage, int side) {
    auto it = storage.find(kChenLvBase + side);
    return it == storage.end() ? 0 : leichen_int_or(it->second);
}
inline void chenlv_set(std::map<int, std::any>& storage, int side, int value) {
    storage[kChenLvBase + side] = value < 0 ? 0 : (value > 4 ? 4 : value);
}

// 「雷」冠名判定（BWIKI 复核结论：技名首字 = 雷（UTF-8 E9 9B B7）——仲裁口子，
// 推翻时只改本函数，见文件头）。
inline bool leichen_named_lei(const std::string& name) {
    return name.size() >= 3
        && static_cast<unsigned char>(name[0]) == 0xE9
        && static_cast<unsigned char>(name[1]) == 0x9B
        && static_cast<unsigned char>(name[2]) == 0xB7;
}

// 「所有技能均以雷冠名」门（魂印 2548 ①②③④前置条件）：5 槽技名全部首字=雷。
inline bool leichen_all_named_lei(const ElfPet& pet) {
    for (int i = 0; i < 5; ++i) {
        if (!leichen_named_lei(pet.skills[static_cast<std::size_t>(i)].name)) {
            return false;
        }
    }
    return true;
}

// 计数面（2289 威力颗数 / 2290 全属性颗数）= 位图点亮数 + 辰律伪雷辰。
// 辰律项在读侧也钳 cap 4（effect_des 568「上限4点」——写入侧 chenlv_set 已钳，
// 读侧再钳一道：未来 2548 生产端若直写裸键也不越上限）。
inline int leichen_lit_count(const ElfPet& pet,
                             const std::map<int, std::any>& storage, int side) {
    const int chenlv = chenlv_of(storage, side);
    return leichen_seq_len(leichen_state_of(pet)) + (chenlv > 4 ? 4 : chenlv);
}

#endif  // LEICHEN_RUNTIME_H
