// K407 孤梦噬魂/雄性门族 —— effect 1646/1565（2 宏 / 4 行，双行档位差）
//（看板 K407「通道=HEAL_BLOCK=K394 ④ 条件票直系；直接死亡=K402 1475 死亡双桶/
//  归因臂戳族 + 秒杀通道（瞬杀 force_hp_to_zero 先例）；概率翻倍=睡眠态判定×2
//  （has_active_abnormal_status）；性别查证走 monsters schema + pet_factory。
//  dump 全文先行」，a01-knight11 2026-11-03，worktree ../agents/k407；
//  号段 7650-7659。通道落地=K7 1673 reduce_max_hp_pct/heal ROUND_END 对句 +
//  K402 1630 DETECT 翻倍/FMR pending 戳 + K184 REDUCE_PCT 性别门减伤 +
//  K396 概率未触发补偿/审计槽 + K401 888 首次登场账本。板行第三列（家族名/
//  effect id 清单）在看板文件中缺位（历史行文截断）——本单 effect id 以任务
//  指令 1646/1565 为准，行文以 dump 全文为权威（K405/K403「能落多少落多少、
//  范围外记档跳过」口径）。）
//
// 行清单（tools/soul_effects_dump.py 1646 1565，2026-11-03）：
//   1646 魏武帝·曹操 4058 两行（row1305 本体 / row1493 伙伴「魏武情长伙伴效果」，
//   kind 同 [1,5,12]，**args 档位差**常规行≠宏——两行 8 参同构逐位对齐）：
//     row1305 args[10,1,50,1,20,50,1,20]：「回合开始时若自身处于异常状态状态则触发
//       孤梦噬魂：对手使用攻击则战斗阶段结束时10%的概率直接死亡（处于睡眠状态下
//       概率翻倍），未触发则令对手全属性-1；|自身受到攻击时50%使对手造成的伤害减半
//       且使对手下回合先制-1，未触发减伤则战斗阶段结束时恢复自身最大体力的1/3且
//       下回合触发概率提升20%；|自身攻击命中时50%使自身造成的伤害翻倍且使自身下回合
//       先制+1，未触发增伤则战斗阶段结束时对手减少最大体力的1/3且下回合触发概率
//       提升20%（boss无效）」
//     row1493 伙伴行 args[10,1,50,2,30,50,2,30]：先制量 1→2、爬坡步 20→30
//     （"下回合先制-2/+2"、"概率提升30%"，其余同文）。
//     → db[0]=死亡基础概率%（10）、db[1]=全属性下降量（1）、db[2]=减半概率%（50）、
//       db[3]=减半支对手先制变化量（1/2）、db[4]=减半未触发爬坡步（20/30）、
//       db[5]=翻倍概率%（50）、db[6]=翻倍支自身先制变化量（1/2）、
//       db[7]=翻倍未触发爬坡步（20/30）——8 参全消费，每个 tips 数值都有锚。
//   1565 闭月之姿·貂蝉 3965 两行（row1230 本体 / row1242 伙伴「闭月无双伙伴效果」，
//   kind 同 [1,8,10]，args 档位差仅阈值两位）：
//     row1230 args[100,50,300,100,50,400,100,1]：「首次登场时，若对手为雄性精灵则
//       100%消除对手回合类效果和能力提升状态；对手为雄性精灵时，当回合自身造成的
//       攻击伤害提升50%且造成的伤害低于300时附加伤害值100%的固定伤害，当回合自身
//       受到的攻击伤害降低50%且受到的伤害高于400时100%令对手害怕（boss无效）」
//     row1242 伙伴行 args[100,50,350,100,50,350,100,1]：低伤阈值 300→350、
//     高伤阈值 400→350（行文与 args 互锚）。
//     → db[0]=首登消除申报概率（100）、db[1]=增伤%（50）、db[2]=低伤附加阈值
//       （300/350）、db[3]=附加百分比（100）、db[4]=受击减伤%（50）、
//       db[5]=高伤害怕阈值（400/350）、db[6]=害怕申报概率（100）、db[7]=1 旗不消费
//       （K1 纪律）。
//
// ★ 性别字段查证（本单关键前置，2026-11-03 逐层实锤，三层链全通）：
//   ①DB 层：monsters 表原生 gender INTEGER 列（schema 实证），加载 SQL
//     official_data_repository.cpp:507 `COALESCE(json_extract(raw_json,'$.gender'),2)`
//     → raw_json $.gender 缺失兜 2；实测 4058 魏武帝·曹操 gender=1（雄）、
//     3965 闭月之姿·貂蝉 gender=2（雌）、谱尼=0/盖亚=1/启灵元神=2（码表实测）。
//   ②工厂层：pet_factory.cpp map_gender（raw 0=无性别/1=雄/2=雌/其它=无性别）
//     → ElfPet.gender（pet_factory.cpp:325 map_gender(monster->gender)）；
//     Gender 枚举 MALE=0/FEMALE=1/NONE=2（elf-pet.h:21）。
//   ③读者层：k184_male_mitigate.cpp（272「减少20%来自雄性精灵的攻击伤害」）
//     已有性别门首读者先例（REDUCE_PCT 回调内 attacker.gender==Gender::MALE 实测绿）。
//   → 结论：**可达，1565 全宏照落**；1646 无性别子句不受影响。
//
// ⚠️ 双行判别（逐宏定案记档）：1646/1565 两行均同宠同 kind 同参数个数，唯一运行期
//   差异=行参数值（1646：db[3]/db[4]/db[6]/db[7] 四位；1565：db[2]/db[5] 两位）
//   ——纯 args 驱动，无需行别通道，实现零硬编码行值（K400 1237「args 档位差」同判）。
//
// ⚠️ 口径记档（majority reading，逐条按先例）：
//   ①【1646A 睡眠翻倍归属】"（处于睡眠状态下概率翻倍）"无主语。两读：对手睡
//     （死亡句宾语续链）/自身睡（孤梦=自身梦态深睡眠）。**取对手睡**：
//     a) 括注紧跟"10%的概率直接死亡"，死亡事件主体=对手（语法续链）；
//     b) 本文本自/对标记极严（同串"自身受到攻击时""使对手造成的伤害""自身攻击命中
//        时"逐处标记），无标记转折默认延续最近子句主体；
//     c) 语料同族母题"若对手处于睡眠状态则威力翻倍"（3 处技能文）——睡眠=击杀窗口；
//        自身态翻倍文均显式写"自身"（"自身处于能力提升状态时概率翻倍"）。
//     ⚠️ 可达面勘误（实现期实测）：睡眠=**控制类**异常（abnormal-types.h
//     is_control_abnormal_status 含 Sleep）——预置睡眠的对手当回合无法行动，
//     "对手使用攻击"不结（skill_use_seq 零增量实测）；受击方睡眠会被命中解除
//     （battleFsm:239），自身读同样窄（对手攻 miss/被盔才保得住曹操的睡眠）。
//     两读的可达面都窄：对手睡读=中盘上睡（本回合已用攻、战阶末已入睡，外部
//     睡眠源），自睡读=对手攻 miss 面。按 a/b/c 取对手睡；判定时机=ROUND_END
//     掷点时现查（CoreApi has_active_anomaly 包装，非 inline 直调不可——
//     core_api.h:218 口径）。场景以 debug 步进注入"已用攻后入睡"白盒夹具断言
//     （scenario_036 S5 同款）。
//   ②【1646A "未触发"】= 死亡掷点 ROLL_FAILED 单档（K396 majority："未触发则"
//     补偿门认概率掷败档，触发缺席≠未触发——对手没用攻击则整段不结、无补偿）。
//   ③【1646A 门快照】"回合开始时若自身处于异常状态"= ROUND_START
//     has_any_active_abnormal_status(owner) 置当回合戳（K1040 满体旗同形——中盘
//     上的异常不追认）。戳=roundCount+1（0 基防撞，K394 T2/K402 口径）。
//   ④【1646A 直接死亡】= force_hp_to_zero(opp, owner)（秒杀族唯一入口；actor=印主，
//     归因臂戳由原语写 last_damage_actor——K402 1475 死亡归因族通道）。存活门由
//     TARGET_DOWN 天然承担。
//   ⑤【1646B 减半相位】"自身受到攻击时…使对手造成的伤害减半"= REDUCE_PCT +
//     MITIGATE（k184 272 同位同形态；掷点在回调内现掷——管线内掷 rand 有 36 命中
//     1% 秒杀先例）。"减半"字面 ×1/2（无幅度参，K402 1630"翻倍=×2 固定"同纪律）。
//     逐段独立掷（每次红伤结算各一跳）。
//   ⑥【1646B/C 先制 pending】"使对手下回合先制-1"/"使自身下回合先制+1"= 金额账+
//     戳双键（K402 1475 pending 戳同构）：掷中当场累加金额、戳=roundCount+1；
//     次回合 FMR 桶（memset 后写）戳==roundCount 才消费、消费即清。多段命中叠加
//     金额（一次消费全结）。宿主缺席 FMR 不消费、戳过期哑火（K394 ③ 记档口径）。
//   ⑦【1646B/C 爬坡】"下回合触发概率提升20%/30%"= 掷败当场 ledger += db[4]/db[7]
//     （pity 爬坡，无 cap 字面——roll_percent ≥100 短路即事实上封顶，记档）；
//     掷中不爬不重置（行文无重置子句，K402 1630"触发爬坡"反例知悉——本宏爬坡在
//     失败侧，机制不对称属官方行文原样）。
//   ⑧【1646B/C 未触发补偿时点】"战斗阶段结束时"= ROUND_END 节点消费当回合失败旗
//     （K1040 兜底同形）：B 旗→heal_amount 自身 maxHP/3（K7 1673 同文"恢复自身最大
//     体力1/3"）；C 旗→reduce_max_hp_pct(opp, 100/3=33, floor=0)（K7 1673 同文
//     "减对手最大体力1/3"直系——1/3→33 整数取整记档）。同回合 B/C 双败两补偿独立
//     并结（两个独立 if）。旗 ROUND_END 消费即清、enter 兜底清（不跨回合残留）。
//   ⑨【1646C 触发位】"自身攻击命中时"= DETECT 相位 own 红伤票（attackerId==owner+
//     isRed+final>0；"命中"→ miss 无伤害管线不触发、属性技无伤害相天然排除——
//     K405 2477②/K406 1463① 同门）。翻倍=×2 固定（K402 1630 同文同判）。
//   ⑩【1565② "对手为雄性精灵时"=常驻门】非"首次登场当回合"限定（首登限定只属
//     ①句——"首次登场时，若…则…；对手为雄性精灵时，当回合…"两句独立分号隔开；
//     ②句无首登字样）。四 rider（增伤/低伤附加/受击减伤/高伤害怕）全部雄性门内。
//   ⑪【1565② 增伤采样】"当回合自身造成的攻击伤害提升50%"= ROUND_START 现场查
//     对手性别→ws.damage_add_pct[own] += db[1]（K399 1264 回合开始采样整回合钉死
//     直系；条件非"回合开始"字面锚但"当回合"整回合语义同判，记档）。中盘换入的
//     重登场回合不补采样（enter 不写——初登场天然早于 R1 ROUND_START，重登场
//     余回合无增伤属采样口径边角，记档）。对手性别中盘不变（性别是精灵常量），
//     只有换人才变——换人轮次下回合 ROUND_START 重采。
//   ⑫【1565② 受击减伤】= REDUCE_PCT + MITIGATE 活票（k184 直系：回调内**现读**
//     attacker.gender==MALE 才 ×(100-db[4])/100——K399 采样形会漏换人后余段，
//     管线票现场判最忠实）。
//   ⑬【1565② 低伤附加】"造成的伤害低于300时附加伤害值100%的固定伤害"= DETECT 相位
//     own 红伤票：d.final < db[2] → deal_pink_damage(opp, d.final×db[3]/100, FIXED)
//     （K406 1460② 877 直系 DETECT 相位同步附伤；"造成的伤害"=含增伤后的落地值
//     ——DETECT 晚于 AMP 公式相，d.final 已含 ⑪ 增伤）。逐段独立判。
//   ⑭【1565② 高伤害怕】"受到的伤害高于400时100%令对手害怕"= EVENT_TAKE_DAMAGE
//     watcher（target=own/actor≠own/攻击伤害时点/K402 ⑦ 同门；ev.amount=实扣值
//     ——受击减伤 ⑫ 之后，"受到的伤害"字面）> db[5] → 100% 申报制害怕
//     （apply_anomaly_checked pct=db[6]，审计槽掷前写；落地计数侧证）。
//   ⑮【1565① 首次登场】= enter 节点 + plugin_storage 首登旗（K401 888 k888First
//     同形）：旗未置→置旗（**无论性别门过否都算"首次登场已发生"**）+对手雄性→
//     100% 申报（审计槽）+ break_round_effects(opp) + clear_stat_boosts(opp)
//     （两原语并列无"消除成功则"链，各自独立执行）。
//   ⑯【boss 尾注】两行（boss无效）均为 tips 末位全句尾注 → 全节点 boss_invalid=true
//     （K402 ⑤/K403/K405 全句尾口径；内核 soul_mark_manager:450 boss 局不注册不执行
//     ——enter 不挂、管线票不存在）；early 挂的管线票/observer 回调内再手动
//     is_boss_challenge 双保险（K391/K405 口径）。
//   ⑰【1646 db[1] 锚定】"未触发则令对手全属性-1"的 -1 = db[1]（两行同值 1；与
//     db[3]/db[6] 先制量互证 8 参无废位）。"全属性"= stat_change 六槽循环含命中
//     （K402 ⑧ k242/k893 → K399 口径）。
//
// ⚠️ 三查记录（a01-knight11 2026-11-03，worktree 内复核）：
//   ①内核 grep -w 1646/1565（src/ include/）零命中（无魂印行为硬编码）；
//   ②effect_info 两 id 均为技能空间异文模板（1646"全属性+{0}致命裂痕"、1565
//     "PP归零"）——魂印语义权威=effect_icon tips（SOP §六.1）；kind=[1,5,12]/[1,8,10]；
//   ③soul_lib 全库 grep 无 SOUL_PROGRAM(1646/1565)；registry soul 基线 384 无两 id
//     （回归后落盘复核 soul=386=384+2、1646/1565 均入账，moves=1411/suit=4）；
//     号段 7650-7659 全分支无撞。
// 存储键段：1016460000+owner*16+槽（1646，槽 0..15）/ 1015650000+owner*16+槽
//   （1565，槽 0..5），10^9+id×10^4 段（K406 同式），tools/check_storage_keys.py 门禁。
// 注册：SOUL_PROGRAM ×2（1646/1565 入账，只增不改）。
// 场景回归：test/scenario/scenario_7650_solitude_dream_gender.cpp（号段 7650-7659）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <entities/elf-pet.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark1646 = 1646;
constexpr int kMark1565 = 1565;

constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);    // 8
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);      // 6
constexpr int kCombatStatCount = 6;   // 六槽含命中（K402 ⑧ 口径）
constexpr int kOwnerStride = 16;      // 每 owner 键段步长

// ── 存储键段（base + owner*16 + 槽；10^9+id×10^4 段，grep 无撞）──────────────
constexpr int k1646Base = 1016460000;
constexpr int k1646AStamp = 0;        // 孤梦噬魂当回合活跃戳（=roundCount+1；0=无）
constexpr int k1646BFail = 1;         // 减半支当回合掷败旗（1/0）
constexpr int k1646CFail = 2;         // 翻倍支当回合掷败旗
constexpr int k1646BPendAmt = 3;      // 减半支对手先制 pending 金额（累加）
constexpr int k1646BPendStamp = 4;    // 减半支 pending 戳（=roundCount+1）
constexpr int k1646CPendAmt = 5;      // 翻倍支自身先制 pending 金额
constexpr int k1646CPendStamp = 6;    // 翻倍支 pending 戳
constexpr int k1646BLedger = 7;       // 减半概率账（0=未记→base db[2]）
constexpr int k1646CLedger = 8;       // 翻倍概率账（0=未记→base db[5]）
constexpr int k1646DeathAudit = 9;    // 死亡申报概率（审计槽，掷前写）
constexpr int k1646DeathLand = 10;    // 死亡落地计数（统计带侧证）
constexpr int k1646HealLand = 11;     // B 补偿回血计数（侧证）
constexpr int k1646CutLand = 12;      // C 补偿削上限计数（侧证）
constexpr int k1646Wired = 13;        // 管线票已挂旗
constexpr int k1646BAudit = 14;       // 减半申报概率（审计槽，掷前写）
constexpr int k1646CAudit = 15;       // 翻倍申报概率（审计槽，掷前写）

