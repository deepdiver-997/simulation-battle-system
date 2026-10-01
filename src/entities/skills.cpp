#include <entities/skills.h>

#include <cstdlib>

#include <entities/elf-pet.h>

#include <algorithm>

#include <abnormal-system/abnormal-types.h>
#include <effects/effect_meta.h>
#include <effects/effect_unit_parser.h>
#include <effects/effect_unit_loader.h>
#include <effects/common_trait_effects.h>
#include <effects/trait_state.h>
#include <fsm/battleContext.h>

namespace {

SkillType map_skill_type(int category) {
    switch (category) {
        case 1: return SkillType::Physical;
        case 2: return SkillType::Special;
        default: return SkillType::Attribute;
    }
}

EffectArgs build_effect_args_for_skill(const official_data::SkillEffectRecord& record) {
    std::vector<int> args;
    args.reserve(static_cast<std::size_t>(record.arg_count) + 2);
    args.push_back(0);
    args.push_back(1);
    args.insert(args.end(), record.args.begin(), record.args.end());
    return EffectArgs(std::move(args));
}

bool monster_has_skill(const official_data::MonsterRecord& monster, int skill_id) {
    // 第五技能（hide_moves，如圣灵谱尼神灵救世光）也算"属于这只精灵"；
    // 它只能占第 5 槽的槽位规则由 PetFactory::create_skills_for_pet 把守。
    if (std::find(monster.hidden_moves.begin(), monster.hidden_moves.end(), skill_id)
        != monster.hidden_moves.end()) {
        return true;
    }
    return std::any_of(
        monster.learnable_moves.begin(),
        monster.learnable_moves.end(),
        [skill_id](const official_data::LearnableMoveRecord& move) {
            return move.move_id == skill_id;
        }
    );
}

// 连击模板（"1回合做 x~y 次攻击"）：effect_id → （下限参数下标, 上限参数下标）。
//
// 官方这一族共 20 条模板 / 158 个技能引用（`effect_info` 实测），本轮只取**静态 x~y 区间**那一层
// （12 条）。参数位置几乎都是 `{0}-{1}`，只有 1172/1577 是 `{0}回合做{1}-{2}次`（前面多一个回合数）。
// ⚠️ 只解析**参数下标**、不碰 `effect_info.info` 文本——文本解析是既定的脆弱层，官方换措辞就挂。
// 认证数据层方向：将来可迁 `custom_effect_overrides(override_type='combo')`，现在先落开关表。
// 未覆盖（见 docs/05-任务清单/待做-变威力与增减伤时点.md §2）：
//   · 动态加数 484 / 1108 / 1795 / 1863 / 1930（基数固定、靠计数器加）
//   · 连击上限修正 1500 / 1546 / 1577 / 1593 / 1627 / 1666 / 1685（需"护盾/蓄力/PP/体力/领域/异常"谓词）
//   · 无参数常量 1608 / 1609 / 1610（区间写死在文案里）
std::optional<std::pair<int, int>> combo_arg_indices(int effect_id) {
    switch (effect_id) {
        case 31:    // （无 effect_info 行的裸连击模板）经典连击族：乱突/疯狂乱抓[2,5]、
                    // 花瓣舞[2,3]——args[0]=下限 args[1]=上限。2026-09-17 用引用它的 303 个
                    // 技能逐个核对：全是老连击招，[lo,hi] 就是攻击次数区间
                    //（交接文档旧表把 31 误记为"消强"——消强是 effect 33，已有实现）。
                    // 模板本体无文本无动作 → loadSkills 在这里顺路取完连击数后，
                    // 注册一个 no-op 处理器仅用于消音启动审计（见 moves_lib effect_31_combo_noop）。
            return std::make_pair(0, 1);
        case 1172:  // {0}回合做{1}~{2}次攻击，每次攻击都有{3}%的概率令自身{4}
        case 1577:  // {0}回合做{1}-{2}次攻击，当前技能PP值小于{3}时连击上限为{4}
        case 1627:  // {0}回合做{1}-{2}次攻击，若本回合攻击次数达到最大则必定秒杀对手
            return std::make_pair(1, 2);
        case 1141:  // 1回合做{0}~{1}次攻击，每次攻击{2}%令对手{3}，攻击低于{4}次则…
        case 1454:  // 1回合做{0}-{1}次攻击，每次攻击有{2}%的概率附加{3}点固定伤害
        case 1455:  // 1回合做{0}-{1}次攻击，若本回合攻击次数达到最大则…必定秒杀对手
        case 1500:  // 1回合做{0}-{1}次攻击，自身处于护盾状态下连击上限为{2}
        case 1546:  // 1回合做{0}-{1}次攻击，自身每存在1层蓄力则连击上限次数额外增加{2}次
        case 1593:  // 1回合做{0}-{1}次攻击，自身体力低于对手时连击上限为{2}
        case 1666:  // 1回合做{0}-{1}次攻击，自身处于领域效果下连击上限为{2}
        case 1685:  // 1回合做{0}-{1}次攻击，对手处于异常状态时连击上限为{2}
        case 1732:  // 激发古渊灵鱼全部的力量，1回合做{0}-{1}次攻击
            return std::make_pair(0, 1);
        default:
            return std::nullopt;
    }
}

// 效果注册时点（后续数据化：从 effect_info / side_effect 表查 register_state 列）
State effect_register_state(int effect_id) {
    switch (effect_id) {
        case 6:
        case 8:
            return State::BATTLE_FIRST_ATTACK_DAMAGE;
        case 1249:  // 两代五技共有"造成伤害的{0}%恢复自身体力(+条件等量百分比伤害)"：
                    // 需伤害结算后读 ws.raw_attack_damage（攻伤台账），同 422/1256 族
            return State::BATTLE_FIRST_ATTACK_DAMAGE;
        case 2020:  // 末法迪知 "若自身处于异常状态则先制+3"：先制计算时点判定
            return State::BATTLE_FIRST_MOVE_RIGHT;
        case 2397:  // 王·天衍星霜 "击败对手时转移宙变之殢"：击败对手后时点（1960 同款）
        case 865:   // 重生之翼·无上天命剑 "击败对手则获得{0}层神耀能量"：击败与否在
                    // "击败对手后"时点才可知；归因读 pet 槽 kWingKillRoundKey
                    // （魂印侧 EVENT_DEATH watcher 写；wing_skills.cpp，2026-09-23）
            return State::BATTLE_AFTER_DEFEATING_OPPONENT;
        case 1256:  // 王·酷烈风息 "造成的伤害低于X"：需伤害结算后读 resolvedDamage.final
        case 101:   // "伤害数值的{0}%恢复自身"（吸血）：同上，伤害结算后按最终伤害回血
        case 422:   // "附加所造成伤害值{0}%的固定伤害"：伤害结算后读**裸伤台账**
                    // （ws.raw_attack_damage，"管线前"的值；见 battleWorkspace.h 三值说明）
        case 1221:  // 王·酷烈风息 "反转自身能力下降"：攻击技能**先结算伤害再反转**——
                    // 反转不参与本次伤害（本次用反转前等级，提升留给下次），故伤害结算后操作 levels
        case 521:   // 反转自身能力下降状态（无参基本形，1221 的主子句同族）：同上口径
        case 1208:  // 天启帝君·天君启示录 "反转自身能力下降…成功则对手下N次攻击无效"：
                    // 1221/521 同族（天启帝君 3550 线，2026-09-20）
        case 505:   // 天启帝君·流风剑道 "打出致命一击则伤害值{0}%回血"：
                    // 暴击与否与最终伤害都在结算后可知
        case 1306:  // 天启帝君·纵横三千界 "造成的伤害低于350…"：1256 同款读 resolvedDamage.final
        case 1307:  // 天启帝君·纵横三千界 "未击败对手则…"：击败与否在伤害落地后判定
        case 1308:  // 天启帝君·纵横三千界 "击败对手则…"：同上（1307 的补集分支）
        case 1382:  // 圣光莫妮卡·万籁俱寂 "命中后获得{0}层，若造成的伤害高于{1}则额外+{2}层"：
                    // 伤害阈值在结算后可知（1256 同族；圣莫技能组 2026-09-25）
        case 1054:  // 圣光莫妮卡·王·鸾歌余音 "反转自身能力下降，反转成功则对手{0}"：
                    // 1221/521 反转族口径——攻击先结算伤害再反转
            return State::BATTLE_FIRST_AFTER_ACTION;
        case 531:   // 莫塔里安·黯狱责罚 "造成的伤害低于280时每相差2点有1%概率使对手害怕"
                    // → 概率由**最终伤害**决定，伤害结算后才知道（1256/1306 同族）
        case 793:   // 莫塔里安·禁命永囚劫 "若造成的伤害低于{0}，则下{1}回合每回合造成{2}点固伤"
                    // → 同上，读 final 判阈值
            return State::BATTLE_FIRST_AFTER_ACTION;
        case 1070:  // 莫塔里安·尘沙结天咒 "对手处于能力下降状态时自身先制+1"
                    // → 先手权判定时读对手等级（固有效果族；每回合重判）
        case 2079:  // 灵巢之主·无念归空净 "对手处于护盾状态时先制+1并无视对手护盾效果"
        case 881:   // 阿尔忒弥斯·森语百兽谣 "若自身处于能力下降状态则技能先制+3"
        case 2273:  // 阿尔忒弥斯·穿林响 "对手选择攻击技能时此技能先制+3"
                    // → 同为条件先制族，一律挂先手权判定时点
            return State::BATTLE_FIRST_MOVE_RIGHT;
        case 1960:  // 希拓·神煌炎舞斩 "击败对手则令自身N回合内强化无法被消除或吸取"
                    // → 击败对手后时点（本轮线性序最后，本技能效果仍在桶里）
            return State::BATTLE_AFTER_DEFEATING_OPPONENT;
        // ── 空元之诗（空元行者 4586；官方 effect_icon 1791）────────────────────
        // 「空元行者技能中的空元之诗效果**不因原本的行动而触发**，每次行动结束后，若本回合
        //   所选技能的空元之诗效果生效且条件满足，空元行者会抹除一篇空妄诗章，抹除成功后
        //   进行**一次额外行动触发空元之诗效果**」
        // 这 5 条子效果（渍/镀/柱/烙/均，各挂在一把 4586 技能上，全库唯一宿主）**本来就是
        // 技能的 side_effect**：默认时点（BATTLE_FIRST_SKILL_EFFECT）落在普通行动里 →
        // 会"因原本的行动而触发"，与官方口径相反。
        // ★ 所以"门控"与"payload"是**同一个决定**：把它们的注册时点直接挪到额外行动时点。
        //   于是 ①普通行动里注册不进来 → 结构上不可能触发（不是靠执行期早退）；
        //        ②额外行动桶里它就在 → "声明内容"由既有桶模型天然承载，无需 payload 结构。
        //   两条要求一次满足，且不改 `UnitAdmissionFn`（避免动插件接口）。
        // ⚠️ 依赖 state_for_owner 的 BATTLE_FIRST_EXTRA_ACTION 镜像（上方已加）。
        case 2086:  // 空元之诗·渍：若技能无效，则消除对手回合类效果、能力提升效果，
                    //            消除成功任意一项则令对手诅咒
        case 2087:  // ·镀：若对手处于控制类异常状态，则己方免疫下2次受到的异常状态
        case 2088:  // ·柱：若对手处于回合类效果，则对手每有1个技能PP值不为满附加50点
                    //       次元·龙系伤害且对手下回合无法主动切换精灵
        case 2089:  // ·烙：若自身为先出手，则此技能每剩余1点PP值附加50点次元·龙系伤害，
                    //       下次击败对手后恢复自身全部体力与PP值
        case 2090:  // ·均：附加双方体力上限差值50%的次元·龙系伤害，自身体力上限高于对手时
                    //       额外吸取对手第五技能剩余的PP值，低于对手时附加伤害翻倍
            return State::BATTLE_FIRST_EXTRA_ACTION;
        case 1218:  // 神忏福音之章 "未击败对手则下{0}回合自身先制+{1}"：
                    // "未击败"要在**伤害落地之后**判（1307 同款，官方 idx=401 第 6 条把
                    // "未击败/击败"收纳在同一时点）→ 同 1307/2105 归位 AFTER_ACTION
        case 984:   // 神游·浩渺星河 "未击败对手则下回合自身所有技能先制+{0}"
                    // （启灵元神 33893）：1218 同款归位 AFTER_ACTION（2026-09-22）
            return State::BATTLE_FIRST_AFTER_ACTION;
        case 1323:  // 「造成伤害的{0}%恢复自身体力，暴击则附加恢复量等量的百分比伤害」
                    // （协议IV·解 37357）：读**结算后**的 resolvedDamage.final / isCrit
                    // → 挂 AFTER_ACTION（101 吸血同路，state_for_owner 镜像 SECOND）
            return State::BATTLE_FIRST_AFTER_ACTION;
        case 1533:  // 「造成的攻击伤害低于{0}时附加大量真实伤害」（启灵元神 33894）：
                    // 读裸伤台账 ws.raw_attack_damage（ATTACK_DAMAGE 收尾才写入）→
                    // 挂 AFTER_ACTION（422/雷解同款"造成的伤害"读法）
            return State::BATTLE_FIRST_AFTER_ACTION;
        case 2440:  // 「未击败对手时恢复当回合所造成攻击伤害的{0}%的体力，若未造成攻击
                    // 伤害则对手{1}回合内无法主动切换精灵」（百罗鬼帝 38383 蔑天行·残虐）：
                    // "未击败"与伤害量都要在伤害落地后判（1218/1533 同款归位）；
                    // 裸伤读台账，未击败 = 对手 hp>0
            return State::BATTLE_FIRST_AFTER_ACTION;
        default:
            return State::BATTLE_FIRST_SKILL_EFFECT;
    }
}

// 基值先制效果模板：MOVE_RIGHT 时点给 owner 的先手权累加 priority。
// Args: [owner(0), target(1), priority(2)]（bind_participants 会填 owner/target）
EffectResult effect_apply_base_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int priority = args.int_args[2];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += priority;
    return EffectResult::kOk;
}

