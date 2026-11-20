// K413 免疫/吸取转嫁/封属窗族 —— effect 274/748/764（3 宏 / 6 行，双行档位差）
//（看板 K413「免疫吸取封属 2026-11-04」，a01-k413，worktree ../agents/k413；
//  号段 7730-7739；通道=274 概率免疫=票内掷+审计槽（K400 形，满体门 K377）+
//  748 吸取转嫁=transfer_stat_boosts/steal（K408 1668 镜像直系）+登场免疫窗
//  （K401 888）+ 764 封属=SEAL_ATTRIBUTE 2 回合窗（K399 1470①/K311，hit 触发窗
//  K399 1470②）+高体门。dump 全文先行，双行档位差纯 args。三查同前例）
//
// 行清单（tools/soul_effects_dump.py 274 748 764，2026-11-04）：
//   274 将神·吕布 2496 两行（kind 同 [7]，**db[2] 档位差** 50/100 常规行≠宏）：
//     row25 本体 args[100,100,50]：「50%概率免疫异常状态」
//     row26 args[100,100,100]：「满体力时100%概率免疫异常状态」
//     → db[0]/db[1]=100 恒（语义无 tips 锚——K171 285 同宠双参位是 免异常%/免降%
//       两票位，本条 tips 只出现免异常一票 → 两位置按 K1 纪律不消费，记档）；
//       db[2]=免疫申报概率（50/100，两行 tips 字面互锚——50 档行文无门、100 档带
//       "满体力时"门，档位差位即行判别锚，见下【274 行判别】）。
//   748 龙胆·赵云 3078 两行（kind 同 [7,9,12]，**db[0]/db[1] 档位差**，纯 args）：
//     row426 本体 args[1,2,1,1]：「每次登场首回合免疫异常状态；回合开始时自身处于
//       能力提升则所有技能先制+1；每回合随机吸取对手2项属性-1，并将该属性附加给
//       自己（boss无效）」
//     row460 伙伴行 args[2,3,1,1]（come=伙伴效果）：先制+2、吸取 3 项。
//     → db[0]=②先制量（1/2，tips"先制+1/+2"互锚）、db[1]=③吸取项数（2/3，tips
//       "2项/3项"互锚——K267 738 旁证"随机吸取对手2项属性-1"项数同位）、
//       db[2]/db[3]=1 恒尾位旗不消费（K1 纪律）。
//   764 斗天武神·瑞尔斯 3102 两行（kind 同 [1,5,13]，**args 计数互异** 2/4）：
//     row449 本体 args[2,100]：「自身攻击技能命中后则令对手当回合及下回合使用的
//       属性技能失效（该效果为回合类效果）；|回合开始时若自身体力高于对手，则当
//       回合造成的攻击伤害提升100%；回合开始时若自身体力低于对手，则下1次使用技
//       能造成的攻击伤害会恢复自身体力，之后下1次使用技能给对手造成等同于恢复量
//       的固定伤害（boss有效）」
//     row456 伙伴行 args[2,100,50,50]（come=战神兄弟伙伴效果）：同文 +（攻击+50，
//       体力+50，boss有效）。
//     → db[0]=①封属窗回合数（2 恒，"当回合及下回合"互锚）；db[1]=②高体增伤%
//       （100 恒，tips"提升100%"互锚）；db[2]/db[3]=伙伴面板 攻+50/体+50（**划界
//       不消费**：板行仅处方 封属窗+高体门，伙伴静态面板 SOUL_PRE_BATTLE 另批，
//       K409 划界同款记档）。
//
// ⚠️ 口径记档（majority reading，均按先例）：
//   【274 行判别】两行 args 计数同（3/3）、同宠同 kind，唯一运行期可见差异=db[2]
//     档位（50/100）。门随档位走：db[2]>=100 = 满体力门+确定免（row26 语义——
//     "满体力时100%"字面）；db[2]<100 = 无门逐次掷 db[2]（row25 语义——"50%概率"
//     无门字面）。全 dump 仅此两行 → 值驱动门映射安全（tips 互锚，记档）。
//   【274 票形】ANOMALY 免疫票 enter+ROUND_START 双节点重授（K171 285 直系——
//     同宠 2496、duration=1 每回合窗、固定句柄刷新非追加、soul_immunity=true
//     221 同层、ON_STAGE 切走即失效）；**满体门与掷骰都在 condition 里现场判**
//     （查询点每次施加掷一次、票不因掷骰消耗——grant_immunity 注释钦定；满体
//     判定逐次读现值，不在挂票时把整回合钉死在挂票时刻血量，K171 口径）。
//   【274 审计槽】票内掷 = condition lambda 内 const_cast 写透 plugin_storage
//     审计槽（掷前写申报概率，K400 形；const 写透有 tianqi_xinghun_4677
//     const_cast 先例）+ EVENT_ANOMALY_IMMUNED watcher 计免疫落地数（battle_
//     primitives [3]/[5] 免疫命中事件，K401 888① 同门 target==own/actor≠own）
//     ——场景无随机精确断言 + 落地计数侧证。
//   【274 boss 尾注】行文无尾注 → 全节点 boss_invalid=false、票照常（boss 局
//     也免疫；K401 888①② 无尾注照常同口径）。
//   【748① 登场免疫窗】"每次登场首回合" = enter 节点每次登场重授 ANOMALY 免疫票
//     （duration=1 首回合窗、固定句柄刷新、"每次"=非"首次"——K401 2087 涤荡
//     每次登场重授直系，与 888"首次严格一次"相区分）；无概率（确定免）→ 无审计
//     槽；无尾注 → boss 局照常（boss_invalid=false）。
//   【748③ 吸取转嫁】"每回合"= ROUND_START 节点（K267 645/K4 622 同款时点）；
//     "随机吸取对手N项属性-1"= 随机 db[1] 个**互不相同**槽（K267 645/shenjue
//     4676 去重取法；覆盖全 6 槽含命中——"属性"未排除命中，K267/K313 同口径），
//     逐槽 stat_drop(opp, 槽, 1)（弱化通道查免弱——K408 1668② 镜像赋降同原语）；
//     "并将该属性附加给自己" = drop SUCCESS 才 stat_change(own, 同槽, +1)（自身
//     增益通道不查免弱，K267 622 口径）。**吸取失败不转嫁**（IMMUNE=免弱挡下 /
//     AT_FLOOR=对手 -6 已到底无可吸取 → 自身不加，"吸取…并附加"对称读法，
//     majority 记档）。对手已倒地不结算（K401/K408 口径）。随机目标类无概率
//     申报 → 无审计槽，落地槽次计数侧证（K267 口径）。boss_invalid=true
//     （"（boss无效）"紧贴③句尾——K408 1668 仅尾支口径；①无尾注照常）。
//   【748② 划界记档】"回合开始时自身处于能力提升则所有技能先制+N" 板行未处方
//     （板行仅 ③吸取转嫁+①登场免疫窗；先制族 K403/K218/K400-1151 形另批），
//     db[0] 本单不消费——后续单若补 ②，直接读 db[0] FMR preemptive_level
//     （K400 1151 直系）即可，行参无需迁移。
//   【764① 封属窗】"自身攻击技能命中后"= hit 触发 = EVENT_TAKE_DAMAGE watcher
//     （target==对手/actor==own/amount>0/damage_kind==NORMAL——E12 攻击伤害过滤，
//     K399 1470②"受到攻击"同门镜像到攻方视角；miss 不触发——"命中后"字面，
//     与 K400 1151"使用即发"窗相区分）；封属 = seal_skill(attribute=true,
//     attack=false, count=0, duration_rounds=db[0]=2, 固定句柄刷新非追加,
//     chance_pct=100)（"使用的属性技能失效"= SEAL_ATTRIBUTE 非 hit_invalid，
//     K311/K399 1470① 口径；"当回合及下回合"=2 回合窗，单次授予 duration=N
//     靠 tick 自灭——K400 1151"2回合内"同形；重复命中同句柄刷新非追加）。
//     同回合多次命中=刷新窗（不叠加不缩短，majority 记档）。
//   【764② 高体门】"回合开始时若自身体力高于对手"= ROUND_START 严格档比较
//     （self.hp > opp.hp，K379/K397 compare 直系）→ ws.damage_add_pct[own] +=
//     db[1]（ROUND_START 直写 ws——桶在 resetWorkspace 后，memset 后写全回合
//     有效，K399 1264③/K400 1474 伙伴通道同款；"回合开始时"管辖全句——回合内
//     血量反超不翻转，K400① 同口径记档）。审计槽写现行增幅值（侧证精确断言）。
//   【764 括注/尾注】"（该效果为回合类效果）"= 封属窗由 duration_rounds 承载
//     回合窗语义（tick 自灭），无需回合类效果载体（K400 1151 记档延续）；
//     "（boss有效）"尾注最近邻=低体句 → 全行语义=对 boss 生效（无无效化子句）
//     → 全节点 boss_invalid=false、watcher 无 is_boss_challenge 门（boss 局
//     照常封属/增伤——与常规"boss无效"族相反向，K401/K408 归属判的镜像面）。
//   【764 划界记档】低体支（下1次攻击吸血+等量固伤）与伙伴面板（攻+50 体+50）
//     板行未处方 → 不实现；db[2]/db[3] 不消费。
//
// ⚠️ 三查记录（a01-k413 2026-11-04，worktree 内复核）：
//   ①内核 grep \b274\b/\b748\b/\b764\b（src/ include/）：274 唯一命中
//     battle_primitives.h:843 注释"idx=274 尤纳斯算例"（取整算例引用，非行为）；
//     748/764 零命中——魂印行为零硬编码；
//   ②effect_info 274 无模板行；748="若打出的伤害在{0}至{1}之间则对手{2}"/
//     764="{0}回合内若对手使用攻击技能降低对手最大体力的1/{1}"均为技能空间异文
//     模板——effect_icon tips 为魂印权威（SOP §六.1，dump 原文已核）；
//   ③全库 grep 无 SOUL_PROGRAM(274/748/764)（soul_lib+test）；场景号段
//     7730-7739 全分支无撞；存储键段 99274xxxx/99748xxxx/99764xxxx
//     tools/check_storage_keys.py 全绿；免疫票句柄 2741/7481（mark*10+n，
//     K401 形）、封属句柄 76401（mark*100+1，K399/K400 形）grep 全库无撞。
// 注册：SOUL_PROGRAM ×3（274/748/764 入账，只增不改）。
// 场景回归：test/scenario/scenario_7730_immune_274.cpp /
//   scenario_7733_steal_748.cpp / scenario_7736_seal_764.cpp（号段 7730-7739）。