constexpr int k1565Base = 1015650000;
constexpr int k1565FirstDone = 0;     // 首次登场已发生旗
constexpr int k1565Wired = 1;         // 管线票/observer 已挂旗
constexpr int k1565BreakAudit = 2;    // 首登消除申报概率（审计槽）
constexpr int k1565BreakLand = 3;     // 首登消除落地计数（侧证）
constexpr int k1565FearAudit = 4;     // 害怕申报概率（审计槽，掷前写）
constexpr int k1565FearLand = 5;      // 害怕落地计数（统计带侧证）

// ── 共用小件（HostRow/db_at 原序读 owned_int_args，K400/K402 定例）──────────

struct HostRow {
    const std::vector<int>* db = nullptr;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const SoulNodeArgs& a, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

int read_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

void write_int(BattleContext* ctx, int key, int v) {
    ctx->plugin_storage[key] = v;
}

void erase_int(BattleContext* ctx, int key) {
    ctx->plugin_storage.erase(key);
}

// const 版在场守卫（管线票/observer 语境）。
bool host_onstage_const(const BattleContext* c, int owner, int mark_id) {
    if (!c || owner < 0 || owner > 1 || c->on_stage[owner] < 0
        || c->on_stage[owner] > 5) {
        return false;
    }
    return c->seerRobot[owner].elfPets[c->on_stage[owner]].soulMark.id == mark_id;
}

// 对手在场宠是否雄性（性别门唯一判据：Gender::MALE，k184 首读者同判）。
bool foe_is_male(const BattleContext* c, int owner) {
    if (!c || owner < 0 || owner > 1) {
        return false;
    }
    const int foe = 1 - owner;
    if (c->on_stage[foe] < 0 || c->on_stage[foe] > 5) {
        return false;
    }
    const ElfPet& foe_pet = c->seerRobot[foe].elfPets[c->on_stage[foe]];
    return foe_pet.hp > 0 && foe_pet.gender == Gender::MALE;
}

// 执行判据私副本（速查 §四点五；K402 同款）。
bool executed_skill_this_round(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->skill_use_seq[owner] > ctx->ws.skill_use_seq_round_start[owner];
}

bool executed_skill_is_attack(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->ws.skill_type_view[owner] != static_cast<int>(SkillType::Attribute);
}

// 攻击伤害事件时点（K402 ⑦ 同款私副本）。
bool attack_damage_event(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1646 魏武帝·曹操：孤梦噬魂三段
//   A 回合开始自身异常→（对手用攻→战阶末 db[0]% 直接死亡[对手睡眠×2]／掷败→对手
//     全属性-db[1]）｜B 受攻 db[2]% 减半+对手下回合先制-db[3]／掷败→战阶末自回
//     maxHP/3+爬坡 db[4]｜C 命中 db[5]% 翻倍+自身下回合先制+db[6]／掷败→战阶末
//     削对手 maxHP/3+爬坡 db[7]
// ═════════════════════════════════════════════════════════════════════════════

// enter(early)：接线两管线票（一场一次，TEAM 常驻+回调 live 在场门）；回合域残账
// 清（A 戳/B/C 失败旗——pending 金额/戳不清：掷中后离场 K394 ③"过期哑火"口径）。
EffectResult k407_1646_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1646Base + owner * kOwnerStride;
    erase_int(ctx, base + k1646AStamp);
    erase_int(ctx, base + k1646BFail);
    erase_int(ctx, base + k1646CFail);
    if (read_int(ctx, base + k1646Wired) == 1) {
        return EffectResult::kOk;
    }
    write_int(ctx, base + k1646Wired, 1);
    const HostRow row = host_row(ctx, a, kMark1646);
    const int b_base_pct = db_at(row, 2, 50);   // db[2]，兜底本体字面 50
    const int b_step = db_at(row, 4, 20);       // db[4]，兜底本体字面 20
    const int b_pl = db_at(row, 3, 1);          // db[3]，兜底本体字面 1
    const int c_base_pct = db_at(row, 5, 50);   // db[5]，兜底本体字面 50
    const int c_step = db_at(row, 7, 20);       // db[7]，兜底本体字面 20
    const int c_pl = db_at(row, 6, 1);          // db[6]，兜底本体字面 1

    // ── B 票：REDUCE_PCT + MITIGATE（k184 同位；受击侧回调内掷点）────────────
    ctx->register_default_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner, base, b_base_pct, b_step, b_pl](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != owner || !d.isRed || d.final <= 0) {
                return;   // 只认打向自身的攻击伤害（k184 同门）
            }
            if (!host_onstage_const(c, owner, kMark1646) || c->is_boss_challenge) {
                return;   // boss 双保险（K391 管线票口径）
            }
            const int pct = read_int(c, base + k1646BLedger) > 0
                                ? read_int(c, base + k1646BLedger)
                                : b_base_pct;   // 0=未记→基础档
            write_int(c, base + k1646BAudit, pct);   // 审计槽：掷前写申报值
            if (!roll_percent(pct)) {
                // 掷败：当回合失败旗（ROUND_END 补偿）+ 爬坡（记档⑦）。
                write_int(c, base + k1646BFail, 1);
                write_int(c, base + k1646BLedger, pct + b_step);   // 爬坡（记档⑦）
                return;
            }
            // 掷中：减半（字面 ×1/2，记档⑤）+ 对手下回合先制 -b_pl（记档⑥）。
            d.final = d.final / 2;
            write_int(c, base + k1646BPendAmt, read_int(c, base + k1646BPendAmt) + b_pl);
            write_int(c, base + k1646BPendStamp, c->roundCount + 1);
        });

    // ── C 票：DETECT（attacker 侧；K402 1630 翻倍同位）────────────────────────
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, base, c_base_pct, c_step, c_pl](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != 1 - owner || !d.isRed
                || d.final <= 0) {
                return;   // 自身攻击红伤才消费（记档⑨）
            }
            if (!host_onstage_const(c, owner, kMark1646) || c->is_boss_challenge) {
                return;
            }
            const int pct = read_int(c, base + k1646CLedger) > 0
                                ? read_int(c, base + k1646CLedger)
                                : c_base_pct;
            write_int(c, base + k1646CAudit, pct);   // 审计槽：掷前写申报值
            if (!roll_percent(pct)) {
                write_int(c, base + k1646CFail, 1);
                write_int(c, base + k1646CLedger, pct + c_step);   // 爬坡（记档⑦）
                return;
            }
            // 掷中：翻倍（×2 固定，记档⑨）+ 自身下回合先制 +c_pl（记档⑥）。
            d.final = d.final * 2;
            write_int(c, base + k1646CPendAmt, read_int(c, base + k1646CPendAmt) + c_pl);
            write_int(c, base + k1646CPendStamp, c->roundCount + 1);
        });
    return EffectResult::kOk;
}

