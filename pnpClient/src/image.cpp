
#include <png.h>
#include <string.h>
#include "image.h"
#include "log.h"

using namespace std;

bool savePNG(string filename, int width, int height, int planes, uint8_t* bytes) {
    FILE *fp = fopen(filename.c_str(), "wb");
    if ( ! fp )
        return false;

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if ( ! png_ptr ) {
        fclose(fp);
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if ( ! info_ptr ) {
        png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
        fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return false;
    }

    png_init_io(png_ptr, fp);
    //png_set_write_status_fn(png_ptr, write_row_callback);
    png_set_compression_level(png_ptr, PNG_Z_DEFAULT_COMPRESSION);
    png_set_compression_mem_level(png_ptr, 8);
    png_set_compression_strategy(png_ptr, PNG_Z_DEFAULT_STRATEGY);
    png_set_compression_window_bits(png_ptr, 15);
    png_set_compression_method(png_ptr, 8);
    png_set_compression_buffer_size(png_ptr, 8192);

    int colorType = planes == 1 ? PNG_COLOR_TYPE_GRAY : PNG_COLOR_TYPE_RGB;

    png_set_IHDR(png_ptr, info_ptr, width, height, 8, colorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for (int i = 0; i < height; i++)
        row_pointers[i] = &(bytes[i * planes * width]);

    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, NULL);

    free(row_pointers);

    fclose(fp);

    png_destroy_write_struct(&png_ptr, &info_ptr);

    return true;
}

// always load into RGB buffer, bytes must be of size width*height*3 (drop alpha data)
bool loadPNG(string filename, int width, int height, uint8_t* bytes)
{
    FILE *fp = fopen(filename.c_str(), "rb");
    if ( ! fp ) {
        g_log.log(LL_ERROR, "loadImage: file not found: %s", filename.c_str());
        return false;
    }

    uint8_t sig[8];

    fread(sig, 1, 8, fp);
    if ( ! png_check_sig(sig, 8) ) {
        g_log.log(LL_ERROR, "loadImage: not a PNG file: %s", filename.c_str());
        return false;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if ( ! png_ptr ) {
        g_log.log(LL_ERROR, "loadImage: png_create_read_struct error: %s", filename.c_str());
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if ( ! info_ptr ) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        g_log.log(LL_ERROR, "loadImage: png_create_info_struct error: %s", filename.c_str());
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        g_log.log(LL_ERROR, "loadImage: setjmp error: %s", filename.c_str());
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    uint32_t w;
    uint32_t h;
    int bit_depth;
    int color_type;
    png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type, NULL, NULL, NULL);

    if ( (int)w != width || (int)h != height ) {
        fclose(fp);
        g_log.log(LL_ERROR, "loadImage: invalid size (expected %dx%d): %s", width, height, filename.c_str());
        return false;
    }

    int bpp = 0;        // bytes per pixel
    int useBytes = 0;   // bytes to use (will repeat if < 3)

    switch ( color_type ) {
        case PNG_COLOR_TYPE_GRAY:
            bpp = 1;
            useBytes = 1;
            break;
        case PNG_COLOR_TYPE_GRAY_ALPHA:
            bpp = 2;
            useBytes = 1;
            break;
        case PNG_COLOR_TYPE_RGB: bpp = 3;
            bpp = 3;
            useBytes = 3;
            break;
        case PNG_COLOR_TYPE_RGB_ALPHA: bpp = 3;
            bpp = 4;
            useBytes = 3;
            break;
        default:
            g_log.log(LL_ERROR, "loadImage: unknown PNG color_type: %d", color_type);
            return false;
    }

    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);

    uint8_t* tmp = (uint8_t*)malloc( width * height * bpp );
    for (int i = 0; i < height; i++)
        row_pointers[i] = &(tmp[i * bpp * width]);
    png_read_image(png_ptr, row_pointers);
    png_read_end(png_ptr, NULL);

    int ind = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int offset = 0;
            bytes[ind++] = row_pointers[y][bpp*x+(offset%useBytes)];
            offset++;
            bytes[ind++] = row_pointers[y][bpp*x+(offset%useBytes)];
            offset++;
            bytes[ind++] = row_pointers[y][bpp*x+(offset%useBytes)];
        }
    }

    free(tmp);

    free(row_pointers);

    fclose(fp);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    return true;
}























