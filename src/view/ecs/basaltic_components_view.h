#ifndef BASALTIC_COMPONENTS_VIEW_H_INCLUDED
#define BASALTIC_COMPONENTS_VIEW_H_INCLUDED

#include "htw_core.h"
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

ECS_STRUCT(ModelStepControl, {
    s32 stepsPerRun;
    s32 framesBetweenRuns;
    bool doSingleRun;
    bool doAuto;
});

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

ECS_STRUCT(MousePreferences, {
    float sensitivity;
    float verticalSensitivity; // Extra factor applied to vertical movement only
    float scrollSensitivity;
    bool invertX;
    bool invertY;
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

// Currently visible plane
ECS_STRUCT(FocusPlane, {
    ecs_entity_t entity; // in model world
});

// Entity selected for inspection
ECS_STRUCT(FocusEntity, {
    ecs_entity_t entity; // in model world
});

// Entity controlled by player
ECS_STRUCT(PlayerEntity, {
    ecs_entity_t entity; // in model world
});

typedef htw_geo_GridCoord HoveredCell;
BC_DECL ECS_COMPONENT_DECLARE(HoveredCell);

typedef htw_geo_GridCoord SelectedCell;
BC_DECL ECS_COMPONENT_DECLARE(SelectedCell);

ECS_STRUCT(DirtyChunkBuffer, {
    u32 count;
    u32 *chunks;
});

// Singletons that can be picked in the editor to change interaction mode
BC_DECL ECS_TAG_DECLARE(Tool);

// Exclusive relationship where target is a meta member of a Component (child of Component entity)
BC_DECL ECS_TAG_DECLARE(BrushField);

ECS_STRUCT(BrushSize, {
    s32 radius; // in cells
});

// Modify cell fields up or down by value
ECS_STRUCT(AdditiveBrush, {
    s32 value;
});

// Average nearby cells towards each other
ECS_STRUCT(SmoothBrush, {
    float strength;
});

// Set value on cell fields
ECS_STRUCT(ValueBrush, {
    s32 value;
});

// Set value on cell fields
ECS_STRUCT(RiverBrush, {
    s32 value;
});

// TODO: bitmask brush?

// Instantiate prefabs
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


/* Uniform Data */

ECS_STRUCT(WindowSize, {
    s32 x;
    s32 y;
});

// Pixel coordinates of a mouse or touch formatted for shader uniform use; origin at bottom-left, scaled by RenderScale, lie on half integer boundary (+0.5, +0.5)
ECS_STRUCT(ScaledCursor, {
    float x;
    float y;
});

// Minimum visibility level, useful for switching between player and editor perspectives
ECS_STRUCT(Visibility, {
    u8 override;
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

 /* Uniforms */
 // Added to uniform components for convient building of shader descriptions
 // TODO: can setup manually for now, but later should be possible to use reflection info to generate
typedef sg_shader_uniform_block_desc UniformBlockDescription;
BC_DECL ECS_COMPONENT_DECLARE(UniformBlockDescription);

// Singletons, updated before first render pass, bound to uniform block slot 0
ECS_STRUCT(GlobalUniformsVert, {
    PVMatrix pv;
});

ECS_STRUCT(GlobalUniformsFrag, {
    float time;
    vec2 mouse;
});

// Added to specific pipelines, updated before first render pass, bound to uniform block slot 1
ECS_STRUCT(TerrainPipelineUniformsVert, {
    u32 visibility;
});

ECS_STRUCT(TerrainPipelineUniformsFrag, {
    bool drawBorders;
    ECS_PRIVATE
    u8 padding[3]; // for uniform member alignment
});

// Updated by draw systems, not necessarially added to any entity, bound to uniform block slot 2
ECS_STRUCT(CommonDrawUniformsVert, {
    ModelMatrix m;
});

/* Rendering */

// To hold different RenderPasses and RenderTargets
// Also, relationship where the target is a Pipeline entity to be bound in that pass
BC_DECL ECS_TAG_DECLARE(ShadowPass);
BC_DECL ECS_TAG_DECLARE(GBufferPass);
BC_DECL ECS_TAG_DECLARE(LightingPass);
BC_DECL ECS_TAG_DECLARE(TransparentPass);
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

// *InstanceData: Used to define data layout for shaders, and what is expected when filling InstanceBuffers. InstanceBuffers should always be associated with a specific type of instance data by making it the target of a pair e.g. (InstanceBuffer, DebugInstanceData)
ECS_STRUCT(TerrainChunkInstanceData, {
    vec4 position;
    vec2 rootCoord;
});

ECS_STRUCT(DebugInstanceData, {
    vec4 position;
    Color color;
    float scale;
});

ECS_STRUCT(ArrowInstanceData, {
    vec3 start;
    vec3 end;
    Color color;
    float width;
    float speed;
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
