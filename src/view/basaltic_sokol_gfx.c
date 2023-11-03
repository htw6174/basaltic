#include "basaltic_sokol_gfx.h"
#include "htw_core.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define SOKOL_IMPL
#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE33
#endif

#define SOKOL_NO_DEPRECATED
#ifdef DEBUG
#define SOKOL_DEBUG
#endif
#include "sokol_gfx.h"
#include "sokol_log.h"

#ifdef SOKOL_GLCORE33

#ifndef _WIN32
// FIXME: compiler error on windows
void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    if (severity > GL_DEBUG_SEVERITY_NOTIFICATION) {
        // TODO: pretty print the source, type, and severity. Maybe allow configuring severity threshold?
        fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
                 ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
                 type, severity, message );
    }
    //assert(severity != GL_DEBUG_SEVERITY_HIGH);
}
#endif

void bc_gfxCheck(void) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        printf("GL ERROR: %x\n", err);
    }
}

uint32_t bc_sg_getImageGfxApiId(sg_image image) {
    _sg_image_t *img = _sg_image_at(&_sg.pools, image.id);
    return img->gl.tex[0];
}

#elif defined SOKOL_GLES3
void bc_gfxCheck(void) {
    // TODO: check for WebGPU errors
}

uint32_t bc_sg_getImageGfxApiId(sg_image image) {
    // FIXME: wgpu textures are opaque pointers, instead of uints like OpenGL, making this method incompatible across APIs
    //_sg_image_t *img = _sg_image_at(&_sg.pools, image.id);
    //return img->wgpu.tex;
    return 0;
}

#else
void bc_gfxCheck(void) {
    // TODO: message for unknown backend
}

uint32_t bc_sg_getImageGfxApiId(sg_image image) {
    return 0
}

#endif

void bc_sg_setup(void) {
    sg_desc sgd = {
        .logger.func = slog_func,
    };
    sg_setup(&sgd);
    assert(sg_isvalid());

    //glEnable(GL_DEBUG_OUTPUT);
    //glDebugMessageCallback(MessageCallback, 0);
}

vec3 bc_sphereToCartesian(float azimuth, float inclination, float radius) {
    azimuth *= DEG_TO_RAD;
    inclination *= DEG_TO_RAD;
    float xyDist = cos(inclination) * radius;
    return (vec3){
        .x = xyDist * sin(azimuth),
        .y = xyDist * -cos(azimuth),
        .z = sin(inclination) * radius
    };
}

void bc_drawWrapInstances(int base_element, int num_elements, int num_instances, int modelMatrixUniformBlockIndex, vec3 position, const vec3 *offsets) {
    static mat4x4 model;
    for (int i = 0; i < 4; i++) {
        mat4x4SetTranslation(model, vec3Add(position, offsets[i]));
        sg_apply_uniforms(SG_SHADERSTAGE_VS, modelMatrixUniformBlockIndex, &SG_RANGE(model));
        sg_draw(base_element, num_elements, num_instances);
    }
}

sg_image bc_loadImage(const char *filename) {
    int width, height, channels = 0;
    // Get channel count before loading. Must work with the image formats that sokol allows, right now only 1, 2, or 4 channel for 8 bits
    stbi_info(filename, NULL, NULL, &channels);

    if (channels == 3) channels = 4;

    u8 *imgData = stbi_load(filename, &width, &height, NULL, channels);
    if (imgData == NULL) {
        printf("Failed to load image %s: %s\n", filename, stbi_failure_reason());
        return (sg_image){0};
    }

    size_t imgDataSize = width * height * channels;
    // **NOTE** If using mips at all, need to provide *every* possible mipmap level, down to 1x1 (at least for OpenGL)
    // If you don't do this, there is no error or warning, and any sample taken from the image will be black
    int mipCount = log2(width) + 1;

    sg_image_desc id = {
        .width = width,
        .height = height,
        .num_mipmaps = mipCount,
        .usage = SG_USAGE_IMMUTABLE,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data = {.subimage[0][0] = {.ptr = imgData, .size = imgDataSize}}
    };

    // TODO: newer versions of sokol need samplers
    sg_sampler_desc sd = {
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_NEAREST, // NOTE: mag filter must be nearest or liner, can't use mipmaps (and wouldn't make sense to anyway)
        .mipmap_filter = SG_FILTER_LINEAR,
    };

    // generate mips
    u8 *mData[mipCount];
    for (int mip = 1; mip < mipCount; mip++) {
        // TODO: handling for non-PoT textures
        int mw = width >> mip;
        int mh = height >> mip;
        size_t mSize = mw * mh * channels;
        u8 *ptr = mData[mip] = malloc(mSize);
        for (int y = 0; y < mh; y++) {
            int sampleY = y << mip;
            for (int x = 0; x < mw; x++) {
                int sampleX = x << mip;
                for (int c = 0; c < channels; c++) {
                    *ptr++ = imgData[((sampleX + (sampleY * width)) * channels) + c];
                }
            }
        }
        id.data.subimage[0][mip] = (sg_range){.ptr = mData[mip], .size = mSize};
    }

    sg_image img = sg_make_image(&id);

    stbi_image_free(imgData);
    for (int m = 1; m < mipCount; m++) {
        free(mData[m]);
    }

    return img;
}

int bc_loadShader(const char *vertSourcePath, const char *fragSourcePath, const sg_shader_desc *shaderDescription, sg_shader *out_shader) {
    char *vert = htw_load(vertSourcePath);
    char *frag = htw_load(fragSourcePath);

    // NOTE: not great to make a 3kb copy here just to set 2 pointers; better approach?
    sg_shader_desc sd = *shaderDescription;
    sd.vs.source = vert;
    sd.fs.source = frag;

    sg_shader shd = sg_make_shader(&sd);

    free(vert);
    free(frag);

    sg_resource_state shaderState = sg_query_shader_state(shd);
    if (shaderState == SG_RESOURCESTATE_VALID) {
        *out_shader = shd;
        return 0;
    } else {
        if (shaderState == SG_RESOURCESTATE_FAILED) {
            sg_destroy_shader(shd);
        }
        *out_shader = shd;
        return -1; // TODO: optionally get more detailed error info from sokol
    }
}

int bc_createPipeline(const sg_pipeline_desc *pipelineDescription, const sg_shader *shader, sg_pipeline *out_pipeline) {
    sg_pipeline_desc pd = *pipelineDescription;
    if (sg_query_shader_state(*shader) == SG_RESOURCESTATE_VALID) {
        pd.shader = *shader;
    } else if (sg_query_shader_state(pipelineDescription->shader) != SG_RESOURCESTATE_VALID) {
        return -2;
    }

    sg_pipeline p = sg_make_pipeline(&pd);

    sg_resource_state pipelineState = sg_query_pipeline_state(p);
    if (pipelineState == SG_RESOURCESTATE_VALID) {
        *out_pipeline = p;
        return 0;
    } else {
        if (pipelineState == SG_RESOURCESTATE_FAILED) {
            sg_destroy_pipeline(p);
        }
        return -1; // TODO: optionally get more detailed error info from sokol
    }
}
