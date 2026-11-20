#ifndef MEIHEWEITE_BAO_RUNTIME_H
#define MEIHEWEITE_BAO_RUNTIME_H

// 梅赫维特 4853「曝」印记 —— 跨 dylib 共享的 plugin_storage int 槽约定（bailuo_runtime.h
// 同款模式）。曝的锚点目前的生产/管理端都在 soul_lib/k421_meiheweite_lyrics.cpp（魂印
// 2456 本体），但 moves 侧已有两个等锚续单（K419 头注记档）：
//   · 2382「对手拥有曝时此技能先制+1且必定命中」（逐日长诗 38171 专属）
//   · 2384「对手拥有曝时将对手的烧伤转化为焚烬」
// 本头文件就是它们落地时的共享读取面（K419 头注"曝时锚点落地后一并解"）。
//
// ══ 曝的官方口径（2026-11-06 现查语料 reference/scrape/out/all_posts_final.jsonl
//    《机制解析—梅赫维特》行 130/136 + 曝机制专题散句）════════════════════════
//   「曝：持有印记的精灵，不高于50%的异常抗性减少10%（包含5%全免）
//     同时被击败效果失效和被击败时消逝」
//   「曝印记，绑定对手，下场保留，与异常抗性减法计算（包含5%全免），
//     如果对手本身没有开抗性就不受影响」
//   三行为：
//   ① 低段抗性惩罚：>0 且 ≤50% 的抗性段（含全免%）-10 点 —— 内核通用面
//     `BattleContext::anomaly_resist_low_penalty[side][slot]` +
//     `Resistance::isResistantTo(anomaly_id, penalty)`（apply_anomaly_impl [4] 消费）。
//   ② 被击败效果失效：复用 2026-09-22 百罗鬼帝 2085 的压制门
//     （`set_defeat_effects_suppressed`：死亡时点桶门 battleContext.cpp + 自有
//     EVENT_DEATH 监听门 event_center.h "watcher.owner == event.target"）——
//     第三方死亡观察者（赫斯提亚灼火/神罗裂缝/奥菲支援族）不受影响，与语料一致；
//     免死拦截器在死亡成立**之前**问询，天然不受压制。
//   ③ 被击败时消逝：**曝印记本身**随载体死亡注销（若载体被复活，不再带曝）。
//     语料另读："载体如星皇之怒式硬消逝（hp/上限归零、不走红伤死亡流）"——本引擎取
//     "正常死亡 + 印记注销"口径：语料 bug 记录明写曝载体仍走死亡切换
//     （「下场时和下场后都有个死亡切换」），硬消逝该记录即不成立；如官方实测翻案，
//     改 defeat_pet 路径一处即可（记档待官方数值/实测锚点）。
//   生命周期：绑定对手（施加时记到对方精灵），**下场保留**（换入换出不注销），
//   载体死亡/被消逝注销。producer = 永沐（对方执行实际概率 ≤50% 的异常附加时，
//   EVENT_ANOMALY_ATTACH_LOW_CHANCE 事件面）。
//
// ⚠️ 键段 4697200：K419/K421 已备案 4697 段（4697000 唱词总数 / 4697010+owner*10+kind
//    各唱词份数，最高 4697023）。本段 4697200..4697200+11 无重叠；新增前照例
//    `grep -rn 46972 resources/ src/` 复查 + tools/check_storage_keys.py 门禁兜底。
//    ⚠️ 若未来第二个 TU include 本头（如 k419 消费端），键常量会出现跨文件同值——
//    需在 tools/check_storage_keys.py 的 intentional-shared 备案加行（4697000 先例）。
#include <typeinfo>

#include <any>
#include <map>

#include <fsm/battleContext.h>

namespace meiheweite_bao {

// 梅赫维特宿主宠 id（effect_icon 行 2038 pet=[4853]，魂印 2456 唯一宿主行）。
// 永沐的"自身位于出战背包时"按 id 扫 6 槽判定（场下魂印停转，无法按 mark 查背包位）。
constexpr int kMeiheweitePetId = 4853;

// 曝印记锚点：+ side*6 + slot（槽位 = 精灵身份，elfPets 数组整场静态）。
// 值 = 1（挂曝）。绑定对手、下场保留、死亡/消逝注销（管理端 k421）。
constexpr int kBaoMarkBase = 4697200;

// 低段抗性惩罚点数（曝固定 10；独立常量便于将来同类印记参数化）。
constexpr int kBaoResistLowPenalty = 10;

inline int bao_key(int side, int slot) { return kBaoMarkBase + side * 6 + slot; }

inline bool has_bao(const BattleContext* ctx, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6) {
        return false;
    }
    const auto it = ctx->plugin_storage.find(bao_key(side, slot));
    return it != ctx->plugin_storage.end() && it->second.type() == typeid(int)
           && std::any_cast<int>(it->second) == 1;
}

// 挂曝（重复附加 = 刷新，不叠加——官方"附加曝"无叠层语义）。返回挂后状态。
inline bool attach_bao(BattleContext* ctx, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6) {
        return false;
    }
    ctx->plugin_storage[bao_key(side, slot)] = 1;
    // ① 低段抗性惩罚同步生效（死亡/消逝注销时由管理端清零）。
    ctx->anomaly_resist_low_penalty[side][slot] = kBaoResistLowPenalty;
    return true;
}

// 注销曝（载体死亡/被消逝；管理端调用）。抗性惩罚一并复位。
inline void clear_bao(BattleContext* ctx, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6) {
        return;
    }
    ctx->plugin_storage.erase(bao_key(side, slot));
    ctx->anomaly_resist_low_penalty[side][slot] = 0;
}

}  // namespace meiheweite_bao

#endif  // MEIHEWEITE_BAO_RUNTIME_H