/**
 * 效果所属分支（HIT / SKILL_INVALID）。
 *
 * 官方 effect 描述决定效果何时触发：
 *   - 普通效果：技能命中才触发 → HIT 分支
 *   - "技能无效时XXX" 的被动效果：技能 miss/禁用时仍要执行 → SKILL_INVALID 分支
 *
 * 目前已知的"无效时触发"效果（数据可后续移入 DB 表）：
 *   - 2006：技能无效时，免疫下1次对手的攻击，免疫成功则令对手全属性+1（索杰德尔·无念归空净）
 *   - 2501：技能无效时，重新进行伤害结算且每260特攻威力翻倍1次（薇尔诗·乐园之初诞）
 *   - 136 ：若Miss则自己恢复1/{0}体力（灵魂之歌 22410；盔挡下同走无效分支，口径同 2126）
 *   - 2418：技能无效时，获得等同于对手最大体力1/3的护盾、护罩（大赤殓 38282）
 */
SkillExecResult default_branch_for_effect(int effect_id) {
    switch (effect_id) {
        case 2006:
        case 2501:
        case 2126:  // 烬灭神咒剑：技能无效时消除对手回合类/能力提升 + 焚烬
        case 136:   // 灵魂之歌：若Miss则自己恢复1/4体力
        case 2418:  // 大赤殓：技能无效时获得对手最大体力1/3的护盾、护罩
        case 2175:  // 混色神·虚之彩：技能无效时随机附加2种非限制类异常（D27 涂蝶 4861，
                    // 实现 moves_lib/d5_D27.cpp；池 = 全池⊖限制类）
            return SkillExecResult::SKILL_INVALID;
        case 2086:  // 空元之诗·渍：「**若技能无效**，则消除对手回合类效果、能力提升效果…」
                    // 它的整个条件就是"技能无效"→ 必须挂在 SKILL_INVALID 分支上，
                    // 否则技能 miss/被盔时该效果根本不注册（那条正是它唯一的用武之地）。
                    // 时点仍是额外行动（见 effect_register_state）——分支决定"哪一趟注册"，
                    // 时点决定"哪一趟执行"，两者正交。
            return SkillExecResult::SKILL_INVALID;
        // 空元之诗·镀/柱/烙/均（2087~2090）：**固有效果**（用户 2026-09-20 口径）——
        // 只因"封效果"类机制不触发；被封属 / 打在盔上 / miss（都走 SKILL_INVALID 补偿趟）
        // 照常注册。归 INHERENT：HIT 与 SKILL_INVALID 两趟都注册（见 Skills::execute）。
        // 它们各自的"若…则…"条件不在分支里判——由魂印侧在**抹除诗章之前**求值
        // （vanish_kongyuan.cpp 的 soulmark_2175_poem_declare）。
        case 2087:
        case 2088:
        case 2089:
        case 2090:
            return SkillExecResult::INHERENT;
        default:
            return SkillExecResult::HIT;
    }
}

// 穿透类效果的凭证位（697"无视伤害限制"/699"无视攻击免疫"）。
// 与 effect_meta 的 Penetration 分类双保险：meta 负责"这是不是穿透类"，
// 此表负责"穿什么"；未列出的穿透效果返回全零（降级为无凭证）。
Skills::PenetrationFlags penetration_flags_for_effect(int effect_id) {
    Skills::PenetrationFlags flags;
    switch (effect_id) {
        case 697:  // 无视伤害限制效果
            flags.ignore_damage_limit = true;
            flags.level = 1;
            break;
        case 699:  // 无视攻击免疫效果
            flags.ignore_attack_immunity = true;
            flags.level = 1;
            break;
        default:
            break;
    }
    return flags;
}

