#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>
#include <string>

bool savePNG(std::string filename, int width, int height, int planes, uint8_t* bytes);
bool loadPNG(std::string filename, int width, int height, uint8_t* bytes);

#endif // IMAGE_H
