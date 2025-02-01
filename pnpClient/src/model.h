#ifndef MODEL_H
#define MODEL_H

extern float currentModelLoadPercent;

void* loadModel(const char* path);
void enableModelRenderState();
void renderModel(const void* m);
void releaseModel(const void* m);
void cleanupAssimp();

#endif
