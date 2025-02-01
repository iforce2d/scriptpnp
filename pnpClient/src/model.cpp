
#include <GL/gl.h>
#include <GL/glu.h>

#include <thread>
#include <chrono>

/* assimp include files. These three are usually needed. */
#include <assimp/cimport.h>
#include <assimp/Importer.hpp>
#include <assimp/ProgressHandler.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "scv/vec3.h"
#include "log.h"

using namespace Assimp;

using namespace std::chrono_literals;

/* ---------------------------------------------------------------------------- */
// inline static void print_error(const char* msg) {
//     printf("ERROR: %s\n", msg);
// }

//#define NEW_LINE "\n"
//#define DOUBLE_NEW_LINE NEW_LINE NEW_LINE

/* ---------------------------------------------------------------------------- */

/* the global Assimp scene object */
struct modelInstance_t {
    Importer* importer;
    const C_STRUCT aiScene* scene;
    GLuint scene_list;
};

/* ---------------------------------------------------------------------------- */
void color4_to_float4(const C_STRUCT aiColor4D *c, float f[4])
{
    f[0] = c->r;
    f[1] = c->g;
    f[2] = c->b;
    f[3] = c->a;
}

/* ---------------------------------------------------------------------------- */
void set_float4(float f[4], float a, float b, float c, float d)
{
    f[0] = a;
    f[1] = b;
    f[2] = c;
    f[3] = d;
}

/* ---------------------------------------------------------------------------- */
void apply_material(const C_STRUCT aiMaterial *mtl)
{
    float c[4];

    GLenum fill_mode;
    int ret1, ret2;
    C_STRUCT aiColor4D diffuse;
    C_STRUCT aiColor4D specular;
    C_STRUCT aiColor4D ambient;
    C_STRUCT aiColor4D emission;
    ai_real shininess, strength;
    int two_sided;
    int wireframe;
    unsigned int max;

    set_float4(c, 0.8f, 0.8f, 0.8f, 1.0f);
    if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse))
        color4_to_float4(&diffuse, c);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, c);

    set_float4(c, 0.0f, 0.0f, 0.0f, 1.0f);
    if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_SPECULAR, &specular))
        color4_to_float4(&specular, c);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, c);

    set_float4(c, 0.2f, 0.2f, 0.2f, 1.0f);
    if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_AMBIENT, &ambient))
        color4_to_float4(&ambient, c);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, c);

    set_float4(c, 0.0f, 0.0f, 0.0f, 1.0f);
    if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_EMISSIVE, &emission))
        color4_to_float4(&emission, c);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, c);

    max = 1;
    ret1 = aiGetMaterialFloatArray(mtl, AI_MATKEY_SHININESS, &shininess, &max);
    if(ret1 == AI_SUCCESS) {
        max = 1;
        ret2 = aiGetMaterialFloatArray(mtl, AI_MATKEY_SHININESS_STRENGTH, &strength, &max);
        if(ret2 == AI_SUCCESS)
            glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess * strength);
        else
            glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
    }
    else {
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
        set_float4(c, 0.0f, 0.0f, 0.0f, 0.0f);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, c);
    }

    max = 1;
    if(AI_SUCCESS == aiGetMaterialIntegerArray(mtl, AI_MATKEY_ENABLE_WIREFRAME, &wireframe, &max))
        fill_mode = wireframe ? GL_LINE : GL_FILL;
    else
        fill_mode = GL_FILL;
    glPolygonMode(GL_FRONT_AND_BACK, fill_mode);

    max = 1;
    if((AI_SUCCESS == aiGetMaterialIntegerArray(mtl, AI_MATKEY_TWOSIDED, &two_sided, &max)) && two_sided)
        glDisable(GL_CULL_FACE);
    else
        glEnable(GL_CULL_FACE);
}

