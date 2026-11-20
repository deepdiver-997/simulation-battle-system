// K421 —— 梅赫维特 4853 唱词族（soul 通道：唱词获得规则 + 持有态行为 + 4697 段计数槽生产端）
// （看板 K421，a01-k421 2026-11-06，worktree ~/.agent-workspaces/workspaces/sbs-local/k421，
//  分支 feat/k421；场景号段 8010-8019，场景 test/scenario/scenario_8010_meiheweite_lyrics.cpp。
//  K419 头注（resources/moves_lib/k419_meiheweite_pool.cpp）留的魂印续单，本单收口。）
//
// ══ 选载体：注册 id = 2456（魂印面），不是 2372-2375 ═════════════════════════════
//   官方 effect_icon 行 2038（pet=[4853]，effect_id=2456，kind=[7,5,14]，args="1 0"
//   —— 行参丢前两位后为空，全部数值按 tips/语料落地不消费行参）五段魂印原文（2026-11-06
//   现查，tips 按 | 分句；「（boss无效）」为独立尾段 → 按 K405 先例全句尾归属，
//   全节点 boss_invalid=true，resources/soul_lib/k405_pp_interact_family.cpp:49 同判）：
//     永沐：自身位于出战背包时，若对方精灵执行概率不高于50%的异常状态附加效果则为对手附加曝
//     朝曦：自身主动切换时当回合自身选择自身的1号位技能并获得1句技能对应的唱词，
//           若对手拥有曝则登场时自身恢复全部体力与PP值且当回合额外免疫并反弹受到的异常状态
//     眷颂：自身使用技能的当回合结束后，若自身本回合没有以此法以外的方式获得过唱词且
//           当回合对手没有使用技能，则自身获得1句所使用技能对应的唱词
//     炽耀：回合开始时，自身每有1句唱词，自身受到的攻击伤害减少15%，自身行动阶段结束后，
//           若自身的唱词达到了4句，则进行一次额外行动以触发破黯穿穹
//     余晖：自身下场时，若对手拥有曝，己方下只精灵1回合内免疫并反弹受到的异常状态
//   2372-2375 是 moves 空间 effect_info 模板（四唱词的**持有态行为定义**，iconInherent
//   标签，args_num 全 0；K419 头注裁决"不在技能命中时获得"）——soul 空间同号已有归属：
//     2373 = effect_icon 行 1957（宠 4753，"回合开始时令对手与自身速度-1…"异文）
//     2374 = effect_icon 行 1958（宠 4754）且**已被注册**：k_foe_boost_crit_damage.cpp
//            SOUL_PROGRAM(2374)（K356，"对手有能力提升→必暴+50%伤害"，registry soul 在册）
//     2375 = effect_icon 行 1959（宠 3896，"登场抵挡首次致命一击…"异文）
//     2372 = 无任何 effect_icon 行（两空间皆无行）
//   —— 若按字面注册 SOUL_PROGRAM(2372..2375) 会撞掉 4753/4754/3896 三宠魂印（2374 直接
//   静默覆盖，2373/2375 绑错宿主）。四唱词行为本体落在本魂印面 2456 程序内（见下节点表），
//   registry soul 只增 +1（2456）。
//
// ══ 语义现查（2026-11-06，萌兜官方语料 reference/scrape/out/all_posts_final.jsonl；
//    行号 sed -n 'Np' 验证）══════════════════════════════════════════════════════
//   语料行 130《机制解析—梅赫维特》+ 行 136《梅赫维特（体验服）》取证：
//   - 唱词 = 印记效果，**可以重复记录同种唱词，最多记录 4 个**；**记录下场不保留**。
//   - 四技能（38171 逐日长诗/29215 灼目天星/29216 七重沙海/38172 长叹·天人命途）链上的
//     2372-2375 是"该技对应哪句唱词"的**标签位**——"无法通过使用技能以触发唱词效果，
//     依赖于魂印触发"（无为觉者无为效果同型）：即本插件按载体技 side_effect 标签运行期
//     推导唱词种类（Skills::rawEffectRecords.effect_id ∈ {2372..2375}，DB 锚定）。
//   - 朝曦获得 = **主动切换**（语料行 130"不包含死亡切换和驱逐切换"；EVENT_SWAP 仅主动
//     中切 emit、死亡换宠不发，battleFsm.cpp:640 注释即口径）。引擎无"选择技能期"时点，
//     PP 门以 drain 时 1 号位 pp 近似（pp==0 不可选 = 不获得；语料行 130"因1号技能pp为0
//     无法行动……选择技能时点早于登场时"）。官方"切换当回合自动施放1号位技能"的施放面
//     = 9901006 旧链路（canglan_tears.cpp，私有段保留，真实对局不挂载）——本单获得面
//     不依赖施放面。
//   - 眷颂获得 = 自身使用技能的当回合结束 + 本回合没以其他方式获得过 + 对手当回合没使用
//     技能（语料行 130："对手没有使用技能包含嗑药、切换、因异常无法行动"）。引擎口径：
//     EVENT_SKILL_USED（使用即发，miss/被封也算）双侧记回合戳，BATTLE_ROUND_END 结算。
//   - 持有态行为叠加口径（两语料行同文）："相同唱词效果可以叠加"；"除了减先制无法叠加
//     （遵循同源不叠加），对手撑死先制-1"；行 130"续航效果默认0血不触发"。
//   - ★ 共享触发（行 130"不同唱词效果共享时点和触发条件" / 行 136"不同唱词效果共享触发
//     节点和触发条件"，两帖同给例子：持摇篮曲/叙事诗/欢歌三条时"满足条件就触发回血1/6、
//     先制-1、所有技能恢复1点pp"，且"自身受到攻击时和战斗阶段结束时，这2个时点也会重复
//     上述判定"）→ **被动三唱词（欢歌/叙事诗/摇篮曲）实现为共享判定引擎**：触发节点 =
//     {自身回合类效果被消除（EVENT_BREAK target=own）｜自身受到攻击伤害（EVENT_TAKE_
//     DAMAGE 红伤 target=own）｜战斗阶段结束（BATTLE_ROUND_END 节点）}；触发条件 =
//     三唱词官方模板前提的并集（对手处于回合类效果｜对手处于能力提升｜自身体力低于对手）。
//     任一条件成立 → 持有的每条唱词按份数各自触发（不做逐条前提过滤——语料例子明写
//     "满足三个条件中任意一个就触发"全部）。赞颂（使用技能后+对手异常）两帖例子均未
//     纳入共享组，保持独立节点独立前提（记档见尾）。
//   - 炽耀 4 句清空：行 130"唱词达到4层时，在自身行动结束后会清空唱词、打出特殊二段
//     伤害以及追加效果"——**清空**落地（AFTER_ACTION_END 桶，行动方真出手口径 K415 §9
//     记档同源）；"特殊二段伤害/追加效果（破黯穿穹）"无数据锚点未落地（见尾记档）。
//   - 与 K419 记档差异：K419 头注"切换用 1 号位技/回合结算补发"两规则成立；补正三点——
//     ①"破黯穿穹（4 句额外行动）"无数据锚点（moves 全库无此名行，38173=合奏·常昼大诰
//     是第五技非破黯穿穹，2026-11-06 现查），只有"4 句清空"半边可落地；②每句 15% 减伤
//     上限 60%（行 130"最多60%通用减伤，加法计算"）；③被动三唱词是共享触发不是各管各。
//
// ══ 四查记录（2026-11-06，soul 口径）════════════════════════════════════════════
//   ① 内核 grep \b(2456|2372|2373|2375)\b（src/ include/）：零命中（零硬编码）。
//   ② soul 相关表在列：effect_icon 2456=行 2038（宠 4853 唯一宿主行，独立尾注 boss无效）；
//     2373/2374/2375 有他宠异文行（见上），2372 无行；custom_effect_programs /
//     custom_effect_overrides 五 id 0 命中。
//   ③ 全库并集无注册：主检出 build/registry.json soul=410 无 2456（2374 已被
//     k_foe_boost_crit_damage 占用——本单不注册它）、moves=1446 无 2456/2372-2375；
//     全 worktree（agents/j332/k421）git grep SOUL_*（2456|2372|2375）零命中；
//     boss_invalid_effects.json（resources/）2456/2373/2374/2375 在册（（boss无效）
//     机器口径，2372 无行无注），全部节点 boss_invalid=true 同步。
//   ④ 废弃 parser 覆盖面（只记录）：soul 空间注册不进 EffectUnit/moves parser 通道；
//     2456 无 effect_info 模板行（魂印行无 moves 模板），无半解析面——零风险。
//
// ══ 节点表（全部 STAGE scope + boss_invalid=true；行 2038 独立尾注全句归属）═══════
//   ① arm    OPERATION_ENTER_EXIT_STAGE(early)：挂 4 只 TEAM watcher（SWAP 朝曦获得 /
//      SKILL_USED 双侧回合戳 / TAKE_DAMAGE 共享判定 / BREAK 共享判定）+
//      1 张 REDUCE_PCT 减伤票（炽耀 15%×句数 钳 60，回调 live 读计数槽 + onstage 门，
//      K415 ②票形）；plugin_storage 旗标防重挂（k266 同款；旗标**下场不清**——
//      TEAM watcher 存续）。
//   ② 朝曦   EVENT_SWAP watcher 回调：主动切换入场（drain 时新宠已在场）→ 1 号位技
//      （skills[0]，pp!=0 门）标签 → 获得 1 句（上限 4、种类可重复）。
//   ③ 眷颂   BATTLE_ROUND_END 节点：本回合用过技 + 本回合未获得过 + 对手没用过技 →
//      所用技标签 → 获得 1 句。（共享判定节点注册在其前：新获唱词次时点才生效。）
//   ④ 共享   BATTLE_ROUND_END 节点 + TAKE_DAMAGE/BREAK watcher：共享判定引擎（见上），
//      欢歌→pp_restore(份数)、叙事诗→记 pending（同源不叠加=每回合至多一次）、
//      摇篮曲→heal(上限 1/6×份数)；hp<=0 早退（"续航效果默认0血不触发"）。
//   ⑤ 赞颂2372 BATTLE_FIRST/SECOND_AFTER_ACTION（行动方桶，K419 2376 同款六槽）：
//      持有≥1 + 本回合用过技 + 对手处于异常 → 全属性+1×份数（stat_change 真原语）。
//   ⑥ 叙事诗消费 BATTLE_FIRST_MOVE_RIGHT 桶（memset 后执行，双侧桶均跑）：上回合
//      pending → 下回合对手先制-1（ws.preemptive_level[对手] -= 1），K266 FMR 形。
//   ⑦ 炽耀   BATTLE_FIRST/SECOND_AFTER_ACTION_END：唱词达 4 句 → 清空全部唱词槽
//      （语料行 130"行动结束后会清空唱词"；二段伤害未落地见尾记档）。
//   ⑧ 离场   SOUL_MARK_HOOKS(2456, nullptr, on_exit)：下场/阵亡（perform_switch 旧宠
//      与死亡漏斗均经 deactivate_soul_mark）清全部唱词槽与回合戳——"记录下场不保留"；
//      **wired 旗标与累计获得审计除外**（见键段）。
//
// ══ 存储键段（新增前已 grep 全库无撞；tools/check_storage_keys.py 门禁兜底）══════
//   - plugin_storage 4697 段（4697000 = K419 已声明的消费端对接面，INTENTIONAL_SHARED
//     备案登记见 tools/check_storage_keys.py；⚠️ kLyricCountBase 常量在 moves_lib/
//     soul_lib 各自定义同值 4697000——跨 .so 不共符号，契约=数值键位本身）：
//     4697000+side = 唱词总句数（0/缺=无；生产端=本文件，消费端=K419 2376）
//     4697010+side*10+kind = 各唱词份数（kind 0赞颂/1欢歌/2叙事诗/3摇篮曲 = 2372..2375-2372）
//   - 审计/状态槽 10<id>000x 段（id=2456，K416-K419 同款；除 kWiredKey 外全部键带 +owner）：
//     1024560001 朝曦累计获得句数 / 1024560002 眷颂累计获得句数 / 1024560003 炽耀当回合
//     减伤%（live）/ 1024560004 本回合已获得唱词回合戳 / 1024560005 自身当回合所用技能
//     id / 1024560006 自身用技回合戳 / 1024560007 对手用技回合戳 / 1024560008 叙事诗
//     pending 触发回合戳（-1=无）/ 1024560009 赞颂落地份数 / 1024560010 欢歌 PP 恢复
//     申报量 / 1024560011 wired 旗标（下场不清）/ 1024560012 叙事诗 FMR 实际施加
//     delta / 1024560013 摇篮曲回复量
//
// ══ 未落地子句记档（无锚点/跨面，不产码——K419 2382 同款裁决）══════════════════
//   - 破黯穿穹（炽耀 4 句的额外行动+特殊二段伤害+追加效果）：moves 全库无此名行，威力/
//     效果零锚点；**清空半边已按语料落地**（⑦），行动/伤害半边落地条件 = 官方数值入库
//     （续单锚点落地后按 EXTRA_ACTION 桶节点补，tianqi_xinghun_4677 覆宇宙星回同式）。
//   - 【2026-11-06 曝线落地，本文件续单收口】原记档四项全部落地：
//     · 永沐（≤50% 控场检测挂曝）= 新事件面 EVENT_ANOMALY_ATTACH_LOW_CHANCE
//       （裁定阶段直发、与掷中无关；来源去 Trait）+ 挂曝；
//     · 曝锚点/生命周期 = resources/meiheweite_bao_runtime.h（4697200 段，下场保留，
//       死亡/消逝注销——"被击败时消逝"取"正常死亡+印记注销"口径，bug 语料佐证曝载体
//       仍走死亡切换；如官方实测翻案改 defeat_pet 路径一处）；
//     · 曝·被击败效果失效 = 复用百罗鬼帝 2085 压制门（死亡时点桶 + 自有 EVENT_DEATH
//       监听门；第三方死亡观察者/免死不受影响，与语料一致），per-side 粒度近似同百罗；
//     · 曝·低段抗性惩罚 = 内核通用面 anomaly_resist_low_penalty +
//       isResistantTo(…, penalty)（>0 且 ≤50% 段 -10，含全免%）。
//   - 朝曦后半（对手拥有曝→登场回满血PP+免疫反弹）/ 余晖（下场时对手拥有曝→己方下只
//     精灵1回合免疫反弹）随锚点落地（本文件 SWAP watcher / on_exit 钩子）。
//   - K419 留续单 2382/2384（"对手拥有曝时…"）现可落：读取面 =
//     meiheweite_bao_runtime.h::has_bao；2382 仍受条件先制族路由门槛约束（K419 头注）。
//   - 赞颂不进共享组：两语料行的共享例子均只含摇篮曲/叙事诗/欢歌（防御/续航组），
//     赞颂官方前提（使用技能后+对手异常）自带独立时点，保守不并入；后续官方实测若证
//     四条全共享，改 k421_shared_passive_eval 一处即可。

