// K405 PP 交互族二刀 —— effect 2475/2477/2510（3 宏 / 6 行，双行档位差，2026-11-02
// 编排挖掘；看板 K405，a01-knight9 2026-11-03，worktree ../agents/k405；
// 号段 7630-7639。通道=PP 总和查询=roster/skills 遍历（K398 848 pp_reduce 对手全技
// 同构反向读）＋减半次数票=消费型票（K1 CAP 凭证消费链 697 同构——每次伤害消费一张，
// 相位查）＋PP 恢复=pp_restore 原语（K403 1856 已实证）＋PP 随机重置=逐槽写值＋
// 伤害+30/40%=damage_add_pct 条件窗（K399 1264 同构）。dump 全文先行。）
//
// 行清单（tools/soul_effects_dump.py 2475 2477 2510，2026-11-03）：
//   2475 埃斯宾诺 4866 两行（kind 同 [3,5]，双行档位差）：
//     row2052 本体 args[0,1,1]：「自身的PP值总和每有对手的1倍，自身受到的固定伤害、
//       百分比伤害减半1次，每次伤害最多减半8次|每回合前8次受到固定伤害、百分比伤害时，
//       …（长子句，板行未列）…额外对其相邻精灵造成等量真实伤害|（boss无效）」
//     row2101 伙伴行（黄金圣徒伙伴效果）args[1,1,1]：同文但「最多减半10次」「前10次」。
//     → db[0]=档位判别位（0=本体档 8 次 / 1=伙伴档 10 次，双行唯一差异位互锚）；
//       db[1]/db[2]=1 旗不消费（子句无数值锚，K1 纪律）。
//     ⚠️ 第二子句（每回合前8/10次受固伤粉伤→恢复对手已损PP/自最高PP技等量失去/归0禁回/
//       PP最低技吸1点PP+吸240体力/未减少对相邻精灵补真伤）——板行未列 → 记档跳过不产码
//       （K403 1680/1856「板行未列子句记档」先例）；其中「相邻精灵」在 1v1 模型无对象，
//       子句本体亦不可完整落地。
//   2477 康迪亚克 4870 两行（kind 同 [1,9]，纯 args 档位差，无需行判别）：
//     row2057 本体 args[30,400,80,3,1]：「对手pp值总和每比自身高1倍，自身造成的攻击伤害
//       提升30%|回合开始时，若自身存在pp值相等的技能则当回合自身技能命中后恢复所有技能
//       1点pp并吸取对手400点体力，自身pp值每恢复一点则吸取效果减少80点，若不存在pp值
//       相等的技能则当回合必定打出致命一击|战斗阶段结束时…（板行未列）…最高+3|（boss无效）」
//     row2102 伙伴行 args[40,400,40,4,1]：30→40 / 80→40 / 3→4。
//     → db[0]=增伤%（30/40）、db[1]=吸取基数（400）、db[2]=每恢复 1 点递减量（80/40）、
//       db[3]=先制上限（3/4）——属第三子句（板行未列）→ 不消费；db[4]=1 旗不消费。
//     第三子句（战阶末 PP 均值化+每减 1 点下回合先制+1 最高+3）板行未列 → 记档跳过。
//   2510 莱布缇 4902 两行（kind 同 [1,10]，双行档位差）：
//     row2089 本体 args[1,0]：「回合结束后，使自身所有技能的pp值分别随机变化为m-n中的
//       一个值，然后若自身所有技能的pp值总和不低于25则获得所有技能pp值总和*10点护罩且
//       1回合内自身造成的固定伤害和百分比伤害提高25%，若不高于25则获得所有技能已损失
//       pp值总和*10点护盾且1回合内自身免疫受到的异常状态(m=自身体力/100和自身已损失
//       体力/100中的较小项，最小为0；n=自身体力/100和自身已损失体力/100中的较大项，
//       最大为10）|自身技能命中时…（板行未列）…令对手当回合选择的技能损失相等的pp值」
//     row2105 伙伴行 args[1,1]：阈值 25/25 → 20/30。
//     → db[0]=1 旗恒不消费；db[1]=档位判别位（0=本体档 T1=25/T2=25、1=伙伴档 T1=20/
//       T2=30，双行唯一差异位互锚）。阈值 25/20/30、×10 系数、增伤 25% 均不在 args
//       （K381 2518/K355 2381「args 无锚数值」同款）→ tips 字面 fallback 常量锚。
//     ⚠️ 第二子句（自身技能命中→已损PP≥5 令对手神游/<5 令对手当回合选择技损等量PP）
//       板行未列 → 记档跳过不产码。
//
// ⚠️ 口径记档（majority reading，逐条按先例）：
//   【双行判别】逐宏定案（看板行「双行判别方式逐宏定案记档」）：2475 以 db[0] 位判
//     （0→cap8/1→cap10）、2477 纯 args 驱动无判别需求、2510 以 db[1] 位判（0→25/25、
//     1→20/30）。两宏判别位不同是 dump 原始 args 布局使然（2475 差异在首位、2510 在
//     次位），各按双行唯一差异位锚定；阈值不在 args 的数值全走 tips 字面 fallback
//     （K398 1430 k1430SleepPct 同纪律），实现零行数值硬编码。
//   【boss 尾注】三宏 tips 尾注（boss无效）均为独立尾段（收尾于最后一个 | 之后）→
//     按全句尾归属 → 全节点 boss_invalid=true（K403 1856「全句尾→全节点」惯例）；
//     enter 早挂的管线票不随节点走 → 回调内手动 is_boss_challenge 双保险（K391 口径）。
//   【2475 票池】"自身的PP值总和每有对手的1倍"= 票数 k = own_pp_sum / opp_pp_sum
//     （整数除；opp_pp_sum<=0 时无从度量 → k=0 保守档，记档）。票池为**活函数**：
//     每段粉伤结算在相位回调内现查现消费（板行「BEFORE/CAP 相位查」的落地读法），PP
//     总和随用技消耗实时反映——PP 囤积即防御，随 PP 消耗衰减（PP 交互族设计意图）。
//     跨伤害不累积余量（每段独立查表）——"每次伤害消费一张"按每次减半消费一张落地。
//   【2475 相位】"受到的固定伤害、百分比伤害减半"= 粉伤管线 REDUCE_EXTRA（百分比
//     免减·乘法链位，K1 254 粉伤半边同位；板行"BEFORE/CAP 相位"按 K1 254 先例记档
//     落 REDUCE_EXTRA——减半是乘法免减非锁伤，CAP 位语义不合）。每次减半 p.final/=2
//     逐步取整（"减半 N 次"逐步语义），h = min(k, cap)。
//   【2475 PP 总和口径】sum 只累加有限 PP 槽（pp>=0；pp==-1 无限 PP 槽不计——无限
//     无总量语义，记档）；遍历 on-stage 宠 5 槽（K321 全 5 技能口径）。
//   【2477① 采样时点】"对手PP总和每比自身高1倍"在回合开始采样（K399 1264 同构——
//     门条件回合开始评估、ws.damage_add_pct 每回合重写；回合内 PP 消耗不重估，记档）。
//     倍数 k = (opp_sum - own_sum) / own_sum（own>0 且 opp>own 才计；"高1倍"=两倍）。
//   【2477② 相等判定】"存在pp值相等的技能"= 自身 5 槽中存在两槽 pp 相等（只扫有限
//     PP 槽 pp>=0；无限槽不计，记档）。分支在回合开始评估并**整回合钉死**（"当回合"
//     语义；回合内 PP 变化不翻转，记档）。
//   【2477② 命中窗】"当回合自身技能命中后"= DETECT 相位 own 红伤票（attackerId==
//     owner+isRed+final>0；"命中后"→ miss 无伤害管线不触发；属性技无伤害相天然排除
//     ——G5，K398 1430③ 同款）。逐段触发（每次红伤结算各一跳，K164 每次攻击口径）。
//   【2477② 恢复+吸取】pp_restore(自身,1,only_empty=false) 返回实际变化槽数=恢复点数
//     （钳 maxPP、无限槽跳过、不 emit——core_api 恢复族口径）；吸取量 = db[1] -
//     db[2]×恢复点数（≤0 不吸）；吸取=deal_pink_damage(FIXED)+heal_amount 等量
//     （K302 拆原语），min(吸取量, 对手存量)（吸取不越存量，K302/K338 口径）。
//   【2477② else 必暴】"当回合必定打出致命一击"= ROUND_START 写 ws.must_crit[own]
//     （ws 每回合 reset；K305 实证 ROUND_START 写位有效——暴击掷点在 query_usage，
//     事件面写不生效的 K194 坑规避）。整回合每刀必暴（majority："必定打出"无次数限）。
//   【2510 m/n】m=min(hp/100, 已损/100)（下限 0 天然成立）、n=max(hp/100, 已损/100)
//     （上限 10）。字面区间 [m,n] 在中盘（hp/100 与已损/100 均 >10 时 m>n）倒挂 →
//     按无序区间 [min(m,n), max(m,n)] 落地（majority：区间两端点字面保留、序无关；
//     满血/残血端行为与字面一致）。逐槽独立均匀取值（"分别随机"）。
//   【2510 逐槽写值】无直接赋值原语（**内核缺口记档**：CoreApi 只有 reduce/zero/
//     restore 三族）→ 逐槽 组合落地：下调=pp_zero_slot(actor=-1 无归属，己方清自身
//     族不触发琼华之庇)+pp_restore_points 回补；上调=pp_restore_points；不变=跳过。
//     无限 PP 槽（pp==-1）跳过不动（pp_zero_slot INVALID_PARAM 同源口径，记档）。
//   【2510 分支重叠】两档条件字面独立（row2089 "不低于25"/"不高于25"恰 25 双发；
//     row2105 "不低于20"/"不高于30" 在 [20,30] 双发）→ 两个独立 if，重叠区间两支
//     都结算（官方行文如此，记档）。
//   【2510 窗口】"1回合内"（跟随回合结束的重置，覆盖下一整回合）：
//     增伤 25%=粉伤管线 AMP 票（enter 早挂一次、回调读戳 ==roundCount 生效——增粉挂
//     来源方桶 battleContext.h:1371 口径；戳 until=roundCount+1，K391/K399 1530
//     until 精确窗形）；免疫=ROUND_START 消费戳 grant_immunity(ANOMALY, coverage_all,
//     duration=1, 同句柄刷新，tier=Ancient)——⚠️ 实测勘误两则：①IMMUNE 票随
//     tick_rounds 回合界递减（battleFsm.cpp:2169），ROUND_END 直授 duration=2 会被
//     同回合链尾 tick 吃成 duration=1 次回合即过期（probe 实证），故取 ROUND_START
//     授 duration=1 形（自授起恰活一整回合、回合界自动销票零泄漏；K299 364 重授
//     同款）；②tier 取 Ancient（K275 644「免疫所有异常」blanket 同款）——"免疫受到
//     的异常状态"无通道限定 → 古早主动毒照挡（Modern 层票对古早施加不可见，
//     core_api.h Ancient 通道口径；场景以火焰漩涡 10034 古早烧伤被拦直证）。
//   【2510 盾罩刷新】护罩=hood_bank_、护盾=shield_bank_（结构同体，elf-pet.h:188），
//     同 source_id add/refresh（K401 720 同款；连续两回合达档=刷新不叠层，记档）。
//   【已损失 PP 总和】Σ max(0, maxPP-pp)（有限槽；pp 已超 maxPP 的异常夹具钳 0）。
//
// 三查记录（a01-knight9 2026-11-03，worktree 内复核）：
//   ①内核 grep -w 2475/2477/2510（src/ include/）零命中（无魂印行为硬编码）；
//   ②effect_info 三 id 均为技能空间异文模板（2475 信仰回血秒杀 / 2477 异常吸取 /
//     2510 提升复制封属）——effect_icon tips 为权威（SOP §六.1）；kind=[3,5]/[1,9]/[1,10]；
//   ③soul_lib 全库无 SOUL_PROGRAM/SOUL_MARK(2475/2477/2510)（grep 实证）、registry
//     soul 基线无三 id（build/registry.json 落盘复核于场景回归后写回状态列）；号段
//     7630-7639 全分支无撞。场景载体：纯链拍打 10004/水枪 10013/缩头 20001 + 场景直调
//     deal_pink_damage/apply_anomaly 受控粉伤（scenario_051 粉伤管线直调先例）。
// 存储键段：9924750xxx（2475）/ 9924770xxx（2477）/ 9925100xxx（2510）+owner，
//   tools/check_storage_keys.py 门禁扫描。
// 注册：SOUL_PROGRAM ×3（2475/2477/2510 入账，只增不改）。
// 场景回归：test/scenario/scenario_7630_pp_interact_family.cpp（号段 7630-7639）。

