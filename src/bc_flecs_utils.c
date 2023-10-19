#include "bc_flecs_utils.h"
#include "bc_components_common.h"
#include "time.h"
#include <sys/stat.h>


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
