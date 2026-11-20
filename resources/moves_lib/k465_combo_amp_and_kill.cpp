// K465 · 多段攻击两族 —— 1863×7 + 1627×7（2026-11-13，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k465，分支 feat/k465；
// 场景 test/scenario/scenario_8520_k465.cpp，号段 8520-8529，基点 e5c341b1）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-13）══
//   1863（args_num=2）"1回合做{0}次攻击，自身每有1项技能PP值不为满则连击次数+{1}"
//   1627（args_num=3）"{0}回合做{1}-{2}次攻击，若本回合攻击次数达到最大则必定秒杀对手"
//
// ══ 载体切片（unity 库 json_each + side_effect.arg_count 优先顺序切分，
//   14 载体全链零剩余，脚本核算 2026-11-13；场景 T8520-pre 以实际加载器
//   load_skill_effects 逐载体锁定）══
//   1863 [{0}=基数,{1}=加数]（7 载体）：
//     36147 辉·沧天逆流拳 [763(6),1863(2),1737(1),1156(0)]=[1×6|**7,7**|40|] 、
//     36684 呼朋引伴 [1863(2)]=**[2,1]**（**纯链单效果** cat2 特50 acc95 prio0
//       ——行为面主载体；monster 4520 扎里里 learnable）、
//     37685 毋视之恩怀 [1863(2),1355(1),732(6)]=**[5,3]**|1|1×6、
//     37877 多菱光 [1114(1),1863(2)]=[1|**2,2**]（1114 扣 PP 兄弟——切片核验用，
//       行为面不选）、
//     38059 旧迹觅遗枝 [1681(2),1863(2),134(2)]=[1,300|**10,1**|300,1]、
//     38267 慰心 [1863(2),1037(2),1740(1)]=**[10,1]**|1,50|80、
//     38458 静籁染潮 [881(0),1232(0),1863(2)]=||**[10,1]**。
//   1627 [{0}=回合,{1}=下限,{2}=上限]（7 载体）：
//     34662 夜怨缠身 [1022(1),1627(3)]=[300|**1,1,10**]、
//     35526 万念俱寂 [1627(3),1249(1)]=**[1,1,10]**|50、
//     36178 裂地突袭 [1627(3),1868(4)]=**[1,1,20]**|30,9,50,6、
//     36465 合十 [1254(0),978(0),1627(3)]=||**[1,5,10]**（唯一 {1}=5——区间下限档
//       验证载体；prio=3 先制污染，断言不涉先手故可用）、
//     36936 黑暗之触 [1627(3)]=**[1,1,8]**（**纯链单效果** cat1 物85 acc95 prio0
//       ——行为面主载体；monster 4571 帕斯裟帝 learnable）、
//     37064 超感粒子 [1627(3),1128(1)]=**[1,1,10]**|1、
//     38016 异怪波动 [616(2),1627(3)]=[3,2|**1,7,15**]。
//
// ══ 实现口径（三查结论 2026-11-13，本 worktree + 主库 file URI ?mode=ro）══
//   ① 内核 grep 1863/1627（src/ include/ 全量裸 id）零行为命中（resources 命中均为
//     注释记档：K433/K434/K437/K440/K442/K444/K447 的"7 载体遗留"候选清单、
//     f_misc_family "动态加数"未覆盖清单、k377 的 effect_icon 空间 icon1627
//     ——均非注册）；registry 无两 id。
//   ② effect_info 两 id 在列 args_num=2/3，与 side_effect.arg_count 全等；
//     boss_invalid_effects.json 含 1627（line 740）/不含 1863——该文件
//     namespace=effect_icon 尾注证据索引（1141/2456 同判例），场景框架不走 boss
//     预填路径，记档不处理。
//   ③ 全库并集无注册：resources/（moves_lib+soul_lib）grep SKILL_EFFECT(
//     SOUL_PROGRAM( / kMark1863/kMark1627 零命中；兄弟 worktree（k466/k467/k468/
//     endgame-walk/web-replay/agents）精确 grep 零命中——真缺口。
//     **soul 空间同号甄别**（看板"两族键段共存"项）：官方魂印数据 1863=月灵·陌羽
//     （4277，row1501）、1627=森莎（4034，row1290）均**未实现**（soul_lib 零命中），
//     但双空间同号在档 → 审计键按 K458（soul 1565=K407）/K461（soul 1638=K404）
//     **子块拆分先例**取 +0100 子块（1018630100/1016270100），给将来 soul 实现
//     留出基段 1018630000/1016270000。
//   历史记档甄别（工单"确认无拆分迁移与退役记录"项）：grep tools/db_patches 两 id
//     零命中（无 900000 私有段拆分）；无任何"跳过/退役"记档——K433 头注将 1627/
//     1863 记为"逐一体量评估"的遗留候选，属"待立单"而非退役。
//
// ══ 1863 实现口径（通道裁定：内核连击通道，非额外行动）══
//   · 派发"多段攻击沿既有额外行动原语"按本单独立三查修正：本族"1回合做N次攻击/
//     连击次数"在引擎的既有通道 = **内核连击**（J326 custom_skill_combo_rules +
//     roll_combo_count + "一次公式×N"，官方 L101 口径）——"做{0}~{1}次攻击" 16 模板
//     全部走此通道（1141/1172/1454/1455/1500/1546/1577/1593/1608-1610/1627/1666/
//     1685/1732/31），零先例用额外行动建模多段攻击（额外行动=整段新行动，
//     非"一次公式×N"，语义不符）。1863 的模板词"连击次数+{1}"与官方连击公式
//     逐字对应 → 走 **ws.combo_count_resolver**（battleWorkspace.h:481，
//     stage_simple_attack_damage 第一次公式前咨询、materialize 写回、用后即焚）。
//     1863 无规则行（动态加数族），静态 roll 退化 1~1（**不消耗 sbs_rand**，
//     skills.h roll_combo_count 短路档）→ resolver 返回完整总数 {0}+非满项×{1}，
//     与 484（基数+已命中×步长）/1685（条件上限）同构（j_combo_face.cpp 直系）。
//   · PP 非满项数读面（SKILL_EFFECT 时点安装 resolver 时计一次，闭包捕获）：
//     自身 5 槽逐槽 `pp >= 0 && maxPP > 0 && pp < maxPP`（pp==-1 无限槽=恒满不计、
//     maxPP<=0 空位垫底槽不计）；载体自身本回合 PP 消耗在 AFTER_ACTION
//     （battleFsm.cpp:432 consume_selected_skill_pp），晚于安装时点 → 本回合
//     自槽恒满，计数只反映施放前的真实面。
//   · {0}/{1} 行参守卫：{0}>0（"做0次攻击"官方无此档，EFFECT_ARG_GUARD 响亮拒）；
//     {1}>=0 在位即消费（"+0"=无加数关闸档合法，K314/K340 >0 守卫教训）。
//   · 解析值=1 档（理论：{0}=1 且全满）：内核不置 rewritten → 与普通单击等价
//     （battleFsm.cpp:745 注释）——真实载体 {0}≥2 不落此档，记档。
//
// ══ 1627 实现口径（次数腿内核已有 + 秒杀门插件腿）══
//   · 次数腿 **不实现**：J326 起规则表已有行 (1627,1,2)（tools/db_patches/j4/
//     1608_combo_rules.sql："{0}回合做{1}-{2}次攻击，达上限必定秒杀（秒杀子句
//     未实现，区间照取；原 case 1627）"）→ 内核每次使用掷一次 N∈[{1},{2}] 写
//     combo_view（resolve_skill_execution 物化，SKILL_EFFECT 时点已可见）；
//     {0}=回合描述位不消费（1172 同形同判——规则行 (1172,1,2) 同样只取 {1}{2}）。
//   · 秒杀门 = **AFTER_ACTION 延迟器**（J23 36/J130 691 直系：SKILL_EFFECT 只挂
//     FIRST/SECOND AFTER_ACTION 双桶一次性 probe（CE duration=1 + lr=0 = 一次性
//     动作在途节点，不计回合类、不被断回合；arm_round 回合绑防跨回合引爆），
//     桶按 (state, owner) 配对只跑行动方那条）。伤害落地后判秒杀，避免"先秒后伤"
//     乱序（force_hp_to_zero 时点语义：瞬杀归零在红伤结算完之后，
//     battle_primitives.h:757 用户 2026-09-15 实测口径）。
//   · probe 恒注册（SKILL_EFFECT 每有效趟都挂，达上限与否的**唯一行为门在 probe
//     内**——不设注册面二次预判：双门冗余会被单点变异穿透，2026-11-13 权威变异轮
//     M5a 实证（仅 relax probe 门时注册门兜住不红）后收敛单门；未达上限趟在 probe
//     门闭并记 slot5=0，负测面有 probe 侧可观测凭证）。N 快照经 probe 行参带入
//     （与 SKILL_EFFECT 时点 combo_view 读数同源——combo_view.consume() 只清
//     rewritten 标记不清值，battleWorkspace.h:93）。
//   · 门判据五条（probe 内依序）：①arm_round==roundCount（一次性节点回合绑）；
//     ②行动方=self（桶按 (state, owner) 配对，行动方那条跑）；③命中门
//     ws.skill_exec_result[self]==HIT（miss/被封趟 HIT 分支本就不注册，与 691
//     "命中后"门同源——62/136 族 miss 补偿是 SKILL_INVALID 单桶白名单）；
//     ④存活门 对手 hp>0（已被本次攻击打死则无需再秒，2295 存活门；TARGET_DOWN
//     同判）；⑤**达上限门 N >= {2}**（"达到最大"）。
//   · 秒杀走 **force_hp_to_zero 原语**（秒杀族唯一入口：免疫票不拦但秒杀短路/
//     转化在原语内统一处理 + EVENT_HP_TO_ZERO 必发，battle_primitives.h:731），
//     不直写 hp；结果码（HpZeroResult）落审计槽。
//
// ══ 生命周期/清理 ══
//   · 两族均**无跨回合持久票**：1863 resolver 用后即焚（battleFsm.cpp:740 置空）
//     + ws 每回合 reset 兜底清（battleWorkspace.h:158）；1627 probe duration=1
//     一次性节点（K464 验收口径：window=1 时点桶 + 次回合惰性清理）——"离场清理"
//     结构性成立（无账本可残留），场景 T8520.g/T8520.q 锁"次回合无残留效应"
//     （T8520.h/p 锁 miss 趟零注册面）。
//   · 镜像：1863 resolver/probe 均挂施放方 owner 槽，先/后手 symmetric（桶按
//     (state, owner) 配对）；1627 双桶注册行动方那条跑——T8520.f/T8520.n 后手档。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + **0100 子块** + owner×16 + slot；
//   1018630100/1016270100 段全库 grep 零占用 2026-11-13，含兄弟 worktree）══
//   1863 slot0={0}回显 1={1}回显 2=末次解析PP非满项数 3=末次解析总连击数
//     （=resolver 返回值） 4=累计安装次数 5=末次PP非满槽位掩码(bit i=槽i) 6=末次解析回合
//   1627 slot0={0}回合回显 1={1}下限回显 2={2}上限回显 3=末次注册时 N（combo_view
//     物化值） 4=armed 旗标（1=N>=上限） 5=末次 probe 门判（1=发起秒杀/0=门闭）
//     6=末次秒杀结果码（HpZeroResult int：0 EXECUTED/1 CONVERTED/2 TARGET_DOWN/
//     3 INVALID_PARAM；0 默认=未发起） 7=累计秒杀发起数 8=累计注册数
//   布局自防撞：单基址 per effect + owner×16 步长 > 槽位上限 8 → 同基址内 owner
//   间无交叠；写键全走本文件 set_audit/get_audit helper（K460 封装纪律），跨文件
//   撞键由 tools/check_storage_keys.py 门禁静态扫。
//
// ══ 变异测试（权威轮 build/k465-mutations/，底本 SHA 见其 baseline.sha，
//   本仓 dylib 构建确定性同 K459 实验：同源重编同 SHA、还原后逐字节可比）══
//   权威轮 8/8 真红（rc=134 + 首杀特征符合）→ 还原 SHA==底本 → deploy 复绿
//   （183 断言 rc=0）：
//   M1 通道（1863 resolver 不安装→静态退化单击）→ 首杀 T8520.a combo_view=1≠2；
//   M2 计数门反转（PP 非满判据取反→满槽计入）→ 首杀 T8520.a combo_view=7≠2；
//   M3 键位（audit_key_of 丢 owner 维度→双侧串账）→ 首杀 T8520.f 后手回显=0；
//   M4 边界语义（pp=-1 无限槽计入非满项数）→ 首杀 T8520.e combo_view=4≠3；
//   M5 秒杀门差一（达上限判据 n>=hi→n>=hi-1）→ 首杀 T8520.j probe 门判=1≠0；
//   M6 存活门移除（红伤先杀趟照发原语）→ 首杀 T8520.o probe 门判=1≠0；
//   M7 目标置换（force_hp_to_zero(opp,self)→(self,opp)）→ 首杀 T8520.k 对手
//     hp=98495≠0（误杀自己，红伤面存活）；
//   M8 注册缺席（SKILL_EFFECT(1627→1628)）→ 首杀 T8520.i N 回显=0≠1。
//   变异史（首轮探索，2026-11-13）：初版 1627 为"注册面预判 + probe 二次判"双门
//   冗余，M5a（仅 relax probe 门）与 M5b（仅 relax 注册门）双双存活（另一门兜住）
//   ——设计缺陷被变异测试实证后**收敛单门**（probe 恒注册、唯一门在 probe 内，
//   见 1627 实现口径节），权威轮 M5 即真红。教训入档：门语义的冗余双写是变异
//   测试假绿温床，行为门只留一处。
//   概率面 N/A（两族无概率行参无掷点通道——1863 计数确定、1627 N 掷点在内核
//   规则行非本插件）；once 面 N/A（probe duration=1 一次性由 M5/M6 门面覆盖，
//   跨回合残留由 M8 注册面+T8520.q 行为面双重锁定）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── 审计键基址（soul 空间同号 → +0100 子块拆分，见头注）──
constexpr int k1863AuditBase = 1018630100;   // 10^9 + 1863×10^4 + 0100 子块
constexpr int k1627AuditBase = 1016270100;   // 10^9 + 1627×10^4 + 0100 子块

