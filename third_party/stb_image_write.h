/* stb_image_write.h - v1.16 - public domain image writer
   Minimal stub for compilation — replace with full stb_image_write.h from
   https://github.com/nothings/stb/blob/master/stb_image_write.h

   In production, download the real header. This is a placeholder that
   provides the required API declarations so the project compiles.
*/
#ifndef STB_IMAGE_WRITE_H
#define STB_IMAGE_WRITE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STBIW_ASSERT
#include <assert.h>
#define STBIW_ASSERT(x) assert(x)
#endif

extern int stbi_write_png(const char *filename, int w, int h, int comp,
                           const void *data, int stride_in_bytes);
extern int stbi_write_jpg(const char *filename, int w, int h, int comp,
                           const void *data, int quality);

typedef void (*stbi_write_func)(void *context, void *data, int size);

extern int stbi_write_png_to_func(stbi_write_func func, void *context,
                                   int w, int h, int comp,
                                   const void *data, int stride_in_bytes);

#ifdef __cplusplus
}
#endif

#endif /* STB_IMAGE_WRITE_H */
