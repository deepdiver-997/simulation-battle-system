#include <utils/sbs_rand.h>
// 神印族（启灵元神 1581：神印 + 神殇/神谕/神照/神启 全条，2026-09-21 补全）
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；2026-09-21 对齐官方时点（神印两段式）并
//   补齐其余四条子句——口径全部来自语料 idx=439《启灵元神魂印补全》/ idx=460
//   《机制解析—启灵元神》/ 4399 图鉴 907369，见各节注释。）
//
// 神谕的**查询桥**在 core：BattleContext::off_field_stats_protected（场下体力写入
// 路径的统一入口）内联了神谕条件——因为防炸背包必须在"任何场下写入"处可见，
// 而写入点散在 core/插件各处；1581 硬编码于该桥接点（有注释）。
#include "common.h"
#include "../qiling_runtime.h"

namespace {

constexpr int kShenyinMaxStacks = 3;

struct ShenyinStorage {
    int stacks = 0;  // 神印层数（谁身上挂着这个槽位，谁就是被切上来的那位）
    // ★ 本次登场是否待结算（2026-09-21 对齐官方时点）：神印伤害在「登场**首战斗阶段**
    //   结束时」结算（DB effect_icon 1804/2050 同款措辞 = 每次登场后的第一个战斗阶段），
    //   不是切换登场立即扣。挂层时置位，阶段末消费一次；之后回合不再重复（没再登场）。
    bool damage_pending = false;
};

// ── 魂印级运行时（挂 **holder** pet 的 soulmark_storage[kQilingRuntimeKey]）──
// ⚠️ 与 ShenyinStorage（挂**被切上来的受害者**身上）故意不同键不同结构：
//    双方镜像对局时同一只 pet 可能既是 holder 又是受害者，同键异构 any_cast 必炸。
struct QilingRuntime {
    int first_slot = -1;             // 神照：本方**首发精灵**槽位（deploy 时记录）
    int shenqi_r1_sids[2] = {0, 0};  // 神启 R1 免控/免弱票（任意一方下1次技能命中前）
    bool hit_watcher_registered = false;
    bool enter_watcher_registered = false;
    bool deploy_done = false;        // PROTECTION_1 每回合都跑 → deploy 只做一次
};
constexpr int kQilingRuntimeKey = 15811;   // 故意 ≠ 1581（见上）

// 神照/神启 R4 的护盾 source_id（ShieldBank 按 source_id refresh/区分）。
constexpr int kShieldSourceShenzhao = 158101;  // 神照：首发精灵登场盾
constexpr int kShieldSourceShenqi = 158102;    // 神启 R4：低体力自盾
constexpr int kShenqiShieldPct = 30;           // 「最大体力值30%」（DB/语料 idx=439 同口径；
                                               //  4399 图鉴写"体力值"，少数派，记差异）

// plugin_storage 只装一次旗标（watcher 挂载是 owner 级，不能放 per-pet storage——
// 双方镜像对局时两个 holder 各自 deploy 会对同一 owner 重复挂）。
bool* qiling_armed_flag(BattleContext* ctx, int owner, int salt) {
    auto& storage = ctx->plugin_storage;
    const int key = 15810001 + owner * 10 + salt;
    auto it = storage.find(key);
    if (it == storage.end()) {
        it = storage.emplace(key, false).first;
    }
    return &std::any_cast<bool&>(it->second);
}

// 算分母：层数 → 真伤分母（1 层=1/8，2 层=1/6，3 层=1/4）
static int shenyin_denom(int stacks) {
    if (stacks >= 3) return 4;
    if (stacks >= 2) return 6;
    return 8;
}

EffectResult soulmark_1581_register_swap_watcher(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int opponent = 1 - owner;
    // 注册 EVENT_SWAP watcher：event.actor == opponent（对方在切）→ 一步完成
    // "挂层 + 立即按层数真伤"。神印层数存在 actor（切上来的精灵）自己身上，跨切换天然跟随。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SWAP,
        owner,
        ctx->roundCount,
        /*duration_rounds=*/0,  // 永久
        /*once=*/false,
        [opponent, owner](BattleContext* wctx, const BattleEvent& event) {
            if (event.actor != opponent) {
                return;  // 己方中切：跳过（双方都带启灵元神时双方 watcher 各管各的对手）
            }
            // 0) 前提门："自身在出战背包时"——持有方（元神）必须还在背包。语料
            //    idx=402/403 + 2024.1.12 一刀切口径：背包辅助魂印随源**消逝**无效化
            //    （武心婵 4500 同款消逝敏感；消逝原语会把上限清 0/0 → 也无基数可算）。
            const int holder_slot = wctx->find_mark_in_roster(owner, 1581);
            // 1) 挂层：actor 自身 soulmark_storage[1581].stacks += 1（截断 ≤3）+ 置待结算
            //    注：on_stage[actor] 已经在 handle_OperationChooseSkillMedicament
            //    收到 CHOOSE_PET 时同步更新到 target_slot（perform_switch 已做完），
            //    所以这里 getPet(actor) 就是"刚切上来的新精灵"。
            //    ⚠️ 2026-09-21 对齐官方时点：这里**只挂层**，不扣血——伤害挪到
            //    「登场首战斗阶段结束时」（soulmark_1581_phase_end_damage，BATTLE_ROUND_END）。
            ElfPet& actor_pet = wctx->getPet(event.actor);
            auto& storage = actor_pet.soulmark_storage;
            int stacks = 0;
            if (const auto existing = storage.find(1581); existing != storage.end()) {
                stacks = std::any_cast<ShenyinStorage&>(existing->second).stacks;
            }
            if (holder_slot < 0) {
                // 元神已消逝 → 不挂层、不扣血；但**照常发布**刚登场那只的真实层数
                // （已挂的层不清——1532/1536 读的是"身上有没有层"，发布槽必须跟着
                // 在场精灵走，否则残留旧值会张冠李戴）。
                qiling::publish_shenyin_stacks(wctx->plugin_storage, opponent, stacks);
                return;
            }
            if (!storage.count(1581)) {
                storage[1581] = ShenyinStorage{};
            }
            ShenyinStorage& slot = std::any_cast<ShenyinStorage&>(storage[1581]);
            slot.stacks = std::min(kShenyinMaxStacks, slot.stacks + 1);
            slot.damage_pending = true;
            // 2) 发布当前在场受害者的层数（技能侧 1532/1536 跨 dylib 只读这个 int——
            //    qiling_runtime.h 键段 15830000；watcher 恰在"在场受害者"变更时点跑，
            //    发布值恒等于场上那只的真实层数）。
            qiling::publish_shenyin_stacks(wctx->plugin_storage, opponent, slot.stacks);
        },
        WatcherScope::TEAM  // 启灵元神在背包也生效；切到别的精灵此魂印不丢
    );
    return EffectResult::kOk;
}

