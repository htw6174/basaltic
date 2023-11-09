#ifndef BASALTIC_COMPONENTS_VIEW_H_INCLUDED
#define BASALTIC_COMPONENTS_VIEW_H_INCLUDED

#include "htw_core.h"
#include "basaltic_worldState.h"
#include "basaltic_components.h"
#include <time.h>
#include "ccVector.h"
#include "sokol_gfx.h"
#include "flecs.h"

// Ensure meta symbols are only defined once
#undef ECS_META_IMPL
#undef BC_DECL
#ifndef BASALTIC_VIEW_IMPL
#define ECS_META_IMPL EXTERN
#define BC_DECL extern
#else
#define BC_DECL
#endif

/** Number of "in-flight" buffers used for things like Pixel Pack Buffers
 * The higher this number, the larger a delay between reads and writes
 * However, if this number is too small, it will limit framerate for reasons explained here: https://www.khronos.org/opengl/wiki/Pixel_Buffer_Object
 * Reasonable values are between 2 and 4
 */
#define RING_BUFFER_LENGTH 2
#define MAX_QUERY_EXPR_LENGTH 1024

/* Common types */
BC_DECL ECS_COMPONENT_DECLARE(vec2);
BC_DECL ECS_COMPONENT_DECLARE(vec3);
BC_DECL ECS_COMPONENT_DECLARE(vec4);

typedef vec3 Scale;
BC_DECL ECS_COMPONENT_DECLARE(Scale);

typedef vec4 Color;
BC_DECL ECS_COMPONENT_DECLARE(Color);

typedef vec4 Rect;
BC_DECL ECS_COMPONENT_DECLARE(Rect);


/* Connection to Model */

ECS_STRUCT(ModelWorld, {
    ecs_world_t *world;
    u64 lastRenderedStep;
    bool renderOutdated;
});

#define bc_redraw_model(view_world) { \
    ecs_singleton_get_mut(view_world, ModelWorld)->renderOutdated = true; \
    ecs_singleton_modified(view_world, ModelWorld); \
} \


ECS_STRUCT(ModelQuery, {
    ecs_query_t *query;
});

ECS_STRUCT(QueryDesc, {
    char *expr;
});



/* User Interface */

BC_DECL ECS_TAG_DECLARE(Previous); // Relationship where target is previous value of a component

ECS_STRUCT(DeltaTime, {
    float seconds;
});

// Corresponds to SDL_BUTTON_* defines
ECS_ENUM(MouseButton, {
    BC_MOUSE_NONE =     0,
    BC_MOUSE_LEFT =     1,
    BC_MOUSE_MIDDLE =   2,
    BC_MOUSE_RIGHT =    3,
    BC_MOUSE_X1 =       4,
    BC_MOUSE_X2 =       5
});

ECS_BITMASK(InputType, {
    BC_INPUT_DEFAULT =  0x00,
    BC_INPUT_PRESSED =  0x01,
    BC_INPUT_RELEASED = 0x02,
    BC_INPUT_HELD =     0x04
});

ECS_BITMASK(InputMotion, {
    BC_MOTION_NONE =    0x00,
    BC_MOTION_SCROLL =  0x01, // set by scroll events
    BC_MOTION_MOUSE =   0x02, // set when mouse position changes
    BC_MOTION_TILE =    0x04, // set when hovered map cell changes
});

// Corresponds to SDL_Keymod values
ECS_BITMASK(InputModifier, {
    BC_KMOD_NONE = 0x0000,
    BC_KMOD_LSHIFT = 0x0001,
    BC_KMOD_RSHIFT = 0x0002,
    BC_KMOD_LCTRL = 0x0040,
    BC_KMOD_RCTRL = 0x0080,
    BC_KMOD_LALT = 0x0100,
    BC_KMOD_RALT = 0x0200,
    BC_KMOD_LGUI = 0x0400,
    BC_KMOD_RGUI = 0x0800,
    BC_KMOD_NUM = 0x1000,
    BC_KMOD_CAPS = 0x2000,
    BC_KMOD_MODE = 0x4000,
    BC_KMOD_SCROLL = 0x8000,

    // NOTE: don't use these, because input matching relies on exact modifier mask matching. OK for a game, but should have a better approach for a full editor
    // Also, the reflection macro isn't able to parse these because they aren't numeric
    // BC_KMOD_CTRL = BC_KMOD_LCTRL | BC_KMOD_RCTRL,
    // BC_KMOD_SHIFT = BC_KMOD_LSHIFT | BC_KMOD_RSHIFT,
    // BC_KMOD_ALT = BC_KMOD_LALT | BC_KMOD_RALT,
    // BC_KMOD_GUI = BC_KMOD_LGUI | BC_KMOD_RGUI,
});