#include "common.h"

#include "../gaia_wudao_runtime.h"   // K449 2574 场地盾查询面（永沐挂曝门，2026-11-11）
#include "../meiheweite_bao_runtime.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 2456;

// ── 唱词计数槽（4697 段；4697000 与 moves_lib/k419_meiheweite_pool.cpp 同值——
//    跨 .so 不共符号，各自 constexpr，契约 = plugin_storage 数值键位本身）────────
constexpr int kLyricCountBase = 4697000;  // +owner：唱词总句数（K419 2376 消费端）
constexpr int kLyricKindBase = 4697010;   // +owner*10+kind：各唱词份数
constexpr int kLyricMaxTotal = 4;         // 语料"最多记录4个"

// ── 审计/状态槽（10<id>000x，id=2456；除 wired 全部 +owner）────────────────────
constexpr int kZhaoxiAudit      = 1024560001;
constexpr int kJuansongAudit    = 1024560002;
constexpr int kChiyaoRedAudit   = 1024560003;
constexpr int kGainRoundKey     = 1024560004;
constexpr int kOwnSkillIdKey    = 1024560005;
constexpr int kOwnSkillRoundKey = 1024560006;
constexpr int kFoeSkillRoundKey = 1024560007;
constexpr int kNarrPendingKey   = 1024560008;
constexpr int kZansongAudit     = 1024560009;
constexpr int kHuangetAudit     = 1024560010;
constexpr int kWiredKey         = 1024560011;   // 唯一"下场不清"键（TEAM watcher 存续）
constexpr int kNarrApplyAudit   = 1024560012;
constexpr int kYaolanAudit      = 1024560013;
// ── 曝线（2026-11-06 本文件续单）审计槽 ─────────────────────────────────────
constexpr int kYongmuAudit      = 1024560014;   // 永沐挂曝次数（+owner，整场账）
constexpr int kZhaoxiBaoAudit   = 1024560015;   // 朝曦曝半边触发回合戳（+owner）
constexpr int kYuhuiBaoAudit    = 1024560016;   // 余晖曝半边授予回合戳（+owner）
// 曝满额 PP 授予量（pp_restore 钳 maxPP 只补不削，大额=全满；PP 上限为两位数量级）。
constexpr int kFullPpGrant      = 99;

