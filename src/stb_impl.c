/* stb_impl.c - single translation unit instantiating stb_image, plus a small
 * helper that loads a PNG into a 32-bit SDL surface (replaces SDL2_image).
 */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "winmine.h"

static SDL_Surface *surface_from_px(unsigned char *px, int w, int h)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) {
        stbi_image_free(px);
        return NULL;
    }
    const Uint32 *src = (const Uint32 *)px;          /* stb RGBA bytes = LE 0xAABBGGRR */
    Uint32 *dst = (Uint32 *)s->pixels;
    for (int i = 0; i < w * h; i++) {
        Uint32 v = src[i];
        Uint32 r = v & 0xff, g = (v >> 8) & 0xff, b = (v >> 16) & 0xff, a = (v >> 24) & 0xff;
        dst[i] = a << 24 | r << 16 | g << 8 | b;      /* ARGB8888 */
    }
    stbi_image_free(px);
    return s;
}

SDL_Surface *stb_load_surface(const char *path)
{
    int w = 0, h = 0, ch = 0;
    unsigned char *px = stbi_load(path, &w, &h, &ch, 4);
    if (!px) return NULL;
    return surface_from_px(px, w, h);
}

SDL_Surface *stb_load_surface_mem(const void *data, int len)
{
    int w = 0, h = 0, ch = 0;
    unsigned char *px = stbi_load_from_memory((const unsigned char *)data, len, &w, &h, &ch, 4);
    if (!px) return NULL;
    return surface_from_px(px, w, h);
}