// ROUND_START：孤梦噬魂门快照（记档③）——自身处于任意异常 → 置当回合活跃戳。
EffectResult k407_1646_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1646Base + owner * kOwnerStride;
    if (!onstage_has_soul_mark(ctx, owner, kMark1646)) {
        return EffectResult::kOk;
    }
    if (ctx->has_any_active_abnormal_status(owner)) {
        write_int(ctx, base + k1646AStamp, ctx->roundCount + 1);
    }
    return EffectResult::kOk;
}

// FMR：消费两支 pending 先制（memset 后写，K394 ③/K402 ⑥ 同构；戳==当回合才结）。
EffectResult k407_1646_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1646Base + owner * kOwnerStride;
    if (!onstage_has_soul_mark(ctx, owner, kMark1646)) {
        return EffectResult::kOk;   // 宿主缺席不消费——戳过期哑火（K394 ③ 口径）
    }
    const int b_stamp = read_int(ctx, base + k1646BPendStamp);
    if (b_stamp > 0 && b_stamp == ctx->roundCount) {
        const int amt = read_int(ctx, base + k1646BPendAmt);
        if (amt > 0) {
            ctx->ws.preemptive_level[1 - owner] -= amt;   // 对手下回合先制-
        }
        erase_int(ctx, base + k1646BPendAmt);
        erase_int(ctx, base + k1646BPendStamp);
    }
    const int c_stamp = read_int(ctx, base + k1646CPendStamp);
    if (c_stamp > 0 && c_stamp == ctx->roundCount) {
        const int amt = read_int(ctx, base + k1646CPendAmt);
        if (amt > 0) {
            ctx->ws.preemptive_level[owner] += amt;   // 自身下回合先制+
        }
        erase_int(ctx, base + k1646CPendAmt);
        erase_int(ctx, base + k1646CPendStamp);
    }
    return EffectResult::kOk;
}