typedef s32 KeyCode;
BC_DECL ECS_COMPONENT_DECLARE(KeyCode);

// Place input bindings under a bindgroup to enable/disable multiple bindings at once
BC_DECL ECS_TAG_DECLARE(InputBindGroup);

// NOTE: must have either key, button, or motion set
ECS_STRUCT(InputBinding, {
    KeyCode key; // Keyboard key, should not be set at the same time as button. Treated as an SDL_KeyCode
    MouseButton button; // Mouse button, should not be set at the same time as key
    InputType triggerOn; // Can OR together multiple input types, fires when any of the set input types is true; defaults to PRESSED; only applicable if key or button is set
    InputModifier modifiers; // Can OR together multiple modifiers, binding will only fire when modifier mask is identical
    InputMotion motion; // Can OR together multiple motions. If a motion is set, binding will only fire when all set motions happen that frame AND key or button is held
    ecs_entity_t system; // System to run when the binding is used
});

// Pixel coordinates directly from SDL; origin at top-left
ECS_STRUCT(Pointer, {
    s32 x;
    s32 y;
    s32 lastX;
    s32 lastY;
});

// TODO: setup camera 'real' position and 'target' position, smoothly interpolate each frame
ECS_STRUCT(Camera, {
    vec3 origin;
    float distance;
    float pitch;
    float yaw;
    float zNear;
    float zFar;
});

// max camera distance from origin, in hex coordinate space
ECS_STRUCT(CameraWrap, {
    float x;
    float y;
});

ECS_STRUCT(CameraSpeed, {
    float movement;
    float rotation;
    float zoom;
});

ECS_STRUCT(FocusPlane, {
    ecs_entity_t entity;
});

// TODO: consider adding a flag to this, reset every frame and only set in shader feedback if any cell is hovered
ECS_STRUCT(HoveredCell, {
    s32 x;
    s32 y;
});

ECS_STRUCT(SelectedCell, {
    s32 x;
    s32 y;
});

ECS_STRUCT(DirtyChunkBuffer, {
    u32 count;
    u32 *chunks;
});

BC_DECL ECS_TAG_DECLARE(Tool);

ECS_STRUCT(TerrainBrush, {
    s32 value;
    s32 radius;
    // TODO: repeat modes
    // TODO: brush types
    // TODO: data layer
    // - TODO: bitmask brush for bitmask layers
});

ECS_STRUCT(PrefabBrush, {
    ecs_entity_t prefab;
});


/* Global Settings */

// Single point of access for video settings, inherits from a settings prefab
BC_DECL ECS_TAG_DECLARE(VideoSettings);

typedef float RenderScale;
BC_DECL ECS_COMPONENT_DECLARE(RenderScale);

typedef s32 ShadowMapSize;
BC_DECL ECS_COMPONENT_DECLARE(ShadowMapSize);

ECS_STRUCT(RenderDistance, {
    u32 radius;
});


/* Rendering */

ECS_STRUCT(WindowSize, {
    s32 x;
    s32 y;
});

// Pixel coordinates formatted for shader uniform use; origin at bottom-left, scaled by RenderScale, lie on half integer boundary (+0.5, +0.5)
ECS_STRUCT(Mouse, {
    float x;
    float y;
});

typedef mat4x4 PVMatrix;
BC_DECL ECS_COMPONENT_DECLARE(PVMatrix);

typedef mat4x4 ModelMatrix;
BC_DECL ECS_COMPONENT_DECLARE(ModelMatrix);

typedef mat4x4 SunMatrix;
BC_DECL ECS_COMPONENT_DECLARE(SunMatrix);

ECS_STRUCT(InverseMatrices, {
    mat4x4 view;
    mat4x4 projection;
});

ECS_STRUCT(Clock, {
    float seconds;
});

ECS_STRUCT(SunLight, {
    float azimuth;
    float inclination;
    float projectionSize;
    Color directColor;
    Color indirectColor;
});

// To hold different RenderPasses and RenderTargets
// Also, relationship where the target is a Pipeline entity to be bound in that pass
BC_DECL ECS_TAG_DECLARE(ShadowPass);
BC_DECL ECS_TAG_DECLARE(GBufferPass);
BC_DECL ECS_TAG_DECLARE(LightingPass);
BC_DECL ECS_TAG_DECLARE(FinalPass);

