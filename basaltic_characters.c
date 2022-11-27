#include "htw_core.h"
#include "basaltic_characters.h"

bc_Character bc_createRandomCharacter() {
    bc_CharacterState state = {
        .worldCoord = {0, 0},
        .currentHitPoints = 10,
        .currentStamina = 10
    };
    bc_CharacterPrimaryStats primaryStats = {
        0
    };
    bc_CharacterSecondaryStats secondaryStats = {
        0
    };
    bc_CharacterAttributes attributes = {
        0
    };
    bc_CharacterSkills skills = {
        0
    };
    bc_Character newCharacter = {
        .id = 0,
        .currentState = state,
        .primaryStats = primaryStats,
        .secondaryStats = secondaryStats,
        .attributes = attributes,
        .skills = skills
    };
    return newCharacter;
}

u32 bc_moveCharacter(bc_Character *subject, htw_geo_GridCoord newCoord) {
    // TODO: movement range checking
    // TODO: stamina use vs current stamina checking
    subject->currentState.worldCoord = newCoord;
    // TODO: return something indicating result of movement (distance successfully moved or 0 for failure, new position?)
    return 0;
}
