// K408 六异常全家桶/提升转嫁族 —— effect 1805/1668（2 宏 / 4 行，双行档位差）
//（看板 K408「六异常全家桶/提升转嫁族 2026-11-02 编排挖掘」，a01-knight12，
//  worktree ~/.agent-workspaces/.../agents/k408；号段 7660-7669；
//  通道=1805=K401 888 attach_random_anomalies 六池直系（独立掷=逐异常
//  apply_anomaly_checked 申报——与 888「三选一」结构不同，本条是六连掷，审计槽
//  逐异常）+ 1668=吸取提升=transfer_stat_boosts（效果85转化/1287吸取共用原语）+
//  镜像赋降=stat_drop 对称槽 + 免疫路 drain_hp（K399/K403 1856 同构）+
//  随机 PP 归零=pp_zero_slot 循环（k230 同款））
//
// 行清单（tools/soul_effects_dump.py 1805 1668，2026-11-03）：
//   1805 拉克希斯 4216 两行（**args 六位概率档位差**，纯 args 行≠宏、无运行期判别）：
//     row1434 本体 args[20,20,20,20,20,20,1,1] kind[1,7,10]：
//       「自身使用攻击技能则20%令对手中毒、20%令对手烧伤、20%令对手冻伤、20%令对手
//        麻痹、20%令对手睡眠、20%令对手害怕，未触发则令自身全属性+1且随机为自身附加
//        中毒、烧伤、冻伤中的1种异常状态；战斗阶段结束时，若自身处于异常状态则下回合
//        开始时解除自身异常状态且当回合攻击必定打出致命一击（boss无效）」
//     row1703 伙伴行（光之女神伙伴效果）args[30×6,1,1]：同文、六率 30%。
//     → db[0..5]=六异常申报率（tips 序：中毒/烧伤/冻伤/麻痹/睡眠/害怕——两行六位
//       同值、排列序风险归零，仍按 tips 序映射并记档）；db[6]=补偿全属性+1 量（1，
//       tips 互锚）；db[7]=补偿随机附加种数（1，tips"中的1种"互锚）。
//   1668 混沌界神 4075 两行（**db[1]/db[2]/db[3] 档位差**，纯 args 行≠宏）：
//     row1320 本体 args[3,1,60,60,1] kind[5,11,12]：
//       「自身使用技能后吸取对手能力提升状态，之后将自身相应的能力提升状态赋予给对手
//        相应的能力下降状态，若对手免疫则{吸取}对手最大{体力}1/3且令对手随机1项技能
//        PP值归零；|回合开始时，自身每处于1种能力提升状态则战斗阶段结束时恢复60点
//        体力，对手每处于1种能力下降状态则战斗阶段结束时损失60点体力（boss无效）」
//     row1544 伙伴行（六界神谕伙伴效果）args[3,2,70,70,1]：同文、随机2项 PP 归零、
//       恢复/损失 70 点。
//     → db[0]=免疫路吸取分母（3 恒，"1/3"互锚）；db[1]=免疫路随机 PP 归零项数
//       （1/2）；db[2]=战阶末每提升状态回复量（60/70）；db[3]=战阶末每下降状态损失
//       量（60/70）；db[4]=1 旗不消费（（boss无效）尾注由行文定、静态节点
//       boss_invalid 置位；db[4] 两行恒 1 与行文互锚记档）。
//
// ⚠️ 口径记档（majority reading，均按先例）：
//   【1805① 主支】攻击技窗 = EVENT_SKILL_USED own+!used_attribute（E13 使用即发、
//     miss 也算——K396 ③/K164 116 定例）；六码按 tips 序逐个 apply_anomaly_checked
//     (db[i])——六连掷**独立掷**（board 行钦定；与 888 池内三选一单掷结构不同），
//     每掷申报前写**逐异常审计槽**（K662/F9 形：掷前写申报率）。对手已倒地不掷
//     （K401 888 同守卫——不掷则也不走补偿，majority：官方"令对手…"对已倒地无义）。
//   【1805② 补偿】"未触发"= 六掷 landed 全 0（K403 1772 landed 口径：SUCCESS/
//     EXTENDED/REPLACED/CONVERTED 算落地；ROLL_FAILED/免疫/弹控/抗性都算未触发）。
//     补偿 = ①stat_change 六槽 +db[6]（"全属性"=六项能力等级含命中——chaos_sorensen/
//     jiyuan_ds001/K403 1772 同口径；stat_change 不查免弱=自身增益钦定通道）+
//     ②attach_random_anomalies(自身, {中毒1,烧伤2,冻伤5}, db[7], 100)（"随机为自身
//     附加…中的1种"——100% 申报短路概率门、只耗挑池 rand；目标=自身）。
//   【1805③ 尾支】"战斗阶段结束时，若自身处于异常状态"= ROUND_END 桶查
//     has_any_active_abnormal_status（K230 全池扫口径）→ 武装戳=roundCount+1；
//     "下回合开始时解除自身异常状态且当回合攻击必定打出致命一击"= 下回合 ROUND_START
//     桶（**桶在 resetWorkspace 之后**——battleFsm.cpp handle_BattleRoundStart 实锚，
//     ws.must_crit 写入全回合有效，k_round_recover 227 同位）→ dispel_active_anomalies
//     （效果解除路径原语）+ ws.must_crit[own]=true（暴击掷点 query_usage ①.5 读
//     must_crit——skills.cpp:994 实锚；"当回合"=写入当轮所有攻击技必暴）。武装戳
//     roundCount+1 ≥1 恒非 0（K402/K405/K406"缺失键 0 撞首回合"守卫天然成立）；仅
//     roundCount==stamp 当回合消费、roundCount>stamp 过期显式擦（防换宠跳回合后旧戳
//     迟到误发，K401 补偿窗同款绝对窗口径）。
//   【1805 boss 尾注】"（boss无效）"紧贴尾支句尾 → **仅尾支**两节点 boss_invalid=true
//     （K401 888 仅③口径）；主支无尾注 → boss 局照常六连掷。
//   【1668① 吸取】"自身使用技能后"= EVENT_SKILL_USED own、**无系别过滤**（行文无
//     "攻击"限定——属性技/miss 都算，K175 893"使用后"口径；K186/K403 1856 同窗）。
//     吸取 = transfer_stat_boosts(ctx, from=opp, to=own)（battle_primitives.cpp:1672
//     实锚：from 正等级整体搬到 to、from 清零 to 等量累加钳 +6；from 免消除强化
//     (STAT_CLEAR) 返回 0 且 from 保有提升——"无法被消除或吸取"）。
//   【1668② 镜像赋降】"之后将自身相应的能力提升状态赋予给对手相应的能力下降状态"
//     = 逐槽对称 stat_drop(opp, 槽, own 该槽正等级)——**幅度镜像**（board 行"提升→
//     转嫁下降镜像"，镜像保幅度 majority reading 记档；量级=吸取**后**自身该槽等级，
//     "之后"字面序——偷来的强化也转嫁）。stat_drop 查免弱（IMMUNE）与 AT_FLOOR
//     内核自理。
//   【1668③ 免疫路】"若对手免疫"行为面判（CoreApi 无免弱查询槽）：①对手有提升但
//     transfer 返回 0（被 from 免消除强化挡下）或 ②镜像 stat_drop 返回 IMMUNE（对手
//     免弱 STAT_DROP）→ drain_hp(own, opp, db[0])（原语自理：max/db[0] FIXED 落+
//     heal_impl 等量回、不越存量、封回血生效——battle_primitives.cpp:1943 实锚）+
//     随机 db[1] 项 PP 归零（pp_zero_slot 循环 sbs_rand()%5、guard 100 重试——k230
//     同款）。两判其一即落免疫路（吸取已发生的部分不回滚，majority：行文顺序执行）。
//     对手无提升且自身无提升（无事可做、无免疫信号）→ 不落免疫路。
//   【1668④ 尾支】"回合开始时"锚=ROUND_START 快照双方状态数（自身提升状态数=正等级
//     槽数、对手下降状态数=负等级槽数——"每处于1种"按状态数计、与幅度无关）+
//     回合戳防跨回合残留（换宠中途入场消费旧快照防御，K230 快照账本同款）；"战斗阶段
//     结束时"=ROUND_END：heal_amount(own, db[2]×提升数)（吃封回血）、
//     deal_pink_damage(opp, db[3]×下降数, FIXED)（"损失体力"=FIXED 粉伤——K233
//     固定伤同 kind；数量 0 不调原语）。
//   【1668 boss 尾注】"（boss无效）"全句尾、紧贴尾支 → 仅尾支两节点 boss_invalid；
//     主支（吸取/镜像/免疫路）无尾注 → boss 局照常。
//   【概率单位】1805 db[0..5]=百分比（tips "20%/30%"互锚）；1668 无概率申报
//     （免疫路为条件分支非概率支）。
//
// ⚠️ 三查记录（a01-knight12 2026-11-03，worktree 内复核）：
//   ①内核 git grep -w 1805/1668（src/ include/）零命中——魂印行为零硬编码；
//   ②effect_info 1805/1668 均为技能空间异文模板（1805"若命中前对手不处于异常状态则
//     造成的伤害翻倍"/1668"附加对手当前已损失技能PP值总和×{0}的固定伤害…"）——
//     effect_icon tips 为魂印权威（SOP §六.1，dump 原文已核）；
//   ③registry 无两 id；全 feat 分支（本地+远端）git grep 无 SOUL_PROGRAM(1805/1668)；
//     场景号段 7660-7669 全分支无撞；存储键段 991805xxxx/991668xxxx
//     tools/check_storage_keys.py 全绿。
// 注册：SOUL_PROGRAM ×2（1805/1668 入账，只增不改）。
// 场景回归：test/scenario/scenario_7660_lakehexis_1805.cpp /
//   scenario_7663_chaos_realm_1668.cpp（号段 7660-7669）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

