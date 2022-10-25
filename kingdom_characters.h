
#include <stdint.h>
#include "htw_core.h"
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

typedef uint32_t kd_CharacterId;

typedef struct kd_CharacterPrimaryStats {
    // mind
    uint16_t instinct;
    uint16_t reason;
    // body
    uint16_t endurance;
    uint16_t strength;
    // spirit
    uint16_t independence;
    uint16_t cooperation;
} kd_CharacterPrimaryStats;

// NOTE: these stats should be treated as bonuses, and will normally be 0 regardless of primary stats
typedef struct kd_CharacterSecondaryStats {
    // mind
    // instinct
    int16_t evasion;
    int16_t stealth;
    int16_t attunement;
    // reason
    int16_t accuracy;
    int16_t artifice;
    int16_t logic;

    // body
    // endurance
    int16_t fortitude;
    int16_t stamina;
    int16_t recovery;
    // strength
    int16_t might;
    int16_t speed;
    int16_t acrobatics;

    // spirit
    // independence
    int16_t willpower;
    int16_t ingenuity;
    int16_t reflection;
    // cooperation
    int16_t teamwork;
    int16_t negotiation;
    int16_t scholarship;
} kd_CharacterSecondaryStats;

typedef struct kd_CharacterAttributes {
    kd_GenderBitflags gender;
    kd_CharacterSize size;
} kd_CharacterAttributes;

typedef struct kd_CharacterSkills {

} kd_CharacterSkills;

typedef struct kd_CharacterState {
    int32_t currentHitPoints;
    int32_t currentStamina;
} kd_CharacterState;

typedef struct kd_Character {
    kd_CharacterId id;
    kd_CharacterState currentState;
    kd_CharacterPrimaryStats primaryStats;
    kd_CharacterSecondaryStats secondaryStats;
    kd_CharacterAttributes attributes;
    kd_CharacterSkills skills;
} kd_Character;