// ROUND_END：A 死亡掷/全属性降 + B/C 未触发补偿（记档②⑧）。
EffectResult k407_1646_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->force_hp_to_zero || !g_core->stat_change
        || !g_core->heal_amount || !g_core->reduce_max_hp_pct) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark1646)) {
        return EffectResult::kOk;
    }
    const int base = k1646Base + owner * kOwnerStride;
    const int foe = 1 - owner;
    const HostRow row = host_row(ctx, a, kMark1646);

    // ── A：孤梦噬魂当回合活跃 + 对手使用攻击 → 掷死亡（记档②③④）──────────
    const int a_stamp = read_int(ctx, base + k1646AStamp);
    erase_int(ctx, base + k1646AStamp);   // 当回合戳必消费（活跃与否都清）
    if (a_stamp > 0 && a_stamp == ctx->roundCount + 1) {
        const int foe_slot = ctx->on_stage[foe];
        if (foe_slot >= 0 && foe_slot <= 5 && ctx->seerRobot[foe].elfPets[foe_slot].hp > 0
            && executed_skill_this_round(ctx, foe) && executed_skill_is_attack(ctx, foe)) {
            const int death_base = db_at(row, 0, 10);   // db[0]，兜底字面 10
            int pct = death_base;
            // 睡眠翻倍：对手处于睡眠态 → ×2（记档①——majority reading 对手睡）。
            // 查询走 CoreApi 包装（has_active_abnormal_status 非 inline 插件不可直调，
            // core_api.h:218 口径；K6 虚拟异常视图层一并生效）。
            if (g_core->has_active_anomaly && g_core->has_active_anomaly(ctx, foe, kSleep)) {
                pct = death_base * 2;
            }
            write_int(ctx, base + k1646DeathAudit, pct);   // 审计槽：掷前写申报值
            if (roll_percent(pct)) {
                (void)g_core->force_hp_to_zero(ctx, foe, owner);   // 秒杀通道（记档④）
                write_int(ctx, base + k1646DeathLand,
                          read_int(ctx, base + k1646DeathLand) + 1);
            } else {
                // 未触发补偿：对手全属性 -db[1]（六槽含命中，记档⑰）。
                const int down = db_at(row, 1, 1);
                if (down > 0) {
                    for (int i = 0; i < kCombatStatCount; ++i) {
                        (void)g_core->stat_change(ctx, foe, i, -down);
                    }
                }
            }
        }
    }

    // ── B 补偿：当回合减半掷败旗 → 战阶末恢复自身最大体力 1/3（记档⑧）──────
    if (read_int(ctx, base + k1646BFail) == 1) {
        erase_int(ctx, base + k1646BFail);
        const ElfPet& self = ctx->getPet(owner);
        if (self.hp > 0) {
            const int max_hp = resolve_pet_max_hp(self);
            if (max_hp / 3 > 0) {
                (void)g_core->heal_amount(ctx, owner, max_hp / 3);
                write_int(ctx, base + k1646HealLand,
                          read_int(ctx, base + k1646HealLand) + 1);
            }
        }
    }
    // ── C 补偿：当回合翻倍掷败旗 → 战阶末对手减少最大体力 1/3（K7 1673 直系）──
    if (read_int(ctx, base + k1646CFail) == 1) {
        erase_int(ctx, base + k1646CFail);
        const int foe_slot2 = ctx->on_stage[foe];
        if (foe_slot2 >= 0 && foe_slot2 <= 5
            && ctx->seerRobot[foe].elfPets[foe_slot2].hp > 0) {
            (void)g_core->reduce_max_hp_pct(ctx, foe, foe_slot2, 100 / 3, 0);
            write_int(ctx, base + k1646CutLand, read_int(ctx, base + k1646CutLand) + 1);
        }
    }
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1565 闭月之姿·貂蝉：首登雄性消除 + 雄性常驻 aura（增伤/低伤附加/
//   受击减伤/高伤害怕）
// ═════════════════════════════════════════════════════════════════════════════