// ── 存储键段（+owner；tools/check_storage_keys.py 已核无撞）──────────────────
constexpr int k1805Wire = 991805000;      // SKILL_USED watcher 已挂旗
constexpr int k1805Audit = 991805010;     // 逐异常申报率审计槽（+i*10+owner，i=0..5）
constexpr int k1805Land = 991805100;      // 逐异常落地计数（统计带侧证，+i*10+owner）
constexpr int k1805Comp = 991805200;      // 六掷全空→补偿触发计数
constexpr int k1805CompAttach = 991805210;  // 补偿随机附加执行计数
constexpr int k1805Stamp = 991805300;     // 尾支武装戳（=roundCount+1，0=无）
constexpr int k1805ArmCnt = 991805310;    // 尾支武装计数（侧证）
constexpr int k1805Dispel = 991805320;    // 尾支解除异常条数（最近一次，侧证）
constexpr int k1805CritCnt = 991805330;   // 尾支必暴置位计数（侧证）

constexpr int k1668Wire = 991668000;      // SKILL_USED watcher 已挂旗
constexpr int k1668Steal = 991668010;     // 吸取搬走项数累计（侧证）
constexpr int k1668Mirror = 991668020;    // 镜像赋降落地槽次累计（侧证）
constexpr int k1668Drain = 991668030;     // 免疫路吸取计数（侧证）
constexpr int k1668PpZero = 991668040;    // 免疫路 PP 归零槽次累计（侧证）
constexpr int k1668SnapOwn = 991668050;   // 尾支快照：自身提升状态数（本回合）
constexpr int k1668SnapOpp = 991668060;   // 尾支快照：对手下降状态数（本回合）
constexpr int k1668SnapRc = 991668070;    // 尾支快照回合戳（防跨回合残留）
constexpr int k1668HealLast = 991668080;  // 尾支最近一次回复量（侧证）
constexpr int k1668DmgLast = 991668090;   // 尾支最近一次损失量（侧证）