#include "common.h"

#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include <array>

namespace {

// ── 存储键段（+owner；tools/check_storage_keys.py 已核无撞）──────────────────
constexpr int k274Audit = 992740000;    // 免疫申报概率审计槽（掷前写，K400 形）
constexpr int k274ImmCnt = 992740010;   // 免疫落地计数（ANOMALY_IMMUNED 侧证）
constexpr int k274Wire = 992740020;     // IMMUNED watcher 已挂旗

constexpr int k748StealCnt = 997480000;  // 吸取转嫁落地槽次累计（侧证）

constexpr int k764SealCnt = 997640000;   // 封属授予计数（侧证）
constexpr int k764GateAmp = 997640010;   // 高体门现行增幅（审计，ROUND_START 写）
constexpr int k764WireTake = 997640020;  // TAKE_DAMAGE watcher 已挂旗

// 免疫票句柄（mark*10+n 稳定句柄形，K401 同款：同句柄重授=刷新非追加）。
constexpr int k274ImmuneTicket = 274 * 10 + 1;   // 2741
constexpr int k748ImmuneTicket = 748 * 10 + 1;   // 7481
// 封属句柄（mark*100+1 形，K399 147001/K400 115101 同款）。
constexpr int k764SealHandle = 764 * 100 + 1;    // 76401

// ── 共用小件（db_at 原序读 owned_int_args = 方案 B，K172/K400 定例）──────────

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

// 随机 n 个互不相同的槽（0..5）——K267 645/shenjue_mistike_4676 同款去重取法。
std::array<int, BattleContext::kAbilityLevelSlotCount> random_distinct_slots(int n) {
    std::array<int, BattleContext::kAbilityLevelSlotCount> picked{};
    const int total = BattleContext::kAbilityLevelSlotCount;
    const int count = (n < 0) ? 0 : ((n > total) ? total : n);
    bool used[BattleContext::kAbilityLevelSlotCount] = {false};
    for (int i = 0; i < count; ++i) {
        int slot = 0;
        for (int attempt = 0; attempt < 64; ++attempt) {
            slot = sbs_rand() % total;
            if (!used[slot]) {
                break;
            }
        }
        used[slot] = true;
        picked[i] = slot;
    }
    return picked;
}

// 满体力判定（逐次施加时点现场读——掉血/回血即时反映，K171 285 私副本同款）。
bool full_hp_at(const BattleContext* c, int owner, int slot) {
    if (!c || c->on_stage[owner] != slot) {
        return false;   // 换下场即失去"在场满体力"前提
    }
    const ElfPet& pet = c->seerRobot[owner].elfPets[slot];
    return pet.hp > 0 && pet.hp >= pet.numericalBase[NumericalPropertyIndex::HP];
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 274 将神·吕布：概率免疫异常状态（row25 无门 50% / row26 满体门 100%）
// ═════════════════════════════════════════════════════════════════════════════

// ANOMALY 免疫落地计数 watcher（一场一次；"免疫成功"族检测点，K401 888① 同门）。
void arm_274_immune_counter(BattleContext* ctx, int owner) {
    if (!ctx || ctx->plugin_storage.count(k274Wire + owner)) {
        return;
    }
    ctx->plugin_storage.emplace(k274Wire + owner, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_ANOMALY_IMMUNED, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c) {
                return;
            }
            if (ev.target != owner || ev.actor < 0 || ev.actor > 1
                || ev.actor == owner) {
                return;   // 只计"别人对我就地免疫成功"的异常
            }
            if (!onstage_has_soul_mark(c, owner, 274)) {
                return;
            }
            write_int(c, k274ImmCnt + owner, read_int(c, k274ImmCnt + owner) + 1);
        },
        WatcherScope::TEAM);
}