// ── 炽耀（官方 tips 字面：每句 15% 加算、语料"最多60%通用减伤，加法计算"）──────────
constexpr int kChiyaoRedPerSong = 15;
constexpr int kChiyaoRedCap     = 60;

// ── 小助手 ──────────────────────────────────────────────────────────────
inline int k421_read(BattleContext* ctx, int key, int fallback) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return fallback;
    }
    return any_int_or(it->second, fallback);
}

inline void k421_write(BattleContext* ctx, int key, int v) {
    ctx->plugin_storage[key] = v;
}

inline int k421_song_copies(BattleContext* ctx, int owner, int kind) {
    return k421_read(ctx, kLyricKindBase + owner * 10 + kind, 0);
}

inline int k421_total(BattleContext* ctx, int owner) {
    return k421_read(ctx, kLyricCountBase + owner, 0);
}

// 获得 1 句 kind 唱词（上限 4；种类可重复）。写计数槽 + "本回合已获得"回合戳 + 审计。
inline bool k421_gain_lyric(BattleContext* ctx, int owner, int kind, int audit_base) {
    if (kind < 0 || kind > 3) {
        return false;
    }
    if (k421_total(ctx, owner) >= kLyricMaxTotal) {
        return false;
    }
    k421_write(ctx, kLyricCountBase + owner, k421_total(ctx, owner) + 1);
    k421_write(ctx, kLyricKindBase + owner * 10 + kind, k421_song_copies(ctx, owner, kind) + 1);
    k421_write(ctx, kGainRoundKey + owner, ctx->roundCount);
    k421_write(ctx, audit_base + owner, k421_read(ctx, audit_base + owner, 0) + 1);
    return true;
}

