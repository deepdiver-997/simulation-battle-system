// K479 · 击败回满增益 + 击败回敌两族 —— 1436×5 + 536×5（2026-11-18，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k479，分支 feat/k479，基点 7968ed04；
// 场景 test/scenario/scenario_8660_k479.cpp，号段 8660-8669）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 ?mode=ro 现查 2026-11-18）══
//   1436（args_num=2）"当回合击败对手则{0}%恢复自身全部体力，未触发则全属性+{1}"
//     （analyze 全文同，绿色高亮=击败、蓝色高亮=当回合/未触发/全属性；尾注"（boss有效）"）
//   536（args_num=1）"若当次攻击击败对手，恢复对手体力上限1/{0}的体力"
//     （analyze 全文同，尾注"（boss有效）"；effect_des id=536 为 kind=6 规则概念
//     词条「临时体力上限」——**异表异空间**，非本效果语义，不构成对向反证）
//
// ══ 载体切片（unity 库 moves 列面 side_effect/side_effect_arg + effect_info
//   .args_num 顺序切分；side_effect.arg_count 兜底（效果 31 无 effect_info 行，
//   ac=2），2026-11-18 机验 5+5 载体全链零剩余；实现读行参不硬编码）══
//   1436 [{0}=回满概率%,{1}=全属性+量]：
//     32912 混沌·生死判 [1436,1218,1437]|[50,1,2,2]     → **[50,1]**（链 1218/1437）
//     33806 混沌·八荒逝 [1516,833,1436]|[3,50,1]        → **[50,1]**（链 1516/833）
//     34068 哀·岩崩地冲 [951,920,1436]|[1,50,50,1]      → **[50,1]**（链 951/920）
//     34834 回光赤箭雨 [31,942,1274,1436]|[4,8,3,35,22,50,1] → **[50,1]**（31 无模板 ac=2）
//     36775 翡翡聚星闪 [1284,1244,1436]|[1,100,200,300,400,70,1] → **[70,1]**（唯一 {0}=70 档）
//     ⚠️ {1} 全 1（无族内差分）；{0}∈{50,70} 两档——概率差分腿走 EffectParamOverride。
//   536 [{0}=回敌分母]：
//     15076 幽冥铁碎牙 [536]|[2]          → **[2]**（戴斯/戴斯威尔，纯链）
//     15971 最后遗言   [536]|[1]          → **[1]**（西姆/塔西姆，唯一回满档=复活满血）
//     16215 圣光创世箭 [471,536]|[2,2]    → **[2]**（链 471）
//     16266 箴言灭世咒 [593,536]|[0,4,20,2] → **[2]**（链 593）
//     31198 全能机械姬 [582,536]|[2,2]    → **[2]**（链 582「未击败则下2回合必暴」——
//       与 536 构成"没杀给暴击补偿/杀了给对手回血"的成套仁慈技，载体上下文互证）
//
// ══ 1436 口径定案（S1：encarlos 1040 结构双胞胎裁定，2026-11-18 本单三线证据）══
//   官方同句式先例 = **1040**「当回合若未击败对手则{0}%令对手{1}，未触发则令自身
//   全属性+{2}」（encarlos_skills.cpp:159-192，已收官 gated）：
//     · 门关（1040=已击败 / 镜像到 1436=未击败）→ **整段不结算**（连回退都不武装，
//       K474 1430 沿 1040 同款裁定"门关=整段不结算（含回退）"已收官 gated）；
//     · 门开 → 掷 {0}% → 主效果落地 / **未触发 → 回退补偿**。
//   「未触发」语义三证：
//     ① 官方要写"未击败"分支时用「未击败对手时」字面（1628「…未击败对手时令自身
//        下回合攻击技能先制+{1}」、719/920/823 同族）——1436 用「未触发」≠「未击败」；
//     ② 1040 头注明写"未触发=异常没有成功落上（免疫/抗性/反弹都算）"——**落地判据**
//        而非纯掷点判据；1436 的恢复落地 = heal 实际量 >0（heal_max_hp_fraction
//        返回 hp 差），掷歪/封回血钳零/满血钳零统一 = 未恢复成功 → 回退（K473 2230
//        "未恢复"判据 ws.last_heal_amount 内生面同哲学：全零档入回退无特例表）；
//     ③ 1551/916「未触发则恢复自身全部体力」同语法族：未触发 = 概率动作未成立。
//   → 三态：未击败（本回合无己方归因击败）→ 不掷点不增益；击败+恢复落地 → 回满
//     无增益（触发不回退）；击败+恢复未落地（掷歪/封回血/满血钳零）→ 全属性+{1}。
//   载体 kit 语境互证：1436 五载体链上 1218/920/833 全是"未击败→补偿"族
//   （先制/增伤/吸取），1436 的回退若也是"未击败补偿"则四条冗余；S1 下 kit 读作
//   "杀了→50/70%回满（没回成→全属性+1）；没杀→链上其它补偿"——成套不冗余。
//   ⚠️ 板行预检短语"（未击败才全属性）"与本裁定相反，按派发"语义验收须你重做"
//   指令以上述三线证据定案 S1；差异记档于完成报告（1024「当回合若未击败对手则
//   {0}%令对手{1}，未触发则令自身全属性+{2}」与 1436 的门向互补对偶关系另证）。
//
// ══ 触发时点与作用域（K453 漏斗/K464 窗1 watcher 直系）════════════════════
//   触发 = 官方击败漏斗（EVENT_OPPONENT_DEFEATED，battle_primitives.cpp:2177-2190
//   与 EVENT_DEATH 同点成对 emit；官方名词解释「击败」=「被击败时」之后时点，
//   K453/K464 同源）。watcher duration_rounds=1 = 武装动作的当回合活跃（"当回合"
//   面），次回合 deliver 窗口跳过 + cleanup 自动移除（生命周期全自动，不手写账本）。
//   归因过滤 = ev.actor == owner && ev.target == 1-owner（自毁 actor==target 天然
//   排除；无归因致死 actor=-1——异常/反伤直写路径——排除：异常跳死不算"当回合
//   击败对手"，漏斗归因口径 battle_primitives.cpp:2184 注释）。两族同构同判。
//   ⚠️ 归因面引擎事实（2026-11-18 本单实测入档）：**红伤落杀**才写击败归因
//   （deal_damage 非粉路径 :1121 写 last_damage_actor）；**粉伤/真伤击杀无归因**
//   （run_pink_damage :975-1051 / deal_true_damage :1163 均不写 last_damage_actor
//   → 击败事件 actor=-1 → 本族过滤不中）——"当次攻击击败"面天然只认红伤落杀，
//   与官方"攻击击败"语感一致。场景 T8663.c 载体链 1244 粉伤先手击杀腿据此以
//   对手压血 300 让红伤落杀规避（粉伤先削、红伤收头，归因归红伤）。
//   1436「当回合」vs 536「当次攻击」互斥口径甄别：窗=1 watcher 的归因击败覆盖面 =
//   武装回合内己方归因击败（含同趟攻击击败——事件晚 drain；536 五载体链上无额外
//   行动/多段生成器（471 免疫窗/593 固伤附加/582 必暴臂皆非），归因击败 ≡ 当次
//   攻击击败，精密化判据不引入（真链击杀归因=last_damage_actor 本单一跳直读）。
//   1436 同回合多杀（额外行动/链式）每次击败各掷各结算（once=false，K464 同款）。
//
// ══ 536 作用方向定案（复活类；引擎唯一合法出口 revive_pet）════════════════
//   「恢复对手体力上限1/{0}的体力」在击败门下 = **对手已被击败（登记死亡）后拉回
//   1/{0} 上限体力** = 复活（15971 {0}=1 = 回满复活"最后遗言"；4 载体 {0}=2 = 半血
//   复活；31198 与 582 的 kit 互证见上）。引擎法条：登记死亡宠 heal_impl 直接 no-op
//   （"★死亡裁决封闭…奶活不可逆转死亡…复活走 revive_pet 原语"，battle_primitives.cpp
//   heal_impl D2 注释 + battleContext.h:551"唯一合法出口是 revive_pet"）→ 本效果
//   走 revive_pet(对手侧, ev.slot, 对手上限/{0})（复位死亡登记，复活后可再死）。
//   ⚠️ 板行短语"对手回血沿 heal_amount 对向"按引擎法条修正为 revive_pet 对向：
//   heal_amount 对已登记死亡的对手结构性零恢复（假落地），revive_pet 才是复活
//   语义的正式原语（k_death_revenge_family 304 重生/j_defeat_family 复活同通道）。
//   封回血（HEAL_BLOCK）不拦复活：revive_pet 是复活出口不是 heal 管线成员
//   （304 重生同口径；官方复活类无封回血实测，按原语内生行为建模记档）。
//   时点：漏斗事件在死亡时点桶后 drain 投递（battleFsm.cpp:1322）——复活发生于
//   AFTER_DEFEATED 对账网/CHOOSE_AFTER_DEATH 补位输入**之前**；复活后对手 hp>0 →
//   handle_ChooseAfterDeath 安全网明文路径（"死亡被效果逆转…两边场上都活着 →
//   推进状态"，battleFsm.cpp:2292-2307——D5 明文允许的登记后复活取消补位）。
//   同归保护交互：击败方被亡语/反射同波击倒 → 存活门早退（K453 T8413「官方
//   『击败』=未死者触发」同判；内核同归保护保 1 血存活者照常复活对手）。
//   全灭终局交互：漏斗尾 evaluate_battle_end（battleFsm.cpp:139 附近）先于 drain，
//   全灭先立 finish_flag → 本复活照常结算但**终局不回滚**（D5「重生之翼实测口径：
//   动画计数跳动了一瞬」，runInternal D5 检查点注释同文）。
//
// ══ 生命周期 ══════════════════════════════════════════════════════════════
//   per-pet on_stage_storage 存 watcher 句柄/武装槽（K453/K464 同款重武装卫生：
//   重施放先 remove_watcher 旧 id 防双 watcher）；watcher ON_STAGE scope + 显式
//   valid_id（缺省 0 永不匹配——K453 :165 坑）；窗=1 → 换宠/下场由窗口+epoch
//   双保险作废，无需额外清理（K464 同款；离场清理=窗口过期+epoch 作废结构性成立）。
//
// ══ 行参在位即消费（K314/K340 两犯纪律）═══════════════════════════════════
//   1436：{0}=概率%：>=0 合法（显式 0 = 关闸档：恢复永不落地 → 每次击败全走
//     回退分支，**不拒**——roll_percent(<=0) 结构性 false）；<0 无定义拒。
//     {1}=全属性+量：>=0 合法（显式 0 = 关闸档：回退分支 no-op，**不拒**）。
//   536：{0}=分母：>0 才有定义（1/{0}），<=0 拒。max_hp/{0} 整除 floor；
//     max_hp<{0} 时商 0 → revive_pet 拒 hp<=0 → 保持死亡（字面"恢复 0 点体力"）。
//
// ══ 动工前置三查（2026-11-18 本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep 1436/536（src/ include/）零行为命中（536 惟二命中 = skills.cpp:246
//     注释内载体技能 id 38536 串位 / abnormal-types.h:291 引 effect_des kind=6
//     词条——均为非行为面）；moves 空间两 id 无路由无硬编码，SKILL_EFFECT default 桶。
//   ② effect_info 1436/536 行即模板（args_num=2/1，无同文重复族——1436 与 672
//     差概率+回退子句、与 1040 门向互补、与 1628 差回退字面，均为异文近邻已核）；
//     side_effect 表 536 行 ac=1；effect_des kind=6 id=536 词条与 moves 536 异空间
//     不冲突；载体 10 条顺序切分零剩余（31 无模板用 ac=2 兜底）。
//   ③ registry（build/registry.json 基线门产，moves=1578/soul=415/suit=4，K473
//     合入后）无 1436/536；本库 resources/test grep SKILL_EFFECT(1436/536、
//     SOUL_PROGRAM(1436/536 零命中；全部并行 worktree（k478/k480/agents/
//     endgame-walk/web-replay）同 grep 零命中——真缺口。
//   双空间甄别（K458/K465 流程）：effect_icon 1120 行 effect_id=1436（宠 3847
//     魂帝·天循道魂印，tips=吸取/反弹/增伤条件印语义）**未注册**（registry soul
//     无 1436）→ 双空间共存无运行时键段冲突，moves 侧占 1014360000 基址，将来
//     soul 空间实现按 K465 先例取 1014360100 子块；536 无 effect_icon 行（soul
//     空间 536 不存在）→ 单基址免拆分。custom_effect_overrides/custom_effect_
//     programs 两 id 零行；tasklog 8660 段为板行预留、无 900000 拆分迁移与故意
//     退役记录（db_patches 全目录两 id 零命中；效果id撞号审计-2026-10-22.md 无行）。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456/K464
//   键式；1014360/1005360 段全库 grep 零占用 2026-11-18，check_storage_keys.py
//   门禁兜底）══
//   1436（基 1014360000）：slot0=击败回调到达计数（归因过滤后）、1=末次掷点结果
//     （1=掷中/0=掷歪/-1=未掷）、2=末次恢复落地量（heal_max_hp_fraction 返回 hp 差）、
//     3=末次回退 boost 值（{1} 或 0）、4=武装旗、5=跳过原因（0=无/1=自身 hp<=0/
//     2=槽绑定失败）、6={0} 回显、7={1} 回显
//   536（基 1005360000）：slot0=击败回调到达计数、1=末次复活量（对手上限/{0}）、
//     2=末次复活后对手 hp、3=武装旗、4=跳过原因（0=无/1=击败方 hp<=0/2=槽绑定
//     失败/3=复活量<=0 不复位）、5={0} 回显
//   布局自防撞：单基址 per effect + owner×16 步长 > 槽位上限 7 → owner 间无交叠；
//   与 K456(1013690/1011960)/K457(1014590/1017990)/K458(1015650/1014940)/
//   K462(1015750/1018550)/K464(1021660/1016690)/K467(1020670/1021280)/
//   K468(1014570/1001230)/K471(1022310/1014160)/K473(1022300/1015590)/
//   K474(1014300/1011500 系) 已落键段零交叠；跨文件撞键由 check_storage_keys.py 扫。
//
// ══ 变异测试实测（2026-11-18，本 worktree；证据 build/k479-mutations/M*/，
//   每份含 apply.diff+build.log+run.log+run.rc+first-kill+dylib-sha-mutated/
//   restored+restore-build.log+rerun.log+rerun.rc；底本 dylib SHA
//   a0d6d88db3e308a3…；每次**删 .o 强编**（macOS Make 3.81 mtime 整数秒截断
//   ——K468 记档），变异 dylib SHA 均≠底本、还原（备份拷贝字节级）后
//   SHA==底本再复绿 rc=0/SBS_SCENARIO_CHECKS=196）══
//   M1 通道·回满量折半（denom 1→2）→ T8660.a hp=50099≠99999 红；
//   M2 回退分支·落地判据翻转（掷中也回退）→ T8660.a levels=1≠0 红；
//   M3 门·1436 归因过滤删除 → cb=2≠1 红（对手侧击败事件也接）；
//   M4 S1 特征·武装即无条件全属性 → levels=1≠0 红（门关整段不结算锁死）；
//   M5 概率·0% 关闸档翻转（0 必掷歪→必掷中）→ T8660.b hp=99999≠100 红；
//   M6 键位·owner 步长归零（两侧撞键）→ T8662.c 镜像腿 armed 侧读 0 红；
//   M7 生命周期·536 窗 1→2（跨回合携带）→ T8666.a cb=2≠1 红
//     （M7-v1 初版"重武装不摘旧 watcher"实测**假绿**记档：窗=1 watcher 次回合
//     deliver 窗口跳过=结构惰性，摘旧卫生对本族不可观测——K453 duration=0 面
//     才有双投风险，窗=1 族摘旧仅为卫生非行为面，记档免后人再试）；
//   M8 方向·复活对向翻转（复活自己）→ T8664.a on_stage[1]=1≠0 红（补位发生）；
//   M9 存活门·536 门删除 → T8665.b skip=3≠1 红（亡语击倒者仍复活）；
//   M10 复活量·分母丢弃（半血→回满）→ T8664.b hp=99999≠49999 红。
//   10 变异真红一杀全档、9 类变异面（通道/回退分支/门/S1 特征/概率/键位/
//   生命周期/方向/存活门/量）。
//
// 先例：K464 2166（resources/moves_lib/k464_defeat_restore_allboost.cpp：漏斗
//   watcher/存活门/valid_id/重武装/槽绑定/heal_max_hp_fraction/all_stats_up 全套
//   结构）、K453 2456（k453_defeat_restore.cpp：漏斗原件+亡语同波存活门）、
//   encarlos 1040（encarlos_skills.cpp:159-192：同句式门关整段不结算+未触发回退
//   落地判据）、K474 1430（门关整段不结算沿 1040 裁定收官件）、K473 2230
//   （ws.last_heal_amount 内生面/全零档入回退哲学）、k_death_revenge_family 304
//   （revive_pet 复活原语通道）、j_amp_foe_heal 1883（对手向恢复对向先例）。
// 场景回归：test/scenario/scenario_8660_k479.cpp（8660 号段）。