#include "common.h"

#include <effects/pink_damage_pipeline.h>
#include <primitives/battle_primitives.h>

namespace {

// ── 存储键段（+owner）────────────────────────────────────────────────────────
constexpr int kWire2475 = 992475000;      // 2475 粉伤减半票已挂旗（一场一次）
constexpr int kWire2477 = 992477000;      // 2477 DETECT 票已挂旗（一场一次）
constexpr int k2477Equal = 992477010;     // 2477② 相等技旗（ROUND_START 写 1/0）
constexpr int kWire2510 = 992510000;      // 2510 AMP 票已挂旗（一场一次）
constexpr int k2510AmpUntil = 992510010;  // 2510 增伤 25% 窗（until=roundCount+1）
constexpr int k2510ImmuneUntil = 992510020;  // 2510 免异常窗（until，RS 消费）
constexpr int k2510AuditSum = 992510030;  // 2510 重置后 PP 总和审计槽（F9 782）

// tips 字面锚（args 缺位 fallback，K398 k1430SleepPct 同纪律）
constexpr int k2475CapBody = 8;      // row2052 "最多减半8次"
constexpr int k2475CapPartner = 10;  // row2101 "10次"
constexpr int k2477AmpFallback = 30;      // "提升30%"
constexpr int k2477DrainFallback = 400;   // "吸取对手400点体力"
constexpr int k2477StepFallback = 80;     // "每恢复一点…减少80点"
constexpr int k2510T1Body = 25;      // row2089 "不低于25"
constexpr int k2510T1Partner = 20;   // row2105 "不低于20"
constexpr int k2510T2Body = 25;      // row2089 "不高于25"
constexpr int k2510T2Partner = 30;   // row2105 "不高于30"
constexpr int k2510AmpPct = 25;      // "提高25%"
constexpr int k2510Mul = 10;         // "总和*10点"
constexpr int kSkillSlotCount = 5;   // ElfPet::skills 固定 5 槽

constexpr int k2510HoodSrc = 251001;     // 护罩 source_id（hood_bank_ 定位）
constexpr int k2510ShieldSrc = 251002;   // 护盾 source_id（shield_bank_ 定位）
constexpr int k2510ImmuneHandle = 251003;  // 免疫票 source_id（0=新建规则→>0 复用）

// ── 共用小件（db_at 原序读 owned_int_args = 方案 B，K398/K403 同款私副本）──────

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

void erase_key(BattleContext* ctx, int key) { ctx->plugin_storage.erase(key); }

// 有限 PP 槽总和（pp==-1 无限槽不计，记档【2475 PP 总和口径】）。
int pp_sum(const ElfPet& pet) {
    int sum = 0;
    for (const Skills& s : pet.skills) {
        if (s.pp >= 0) {
            sum += s.pp;
        }
    }
    return sum;
}

// 已损失 PP 总和：Σ max(0, maxPP - pp)（有限槽；超装夹 0）。
int pp_lost_sum(const ElfPet& pet) {
    int sum = 0;
    for (const Skills& s : pet.skills) {
        if (s.pp >= 0 && s.maxPP > s.pp) {
            sum += s.maxPP - s.pp;
        }
    }
    return sum;
}

// "存在pp值相等的技能"：有限槽两两同值（记档【2477② 相等判定】）。
bool has_equal_pp_skills(const ElfPet& pet) {
    for (int i = 0; i < kSkillSlotCount; ++i) {
        if (pet.skills[i].pp < 0) {
            continue;
        }
        for (int j = i + 1; j < kSkillSlotCount; ++j) {
            if (pet.skills[j].pp >= 0 && pet.skills[j].pp == pet.skills[i].pp) {
                return true;
            }
        }
    }
    return false;
}

// 逐槽把 PP 写到 value（组合落地，记档【2510 逐槽写值】：无直接赋值原语——内核缺口；
// 无限槽跳过；下调=zero+回补（actor=-1 无归属，琼华之庇不触发）、上调=定点恢复）。
void set_pp_slot(BattleContext* ctx, int side, int slot, int value) {
    if (!ctx || !g_core || !g_core->pp_zero_slot || !g_core->pp_restore_points) {
        return;
    }
    const ElfPet& pet = ctx->getPet(side);
    const int cur = pet.skills[slot].pp;
    if (cur < 0 || value == cur || value < 0) {
        return;   // 无限槽不动 / 等值幂等 / 非法目标值
    }
    if (value < cur) {
        const PpReduceResult r = g_core->pp_zero_slot(ctx, side, slot, /*actor=*/-1);
        if (r == PpReduceResult::SUCCESS && value > 0) {
            (void)g_core->pp_restore_points(ctx, side, slot, value);
        }
        return;
    }
    (void)g_core->pp_restore_points(ctx, side, slot, value - cur);
}

// ════════════════ 2475 埃斯宾诺：PP 总和倍数 → 受固伤/粉伤减半票 ════════════════

// enter(early)：接线粉伤 REDUCE_EXTRA 票（一场一次；team 条目不随切换失效）。
EffectResult k2475_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (ctx->plugin_storage.count(kWire2475 + owner)) {
        return EffectResult::kOk;   // early 双触发防重（K391/K399 同款）
    }
    ctx->plugin_storage.emplace(kWire2475 + owner, 1);
    ctx->register_default_pink_effect(
        PinkDamagePhase::REDUCE_EXTRA, owner,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;   // 承受方桶（battleContext.h:1371 口径）
            }
            PinkDamageResolved& p = c->resolvedPink;
            if (p.target != owner || p.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, 2475) || c->is_boss_challenge) {
                return;   // boss 双保险（K391 管线票口径）
            }
            // 票池活函数：每段粉伤现查现消费（记档【2475 票池】）。
            const int own = pp_sum(c->getPet(owner));
            const int opp = pp_sum(c->getPet(1 - owner));
            const int tickets = (opp > 0) ? own / opp : 0;
            if (tickets <= 0) {
                return;   // 0 倍 → 无票（含对手 PP 打光保守档）
            }
            // 档位：db[0]==1 → 伙伴档 10 次，否则本体档 8 次（记档【双行判别】）。
            const int slot = c->find_pet_with_soulmark(owner, 2475);
            int cap = k2475CapBody;
            if (slot >= 0 && slot <= 5) {
                const std::vector<int>& db =
                    c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
                cap = (!db.empty() && db[0] == 1) ? k2475CapPartner : k2475CapBody;
            }
            const int halvings = (tickets < cap) ? tickets : cap;
            for (int i = 0; i < halvings; ++i) {
                p.final /= 2;   // "减半 N 次"逐步取整（记档【2475 相位】）
            }
        });
    return EffectResult::kOk;
}

