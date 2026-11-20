// K471 · 命耗百分比伤族 —— 2231×5（2026-11-17，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k471，分支 feat/k471，基点 fda62989；
// 场景 test/scenario/scenario_8580_k471.cpp，号段 8580-8589；姊妹文件
// k471_selfdestruct_dodge_relay.cpp 承担 1416）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库现查 2026-11-17）══
//   2231（args_num=2）"下{0}回合攻击命中时消耗自身体力的1/{1}并造成等量百分比伤害"
//
// ══ 载体切片（unity 库 SQL 现查 + side_effect_arg 按 effect_info.args_num 顺序
//   切分，5 载体全链零剩余；场景 T8580-pre 以实际加载器 load_skill_effects 逐载体
//   锁定；实现读行参不硬编码）══
//   2231 [{0}=回合, {1}=分母]：
//     28867 欲壑孽海   [1687(1),2230(4),2229(1),2231(2),2232(1)]
//                       =[4|4,2,100,22|3|**2,2**|2]（cat4 属性技 must_hit——武装腿）、
//     28971 花满迷踪   [911(1),2231(2),58(1),843(2)]=[1|**2,3**|2|2,2]
//       （cat4 属性技 must_hit；行为腿经 DisabledEffects 剥 911/58/843 兄弟）、
//     29424 事务排期   [885(1),2231(2),843(2)]=[1|**3,3**|2,2]、
//     38392 锋毫浸血雨 [2231(2),478(1),2320(8),2269(0),2351(2)]
//                       =[**1,2**|2|2,-1,0,-1,0,-1,0,2,1|∅|1,2]
//       （cat2 威力 150 **攻击技**载体——施放回合自伤不触发"下{0}回合"窗的
//       行为腿主载体）、
//     38456 邪·天壤残火 [2079(0),1922(1),2116(2),1799(3),2231(2)]
//                       =[∅|2|2,70|100,22,1|**2,2**]（K457 头注切片互证）。
//   {0}∈{1,2,3}×{1}∈{2,3}，行参全差分由场景腿锁定。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例）══
//   · **窗界 = "下{0}回合"（NextRounds 家族）**：本回合不算、生效 [R+1, R+{0}]
//     （battleContext.h round_effect_start_round 注释定版口径；843 头注"下N回合
//     先/后出手都从下一回合起算"直系）。触发面是**逐命中事件**而非回合 tick →
//     载体 = EVENT_TAKE_DAMAGE watcher：duration_rounds={0}+1（活跃 R..R+{0}，
//     event_center.h cleanup 算术 `current_round-register_round>=duration`）+
//     回调实时门 `roundCount > armed_round`（排除施放回合——38392 施放当回合
//     自身攻击不触发，T8580.d 行为锁定；K456 首跳口径不适用：本族无"当回合"子句）。
//   · **"攻击命中"判据 = EVENT_TAKE_DAMAGE**（f_onhit_family 817 直系挂法；
//     E12 damage_kind 口径）：ev.actor==self && ev.target==1-self &&
//     ev.damage_kind==NORMAL && ev.amount>0——红伤(NORMAL)只在真实落血后 emit
//     （battle_primitives.cpp deal_damage：护盾完全挡下 early return 不发事件、
//     实际扣血才 emit）；miss/被盔无落血 → 无事件（miss 负测 T8580.e）。
//     属性技无红伤 → 天然不触发（"攻击命中"字面）。自耗/反噬自身产生的
//     TAKE_PINK_DAMAGE/TAKE_DAMAGE（kind=FIXED/PERCENT_VALUE）被 kind 过滤排除
//     ——无递归。K1 语义：回调晚 drain，只读 ev.* 载荷不查现场条件——本触发
//     无现场条件查询，载荷过滤即全部判据。
//   · **消耗基准 = 自身当前体力 1/{1}（整数除法 floor）**：唯一同文形先例
//     1185"消耗自身当前体力的1/{0}并使对手受到等量百分比伤害"
//     （j_first_drain_selfcost.cpp:69-80，cost=hp/denom）——消耗类分数模板
//     显式"当前"（1185）/显式"上限"（2254），2231 裸"自身体力"按 1185 直系
//     取当前体力；heal 侧裸"自身体力"（438）取最大的惯例**不跨向消耗类**
//     （heal 天然对 max 定义，消耗天然从当前库存扣）。⚠️ 备择读法（max 基准）
//     无载体可校验，已记档：若官方实测推翻，改 cost 计算一行。
//   · **钳零 = cost = hp/{1} ≤ 0（体力不足 1/{1}）→ 不消耗不反噬**（整数除法
//     floor 结构性钳零；hp=1/{1}=2 → 0 → 早退，审计 slot7=1，T8580.f）。
//     cost ≤ hp/{1} < hp（{1}≥2）→ 消耗永不致死（钳零是唯一不足形态）。
//   · **"等量"= 实耗量（hp delta）**：家族等量百分比伤模板 1005/532/1065/1239
//     全部按**实际恢复量**反噬（1005 healed=heal_max_hp_fraction 实得，满血
//     healed=0 → 不反伤；"避免满血灌 N 点"判例）→ 消耗侧对称取实耗。
//     干净路径 cost==实耗==反噬（与 1185 申报量口径重合）；自耗被自身护罩
//     吸收时实耗=0 → 不反噬（申报量口径则仍反噬——两读法无官方实测，本实现
//     取 1005 家族实耗口径并记档）。
//   · **通道**（1185 直系）：自耗 = deal_pink_damage(self, cost, FIXED)（粉伤
//     FIXED 档：吃自身护罩、emit TAKE_PINK_DAMAGE）；反噬 = deal_pink_damage
//     (target, consumed, PERCENT_VALUE)（百分比档，吃免粉/百分比抗性/护罩——
//     "百分比伤害"字面通道）。顺序按官方语序先消耗后反噬。
//   · **生命周期**：watcher ON_STAGE scope + 显式 valid_id（K453 :165 坑——
//     缺省 0 永不匹配）→ 换宠/下场 epoch 作废；重施放先 remove_watcher 旧 id
//     （on_stage_storage 存 id，防双 watcher 双消耗——K453/K464 卫生）。
//     per-pet 键放 on_stage_storage（下场即清，K453 2026-11-10 统一建模口径：
//     本句无固有/保留说明 → 随下场失效）。
//
// ══ 动工前置三查（2026-11-17，k471 worktree + 共享库只读现查）══
//   ① 内核 grep SKILL_EFFECT(2231/SOUL_PROGRAM(2231/kMark2231 + 2231 裸词
//     src/ include/ 全量：零注册零硬编码（SKILL_EFFECT default 桶）；
//   ② effect_info 2231 行即模板（args_num=2、param=[]）；side_effect 表
//     ac=2；effect_des 无行；custom_effect_overrides 零行（无窗界声明 →
//     NextRounds 显式注册，不依赖家族查询）；boss_invalid_effects.json 无
//     moves 2231 尾注（克雷弗德 2231 = effect_icon 行 = soul 空间，见③）；
//     tools/db_patches grep 2231 零命中（无 900000 拆分迁移与退役记录）；
//   ③ registry moves=1566/soul=415/suit=4（build/registry.json 基线）无 2231；
//     本库 resources/test grep 零注册；兄弟 worktree（agents/endgame-walk/
//     k469/k472/web-replay）SKILL_EFFECT(2231)/SOUL_PROGRAM(2231) 零命中。
//     ⚠️ soul 空间同号甄别：soul_lib/crawford_deleermo_4644.cpp 经
//     `constexpr int kMark = 2231` + SOUL_PROGRAM(kMark,…) 间接注册
//     （克雷弗德/德莱赫默组合印，effect_icon.effect_id=2231 键）——
//     **moves/soul 双空间分表注册，互不冲突**（K453 2456 判例同款）；
//     本族审计键占 1022310000 基址，soul 空间将来若需同号键段按 K458 先例
//     取子块（1022310100+）。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456 键式；
//   1022310 段全库 grep 零占用 2026-11-17，check_storage_keys.py 门禁兜底）══
//   2231 基 1022310000：slot0={0} 回合行参回显、1={1} 分母行参回显、
//     2=末次触发回合（-1=未触发）、3=触发计数（累计）、
//     4=末次申报 cost（=hp/{1}）、5=末次实耗量（hp delta）、
//     6=末次反噬申报量（=实耗）、7=末次钳零标记（1=cost≤0 早退/0=正常）、
//     8=施放计数、9=跳过原因（0=无/1=缺 deal_pink_damage 原语）
//   布局自防撞：单基址 per effect + owner×16 步长 > 槽位上限 9 → 同基址
//   owner 间无交叠；姊妹文件 1416 基=1014160000 不同段。
//
// ══ 变异测试实测（2026-11-17，本 worktree；证据 build/k471-mutations/M1..M8/，
//   每份含 semantic-change.py+semantic.diff+build.log+run.rc+first-kill+
//   dylib-sha-mutated/restored+rerun.rc；变异驱动删 .o 强编——Make 3.81 mtime
//   整数秒截断坑，SHA 双守卫：变异 dylib SHA≠底本 3b292710…、还原后 SHA==底本
//   再强编 deploy 复绿 rerun rc=0/SBS_SCENARIO_CHECKS=213）══
//   本文件（2231）承担的四杀（均真红一杀）：
//     M1 键位·owner 步长归零（audit_key_of *16→*0 两 owner 撞键）→
//       T8580.d side0 施放计数 2≠1（镜像串账）；
//     M4 门·施放回合实时门删除 → T8580.c rc0 触发 1≠0（下{0}回合窗界塌缩）；
//     M6 回退分支·钳零早退删除 → T8580.e 钳零标记 0≠1（体力不足不早退）；
//     M7 行参硬编码·{1} 分母写死 2 → T8580.a 申报 cost 30000≠20000（行参驱动）。
//   姊妹文件（1416）承担：M2 生命周期、M3 通道（自爆收尾）、M5 概率申报面、
//   M8 公式基准——见 k471_selfdestruct_dodge_relay.cpp 头注变异节。
//   helper 封装写键键盲区防撞（K460 先例）：audit_key_of 为本文件匿名空间
//   helper，check_storage_keys.py 静态扫不可见 → 1022310 键段已全库 grep
//   补核零占用（2026-11-17，含兄弟 worktree）。

