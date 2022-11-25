#include "htw_core.h"
#include "basaltic_characters.h"

bt_Character bt_createRandomCharacter() {
    bt_CharacterState state = {
        .worldCoord = {0, 0},
        .currentHitPoints = 10,
        .currentStamina = 10
    };
    bt_CharacterPrimaryStats primaryStats = {
        0
    };
    bt_CharacterSecondaryStats secondaryStats = {
        0
    };
    bt_CharacterAttributes attributes = {
        0
    };
    bt_CharacterSkills skills = {
        0
    };
    bt_Character newCharacter = {
        .id = 0,
        .currentState = state,
        .primaryStats = primaryStats,
        .secondaryStats = secondaryStats,
        .attributes = attributes,
        .skills = skills
    };
    return newCharacter;
}

u32 bt_moveCharacter(bt_Character *subject, htw_geo_GridCoord newCoord) {
    // TODO: movement range checking
    // TODO: stamina use vs current stamina checking
    subject->currentState.worldCoord = newCoord;
    // TODO: return something indicating result of movement (distance successfully moved or 0 for failure, new position?)
    return 0;
}
