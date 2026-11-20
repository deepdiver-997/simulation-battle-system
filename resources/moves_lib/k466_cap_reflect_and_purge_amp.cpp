// K466 · 封顶反弹 + 消除接附加伤 两族 —— 1834×6 + 1849×6（2026-11-13，
// worktree ~/.agent-workspaces/workspaces/sbs-local/k466，分支 feat/k466，基点 31fe17d7；
// 场景 test/scenario/scenario_8530_k466.cpp，号段 8530-8539）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-13）══
//   1834（args_num=3）"自身下{0}次受到的攻击伤害不超过{1}，并反弹给对手原本超出部分
//         {2}%的百分比伤害"
//   1849（args_num=2）"消除双方能力提升、下降状态，消除任意一方成功则自身下{0}次攻击
//         技能附加对手最大体力1/{1}的百分比伤害"
//
// ══ 载体切片（unity 库 SQL 现查 + loader build_skill_effects 顺序切分；side_effect
//   表缺行时 loader 按 effect_info.args_num 兜底（official_data_repository.cpp:174
//   .value_or 直系）——29455 的 2576、38228 的 2355/2391 三行即走兜底，切分零剩余；
//   实现读行参不硬编码，场景 T8530-pre 以实际加载器 load_skill_effects 逐载体锁定）══
//   1834 [{0}=次数,{1}=封顶,{2}=反弹%]（六载体：四属性技 + 两特攻攻击技）：
//     28276 灼魂炼骸 [191(1),1067(4),1834(3),843(2)]=[4|4,3,2,3|**1,280,100**|2,2]、
//     29080 回想重叠 [48(1),1949(1),1834(3)]=[4|1|**2,600,60**]（唯一 {0}=2）、
//     29117 划裂围牢 [1687(1),1004(2),1834(3),1614(4)]=[4|4,200|**1,300,100**|3,100,7,1]、
//     29455 雷·万鸣天锻 [191(1),2576(2),1683(1),1200(3),1834(3)]=[4|4,70|4|4,100,0|
//       **1,300,70**]、
//     36006 沐花释离愁 [1834(3),949(1),681(1)]=[**1,300,100**|300|2]（cat2 攻击技
//       power160、hide_moves 宠 4397——主载体）、
//     38376 夜话 [1949(1),1834(3)]=[1|**1,0,100**]（cat2 攻击技 prio3，{1}=**0**——
//       "不超过 0"全反档边界载体：受击归零 + 超出部分=原伤全额反弹）。
//   1849 [{0}=次数,{1}=分母]（六载体：三属性技 + 三攻击技）：
//     28068 威仪之姿 [1849(2),597(2),1213(2)]=[**1,2**|4,3|4,3]（唯一 {1}=2）、
//     29344 公平对话 [1849(2),1850(2),843(2)]=[**1,3**|1,50|2,2]、
//     36390 守鹿 [1850(2),1849(2)]=[1,100|**1,3**]（cat1 物理 power140）、
//     36682 安营扎寨 [1849(2)]=[**1,3**]（cat1 物理 power85 prio3——**纯链单效果
//       攻击技，主载体**）、
//     37248 皓曜华宴 [1849(2)]=[**1,3**]（cat2 特攻 power130——纯链单效果，主载体）、
//     38228 赤绘 [1849(2),2068(2),2073(1),2355(3),2391(2)]=[**2,3**|100,16|50|3,23,2|
//       0,2]（唯一 {0}=2；2068 掷点异常链——精确断言不选，只进切片核验）。
//
// ══ 1834 实现口径 ══
//   · 封顶 = **DamagePhase::CAP 一等公民通道**（damage_pipeline.h:52"伤害锁定——最后
//     再拦一次，697 凭证可穿"；K1 766 首批消费者 + J-purge 1897 消强接锁伤直系 +
//     f_mitigate_cap 794 同通道）。类别 = MITIGATE（官方"减伤不可被挡伤失效无视"）。
//   · "攻击伤害"甄别 = d.isRed 且 **非属性伤害**（d.attribute_element 双零判据——
//     deal_attribute_damage 专设字段，battle_primitives.cpp:1252；官方《赛学必修16》
//     攻击伤害/属性伤害独立分类，k_damage_cap2 928 实时判同口径）。属性直伤、粉伤、
//     真伤一律不封顶不消耗。
//   · "下{0}次" = 次数票 **per-side 逐次消耗**（每次合格命中扣 1，含未超出封顶的
//     命中——票的承诺面是"下一次受击"，与消费量纲无关；坑集口径"次数票响应即扣"
//     同式）。票态存 plugin_storage（battle 级审计键段），CAP 条目本身 ON_STAGE
//     绑定 pipeline_valid_id（换宠 epoch 作废 = 离场清理结构性成立）+ 授予代际
//     generation（重施放**刷新不叠加**，旧 lambda 代际不匹配自灭——K459 1804/K456
//     覆盖键记档同口径；official 无叠加实测，按内核既定覆盖语义记档）。
//   · miss/技能无效趟管线整段不跑（battleFsm ATTACK_DAMAGE 早退）→ 天然不消耗
//     （damage_pipeline.h:104 内生保证，场景变异锁定）。
//   · 697「无视伤害限制」凭证消费 = CAP 回调查 ws.attack_credential[attacker]
//     .ignore_damage_limit（K1 766 头注"实现锁伤必须查"纪律；7450 场景 T3 同测面）：
//     凭证命中 → 不封顶不反弹（无"原本超出部分"可言）但**仍消耗**（受击事实成立，
//     票的承诺被凭证压过——官方无实测，按保守口径记档）。
//   · 反弹 = **超出部分 × {2}%**（超出部分 = 封顶前管线值 − {1}，**非全额**），
//     沿 DETECT 相位内联 deal_pink_damage(PERCENT_VALUE)（j_order_pink 853 /
//     j_miss_drain_pierce 989 同款"DETECT 相位内联附加/反弹粉伤"先例；K462 1855
//     裁定 PERCENT_VALUE 才是"值由来源算出、走百分比抗性/护罩"正确档，783/1853
//     旧先例 PERCENT 档错位不复刻）。反弹目标 = 本次攻击方 d.attackerId。
//   · CAP→DETECT 同一 walk 传递（kOrder CAP=7 < DETECT=9，phase_mask 顺序保证）：
//     CAP 写 slot7(超出量)/slot13(post-cap 值)，DETECT 读之——**逐段同步**，多段
//     攻击逐 hit 消费/反弹，无 TAKE_DAMAGE 批量 drain 的重复反弹问题（E12 watcher
//     式反弹在多段面会重复回敬，故不走 watcher）。
//   · "反弹跟随伤害落地"对账 = DETECT 读 d.final==slot13（post-cap 值）才反弹：
//     CAP 后 BLOCK 相归零（免伤票挡下）的命中不反弹（伤害没落地；票仍消耗——封顶
//     义务在 CAP 相位已履行）。cap=0 载体 post-cap 本就是 0，与 BLOCK 归零同形，
//     反弹照发（超出部分在封顶时已成立）。
//   · 反弹量 = exceed × {2}% 整除；{2}=0 = 0% 在位即消费（反弹 0，封顶照常）；
//     {2}=100（五载体）= 全额反弹超出部分。
//
// ══ 1849 实现口径 ══
//   · 消除 = **clear_stat_boosts / clear_stat_drops 原语四象限独立调用**（self/opp ×
//     提升/下降；"效果解除路径"唯一落点：本体/视图同步 write_ability_level + 提升
//     侧 emit EVENT_STAT_BOOSTS_LOST（E13）+ 逐槽 EVENT_STAT_CHANGED；下降侧故意
//     不查有利免疫、唯一例外 STAT_DROP_LOCK——battle_primitives.cpp:1633 内生）。
//     四象限返回值（实际清除槽位数）分记审计槽 7/8/9/10。
//   · "消除任意一方成功"门 = **四象限合计 >0**（至少一处真清除；双侧全无 → 门关
//     不授票，负测腿）。STAT_CLEAR 免疫（1960 族）会整侧封死提升消除——原语返回
//     0 即门面如实收缩（下降侧仍可解），官方语义内生不特判。
//   · 附加伤票 = **ON_SKILL_HIT 读面消费**（K459 1804 同构直系）：SKILL_EFFECT 授票
//     （{0} 行参；重施放刷新不叠加）+ FIRST/SECOND 双注册 ON_SKILL_HIT 桶
//     （CE duration=-1 永久、断回合不清、ON_STAGE 换宠 epoch 作废；桶在
//     resolve_skill_execution 之后、SKILL_EFFECT/ATTACK_DAMAGE 之前——载体趟
//     自身不消费"下{0}次"）。攻击技**有效结算趟**消费一票（miss/被封趟引擎不执行
//     ON_SKILL_HIT 桶，结构性不消耗）；属性技不消耗不附加（skill_type_view 门）。
//   · 附加伤 = deal_pink_damage(PERCENT_VALUE)，amount = 对手最大体力 / {1} 整除
//     （K462 1855/1393 同文同通道直系；{1}<=0 = 关闸——票照消费、不造成伤害，
//     K462 "{2}<=0 在位即消费"记档口径）。
//
// ══ 动工前置三查（2026-11-13，k466 worktree + unity/lxy 库 file URI ?mode=ro）══
//   ① 内核 grep 1834/1849（src/ include/ 全量）零命中——无路由无硬编码；
//   ② effect_info 两 id 在列（args_num=3/2），与看板行派发一致；12 载体顺序切分
//     零剩余（上方清单，2355/2391/2576 走 args_num 兜底）；effect_icon 空间同号
//     甄别（K459 1804 同款）：icon 行 1834（宠 3941，"免疫并反弹异常+害怕"魂印）、
//     1849（宠 4649，"星火之灼+攻特攻取最高"魂印）——魂印空间同号，与 move 效果
//     两族无关（三验②文件级甄别记档）；boss_invalid/拆分迁移：tools/db_patches
//     与 06-交接/08-参考 grep 1834/1849 零命中——无 900000 拆分与退役记录；
//   ③ registry moves 基线（31fe17d7 树）无两 id；resources/test/examples + 兄弟
//     worktree（k465/k467/k468/endgame-walk/web-replay）SKILL_EFFECT(1834/1849)/
//     SOUL_PROGRAM(1834/1849)/kMark1834/kMark1849 零命中——真缺口。
//   审计键段全库 grep 无撞（2026-11-13）：1834 基 1018340000、1849 基 1018490000
//   （10^9+id×10^4 口径）；soul 空间无同号实现 → 无需 K461 式审计键子块拆分。
//   ⚠️ 基点记档（与派发文本差集）：31fe17d7 本身不含 K461-K463 实现，但 feat/k466
//   分支 tip 已随看板同步提交（f18f8055 等）带上 K461-K463 合入内容——k461/k463
//   先例文件在本树可用（K463 2226 消除原语与 K459 1804 读面票均直接照抄结构）。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456 键式；
//   1018340/1018490 段全库 grep 零占用 2026-11-13）══
//   1834 基 1018340000：slot0=当前余票、1=授予时 pipeline_valid_id 快照、2={0}回显、
//     3={1}封顶回显、4={2}反弹%回显、5=末次封顶回合、6=累计封顶次数、7=末次命中
//     超出量（CAP→DETECT 同 walk 传递；票尽趟清 0）、8=授予代际、9=跳过原因
//     （1=缺 deal_pink_damage）、10=末次反弹量、11=累计反弹次数、12=末次反弹回合、
//     13=末次命中 post-cap 值（DETECT 对账）、14=末次命中 697 穿透标记
//   1849 基 1018490000：slot0=当前余票、1=授予时 round_effect_valid_id 快照、
//     2={0}回显、3={1}分母回显、4=末次消费回合、5=累计消费计数、6=末次附加伤量、
//     7=消除 self 提升、8=消除 opp 提升、9=消除 self 下降、10=消除 opp 下降、
//     11=门判定（0=关/1=开）、12=累计授予计数
//   布局自防撞：单基址 per effect + owner×16 步长 > 槽位上限 14 → 同基址内 owner
//   间无交叠；跨文件撞键由 tools/check_storage_keys.py 门禁静态扫。
//
// ══ 变异测试实测（2026-11-13，本 worktree；10/10 真红→源码逐字节还原→SHA==底本
//   4eedba46…→强编 deploy 复绿 224 断言 rc=0；权威轮=build/k466-mutations/，
//   每变异：semantic-diff/build.log/run.rc/first-kill/dylib-sha-mutated/restored/
//   rerun.rc）══
//   M1 键位步长（owner×16→×8 同基址交叠）→ 首杀 T8530.e R3 镜像腿②（消费串账）；
//   M2 属性伤害甄别移除 → 首杀 T8530.h（属性伤被封顶 loss=300≠5000）；
//   M3 门缺失（1849 无条件授票）→ 首杀 T8530.m（双侧全无 gate=1≠0）；
//   M4 反弹基数全额化（超出量记全额）→ 首杀 T8530.a（反弹带超带）；
//   M5 消费仅封顶命中 → 首杀 T8530.b（不超侧票未消耗）；
//   M6 生命周期（管线 scope ON_STAGE→TEAM）→ 首杀 T8530.f（换宠后旧票仍封顶）；
//   M7 重施放叠加化 → 首杀 T8530.r（余 2≠1；⚠️ 第一轮幸存记档：原单载体断言下
//     消费+叠加==刷新重合——载体重施放趟自身先消费一票（ON_SKILL_HIT 先于
//     SKILL_EFFECT），改双载体强判别（R1 {0}=2→R2 {0}=1）后重打压杀）；
//   M8 附加伤公式（分母+1）→ 首杀 T8530.l（pink 24999≠33333 停泊观测）；
//   M9 封顶行参硬编码（cap 写死 300）→ 首杀 T8530.c（600 载体 loss=300≠600）；
//   M10 下降象限消除缺失 → 首杀 T8530.l（slot9=0≠1）。
//   概率面 N/A（两族无概率行参无掷点通道）；「未触发回退」面 = M3 覆盖（1849
//   消除成功门即本族唯一条件分支）——K456 同口径记档。
//
// ══ 先例 ══
//   K459 1804（ON_SKILL_HIT 读面票/epoch 作废/刷新不叠加——1849 票同构）、
//   K463 2226（消除原语四象限独立调用/门=合计>0——1849 消除直系）、
//   K462 1855（PERCENT_VALUE 附加百分比伤正确档/K461-K463 在树佐证）、
//   K1 766 + j_purge_success2 1897 + f_mitigate_cap 794（CAP 相位/697 凭证/
//   MITIGATE 类别）、j_order_pink 853 + j_miss_drain_pierce 989（DETECT 内联
//   粉伤附加/反弹）、E12（受击反弹族对照——本单因多段重复反弹弃 watcher 走 DETECT）。
// 场景回归：test/scenario/scenario_8530_k466.cpp（号段 8530-8539）。