#include "common.h"

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k2231AuditBase = 1022310000;   // 10^9 + 2231×10^4

// on_stage_storage 键段（per-pet 本场上场期；本文件私有，check_storage_keys.py 兜底）：
//   9902231xx = 99 + 效果号 2231 系（K453 9902456xx 同构）。
constexpr int k2231WatcherIdKey = 990223101;  // 活跃 watcher id（重施放先摘旧）

inline int audit_key_of(int base, int owner, int slot) { return base + owner * 16 + slot; }

int storage_int_local(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    return any_int_or(it->second, def);
}

void set_storage_int_local(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

int pet_storage_int(const ElfPet& pet, int key, int def = 0) {
    const auto it = pet.on_stage_storage.find(key);
    return it == pet.on_stage_storage.end() ? def : any_int_or(it->second, def);
}

// ── 2231 的 TAKE_DAMAGE watcher 回调 ──
// 过滤序（全在副作用前）：本方登场（scope/valid_id 兜）→ 自方攻击落血
// （actor/target/kind/amount）→ 窗界实时门（下{0}回合：排除施放回合）→
// 存活门 → cost 计算（钳零）→ 自耗（FIXED 粉）→ 实耗 → 反噬（PERCENT_VALUE 粉）。
void k471_on_attack_hit(BattleContext* c, const BattleEvent& ev, int self, int denom,
                        int armed_round) {
    if (!c) {
        return;
    }
    // "攻击命中"= 自方红伤真实落血（E12/817 口径；miss/被盔/属性技/自伤/反噬
    // 皆无此形态事件——miss 负测与施放回合门由场景锁定）。
    if (ev.actor != self || ev.target != 1 - self) {
        return;
    }
    if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
        return;
    }
    // "下{0}回合"窗界实时门：施放回合（含施放技自身攻击）不触发（843 NextRounds 口径）。
    if (c->roundCount <= armed_round) {
        return;
    }
    ElfPet& armed = c->getPet(self);
    if (armed.hp <= 0) {
        return;   // 存活门（K453 同款保守读法；攻击命中者理论上必存活）
    }

    // 消耗 = 自身当前体力 1/{1}（1185 直系；整数除法 floor）。
    const int cost = armed.hp / denom;
    set_storage_int_local(c, audit_key_of(k2231AuditBase, self, 2), c->roundCount);
    set_storage_int_local(c, audit_key_of(k2231AuditBase, self, 3),
                          storage_int_local(c, audit_key_of(k2231AuditBase, self, 3)) + 1);
    set_storage_int_local(c, audit_key_of(k2231AuditBase, self, 4), cost);
    if (cost <= 0) {
        // 钳零（体力不足 1/{1}）：不消耗不反噬（整数除法结构性钳零；审计可断言）。
        set_storage_int_local(c, audit_key_of(k2231AuditBase, self, 5), 0);
        set_storage_int_local(c, audit_key_of(k2231AuditBase, self, 6), 0);
        set_storage_int_local(c, audit_key_of(k2231AuditBase, self, 7), 1);
        return;
    }
    set_storage_int_local(c, audit_key_of(k2231AuditBase, self, 7), 0);
    if (!g_core || !g_core->deal_pink_damage) {
        set_storage_int_local(c, audit_key_of(k2231AuditBase, self, 9), 1);
        return;
    }
    // 自耗：粉伤 FIXED 档打自己（吃自身护罩；emit TAKE_PINK_DAMAGE——
    // kind 过滤保证不自我递归触发）。
    const int hp_before = armed.hp;
    (void)g_core->deal_pink_damage(c, self, cost, DamageKind::FIXED, self);
    // "等量"= 实耗量（hp delta；1005 家族实耗口径——自耗被自身护罩吸收时
    // 实耗=0 → 不反噬；干净路径实耗==cost）。
    const int consumed = hp_before - c->getPet(self).hp;
    set_storage_int_local(c, audit_key_of(k2231AuditBase, self, 5), consumed);
    set_storage_int_local(c, audit_key_of(k2231AuditBase, self, 6), consumed);
    if (consumed > 0) {
        (void)g_core->deal_pink_damage(c, /*target=*/1 - self, consumed,
                                       DamageKind::PERCENT_VALUE, self);
    }
}

