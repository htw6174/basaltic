#ifndef BC_FLECS_UTILS_H_INCLUDED
#define BC_FLECS_UTILS_H_INCLUDED

#include "htw_core.h"
#include "flecs.h"

// Macro block to use Flecs meta symbols and avoid re-declaring Components, Tags, etc. in source files
// - Add these blocks to the corresponding files
// - Use `BC_DECL` instead of `extern` for declaring tags, components, entities
// - Use Flecs meta macros without any markup, those methods already expand to extern/non-extern versions

/* components_*.h:

#undef ECS_META_IMPL
#undef BC_DECL
#ifndef BC_COMPONENT_IMPL
#define ECS_META_IMPL EXTERN
#define BC_DECL extern
#else
#define BC_DECL
#endif

 */
/* components_*.c:

// Put other component header includes above the define to avoid duplicate definitions
// #include "components_someotherheader.h"

#define BC_COMPONENT_IMPL
#include "components_*.h"

 */

ecs_entity_t bc_instantiateRandomizer(ecs_world_t *world, ecs_entity_t prefab);

/**
 * @brief Load a plecs script to do final setup for a module. This should be called at the end of module import, the current scope must be the same as when the module was loaded, and the directory structure inside modulesPath should reflect the module scructure. For exampe, calling this from the Import function for BcActors will load the file "{modulesPath}/bc/actors.flecs"
 *
 * @param world p_world:...
 * @param modulesPath absolute path or relative to CWD with '/' seperator; must not have a trailing '/'
 */
void bc_loadModuleScript(ecs_world_t *world, const char *modulesPath);

void bc_reloadFlecsScript(ecs_world_t *world, ecs_entity_t query);


/**
 * @brief Cast *component_at_member to the int type identified by kind, then expand to Int64
 *
 * @param component_at_member p_component_at_member:...
 * @param kind p_kind:...
 * @return s64
 */
s64 bc_getMetaComponentMemberInt(void *component_at_member, ecs_primitive_kind_t kind);

/**
 * @brief Directly set the value of a component member by primitive kind. Value will first be clamped to the range of the type identified by kind, then cast to that type
 *
 * @param component_field pointer to the component *plus* the offset of the member to be set
 * @param kind primitive kind from the entity in member.type
 * @param value value to assign, will be cast to the type matching prim
 * @return 0 on success, error code if kind isn't an integral type or other failure
 */
int bc_setMetaComponentMemberInt(void *component_at_member, ecs_primitive_kind_t kind, s64 value);


#endif // BC_FLECS_UTILS_H_INCLUDED
