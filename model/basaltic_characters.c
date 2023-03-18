#include "htw_core.h"
#include "htw_random.h"
#include "htw_geomap.h"
#include "basaltic_characters.h"

ECS_COMPONENT_DECLARE(bc_GridPosition_c);

bc_CharacterPool *bc_createCharacterPool(size_t poolSize) {
    bc_CharacterPool *newPool = calloc(1, sizeof(bc_CharacterPool));
    newPool->poolSize = poolSize;
    newPool->characters = calloc(poolSize, sizeof(bc_Character));
    newPool->spatialStorage = htw_geo_createSpatialStorage(poolSize);

    return newPool;
}

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

void bc_placeTestCharacters(ecs_world_t *world, u32 count, u32 maxX, u32 maxY) {
    ECS_COMPONENT_DEFINE(world, bc_GridPosition_c);
    for (int i = 0; i < count; i++) {
        ecs_entity_t newCharacter = ecs_new(world, bc_GridPosition_c);
        ecs_set(world, newCharacter, bc_GridPosition_c, {htw_randRange(maxX), htw_randRange(maxY)});
    }
    // for (int i = 0; i < characterPool->poolSize; i++) {
    //     bc_Character newCharacter = bc_createRandomCharacter();
    //     newCharacter.currentState.destinationCoord = newCharacter.currentState.worldCoord = (htw_geo_GridCoord){htw_randRange(maxX), htw_randRange(maxY)};
    //     htw_geo_spatialInsert(characterPool->spatialStorage, newCharacter.currentState.worldCoord, i);
    //     characterPool->characters[i] = newCharacter;
    // }
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

void bc_moveCharacters(bc_CharacterPool *characterPool) {
    for (int i = 0; i < characterPool->poolSize; i++) {
        bc_Character *character = &characterPool->characters[i];
        htw_geo_GridCoord currentPos = character->currentState.worldCoord;
        if (character->isControlledByPlayer == false) {
            // TEST: random walk
            htw_geo_GridCoord newDest = htw_geo_addGridCoords(currentPos, htw_geo_hexGridDirections[htw_randRange(HEX_DIRECTION_COUNT)]);
            character->currentState.destinationCoord = newDest;
        }
        if (bc_doCharacterMove(character)) {
            htw_geo_spatialMove(characterPool->spatialStorage, currentPos, character->currentState.worldCoord, i);
        } else {
            character->currentState.currentStamina += 5;
        }
    }
}
