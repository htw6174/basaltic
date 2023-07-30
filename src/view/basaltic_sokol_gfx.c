#include "basaltic_sokol_gfx.h"
#include "htw_core.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

    sg_image_desc desc = {
        .width = width,
        .height = height,
        .num_mipmaps = mipCount,
        .usage = SG_USAGE_IMMUTABLE,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR_MIPMAP_LINEAR,
        .mag_filter = SG_FILTER_NEAREST, // NOTE: mag filter must be nearest or liner, can't use mipmaps (and wouldn't make sense to anyway)
        .data = {.subimage[0][0] = {.ptr = imgData, .size = imgDataSize}}
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
        desc.data.subimage[0][mip] = (sg_range){.ptr = mData[mip], .size = mSize};
    }

    sg_image img = sg_make_image(&desc);

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
        return -1; // TODO: optionally get more detailed error info from sokol
    }
}