#include "common.h"

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1436AuditBase = 1014360000;   // 10^9 + 1436×10^4
constexpr int k536AuditBase  = 1005360000;   // 10^9 + 536×10^4

// on_stage_storage 键段（per-pet 本场上场期；本文件私有）：
//   9901436xx / 990536xx = 99 + 效果号系（K453 9902456xx 同构）。
constexpr int k1436WatcherIdKey = 990143601;  // 活跃 watcher id（重武装先摘旧）
constexpr int k1436ArmedSlotKey = 990143602;  // 武装时在场槽位（槽绑定双保险）
constexpr int k536WatcherIdKey  = 990053601;
constexpr int k536ArmedSlotKey  = 990053602;

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

// ══════════════════════════════════════════════════════════════════════════
// ── 1436 —— 当回合击败对手则{0}%恢复自身全部体力，未触发则全属性+{1} ──
// int_args = [owner, target, {0}=概率%, {1}=全属性+量]（args_num=2 + 前缀）。
// 口径：S1（encarlos 1040 结构双胞胎裁定，文件头「1436 口径定案」节）。
// ══════════════════════════════════════════════════════════════════════════
EffectResult effect_1436_defeat_fullheal_or_boost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1436, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1436, args,
                     "1436 布局=[owner,target,概率%,全属性+量]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int boost = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1436, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct >= 0, 1436, args,
                     "需要 概率% {0} >= 0（显式 0 = 关闸档：恢复永不落地→每次击败走回退，在位即消费不拒）");
    EFFECT_ARG_GUARD(boost >= 0, 1436, args,
                     "需要 全属性+量 {1} >= 0（显式 0 = 关闸档：回退分支 no-op，在位即消费不拒）");
    if (!g_core) {
        return EffectResult::kOk;
    }

    const int armed_slot = ctx->on_stage[owner];
    ElfPet& pet = ctx->getPet(owner);

    // 重武装先摘旧 watcher（窗=1 下同回合双投不可达，摘旧是 K453/K464 同款卫生）
    const int old_watcher = pet_storage_int(pet, k1436WatcherIdKey);
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }
    pet.on_stage_storage[k1436ArmedSlotKey] = armed_slot;

    // 事件投递按 type 不按 owner → 回调闭包捕获武装方 side 自过滤
    //（K464 同构；K1 语义：emit 入队晚 drain，回调读 ev.* 载荷不读现场）。
    auto on_defeated = [owner, pct, boost](BattleContext* c, const BattleEvent& ev) {
        if (!c || ev.type != EventType::EVENT_OPPONENT_DEFEATED) {
            return;
        }
        if (ev.actor != owner || ev.target != 1 - owner) {
            return;
        }
        set_storage_int_local(c, audit_key_of(k1436AuditBase, owner, 0),
                              storage_int_local(c, audit_key_of(k1436AuditBase, owner, 0)) + 1);
        ElfPet& armed = c->getPet(owner);
        // 存活门（K453 T8413 亡语同波先例）：DEATH 回调在 drain 同波先击倒击败方时，
        // 本回调触发时 hp=0 且未登记 → 官方「击败」=未死者触发，直接早退。
        // 同归路径由内核同归保护保留 1 血 → 门不误伤（K453 T8410/K464 T8514）。
        if (armed.hp <= 0) {
            set_storage_int_local(c, audit_key_of(k1436AuditBase, owner, 5), 1);
            return;
        }
        // 槽绑定：只有"武装宠本尊在场时达成的击败"才结算（换宠不串权）
        const int slot_key = pet_storage_int(armed, k1436ArmedSlotKey, -1);
        if (slot_key < 0 || slot_key > 5 || c->on_stage[owner] != slot_key) {
            set_storage_int_local(c, audit_key_of(k1436AuditBase, owner, 5), 2);
            return;
        }

        // 掷 {0}%（roll_percent：<=0 结构性 false = 关闸档在位消费；>=100 短路 true）
        const int rolled = roll_skill_percent(*c, 1436, 0, owner, owner, pct) ? 1 : 0;
        set_storage_int_local(c, audit_key_of(k1436AuditBase, owner, 1), rolled);

        // ① 恢复自身全部体力：掷中才尝试（{0}% 门闸恢复动作本身）——
        //   heal_max_hp_fraction(denom=1) 缺口喂 heal_amount 正式原语（吃 HEAL_BLOCK
        //   封回血 + 恢复%修正；返回实际落地量 = hp 前后差）。掷歪 = 无尝试 → 落地 0。
        const int healed = rolled ? heal_max_hp_fraction(c, owner, /*denom=*/1) : 0;
        set_storage_int_local(c, audit_key_of(k1436AuditBase, owner, 2), healed);

        // ② 未触发（恢复未落地：掷歪 / 封回血钳零 / 满血钳零统一——S1 落地判据，
        //   1040「未触发=没成功落上」+ K473「全零档入回退无特例表」双先例）→
        //   全属性+{1}：六槽逐项 stat_change 真写（本体/视图同步、逐项事件，
        //   K464 2166 同款 all_stats_up 真写口径）。
        int boosted = 0;
        if (healed <= 0) {
            if (boost != 0) {
                all_stats_up(c, owner, boost);
            }
            boosted = boost;
        }
        set_storage_int_local(c, audit_key_of(k1436AuditBase, owner, 3), boosted);
    };

    // 窗=1：武装动作的当回合活跃；次回合 deliver 窗口跳过 + cleanup 自动移除
    //（生命周期全自动——「当回合击败对手」面=武装回合内归因击败，不跨回合携带）。
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_OPPONENT_DEFEATED, owner, ctx->roundCount,
        /*duration_rounds=*/1, /*once=*/false, std::move(on_defeated),
        WatcherScope::ON_STAGE,
        /*valid_id=*/ctx->watcher_valid_id[owner]);   // 必传当前 epoch——缺省 0 永不匹配
    pet.on_stage_storage[k1436WatcherIdKey] = watcher_id;
    set_storage_int_local(ctx, audit_key_of(k1436AuditBase, owner, 4), 1);
    set_storage_int_local(ctx, audit_key_of(k1436AuditBase, owner, 6), pct);
    set_storage_int_local(ctx, audit_key_of(k1436AuditBase, owner, 7), boost);
    return EffectResult::kOk;
}