// ════════════════ 2477 康迪亚克 ══════════════════════════════════════════════

// ROUND_START：① 倍数差采样 → ws.damage_add_pct；② 相等技旗 / else 必暴。
EffectResult k2477_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, 2477)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 2477);
    // ① 对手 PP 总和每比自身高 1 倍 → 攻击伤害 +db[0]×k（1264 同构，采样记档）。
    const int amp_pct = db_at(row, 0, k2477AmpFallback);
    const int own_sum = pp_sum(ctx->getPet(owner));
    const int opp_sum = pp_sum(ctx->getPet(1 - owner));
    if (amp_pct > 0 && own_sum > 0 && opp_sum > own_sum) {
        const int k = (opp_sum - own_sum) / own_sum;   // "高 N 倍"整数除
        if (k > 0) {
            ctx->ws.damage_add_pct[owner] += static_cast<float>(amp_pct * k);
        }
    }
    // ② 相等技旗（整回合钉死）/ else 当回合必暴（K305 ROUND_START 写位）。
    const bool equal = has_equal_pp_skills(ctx->getPet(owner));
    write_int(ctx, k2477Equal + owner, equal ? 1 : 0);
    if (!equal) {
        ctx->ws.must_crit[owner] = true;
    }
    return EffectResult::kOk;
}