#include "common.h"

#include <primitives/battle_primitives.h>

#include <effects/damage_pipeline.h>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1834AuditBase = 1018340000;   // 10^9 + 1834×10^4
constexpr int k1849AuditBase = 1018490000;   // 10^9 + 1849×10^4

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

// ══════════════════ 1834 封顶反弹 ══════════════════

// CAP 相位回调：合格命中扣票 + 封顶 + 记超出量（DETECT 同 walk 读取）。
// 合格 = 红伤、非属性伤害、final>0、本方代际在任、余票>0。
void k466_1834_cap(BattleContext* c, int bucket_owner, int self, int generation) {
    if (!c || bucket_owner != self) {
        return;
    }
    // 代际守卫：重施放后旧条目自灭（桶条目 ON_STAGE valid_id 换宠已作废，
    // 同 epoch 重授时代际不匹配者不得再写票态）。
    if (storage_int_local(c, audit_key_of(k1834AuditBase, self, 8)) != generation) {
        return;
    }
    DamageSnapshot& d = c->resolvedDamage;
    if (d.defenderId != self || !d.isRed || d.final <= 0) {
        return;   // 只管自己受到的红伤（攻击伤害面）
    }
    if (d.attribute_element[0] != 0 || d.attribute_element[1] != 0) {
        return;   // 属性伤害 ≠「攻击伤害」（赛学必修16 分类；k_damage_cap2 928 口径）
    }
    const int cnt_key = audit_key_of(k1834AuditBase, self, 0);
    const int remain = storage_int_local(c, cnt_key);
    set_storage_int_local(c, audit_key_of(k1834AuditBase, self, 13), d.final);
    set_storage_int_local(c, audit_key_of(k1834AuditBase, self, 14), 0);
    if (remain <= 0) {
        set_storage_int_local(c, audit_key_of(k1834AuditBase, self, 7), 0);   // 票尽：清陈旧超出量
        return;   // 无票不封顶不消耗
    }
    set_storage_int_local(c, cnt_key, remain - 1);   // 逐次消耗（含未超封顶命中）
    // 697「无视伤害限制」凭证穿锁伤（K1 766 纪律）：不封顶不反弹，仍消耗。
    if (d.attackerId >= 0 && d.attackerId <= 1
        && c->ws.attack_credential[d.attackerId].ignore_damage_limit) {
        set_storage_int_local(c, audit_key_of(k1834AuditBase, self, 14), 1);
        set_storage_int_local(c, audit_key_of(k1834AuditBase, self, 7), 0);
        return;
    }
    const int cap = storage_int_local(c, audit_key_of(k1834AuditBase, self, 3));
    if (d.final > cap) {
        set_storage_int_local(c, audit_key_of(k1834AuditBase, self, 7), d.final - cap);
        d.final = cap;   // 封顶（"受到的攻击伤害不超过{1}"）
        set_storage_int_local(c, audit_key_of(k1834AuditBase, self, 5), c->roundCount);
        set_storage_int_local(c, audit_key_of(k1834AuditBase, self, 6),
                              storage_int_local(c, audit_key_of(k1834AuditBase, self, 6)) + 1);
    } else {
        set_storage_int_local(c, audit_key_of(k1834AuditBase, self, 7), 0);   // 未超出：无反弹
    }
    set_storage_int_local(c, audit_key_of(k1834AuditBase, self, 13), d.final);
}