// ══════════════════════════════════════════════════════════════════════════
// ── 536 —— 若当次攻击击败对手，恢复对手体力上限1/{0}的体力 ──
// int_args = [owner, target, {0}=回敌分母]（args_num=1 + 前缀）。
// 方向：复活对手（击败门下"恢复"已登记死亡者 → revive_pet 唯一合法出口，
// 文件头「536 作用方向定案」节）。
// ══════════════════════════════════════════════════════════════════════════
EffectResult effect_536_kill_restore_foe(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 536, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 536, args,
                     "536 布局=[owner,target,回敌分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     536, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 536, args, "需要 分母 {0} > 0（1/{0} 才有定义）");
    if (!g_core || !g_core->revive_pet) {
        return EffectResult::kOk;
    }

    const int armed_slot = ctx->on_stage[owner];
    ElfPet& pet = ctx->getPet(owner);

    // 重武装先摘旧 watcher（K453/K464 同款卫生）
    const int old_watcher = pet_storage_int(pet, k536WatcherIdKey);
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }
    pet.on_stage_storage[k536ArmedSlotKey] = armed_slot;

    auto on_defeated = [owner, denom](BattleContext* c, const BattleEvent& ev) {
        if (!c || ev.type != EventType::EVENT_OPPONENT_DEFEATED) {
            return;
        }
        if (ev.actor != owner || ev.target != 1 - owner) {
            return;
        }
        set_storage_int_local(c, audit_key_of(k536AuditBase, owner, 0),
                              storage_int_local(c, audit_key_of(k536AuditBase, owner, 0)) + 1);
        // 存活门（K453 T8413 同判）：击败方被亡语同波击倒 → 官方「击败」=未死者触发
        ElfPet& armed = c->getPet(owner);
        if (armed.hp <= 0) {
            set_storage_int_local(c, audit_key_of(k536AuditBase, owner, 4), 1);
            return;
        }
        // 槽绑定（武装宠在场）+ 在场槽一致（被击败宠尚未被补位换下——复活必须
        // 作用于仍占场槽的被击败宠；补位后不追溯）
        const int slot_key = pet_storage_int(armed, k536ArmedSlotKey, -1);
        if (slot_key < 0 || slot_key > 5 || c->on_stage[owner] != slot_key) {
            set_storage_int_local(c, audit_key_of(k536AuditBase, owner, 4), 2);
            return;
        }
        const int foe = 1 - owner;
        const int foe_slot = ev.slot;
        if (foe_slot < 0 || foe_slot > 5 || c->on_stage[foe] != foe_slot) {
            set_storage_int_local(c, audit_key_of(k536AuditBase, owner, 4), 2);
            return;
        }

        // 恢复对手体力上限 1/{0} 的体力：登记死亡宠唯一合法出口 revive_pet
        //（heal_impl 死亡裁决封闭 no-op；revive_pet 复位登记 + 写 hp，304 重生同通道）。
        const int max_hp = max_hp_of(c, foe);
        const int amount = max_hp / denom;
        if (amount <= 0) {
            // max_hp < {0}：整除商 0 = 字面"恢复 0 点体力"→ 不复活保持死亡
            //（revive_pet 拒 hp<=0；非钳制，字面语义）。
            set_storage_int_local(c, audit_key_of(k536AuditBase, owner, 4), 3);
            return;
        }
        const int revived = g_core->revive_pet(c, foe, foe_slot, amount);
        set_storage_int_local(c, audit_key_of(k536AuditBase, owner, 1), amount);
        set_storage_int_local(c, audit_key_of(k536AuditBase, owner, 2),
                              c->seerRobot[foe].elfPets[foe_slot].hp);
        if (revived < 0) {
            // 消逝不可逆（revive_pet 内生拒）等：记档不重试
            set_storage_int_local(c, audit_key_of(k536AuditBase, owner, 4), 3);
        }
    };

    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_OPPONENT_DEFEATED, owner, ctx->roundCount,
        /*duration_rounds=*/1, /*once=*/false, std::move(on_defeated),
        WatcherScope::ON_STAGE,
        /*valid_id=*/ctx->watcher_valid_id[owner]);
    pet.on_stage_storage[k536WatcherIdKey] = watcher_id;
    set_storage_int_local(ctx, audit_key_of(k536AuditBase, owner, 3), 1);
    set_storage_int_local(ctx, audit_key_of(k536AuditBase, owner, 5), denom);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1436, effect_1436_defeat_fullheal_or_boost)
SKILL_EFFECT(536, effect_536_kill_restore_foe)
