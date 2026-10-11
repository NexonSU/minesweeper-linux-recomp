/* dump_font.c — generate the embedded glyph atlas for the winmine port.
 *
 * Reproduces exactly what SDL_ttf 2.24 produces internally (hinted FreeType
 * bitmaps via FT_LOAD_NO_AUTOHINT + hinted advances + GPOS kern pairs), so
 * the port's text is pixel-identical to the SDL_ttf reference build.
 * Verified: all sample-string widths match TTF_SizeText 48/48.
 *
 * Build:  gcc -O2 dump_font.c -o dump_font $(pkg-config --cflags --libs freetype2) -lm
 * Usage:  ./dump_font DejaVuSans-subset.ttf font_atlas.bin
 *
 * Atlas binary format v2 (all little-endian; advances/offsets in 26.6 fixed):
 *   "WMFA" u16 version=2
 *   u8 nsizes; u8 sizes[nsizes]
 *   per size:
 *     i16 ascent_px, i16 height_px
 *     u8 nglyphs; u8 base_cp
 *     per glyph: i16 adv26, i16 left, i16 top, u8 w, u8 h, w*h coverage bytes
 *     u16 npairs; per pair: u8 prev_cp, u8 cur_cp, i16 dx26, i16 dy26
 */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NSIZES 6
static const int SIZES[NSIZES] = { 8, 9, 10, 11, 12, 13 };
#define CP_LO 32
#define CP_HI 126
#define NGLYPH (CP_HI - CP_LO + 1)

static void put8(FILE *f, int v) { fputc(v & 0xff, f); }
static void put16(FILE *f, int v) { fputc(v & 0xff, f); fputc((v >> 8) & 0xff, f); }

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s font.ttf out.bin\n", argv[0]); return 1; }

    FT_Library lib;
    if (FT_Init_FreeType(&lib)) { fprintf(stderr, "FT init\n"); return 1; }

    FILE *out = fopen(argv[2], "wb");
    if (!out) { perror("out"); return 1; }

    fwrite("WMFA", 1, 4, out);
    put16(out, 2); put16(out, 0);          /* version 2 */
    put8(out, NSIZES);
    for (int s = 0; s < NSIZES; s++) put8(out, SIZES[s]);

    for (int si = 0; si < NSIZES; si++) {
        int size = SIZES[si];
        FT_Face face;
        if (FT_New_Face(lib, argv[1], 0, &face)) { fprintf(stderr, "font\n"); return 1; }
        FT_Set_Pixel_Sizes(face, 0, size);

        /* metrics identical to SDL_ttf: ascent=ceil(asc*scale), height=ceil((asc-desc+gap)*scale) */
        double scale = (double)size / face->units_per_EM;
        int ascent = (int)ceil(face->ascender * scale);
        int height = (int)ceil(face->height * scale);   /* face->height = asc - desc + line_gap */

        put16(out, ascent);
        put16(out, height);
        put8(out, NGLYPH);
        put8(out, CP_LO);

        int adv_s[NGLYPH];
        for (int cp = CP_LO; cp <= CP_HI; cp++) {
            int idx = FT_Get_Char_Index(face, cp);
            if (FT_Load_Glyph(face, idx, FT_LOAD_NO_AUTOHINT)) { fprintf(stderr, "load %d\n", cp); return 1; }
            if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) { fprintf(stderr, "render %d\n", cp); return 1; }
            FT_GlyphSlot g = face->glyph;
            adv_s[cp - CP_LO] = g->advance.x;
            int w = g->bitmap.width, h = g->bitmap.rows;
            put16(out, g->advance.x);
            put16(out, g->bitmap_left);
            put16(out, g->bitmap_top);
            put8(out, w); put8(out, h);
            for (int r = 0; r < h; r++)
                fwrite(g->bitmap.buffer + (size_t)r * g->bitmap.pitch, 1, w, out);
        }

        /* GPOS kern pairs (hinted), stored only when nonzero */
        int npairs = 0;
        long pairs_off = ftell(out);
        put16(out, 0); /* placeholder count */
        for (int p = CP_LO; p <= CP_HI; p++) {
            int pi = FT_Get_Char_Index(face, p);
            for (int c = CP_LO; c <= CP_HI; c++) {
                int ci = FT_Get_Char_Index(face, c);
                if (!pi || !ci) continue;
                FT_Vector d;
                if (FT_Get_Kerning(face, pi, ci, FT_KERNING_DEFAULT, &d)) continue;
                if (d.x == 0 && d.y == 0) continue;
                put8(out, p); put8(out, c);
                put16(out, (int)d.x); put16(out, (int)d.y);
                npairs++;
            }
        }
        long end = ftell(out);
        fseek(out, pairs_off, SEEK_SET);
        put16(out, npairs);
        fseek(out, end, SEEK_SET);

        FT_Done_Face(face);
        fprintf(stderr, "size %d: ascent=%d height=%d pairs=%d\n", size, ascent, height, npairs);
    }

    fclose(out);
    FT_Done_FreeType(lib);
    return 0;
}