// ── 登场首战斗阶段结束时（BATTLE_ROUND_END —— 引擎惯例的官方"战斗阶段结束时"落点，
//    chaos_sorensen/puni_1038/yanshou 同款）：按层数对"本次登场待结算"的对方精灵扣真伤 ──
// 官方（effect_icon 1246）：「1层：首战斗阶段结束时受到自身最大体力1/8的真实伤害」——
// 伤害基数 = **元神（持有方）自己的最大体力**（本魂印文本的「自身」一贯指元神本人，
// 神照「等同于启灵元神最大体力值30%的护盾」同款语汇；旧实现误读成被切上来的精灵，
// 2026-09-21 用户纠错）。神印**不清零**——按用户"神印无法减少"持续累计到 3 层封顶。
// ⚠️ 时序细节：每方每回合只行动一次 → 待结算的至多是该方**场上**那只（刚切上来的）；
//    登场后中途被替换/击杀出局的，首战斗阶段已被打断 → 清待结算位、不补扣。
EffectResult soulmark_1581_phase_end_damage(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int holder = ctx->find_mark_in_roster(owner, 1581);
    if (holder < 0) {
        return EffectResult::kOk;   // 元神已消逝 → 不再提供（一刀切，武心婵同款）
    }
    const int basis = resolve_pet_max_hp(ctx->seerRobot[owner].elfPets[holder]);
    if (basis <= 0) {
        return EffectResult::kOk;
    }
    const int target_side = 1 - owner;
    auto& pets = ctx->seerRobot[target_side].elfPets;
    for (int slot = 0; slot < 6; ++slot) {
        auto it = pets[slot].soulmark_storage.find(1581);
        if (it == pets[slot].soulmark_storage.end()) {
            continue;
        }
        ShenyinStorage& st = std::any_cast<ShenyinStorage&>(it->second);
        if (!st.damage_pending) {
            continue;
        }
        st.damage_pending = false;   // 只结算"本次登场"的首个战斗阶段结束
        if (slot != ctx->on_stage[target_side]) {
            continue;   // 已不在场上（被替换/击杀出局）→ 不补扣
        }
        // 走**真伤原语**：直写 hp 会绕过护盾/护罩、不发 EVENT_TAKE_DAMAGE、不走死亡结算。
        if (g_core && g_core->deal_true_damage) {
            g_core->deal_true_damage(ctx, target_side,
                                     std::max(1, basis / shenyin_denom(st.stacks)), owner);
        }
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// [神殇]（子句①，ROUND_START ROSTER）—— 语料 idx=439 逐字口径：
//   每回合开始时 100% 创造神秘结界：随机放逐对手 x-y 个**不在场**精灵，使其不能
//   **主动**切换上场；存活精灵 ≥5 → 放逐 1-3 个，<5 → 1-2 个（DB 官方数据；
//   4399 图鉴写 2-3 个，少数派记差异）；已死亡的不放逐；个数上限 = 存活数 - 2。
// 落点：放逐 = ElfPet::is_locked（CHOOSE_PET 主动切换门的既有校验位，引擎此前
// 从不 set——现在归神殇所有）；**死亡补位走 is_forced 绕过**（官方"不能主动切换"，
// battleFsm 的 is_locked 检查已补 is_forced 旁路）。每回合整场重掷（清了再放）。
// 放逐位图同步发布到 plugin_storage（qiling_runtime.h），供第五技 1534 跨 dylib 读取。
// [神启] R3/R4（子句⑤，ROUND_START）：自身 hp > max/2 → 免控免弱至下回合开始
//   （TEAM，下场保留）；hp < max/2 → 自盾 = 自身最大体力 30%（语料 idx=460 拆出的
//   第 3/4 条；R3 的窗口"延续至下回合的回合开始时" = duration_rounds=1 绝对窗口）。
// ════════════════════════════════════════════════════════════════════
EffectResult soulmark_1581_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int holder = ctx->find_mark_in_roster(owner, 1581);

    // ── 神殇：重掷对手的放逐集 ──
    if (holder >= 0) {
        const int foe = 1 - owner;
        auto& pets = ctx->seerRobot[foe].elfPets;
        int alive = 0;
        for (int slot = 0; slot < 6; ++slot) {
            pets[slot].is_locked = false;   // 整场重掷：先全放出来
            if (pets[slot].hp > 0) {
                ++alive;
            }
        }
        int mask = 0;
        const int cap = alive - 2;   // 至少留 2 个存活不可放逐
        if (cap >= 1) {
            const int low = 1;
            const int high = alive >= 5 ? 3 : 2;
            const int upper = high < cap ? high : cap;
            int n = low + sbs_rand() % (upper - low + 1);
            // 候选 = 不在场且存活的槽位，随机不放回取 n 个
            int candidates[6];
            int candidate_count = 0;
            for (int slot = 0; slot < 6; ++slot) {
                if (slot != ctx->on_stage[foe] && pets[slot].hp > 0) {
                    candidates[candidate_count++] = slot;
                }
            }
            while (n > 0 && candidate_count > 0) {
                const int pick = sbs_rand() % candidate_count;
                const int slot = candidates[pick];
                pets[slot].is_locked = true;
                mask |= (1 << slot);
                candidates[pick] = candidates[--candidate_count];
                --n;
            }
        }
        qiling::publish_exile_mask(ctx->plugin_storage, foe, mask);
    }

    // ── 神启 R3/R4 ──
    if (holder >= 0) {
        ElfPet& self = ctx->seerRobot[owner].elfPets[holder];
        const int max_hp = resolve_pet_max_hp(self);
        if (max_hp > 0) {
            if (self.hp * 2 > max_hp) {   // 高于 1/2 → 免控免弱至下回合开始（下场保留）
                // 用户口径（2026-09-26）：只保 holder 本人。TEAM 票的免疫查询是 side 级
                // （下场后队友在场也命中）→ 必须带 condition 在场守卫；漏了 = 全队免控。
                ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                                    /*anomaly_mask=*/0, /*duration_rounds=*/1,
                                    /*source_id=*/15813, /*soul_immunity=*/false,
                                    EffectScope::TEAM,
                                    /*counts=*/0, /*source_effect_id=*/-1,
                                    ImmunityTier::Modern,
                                    [owner, holder](const BattleContext* c, int) {
                                        return c && c->on_stage[owner] == holder;
                                    });
                ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                                    /*anomaly_mask=*/0, /*duration_rounds=*/1,
                                    /*source_id=*/15814, /*soul_immunity=*/false,
                                    EffectScope::TEAM,
                                    /*counts=*/0, /*source_effect_id=*/-1,
                                    ImmunityTier::Modern,
                                    [owner, holder](const BattleContext* c, int) {
                                        return c && c->on_stage[owner] == holder;
                                    });
            } else if (self.hp * 2 < max_hp && g_core) {   // 低于 1/2 → 自盾 30%
                self.shield_bank_.add(ShieldBank::kSoulShieldPriority,
                                      max_hp * kShenqiShieldPct / 100,
                                      kShieldSourceShenqi, /*duration=*/0);
            }
        }
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// [神照]/[神启 R1/R2] 的 deploy 节点（PROTECTION_1，战斗开始一次）——
// 语料 idx=460 把神启拆成 5 条：R1 游戏开始时高体力免控免弱至**任意一方下1次技能
// 命中前**（限定1次——覆盖出战操作期的控场）；R2 每次**自身**登场时（含首次）高体力
// 免控免弱至下回合开始（下场保留）。神照 = 己方**首发精灵**每次登场后获得
// 启灵最大体力 30% 的护盾（idx=439：盾下场不清，站场才被磨——duration=0）。
// ════════════════════════════════════════════════════════════════════
EffectResult soulmark_1581_deploy(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int holder = ctx->find_mark_in_roster(owner, 1581);
    if (holder < 0) {
        return EffectResult::kOk;   // 消逝后不再提供
    }
    ElfPet& self = ctx->seerRobot[owner].elfPets[holder];
    auto& storage = self.soulmark_storage;
    if (!storage.count(kQilingRuntimeKey)) {
        storage[kQilingRuntimeKey] = QilingRuntime{};
    }
    QilingRuntime& rt = std::any_cast<QilingRuntime&>(storage[kQilingRuntimeKey]);
    if (rt.deploy_done) {
        return EffectResult::kOk;   // PROTECTION_1 在回合链里每回合都跑 → deploy 只做一次
    }
    rt.deploy_done = true;
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0) {
        return EffectResult::kOk;
    }

    // 神照：登记首发槽 + 首发已登场 → 初始盾（后续再登场由 ENTER_STAGE watcher 补）
    rt.first_slot = ctx->on_stage[owner];
    ctx->seerRobot[owner].elfPets[rt.first_slot].shield_bank_.add(
        ShieldBank::kSoulShieldPriority, max_hp * kShenqiShieldPct / 100,
        kShieldSourceShenzhao, /*duration=*/0);

    // 神启 R1：游戏开始时高体力 → 免控免弱，至任意一方下 1 次技能命中前（限定 1 次）
    // 同 R3：只保 holder 本人（TEAM 票 + condition 在场守卫，用户口径 2026-09-26）。
    if (self.hp * 2 > max_hp) {
        const int r1_holder = rt.first_slot;
        rt.shenqi_r1_sids[0] = ctx->grant_immunity(
            owner, ImmunityType::ANOMALY, coverage_all(), 0, /*duration_rounds=*/0,
            /*source_id=*/15811, /*soul_immunity=*/false, EffectScope::TEAM,
            /*counts=*/0, /*source_effect_id=*/-1, ImmunityTier::Modern,
            [owner, r1_holder](const BattleContext* c, int) {
                return c && c->on_stage[owner] == r1_holder;
            });
        rt.shenqi_r1_sids[1] = ctx->grant_immunity(
            owner, ImmunityType::STAT_DROP, coverage_all(), 0, 0,
            /*source_id=*/15812, /*soul_immunity=*/false, EffectScope::TEAM,
            /*counts=*/0, /*source_effect_id=*/-1, ImmunityTier::Modern,
            [owner, r1_holder](const BattleContext* c, int) {
                return c && c->on_stage[owner] == r1_holder;
            });
    }

    // 神启 R2（首次登场）：holder 本人就在场上 → 立刻判定（后续登场走 ENTER_STAGE）
    if (ctx->on_stage[owner] == holder && self.hp * 2 > max_hp) {
        ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), 0, 1,
                            /*source_id=*/15813, /*soul_immunity=*/false, EffectScope::TEAM);
        ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(), 0, 1,
                            /*source_id=*/15814, /*soul_immunity=*/false, EffectScope::TEAM);
    }

    // watcher ×2（owner 级只挂一次）
    if (!*qiling_armed_flag(ctx, owner, 0)) {
        *qiling_armed_flag(ctx, owner, 0) = true;
        // 神启 R1 的撤销：任意一方下 1 次技能命中（once；EVENT_HIT 即"技能命中"）
        ctx->event_center_.register_watcher(
            EventType::EVENT_HIT, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/true,
            [owner](BattleContext* wctx, const BattleEvent&) {
                if (!wctx) {
                    return;
                }
                const int h = wctx->find_mark_in_roster(owner, 1581);
                if (h < 0) {
                    return;
                }
                auto& st = wctx->seerRobot[owner].elfPets[h].soulmark_storage;
                const auto it = st.find(kQilingRuntimeKey);
                if (it == st.end()) {
                    return;
                }
                QilingRuntime& rt2 = std::any_cast<QilingRuntime&>(it->second);
                for (int sid : rt2.shenqi_r1_sids) {
                    if (sid > 0) {
                        wctx->rule_center_.revoke(sid);
                    }
                }
                rt2.shenqi_r1_sids[0] = 0;
                rt2.shenqi_r1_sids[1] = 0;
            },
            WatcherScope::TEAM);
    }
    if (!*qiling_armed_flag(ctx, owner, 1)) {
        *qiling_armed_flag(ctx, owner, 1) = true;
        // 神照（首发再登场补盾）+ 神启 R2（自身再登场免控免弱）
        // ⚠️ EVENT_ENTER_STAGE 只在 perform_switch（中切/死亡补位）发；登场槽位 = 事件
        //    drain 时 on_stage[actor]（emit 前已更新）。
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.actor != owner) {
                    return;
                }
                const int h = wctx->find_mark_in_roster(owner, 1581);
                if (h < 0) {
                    return;   // holder 消逝 → 神照/神启都不再提供
                }
                auto& st = wctx->seerRobot[owner].elfPets[h].soulmark_storage;
                const auto it = st.find(kQilingRuntimeKey);
                if (it == st.end()) {
                    return;
                }
                QilingRuntime& rt2 = std::any_cast<QilingRuntime&>(it->second);
                const int entered = wctx->on_stage[owner];
                ElfPet& entered_pet = wctx->seerRobot[owner].elfPets[entered];
                const int holder_max = resolve_pet_max_hp(wctx->seerRobot[owner].elfPets[h]);
                // 神照：首发精灵再登场 → 补一个 30% 盾（下场不清，站场才被磨 → duration=0）
                if (entered == rt2.first_slot && holder_max > 0) {
                    entered_pet.shield_bank_.add(ShieldBank::kSoulShieldPriority,
                                                 holder_max * kShenqiShieldPct / 100,
                                                 kShieldSourceShenzhao, /*duration=*/0);
                }
                // 神启 R2：**自身**再登场且高体力 → 免控免弱至下回合开始
                if (entered == h && holder_max > 0 && entered_pet.hp * 2 > holder_max) {
                    wctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), 0, 1,
                                         /*source_id=*/15813, /*soul_immunity=*/false,
                                         EffectScope::TEAM);
                    wctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                                         0, 1,
                                         /*source_id=*/15814, /*soul_immunity=*/false,
                                         EffectScope::TEAM);
                }
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ============================================================
// 薇尔诗 2513（effect_icon id=2098）：场下源抑制场域
// ============================================================
// 官方文本（子句①）："自身存活于出战阵容时，双方场下的精灵无法指定场上精灵为效果对象，
//   自身死亡时，为己方存活精灵附加黄金万象"。
// 程序模型（docs/02-效果系统/魂印机制设计与精灵表现档案.md §4.0-4.3）：
//   ① 场域开启（OPERATION_ENTER_EXIT_STAGE, ROSTER）：block_offstage_to_onstage = true
//     + 注册**战斗开始的死亡监控**（EVENT_DEATH watcher, TEAM 永久）——宿主阵亡（双方都无
//     存活 2513 宿主）→ 清场域。监控独立于魂印开闭路径：ROSTER 魂印切走不清场域（存活于
//     背包仍生效），而死亡路径必须不被"跳过时点"类效果（星皇之怒）波及——事件监控天然满足。
//   ② 黄金万象（死亡附加）：待做（依赖亡语/死亡触发的 grant 落点）。
// 判别轴（档案 §4.0）：只挡"**源是场下精灵**"的效果；亡语类/源是印记的效果/额外精灵不挡
// ——该判别在**各效果入口**做（源是精灵还是印记由效果自身声明），场域只提供开关。

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // 启灵元神 1581：[神印] 对方主动切换登场挂层 → 登场首战斗阶段结束时按层数真伤
    //   （2026-09-21 对齐官方时点，两段式）
    //   节点 A：OPERATION_ENTER_EXIT_STAGE（once=true, **ROSTER**）— 注册 EVENT_SWAP watcher（永久）
    //           ⚠️ scope 必须是 ROSTER：官方前提「自身**在出战背包时**」——元神在**场下**
    //           照样提供神印（旧默认 STAGE 只在元神**在场**时武装 watcher，场下元神整段
    //           静默——场景 107 A 组即以"元神在背包"搭建而暴露）。
    //           回调内：event.actor == 1-owner → 持有方在背包（消逝即失效）→ stacks += 1
    //           （截断 ≤3）→ 置待结算位（**不扣血**，伤害挪到阶段末）；并把刚登场受害者
    //           的层数**发布**到 plugin_storage（qiling_runtime.h 15830000+side 槽，供
    //           技能侧 1532/1536 跨 dylib 只读；消逝路径照发真实层数、只是不再 +1）。
    //   节点 B：BATTLE_ROUND_END（= 官方"战斗阶段结束时"）— 消费待结算位：按层数对
    //           该方场上那只扣真伤（基数 = **元神自己**的最大体力）；神印不清零。
    //           场景 107 A（基数）/ B（消逝失效）/ E（时点）锁定。
    SOUL_PROGRAM(1581, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1581_register_swap_watcher,
                         /*once=*/true, /*early=*/false, SoulScope::ROSTER),
         SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_1581_phase_end_damage,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         // [神殇] 放逐重掷 + [神启] R3/R4（回合开始；holder 在背包即提供）
         SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1581_round_start,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         // [神照]/[神启] R1/R2 的 deploy：首发登记 + 初始盾 + R1 免控免弱 + watcher×2
         // （PROTECTION_1 恒晚于 ENTER_EXIT_STAGE 桶，903 之佑同款时点理由）
         SoulMarkNodeRef(State::OPERATION_PROTECTION_MECHANISM_1, &soulmark_1581_deploy,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER)});

}  // namespace
