#ifndef BASALTIC_CHARACTER_H_INCLUDED
#define BASALTIC_CHARACTER_H_INCLUDED

#include "flecs.h"
#include "htw_geomap.h"

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

typedef struct {
    // mind
    u16 instinct;
    u16 reason;
    // body
    u16 endurance;
    u16 strength;
    // spirit
    u16 independence;
    u16 cooperation;
} bc_PrimaryStats;

// NOTE: these stats should be treated as bonuses, and will normally be 0 regardless of primary stats
typedef struct {
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
} bc_SecondaryStats;

// Character skills: implement as relationships? e.g. (Bob, HasSkill, Alchemy, { .level = 2 }), where HasSkill is a struct with member level

typedef struct {
    s32 maxHP;
    s32 currentHP;
} bc_HP;

typedef struct {
    s32 maxStamina;
    s32 currentStamina;
} bc_Stamina;

typedef htw_geo_GridCoord bc_GridPosition;
typedef htw_geo_GridCoord bc_GridDestination;

#endif // BASALTIC_CHARACTER_H_INCLUDED