// 把"技能自带穿透（697/699）+ 活跃次数授予"合并进 ws.attack_credential[owner]。
// 攻击时现算（每次攻击重新合并）而非每回合物化——额外行动/多攻击不会复用已消费凭证，
// 回合中段新获得的授予也能即时读到。凭证在 ws 里每回合 reset 自动清，无需手动销毁。
void materialize_attack_credential(BattleContext* ctx, int owner, const Skills& skill) {
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    BattleWorkspace::AttackCredential& cred = ctx->ws.attack_credential[owner];
    cred = BattleWorkspace::AttackCredential{};  // 用默认成员清零
    cred.ignore_attack_immunity |= skill.penetration_flags.ignore_attack_immunity;
    cred.ignore_damage_limit |= skill.penetration_flags.ignore_damage_limit;
    cred.force_execute |= skill.penetration_flags.force_execute;
    cred.level = std::max(cred.level, skill.penetration_flags.level);
    // 次数型/窗口型「无视」凭证：查 **RuleCenter 的 PENETRATE_ATTACK 门**。
    // ⚠️ 这里查的是"攻击技能"那条路 —— 只注册了 PENETRATE_ATTRIBUTE 的凭证（薇尔诗式全凭证
    //    若只登记属性侧）不会被算进攻击凭证；反之 1926「下N次**攻击技能**无视」（无罔之心）
    //    只登记攻击侧，属性伤害那条路查另一个门 → 既不命中也不消耗（用户 2026-09-18 口径）。
    const RuleCenter::PenetrationQuery pen =
        ctx->rule_center_.query_penetrate(owner, RuleCategory::PENETRATE_ATTACK, ctx->roundCount);
    if (pen.valid) {
        cred.ignore_attack_immunity |= pen.ignore_attack_immunity;
        cred.ignore_damage_limit |= pen.ignore_damage_limit;
        cred.level = std::max(cred.level, pen.level);
    }
    // 魂印条件凭证（SET 端）：使用 PP=0 技能 + force_execute_on_pp0 → 强制执行（无为觉者 2260）。
    if (ctx->force_execute_on_pp0[owner] && skill.pp == 0) {
        cred.force_execute = true;
    }
    // 必定命中（三合一）。判"是否必中"一律读这个凭证，不要只读 skill 里写死的字段——
    // 条件必中固有效果（如 2000「对手处于能力提升则先制+1且必中」）是靠 ② 在**出手前**
    // 授予的（效果体在 MOVE_RIGHT 时点写 ws.must_hit_grant）。
    cred.must_hit = skill.must_hit                  // ① 官方 moves.must_hit 固有必中
                 || ctx->ws.must_hit_grant[owner]   // ② 本回合效果授予的条件必中
                 || cred.force_execute;             // ③ 强制执行隐含必定命中
    // 基础命中率修正 100%（守御八方 1275 传承，见 ws.accuracy_fix_grant 注释）——
    // 与 must_hit 分离：它不打必中标记（弱化仍可 Miss），只改 roll 起点 + 跳过必闪减避。
    cred.accuracy_fix_100 = ctx->ws.accuracy_fix_grant[owner];
    cred.valid = cred.ignore_attack_immunity || cred.ignore_damage_limit || cred.force_execute;
}

// 通用执行器：运行解析出的条件效果单元（Effect.args.extra 指向单元，见 loadSkills）。
// 组合语法：未注册函数的效果模板经 parse_effect_unit 解析成单元，由此函数驱动。
EffectResult effect_run_parsed_unit(BattleContext* ctx, const EffectArgs& args) {
    const auto* unit = static_cast<const EffectUnit*>(args.extra);
    if (!ctx || !unit) {
        return EffectResult::kOk;
    }
    execute_effect_unit(ctx, args, *unit);
    return EffectResult::kOk;
}

// 同上，但**跳过本单元的条件**（单元准入门放行时 register_branch 换用本执行器）。
// 语义 = "该条已永久无条件"；概率 roll 与分支递归照常。
EffectResult effect_run_parsed_unit_unconditional(BattleContext* ctx, const EffectArgs& args) {
    const auto* unit = static_cast<const EffectUnit*>(args.extra);
    if (!ctx || !unit) {
        return EffectResult::kOk;
    }
    execute_effect_unit_unconditional(ctx, args, *unit);
    return EffectResult::kOk;
}

} // namespace

// 全局记账出口（供 registry dump 并集；见 include/entities/skills.h 尾注）。
const std::vector<int>& kernel_penetration_effect_ids() {
    static const std::vector<int> ids = {697, 699};
    return ids;
}

Skills::Skills(int id, const official_data::MonsterRecord& monster,
               std::vector<EffectParamOverride> arg_overrides,
               const DisabledEffects& disabled)
    : id(id),
      arg_overrides_(std::move(arg_overrides)),
      disabled_(disabled) {
    if (id <= 0) {
        throw std::runtime_error("invalid skill id: " + std::to_string(id));
    }
    if (!monster_has_skill(monster, id)) {
        throw std::runtime_error(
            "skill " + std::to_string(id) + " does not belong to pet " +
            std::to_string(monster.id) + " (" + monster.name + ")"
        );
    }
    if (!loadSkills()) {
        throw std::runtime_error("Failed to load skill with id: " + std::to_string(id));
    }
}

// 词条参数覆盖（调试台"词条编辑"线，2026-09-26）。
// arg_index 与 effect_info.info 模板的 {n} 占位符同下标；DB 省略的隐式尾参按 0 补齐。
// 覆盖了本技能不存在的效果 → 抛错（try_start_battle 捕获后转 lineup rejected，
// 用户在控制台立刻能看到手误）。
void Skills::apply_arg_overrides() {
    if (arg_overrides_.empty()) {
        return;
    }
    for (const EffectParamOverride& ov : arg_overrides_) {
        bool hit = false;
        for (official_data::SkillEffectRecord& rec : rawEffectRecords) {
            if (rec.effect_id != ov.effect_id) {
                continue;
            }
            hit = true;
            if (ov.arg_index < 0) {
                throw std::runtime_error(
                    "effect param override: negative arg index on effect " +
                    std::to_string(ov.effect_id));
            }
            if (static_cast<std::size_t>(ov.arg_index) >= rec.args.size()) {
                rec.args.resize(static_cast<std::size_t>(ov.arg_index) + 1, 0);
            }
            rec.args[static_cast<std::size_t>(ov.arg_index)] = ov.value;
        }
        if (!hit) {
            throw std::runtime_error(
                "effect param override: skill " + std::to_string(id) + " has no effect " +
                std::to_string(ov.effect_id));
        }
    }
}