// enter/ROUND_START 共用挂票体：ANOMALY 免疫票 duration=1 固定句柄刷新
//（K171 285 直系）；门与掷骰在 condition 现场判、掷前写审计槽（K400 形）。
EffectResult k413_274_grant(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 274)) {
        return EffectResult::kOk;
    }
    arm_274_immune_counter(ctx, owner);
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(owner, 274);
    }
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 274);
    const int pct = db_at(row, 2, -1);
    if (pct <= 0) {
        return EffectResult::kOk;   // 无参不挂票（K1 纪律：不造 0% 票）
    }
    const bool gated = pct >= 100;   // 【274 行判别】100 档=满体门；<100 档=无门
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/1,
                        /*source_id=*/k274ImmuneTicket, /*soul_immunity=*/true,
                        EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/274, ImmunityTier::Modern,
                        [owner, slot, pct, gated](const BattleContext* c, int) {
                            if (gated && !full_hp_at(c, owner, slot)) {
                                return false;   // 满体门（逐次现场判，K171 口径）
                            }
                            // 审计槽：掷前写申报概率（const 写透有 4677 先例）。
                            auto* cc = const_cast<BattleContext*>(c);
                            cc->plugin_storage[k274Audit + owner] = pct;
                            return roll_percent(pct);
                        });
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 748 龙胆·赵云：①每次登场首回合免疫异常（boss 局照常）
//   ③每回合随机吸取对手 N 项属性-1 并转嫁自身（boss无效）
// ═════════════════════════════════════════════════════════════════════════════