// 1805 六异常池（tips 行文序：中毒、烧伤、冻伤、麻痹、睡眠、害怕；码=abnormal-types.h）。
constexpr int k1805Pool[6] = {
    static_cast<int>(AbnormalStatusId::Poison),     // 1 中毒
    static_cast<int>(AbnormalStatusId::Burn),       // 2 烧伤
    static_cast<int>(AbnormalStatusId::Frostbite),  // 5 冻伤
    static_cast<int>(AbnormalStatusId::Paralysis),  // 0 麻痹
    static_cast<int>(AbnormalStatusId::Sleep),      // 8 睡眠
    static_cast<int>(AbnormalStatusId::Fear),       // 6 害怕
};
constexpr int k1805PoolSize = 6;

// 1805 补偿附加池（"随机为自身附加中毒、烧伤、冻伤中的1种"——tips 行文序）。
constexpr int k1805CompPool[3] = {
    static_cast<int>(AbnormalStatusId::Poison),     // 1
    static_cast<int>(AbnormalStatusId::Burn),       // 2
    static_cast<int>(AbnormalStatusId::Frostbite),  // 5
};
constexpr int k1805CompPoolSize = 3;

// ── 共用小件（db_at 原序读 owned_int_args = 方案 B，K172/K403/K400 定例）──────

struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
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
    const ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    row.db = &host.soulMark.args.owned_int_args;
    row.monster_id = host.soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

void write_int(BattleContext* ctx, int key, int v) {
    ctx->plugin_storage[key] = v;
}

int read_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

// K238/K400 落地集（"未触发"只认非这些档）。
bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::CONVERTED;
}

// "全属性±N"（六项能力等级含命中——chaos_sorensen/jiyuan_ds001/K403 1772 口径）。
void boost_all_six(BattleContext* ctx, int target, int delta) {
    if (!ctx || !g_core || !g_core->stat_change || delta == 0) {
        return;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_change(ctx, target, i, delta);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1805 拉克希斯：攻击技六连掷（逐异常独立申报）→ 全空补偿（全属性+1+自附异常）；
//   尾支（boss无效）：异常在身 → 下回合开始解除+当回合必暴
// ═════════════════════════════════════════════════════════════════════════════

// enter：接线六连掷 watcher（一场一次；主支无 boss 尾注 → boss 局照常掷）。
EffectResult k408_1805_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked || !g_core->stat_change
        || !g_core->attach_random_anomalies) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->plugin_storage.count(k1805Wire + owner)) {
        return EffectResult::kOk;   // early 双触发防重（k403/K400 同款）
    }
    ctx->plugin_storage.emplace(k1805Wire + owner, 1);
    const HostRow row = host_row(ctx, a, 1805);
    std::array<int, k1805PoolSize> pcts{};
    for (int i = 0; i < k1805PoolSize; ++i) {
        pcts[i] = db_at(row, i, 20);   // db[0..5]，兜底本体字面 20
    }
    const int comp_boost = db_at(row, 6, 1);   // db[6]，兜底 tips 字面 +1
    const int comp_cnt = db_at(row, 7, 1);     // db[7]，兜底 tips 字面 1 种
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pcts, comp_boost, comp_cnt](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked || !g_core->stat_change
                || !g_core->attach_random_anomalies) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技窗（E13 使用即发，miss 也算）
            }
            if (!onstage_has_soul_mark(c, owner, 1805)) {
                return;
            }
            const int opp = 1 - owner;
            if (c->getPet(opp).hp <= 0) {
                return;   // 对手已倒地不掷（记档：不掷则也不走补偿）
            }
            // 六连掷：逐异常独立申报（审计槽掷前写，K662/F9 形）。
            int landed_total = 0;
            for (int i = 0; i < k1805PoolSize; ++i) {
                write_int(c, k1805Audit + i * 10 + owner, pcts[i]);
                const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                    c, opp, k1805Pool[i], random_anomaly_duration(), owner, pcts[i],
                    ChanceSource::SoulMark);
                if (anomaly_landed(r)) {
                    ++landed_total;
                    write_int(c, k1805Land + i * 10 + owner,
                              read_int(c, k1805Land + i * 10 + owner) + 1);
                }
            }
            if (landed_total > 0) {
                return;   // 任一落地即无补偿（"未触发"=六掷全空）
            }
            // 补偿①：自身全属性 +db[6]（六槽 stat_change，不查免弱钦定通道）。
            boost_all_six(c, owner, comp_boost);
            write_int(c, k1805Comp + owner, read_int(c, k1805Comp + owner) + 1);
            // 补偿②：随机为自身附加中毒/烧伤/冻伤中 db[7] 种（100% 申报短路概率门）。
            if (comp_cnt > 0) {
                const std::vector<int> pool(k1805CompPool,
                                            k1805CompPool + k1805CompPoolSize);
                (void)g_core->attach_random_anomalies(c, owner, pool, comp_cnt,
                                                      /*probability_pct=*/100, owner,
                                                      ChanceSource::SoulMark);
                write_int(c, k1805CompAttach + owner,
                          read_int(c, k1805CompAttach + owner) + 1);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// 尾支·ROUND_END：自身处于异常状态 → 武装下回合（boss_invalid 节点，K401 仅尾支口径）。
EffectResult k408_1805_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, 1805)) {
        return EffectResult::kOk;
    }
    if (!ctx->has_any_active_abnormal_status(a.owner)) {
        return EffectResult::kOk;
    }
    write_int(ctx, k1805Stamp + a.owner, ctx->roundCount + 1);
    write_int(ctx, k1805ArmCnt + a.owner, read_int(ctx, k1805ArmCnt + a.owner) + 1);
    return EffectResult::kOk;
}

