#ifndef BASALTIC_CHARACTERS_H_INCLUDED
#define BASALTIC_CHARACTERS_H_INCLUDED

#include <stdint.h>
#include "htw_core.h"
#include "htw_geomap.h" // NOTE: only needed for GridCoord def; should be in seperate smaller header?
//#include "basaltic_logic.h"

typedef enum bc_GenderBitflags {
    BC_GENDER_NEITHER = 0x0,
    BC_GENDER_MALE_BIT = 0x1,
    BC_GENDER_FEMALE_BIT = 0x2,
    BC_GENDER_BOTH_BITS = 0x3,
} bc_GenderBitflags;

typedef enum bc_CharacterSize {
    BC_CHARACTER_SIZE_NONE = 0, // incorporial things
    BC_CHARACTER_SIZE_MINISCULE = 1, // rats, insects, characters this size are usually treated as swarms
    BC_CHARACTER_SIZE_TINY = 2, // human baby, snake, rabbit
    BC_CHARACTER_SIZE_SMALL = 3, // human child, medium dog
    BC_CHARACTER_SIZE_AVERAGE = 4, // human adult, wolf
    BC_CHARACTER_SIZE_LARGE = 5, // horse, crocodile
    BC_CHARACTER_SIZE_HUGE = 6, // moose, rhino
    BC_CHARACTER_SIZE_ENORMOUS = 7, //elephant and larger
} bc_CharacterSize;

typedef u32 bc_CharacterId;

typedef struct bc_CharacterPrimaryStats {
    // mind
    u16 instinct;
    u16 reason;
    // body
    u16 endurance;
    u16 strength;
    // spirit
    u16 independence;
    u16 cooperation;
} bc_CharacterPrimaryStats;

// NOTE: these stats should be treated as bonuses, and will normally be 0 regardless of primary stats
typedef struct bc_CharacterSecondaryStats {
    // mind
    // instinct
    s16 evasion;
    s16 stealth;
    s16 attunement;
    // reason
    s16 accuracy;
    s16 artifice;
    s16 logic;

    // body
    // endurance
    s16 fortitude;
    s16 stamina;
    s16 recovery;
    // strength
    s16 might;
    s16 speed;
    s16 acrobatics;

    // spirit
    // independence
    s16 willpower;
    s16 ingenuity;
    s16 reflection;
    // cooperation
    s16 teamwork;
    s16 negotiation;
    s16 scholarship;
} bc_CharacterSecondaryStats;

typedef struct bc_CharacterAttributes {
    bc_GenderBitflags gender;
    bc_CharacterSize size;
} bc_CharacterAttributes;

typedef struct bc_CharacterSkills {
    // TODO: maybe have a limit on number of learned skills; use small uint for skill id + level
    u16 skill1;
    u16 skill2;
} bc_CharacterSkills;

typedef struct bc_CharacterState {
    htw_geo_GridCoord worldCoord;
    s32 currentHitPoints;
    s32 currentStamina;
} bc_CharacterState;

typedef struct bc_Character {
    bc_CharacterId id;
    bc_CharacterState currentState;
    bc_CharacterPrimaryStats primaryStats;
    bc_CharacterSecondaryStats secondaryStats;
    bc_CharacterAttributes attributes;
    bc_CharacterSkills skills;
} bc_Character;

bc_Character bc_createRandomCharacter();
u32 bc_moveCharacter(bc_Character *subject, htw_geo_GridCoord newCoord);

#endif // BASALTIC_CHARACTERS_H_INCLUDED