// enter(early)：接线 ② 命中窗票（DETECT；旗在伤害时点读账本）。
EffectResult k2477_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (ctx->plugin_storage.count(kWire2477 + owner)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(kWire2477 + owner, 1);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->pp_restore
                || !g_core->deal_pink_damage || !g_core->heal_amount) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 自身攻击技命中（红伤落地；miss/属性技不触发，记档）
            }
            if (!onstage_has_soul_mark(c, owner, 2477) || c->is_boss_challenge) {
                return;
            }
            if (read_int(c, k2477Equal + owner) != 1) {
                return;   // 无相等技回合走必暴支（ROUND_START 已写 must_crit）
            }
            // 恢复所有技能 1 点 PP → 恢复点数=实际变化槽数（core_api 返回口径）。
            const int restored = g_core->pp_restore(c, owner, 1, /*only_empty=*/false);
            const int slot = c->find_pet_with_soulmark(owner, 2477);
            int drain_base = k2477DrainFallback;
            int step = k2477StepFallback;
            if (slot >= 0 && slot <= 5) {
                const std::vector<int>& db =
                    c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
                if (db.size() > 0) {
                    drain_base = db[1];   // db[1]=400（args 驱动，禁硬编码）
                }
                if (db.size() > 2) {
                    step = db[2];         // db[2]=80/40
                }
            }
            int drain = drain_base - step * restored;
            if (drain <= 0) {
                return;   // 恢复点数吃满递减 → 不吸（majority：负量无效）
            }
            const int opp = 1 - owner;
            const int foe_hp = c->getPet(opp).hp;
            if (foe_hp <= 0) {
                return;
            }
            if (drain > foe_hp) {
                drain = foe_hp;   // 吸取不越存量（K302/K338 口径）
            }
            (void)g_core->deal_pink_damage(c, opp, drain, DamageKind::FIXED, owner);
            (void)g_core->heal_amount(c, owner, drain);
        });
    return EffectResult::kOk;
}