inline int audit_key_of(int base, int owner, int slot) { return base + owner * 16 + slot; }

int get_audit(const BattleContext* ctx, int base, int owner, int slot, int def = 0) {
    const auto it = ctx->plugin_storage.find(audit_key_of(base, owner, slot));
    return it == ctx->plugin_storage.end() ? def : any_int_or(it->second, def);
}

void set_audit(BattleContext* ctx, int base, int owner, int slot, int value) {
    ctx->plugin_storage[audit_key_of(base, owner, slot)] = value;
}

void add_audit(BattleContext* ctx, int base, int owner, int slot, int delta) {
    set_audit(ctx, base, owner, slot, get_audit(ctx, base, owner, slot) + delta);
}

// ════════════ 1863 —— 1回合做{0}次攻击，每有1项PP非满连击+{1} ════════════

// PP 非满项数读面：自身 5 槽逐槽（pp==-1 无限=恒满不计；maxPP<=0 空位不计）。
// 返回项数，mask 回填非满槽位掩码（bit i = 槽 i）。
int count_pp_not_full(const ElfPet& pet, int& mask) {
    int count = 0;
    mask = 0;
    for (int i = 0; i < static_cast<int>(pet.skills.size()); ++i) {
        const Skills& s = pet.skills[static_cast<std::size_t>(i)];
        if (s.pp >= 0 && s.maxPP > 0 && s.pp < s.maxPP) {
            ++count;
            mask |= (1 << i);
        }
    }
    return count;
}