bool Skills::loadSkills() {
    auto& store = official_data::OfficialDataStore::instance();
    if (!store.ready()) {
        if (!store.initialize()) {
            return false;
        }
    }

    const std::optional<official_data::SkillRecord> record = store.repository().load_skill(id);
    if (!record) {
        return false;
    }

    name = record->name;
    type = map_skill_type(record->category);
    power = record->power;
    accuracy = record->accuracy;
    must_hit = record->must_hit != 0;
    priority = record->priority;
    maxPP = record->max_pp;
    pp = record->max_pp;
    // 暴击率（官方 moves.crit_rate，用户 2026-09-13 定口径）：**分母 16 的分子**——
    // crit_rate=8 → 8/16 = 50%、=16 → 100%；**`0` = 0/16 = 永不必暴**
    // （不是"基础 1/16"——确实有天生暴击率为 0 的技能，没有别的引爆效果就永远不会暴击）。
    // 本字段存**百分比**（与 ws.crit_rate_mod 的乘算口径一致，效果可直接改 mod 缩放它）。
    critical_strike_rate = static_cast<float>(record->crit_rate) * 100.0f / 16.0f;
    element[0] = record->type_id;
    element[1] = 0;
    rawEffectRecords = record->effects;
    // 效果禁用（2026-09-26 禁用基建）：在参数覆盖之前摘除——被禁效果视为不存在，
    // 之后的参数覆盖对它报"效果不存在"（与手误路径一致）。
    if (!disabled_.skill_effects.empty()) {
        rawEffectRecords.erase(
            std::remove_if(
                rawEffectRecords.begin(), rawEffectRecords.end(),
                [this](const official_data::SkillEffectRecord& rec) {
                    return std::find(disabled_.skill_effects.begin(),
                                     disabled_.skill_effects.end(),
                                     rec.effect_id) != disabled_.skill_effects.end();
                }
            ),
            rawEffectRecords.end()
        );
    }
    // 词条参数覆盖（调试台线）：先改记录、后建效果——下方所有 build_effect_args_for_skill
    // 读到的都是覆盖后的 args（注册分支/选择期/自定义程序全路径生效）。
    apply_arg_overrides();
    effectBranches.clear();
    selection_effects_.clear();
    parsed_units_.clear();
    // 携带类效果装槽（加载期识别）：逐条查携带注册表，命中即装。战斗期扫描只读本槽
    // （init_battle），不再查注册表——见 Skills::carryEffects 注释。必须在
    // rawEffectRecords 定型后执行；本函数失败即 throw，不存在半装槽状态外泄。
    carryEffects.clear();
    // 可用性修饰（魂印/印记带来的"PP=0 仍可释放"）同样是**战斗内动态写入**的槽
    // （之铭·叵 每回合按面板槽刷），重载技能时必须清空——否则同一 Skills 对象被复用
    // （换形态/重载数据）时上一轮的修饰会残留。
    usabilityEffects.clear();
    for (const auto& effect_record : rawEffectRecords) {
        if (CarryEffectFn carry_fn =
                EffectFactory::getInstance().find_carry_effect(effect_record.effect_id)) {
            carryEffects.emplace_back(effect_record.effect_id, carry_fn);
        }
    }
    // 解析器的分支指针指向 parsed_units_ 内元素，reserve 足量防 realloc 悬垂。
    parsed_units_.reserve(rawEffectRecords.size() * 3 + 4);

    for (const auto& effect_record : rawEffectRecords) {
        // 连击（"1回合做 x~y 次攻击"）：从本效果自己的参数里取下/上限，填技能静态基数。
        // ⚠️ **不 continue**——这些模板的其余子句（如 1172 的"每次攻击X%概率令自身Y"）现在由
        //    下方注册/解析路径部分覆盖着，跳过会回归。这里只是"顺路取个数"。
        if (const auto combo_idx = combo_arg_indices(effect_record.effect_id)) {
            const auto& a = effect_record.args;
            const std::size_t hi = static_cast<std::size_t>(combo_idx->second);
            if (a.size() > hi) {
                const int lo = std::max(1, a[static_cast<std::size_t>(combo_idx->first)]);
                const int up = std::max(1, a[hi]);
                combo_min = std::min(lo, up);
                combo_max = std::max(lo, up);
            }
        }
        // 穿透类效果（697"无视伤害限制"/699"无视攻击免疫"）→ 并入本技能穿透凭证，
        // 不注册普通分支。理由：穿透在 query_usage 门判定（效果注册之前）就消费，
        // 697/699 是 args_num=0 的纯标记模板，注册成 HIT 分支既无函数可执行也时机太晚。
        const EffectMeta* meta = EffectMetaCatalog::instance().find(effect_record.effect_id);
        if (meta && meta->category == EffectCategory::Penetration) {
            const PenetrationFlags pf = penetration_flags_for_effect(effect_record.effect_id);
            penetration_flags.ignore_attack_immunity |= pf.ignore_attack_immunity;
            penetration_flags.ignore_damage_limit |= pf.ignore_damage_limit;
            penetration_flags.level = std::max(penetration_flags.level, pf.level);
            continue;
        }
        // 选择期先制效果 → selection_effects_（on_selected 统一注册到 MOVE_RIGHT 桶，
        // 先手权比较前生效）。与基值先制同族——**必须在选技时决定**，注册到 SKILL_EFFECT
        // 等执行期时点就太晚了（先手权早已结算完）。
        //   2000 大雪纷飞/烬灭神咒剑：若对手处于能力提升状态则先制+1且必定命中
        //   610  璨灵圣光：遇到天敌时先制+{0}
        //   954  「若当前体力低于对手则先制+1」（协议I·激 37354 等 36 技通用模板）
        //   1848 王·御影神罚族：对手处于能力下降状态时先制+2（神觉·米斯蒂克）
        //   881  神火剑阵 37221：若自身处于能力下降状态则先制+3（2026-09-20 烧伤三精灵线）
        // 一般化的"选择期效果路由"（按 EffectMeta 分类而非硬编码 id）留数据驱动后续。
        //   1532 神游·浩渺星河：对手不存在神印则攻击必定致命一击（2026-09-22 启灵线）——
        //        必暴写 ws.must_crit，而暴击掷点在 query_usage ①.5（ON_SKILL_HIT），
        //        执行期桶（SKILL_EFFECT 及以后）写必暴只惠及下次攻击；MOVE_RIGHT 是
        //        掷点前最后一个可执行时点（且其效果体运行时本轮所有提交均已应用 →
        //        神印发布槽为新鲜值）。PP 归零半边由效果体自延后到自身行动期
        //        （见 qiling_skills.cpp：先/后手各挂一格、只在自己那格执行）。
        if (effect_record.effect_id == 2000 || effect_record.effect_id == 610
            || effect_record.effect_id == 954 || effect_record.effect_id == 1848
            || effect_record.effect_id == 881 || effect_record.effect_id == 1532) {
            Effect sel = clone_effect(effect_record.effect_id, build_effect_args_for_skill(effect_record));
            if (sel.logic) {
                selection_effects_.push_back(
                    SkillEffectNode(std::move(sel), State::BATTLE_FIRST_MOVE_RIGHT));
            }
            continue;
        }
        // 选择期"**微弱**对手则…"一族（用户 2026-09-14 定口径）：
        //   785  若**自身**攻击对手时克制关系为微弱 → 先制+2   （判**精灵**系别）
        //   2030 若**本次技能**微弱对手     → 先制+1   （判**技能**系别）
        //   2490 此技能微弱对手时 → 先制+3、获得本系加成并以神灵系结算（判**技能**系别）
        // 全族 args_num=0 → 先制数写死在文案里，插件侧是常量。
        // ★ 判"**此技能**是否微弱"的两条（2030/2490）必须用**技能槽位的静态系别**，不能读
        //   `ws.skill_element_view`——视图会被"以XX系别进行伤害结算"这类效果改写，读视图等于
        //   把改写后的系别喂回判定。插件拿不到 core 内部的 `resolve_executing_skill`，
        //   所以**把技能静态系别追加进 args**（args[2]/args[3]）由 core 喂过去，不必开 CoreApi 新槽。
        //   785 判的是"**自身（精灵）**是否微弱"，系别在运行时从精灵系别视图取（`ws.view_elementalAttributes`），
        //   不需要额外参数（精灵系别会被"属性反转"这类效果改，读视图才对）。
        if (effect_record.effect_id == 785 || effect_record.effect_id == 2030
            || effect_record.effect_id == 2490) {
            EffectArgs sel_args = build_effect_args_for_skill(effect_record);
            if (effect_record.effect_id == 2030 || effect_record.effect_id == 2490) {
                if (sel_args.owned_int_args.size() < 4) {
                    sel_args.owned_int_args.resize(4, 0);
                }
                sel_args.owned_int_args[2] = element[0];
                sel_args.owned_int_args[3] = element[1];
                sel_args.refresh_views();
            }
            Effect sel = clone_effect(effect_record.effect_id, std::move(sel_args));
            if (sel.logic) {
                selection_effects_.push_back(
                    SkillEffectNode(std::move(sel), State::BATTLE_FIRST_MOVE_RIGHT));
            }
            continue;
        }
        // ── 认证数据层（custom_* 表）──────────────────────────────
        // ① override(官差纠偏, 待 seeds 落地后接 dispatch：ignore/dead_column/map_to/...)；
        // ② program(离线编码效果程序)：命中则用 JSON 加载器构造单元, 替代下方"注册函数→
        //    运行时文本 parser"路径（离线编码、不碰脆弱官方文本解析）。
        // 两张表在未落地前(表空/缺表)安全返回 nullopt → 走既有路径, 行为不变。
        auto& repository = store.repository();
        const auto custom_ovr = repository.load_custom_override(effect_record.effect_id);
        const auto custom_prog = repository.load_custom_program(effect_record.effect_id, this->id);
        (void)custom_ovr;  // override dispatch 待 seeds 落地后接（现在只是查询接缝）。
        if (custom_prog) {
            // 离线编码程序：JSON → EffectUnit（skill_args 解析 {"arg":n} 占位）。
            const int unit_idx =
                load_effect_unit_from_json(custom_prog->unit_json, effect_record.args, parsed_units_);
            if (unit_idx >= 0) {
                // 全参构造（Effect 无默认构造）：left_round=0 → 一次性（本回合有效）。
                Effect unit_effect(effect_record.effect_id, 0, 0, 0,
                                   EffectArgs(
                                       build_effect_args_for_skill(effect_record).owned_int_args,
                                       &parsed_units_[static_cast<std::size_t>(unit_idx)]),
                                   &effect_run_parsed_unit);
                add_effect_node(
                    default_branch_for_effect(effect_record.effect_id),
                    SkillEffectNode(std::move(unit_effect), effect_register_state(effect_record.effect_id))
                );
            }
            // 程序加载失败(unit_idx<0)：声明在 custom 层却解析失败 → 数据 bug, 跳过并暴露。
            continue;
        }
        // 未命中离线程序：走既有 注册函数 → parser 兜底路径。
        // effect 9（连续使用威力递增）：连用判定按**技能 id**——core 把自己的 id 追加进
        // args[4]（同 2030/2490 的 core 喂参先例；插件拿不到 resolve_executing_skill）。
        EffectArgs effect_args = build_effect_args_for_skill(effect_record);
        if (effect_record.effect_id == 9) {
            if (effect_args.owned_int_args.size() < 5) {
                effect_args.owned_int_args.resize(5, 0);
            }
            effect_args.owned_int_args[4] = id;
            effect_args.refresh_views();
        }
        // 元神神之宣告（27021）的免断**特判路由**：821 通用模板（25+ 技能共享，含轮回之狱）
        // 保持不动，只有元神被官方特殊处理为 **n-1 回合**——最准确描述是
        // 「1回合免断 ➕ 下1次免断」，不是描述上的 2 回合（语料《机制解析—免断》
        // + 作者顶置；"元神2回合免断和圣雷2回合免断比起来，会少1回合"）。
        // 内部变体 id = 技能id×10 = 270210（刻意避开 DB effect id 空间，DB 8000~9000
        // 段现为空、4 位 id 空间仍会扩充），由 qiling 插件注册实现。
        if (id == 27021 && effect_record.effect_id == 821) {
            Effect variant = clone_effect(270210, build_effect_args_for_skill(effect_record));
            if (variant.logic) {
                add_effect_node(
                    default_branch_for_effect(effect_record.effect_id),
                    SkillEffectNode(std::move(variant),
                                    effect_register_state(effect_record.effect_id)));
            }
            continue;
        }
        Effect effect = clone_effect(effect_record.effect_id, std::move(effect_args));
        if (!effect.logic) {
            // 未注册函数：尝试解析模板为条件效果单元（组合语法），成功则注册通用执行器
            // （args.extra 指向 Skills::parsed_units_ 内单元）。失败维持跳过（现状）。
            const int unit_idx = parse_effect_unit(effect_record.info, effect_record.args, parsed_units_);
            if (unit_idx >= 0) {
                // 全参构造（Effect 无默认构造）：left_round=0 → 一次性（本回合有效）。
                Effect unit_effect(effect_record.effect_id, 0, 0, 0,
                                   EffectArgs(
                                       build_effect_args_for_skill(effect_record).owned_int_args,
                                       &parsed_units_[static_cast<std::size_t>(unit_idx)]),
                                   &effect_run_parsed_unit);
                add_effect_node(
                    default_branch_for_effect(effect_record.effect_id),
                    SkillEffectNode(std::move(unit_effect), effect_register_state(effect_record.effect_id))
                );
            }
            continue;
        }
        add_effect_node(
            default_branch_for_effect(effect_record.effect_id),
            SkillEffectNode(std::move(effect), effect_register_state(effect_record.effect_id))
        );
    }

    // 基值先制也作为一条选择期效果数据放进 selection_effects_：
    // on_selected 统一遍历注册到 MOVE_RIGHT 时点（即使本回合被控导致出招失败也生效）。
    // 条件先制效果（如"对手有护盾则先制+1"）由数据/插件在此之后追加。
    // ⚠️ left_round=-1：先制是**即时结算**（注册后立刻在 MOVE_RIGHT 时点跑一次，
    // 加完 preemptive_level 即使命）—— 必须是非回合类（duration=-1 → isRoundEffect()=false）：
    // 若被当成回合类，会计入 has_round_effects，让 797/889"消回合成功"族误判对手
    // "有回合类效果可断"（072 B1b 曾因此失败，根因链见 effect.h 的注释）。
    Effect base_priority_effect(0, 0, 0, -1, EffectArgs(std::vector<int>{0, 1, priority}),
                                &effect_apply_base_priority);
    selection_effects_.push_back(
        SkillEffectNode(std::move(base_priority_effect), State::BATTLE_FIRST_MOVE_RIGHT)
    );
    return true;
}

