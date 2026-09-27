#include <effects/effect_unit_parser.h>

#include <cctype>
#include <cstring>

namespace {

// 在 text 中从 from 起找第一个 "{n}"，返回占位符下标 n；找不到返回 -1。
int find_placeholder(const std::string& text, std::size_t from) {
    const std::size_t open = text.find('{', from);
    if (open == std::string::npos) {
        return -1;
    }
    const std::size_t close = text.find('}', open);
    if (close == std::string::npos || close == open + 1) {
        return -1;
    }
    bool digits = true;
    for (std::size_t j = open + 1; j < close; ++j) {
        if (!std::isdigit(static_cast<unsigned char>(text[j]))) {
            digits = false;
            break;
        }
    }
    if (!digits) {
        return -1;
    }
    return std::stoi(text.substr(open + 1, close - open - 1));
}

// 取 skill_args[placeholder]（越界返回 -1）。
int arg_at(const std::vector<int>& skill_args, int placeholder) {
    if (placeholder < 0 || static_cast<std::size_t>(placeholder) >= skill_args.size()) {
        return -1;
    }
    return skill_args[placeholder];
}

// 找 text[0..end) 中最后一个 "{n}%" 的占位符下标；无则 -1。
int find_last_percent_placeholder(const std::string& text, std::size_t end) {
    int last = -1;
    std::size_t i = 0;
    while (i < end) {
        const std::size_t pct = text.find('%', i);
        if (pct == std::string::npos || pct >= end) {
            break;
        }
        if (pct >= 2 && text[pct - 1] == '}') {
            const std::size_t open = text.rfind('{', pct - 2);
            if (open != std::string::npos && open < pct - 1) {
                bool digits = true;
                for (std::size_t j = open + 1; j < pct - 1; ++j) {
                    if (!std::isdigit(static_cast<unsigned char>(text[j]))) {
                        digits = false;
                        break;
                    }
                }
                if (digits) {
                    last = std::stoi(text.substr(open + 1, pct - 1 - open - 1));
                }
            }
        }
        i = pct + 1;
    }
    return last;
}

// 解析 StatusInflict 主句："令/使对手/自身{状态}"。
// 填 unit（Anomaly 主动作 + chance_value + param0=anomaly_id + param1=2），
// consumed_end = 状态占位符 '}' 之后。失败返回 false。
bool parse_status_inflict_primary(const std::string& text, const std::vector<int>& skill_args,
                                  EffectUnit& unit, std::size_t& consumed_end) {
    static const char* kPatterns[] = {"令对手{", "令自身{", "使对手{", "使自身{"};
    for (const char* pat : kPatterns) {
        const std::size_t pos = text.find(pat);
        if (pos == std::string::npos) {
            continue;
        }
        // '{' 在 pattern 末尾（中文字符按 UTF-8 多字节，用 strlen 定位）。
        const std::size_t brace_pos = pos + std::strlen(pat) - 1;
        const int status_ph = find_placeholder(text, brace_pos);
        if (status_ph < 0) {
            continue;
        }
        const std::size_t close = text.find('}', brace_pos);
        // ⚠️ 占位符后紧跟"回合" → 它是**回合数**，不是异常名：
        //   "令对手{1}回合内属性技能无效"（2321 族）的 {1}=2 是时长，误当异常 id 施加
        //   就会把烧伤(2)挂给对手（2026-09-27 堵煞事故）。这类"回合窗口"模板各有
        //   专有实现/待做，本解析器不得接手。
        if (close != std::string::npos
            && text.compare(close + 1, std::strlen("回合"), "回合") == 0) {
            continue;
        }
        unit.primary_tag = PrimitiveTag::Anomaly;
        unit.target = (std::strstr(pat, "自身") != nullptr) ? 0 : 1;
        unit.param0 = arg_at(skill_args, status_ph);
        unit.param1 = 2;  // 默认持续 2 回合
        unit.chance_value = (find_last_percent_placeholder(text, pos) >= 0)
            ? arg_at(skill_args, find_last_percent_placeholder(text, pos))
            : -1;
        consumed_end = close != std::string::npos ? close + 1 : text.size();
        return true;
    }
    return false;
}

// 递归解析兜底子句，追加到 storage，返回其根下标；不支持返回 -1。
int parse_fallback_unit(const std::string& text, const std::vector<int>& skill_args,
                        std::vector<EffectUnit>& storage) {
    EffectUnit unit;
    // 1) 递归异常：令/使对手/自身{状态}
    std::size_t consumed = 0;
    if (parse_status_inflict_primary(text, skill_args, unit, consumed)) {
        storage.push_back(unit);
        return static_cast<int>(storage.size()) - 1;
    }
    // 2) 恢复：恢复自身最大体力...1/{n} 或 全部
    if (text.find("恢复") != std::string::npos && text.find("自身") != std::string::npos) {
        unit.primary_tag = PrimitiveTag::Heal;
        unit.target = 0;  // 自身
        const std::size_t frac = text.find("1/");
        if (frac != std::string::npos) {
            unit.param0 = arg_at(skill_args, find_placeholder(text, frac));
        } else {
            unit.param0 = 0;  // fraction_denom<=0 → 恢复全部
        }
        storage.push_back(unit);
        return static_cast<int>(storage.size()) - 1;
    }
    // 3) 附加{n}点固定伤害
    if (text.find("附加") != std::string::npos && text.find("固定伤害") != std::string::npos) {
        unit.primary_tag = PrimitiveTag::FixedDamage;
        unit.target = 1;  // 对手
        unit.param0 = arg_at(skill_args, find_placeholder(text, text.find("附加") + 2));
        storage.push_back(unit);
        return static_cast<int>(storage.size()) - 1;
    }
    return -1;
}

// 第二刀：无相谛 5 类条件模板（"条件 → 动作"结构，一次填好 condition + primary_tag）。
// 匹配失败返回 false（维持跳过）。
bool parse_second_pass_unit(const std::string& text, const std::vector<int>& skill_args,
                            EffectUnit& unit) {
    // 1) 179：若属性相同则技能威力提升{n} → SameElement + PowerBoost
    if (text.find("若属性相同") != std::string::npos
        && text.find("威力提升") != std::string::npos) {
        unit.primary_tag = PrimitiveTag::PowerBoost;
        unit.condition = UnitCondition::SameElement;
        unit.target = 1;  // 条件比较双方元素；动作作用于 actor（自身技能威力）
        unit.param0 = arg_at(skill_args, find_placeholder(text, 0));
        return unit.param0 > 0;
    }
    // 2) 700：先出手时降低对手所有PP{n}点 → FirstMove + PpReduce
    if (text.find("先出手") != std::string::npos
        && text.find("降低对手所有PP") != std::string::npos) {
        unit.primary_tag = PrimitiveTag::PpReduce;
        unit.condition = UnitCondition::FirstMove;
        unit.target = 1;
        unit.param0 = arg_at(skill_args, find_placeholder(text, 0));
        return unit.param0 > 0;
    }
    // 3) 1083：若后出手则消除对手回合类效果 → SecondMove + RemoveRoundEffects
    if (text.find("若后出手") != std::string::npos
        && text.find("消除对手回合类") != std::string::npos) {
        unit.primary_tag = PrimitiveTag::RemoveRoundEffects;
        unit.condition = UnitCondition::SecondMove;
        unit.target = 1;
        return true;
    }
    // 4) 1257：对手不处于异常状态则吸取对手最大体力的1/{n} → TargetNoAnomaly + DrainHp
    if (text.find("不处于异常状态") != std::string::npos
        && text.find("吸取对手最大体力") != std::string::npos) {
        unit.primary_tag = PrimitiveTag::DrainHp;
        unit.condition = UnitCondition::TargetNoAnomaly;
        unit.target = 1;
        const std::size_t frac = text.find("1/");
        if (frac != std::string::npos) {
            unit.param0 = arg_at(skill_args, find_placeholder(text, frac));
        }
        return unit.param0 > 0;
    }
    // 5) 456：若对手体力不足{n}则直接秒杀 → TargetHpBelow + Kill
    if (text.find("若对手体力不足") != std::string::npos
        && text.find("直接秒杀") != std::string::npos) {
        unit.primary_tag = PrimitiveTag::Kill;
        unit.condition = UnitCondition::TargetHpBelow;
        unit.target = 1;
        unit.condition_param = arg_at(skill_args, find_placeholder(text, text.find("若对手体力不足")));
        return unit.condition_param > 0;
    }
    return false;
}

}  // namespace