// DETECT 相位回调：读同 walk 超出量，反弹 exceed×{2}% 粉伤给攻击方（853/989 内联先例）。
void k466_1834_reflect(BattleContext* c, int bucket_owner, int self, int generation) {
    if (!c || bucket_owner != self) {
        return;
    }
    if (storage_int_local(c, audit_key_of(k1834AuditBase, self, 8)) != generation) {
        return;   // 代际守卫（同 CAP）
    }
    DamageSnapshot& d = c->resolvedDamage;
    if (d.defenderId != self || !d.isRed || d.attackerId < 0 || d.attackerId > 1
        || d.attackerId == self) {
        return;
    }
    const int exceed = storage_int_local(c, audit_key_of(k1834AuditBase, self, 7));
    if (exceed <= 0) {
        return;   // 无超出（未超封顶/票尽/697 穿透/属性伤）→ 无反弹
    }
    // 对账：d.final 须仍等于 CAP 记下的 post-cap 值——BLOCK 相归零（免伤票挡下）
    // 的命中不反弹（伤害未落地）。cap=0 载体 post-cap 本就是 0，同形照反弹。
    if (d.final != storage_int_local(c, audit_key_of(k1834AuditBase, self, 13))) {
        return;
    }
    const int pct = storage_int_local(c, audit_key_of(k1834AuditBase, self, 4));
    if (pct <= 0 || !g_core || !g_core->deal_pink_damage) {
        set_storage_int_local(c, audit_key_of(k1834AuditBase, self, 9), 1);   // 跳过：0%/缺原语
        return;
    }
    const int back = exceed * pct / 100;
    if (back > 0) {
        (void)g_core->deal_pink_damage(c, d.attackerId, back, DamageKind::PERCENT_VALUE, self);
        set_storage_int_local(c, audit_key_of(k1834AuditBase, self, 10), back);
        set_storage_int_local(c, audit_key_of(k1834AuditBase, self, 11),
                              storage_int_local(c, audit_key_of(k1834AuditBase, self, 11)) + 1);
        set_storage_int_local(c, audit_key_of(k1834AuditBase, self, 12), c->roundCount);
    }
}