// enter(early)：首登旗消费（记档⑮）+ 管线票/observer 接线（一场一次）。
EffectResult k407_1565_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->break_round_effects || !g_core->clear_stat_boosts
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int base = k1565Base + owner * kOwnerStride;
    // ── ① 首次登场（旗无论门过否都置——"首次登场已发生"，记档⑮）────────────
    if (read_int(ctx, base + k1565FirstDone) != 1) {
        write_int(ctx, base + k1565FirstDone, 1);
        const HostRow row = host_row(ctx, a, kMark1565);
        const int declare = db_at(row, 0, 100);   // db[0]，兜底字面 100
        if (!ctx->is_boss_challenge && foe_is_male(ctx, owner) && declare > 0) {
            write_int(ctx, base + k1565BreakAudit, declare);   // 审计槽
            (void)g_core->break_round_effects(ctx, 1 - owner);
            (void)g_core->clear_stat_boosts(ctx, 1 - owner);
            write_int(ctx, base + k1565BreakLand,
                      read_int(ctx, base + k1565BreakLand) + 1);
        }
    }
    // ── 接线（一场一次）──────────────────────────────────────────────────────
    if (read_int(ctx, base + k1565Wired) == 1) {
        return EffectResult::kOk;
    }
    write_int(ctx, base + k1565Wired, 1);
    const HostRow row = host_row(ctx, a, kMark1565);
    const int low_gate = db_at(row, 2, 300);    // db[2]，兜底本体字面 300
    const int attach_pct = db_at(row, 3, 100);  // db[3]，兜底字面 100
    const int mitigate_pct = db_at(row, 4, 50); // db[4]，兜底本体字面 50
    const int high_gate = db_at(row, 5, 400);   // db[5]，兜底本体字面 400
    const int fear_pct = db_at(row, 6, 100);    // db[6]，兜底字面 100

    // ② 受击减伤票（REDUCE_PCT + MITIGATE；k184 直系——回调内现读攻方性别，记档⑫）。
    ctx->register_default_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner, mitigate_pct](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || mitigate_pct <= 0) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!host_onstage_const(c, owner, kMark1565) || c->is_boss_challenge) {
                return;
            }
            // 雄性来源门：攻击方在场宠 gender == MALE（k184 同判）。
            if (d.attackerId < 0 || d.attackerId > 1) {
                return;
            }
            const ElfPet& attacker =
                c->seerRobot[d.attackerId].elfPets[c->on_stage[d.attackerId]];
            if (attacker.gender != Gender::MALE) {
                return;
            }
            d.final = d.final * (100 - mitigate_pct) / 100;
        });

    // ② 低伤附加票（DETECT own 红伤；K406 1460② 877 直系同步附伤，记档⑬）。
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, low_gate, attach_pct](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != 1 - owner || !d.isRed
                || d.final <= 0) {
                return;   // 自身攻击红伤（miss/属性技天然排除）
            }
            if (!host_onstage_const(c, owner, kMark1565) || c->is_boss_challenge) {
                return;
            }
            if (d.final >= low_gate) {
                return;   // "造成的伤害低于{db[2]}"（严格不等号）
            }
            if (!foe_is_male(c, owner)) {
                return;   // 雄性常驻门（记档⑩）
            }
            const int attach = d.final * attach_pct / 100;
            if (attach > 0) {
                (void)g_core->deal_pink_damage(c, 1 - owner, attach, DamageKind::FIXED,
                                               owner);
            }
        });

    // ② 高伤害怕 observer（EVENT_TAKE_DAMAGE 攻击伤害时点；记档⑭）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, high_gate, fear_pct](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.target != owner || ev.actor == owner || ev.amount <= 0
                || !attack_damage_event(ev)) {
                return;   // 只认被对手攻击命中（K402 ⑦ 同门）
            }
            if (!host_onstage_const(c, owner, kMark1565) || c->is_boss_challenge) {
                return;
            }
            if (ev.amount <= high_gate) {
                return;   // "受到的伤害高于{db[5]}"（实扣值，减伤后——记档⑭）
            }
            if (ev.actor < 0 || ev.actor > 1) {
                return;
            }
            const ElfPet& attacker = c->seerRobot[ev.actor].elfPets[c->on_stage[ev.actor]];
            if (attacker.gender != Gender::MALE) {
                return;   // 雄性常驻门
            }
            write_int(c, k1565Base + owner * kOwnerStride + k1565FearAudit,
                      fear_pct);   // 审计槽：掷前写申报值
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, ev.actor, kFear, random_anomaly_duration(), owner, fear_pct,
                ChanceSource::SoulMark);
            if (r == ApplyAnomalyResult::SUCCESS
                || r == ApplyAnomalyResult::DURATION_EXTENDED
                || r == ApplyAnomalyResult::REPLACED_EXISTING
                || r == ApplyAnomalyResult::CONVERTED) {
                write_int(c, k1565Base + owner * kOwnerStride + k1565FearLand,
                          read_int(c, k1565Base + owner * kOwnerStride + k1565FearLand)
                              + 1);   // 落地计数（统计带侧证）
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ROUND_START：雄性常驻 aura 采样——对手雄性 → 当回合增伤 db[1]%（记档⑪）。
EffectResult k407_1565_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1565)) {
        return EffectResult::kOk;
    }
    if (!foe_is_male(ctx, owner)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1565);
    const int amp = db_at(row, 1, 50);   // db[1]，兜底本体字面 50
    if (amp > 0) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(amp);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +2（1646/1565 入账，只增不改）────
// boss 尾注：两行（boss无效）全为 tips 末位全句尾注 → 全节点 boss_invalid=true
//（记档⑯；内核 soul_mark_manager boss 局不注册不执行）。
SOUL_PROGRAM(kMark1646, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k407_1646_enter,
                               /*once=*/false, /*early=*/true, SoulScope::STAGE,
                               /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k407_1646_round_start,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k407_1646_fmr,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_END, &k407_1646_round_end,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true)});

SOUL_PROGRAM(kMark1565, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k407_1565_enter,
                                   /*once=*/false, /*early=*/true, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k407_1565_round_start,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true)});
