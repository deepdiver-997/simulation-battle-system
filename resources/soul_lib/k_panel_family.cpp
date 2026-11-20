// K8 静态面板批 —— effect 247（29 行面板补丁）/ 137（6 行圣甲+达尔妮）/ 203（4 行攻伤+减伤）
//
// 行清单（tools/soul_effects_dump.py 247 137 203，2026-10-04）与参数解码：
//
//   247/137 六元组槽序（5 行互锚定案）：pos0=攻击 pos1=防御 pos2=特攻 pos3=特防
//   pos4=速度 pos5=体力（贝尼 2359「特攻12速6体15」=「0 0 12 0 6 15」、菲尔 2339、
//   比伯 2265、尼古拉 2371、提普 2300 五行一致）→ NumericalPropertyIndex 映射
//   {pos0→物攻0, pos2→特攻1, pos1→防2, pos3→特防3, pos4→速4, pos5→体5}。
//
//   ⚠️ 数值来源政策：一律 **tips 建表**（monster 查表），不直读 args——247 的 5 参行
//   布局不一致（幻瞳 2478 args「20 0 0 25 0」丢了 tips 的速度+12）、纳斯克 2278 args
//   与 tips 冲突（args「10 0 0 0 10 0」= 攻10特攻10，tips「防御+10 特防+10」，判复制
//   粘贴数据错误）——K 工单口径"魂印文本权威源 = effect_icon.tips"。
//
//   记档（247）：
//   - 2484 阿尔达克「每当中秋节和每个月的22日爆发」日期彩蛋子句——日历门未建模，
//     面板常驻部分照常落地；
//   - 2236 阿苏 / 2265 比伯 / 2280 加特 / 2300 提普 各 2 行（基础/进化形态同
//     monster id，引擎无法区分形态）→ 整单记档跳过，需形态区分机制再收。
//
//   203（攻伤提升X%，受伤减Y%）args[a,b] 布局干净（暗翼 [0,20]、拉塞尔 [2,2]/[3,3]、
//   绝灭 [0,30]）→ **args 驱动**：db[0]>0 挂 AMP（攻伤 +db[0]%），db[1]>0 挂 REDUCE_PCT。
//   「组合加成已激活」激活条件无数据（疑似收集/组合机制）→ 两子句常驻、门控记档。
//
// 落点（suit.cpp ④同款范式）：numericalBase（权威基线）与 numericalProperties
// （对战当前值）**同步加**；体力槽 ElfPet::hp 是 numericalProperties[HP] 的引用自动
// 跟随（手动再加会翻倍，场景 090 首跑事故）。注册面 = SOUL_PRE_BATTLE(STAT_MODIFY)
//（hamo 715 卫岳同款），由 BattleContext ctor 的 run_pre_battle_patches 按
// soulMark.id 派发（effect.cpp :355-367，先全场快照段后全场修改段）。
//
// 三查记录（2026-10-04）：①内核无 247/137/203 硬编码；②effect_info 同 id 为技能空间
// 模板，与魂印无关；③registry soul=78 无三 id；未合并分支无实现。
#include "common.h"

#include <map>

namespace {

// ── 247/137：monster → 六维平加（NPI 序：物攻0 特攻1 防2 特防3 速4 体5）────────

struct PanelBonus {
    int flat[6] = {0, 0, 0, 0, 0, 0};
};

// 数值 = effect_icon.tips 行文（头部政策），非 args。
const PanelBonus* panel_table(int mark_id, int monster_id) {
    static const std::map<int, PanelBonus> k247 = {
        // 创世六神兵：体力+15、其余五维+10
        {2519, {{10, 10, 10, 10, 10, 15}}}, {2497, {{10, 10, 10, 10, 10, 15}}},
        {2488, {{10, 10, 10, 10, 10, 15}}}, {2489, {{10, 10, 10, 10, 10, 15}}},
        {2510, {{10, 10, 10, 10, 10, 15}}}, {2525, {{10, 10, 10, 10, 10, 15}}},
        {2754, {{0, 0, 0, 0, 0, 250}}},            // 麦索瑞尔：体力上限+250
        {2234, {{10, 0, 0, 10, 0, 0}}},            // 拉奥：攻10 特防10
        {2359, {{0, 12, 0, 0, 6, 15}}},            // 贝尼：特攻12 速6 体15
        {2267, {{10, 0, 0, 0, 10, 0}}},            // 雷卡：攻10 速10
        {1178, {{10, 0, 10, 10, 10, 0}}},          // 蒙奇：攻10 防10 特防10 速10
        {2278, {{0, 0, 10, 10, 0, 0}}},            // 纳斯克：tips 防10 特防10（args 冲突，弃）
        {2298, {{0, 0, 10, 10, 0, 0}}},            // 法里德：防10 特防10
        {2328, {{0, 10, 10, 10, 0, 10}}},          // 卡米：特攻10 体10 防10 特防10
        {2339, {{0, 10, 0, 15, 10, 15}}},          // 菲尔：特攻10 速10 特防15 体15
        {2320, {{0, 10, 10, 0, 5, 10}}},           // 洛奇：特攻10 防10 体10 速5
        {2332, {{0, 0, 5, 5, 0, 11}}},             // 小光S：体11 防5 特防5
        {2375, {{0, 0, 5, 5, 0, 11}}},             // 小暗S：体11 防5 特防5
        {2371, {{0, 0, 8, 8, 4, 10}}},             // 尼古拉：体10 防8 特防8 速4
        {2384, {{12, 0, 5, 5, 5, 15}}},            // 萨卡罗：体15 攻12 防5 特防5 速5
        {2484, {{0, 10, 10, 10, 10, 20}}},         // 阿尔达克：体20 防10 特防10 速10 特攻10（日期子句记档）
    };
    static const std::map<int, PanelBonus> k137 = {
        {2432, {{0, 0, 16, 16, 0, 40}}},   // 圣甲地裂：体40 防16 特防16
        {2417, {{25, 0, 0, 0, 11, 40}}},   // 圣甲狂战：体40 攻25 速11
        {2460, {{20, 0, 0, 0, 20, 25}}},   // 圣甲雷鸣：体25 攻20 速20
        {2478, {{0, 25, 0, 0, 12, 20}}},   // 圣甲幻瞳：体20 特攻25 速12（args 缺速，tips 补）
        {2442, {{22, 0, 13, 13, 0, 0}}},   // 圣甲永夜：攻22 防13 特防13
        {1948, {{5, 5, 5, 5, 5, 5}}},      // 达尔妮：六维+5（创世六神兵合体）
    };
    if (mark_id == 247) {
        const auto it = k247.find(monster_id);
        return it != k247.end() ? &it->second : nullptr;
    }
    if (mark_id == 137) {
        const auto it = k137.find(monster_id);
        return it != k137.end() ? &it->second : nullptr;
    }
    return nullptr;
}

void apply_panel_patch(BattleContext* ctx, int side, int slot, int mark_id) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot > 5) {
        return;
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    if (pet.soulMark.id != mark_id) {
        return;
    }
    // 宿主 monster id 在魂印上（真实对局由管线填充；pet.id 是对局内精灵 id，
    // 场景构造里两者不同——host_row 同款口径）。
    const int monster_id = pet.soulMark.monster_id != 0 ? pet.soulMark.monster_id : pet.id;
    const PanelBonus* bonus = panel_table(mark_id, monster_id);
    if (!bonus) {
        return;   // 记档行（进化同 id/查无此宠）不注册任何数值
    }
    // suit.cpp ④范式：base 与 properties 同步加；hp 是 properties[HP] 引用自动跟随。
    for (int i = 0; i < 6; ++i) {
        const int delta = bonus->flat[i];
        if (delta == 0) {
            continue;
        }
        const auto idx = static_cast<NumericalPropertyIndex>(i);
        pet.numericalBase[idx] += delta;
        pet.numericalProperties[idx] += delta;
    }
}