// ── 1834 —— 自身下{0}次受到的攻击伤害不超过{1}，并反弹给对手原本超出部分{2}%的百分比伤害 ──
// int_args = [owner, target, {0}=次数, {1}=封顶, {2}=反弹%]（args_num=3 + 前缀）。
EffectResult effect_1834_cap_reflect(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1834, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1834, args,
                     "1834 布局=[owner,target,次数,封顶,反弹%]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int counts = args.int_args[2];
    const int cap = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1834,
                     args, "需要 owner/target ∈ {0,1} 且相异");
    EFFECT_ARG_GUARD(counts > 0 && cap >= 0 && pct >= 0 && pct <= 100, 1834, args,
                     "需要 次数>0、封顶>=0、反弹%∈[0,100]");
    // 票态（重施放刷新不叠加：代际递增，旧 CAP/DETECT 条目代际不匹配自灭）。
    set_storage_int_local(ctx, audit_key_of(k1834AuditBase, self, 0), counts);
    set_storage_int_local(ctx, audit_key_of(k1834AuditBase, self, 1), ctx->pipeline_valid_id[self]);
    set_storage_int_local(ctx, audit_key_of(k1834AuditBase, self, 2), counts);
    set_storage_int_local(ctx, audit_key_of(k1834AuditBase, self, 3), cap);
    set_storage_int_local(ctx, audit_key_of(k1834AuditBase, self, 4), pct);
    set_storage_int_local(ctx, audit_key_of(k1834AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k1834AuditBase, self, 8)) + 1);
    const int generation = storage_int_local(ctx, audit_key_of(k1834AuditBase, self, 8));
    // CAP 条目：ON_STAGE 绑定（register_damage_effect 默认 scope 取当前
    // pipeline_valid_id）→ 换宠 epoch 作废 = 离场清理结构性成立。
    ctx->register_damage_effect(
        DamagePhase::CAP, self, DamageEffectCategory::MITIGATE,
        [self, generation](BattleContext* c, int bucket_owner) {
            k466_1834_cap(c, bucket_owner, self, generation);
        });
    ctx->register_damage_effect(
        DamagePhase::DETECT, self, DamageEffectCategory::DETECT,
        [self, generation](BattleContext* c, int bucket_owner) {
            k466_1834_reflect(c, bucket_owner, self, generation);
        });
    return EffectResult::kOk;
}