// 1863 主体：SKILL_EFFECT 时点计数 + 安装 combo_count_resolver（484/1685 直系）。
// 行参 [owner, target, {0}=基数, {1}=加数]（args_num=2 + [owner,target] 前缀）。
EffectResult effect_1863_pp_amp_combo(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1863, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1863, args,
                     "1863 布局=[owner,target,基数,加数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int base = args.int_args[2];
    const int bonus = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1863, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(base > 0, 1863, args, "需要 {0}基数 > 0");
    EFFECT_ARG_GUARD(bonus >= 0, 1863, args, "需要 {1}加数 >= 0（+0 关闸档合法）");

    int mask = 0;
    const int not_full = count_pp_not_full(ctx->getPet(owner), mask);
    const int total = base + not_full * bonus;

    set_audit(ctx, k1863AuditBase, owner, 0, base);
    set_audit(ctx, k1863AuditBase, owner, 1, bonus);
    set_audit(ctx, k1863AuditBase, owner, 2, not_full);
    set_audit(ctx, k1863AuditBase, owner, 3, total);
    set_audit(ctx, k1863AuditBase, owner, 5, mask);
    set_audit(ctx, k1863AuditBase, owner, 6, ctx->roundCount);
    add_audit(ctx, k1863AuditBase, owner, 4, 1);

    // 内核连击通道：第一次公式前咨询一次、materialize 写回 combo_view、用后即焚。
    ctx->ws.combo_count_resolver[owner] = [total](BattleContext*) { return total; };
    return EffectResult::kOk;
}