// 清空唱词（炽耀 4 句清空 / 离场"下场不保留"共用；audit_keep_mask 按位保留审计键）。
inline void k421_clear_lyrics(BattleContext* ctx, int owner) {
    ctx->plugin_storage.erase(kLyricCountBase + owner);
    for (int kind = 0; kind <= 3; ++kind) {
        ctx->plugin_storage.erase(kLyricKindBase + owner * 10 + kind);
    }
}

// 载体技 → 唱词种类（按 side_effect 标签位 2372..2375 运行期推导；无标签 = -1）。
// DB 锚定：38171 逐日长诗[→2372]、29215 灼目天星[→2373]、29216 七重沙海[→2374]、
// 38172 长叹·天人命途[→2375]（2026-11-06 moves 表现查，见头注）。
inline int k421_lyric_kind_of_skill(const ElfPet& pet, int skill_id) {
    if (skill_id < 0) {
        return -1;
    }
    for (const Skills& s : pet.skills) {
        if (s.id != skill_id) {
            continue;
        }
        for (const auto& rec : s.rawEffectRecords) {
            if (rec.effect_id >= 2372 && rec.effect_id <= 2375) {
                return rec.effect_id - 2372;
            }
        }
        return -1;
    }
    return -1;
}

// 对手是否处于任意异常状态（官方模板"对手处于异常状态"，全槽扫描，K419 2340 同式；
// 插件不链内核符号，判异走 CoreApi 的 has_active_anomaly，k227 has_any_anomaly 同式）。
inline bool k421_foe_has_anomaly(BattleContext* ctx, int owner) {
    const int foe = 1 - owner;
    if (!g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, foe, id)) {
            return true;
        }
    }
    return false;
}

// 对手是否处于能力提升状态（任一槽正等级，k_foe_boost_crit_damage 同款判据）。
inline bool k421_foe_has_boost(BattleContext* ctx, int owner) {
    for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
        if (ctx->ability_levels[1 - owner][slot] > 0) {
            return true;
        }
    }
    return false;
}

// 全属性+N（六槽 stat_change 真原语本体/视图同步；moves_lib all_stats_up 同式——
// soul_lib 不引 moves 头，此处等价内联）。
inline void k421_all_stats_up(BattleContext* ctx, int owner, int delta) {
    if (!ctx || delta == 0 || !g_core || !g_core->stat_change) {
        return;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        g_core->stat_change(ctx, owner, i, delta);
    }
}

