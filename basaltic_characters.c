#include "htw_core.h"
#include "basaltic_characters.h"

bc_Character bc_createRandomCharacter() {
    bc_CharacterState state = {
        .worldCoord = {0, 0},
        .destinationCoord = {0, 0},
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

bool bc_setCharacterDestination(bc_Character *subject, htw_geo_GridCoord destinationCoord) {
    // TODO: movement range checking
    // TODO: stamina use vs current stamina checking
    subject->currentState.destinationCoord = destinationCoord;
    // TODO: return should indicate if destination is valid
    return true;
}

bool bc_setCharacterPosition(bc_Character *subject, htw_geo_GridCoord newCoord) {
    subject->currentState.worldCoord = newCoord;
    return true;
}

bool bc_doCharacterMove(bc_Character *subject) {
    // TODO: move along path towards destination instead of direct assignment
    htw_geo_GridCoord current = subject->currentState.worldCoord;
    htw_geo_GridCoord destination = subject->currentState.destinationCoord;
    if (htw_geo_isEqualGridCoords(current, destination)) {
        return false;
    } else {
        u32 movementDistance = htw_geo_hexGridDistance(current, destination);
        if (subject->currentState.currentStamina >= movementDistance) {
            subject->currentState.currentStamina -= movementDistance;
            subject->currentState.worldCoord = destination;
            return true;
        } else {
            // out of stamina
            return false;
        }
    }
}