// ① enter：每次登场重授 ANOMALY 免疫票（duration=1 首回合窗、固定句柄刷新、
//   确定免无审计槽——K401 2087"每次登场"重授直系：本节点**不带 onstage 守卫**
//   （2087 同形——换宠半程解析里 on_stage 尚未安定，守卫会吞重登场重授）；
//   票 scope=ON_STAGE 自带"不在场即失效"语义）。无尾注 → boss 局照常。
EffectResult k413_748_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;   // 已倒地不授（K401 2087 同款）
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/1,
                        /*source_id=*/k748ImmuneTicket, /*soul_immunity=*/true,
                        EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/748, ImmunityTier::Modern);
    return EffectResult::kOk;
}

// ③ ROUND_START：随机 db[1] 个互不相同槽——stat_drop(opp,槽,1) SUCCESS 才
//   stat_change(own,同槽,+1)（吸取失败不转嫁，记档）。boss_invalid 节点。
EffectResult k413_748_steal(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 748)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 对手已倒地不结算（K401 口径）
    }
    const HostRow row = host_row(ctx, a, 748);
    const int count = db_at(row, 1, 2);   // db[1]，兜底本体字面 2 项
    if (count <= 0) {
        return EffectResult::kOk;
    }
    const std::array<int, BattleContext::kAbilityLevelSlotCount> slots =
        random_distinct_slots(count);
    int landed = 0;
    for (int i = 0; i < count && i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int slot = slots[i];
        // 弱化通道（查免弱）；IMMUNE=免弱挡下 / AT_FLOOR=-6 无可吸取 → 不转嫁。
        if (g_core->stat_drop(ctx, opp, slot, /*amount=*/1, owner)
            == StatDropResult::SUCCESS) {
            ++landed;
            (void)g_core->stat_change(ctx, owner, slot, /*delta=*/1);   // 转嫁自身
        }
    }
    if (landed > 0) {
        write_int(ctx, k748StealCnt + owner, read_int(ctx, k748StealCnt + owner) + landed);
    }
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 764 斗天武神·瑞尔斯：①攻击命中→封属 2 回合窗（boss 有效）
//   ②回合开始高体门→当回合攻击伤害提升
// ═════════════════════════════════════════════════════════════════════════════