// ══ 曝·被击败效果失效（2026-11-06）══════════════════════════════════════════
// 复用百罗鬼帝 2085 的压制门（set_defeat_effects_suppressed → 死亡时点桶门
// battleContext.cpp + 自有 EVENT_DEATH 监听门 event_center.h）。曝的官方口径
// 「被击败效果失效」只封**倒下方自己的**亡语——门本身就是 owner==target 过滤，
// 第三方死亡观察者（赫斯提亚灼火/神罗裂缝/奥菲支援族）照常投递，与语料一致；
// 免死拦截器在死亡成立**之前**问询，天然不受影响。
// 刷新点：永沐挂曝（载体在场立即武装）/ EVENT_ENTER_STAGE（换入换出重算）/
// ROUND_END 节点（新鲜度兜底——复活不触发 ENTER_STAGE 的窗口记档）。
// ⚠️ 死亡/消逝回调里**不刷新**：死亡事件 drain 还在投递倒下方自有监听，中途撤旗
// 会让同事件后续自有监听漏网（gate 逐 watcher 读旗，注册序不定）。
inline void k421_bao_refresh_suppression(BattleContext* ctx) {
    if (!ctx) {
        return;
    }
    for (int side = 0; side < 2; ++side) {
        const int slot = ctx->on_stage[side];
        const bool arm = slot >= 0 && slot < 6 && meiheweite_bao::has_bao(ctx, side, slot);
        ctx->set_defeat_effects_suppressed(side, arm);
    }
}

// 永沐门："自身位于出战背包时"——按宿主 id 扫 6 槽（场下魂印停转，无法按 mark 查
// 背包位；id 锚 = effect_icon 行 2038 pet=[4853]）且存活（hp>0）。
// K449 续作（2026-11-11）：拆出槽位版供 2574 场地盾门判"源在场/场下"（布尔版语义不变）。
inline int k421_yongmu_host_slot(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return -1;
    }
    for (int slot = 0; slot < 6; ++slot) {
        const ElfPet& pet = ctx->seerRobot[owner].elfPets[slot];
        if (pet.id == meiheweite_bao::kMeiheweitePetId && pet.hp > 0) {
            return slot;
        }
    }
    return -1;
}

inline bool k421_yongmu_host_in_party(BattleContext* ctx, int owner) {
    return k421_yongmu_host_slot(ctx, owner) >= 0;
}

// ══ ④ 共享判定引擎（语料行 130/136"共享触发节点和触发条件"）════════════════════
// 触发节点：自身回合类效果被消除（EVENT_BREAK target=own）/ 自身受到攻击伤害
// （EVENT_TAKE_DAMAGE 红伤 target=own）/ 战斗阶段结束（ROUND_END 节点）。
// 条件 = 三唱词官方前提并集；任一成立 → 每条**持有中的**唱词按份数各自触发
// （语料例子明写"满足三个条件中任意一个就触发"全部——不做逐条前提过滤）。
void k421_shared_passive_eval(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return;
    }
    const ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return;   // "续航效果默认0血不触发"（语料行 130）
    }
    const int huan_copies = k421_song_copies(ctx, owner, 1);   // 欢歌 2373
    const int narr_copies = k421_song_copies(ctx, owner, 2);   // 叙事诗 2374
    const int yaol_copies = k421_song_copies(ctx, owner, 3);   // 摇篮曲 2375
    if (huan_copies <= 0 && narr_copies <= 0 && yaol_copies <= 0) {
        return;   // 三条被动唱词全无 → 无可触发面
    }
    const bool cond_yaolan = ctx->has_round_effects(1 - owner);   // 对手处于回合类效果
    const bool cond_narr   = k421_foe_has_boost(ctx, owner);      // 对手处于能力提升
    const bool cond_huan   = self.hp < ctx->getPet(1 - owner).hp; // 自身体力低于对手
    if (!(cond_yaolan || cond_narr || cond_huan)) {
        return;
    }
    if (huan_copies > 0 && g_core && g_core->pp_restore) {
        // "恢复自身所有技能1点PP值"×份数（pp_restore 只补不削）
        (void)g_core->pp_restore(ctx, owner, huan_copies, /*only_empty=*/false);
        k421_write(ctx, kHuangetAudit + owner, huan_copies);
    }
    if (narr_copies > 0) {
        // "下回合令对手的技能先制-1"——记 pending，FMR 桶（memset 后）消费。
        // 同回合多时点覆写同戳 = 天然"同源不叠加"（对手撑死先制-1）。
        k421_write(ctx, kNarrPendingKey + owner, ctx->roundCount);
    }
    if (yaol_copies > 0 && g_core && g_core->heal_amount) {
        // "恢复自身最大体力的1/6"×份数（整除按官方取整口径；多时点可重复触发）
        const int max_hp = resolve_pet_max_hp(self);
        const int amount = max_hp / 6 * yaol_copies;
        if (amount > 0) {
            (void)g_core->heal_amount(ctx, owner, amount);
            k421_write(ctx, kYaolanAudit + owner, amount);
        }
    }
}

