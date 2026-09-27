#include <fsm/battle_mutate.h>

#include <algorithm>
#include <cstdio>

#include <abnormal-system/abnormal-types.h>

namespace battle_mutate {
namespace {

Outcome fail(const std::string& why) {
    return Outcome{false, "", why};
}

Outcome note_out(const std::string& note) {
    return Outcome{true, note, ""};
}

bool side_ok(int side) {
    return side >= 0 && side <= 1;
}

bool slot_ok(int slot) {
    return slot >= 0 && slot < 6;
}

std::string side_name(int side) {
    return side == 0 ? "p0" : "p1";
}

Outcome valid_target(BattleContext& ctx, int side, int slot) {
    if (!side_ok(side) || !slot_ok(slot)) {
        return fail("side/slot out of range");
    }
    return Outcome{true, "", ""};
}

}  // namespace

Outcome set_hp(BattleContext& ctx, int side, int slot, int value) {
    if (Outcome v = valid_target(ctx, side, slot); !v.ok) {
        return v;
    }
    ElfPet& pet = ctx.seerRobot[side].elfPets[slot];
    const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    const int clamped = std::clamp(value, 0, max_hp);
    const int before = pet.hp;
    pet.hp = clamped;
    return note_out(side_name(side) + " " + std::to_string(slot + 1) + "号位 体力 " +
                    std::to_string(before) + " → " + std::to_string(clamped) + "/" +
                    std::to_string(max_hp));
}

Outcome set_pp(BattleContext& ctx, int side, int slot, int skill, int value) {
    if (Outcome v = valid_target(ctx, side, slot); !v.ok) {
        return v;
    }
    if (skill < 0 || skill >= 5) {
        return fail("skill index out of range");
    }
    Skills& sk = ctx.seerRobot[side].elfPets[slot].skills[static_cast<std::size_t>(skill)];
    const int clamped = std::clamp(value, 0, sk.maxPP);
    const int before = sk.pp;
    sk.pp = clamped;
    return note_out(side_name(side) + " " + std::to_string(slot + 1) + "号位 技能" +
                    std::to_string(skill + 1) + "(" + sk.name + ") PP " +
                    std::to_string(before) + " → " + std::to_string(clamped) + "/" +
                    std::to_string(sk.maxPP));
}

Outcome set_level(BattleContext& ctx, int side, int stat, int value) {
    if (!side_ok(side)) {
        return fail("side out of range");
    }
    if (stat < 0 || stat >= BattleContext::kAbilityLevelSlotCount) {
        return fail("stat index out of range");
    }
    const int clamped = std::clamp(value, -6, 6);
    const int before = ctx.ability_levels[side][stat];
    ctx.ability_levels[side][stat] = clamped;
    return note_out(side_name(side) + " 等级[" + std::to_string(stat) + "] " +
                    std::to_string(before) + " → " + std::to_string(clamped));
}

Outcome apply_anomaly(BattleContext& ctx, int side, int id, int rounds) {
    if (!side_ok(side) || id < 0 || id >= kOfficialAbnormalStatusSlotCount) {
        return fail("side/anomaly id out of range");
    }
    if (rounds < 1) {
        return fail("rounds must be >= 1");
    }
    const int end_round = ctx.roundCount + rounds;
    const bool had = ctx.abnormal_status_end_round[side][id] > ctx.roundCount;
    const int before_remaining =
        had ? ctx.abnormal_status_end_round[side][id] - ctx.roundCount : 0;
    ctx.abnormal_status_end_round[side][id] = end_round;
    return note_out(side_name(side) + " 异常[" + std::to_string(id) + " " +
                    abnormal_status_name_cn(id) + "] " +
                    (had ? "剩余" + std::to_string(before_remaining) + "回合 → " : "→ ") +
                    std::to_string(rounds) + "回合");
}

Outcome cure_anomaly(BattleContext& ctx, int side, int id) {
    if (!side_ok(side)) {
        return fail("side out of range");
    }
    if (id < 0) {
        int cured = 0;
        for (int ab = 0; ab < kOfficialAbnormalStatusSlotCount; ++ab) {
            if (ctx.abnormal_status_end_round[side][ab] > ctx.roundCount) {
                ctx.abnormal_status_end_round[side][ab] = 0;
                ++cured;
            }
        }
        return note_out(side_name(side) + " 解除全部异常（" + std::to_string(cured) + " 条）");
    }
    if (id >= kOfficialAbnormalStatusSlotCount) {
        return fail("anomaly id out of range");
    }
    const bool had = ctx.abnormal_status_end_round[side][id] > ctx.roundCount;
    ctx.abnormal_status_end_round[side][id] = 0;
    return note_out(side_name(side) + " 解除异常[" + std::to_string(id) + " " +
                    abnormal_status_name_cn(id) + "]" + (had ? "" : "（原本没有）"));
}

Outcome attach_mark(BattleContext& ctx, int src_side, int dst_side) {
    if (!side_ok(src_side) || !side_ok(dst_side)) {
        return fail("side out of range");
    }
    const int src_slot = ctx.on_stage[src_side];
    const int dst_slot = ctx.on_stage[dst_side];
    if (!slot_ok(src_slot) || !slot_ok(dst_slot)) {
        return fail("on-stage slot invalid");
    }
    const std::size_t before = ctx.hp_consume_marks.size();
    ctx.attach_hp_consume_mark(src_side, src_slot, dst_side, dst_slot);
    if (ctx.hp_consume_marks.size() == before) {
        return fail("印记未挂上（幂等去重或目标已倒）");
    }
    return note_out("死亡印记：p" + std::to_string(src_side) + " " +
                    std::to_string(src_slot + 1) + "号位 → p" + std::to_string(dst_side) +
                    " " + std::to_string(dst_slot + 1) + "号位");
}

Outcome clear_marks(BattleContext& ctx, int side) {
    if (!side_ok(side)) {
        return fail("side out of range");
    }
    const int slot = ctx.on_stage[side];
    if (!slot_ok(slot)) {
        return fail("on-stage slot invalid");
    }
    const std::size_t before = ctx.hp_consume_marks.size();
    ctx.hp_consume_marks.erase(
        std::remove_if(ctx.hp_consume_marks.begin(), ctx.hp_consume_marks.end(),
                       [side, slot](const BattleContext::HpConsumeMark& m) {
                           return m.target_side == side && m.target_slot == slot;
                       }),
        ctx.hp_consume_marks.end());
    return note_out("清除死亡印记 ×" + std::to_string(before - ctx.hp_consume_marks.size()) +
                    "（p" + std::to_string(side) + " " + std::to_string(slot + 1) + "号位）");
}

}  // namespace battle_mutate
