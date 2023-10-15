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