// ══ ① arm：watcher×4 + 减伤票×1（每场一次，wired 旗标防重挂，k266 同款）════════
EffectResult k421_2456_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (ctx->plugin_storage.count(kWiredKey + owner)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(kWiredKey + owner, 1);

    // watcher 朝曦：EVENT_SWAP——主动切换入场 → 1 号位技（skills[0]，pp!=0 门）标签
    //   → 获得 1 句；【曝半边 2026-11-06】对手拥有曝 → 登场时恢复全部体力与PP值 +
    //   当回合额外免疫并反弹受到的异常状态。（EVENT_SWAP 仅主动中切 emit、drain 时
    //   新宠已在场（perform_switch 口径）；死亡换宠/驱逐不发 SWAP = 官方"不包含死亡
    //   切换和驱逐切换"。曝半边与唱词半边**并列**：1 号位 PP==0 只挡唱词，不挡曝半边。）
    ctx->event_center_.register_watcher(
        EventType::EVENT_SWAP, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner) {
                return;
            }
            const bool mark_on_stage = onstage_has_soul_mark(c, owner, kMarkId);
            // ── 朝曦前半：获得 1 句 ──
            if (mark_on_stage) {
                const ElfPet& self = c->getPet(owner);
                if (self.skills[0].pp != 0) {   // 0PP 不可选（语料行 130；pp<0=无限）
                    const int kind = k421_lyric_kind_of_skill(self, self.skills[0].id);
                    (void)k421_gain_lyric(c, owner, kind, kZhaoxiAudit);
                }
            }
            // ── 朝曦后半（曝）：回满血 + 全 PP + 当回合免疫并反弹异常（191/1932 双授予
            //    Modern 层同款：mask=0 全免 + grant_reflect；"当回合"= 1 回合窗口）。──
            const int foe = 1 - owner;
            const int foe_slot = c->on_stage[foe];
            const bool foe_has_bao =
                foe_slot >= 0 && foe_slot < 6 && meiheweite_bao::has_bao(c, foe, foe_slot);
            if (mark_on_stage && foe_has_bao && g_core && g_core->heal_amount
                && g_core->pp_restore) {
                const ElfPet& self = c->getPet(owner);
                if (self.hp > 0) {
                    (void)g_core->heal_amount(c, owner, resolve_pet_max_hp(self));
                    (void)g_core->pp_restore(c, owner, kFullPpGrant, /*only_empty=*/false);
                    c->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                                      /*anomaly_mask=*/0, /*duration_rounds=*/1,
                                      /*source_id=*/kMarkId, /*soul_immunity=*/false,
                                      EffectScope::ON_STAGE, /*counts=*/0,
                                      /*source_effect_id=*/kMarkId, ImmunityTier::Modern);
                    c->rule_center_.grant_reflect(owner, /*source_slot=*/-1,
                                                  /*source_effect_id=*/kMarkId,
                                                  /*target=*/owner, /*rounds=*/1);
                    k421_write(c, kZhaoxiBaoAudit + owner, c->roundCount);
                }
            }
        },
        WatcherScope::TEAM);

    // watcher：EVENT_SKILL_USED（使用即发，miss/被封也算）——双侧回合戳。
    //   own 侧记"本回合所用技能 id + 回合戳"（眷颂/赞颂素材；属性技也算"使用"，
    //   ws.skill_used 只在攻击伤害管线置位不可用）；foe 侧记"对手用技回合戳"（眷颂门）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;   // 持印者不在场不记账（下场不保留口径）
            }
            if (ev.actor == owner) {
                k421_write(c, kOwnSkillIdKey + owner, ev.skill_id);
                k421_write(c, kOwnSkillRoundKey + owner, c->roundCount);
            } else if (ev.actor == 1 - owner) {
                k421_write(c, kFoeSkillRoundKey + owner, c->roundCount);
            }
        },
        WatcherScope::TEAM);

    // watcher：EVENT_TAKE_DAMAGE——共享判定·"自身受到攻击（伤害）时"。红伤过滤
    //   （damage_kind==NORMAL，E12 口径）+ 对手来源 + 持印门。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.actor != 1 - owner) {
                return;
            }
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                return;   // "受到攻击（伤害）"= 红伤
            }
            k421_shared_passive_eval(c, owner);
        },
        WatcherScope::TEAM);

    // watcher：EVENT_BREAK——共享判定·"自身回合类效果被消除时"。EVENT_BREAK 仅断回合
    //   **成功**路径 emit、target=被断方（battle_primitives break_round_effects），
    //   target==owner 即"自身的回合类效果被消除"（自断/被驱散均算）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_BREAK, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner) {
                return;
            }
            k421_shared_passive_eval(c, owner);
        },
        WatcherScope::TEAM);

    // watcher 永沐（2026-11-06）：EVENT_ANOMALY_ATTACH_LOW_CHANCE——"自身位于出战背包
    //   时，若对方精灵执行概率不高于50%的异常状态附加效果则为对手附加曝"。判定点 =
    //   事件面（apply_anomaly_impl 裁定阶段直发：申报过概率 + 闸门改写后实际概率 ≤50，
    //   **与是否掷中无关**，chance_gate_lib.h 口径）；来源过滤 = 去 Trait（特性主动毒/
    //   被动毒不管，与亮节票的两处不同点之一）；附加对象 = 执行方在场宠（绑定对手、
    //   下场保留）。重复附加 = 刷新不叠加（attach_bao 语义）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ANOMALY_ATTACH_LOW_CHANCE, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor < 0 || ev.actor > 1 || ev.actor == owner) {
                return;   // 只有"对方精灵执行"才挂曝
            }
            if (ev.chance_source == static_cast<int>(ChanceSource::Trait)) {
                return;   // 永沐不含特性主动毒/被动毒（chance_gate_lib.h 头注）
            }
            if (!k421_yongmu_host_in_party(c, owner)) {
                return;   // 梅赫维特不在出战背包或已阵亡
            }
            // K449 2574 场地盾（「{0}回合内，双方的在场精灵无法成为场下精灵的效果
            // 目标」）：梅赫维特**在场下**（场下源）→ 执行方在场宠挂曝（场上目标）
            // 属场下效果施加——盾活跃（严格当轮匹配 stamp）时本施加被阻。梅赫维特
            // 亲自在场 → 源在场，天然放行（roster_field 2513/903 同款门形）。查询面
            // ../gaia_wudao_runtime.h（普通 N 回合窗：施放方离场/断回合/到期三路
            // stamp 冻结自假）。行为正负例 scenario_8370 T8374.f；覆盖矩阵见 K449
            // 完成报告（2026-11-11 派发缺口 2 授权的第二个已实现场下→场上入口）。
            const int yongmu_host_slot = k421_yongmu_host_slot(c, owner);
            if (yongmu_host_slot >= 0 && yongmu_host_slot != c->on_stage[owner]
                && gaia_bench_target_locked(c, ev.actor)) {
                return;   // 在场执行宠是场下效果的锁定目标 → 挂曝被场地盾阻
            }
            const int target_slot = c->on_stage[ev.actor];
            if (target_slot < 0 || target_slot >= 6) {
                return;
            }
            (void)meiheweite_bao::attach_bao(c, ev.actor, target_slot);
            k421_write(c, kYongmuAudit + owner, k421_read(c, kYongmuAudit + owner, 0) + 1);
            k421_bao_refresh_suppression(c);   // 载体在场 → 封亡语立即武装
        },
        WatcherScope::TEAM);

    // watcher 曝生命周期：EVENT_ENTER_STAGE——按在场宠重算双方压制位（曝载体在场 =
    //   武装；换下/换上无曝宠 = 撤收）。百罗 2085 同款 per-side 粒度近似。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [](BattleContext* c, const BattleEvent& /*ev*/) {
            k421_bao_refresh_suppression(c);
        },
        WatcherScope::TEAM);

    // watcher 曝生命周期：EVENT_DEATH / EVENT_VANISH——载体死亡/被消逝 → 注销曝
    //   （"被击败时消逝"口径③：若载体被复活，不再带曝）+ 撤抗性惩罚。
    //   ⚠️ 回调里**不刷新压制位**：死亡事件 drain 还在投递倒下方自有监听，中途撤旗
    //   会让同事件后续自有监听漏网（gate 逐 watcher 读旗、注册序不定）；旗的新鲜度
    //   交给 ENTER_STAGE / 永沐挂曝 / ROUND_END 三个刷新点。
    //   ⚠️ 本 watcher 的 owner = 梅赫维特方，与被压制的倒下方（对手）不同侧 →
    //   EVENT_DEATH 压制门（watcher.owner == event.target 才拦）不会拦掉它。
    const auto k421_bao_death_clear = [](BattleContext* c, const BattleEvent& ev) {
        if (!c || ev.target < 0 || ev.target > 1 || ev.slot < 0 || ev.slot >= 6) {
            return;
        }
        if (meiheweite_bao::has_bao(c, ev.target, ev.slot)) {
            meiheweite_bao::clear_bao(c, ev.target, ev.slot);
        }
    };
    ctx->event_center_.register_watcher(EventType::EVENT_DEATH, owner, ctx->roundCount,
                                        /*duration_rounds=*/0, /*once=*/false,
                                        k421_bao_death_clear, WatcherScope::TEAM);
    ctx->event_center_.register_watcher(EventType::EVENT_VANISH, owner, ctx->roundCount,
                                        /*duration_rounds=*/0, /*once=*/false,
                                        k421_bao_death_clear, WatcherScope::TEAM);

    // 减伤票：炽耀——"每有1句唱词，自身受到的攻击伤害减少15%"（语料"加法计算，最多
    //   60%"）。TEAM 票 + 回调 live 读计数槽 + onstage 门（K415 ②票形直系）。
    ctx->register_default_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;   // 减免写 final（K401 720③ 形）
            if (d.defenderId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            int red = k421_total(c, owner) * kChiyaoRedPerSong;
            if (red > kChiyaoRedCap) {
                red = kChiyaoRedCap;
            }
            if (red <= 0) {
                return;
            }
            k421_write(c, kChiyaoRedAudit + owner, red);
            d.final = d.final * (100 - red) / 100;
        });
    return EffectResult::kOk;
}