// ════════════════ 2510 莱布缇 ════════════════════════════════════════════════

// ROUND_END：逐槽随机重置 [m,n] → 总和分档：护罩+增伤窗 / 护盾+免异常窗。
EffectResult k2510_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, 2510)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;   // 阵亡不结（K367 口径）
    }
    // m/n（记档【2510 m/n】）：a=hp/100、b=已损/100；m=较小项(下限0)、n=较大项(上限10)。
    const int max_hp = resolve_pet_max_hp(self);
    const int a100 = self.hp / 100;
    const int b100 = (max_hp - self.hp) / 100;
    if (max_hp <= 0) {
        return EffectResult::kOk;
    }
    int m = (a100 < b100) ? a100 : b100;
    if (m < 0) {
        m = 0;
    }
    int n = (a100 > b100) ? a100 : b100;
    if (n > 10) {
        n = 10;
    }
    int lo = (m < n) ? m : n;   // 无序区间（中盘倒挂 majority，记档）
    int hi = (m < n) ? n : m;
    // 逐槽随机重置（分别随机；组合写值，记档【2510 逐槽写值】）。
    for (int slot = 0; slot < kSkillSlotCount; ++slot) {
        if (self.skills[slot].pp < 0) {
            continue;   // 无限 PP 槽不动
        }
        const int value = lo + static_cast<int>(ctx->rng().next() % static_cast<unsigned>(hi - lo + 1));
        set_pp_slot(ctx, owner, slot, value);
    }
    const int sum = pp_sum(self);
    write_int(ctx, k2510AuditSum + owner, sum);   // 审计槽（F9 782）
    // 档位阈值（db[1] 判别，记档【双行判别】）。
    const HostRow row = host_row(ctx, a, 2510);
    const int tier = db_at(row, 1, 0);
    const int t1 = (tier == 1) ? k2510T1Partner : k2510T1Body;
    const int t2 = (tier == 1) ? k2510T2Partner : k2510T2Body;
    const int until = ctx->roundCount + 1;
    // 分支 A：总和 ≥ T1 → PP 总和×10 护罩 + 1 回合粉伤增伤 25%（字面独立 if）。
    if (sum >= t1) {
        const int hood = sum * k2510Mul;
        if (self.hood_bank_.quantity_of(k2510HoodSrc) > 0) {
            self.hood_bank_.refresh_source(k2510HoodSrc, hood);   // 同罩位刷新
        } else {
            self.hood_bank_.add(ShieldBank::kSoulShieldPriority, hood,
                                k2510HoodSrc, /*duration=*/0);
        }
        write_int(ctx, k2510AmpUntil + owner, until);
    } else {
        erase_key(ctx, k2510AmpUntil + owner);   // 显式写/清（K7 教训）
    }
    // 分支 B：总和 ≤ T2 → 已损 PP 总和×10 护盾 + 1 回合免异常（戳 until，RS 消费）。
    if (sum <= t2) {
        const int shield = pp_lost_sum(self) * k2510Mul;
        if (self.shield_bank_.quantity_of(k2510ShieldSrc) > 0) {
            self.shield_bank_.refresh_source(k2510ShieldSrc, shield);
        } else {
            self.shield_bank_.add(ShieldBank::kSoulShieldPriority, shield,
                                  k2510ShieldSrc, /*duration=*/0);
        }
        write_int(ctx, k2510ImmuneUntil + owner, until);
    } else {
        erase_key(ctx, k2510ImmuneUntil + owner);
    }
    return EffectResult::kOk;
}

