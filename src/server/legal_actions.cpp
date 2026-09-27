#include <server/legal_actions.h>

#include <db/official_data_repository.h>
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

    // 没轮到 / 没在等输入 → 预提交视角：列表照给（当下快照），can_act=false。
    // 调试台要"先给对手交换宠、再给自己交"这类自由顺序 —— 服务端的 pending
    // 队列本来就会把提前交的动作排到轮到时再派发（见 Room::submit_action）。
    bool prospective = false;
    if (!waiting) {
        prospective = true;
    } else if (ctx.currentState == State::CHOOSE_AFTER_DEATH) {
        // ⚠️ 死亡换宠期只等"场上精灵倒了"的那一方，判据不能都用 current_player_id_：
        //   谁的宠物倒了谁换，照 current_player_id_ 提示会让健康一侧去换、败方永不换。
        if (!player_must_switch_pet(ctx, player)) {
            out.reason = "your_on_stage_pet_is_alive";
            return out;
        }
    } else if (ctx.current_player_id_ != player) {
        prospective = true;
    }

    const int slot = ctx.on_stage[player];
    if (slot < 0 || slot >= 6) {
        out.reason = "no_on_stage_pet";
        return out;
    }
    ElfPet& pet = ctx.seerRobot[player].elfPets[slot];

    out.can_act = !prospective;
    out.prospective = prospective;
    out.reason = prospective ? "prospective" : "";
    out.must_choose_pet = !prospective && (ctx.currentState == State::CHOOSE_AFTER_DEATH);

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
    // index = 嗑药库存(按 item_id 升序)位次 —— 客户端提交 USE_MEDICINE 时原样带回。
    // 死亡换宠时点同样一律不可用（FSM 只收 CHOOSE_PET）。
    const SeerRobot& robot = ctx.seerRobot[player];
    int medicine_index = 0;
    for (const auto& [item_id, count] : robot.medicines) {
        LegalMedicine lm;
        lm.index = medicine_index++;
        lm.item_id = item_id;
        if (auto item = official_data::OfficialDataStore::instance().repository()
                             .load_battle_item(item_id)) {
            lm.name = item->name;
        }
        lm.count = count;
        lm.usable = !out.must_choose_pet && (count > 0) && (pet.hp > 0);
        out.medicines.push_back(std::move(lm));
    }

    return out;
}

}  // namespace server
