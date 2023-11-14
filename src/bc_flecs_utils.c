#include "bc_flecs_utils.h"
#include "bc_components_common.h"
#include "htw_core.h"
#include "htw_random.h"
#include "time.h"
#include <sys/stat.h>

// Based on ecs_meta_bounds_signed in flecs.c, but just used for clamping int types to valid range
static struct {
    int64_t min, max;
} bc_ecs_meta_bounds_signed[EcsPrimitiveKindLast + 1] = {
    [EcsBool]    = {false,      true},
    [EcsChar]    = {INT8_MIN,   INT8_MAX},
    [EcsByte]    = {0,          UINT8_MAX},
    [EcsU8]      = {0,          UINT8_MAX},
    [EcsU16]     = {0,          UINT16_MAX},
    [EcsU32]     = {0,          UINT32_MAX},
    [EcsU64]     = {0,          INT64_MAX},
    [EcsI8]      = {INT8_MIN,   INT8_MAX},
    [EcsI16]     = {INT16_MIN,  INT16_MAX},
    [EcsI32]     = {INT32_MIN,  INT32_MAX},
    [EcsI64]     = {INT64_MIN,  INT64_MAX},
    [EcsUPtr]    = {0, ((sizeof(void*) == 4) ? UINT32_MAX : INT64_MAX)},
    [EcsIPtr]    = {
        ((sizeof(void*) == 4) ? INT32_MIN : INT64_MIN),
        ((sizeof(void*) == 4) ? INT32_MAX : INT64_MAX)
    },
    [EcsEntity]  = {0,          INT64_MAX}
};

ecs_meta_cursor_t bc_meta_cursorFromField(ecs_world_t *world, ecs_entity_t e, ecs_entity_t field, ecs_entity_t *out_component) {
    // Detect if target is a struct or primitive component & get base component
    ecs_entity_t component;
    if (ecs_has(world, field, EcsMember)) {
        component = ecs_get_parent(world, field);
    } else if (ecs_has(world, field, EcsPrimitive)) {
        component = field;
    } else {
        component = 0;
        ecs_err("Error creating cursor for %s: %s is not a struct field or primitive\n",
                ecs_get_name(world, e),
                ecs_get_name(world, field)
        );
    }
    // Ensure component is overridden
    ecs_add_id(world, e, component);
    // Get meta cursor to component
    void *c = ecs_get_mut_id(world, e, component);
    if (out_component) *out_component = component;
    return ecs_meta_cursor(world, component, c);
}

ecs_primitive_kind_t bc_meta_cursorToPrim(ecs_meta_cursor_t *cursor, ecs_entity_t component, ecs_entity_t field) {
    ecs_meta_push(cursor);
    if (component != field) {
        // Move cursor to field
        char *fieldPath = ecs_get_path(cursor->world, component, field);
        ecs_meta_dotmember(cursor, fieldPath);
        ecs_os_free(fieldPath);
    }
    // Can be uesd to ensure meta type matches field type
    ecs_entity_t type = ecs_meta_get_type(cursor);
    return ecs_get(cursor->world, type, EcsPrimitive)->kind;
}

ecs_entity_t bc_instantiateRandomizer(ecs_world_t *world, ecs_entity_t prefab) {

    ecs_entity_t e = ecs_new_w_pair(world, EcsIsA, prefab);

    // Find all (RandomizeInt, *) relationships in prefab's type
    ecs_table_t *prefab_table = ecs_get_table(world, prefab);

    ecs_id_t wildcard = ecs_pair(ecs_id(RandomizeInt), EcsWildcard);
    s32 cur = -1;
    ecs_entity_t pair;

    while (-1 != (cur = ecs_search_relation(world, prefab_table, cur + 1, wildcard, EcsIsA, 0, 0, &pair, 0))){
        // Get value based on randomizer params
        const RandomizeInt *r = ECS_CAST(RandomizeInt*, ecs_get_id(world, prefab, pair));
        s32 value;
        switch (r->distribution) {
            case RAND_DISTRIBUTION_UNIFORM:
                value = htw_randIntRange(r->min, r->max);
                break;
            case RAND_DISTRIBUTION_NORMAL:
                value = htw_randPERT(r->min, r->max, r->mean);
                break;
            case RAND_DISTRIBUTION_EXPONENTIAL:
                // TODO
                value = 0;
                break;
        }

        ecs_entity_t field = ecs_pair_second(world, pair);
        ecs_entity_t component;
        ecs_meta_cursor_t cursor = bc_meta_cursorFromField(world, e, field, &component);
        ecs_primitive_kind_t primKind = bc_meta_cursorToPrim(&cursor, component, field);

        // Cast to any valid type to set
        if (ecs_meta_set_int(&cursor, value)) {
            ecs_err(
                "Error instantiating prefab %s: RandomizeInt cannot target field of type %s\n",
                ecs_get_name(world, prefab),
                ecs_get_name(world, primKind)
            );
        }
    }

    // Do the same thing for RandomizeFloat
    wildcard = ecs_pair(ecs_id(RandomizeFloat), EcsWildcard);
    cur = -1;

    while (-1 != (cur = ecs_search_relation(world, prefab_table, cur + 1, wildcard, EcsIsA, 0, 0, &pair, 0))){
        // Get value based on randomizer params
        const RandomizeFloat *r = ECS_CAST(RandomizeFloat*, ecs_get_id(world, prefab, pair));
        float value;
        switch (r->distribution) {
            case RAND_DISTRIBUTION_UNIFORM:
                value = htw_randRange(r->min, r->max);
                break;
            case RAND_DISTRIBUTION_NORMAL:
                value = htw_randPERT(r->min, r->max, r->mean);
                break;
            case RAND_DISTRIBUTION_EXPONENTIAL:
                // TODO
                value = 0;
                break;
        }

        ecs_entity_t field = ecs_pair_second(world, pair);
        ecs_entity_t component;
        ecs_meta_cursor_t cursor = bc_meta_cursorFromField(world, e, field, &component);
        ecs_primitive_kind_t primKind = bc_meta_cursorToPrim(&cursor, component, field);

        // Cast to any valid type to set
        if (ecs_meta_set_float(&cursor, value)) {
            ecs_err(
                "Error instantiating prefab %s: RandomizeFloat cannot target field of type %s\n",
                ecs_get_name(world, prefab),
                ecs_get_name(world, primKind)
            );
        }
    }

    return e;
}