bool Skills::skill_usable(BattleContext* ctx, int owner) {
    if (is_locked) {
        return false;
    }

    if (pp == -1) {
        return true;
    }

    if (pp > 0) {
        return true;
    }

    if (pp < 0) {
        return false;
    }

    // PP=0：魂印 ignore_pp 信号（无为觉者 2260 等）→ 可选
    if (ctx && owner >= 0 && owner <= 1 && ctx->ignore_pp[owner]) {
        return true;
    }

    bool hasIgnorePPEffect = false;
    bool hasForceRespectPPEffect = false;
    for (const auto& entry : usabilityEffects) {
        if (!entry.active) {
            continue;
        }
        if (entry.type == SkillUsabilityEffectType::IgnorePP) {
            hasIgnorePPEffect = true;
        } else if (entry.type == SkillUsabilityEffectType::ForceRespectPP) {
            hasForceRespectPPEffect = true;
        }
    }

    return hasIgnorePPEffect && !hasForceRespectPPEffect;
}

SkillSelectionResult Skills::query_selectable(BattleContext* ctx, int owner) {
    if (is_locked) {
        return SkillSelectionResult::LOCKED;
    }
    if (pp == 0) {
        // PP=0：魂印 ignore_pp 信号优先；否则需 active 的 IgnorePP 效果（且无 ForceRespectPP 覆盖）
        if (ctx && owner >= 0 && owner <= 1 && ctx->ignore_pp[owner]) {
            return SkillSelectionResult::SELECTABLE;
        }
        bool hasIgnorePPEffect = false;
        bool hasForceRespectPPEffect = false;
        for (const auto& entry : usabilityEffects) {
            if (!entry.active) {
                continue;
            }
            if (entry.type == SkillUsabilityEffectType::IgnorePP) {
                hasIgnorePPEffect = true;
            } else if (entry.type == SkillUsabilityEffectType::ForceRespectPP) {
                hasForceRespectPPEffect = true;
            }
        }
        return (hasIgnorePPEffect && !hasForceRespectPPEffect)
            ? SkillSelectionResult::SELECTABLE
            : SkillSelectionResult::PP_EMPTY;
    }
    if (pp < 0) {
        return SkillSelectionResult::LOCKED;
    }
    return SkillSelectionResult::SELECTABLE;
}

void Skills::on_selected(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    if (std::getenv("SEL_DBG")) {
        std::fprintf(stderr, "[sel] o=%d skill=%d nodes=%zu state=%d\n", owner, id,
                     selection_effects_.size(), static_cast<int>(ctx->currentState));
    }
    // 选择期效果统一注册到 MOVE_RIGHT 时点（含基值先制 + 条件先制）。
    // 走 registerEffect 统一处理（valid_id 绑定 + 同源去重），source_id = 技能 id。
    for (const SkillEffectNode& node : selection_effects_) {
        Effect effect = node.effect;
        if (!effect.logic) {
            continue;
        }
        // 绑定参与者：args[0]=owner, args[1]=1-owner
        if (effect.args.owned_int_args.size() >= 2) {
            effect.args.owned_int_args[0] = owner;
            effect.args.owned_int_args[1] = 1 - owner;
            effect.args.refresh_views();
        }
        // left_round==0 的一次性效果归一化为本回合有效（同 continuousEffect.cpp 规则）
        const int left_round = effect.left_round;
        const int duration = (left_round < 0) ? -1 : (left_round == 0 ? 1 : left_round);
        auto ce = std::make_unique<ContinuousEffect>(
            effect, State::BATTLE_FIRST_MOVE_RIGHT, owner, duration, ctx->roundCount
        );
        ce->source_id_ = id;  // 技能 id 作为来源，同源去重
        ctx->registerEffect(State::BATTLE_FIRST_MOVE_RIGHT, owner, std::move(ce), EffectContainer::Skill);
    }
}