int parse_effect_unit(const std::string& info, const std::vector<int>& skill_args,
                      std::vector<EffectUnit>& storage) {
    EffectUnit primary;
    std::size_t consumed = 0;
    if (parse_status_inflict_primary(info, skill_args, primary, consumed)) {
        // 1248 类"对手处于异常状态时{n}%令对手{状态}"：补前置条件（与 1257"无异常吸取"互斥）。
        if (info.find("对手处于异常状态") != std::string::npos) {
            primary.condition = UnitCondition::TargetHasAnomaly;
        }
        storage.push_back(primary);
        const int root_idx = static_cast<int>(storage.size()) - 1;

        // 未触发兜底：切 "，[若]未触发则" 之后的子句（"若未触发则" 含 "未触发则"）。
        const std::string remainder = info.substr(consumed);
        const std::size_t wf = remainder.find("未触发则");
        if (wf != std::string::npos) {
            const std::string fallback_text = remainder.substr(wf + 4);
            const int fallback_idx = parse_fallback_unit(fallback_text, skill_args, storage);
            if (fallback_idx >= 0) {
                storage[root_idx].on_other = &storage[fallback_idx];  // 概率未触发 → 兜底
            }
        }
        return root_idx;
    }

    // 第二刀：5 类条件模板（无相谛 179/700/1083/1257/456）。
    if (parse_second_pass_unit(info, skill_args, primary)) {
        storage.push_back(primary);
        return static_cast<int>(storage.size()) - 1;
    }
    return -1;
}