// ROUND_START：消费免异常窗（until==roundCount → grant duration=1）；增伤窗过期清账。
// ⚠️ 免疫票必须 ROUND_START 授 duration=1（K299 364 重授同款）：IMMUNE 票随
// rule_center_.tick_rounds() 在回合界递减（battleFsm.cpp:2169）——ROUND_END 直授的
// duration=2 会在同回合链尾被 tick 吃掉 1（probe 实证 rem 2→1 → 次回合即过期），
// duration=1 自 ROUND_START 起算恰活一整回合、回合界自动销票（语义零泄漏）。
EffectResult k2510_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, 2510)) {
        return EffectResult::kOk;
    }
    const int immune_until = read_int(ctx, k2510ImmuneUntil + owner);
    if (immune_until > 0 && immune_until == ctx->roundCount) {
        // ⚠️ >0 守卫：缺失键 read_int 回 0 撞首回合 rc=0（K402 同款记档）
        ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                            /*anomaly_mask=*/0, /*duration_rounds=*/1,
                            /*source_id=*/k2510ImmuneHandle, /*soul_immunity=*/true,
                            EffectScope::ON_STAGE, /*counts=*/0,
                            /*source_effect_id=*/2510, ImmunityTier::Ancient);
    }
    erase_key(ctx, k2510ImmuneUntil + owner);   // 消费即清（当回合语义，K399 1530 形）
    const int amp_until = read_int(ctx, k2510AmpUntil + owner);
    if (amp_until != 0 && amp_until < ctx->roundCount) {
        erase_key(ctx, k2510AmpUntil + owner);   // 过期清账（K399 1530 形）
    }
    return EffectResult::kOk;
}

