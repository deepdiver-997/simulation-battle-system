#include <server/legal_actions.h>

#include <entities/seer-robot.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>

namespace server {
namespace {

const char* selection_reason(SkillSelectionResult r) {
    switch (r) {
        case SkillSelectionResult::SELECTABLE: return "";
        case SkillSelectionResult::PP_EMPTY: return "pp_empty";
        case SkillSelectionResult::LOCKED: return "locked";
    }
    return "unknown";
}

}  // namespace

bool player_must_switch_pet(BattleContext& ctx, int player) {
    if (player < 0 || player > 1) {
        return false;
    }
    if (ctx.currentState != State::CHOOSE_AFTER_DEATH) {
        return false;
    }
    const int slot = ctx.on_stage[player];
    if (slot < 0 || slot >= 6) {
        return false;
    }
    return ctx.seerRobot[player].elfPets[slot].hp <= 0;
}

bool has_any_legal_action(const LegalActions& legal) {
    if (!legal.can_act) {
        return false;
    }
    // 必须换宠时只看精灵：FSM 在 CHOOSE_AFTER_DEATH 只接受 CHOOSE_PET
    // （computed legal 里技能/药剂那档已按 must_choose_pet 标成不可用）。
    for (const auto& p : legal.pets) {
        if (p.usable) {
            return true;
        }
    }
    if (legal.must_choose_pet) {
        return false;
    }
    for (const auto& s : legal.skills) {
        if (s.usable) {
            return true;
        }
    }
    for (const auto& m : legal.medicines) {
        if (m.usable) {
            return true;
        }
    }
    return false;
}

LegalActions compute_legal_actions(BattleContext& ctx, int player) {
    LegalActions out;
    if (player < 0 || player > 1) {
        out.reason = "bad_player";
        return out;
    }

    // 与 BattleContext::need_input 同一口径：is_empty 表示"正等着输入"。
    const bool waiting = ctx.is_empty
        && (ctx.currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT
            || ctx.currentState == State::CHOOSE_AFTER_DEATH);

    if (!waiting) {
        out.reason = "not_waiting";
        return out;
    }

    // ⚠️ 两个时点的"轮到谁"判据**不同**，不能都用 current_player_id_：
    //   - 选技能期：就是 current_player_id_（引擎在一方选完后 set_current_player 切另一方）。
    //   - 死亡换宠期：谁**自己的场上精灵倒了**谁换。current_player_id_ 这时可能指向另一方
    //     （引擎的 need_input 只看"有一方倒了"），照着它提示会让客户端去换健康那一侧的精灵，
    //     结果精灵来回切、败方永远不换、对局打不完。
    if (ctx.currentState == State::CHOOSE_AFTER_DEATH) {
        if (!player_must_switch_pet(ctx, player)) {
            out.reason = "your_on_stage_pet_is_alive";
            return out;
        }
    } else if (ctx.current_player_id_ != player) {
        out.reason = "not_your_turn";
        return out;
    }

    const int slot = ctx.on_stage[player];
    if (slot < 0 || slot >= 6) {
        out.reason = "no_on_stage_pet";
        return out;
    }
    ElfPet& pet = ctx.seerRobot[player].elfPets[slot];

    out.can_act = true;
    out.must_choose_pet = (ctx.currentState == State::CHOOSE_AFTER_DEATH);

    // 技能：两个门槛都要过 —— FSM 的 operation() 先用 skill_usable 做第一道闸，
    // 通过后再用 query_selectable 做"PP/锁定"第二道（不过会回"请重选"）。
    // 这里如实报告两道闸的结果，前端才不会给出会被拒的按钮。
    //
    // ⚠️ 死亡换宠时点（CHOOSE_AFTER_DEATH）FSM 只接受 CHOOSE_PET，发技能会回
    //    "Error: must choose pet"。所以这一档把技能/药剂一律标成不可用 ——
    //    否则合法动作集等于在邀请客户端发一个注定被拒的动作。
    for (int i = 0; i < 5; ++i) {
        Skills& sk = pet.skills[i];
        LegalSkill s;
        s.index = i;
        s.id = sk.id;
        s.name = sk.name;
        s.pp = sk.pp;
        s.max_pp = sk.maxPP;

        if (out.must_choose_pet) {
            s.usable = false;
            s.reason = "must_choose_pet";
        } else if (!sk.skill_usable(&ctx, player)) {
            s.usable = false;
            s.reason = "unusable";
        } else {
            const SkillSelectionResult sel = sk.query_selectable(&ctx, player);
            s.usable = (sel == SkillSelectionResult::SELECTABLE);
            s.reason = selection_reason(sel);
        }
        out.skills.push_back(std::move(s));
    }

    // 换宠：与 BattleFsm::operation 的 CHOOSE_PET 分支同口径（hp > 0 且未被锁）。
    // 切到当前场上精灵在引擎里是合法的（no-op），所以不标成不可用，只给 on_stage 让前端自己灰掉。
    for (int i = 0; i < 6; ++i) {
        const ElfPet& p = ctx.seerRobot[player].elfPets[i];
        LegalPet lp;
        lp.slot = i;
        lp.id = p.id;
        lp.name = p.name;
        lp.hp = p.hp;
        lp.max_hp = p.numericalBase[NumericalPropertyIndex::HP];
        lp.on_stage = (i == slot);
        if (p.hp <= 0) {
            lp.usable = false;
            lp.reason = "fainted";
        } else if (p.is_locked) {
            lp.usable = false;
            lp.reason = "locked";
        } else {
            lp.usable = true;
        }
        out.pets.push_back(std::move(lp));
    }

    // 药剂：与 SeerRobot::use_medicine 同口径（数量 > 0；精灵已死不能嗑）。
    // 死亡换宠时点同样一律不可用（FSM 只收 CHOOSE_PET）。
    const SeerRobot& robot = ctx.seerRobot[player];
    for (int i = 0; i < MEDICINES_SIZE; ++i) {
        LegalMedicine lm;
        lm.index = i;
        lm.type = i;
        lm.count = robot.medicines[i];
        lm.usable = !out.must_choose_pet && (robot.medicines[i] > 0) && (pet.hp > 0);
        out.medicines.push_back(std::move(lm));
    }

    return out;
}

}  // namespace server