void panel_247_pre_battle(BattleContext* ctx, int side, int slot) {
    apply_panel_patch(ctx, side, slot, 247);
}

void panel_137_pre_battle(BattleContext* ctx, int side, int slot) {
    apply_panel_patch(ctx, side, slot, 137);
}

// ── 203：攻伤提升 db[0]%（AMP）+ 受伤减 db[1]%（REDUCE_PCT）──────────────────

bool armed_this_epoch(BattleContext* ctx, int owner, int storage_key) {
    ElfPet& pet = ctx->getPet(owner);
    auto it = pet.soulmark_storage.find(storage_key);
    if (it == pet.soulmark_storage.end()) {
        it = pet.soulmark_storage.emplace(storage_key, int{-1}).first;
    }
    int* armed_epoch = std::any_cast<int>(&it->second);
    if (!armed_epoch) {
        return true;   // 键位被异类型占用 → 视为已武装（共享键空间，不抛）
    }
    if (*armed_epoch == ctx->pipeline_valid_id[owner]) {
        return true;
    }
    *armed_epoch = ctx->pipeline_valid_id[owner];
    return false;
}

EffectResult soulmark_203_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (armed_this_epoch(ctx, owner, 203 * 100 + 9)) {
        return EffectResult::kOk;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
        slot = ctx->find_pet_with_soulmark(owner, 203);
    }
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const std::vector<int>& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    const int amp_pct = db.size() > 0 ? db[0] : 0;
    const int reduce_pct = db.size() > 1 ? db[1] : 0;
    if (amp_pct <= 0 && reduce_pct <= 0) {
        return EffectResult::kOk;
    }
    if (amp_pct > 0) {
        // 攻伤 +db[0]%：AMP 加法槽（yanshou_4875 神惧劫炎同款实时读持有状态）。
        ctx->register_damage_effect(
            DamagePhase::AMP, owner, DamageEffectCategory::AMP,
            [amp_pct](BattleContext* c, int bucket_owner) {
                DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != bucket_owner || !d.isRed || d.final <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(c, bucket_owner, 203)) {
                    return;
                }
                d.addPct += amp_pct;
                d.final = d.final * (100 + amp_pct) / 100;
            });
    }
    if (reduce_pct > 0) {
        // 受伤减 db[1]%：红伤 REDUCE_PCT（"受到的伤害"措辞 = K1 3/280 同款仅红伤；
        // K5 725 同构）。
        ctx->register_damage_effect(
            DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
            [reduce_pct](BattleContext* c, int bucket_owner) {
                DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(c, bucket_owner, 203)) {
                    return;
                }
                d.final = d.final * (100 - reduce_pct) / 100;
            });
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（203；247/137 走 PRE_BATTLE
//    注册面，不占 SOUL_PROGRAM 数）+ SOUL_PRE_BATTLE ×2（只增不改）─────────────
SOUL_PRE_BATTLE(247, PreBattlePhase::STAT_MODIFY, panel_247_pre_battle);
SOUL_PRE_BATTLE(137, PreBattlePhase::STAT_MODIFY, panel_137_pre_battle);
SOUL_PROGRAM(203, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_203_arm,
                   /*once=*/false, /*early=*/true)});