// 尾支·下回合 ROUND_START：解除自身异常 + 当回合必暴（桶在 resetWorkspace 后，
// ws.must_crit 全回合有效；boss_invalid 节点）。
EffectResult k408_1805_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, 1805)) {
        return EffectResult::kOk;
    }
    const int key = k1805Stamp + a.owner;
    const int stamp = read_int(ctx, key);
    if (stamp <= 0) {
        return EffectResult::kOk;   // stamp>0 守卫（缺失键 0 撞首回合，K402 勘误）
    }
    if (ctx->roundCount == stamp) {
        ctx->plugin_storage.erase(key);   // 一次性消费
        if (g_core && g_core->dispel_active_anomalies) {
            const int n = g_core->dispel_active_anomalies(ctx, a.owner);
            write_int(ctx, k1805Dispel + a.owner, n);
        }
        ctx->ws.must_crit[a.owner] = true;   // "当回合攻击必定致命"（①.5 掷点读）
        write_int(ctx, k1805CritCnt + a.owner, read_int(ctx, k1805CritCnt + a.owner) + 1);
    } else if (ctx->roundCount > stamp) {
        ctx->plugin_storage.erase(key);   // 过期显式擦（换宠跳回合防迟到误发，记档）
    }
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1668 混沌界神：使用技能 → 吸取对手提升 + 镜像赋降（免疫路：吸 maxHP 1/n
//   +随机 PP 归零）；尾支（boss无效）：回合开始快照 → 战阶末按状态数回复/损失
// ═════════════════════════════════════════════════════════════════════════════