// ══════════════════ 1849 消除接附加伤 ══════════════════

// ON_SKILL_HIT 消费体：本施放方攻击技有效结算趟扣一票 + 附加对手最大体力 1/{1}
// 百分比伤（PERCENT_VALUE，K462 1855 通道）。行参 [self, opp]（票态走 storage 实时面）。
EffectResult effect_1849_on_skill_hit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp) {
        return EffectResult::kOk;
    }
    // ① 攻击技门：属性技不消耗不附加（"下{0}次**攻击**技能"；skill_type_view 在
    //    resolve_skill_execution 已物化，1804 同款读面）。
    if (ctx->ws.skill_type_view[self] == static_cast<int>(SkillType::Attribute)) {
        return EffectResult::kOk;
    }
    // ② 施放方换宠 → 票随 epoch 作废（离场清理；桶条目本身由 cleanup 惰性移除）。
    const int cnt_key = audit_key_of(k1849AuditBase, self, 0);
    const int epoch_key = audit_key_of(k1849AuditBase, self, 1);
    if (storage_int_local(ctx, epoch_key) != ctx->round_effect_valid_id[self]) {
        ctx->plugin_storage.erase(cnt_key);
        return EffectResult::kOk;
    }
    // ③ 余票 >0 才消费；==0 无操作（凭证耗尽的常态路径）。
    const int remain = storage_int_local(ctx, cnt_key);
    if (remain <= 0) {
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, cnt_key, remain - 1);
    set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 5),
                          storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 5)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 4), ctx->roundCount);
    // ④ 附加伤：对手最大体力 1/{1} 整除（PERCENT_VALUE 具体值档）；{1}<=0 关闸
    // （票照消费、不造成伤害——K462 "{2}<=0 在位即消费"记档口径）。
    const int denom = storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 3));
    if (denom > 0 && g_core && g_core->deal_pink_damage) {
        const int amount = max_hp_of(ctx, opp) / denom;
        if (amount > 0) {
            (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE, self);
            set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 6), amount);
        }
    }
    return EffectResult::kOk;
}