void bc_reloadFlecsScript(ecs_world_t *world, ecs_entity_t query) {

    ecs_iter_t it;
    if (query == 0) {
        ecs_filter_t *filter = ecs_filter(world, {
            .expr = "[inout] (ResourceFile, bc.common.FlecsScriptSource)"
        });

        ecs_iter_t fit = ecs_filter_iter(world, filter);
        it = fit;
    } else {
        const ecs_poly_t *poly = ecs_get_pair(world, query, EcsPoly, EcsQuery);
        assert(ecs_poly_is(poly, ecs_query_t));
        ecs_iter_poly(world, poly, &it, NULL);

    }

    while (ecs_iter_next(&it)) {
        ResourceFile *flecsScriptSources = ecs_field(&it, ResourceFile, 1);

        for (int i = 0; i < it.count; i++) {
            time_t lastAccess = flecsScriptSources[i].accessTime;

            struct stat fileStat;
            if (stat(flecsScriptSources[i].path, &fileStat) != 0) {
                // TODO: handle file read error
                continue;
            }
            time_t modifyTime = fileStat.st_mtime;

            if (modifyTime > lastAccess) {
                flecsScriptSources[i].accessTime = flecsScriptSources[i].accessTime = time(NULL);
                int err = ecs_plecs_from_file(it.world, flecsScriptSources[i].path);
                if (err != 0) {
                    // TODO: catch file opening or parsing errors, handle/display log message
                }
            }
        }
    }
}

s64 bc_clampToType(s64 value, ecs_primitive_kind_t kind) {
    return CLAMP(value, bc_ecs_meta_bounds_signed[kind].min, bc_ecs_meta_bounds_signed[kind].max);
}

s64 bc_getMetaComponentMember(void *component_at_member, ecs_primitive_kind_t kind) {
    switch (kind) {
        case EcsBool: return *(bool*)component_at_member;
        case EcsChar: return *(char*)component_at_member;
        case EcsByte: return *(u8*)component_at_member;
        case EcsU8:   return *(u8*)component_at_member;
        case EcsU16:  return *(u16*)component_at_member;
        case EcsU32:  return *(u32*)component_at_member;
        case EcsU64:  return *(u64*)component_at_member;
        case EcsI8:   return *(s8*)component_at_member;
        case EcsI16:  return *(s16*)component_at_member;
        case EcsI32:  return *(s32*)component_at_member;
        case EcsI64:  return *(s64*)component_at_member;
        default:
            ecs_err("Invalid type kind for getMetaComponentMemberInt: %d", kind);
            return 0;
    }
}

int bc_setMetaComponentMember(void *component_at_member, ecs_primitive_kind_t kind, s64 value) {
    switch (kind) {
        case EcsBool: *(bool*)component_at_member = bc_clampToType(value, kind); break;
        case EcsChar: *(char*)component_at_member = bc_clampToType(value, kind); break;
        case EcsByte: *(u8*)component_at_member = bc_clampToType(value, kind);   break;
        case EcsU8:   *(u8*)component_at_member = bc_clampToType(value, kind);   break;
        case EcsU16:  *(u16*)component_at_member = bc_clampToType(value, kind);  break;
        case EcsU32:  *(u32*)component_at_member = bc_clampToType(value, kind);  break;
        case EcsU64:  *(u64*)component_at_member = bc_clampToType(value, kind);  break;
        case EcsI8:   *(s8*)component_at_member = bc_clampToType(value, kind);   break;
        case EcsI16:  *(s16*)component_at_member = bc_clampToType(value, kind);  break;
        case EcsI32:  *(s32*)component_at_member = bc_clampToType(value, kind);  break;
        case EcsI64:  *(s64*)component_at_member = bc_clampToType(value, kind);  break;
        default:
            ecs_err("Invalid type kind for setMetaComponentMemberInt: %d", kind);
            return -1;
    }
    return 0;
}