// ════════════ 1627 —— {0}回合做{1}-{2}次攻击，达上限必定秒杀 ════════════

// AFTER_ACTION probe：伤害已落地（force_hp_to_zero 时点语义），达上限 → 秒杀。
// 行参 [self, opp, 上限, arm_round, N快照]（5 参）。
EffectResult effect_1627_max_kill_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core
        || !g_core->force_hp_to_zero) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int hi = args.int_args[2];
    const int arm_round = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp) {
        return EffectResult::kOk;
    }
    // ① 回合绑：一次性节点（duration=1），跨回合残留防御性早退。
    if (arm_round != ctx->roundCount) {
        return EffectResult::kOk;
    }
    // ② 行动方门：桶按 (state, owner) 配对，行动方那条才跑（双注册的另一半空转）。
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    int acting = -1;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            acting = first;
            break;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            acting = 1 - first;
            break;
        default:
            acting = -1;
            break;
    }
    if (acting != self) {
        return EffectResult::kOk;
    }
    // ③ 命中门：miss/被封趟本就不注册 HIT 分支（本 probe 只在有效趟存在），
    //    此门 = 691/495 秒杀族同源语义面 + 防御性复判。
    if (ctx->ws.skill_exec_result[self] != SkillExecResult::HIT) {
        set_audit(ctx, k1627AuditBase, self, 5, 0);
        return EffectResult::kOk;
    }
    // ④ 存活门：已被本次攻击打死则无需再秒（2295 同款；TARGET_DOWN 同判）。
    if (ctx->getPet(opp).hp <= 0) {
        set_audit(ctx, k1627AuditBase, self, 5, 0);
        return EffectResult::kOk;
    }
    // 达上限判据 = 注册时快照 N >= {2}（行参带入，与 combo_view 物化值同源——
    // **唯一行为门**：probe 恒注册，未达上限趟在此门闭并记 slot5=0（负测观测面），
    // 不做注册面二次预判（双门冗余会被单点变异穿透，2026-11-13 变异实证后收敛单门））。
    // 达标 → force_hp_to_zero 原语（秒杀族唯一入口：短路/转化/EVENT_HP_TO_ZERO
    // 在原语内统一处理）。
    const int n_at_arm = args.int_args[4];
    if (args.int_count >= 5 && n_at_arm >= hi) {
        set_audit(ctx, k1627AuditBase, self, 5, 1);
        add_audit(ctx, k1627AuditBase, self, 7, 1);
        const HpZeroResult r = g_core->force_hp_to_zero(ctx, opp, self);
        set_audit(ctx, k1627AuditBase, self, 6, static_cast<int>(r));
        return EffectResult::kOk;
    }
    set_audit(ctx, k1627AuditBase, self, 5, 0);   // 未达上限：门闭（负测面）
    return EffectResult::kOk;
}