// enter：接线主支 watcher（一场一次；主支无 boss 尾注 → boss 局照常）。
EffectResult k408_1668_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->transfer_stat_boosts || !g_core->stat_drop
        || !g_core->drain_hp || !g_core->pp_zero_slot) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->plugin_storage.count(k1668Wire + owner)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(k1668Wire + owner, 1);
    const HostRow row = host_row(ctx, a, 1668);
    const int drain_denom = db_at(row, 0, 3);   // db[0]，兜底 tips 字面 1/3
    const int pp_cnt = db_at(row, 1, 1);        // db[1]，兜底本体字面 1 项
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, drain_denom, pp_cnt](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->transfer_stat_boosts || !g_core->stat_drop
                || !g_core->drain_hp || !g_core->pp_zero_slot) {
                return;
            }
            if (ev.actor != owner) {
                return;   // 自身使用技能后（无系别过滤——属性技/miss 都算，记档）
            }
            if (!onstage_has_soul_mark(c, owner, 1668)) {
                return;
            }
            const int opp = 1 - owner;
            if (c->getPet(opp).hp <= 0) {
                return;   // 对手已倒地不结算（K401 口径）
            }
            // ① 吸取：对手正等级整体搬到自身（免消除强化挡下返回 0 且对手保有提升）。
            bool opp_has_boost = false;
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                if (c->ability_levels[opp][i] > 0) {
                    opp_has_boost = true;
                    break;
                }
            }
            const int moved = g_core->transfer_stat_boosts(c, opp, owner);
            if (moved > 0) {
                write_int(c, k1668Steal + owner, read_int(c, k1668Steal + owner) + moved);
            }
            // ② 镜像赋降：自身（吸取后）正等级槽 → 对手同槽等量下降（幅度镜像，记档）。
            bool immune = opp_has_boost && moved == 0;   // 判①：吸不进来=对手免疫吸取
            int mirror_landed = 0;
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                const int lv = c->ability_levels[owner][i];
                if (lv <= 0) {
                    continue;
                }
                const StatDropResult r = g_core->stat_drop(c, opp, i, lv, owner);
                if (r == StatDropResult::IMMUNE) {
                    immune = true;   // 判②：赋降被免弱挡下
                } else if (r == StatDropResult::SUCCESS) {
                    ++mirror_landed;
                }
            }
            if (mirror_landed > 0) {
                write_int(c, k1668Mirror + owner,
                          read_int(c, k1668Mirror + owner) + mirror_landed);
            }
            // ③ 免疫路：吸 maxHP 1/db[0] + 随机 db[1] 项 PP 归零。
            if (immune && drain_denom > 0) {
                (void)g_core->drain_hp(c, owner, opp, drain_denom);
                write_int(c, k1668Drain + owner, read_int(c, k1668Drain + owner) + 1);
                if (pp_cnt > 0) {
                    int done = 0;
                    int guard = 0;
                    while (done < pp_cnt && guard < 100) {
                        ++guard;
                        const int slot = sbs_rand() % 5;   // 5 技能槽随机（无限/已零槽原语拒、重试）
                        if (g_core->pp_zero_slot(c, opp, slot, owner)
                            == PpReduceResult::SUCCESS) {
                            ++done;
                        }
                    }
                    write_int(c, k1668PpZero + owner,
                              read_int(c, k1668PpZero + owner) + done);
                }
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// 尾支·ROUND_START：快照自身提升状态数/对手下降状态数（"回合开始时"锚，K230 快照
// 账本同款；回合戳防换宠中途入场消费旧快照——boss_invalid 节点）。
EffectResult k408_1668_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, 1668)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int opp = 1 - owner;
    int own_boost = 0;
    int opp_drop = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            ++own_boost;
        }
        if (ctx->ability_levels[opp][i] < 0) {
            ++opp_drop;
        }
    }
    write_int(ctx, k1668SnapOwn + owner, own_boost);
    write_int(ctx, k1668SnapOpp + owner, opp_drop);
    write_int(ctx, k1668SnapRc + owner, ctx->roundCount);
    return EffectResult::kOk;
}

// 尾支·ROUND_END：每提升状态回 db[2] 点、对手每下降状态损 db[3] 点（boss_invalid 节点）。
EffectResult k408_1668_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1668)) {
        return EffectResult::kOk;
    }
    // 快照回合戳守卫：只消费本回合的快照（缺/旧 = 本回合无快照，不结）。
    if (read_int(ctx, k1668SnapRc + owner) != ctx->roundCount) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 1668);
    const int heal_per = db_at(row, 2, 60);   // db[2]，兜底本体字面 60
    const int dmg_per = db_at(row, 3, 60);    // db[3]，兜底本体字面 60
    const int own_boost = read_int(ctx, k1668SnapOwn + owner);
    const int opp_drop = read_int(ctx, k1668SnapOpp + owner);
    const int opp = 1 - owner;
    if (own_boost > 0 && heal_per > 0 && ctx->getPet(owner).hp > 0) {
        (void)g_core->heal_amount(ctx, owner, heal_per * own_boost);
        write_int(ctx, k1668HealLast + owner, heal_per * own_boost);
    }
    if (opp_drop > 0 && dmg_per > 0 && ctx->getPet(opp).hp > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, dmg_per * opp_drop, DamageKind::FIXED,
                                       owner);
        write_int(ctx, k1668DmgLast + owner, dmg_per * opp_drop);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +2（1805/1668，只增不改）──────
// 主支 enter 节点无尾注（boss 局也接线，watcher 回调无 boss 门）；尾支全节点
// boss_invalid=true（尾注紧贴尾支句尾，K401 888 仅③口径）。
SOUL_PROGRAM(1805, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k408_1805_enter,
                              /*once=*/false, /*early=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k408_1805_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_END, &k408_1805_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});

SOUL_PROGRAM(1668, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k408_1668_enter,
                              /*once=*/false, /*early=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k408_1668_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_END, &k408_1668_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});