// Add to an entity to automatically add and setup a RenderPass and RenderTarget
ECS_STRUCT(RenderPassDescription, {
    sg_pass_action action;
    s32 width;
    s32 height;
    sg_pixel_format colorFormats[SG_MAX_COLOR_ATTACHMENTS];
    sg_pixel_format depthFormat;
    sg_filter filter; /**< Highest filter level to use, will be downgraded from linear to nearest for any formats that don't support linear*/
    sg_compare_func compare; /**< Compare to use for depth attachment */
});

ECS_STRUCT(RenderPass, {
    sg_pass pass;
    // TODO: storing this in the pass description and here seems wrong; better place to put it / initialize it?
    sg_pass_action action;
});

// NOTE: for creating individual render target images, all that's needed is the sg_pixel_format. Everything else is either not needed, or constant for the entire render target (width, height, sample count), which helps because the entire sg_image_desc struct is over 1.5kB
ECS_STRUCT(RenderTarget, {
    sg_image images[SG_MAX_COLOR_ATTACHMENTS];
    sg_image depth_stencil;
    sg_sampler samplers[SG_MAX_COLOR_ATTACHMENTS];
    sg_sampler depthSampler;
});

// Target of a ResourceFile relationship
BC_DECL ECS_TAG_DECLARE(VertexShaderSource);
BC_DECL ECS_TAG_DECLARE(FragmentShaderSource);

// NOTE: could also set this up as 2 different basaltic_components
ECS_STRUCT(PipelineDescription, {
    sg_pipeline_desc *pipeline_desc;
    sg_shader_desc *shader_desc;
});

ECS_STRUCT(Pipeline, {
    sg_pipeline pipeline;
    sg_shader shader;
});

// Tags to differentiate different render types for specalized draw systems
// Reserved for use by core renderer; other rendering modules should define their own tags
BC_DECL ECS_TAG_DECLARE(InternalRender);

BC_DECL ECS_TAG_DECLARE(TerrainRender);
BC_DECL ECS_TAG_DECLARE(DebugRender);

ECS_STRUCT(WrapInstanceOffsets, {
    vec3 offsets[4];
});

// TODO: would be nice if this was resizable
ECS_STRUCT(InstanceBuffer, {
    s32 maxInstances;
    s32 instances;
    ECS_PRIVATE;
    size_t size;
    void *data;
    sg_buffer buffer;
});

/** Immutable buffers */
ECS_STRUCT(Mesh, {
    sg_buffer vertexBuffers[SG_MAX_VERTEX_BUFFERS - 1]; // 0th vertex buffer reserved for instance buffer
    sg_buffer indexBuffer;
    s32 elements;
});

/** Standalone element count, used for shaders that generate their own geometry */
ECS_STRUCT(Elements, {
    s32 count;
});

/** Collection of buffers to allow for async read/write from/to the gpu
 *
 */
ECS_STRUCT(RingBuffer, {
    u32 readableBuffer;
    u32 writableBuffer; // NOTE: confusingly, this should be bound before a call to glReadPixels, as GL will write the result into this buffer
    ECS_PRIVATE;
    u32 _head;
    u32 _buffers[RING_BUFFER_LENGTH];
});

// Immutable texture array, all get bound to the fragment stage
ECS_STRUCT(Texture, {
    sg_sampler sampler;
    sg_image images[SG_MAX_SHADERSTAGE_IMAGES];
});

// Mutable texture
ECS_STRUCT(DataTexture, {
    sg_image image;
    sg_sampler sampler;
    u32 width;
    u32 height;
    void *data;
    size_t size;
    size_t formatSize; // Unfortunately Sokol doesn't provide a way to get size from format, so have to compute it by hand
});

// TODO: purpose of this has changed since removing SSBO. Consider renaming or moving this data
// NOTE: can tell parser to ignore members after a certain line by adding ECS_PRIVATE
ECS_STRUCT(TerrainBuffer, {
    // Radius of visible chunks around the camera center. 1 = only chunk containing camera is visible; 2 = 3x3 area; 3 = 5x5 area; etc.
    u32 renderedChunkRadius;
    u32 renderedChunkCount;
    // length renderedChunkCount arrays that are compared to determine what chunks to reload each frame, and where to place them
    s32 *closestChunks;
    s32 *loadedChunks;
    vec3 *chunkPositions;
});



void BcviewImport(ecs_world_t *world);

#endif // BASALTIC_COMPONENTS_VIEW_H_INCLUDED