// ── 1849 —— 消除双方能力提升、下降状态，消除任意一方成功则自身下{0}次攻击技能附加
//             对手最大体力1/{1}的百分比伤害 ──
// int_args = [owner, target, {0}=次数, {1}=分母]（args_num=2 + 前缀）。
EffectResult effect_1849_purge_boosts_then_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1849, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1849, args,
                     "1849 布局=[owner,target,次数,分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int counts = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1849,
                     args, "需要 owner/target ∈ {0,1} 且相异");
    EFFECT_ARG_GUARD(counts > 0, 1849, args, "需要 次数>0");
    set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 2), counts);
    set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 3), denom);
    if (!g_core || !g_core->clear_stat_boosts || !g_core->clear_stat_drops) {
        set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 11), 0);
        set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 13), 1);  // 跳过：缺原语
        return EffectResult::kOk;
    }
    // 消除双方提升、下降（四象限独立调用、独立计数——"消除**双方**能力提升、下降状态"）。
    const int clear_self_up = g_core->clear_stat_boosts(ctx, self);
    const int clear_opp_up = g_core->clear_stat_boosts(ctx, opp);
    const int clear_self_down = g_core->clear_stat_drops(ctx, self);
    const int clear_opp_down = g_core->clear_stat_drops(ctx, opp);
    set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 7), clear_self_up);
    set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 8), clear_opp_up);
    set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 9), clear_self_down);
    set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 10), clear_opp_down);
    // "消除任意一方成功"门：至少一处真清除。全无 → 门关不授票（负测腿）。
    if (clear_self_up + clear_opp_up + clear_self_down + clear_opp_down <= 0) {
        set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 11), 0);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 11), 1);
    // 附加伤票（刷新语义：重施放覆盖不叠加，1804/K456 覆盖键记档同口径）+ epoch 快照。
    set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 0), counts);
    set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 1),
                          ctx->round_effect_valid_id[self]);
    set_storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 12),
                          storage_int_local(ctx, audit_key_of(k1849AuditBase, self, 12)) + 1);
    // ON_SKILL_HIT 消费钉：FIRST/SECOND 双注册（桶按 (state, owner) 配对 → 只有自己
    // 行动趟跑）；CE duration=-1 永久（次数票无回合窗；断回合不清、ON_STAGE 换宠
    // epoch 作废）；同源键 source_id 重授覆盖。
    for (const State st :
         {State::BATTLE_FIRST_ON_SKILL_HIT, State::BATTLE_SECOND_ON_SKILL_HIT}) {
        Effect e(1849, 0, self, /*left_round=*/0, EffectArgs(std::vector<int>{self, opp}),
                 &effect_1849_on_skill_hit);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 1849;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(1834, effect_1834_cap_reflect)
SKILL_EFFECT(1849, effect_1849_purge_boosts_then_amp)
