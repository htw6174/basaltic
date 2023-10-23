#include "bc_flecs_utils.h"
#include "bc_components_common.h"
#include "htw_core.h"
#include "htw_random.h"
#include "time.h"
#include <sys/stat.h>

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
        free(fieldPath);
    }
    // Ensure meta type matches field type
    ecs_entity_t type = ecs_meta_get_type(cursor);
    return ecs_get(cursor->world, type, EcsPrimitive)->kind;
}

ecs_entity_t bc_instantiateRandomizer(ecs_world_t *world, ecs_entity_t prefab) {
    // Must not defer operations to ensure that the same component pointer is accessed across multiple randomizers
    // Otherwise, the temporary storage returned from ecs_get_mut_id will overwrite earlier calls for the same component
    ecs_defer_suspend(world);

    ecs_entity_t e = ecs_new_w_pair(world, EcsIsA, prefab);

    // Find all (RandomizeInt, *) relationships in prefab's type
    ecs_table_t *prefab_table = ecs_get_table(world, prefab);
    const ecs_type_t *prefab_type = ecs_get_type(world, prefab);
    ecs_id_t wildcard = ecs_pair(ecs_id(RandomizeInt), EcsWildcard);
    s32 cur = -1;
    ecs_entity_t pair;

    while (-1 != (cur = ecs_search_offset(world, prefab_table, cur + 1, wildcard, &pair))){
        // Get value based on randomizer params
        const RandomizeInt *r = ECS_CAST(RandomizeInt*, ecs_get_id(world, prefab, pair));
        s32 value = htw_randRange(r->max - r->min) + r->min;

        ecs_entity_t field = ecs_pair_second(world, pair);
        ecs_entity_t component;
        ecs_meta_cursor_t cursor = bc_meta_cursorFromField(world, e, field, &component);
        ecs_primitive_kind_t primKind = bc_meta_cursorToPrim(&cursor, component, field);

        // Cast to any valid type to set
        switch (primKind) {
            case EcsU8:
            case EcsU16:
            case EcsU32:
            case EcsU64:
            case EcsI8:
            case EcsI16:
            case EcsI32:
            case EcsI64:
                ecs_meta_set_int(&cursor, value);
                break;
            case EcsF32:
            case EcsF64:
                ecs_meta_set_float(&cursor, (double)value);
                break;
            default:
                ecs_err(
                    "Error instantiating prefab %s: RandomizeInt cannot target field of type %s\n",
                    ecs_get_name(world, prefab),
                    ecs_get_name(world, primKind)
                );
                break;
        }
    }

    ecs_defer_resume(world);

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
