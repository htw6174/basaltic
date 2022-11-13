#ifndef KINGDOM_CHARACTERS_H_INCLUDED
#define KINGDOM_CHARACTERS_H_INCLUDED

#include <stdint.h>
#include "htw_core.h"
#include "htw_geomap.h" // NOTE: only needed for GridCoord def; should be in seperate smaller header?
//#include "kingdom_logic.h"

typedef enum kd_GenderBitflags {
    KD_GENDER_NEITHER = 0x0,
    KD_GENDER_MALE_BIT = 0x1,
    KD_GENDER_FEMALE_BIT = 0x2,
    KD_GENDER_BOTH_BITS = 0x3,
} kd_GenderBitflags;

typedef enum kd_CharacterSize {
    KD_CHARACTER_SIZE_NONE = 0, // incorporial things
    KD_CHARACTER_SIZE_MINISCULE = 1, // rats insects, characters this size are usually treated as swarms
    KD_CHARACTER_SIZE_TINY = 2, // human baby, snake, rabbit
    KD_CHARACTER_SIZE_SMALL = 3, // human child, medium dog
    KD_CHARACTER_SIZE_AVERAGE = 4, // human adult, wolf
    KD_CHARACTER_SIZE_LARGE = 5, // horse, crocodile
    KD_CHARACTER_SIZE_HUGE = 6, // moose, rhino
    KD_CHARACTER_SIZE_ENORMOUS = 7, //elephant and larger
} kd_CharacterSize;

typedef u32 kd_CharacterId;

typedef struct kd_CharacterPrimaryStats {
    // mind
    u16 instinct;
    u16 reason;
    // body
    u16 endurance;
    u16 strength;
    // spirit
    u16 independence;
    u16 cooperation;
} kd_CharacterPrimaryStats;

// NOTE: these stats should be treated as bonuses, and will normally be 0 regardless of primary stats
typedef struct kd_CharacterSecondaryStats {
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
} kd_CharacterSecondaryStats;

typedef struct kd_CharacterAttributes {
    kd_GenderBitflags gender;
    kd_CharacterSize size;
} kd_CharacterAttributes;

typedef struct kd_CharacterSkills {
    // TODO: maybe have a limit on number of learned skills; use small uint for skill id + level
    u16 skill1;
    u16 skill2;
} kd_CharacterSkills;

typedef struct kd_CharacterState {
    htw_geo_GridCoord worldCoord;
    s32 currentHitPoints;
    s32 currentStamina;
} kd_CharacterState;

typedef struct kd_Character {
    kd_CharacterId id;
    kd_CharacterState currentState;
    kd_CharacterPrimaryStats primaryStats;
    kd_CharacterSecondaryStats secondaryStats;
    kd_CharacterAttributes attributes;
    kd_CharacterSkills skills;
} kd_Character;

kd_Character kd_createRandomCharacter();
u32 kd_moveCharacter(kd_Character *subject, htw_geo_GridCoord newCoord);

#endif // KINGDOM_CHARACTERS_H_INCLUDED
