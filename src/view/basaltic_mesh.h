#ifndef BASALTIC_MESH_H_INCLUDED
#define BASALTIC_MESH_H_INCLUDED

#include "htw_core.h"

typedef struct {
    //htw_MeshBufferSet meshBufferSet;
    void *vertexData;
    void *indexData;
    void *instanceData;
    size_t vertexDataSize;
    size_t indexDataSize;
    size_t instanceDataSize;
} bc_Mesh;

// TODO: loading from file, procedural primitives, easy instancing

#endif // BASALTIC_MESH_H_INCLUDED