// ══ ④ 共享判定·战斗阶段结束（ROUND_END 节点形；注册在眷颂节点之前）═════════════
EffectResult k421_2456_shared_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    k421_bao_refresh_suppression(ctx);   // 曝压制位新鲜度兜底（复活窗口记档，见 arm 注）
    k421_shared_passive_eval(ctx, a.owner);
    return EffectResult::kOk;
}

// ══ ⑤ 赞颂(2372)：AFTER_ACTION——"自身使用技能后，若对手处于异常状态，自身全属性+1"══
// （行动方桶：execute_registered_actions 按 robotId 过滤，只在自身行动后执行一次。）
EffectResult k421_2372_zansong(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int copies = k421_song_copies(ctx, owner, 0);
    if (copies <= 0) {
        return EffectResult::kOk;   // 未持"莎木斯的赞颂"
    }
    if (k421_read(ctx, kOwnSkillRoundKey + owner, -1) != ctx->roundCount) {
        return EffectResult::kOk;   // 本回合未使用技能
    }
    if (!k421_foe_has_anomaly(ctx, owner)) {
        return EffectResult::kOk;   // 对手无异常
    }
    k421_all_stats_up(ctx, owner, copies);   // 相同唱词可叠加：+1×份数
    k421_write(ctx, kZansongAudit + owner, copies);
    return EffectResult::kOk;
}

