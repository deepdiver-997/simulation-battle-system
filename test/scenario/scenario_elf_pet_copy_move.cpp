#include <entities/elf-pet.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

namespace {
void check(bool value, int line) {
    if (!value) {
        std::cerr << "ElfPet copy/move regression at line " << line << '\n';
        std::abort();
    }
}
#define CHECK(x) check((x), __LINE__)

ElfPet make_pet() {
    numerical_properties base;
    for (int i = 0; i < 6; ++i) base.arr[i] = 10 + i;
    const Skills empty{Skills::EmptyPetTag{}};
    ElfPet pet(41, "pet", {2, 3}, 4, Gender::FEMALE, SoulMark{}, CommonTrait{},
               base, 75, 7, 8, true, {empty, empty, empty, empty, empty});
    pet.soulMark.id = 12;
    pet.soulMark.window_anchor_round_[3] = 9;
    pet.commonTrait.id = 13;
    pet.commonTrait.args = {2, 5};
    pet.resistance.setCompleteImmunity(23);
    pet.damage_resist = {24, 25, 26};
    pet.numericalProperties.arr[0] = 27;
    pet.speed_priority = 28;
    pet.shield_bank_.add(1, 30, 31, 2);
    pet.shield_bank_.add(4, 20, 32, 0);
    pet.hood_bank_.add(3, 40, 33, 3);
    pet.is_spirit_king = true;
    pet.has_hp_consume_kit = true;
    pet.damage_suppress_mask = 0x55;
    pet.skills[1].pp = 6;
    pet.marks.emplace_back(34, "mark", 2);
    pet.soulmark_storage[1] = std::vector<int>{1, 2};
    pet.soulmark_storage[2] = std::make_shared<int>(37);
    pet.on_stage_storage[3] = std::string("stage");
    return pet;
}

void verify(const ElfPet& p) {
    CHECK((p.elementalAttributes == std::array<int, 2>{2, 3}));
    CHECK(p.soulSeal == 4 && p.gender == Gender::FEMALE);
    CHECK(p.soulMark.id == 12 && p.soulMark.window_anchor_round_.at(3) == 9);
    CHECK(p.commonTrait.id == 13 && p.commonTrait.args == std::vector<int>({2, 5}));
    CHECK(p.resistance.getCompleteImmunity() == 23);
    CHECK(p.damage_resist.crit_pct == 24 && p.damage_resist.fixed_pct == 25 && p.damage_resist.percent_pct == 26);
    CHECK(p.numericalBase.arr[0] == 10 && p.numericalBase.arr[5] == 15);
    CHECK(p.numericalProperties.arr[0] == 27 && p.hp == 75);
    CHECK(&p.hp == &p.numericalProperties.arr[5]);
    CHECK(p.speed_priority == 28 && p.shield == 7 && p.cover == 8);
    CHECK(p.shield_bank_.count() == 2 && p.shield_bank_.quantity_of(31) == 30 && p.shield_bank_.quantity_of(32) == 20);
    CHECK(p.hood_bank_.count() == 1 && p.hood_bank_.quantity_of(33) == 40);
    CHECK(p.is_spirit_king && p.has_hp_consume_kit && p.damage_suppress_mask == 0x55 && p.is_locked);
    CHECK(p.skills[1].pp == 6 && p.marks.size() == 1 && p.marks[0].id == 34);
    CHECK(std::any_cast<const std::vector<int>&>(p.soulmark_storage.at(1)) == std::vector<int>({1, 2}));
    CHECK(*std::any_cast<const std::shared_ptr<int>&>(p.soulmark_storage.at(2)) == 37);
    CHECK(std::any_cast<const std::string&>(p.on_stage_storage.at(3)) == "stage");
    CHECK(p.id == 41 && p.name == "pet");
}

void check_independent(ElfPet& source, ElfPet& dest) {
    CHECK(&source.hp != &dest.hp);
    dest.hp = 50;
    dest.shield_bank_.consume(10);
    dest.hood_bank_.tick_duration();
    std::any_cast<std::vector<int>&>(dest.soulmark_storage.at(1)).push_back(3);
    dest.skills[1].pp = 1;
    CHECK(source.hp == 75 && source.shield_bank_.quantity_of(32) == 20);
    CHECK(std::any_cast<const std::vector<int>&>(source.soulmark_storage.at(1)).size() == 2);
    CHECK(source.skills[1].pp == 6);
    // std::any copies the contained value, not pointees managed by that value.
    CHECK(std::any_cast<std::shared_ptr<int>>(source.soulmark_storage.at(2)) ==
          std::any_cast<std::shared_ptr<int>>(dest.soulmark_storage.at(2)));
}
}

int main() {
    auto source = make_pet();
    ElfPet copied(source);
    verify(copied);
    check_independent(source, copied);

    auto assigned = make_pet();
    assigned.shield_bank_.clear();
    assigned.hood_bank_.clear();
    assigned.is_spirit_king = false;
    assigned.has_hp_consume_kit = false;
    assigned.damage_suppress_mask = 0;
    assigned = source;
    verify(assigned);
    check_independent(source, assigned);
    source = source;
    verify(source);

    auto move_source = make_pet();
    ElfPet moved(std::move(move_source));
    verify(moved);
    CHECK(&moved.hp != &move_source.hp);
    moved.hp = 50;
    CHECK(move_source.hp == 75);

    auto move_source2 = make_pet();
    auto move_assigned = make_pet();
    move_assigned.shield_bank_.clear();
    move_assigned.hood_bank_.clear();
    move_assigned.is_spirit_king = false;
    move_assigned.has_hp_consume_kit = false;
    move_assigned.damage_suppress_mask = 0;
    move_assigned = std::move(move_source2);
    verify(move_assigned);
    CHECK(&move_assigned.hp != &move_source2.hp);
    move_assigned = std::move(move_assigned);
    verify(move_assigned);

    std::vector<ElfPet> pets;
    pets.reserve(1);
    pets.push_back(make_pet());
    pets.push_back(make_pet());
    verify(pets[0]);
    verify(pets[1]);
    pets[0].hp = 11;
    CHECK(pets[1].hp == 75);
}