SkillUsageResult Skills::query_usage(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return SkillUsageResult::MISS;
    }

    // 0) 物化本次请求凭证（穿透 697/699 + 次数授予 + 强制执行 + **必中**），供 ①miss 与 ②门判定读。
    materialize_attack_credential(ctx, owner, *this);
    const BattleWorkspace::AttackCredential& cred = ctx->ws.attack_credential[owner];
    const bool is_attribute = (type == SkillType::Attribute);
    ctx->crit_happened[owner] = false;   // 每次技能使用先清，miss 时不残留

    // ① 命中判定（优先级最高）。三个分支，**顺序不可换**：
    //   (a) 强制执行：隐含必定命中 → 跳过一切命中判定（含失明）。
    //   (b) 失明（异常 20）：**非必中技能必定 miss**；**必中技能 50% 正常命中 / 50% 命中效果失效**。
    //   (c) 常规命中率 roll。
    // "是否必中"一律读**凭证** `cred.must_hit`（技能固有 moves.must_hit ∨ 本回合条件必中授予
    // ∨ 强制执行），不要只读 `this->must_hit`——条件必中固有效果是出手前授予的。
    bool blind_hit_invalid = false;   // 失明掷出的"命中效果失效"档（照常继续走 ② 门判定）
    if (!cred.force_execute) {
        if (ctx->has_active_abnormal_status(owner, static_cast<int>(AbnormalStatusId::Blind))) {
            // 失明：见 abnormal-types.h AbnormalStatusId::Blind。
            if (cred.must_hit) {
                // 必中技能：50% 正常命中 / 50% 命中效果失效（官方口径，用户 2026-09-13 确认）。
                blind_hit_invalid = (std::rand() % 2) != 0;
            } else {
                // 非必中技能：必定 miss。走到这里就是 miss（不再掷命中率）。
                ctx->rule_center_.notify(ctx, owner, is_attribute, this->power,
                                         cred.ignore_attack_immunity, ctx->roundCount);
                return SkillUsageResult::MISS;
            }
        } else if (!cred.must_hit) {
            // **属性攻击对自身必定 miss**（effect 86 圣洁）：被保护方置的 `attribute_must_miss`，
            // 攻击方出手时查 `1 - owner`。⚠️ 是"必定 **miss**"（技能打空）不是"失效"——
            // 所以走这条 miss 路径（照常 notify 消费对方的次数类盔，文档 §2.3），
            // 而不是 SKILL_INVALID（那条会走补偿分支，语义不同）。
            // ⚠️ 必中技能（cred.must_hit）**绕过**它——"必定miss"治不了必中，与失明的处理一致。
            const bool attr_must_miss = is_attribute && ctx->ws.attribute_must_miss[1 - owner];
            // 通用特性·精准（攻方 args[0]%）/ 回避（守方 args[0]%）——必修6：两者都**乘算**在
            // 技能初始命中率上（90×1.1=99 / 90×0.9=81），且**都作用于属性技能**。
            // ⚠️ 只有确实带该特性才改写（无特性者零行为、零 rand 消耗）。
            int accuracy = this->accuracy;
            // 基础命中率修正 100%（accuracy_fix_100）：把 roll 起点钉在 100，**先于**精准/回避
            // 与命中等级档位——弱化（命中负档 85/70/…）照样把它乘下去（用户 2026-09-30 实测：
            // 守御八方传承下弱化仍可能 Miss），而必定闪避词条（dodge_rate）在下方被跳过。
            if (cred.accuracy_fix_100) {
                accuracy = 100;
            }
            if (const std::optional<EffectiveTrait> p =
                    ctx->effective_common_trait(owner, TraitKind::Precision)) {
                accuracy = accuracy * (100 + p->args[0]) / 100;
            }
            if (const std::optional<EffectiveTrait> e =
                    ctx->effective_common_trait(1 - owner, TraitKind::Evasion)) {
                accuracy = accuracy * (100 - e->args[0]) / 100;
            }
            // 命中等级（view_levels 槽 **5**，官方能力提升六项之一）——2026-09-18 接入精度公式。
            // 来源有三类：① 异常衍化族（焚烬 22 结束后"转化为烧伤与命中等级-1"，
            // 见 abnormal-types.h 的 abnormal_derivation）；② 效果层"令对手命中等级±n"
            // （`stat_change/stat_drop` 的 `stat=5`，既有插件 stat_dispel.cpp 已在用）；
            // ③ **视为修正** ws.hit_level_extra（2026-09-20 神觉·米斯蒂克 4676 的蚩庸之锁：
            // "每有 1 层，自身视为命中等级额外 -1"——与命中强化相互抵消，官方口径）。
            // 倍率表见 `hit_level_accuracy_pct()`：**负档沿用引擎既有的官方档位表**
            // （-1→85% … -6→25%，原误置在 getTempAbilityValue 里），正档暂无官方表 → 通用等级换算。
            // ⚠️ 必中技能（cred.must_hit）**绕过**它——必中技能走不到这里 → 命中等级治不了必中。
            {
                const int hit_level = ctx->ws.view_levels[owner][kAbilityLevelIndexHit]
                                    + ctx->ws.hit_level_extra[owner];
                if (hit_level != 0) {
                    accuracy = accuracy * hit_level_accuracy_pct(hit_level) / 100;
                }
            }
            // 命中率修正倍率（默认 1.0；此前为无消费点的死字段，2026-09-19 套装线接入）：
            // 圣芒佑界 476"己方精灵命中率提升15%"（战盔额外效果）每回合写 1.15。
            // 与精准/回避同属乘算链，排在命中等级之后、异常修正之前（固定位置可复现）。
            if (ctx->ws.hit_rate_mod[owner] != 1.0f) {
                accuracy = static_cast<int>(accuracy * ctx->ws.hit_rate_mod[owner]);
            }
            // 异常状态对**攻击技能命中率**的修正（官方 effect_des 10/13）：
            //   混乱(10)「攻击技能的命中率**减少80%**」  → ×20%
            //   易燃(13)「攻击技能命中率**降低30%**」    → ×70%
            // 官方措辞都限定"攻击技能" → gate `!is_attribute`（本轮分支本就只处理非必中技能）。
            // 顺序：摆在命中等级之后、`hit_chance` 之前；乘法链的 int 截断会有 ±1 差异，
            // 固定放在最末让结果可复现。
            if (!is_attribute) {
                // 攻击技门控命中率修正（S1-P0 钩子，2026-10-01 接入）：1314/1576/1673/1843
                // 族魂印"对手使用攻击技能命中率降低X%"的落点——hit_rate_mod 无类型维度。
                if (ctx->ws.hit_rate_mod_attack[owner] != 1.0f) {
                    accuracy = static_cast<int>(accuracy * ctx->ws.hit_rate_mod_attack[owner]);
                }
                if (ctx->has_active_abnormal_status(
                        owner, static_cast<int>(AbnormalStatusId::Confusion))) {
                    accuracy = accuracy * 20 / 100;
                }
                if (ctx->has_active_abnormal_status(
                        owner, static_cast<int>(AbnormalStatusId::Flammable))) {
                    accuracy = accuracy * 70 / 100;
                }
            }
            const float dodge_chance = ctx->ws.dodge_rate[1 - owner];
            // accuracy_fix_100 跳过必闪减避（"必定闪避词条都无法生效"，2026-09-30 实测口径）。
            const int hit_chance = cred.accuracy_fix_100
                ? accuracy
                : accuracy - static_cast<int>(dodge_chance * 100);
            // 通用特性·虚无：**有概率闪避对手攻击技能**（必修6：本质是闪避、不是挡伤）——
            // 只对攻击技能生效；命中失败照走 miss 出口（含"miss 也消费次数类盔"的既有约定）。
            bool void_miss = false;
            if (!is_attribute) {
                if (const std::optional<EffectiveTrait> v =
                        ctx->effective_common_trait(1 - owner, TraitKind::VoidDodge)) {
                    void_miss = trait_proc_roll(*v);   // ⚠️ rand 只在带虚无时消耗
                }
            }
            if (attr_must_miss || void_miss || (std::rand() % 100) >= hit_chance) {
                // miss 也照常 notify 中心：文档 §2.3「一旦本次技能命中失败（miss 类），
                // 会消耗所有可响应的次数类效果」——狮盔会被响应并消耗，尽管技能是 miss 的。
                ctx->rule_center_.notify(ctx, owner, is_attribute, this->power,
                                         cred.ignore_attack_immunity, ctx->roundCount);
                return SkillUsageResult::MISS;
            }
        }
    }

    // ①.5 暴击判定（用户 2026-09-13 口径）——**判定位在 miss 之后、门判定之前**：
    //   · 只有 **miss** 会阻止暴击（miss 已在上面 return，这里掷不到）；失明的 50% 档不算 miss，
    //     它照样保留暴击结果；
    //   · 技能无效（盔/威/封属）、命中效果失效**都保留**暴击结果——"打在盔上一样可以触发
    //     暴击并且破对应的防御正等级"；
    //   · **只在使用攻击技能时触发**（属性技能不掷）。
    // 为什么必须在这里掷而不是在伤害结算处：技能无效时伤害结算**整段不执行**
    // （handle_*_AttackDamage 因 allowAttackDamagePipeline=false 早退），在那儿掷就永远掷不到。
    // 一次技能使用掷一次（多段/变威力共用结果）。暴击率 = 技能暴击率 × ws.crit_rate_mod；
    // crit_rate=0 的技能**永不必暴**（没有引爆效果就不会暴击）。
    if (!is_attribute) {
        // `ws.must_crit` = "下N回合自身攻击技能**必定**致命一击"（effect 58 圣光气）。
        // ⚠️ 必须单列一个开关：`crit_rate_mod` 是**乘算**修正，技能自身 `crit_rate == 0`
        //    时 `0 × 任何数 = 0`，表达不了"必定"（`crit_rate==0` 就是"永不必暴"）。
        float rate = critical_strike_rate * ctx->ws.crit_rate_mod[owner];
        // **加算通道**（effect 32 蓄气族"+1/16"）：加法百分点，见 battleWorkspace.h 注释。
        // add>0 时 rate>0 → 下方掷点自然解锁（add 是引爆效果，base=0 照样能暴）。
        rate += ctx->ws.crit_rate_add[owner];
        // 通用特性·会心（必修6 ①②）：**0 星（args[0]<=1）的 6.25%（1/16）与技能初始暴击率
        // **加法**结算；**1-5 星是独立二次结算**（"只要有一个触发则当次攻击必定暴击"）。
        // args[0]=75/88/100/120/140 → ×10 即万分数（75→750/10000=7.5%）。
        bool crit_second_roll = false;
        if (const std::optional<EffectiveTrait> h =
                ctx->effective_common_trait(owner, TraitKind::CritBoost)) {
            if (h->args[0] <= 1) {
                rate += 6.25f;   // 0 星：加法（1/16）
            } else {
                crit_second_roll = (std::rand() % 10000) < h->args[0] * 10;   // 独立二次
            }
        }
        ctx->crit_happened[owner] = ctx->ws.must_crit[owner] || crit_second_roll
            || rate >= 100.0f
            || (rate > 0.0f && (std::rand() % 10000) < static_cast<int>(rate * 100.0f));
        // 2380 「自身下{0}次致命一击视为命中并强制执行命中效果」（圣光莫妮卡·繁嚣此寂
        // 29299；技能组 2026-09-25）：暴击落地即消费 1 层计数（plugin_storage 990238000+owner，
        // 技能效果侧武装）→ cred.force_execute——"视为命中"与"强制执行"都由它承载
        // （②.0 强制执行隐含必定命中；③层命中失效/盔/威照常消费但被绕过）。
        if (ctx->crit_happened[owner]) {
            const int charges_key = 990238000 + owner;
            if (ctx->plugin_storage.count(charges_key)) {
                const int left = std::any_cast<int>(ctx->plugin_storage[charges_key]);
                if (left > 0) {
                    ctx->plugin_storage[charges_key] = left - 1;
                    // query_usage 里的 cred 是 const 引用 → 写底层 ws 槽（同一对象）。
                    ctx->ws.attack_credential[owner].force_execute = true;
                }
            }
        }
    }

    // ② 门判定（技能无效中心：盔 / 威 / 封属 / 封属·命中失效）。
    // 文档 §2.3/§5.2 **全部消费**：一次技能使用会消耗**所有**响应它的次数类条目
    // （无效类与命中失效类在同一次遍历中统一消费），不因某个盔挡了另一个就保留次数。
    // 回合类条目响应但不消耗（靠减扣点/断回合结束）。
    // 穿透只绕"可穿盔"：条件盔/龙威（penetrable=false）即使有凭证也照旧被挡。
    // ⚠️ 这里的 HIT_INVALID 来自 `SealKind::SEAL_ATTRIBUTE_HIT`（**封属·命中失效**），
    //    它**天生只响应属性技能**（781 秩序之助原文："令对手使用的**属性技能**无效"），
    //    与 ③层"命中效果失效"（下面 ②.5）**不是同一件事**——后者攻击/属性技能都可能。
    const SkillInvalidNotifyResult nr = ctx->rule_center_.notify(
        ctx, owner, is_attribute, this->power, cred.ignore_attack_immunity, ctx->roundCount,
        // 每条**被结算**的拦截条目（真正生效 或 被穿）都发一次事件，带上 grant_id + blocked。
        // 用途：带后续子句的盔（"触发成功则…"）按 **grant_id** 精确匹配自己那条，
        //   无论生效与否都自删监听器 → **被穿的盔不会留下野监听器**在下次误触发
        //   （否则盔A被穿→监听器残留→盔B生效时 A/B 都触发，子句多执行一次）；
        //   blocked=false（被穿）时不执行子句——"被穿之后子句自然没有了"。
        [ctx, owner](int source_effect_id, int source_owner, int grant_id, bool blocked) {
            BattleEvent ev{EventType::EVENT_SKILL_ARMOR_RESOLVED, source_owner, owner, source_effect_id};
            ev.grant_id = grant_id;
            ev.blocked = blocked;
            ctx->event_center_.emit(ev);
        });

    // ②.0 强制执行（用户 2026-09-13 口径）：盔/威/封属**照常响应与消耗**（上面 notify 已经做了），
    //   但**不让命中拦截生效**——直接返回 OK，让技能效果照常注册；同时把**本次技能的视图威力置 0**，
    //   于是非变威力技能"只有效果、没有红伤"。
    //   变威力类效果（如 2501「技能无效时，**重新进行伤害结算**且…」）会在效果生效时
    //   **重新设置视图威力**并结算伤害管线 → 照样能打出红伤。
    //   （③层命中效果失效仍被强制执行绕过——见 §六"无为觉者·强制执行无视命中效果失效"。）
    if (cred.force_execute) {
        if (nr != SkillInvalidNotifyResult::NONE) {
            ctx->ws.skill_power_view[owner] = 0;
        }
        return SkillUsageResult::OK;
    }
    if (nr == SkillInvalidNotifyResult::INVALID) {
        return SkillUsageResult::SEALED;      // 被无效（盔/威/封属）→ SKILL_INVALID + 补偿
    }
    if (nr == SkillInvalidNotifyResult::HIT_INVALID) {
        return SkillUsageResult::HIT_INVALID; // 封属·命中失效 → 效果失效、无补偿
    }

    // ②.1 **异常驱动的技能无效**（2026-09-18）
    //
    // 走 `SEALED`（→ SKILL_INVALID，**技能无效** + 补偿分支），**不是** `HIT_INVALID`
    // （命中效果失效、不补偿）：官方与语料把两者分得很清（idx=170「**属性技能命中效果失效是封属性，
    // 而属性技能无效是封属**，这一点不要搞错」），用户 2026-09-18 也特别强调"注意是无效不是命中效果失效"。
    // 放在 ②.5 之前：无效比命中失效更强（前者走补偿，后者不走），语义上应先判。
    // ⚠️ 排在 ② 门判定**之后**：故意让盔/威/封属先响应并**消费次数类条目**（与 miss 同约定
    //    "一次技能使用会消耗所有响应它的次数类盔"）；本条只是"没有别的无效时的收口"。
    // ⚠️ 被 `cred.force_execute` 绕过（与失明/盔/威同款：强制执行无视一切无效）。
    if (!cred.force_execute) {
        // 沉默 30：官方 effect_des 30「该状态下精灵**第五技能无效**，且每回合结束后受到…百分比伤害」；
        // 语料 idx=473《机制讲解—龙威》「沉默：**第五效果必定无效**」。
        // 按**替换后**的技能槽判 —— idx=51「骑士对决…转化为第五**会受到沉默影响**」。
        if (ctx->has_active_abnormal_status(owner, static_cast<int>(AbnormalStatusId::Silence))
            && ctx->executing_skill_slot(owner) == kFifthSkillSlot) {
            return SkillUsageResult::SEALED;
        }
        // 失神 29：官方 effect_des 29「该状态下精灵使用**属性技能 50% 无效**」；
        // 语料 idx=473「失神：50 概率属性无效」。
        if (is_attribute
            && ctx->has_active_abnormal_status(owner, static_cast<int>(AbnormalStatusId::Distraction))
            && (std::rand() % 100) < 50) {
            return SkillUsageResult::SEALED;
        }
        // ②.1' **非同系攻击免疫门**（圣光莫妮卡 2492 神谕口径，2026-09-25 待落①落地）：
        // 守方有律武装 → **攻击技能**（属性技能不进门）系别（skill_element_view，上方
        // resolve_skill_execution 的 ON_SKILL_HIT 物化点已按"选择期授予 > 面板"刷新）
        // ∉ 守方系别集（view_elementalAttributes，系别视图）→ 技能无效（SEALED，走补偿趟）。
        // 官方语料口径（调研报告 115/163）：非同系技能免疫 = "攻击系别∈守方系别集才可伤"；
        // 本质是**可穿盔**——攻方凭证 valid（穿透 697/699 ∪ 强制执行）照样穿（!cred.valid 门）。
        // 排在 ②.1 同一收口位：拦截条目已消费、沉默/失神先判（同为"没有别的无效时的收口"）。
        if (!is_attribute && !cred.valid && ctx->ws.nonnative_immune_armed[1 - owner]) {
            const int defender = 1 - owner;
            const int (&sv)[2] = ctx->ws.skill_element_view[owner];
            const int (&dv)[2] = ctx->ws.view_elementalAttributes[defender];
            const bool same_type =
                (sv[0] != 0 && (sv[0] == dv[0] || sv[0] == dv[1]))
                || (sv[1] != 0 && (sv[1] == dv[0] || sv[1] == dv[1]));
            if (!same_type) {
                return SkillUsageResult::SEALED;
            }
        }
    }

    // ②.5 ③层"命中效果失效"（防御方按次挂载）：**按技能类型分别消费**。
    // ⚠️ 用户 2026-09-13 口径：命中失效**不是属性技能专用**，攻击技能同样会被失效，
    //    所以 RuleCenter 里拆成两个类别（HIT_INVALID_ATTACK / HIT_INVALID_ATTRIBUTE），
    //    本次是攻击技能就消费攻击那条、属性技能就消费属性那条；要"两种都失效"就注册两条。
    //    消费点收口在本函数末尾（而不是 execute 里各判一次），与 miss/sealed 同一处出结果。
    //    强制执行由 `cred.force_execute` 绕过（在 is_hit_effect_invalid 里判）。
    if (!cred.force_execute) {
        const std::optional<int> mode = ctx->rule_center_.consume_hit_invalid(1 - owner, is_attribute);
        if (mode.has_value()) {
            ctx->ws.hit_invalid_detected[owner] = true;
            ctx->ws.hit_invalid_mode[owner] = *mode;   // 供伤害路径判 kFullNull（白板归零）
            return SkillUsageResult::HIT_INVALID;
        }
    }

    // ②.6 「攻击技能无法造成伤害且命中效果失效」票（effect 1090 族，ATTACK_NULLIFY，
    // 2026-09-21 用户口径）：**生效目标**一侧出招 → 命中失效（kFullNull 白板：效果不注册
    // + 红伤归零）。红伤归零点在 finish_attack_damage（伤害管线**之前** → 护盾/护罩不
    // 消耗）；该攻击的**粉伤**在 deal_pink_damage 入口同样被封锁（红粉双封，强制执行
    // 的粉伤效果也不生效）；真伤不封（独立通道）。
    // ⚠️ 与 ③层 同款：**强制执行绕过**——无相谛（威力 0）的强制断回合正是靠这条在
    //    第一趟结算里先拆掉缔笙挂在自己身上的光环（SKILL_EFFECT 时点删票），随后
    //    变威力重结算时票已不在 → 正常伤害（finish_attack_damage 查不到票）。
    if (!cred.force_execute) {
        if (ctx->rule_center_.is_attack_nullified(owner, ctx->roundCount)) {
            ctx->ws.hit_invalid_detected[owner] = true;
            ctx->ws.hit_invalid_mode[owner] = static_cast<int>(HitInvalidMode::kFullNull);
            return SkillUsageResult::HIT_INVALID;
        }
    }

    // 失明的 50% 档：命中效果失效（无挂载条目可消费，纯异常效果）。
    if (blind_hit_invalid) {
        ctx->ws.hit_invalid_detected[owner] = true;
        ctx->ws.hit_invalid_mode[owner] = static_cast<int>(HitInvalidMode::kEffectsOnly);
        return SkillUsageResult::HIT_INVALID;
    }

    return SkillUsageResult::OK;
}

