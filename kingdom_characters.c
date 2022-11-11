#include "htw_core.h"
#include "kingdom_characters.h"

kd_Character kd_createRandomCharacter() {
    kd_CharacterState state = {
        .worldGridX = 0,
        .worldGridY = 0,
        .currentHitPoints = 10,
        .currentStamina = 10
    };
    kd_CharacterPrimaryStats primaryStats = {
        0
    };
    kd_CharacterSecondaryStats secondaryStats = {
        0
    };
    kd_CharacterAttributes attributes = {
        0
    };
    kd_CharacterSkills skills = {
        0
    };
    kd_Character newCharacter = {
        .id = 0,
        .currentState = state,
        .primaryStats = primaryStats,
        .secondaryStats = secondaryStats,
        .attributes = attributes,
        .skills = skills
    };
    return newCharacter;
}

u32 kd_moveCharacter(kd_Character *subject, u32 newX, u32 newY) {
    // TODO: movement range checking
    // TODO: stamina use vs current stamina checking
    subject->currentState.worldGridX = newX;
    subject->currentState.worldGridY = newY;
    // TODO: return something indicating result of movement (distance successfully moved or 0 for failure, new position?)
    return 0;
}