// ── 2231 —— 下{0}回合攻击命中时消耗自身体力的1/{1}并造成等量百分比伤害 ──
// int_args = [owner, target, {0}=回合, {1}=分母]（args_num=2 + 前缀）。
EffectResult effect_2231_hit_cost_equal_damage(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2231, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2231, args,
                     "2231 布局=[owner,target,回合,分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 2231,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0, 2231, args,
                     "需要 回合>0、分母>0（1/{1} 才有定义；0 回合=无窗不武装）");
    if (!g_core) {
        return EffectResult::kOk;
    }

    const int armed_round = ctx->roundCount;
    ElfPet& pet = ctx->getPet(self);

    // 重施放先摘旧 watcher（防双 watcher 双消耗——K453/K464 同款卫生）
    const int old_watcher = pet_storage_int(pet, k2231WatcherIdKey);
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }

    // 行参回显 + 施放计数（审计面）
    set_storage_int_local(ctx, audit_key_of(k2231AuditBase, self, 0), rounds);
    set_storage_int_local(ctx, audit_key_of(k2231AuditBase, self, 1), denom);
    set_storage_int_local(ctx, audit_key_of(k2231AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k2231AuditBase, self, 8)) + 1);
    set_storage_int_local(ctx, audit_key_of(k2231AuditBase, self, 2), -1);   // 窗基线复位
    set_storage_int_local(ctx, audit_key_of(k2231AuditBase, self, 3), 0);
    set_storage_int_local(ctx, audit_key_of(k2231AuditBase, self, 7), 0);

    // 事件投递按 type 不按 owner → 回调闭包捕获武装方 side 自过滤（K453 同构）。
    // duration={0}+1 → 活跃 R..R+{0}；窗内起点门在回调实时判（施放回合排除）。
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, armed_round,
        /*duration_rounds=*/rounds + 1, /*once=*/false,
        [self, denom, armed_round](BattleContext* c, const BattleEvent& ev) {
            k471_on_attack_hit(c, ev, self, denom, armed_round);
        },
        WatcherScope::ON_STAGE,
        /*valid_id=*/ctx->watcher_valid_id[self]);   // 必传当前 epoch——缺省 0 永不匹配
    pet.on_stage_storage[k2231WatcherIdKey] = watcher_id;
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +1）──
SKILL_EFFECT(2231, effect_2231_hit_cost_equal_damage)