Effect Skills::clone_effect(int effectId, EffectArgs args) const {
    return EffectFactory::getInstance().getEffect(effectId, std::move(args));
}

void Skills::add_effect_node(SkillExecResult result, SkillEffectNode node) {
    effectBranches[result].push_back(std::move(node));
}

// ================================================================
// 以下内容原在 src/effects/continuousEffect.cpp —— 该文件 229 行里
// ContinuousEffect:: 的实现是 0 个（全在头文件内联），实际装的全是 Skills:: 的方法。
// 文件名与内容无关，故整体搬入本文件（skills.cpp），使 Skills 的实现集中一处；
// continuousEffect.cpp 随之删除。
// 内容：Skills::execute（执行期主流程）/ query_usage（命中+门判定+③层消费）/
//       register_branch（分支注册 + 逐节点 nullify 过滤）
// ================================================================

namespace {

SkillResolutionFlags resolution_flags_for(SkillExecResult result) {
    switch (result) {
        case SkillExecResult::HIT:
            return SkillResolutionFlags{true, true};
        case SkillExecResult::SKILL_INVALID:
            return SkillResolutionFlags{true, false};
        case SkillExecResult::EFFECT_INVALID:
            return SkillResolutionFlags{false, true};
        default:
            return SkillResolutionFlags{};
    }
}

int first_mover_id(const BattleContext* ctx) {
    if (!ctx) {
        return 0;
    }
    if (ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2) {
        return 1;
    }
    return 0;
}

State state_for_owner(State state, int owner, const BattleContext* ctx) {
    const bool owner_is_first = owner == first_mover_id(ctx);
    if (owner_is_first) {
        return state;
    }

    switch (state) {
        case State::BATTLE_FIRST_ON_SKILL_HIT:
            return State::BATTLE_SECOND_ON_SKILL_HIT;
        case State::BATTLE_FIRST_SKILL_EFFECT:
            return State::BATTLE_SECOND_SKILL_EFFECT;
        case State::BATTLE_FIRST_ATTACK_DAMAGE:
            return State::BATTLE_SECOND_ATTACK_DAMAGE;
        case State::BATTLE_FIRST_AFTER_ACTION:
            return State::BATTLE_SECOND_AFTER_ACTION;
        case State::BATTLE_FIRST_ACTION_END:
            return State::BATTLE_SECOND_ACTION_END;
        case State::BATTLE_FIRST_AFTER_ACTION_END:
            return State::BATTLE_SECOND_AFTER_ACTION_END;
        // 额外行动（2026-09-18）：之诗子效果（2086~2090）就是注册到这个时点的
        // （见 effect_register_state），后出手方必须镜像到 SECOND 侧，否则两个
        // owner 的节点会挤在 FIRST 桶里、后手方那一条永不执行。
        case State::BATTLE_FIRST_EXTRA_ACTION:
            return State::BATTLE_SECOND_EXTRA_ACTION;
        default:
            return state;
    }
}

void bind_participants(Effect& effect, int owner) {
    if (effect.args.owned_int_args.size() < 2) {
        return;
    }
    effect.args.owned_int_args[0] = owner;
    effect.args.owned_int_args[1] = 1 - owner;
    effect.args.refresh_views();
}

/**
 * 把 Effect.left_round 转成 ContinuousEffect 的持续回合数。
 *
 * - left_round < 0：永久效果（从不按回合过期）。
 * - left_round == 0：一次性效果（技能效果默认值）。若直接以 0 作为
 *   duration_rounds_，isExpired 会算出"当前回合 - 注册回合 >= 0"= 立即过期，
 *   导致效果永不执行。这里归一化为 1：本回合有效，后续回合被 isExpired 拦截
 *   不重放，并在下个回合扣减点被 cleanup 移除。
 * - left_round > 0：按原值（N 回合的回合类效果）。
 */
int duration_for_effect(int left_round) {
    if (left_round < 0) return -1;
    if (left_round == 0) return 1;
    return left_round;
}

} // namespace