// 1627 主体：SKILL_EFFECT 读内核物化 N（规则行 (1627,1,2) 已在 resolve_skill_
// execution 掷出）+ 审计面写入 + 恒挂 AFTER_ACTION probe（判定在 probe，见上）。
// 行参 [owner, target, {0}=回合, {1}=下限, {2}=上限]（args_num=3 + [owner,target] 前缀）。
EffectResult effect_1627_combo_max_kill(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1627, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1627, args,
                     "1627 布局=[owner,target,回合,下限,上限]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int lo = args.int_args[3];
    const int hi = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1627, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds >= 1, 1627, args, "需要 {0}回合 >= 1");
    EFFECT_ARG_GUARD(lo >= 1 && hi >= lo, 1627, args, "需要 1 <= {1}下限 <= {2}上限");

    // N = 内核规则行 (1627,1,2) 每次使用掷一次的物化值（SKILL_EFFECT 时点已可见；
    // consume() 只清 rewritten 标记不清值，AFTER_ACTION 前后同值）。
    const int n = static_cast<int>(ctx->ws.combo_view[owner]);
    set_audit(ctx, k1627AuditBase, owner, 0, rounds);
    set_audit(ctx, k1627AuditBase, owner, 1, lo);
    set_audit(ctx, k1627AuditBase, owner, 2, hi);
    set_audit(ctx, k1627AuditBase, owner, 3, n);
    set_audit(ctx, k1627AuditBase, owner, 4, n >= hi ? 1 : 0);
    add_audit(ctx, k1627AuditBase, owner, 8, 1);

    // probe 恒注册（达上限与否的**唯一判据在 probe 门**——本时点只做行参/审计面）。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(1627, 0, owner, /*left_round=*/0,
                 EffectArgs(std::vector<int>{owner, target, hi, ctx->roundCount, n}),
                 &effect_1627_max_kill_probe);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1627;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1863, effect_1863_pp_amp_combo);
SKILL_EFFECT(1627, effect_1627_combo_max_kill);