/* ---------------------------------------------------------------------------- */
void recursive_render (const aiScene* scene, const C_STRUCT aiScene *sc, const C_STRUCT aiNode* nd)
{
    unsigned int i;
    unsigned int n = 0, t;
    C_STRUCT aiMatrix4x4 m = nd->mTransformation;

    aiTransposeMatrix4(&m);
    glPushMatrix();
    glMultMatrixf((float*)&m);

    // draw all meshes assigned to this node
    for (; n < nd->mNumMeshes; ++n) {
        const C_STRUCT aiMesh* mesh = scene->mMeshes[nd->mMeshes[n]];

        apply_material(sc->mMaterials[mesh->mMaterialIndex]);

        if(mesh->mNormals == NULL) {
            glDisable(GL_LIGHTING);
        } else {
            glEnable(GL_LIGHTING);
        }

        for (t = 0; t < mesh->mNumFaces; ++t) {
            const C_STRUCT aiFace* face = &mesh->mFaces[t];
            GLenum face_mode;

            switch(face->mNumIndices) {
            case 1: face_mode = GL_POINTS; break;
            case 2: face_mode = GL_LINES; break;
            case 3: face_mode = GL_TRIANGLES; break;
            default: face_mode = GL_POLYGON; break;
            }

            glBegin(face_mode);

            for(i = 0; i < face->mNumIndices; i++) {
                int index = face->mIndices[i];
                if(mesh->mColors[0] != NULL)
                    glColor4fv((GLfloat*)&mesh->mColors[0][index]);
                if(mesh->mNormals != NULL)
                    glNormal3fv(&mesh->mNormals[index].x);
                glVertex3fv(&mesh->mVertices[index].x);
            }

            glEnd();
        }

    }

    // draw all children
    for (n = 0; n < nd->mNumChildren; ++n) {
        recursive_render(scene, sc, nd->mChildren[n]);
    }

    glPopMatrix();
}

float currentModelLoadPercent = 0;
class MyProgressHandler : public Assimp::ProgressHandler {

    bool Update(float percentage = -1.f) {
        //printf("percent: %f\n", percentage); fflush(stdout);
        currentModelLoadPercent = percentage;
        //std::this_thread::sleep_for(10ms);
        return true;
    }
};

MyProgressHandler mph;

/* ---------------------------------------------------------------------------- */
void* loadModel(const char* path)
{
    Importer* importer = new Importer();
    importer->SetProgressHandler(&mph);
    const aiScene* scene = importer->ReadFile(path, aiProcessPreset_TargetRealtime_Fast);
    importer->SetProgressHandler(NULL);

    //const aiScene* scene = aiImportFile(path,aiProcessPreset_TargetRealtime_Fast);

    if ( ! scene ) {
        g_log.log(LL_ERROR, "Failed to load model: %s", path);
        return NULL;
    }

    modelInstance_t* mi = new modelInstance_t;
    mi->importer = importer;
    mi->scene = scene;
    mi->scene_list = 0;

    return mi;
}


extern scv::vec3 lightPos1;
extern scv::vec3 lightPos2;
extern scv::vec3 lightPos3;
extern bool enableLight1;
extern bool enableLight2;
extern bool enableLight3;

void enableModelRenderState()
{
    glEnable(GL_LIGHTING);

    GLfloat light_diffuse[] = {0.6, 0.6, 0.6, 1.0};
    GLfloat light_ambient[] = {0.6, 0.6, 0.6, 1.0};

    if ( enableLight1 ) {
        GLfloat light1_position[] = {lightPos1.x, lightPos1.y, lightPos1.z, 1.0};
        glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, light_diffuse);
        glLightfv(GL_LIGHT1, GL_AMBIENT, light_ambient);
        glEnable(GL_LIGHT1);
    }

    if ( enableLight2 ) {
        GLfloat light2_position[] = {lightPos2.x, lightPos2.y, lightPos2.z, 1.0};
        glLightfv(GL_LIGHT2, GL_DIFFUSE, light_diffuse);
        glLightfv(GL_LIGHT2, GL_AMBIENT, light_ambient);
        glLightfv(GL_LIGHT2, GL_POSITION, light2_position);
        glEnable(GL_LIGHT2);
    }

    if ( enableLight3 ) {
        GLfloat light3_position[] = {lightPos3.x, lightPos3.y, lightPos3.z, 1.0};
        glLightfv(GL_LIGHT3, GL_DIFFUSE, light_diffuse);
        glLightfv(GL_LIGHT3, GL_AMBIENT, light_ambient);
        glLightfv(GL_LIGHT3, GL_POSITION, light3_position);
        glEnable(GL_LIGHT3);
    }


    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);

}

void renderModel(const void* m)
{
    if ( !m )
        return;

    modelInstance_t* mi = (modelInstance_t*)m;

    // if the display list has not been made yet, create a new one and fill it with scene contents
    if(mi->scene_list == 0) {
        mi->scene_list = glGenLists(1);
        glNewList(mi->scene_list, GL_COMPILE);
        recursive_render(mi->scene, mi->scene, mi->scene->mRootNode);
        glEndList();
    }

    glCallList(mi->scene_list);
}

void releaseModel(const void* m) {
    if ( !m )
        return;
    modelInstance_t* mi = (modelInstance_t*)m;
    if ( mi->scene_list ) {
        glDeleteLists(mi->scene_list, 1);
    }
    //aiReleaseImport( mi->scene );
    delete mi->importer;
    delete mi;
}

void cleanupAssimp() {
    aiDetachAllLogStreams();
}