// enter：接线 hit 触发窗（EVENT_TAKE_DAMAGE：own 攻击命中对手——K399 1470②
//   E12 过滤镜像到攻方视角）。"boss有效" → watcher 无 boss 门。
EffectResult k413_764_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->plugin_storage.count(k764WireTake + owner)) {
        return EffectResult::kOk;   // early 双触发防重（K408/K400 同款）
    }
    ctx->plugin_storage.emplace(k764WireTake + owner, 1);
    const HostRow row = host_row(ctx, a, 764);
    const int seal_rounds = db_at(row, 0, 2);   // db[0]，兜底 tips 字面 2 回合
    if (seal_rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, seal_rounds](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->seal_skill) {
                return;
            }
            // "自身攻击技能命中后"：own 是施加方、目标是对手、实际掉血、攻击伤害。
            if (ev.actor != owner || ev.target != 1 - owner || ev.amount <= 0
                || ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                return;   // miss 不触发（"命中后"字面，E12 过滤 K399 1470② 同门）
            }
            if (!onstage_has_soul_mark(c, owner, 764)) {
                return;
            }
            if (c->getPet(ev.target).hp <= 0) {
                return;   // 对手已倒地不结算（K401/K408 口径）
            }
            // SEAL_ATTRIBUTE 2 回合窗（K311/K399 1470① 口径）；count=0 不限次数、
            // duration=db[0] 靠 tick 自灭、同句柄重复授予=刷新非追加。
            (void)g_core->seal_skill(c, /*source=*/owner, /*target=*/1 - owner,
                                     /*effect_id=*/k764SealHandle,
                                     /*attribute=*/true, /*attack=*/false,
                                     /*count=*/0, /*duration_rounds=*/seal_rounds,
                                     /*penetrable=*/true, /*source_slot=*/-1,
                                     EffectScope::ON_STAGE, /*hit_invalid=*/false,
                                     /*chance_pct=*/100,
                                     /*consumed_when_pierced=*/false);
            write_int(c, k764SealCnt + owner, read_int(c, k764SealCnt + owner) + 1);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ② ROUND_START：高体门（自身 hp 严格 > 对手 hp）→ 当回合攻击伤害 +db[1]%
//  （ws 直写 memset 后有效，K399 1264③ 同款）；审计槽写现行增幅。
EffectResult k413_764_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, 764)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int opp = 1 - owner;
    const HostRow row = host_row(ctx, a, 764);
    const int amp = db_at(row, 1, 100);   // db[1]，兜底 tips 字面 100
    const bool high = ctx->getPet(owner).hp > 0
        && ctx->getPet(owner).hp > ctx->getPet(opp).hp;   // 严格档（K379 口径）
    if (high && amp > 0) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(amp);
    }
    write_int(ctx, k764GateAmp + owner, high ? amp : 0);   // 审计：现行增幅档
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（274/748/764，只增不改）────
// 274：无尾注 → 全节点 boss_invalid=false（boss 局也免疫）。
SOUL_PROGRAM(274, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k413_274_grant,
                              /*once=*/false, /*early=*/false),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k413_274_grant,
                              /*once=*/false, /*early=*/false)});

// 748：①无尾注（boss 局照常免疫）；③"（boss无效）"紧贴 → ROUND_START 节点
//   boss_invalid=true（K408 1668 仅尾支口径）。
SOUL_PROGRAM(748, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k413_748_enter,
                              /*once=*/false, /*early=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k413_748_steal,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});

// 764："（boss有效）" → 全节点 boss_invalid=false、watcher 无 boss 门（boss 局
//   照常封属/增伤）。
SOUL_PROGRAM(764, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k413_764_enter,
                              /*once=*/false, /*early=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k413_764_round_start,
                              /*once=*/false, /*early=*/false)});