// ══ ③ 眷颂：ROUND_END——本回合用过技 + 本回合没获得过 + 对手没用过技 → 获得 1 句 ══
EffectResult k421_2456_juansong(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (k421_read(ctx, kOwnSkillRoundKey + owner, -1) != ctx->roundCount) {
        return EffectResult::kOk;   // 本回合未使用技能
    }
    if (k421_read(ctx, kGainRoundKey + owner, -1) == ctx->roundCount) {
        return EffectResult::kOk;   // 本回合已以此法以外方式获得过（朝曦）
    }
    if (k421_read(ctx, kFoeSkillRoundKey + owner, -1) == ctx->roundCount) {
        return EffectResult::kOk;   // 对手当回合使用过技能（嗑药/切换/被控不算）
    }
    const int skill_id = k421_read(ctx, kOwnSkillIdKey + owner, -1);
    const int kind = k421_lyric_kind_of_skill(ctx->getPet(owner), skill_id);
    (void)k421_gain_lyric(ctx, owner, kind, kJuansongAudit);
    return EffectResult::kOk;
}

// ══ ⑥ 叙事诗(2374) 消费：FMR 桶（memset 后执行）——上回合 pending → 对手先制-1 ══
// （"下回合"窗口 = 回合戳比对；"同源不叠加"= 每回合至多一次、恒 -1。）
EffectResult k421_2374_narr_apply(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int pending = k421_read(ctx, kNarrPendingKey + owner, -1);
    if (pending < 0 || pending != ctx->roundCount - 1) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[1 - owner] -= 1;
    k421_write(ctx, kNarrApplyAudit + owner, -1);
    k421_write(ctx, kNarrPendingKey + owner, -1);   // 消费即清（防跨回合重复）
    return EffectResult::kOk;
}

// ══ ⑦ 炽耀清空：AFTER_ACTION_END——"唱词达到4层时，在自身行动结束后会清空唱词"══
// （行动方真出手口径：AFTER_ACTION_END 桶被"非出手跳过动作流"守门（K415 §9 记档同源）；
//   特殊二段伤害（破黯穿穹）无锚点未落地，见头注尾记档。）
EffectResult k421_2456_chiyao_clear(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (k421_total(ctx, owner) < kLyricMaxTotal) {
        return EffectResult::kOk;
    }
    k421_clear_lyrics(ctx, owner);
    ctx->plugin_storage.erase(kGainRoundKey + owner);
    return EffectResult::kOk;
}

// ══ ⑧ 离场钩子："记录下场不保留"——清全部唱词槽与回合戳（wired 旗标与累计审计除外）══
void k421_2456_exit(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    k421_clear_lyrics(ctx, owner);
    ctx->plugin_storage.erase(kGainRoundKey + owner);
    ctx->plugin_storage.erase(kOwnSkillIdKey + owner);
    ctx->plugin_storage.erase(kOwnSkillRoundKey + owner);
    ctx->plugin_storage.erase(kFoeSkillRoundKey + owner);
    ctx->plugin_storage.erase(kNarrPendingKey + owner);
    ctx->plugin_storage.erase(kChiyaoRedAudit + owner);
    ctx->plugin_storage.erase(kZansongAudit + owner);
    ctx->plugin_storage.erase(kHuangetAudit + owner);
    ctx->plugin_storage.erase(kNarrApplyAudit + owner);
    ctx->plugin_storage.erase(kYaolanAudit + owner);
    // ── 余晖（曝半边 2026-11-06）：自身下场时，若对手拥有曝，己方下只精灵1回合内
    //    免疫并反弹受到的异常状态。下场含死亡切换（语料 bug 记录"下场时/后都有个
    //    死亡切换"佐证死亡下场也走本钩子）；对手 = 场上对手（1-owner）在场宠持曝。
    //    TEAM 票跨过本次下场落到补位宠（1 回合窗口）；双授予 Modern 层同 191/1932。
    {
        const int foe = 1 - owner;
        const int foe_slot = ctx->on_stage[foe];
        if (foe_slot >= 0 && foe_slot < 6 && meiheweite_bao::has_bao(ctx, foe, foe_slot)) {
            ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                                /*anomaly_mask=*/0, /*duration_rounds=*/1,
                                /*source_id=*/kMarkId, /*soul_immunity=*/false,
                                EffectScope::TEAM, /*counts=*/0,
                                /*source_effect_id=*/kMarkId, ImmunityTier::Modern);
            ctx->rule_center_.grant_reflect(owner, /*source_slot=*/-1,
                                            /*source_effect_id=*/kMarkId,
                                            /*target=*/owner, /*rounds=*/1,
                                            EffectScope::TEAM);
            ctx->plugin_storage[kYuhuiBaoAudit + owner] = ctx->roundCount;
        }
    }
    // kWiredKey/kZhaoxiAudit/kJuansongAudit 保留：TEAM watcher/票整场存续，再登场不得
    // 重挂（重挂=双触发，k266 记档）；累计获得句数是整场账。
}

}  // namespace

// ── 效果注册（静态自注册）—— registry soul +1（2456 入账，只增不改）────────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k421_2456_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_AFTER_ACTION, &k421_2372_zansong,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_SECOND_AFTER_ACTION, &k421_2372_zansong,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_AFTER_ACTION_END, &k421_2456_chiyao_clear,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_SECOND_AFTER_ACTION_END, &k421_2456_chiyao_clear,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k421_2456_shared_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k421_2456_juansong,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k421_2374_narr_apply,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
SOUL_MARK_HOOKS(kMarkId, nullptr, &k421_2456_exit);