// enter(early)：接线 AMP 票（增粉挂来源方桶；回调读戳 ==roundCount 生效）。
EffectResult k2510_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (ctx->plugin_storage.count(kWire2510 + owner)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(kWire2510 + owner, 1);
    ctx->register_default_pink_effect(
        PinkDamagePhase::AMP, owner,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;   // 增粉从来源方桶读（battleContext.h:1371 口径）
            }
            PinkDamageResolved& p = c->resolvedPink;
            if (p.actor != owner || p.final <= 0) {
                return;   // 只增幅自身造成的粉伤
            }
            if (!onstage_has_soul_mark(c, owner, 2510) || c->is_boss_challenge) {
                return;
            }
            const int amp_until = read_int(c, k2510AmpUntil + owner);
            if (amp_until <= 0 || amp_until != c->roundCount) {
                return;   // ==until 精确窗（K391 形）；>0 守卫防缺失键撞 rc=0（K402 记档）
            }
            p.final = p.final * (100 + k2510AmpPct) / 100;
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（2475/2477/2510 入账，只增不改）
// 三宏 tips 尾注（boss无效）为独立尾段 → 全句尾归属 → 全节点 boss_invalid=true。
SOUL_PROGRAM(2475, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k2475_arm,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/true)});

SOUL_PROGRAM(2477, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k2477_arm,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k2477_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});

SOUL_PROGRAM(2510, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k2510_arm,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k2510_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_END, &k2510_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});