// Skills::execute — 技能执行期主流程（替代 SkillExecutionEffect）
//
// 流程：query_usage 判可用性（miss/封属性/封攻击）→
//       命中效果失效判定（EFFECT_INVALID，不注册任何效果）→
//       命中（注册 HIT 分支）→ 各分支注册效果到对应时点桶。
std::pair<SkillExecResult, SkillResolutionFlags> Skills::execute(BattleContext* ctx, int owner, State trigger_state) {
    (void)trigger_state;
    if (!ctx || owner < 0 || owner > 1) {
        return {SkillExecResult::SKILL_INVALID, resolution_flags_for(SkillExecResult::SKILL_INVALID)};
    }

    // E13"使用即触发"锚点（2026-09-29）：技能被**选择使用**就发——miss/被封属/打盔
    // 也算"使用"（469"若对手使用属性技能"字面；与 EVENT_HIT 的"命中才发"正交）。
    // target = 对方；used_attribute 标记属性技（消费方：神离之始 469 一族）。
    {
        BattleEvent used_ev{EventType::EVENT_SKILL_USED, owner, ctx->opponent(owner)};
        used_ev.used_attribute = (type == SkillType::Attribute);
        ctx->event_center_.emit(used_ev);
    }

    // 执行期可用性判定（统一走 query_usage：miss + 封属性/封攻击/命中失效）
    const SkillUsageResult usage = query_usage(ctx, owner);
    if (usage == SkillUsageResult::MISS || usage == SkillUsageResult::SEALED) {
        const SkillResolutionFlags flags = resolution_flags_for(SkillExecResult::SKILL_INVALID);
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_SKILL_INVALID, owner, ctx->opponent(owner)});
        register_branch(ctx, owner, SkillExecResult::SKILL_INVALID, flags);
        // 固有效果（INHERENT）：miss/被封属/盔挡的这趟**照常注册**（用户 2026-09-20 口径）
        register_branch(ctx, owner, SkillExecResult::INHERENT, flags);
        return {SkillExecResult::SKILL_INVALID, flags};
    }
    // 成功使用攻击技能 → 统一消费次数型穿透授予（"下一次攻击"语义：即使对手无阻挡也消费）。
    // miss/sealed 已在上方提前 return 不消费；属性技能无攻击语义不消费。
    // 落在**命中失效分支之前**：③层失效（下面那条 HIT_INVALID）也算"成功使用"→ 也消费
    // （原先 ③层 在更靠后的位置处理，消费顺序就是这样的）。
    // ② 门判定那条 HIT_INVALID（SEAL_ATTRIBUTE_HIT）只对**属性技能**响应，这里 `type != Attribute`
    // 天然为假 → 行为与改动前一致（它从来不消费）。
    if (type != SkillType::Attribute) {
        ctx->consume_penetration_grants_after_attack(owner);
        ctx->consume_attack_boost_grants_after_attack(owner);  // 次数型攻击增伤同步消费（"下1次攻击"语义）
    }

    // 命中失效：技能照常命中，但效果不被注册、不触发 SKILL_INVALID 补偿。两个来源合流到这条：
    //   ① ② 门判定的 `SEAL_ATTRIBUTE_HIT`（封属·命中失效，**只可能是属性技能**）；
    //   ② ③层"命中效果失效"（防御方按次挂载，攻击/属性都可能）与失明的 50% 档
    //      —— 由 `query_usage` ②.5 消费后返回（`ws.hit_invalid_detected` 置位）。
    // 两者都走同一执行路径（逐节点 nullify 过滤 + EVENT_HIT），
    // **不 emit EVENT_SKILL_INVALID**、不注册无效补偿分支。
    if (usage == SkillUsageResult::HIT_INVALID) {
        // ③层白板模式（kFullNull）：命中效果失效 + 伤害归 0（ATTACK_DAMAGE 阶段读该标记）。
        if (ctx->ws.hit_invalid_detected[owner]
            && static_cast<HitInvalidMode>(ctx->ws.hit_invalid_mode[owner]) == HitInvalidMode::kFullNull) {
            ctx->ws.hit_invalid_zero_damage[owner] = true;
        }
        const SkillResolutionFlags flags = resolution_flags_for(SkillExecResult::HIT);
        register_branch(ctx, owner, SkillExecResult::HIT, flags, /*filter_hit_invalid=*/true);
        // 固有效果照常注册，但沿用同一张 nullify 过滤——逐节点 hit_effect_invalidatable
        // 就是"该效果可被封"的元数据开关（未被标记的固有效果在此趟不受影响）。
        register_branch(ctx, owner, SkillExecResult::INHERENT, flags, /*filter_hit_invalid=*/true);
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_HIT, owner, ctx->opponent(owner)});
        return {SkillExecResult::HIT, flags};
    }

    const SkillResolutionFlags flags = resolution_flags_for(SkillExecResult::HIT);
    register_branch(ctx, owner, SkillExecResult::HIT, flags);
    register_branch(ctx, owner, SkillExecResult::INHERENT, flags);

    // 技能命中事件（"技能命中后/受到攻击后"监听；属性技能也算命中，但无伤害量）
    ctx->event_center_.emit(BattleEvent{EventType::EVENT_HIT, owner, ctx->opponent(owner)});

    return {SkillExecResult::HIT, flags};
}

void Skills::register_branch(BattleContext* ctx, int owner, SkillExecResult result,
                             const SkillResolutionFlags& flags, bool filter_hit_invalid) {
    if (!flags.registerSkillEffects) {
        return;
    }

    auto it = effectBranches.find(result);
    if (it == effectBranches.end()) return;

    for (const SkillEffectNode& node : it->second) {
        Effect effect = node.effect;
        bind_participants(effect, owner);
        if (!effect.logic) continue;
        // ③层：命中效果失效时跳过可否决节点（逐效果 nullify 标签，非整技能一刀切）
        if (filter_hit_invalid) {
            const EffectMeta* meta = EffectMetaCatalog::instance().find(effect.id);
            if (meta && meta->nullify.hit_effect_invalidatable) {
                continue;
            }
        }
        // 单元准入门（注册期）：条件效果单元逐条问一次魂印侧的门。放行 → **本条换成
        // "跳过条件"的执行器**（技能对象只读，"已永久无条件"由魂印侧的进度表达）。
        // 见 plugin_interface.h 的 UnitAdmissionFn（万相乖离的取消进度）。
        if (effect.logic == &effect_run_parsed_unit && effect.args.extra) {
            const auto* unit = static_cast<const EffectUnit*>(effect.args.extra);
            if (unit >= parsed_units_.data() && unit < parsed_units_.data() + parsed_units_.size()) {
                const int unit_index = static_cast<int>(unit - parsed_units_.data());
                if (unit_admission_grants(ctx, owner, id, unit_index,
                                          static_cast<int>(unit->condition))) {
                    effect.logic = &effect_run_parsed_unit_unconditional;
                }
            }
        }

        const State register_state = state_for_owner(node.registerState, owner, ctx);
        // one-shot (left_round==0) 归一化为本回合有效的 1 回合效果，否则立即过期永不执行
        const int duration = duration_for_effect(effect.left_round);
        // 回合数窗口：官方口径的**生效起点**（"N回合内"后出手顺延 / "下N回合"一律从下回合起算）。
        // ⚠️ 只折算**真正的窗口效果**（left_round > 0，效果本身持续 N 回合）；
        //    left_round==0 的"本回合一次性动作节点"（duration 归一化为 1）**不挪**——
        //    否则"下N回合"技能的**执行节点**会被推到下一回合才跑（动作迟到一拍）。
        //    插件自己 register 的多回合效果（如 843 的 MOVE_RIGHT 条目）由插件用同一个
        //    `round_effect_start_round` 折算（家族经 CoreApi::effect_window_kind 查）。
        const EffectMeta* win_meta = EffectMetaCatalog::instance().find(effect.id);
        const EffectWindowKind window_kind = win_meta ? win_meta->window : EffectWindowKind::InRounds;
        const int start_round = effect.left_round > 0
            ? ctx->round_effect_start_round(owner, duration, window_kind)
            : ctx->roundCount;

        ctx->registerEffect(
            register_state,
            owner,
            std::make_unique<ContinuousEffect>(
                effect,
                register_state,
                owner,
                duration,
                ctx->roundCount
            )
        );
    }
}

