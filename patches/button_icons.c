// @recomp (Android): in-game button icons that match the player's controller and bindings.
//
// The icons for the N64 buttons in text boxes and menus (A, B, Z, L, R, Start, C Up/Down/
// Left) are sprites (sprite table D_global_asm_80755390) whose pictures are textures in ROM
// table 7 or 25. When one of those textures is loaded (getPointerTableFile, patches_draw.c),
// button_icons_load() asks the app (recomp_fill_button_icon, src/game/recomp_api.cpp) to draw
// the icon of the physical button that N64 button is bound to - in the same size and format,
// so the game uses it as it would the original. If there's no controller, or the button
// isn't bound to one, the original texture loads as usual.

#include "patch_helpers.h"
#include "input.h"

extern u8 *D_global_asm_80755390[];
extern void *_malloc(s32);
extern void _free(void *);

typedef struct {
    s32 unk0;
    s8 tiles_x;      // images per frame, across
    s8 tiles_y;      // images per frame, down
    s16 codec;       // 0 IA4, 1 IA8, 2 RGBA16, 3 RGBA32
    s32 unk8;
    s8 unkC;
    s8 table;        // 0: table 7, 1: table 25
    s16 width;       // of one image
    s16 height;
    s16 image_count;
    s16 images[1];   // image_count of them
} ButtonIconSprite;

// The N64 button icon sprites.
static const u8 button_icon_sprites[] = { 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x87, 0x88, 0x91 };

static s32 bytes_for(s32 codec, s32 pixels) {
    switch (codec) {
        case 0: return pixels / 2;
        case 1: return pixels;
        case 2: return pixels * 2;
        case 3: return pixels * 4;
        default: return 0;
    }
}

void *button_icons_load(s32 table, u32 file_index) {
    u32 i;
    s32 j;
    if (table != 7 && table != 25) {
        return NULL;
    }
    for (i = 0; i < sizeof(button_icon_sprites); i++) {
        ButtonIconSprite *sprite = (ButtonIconSprite *)D_global_asm_80755390[button_icon_sprites[i]];
        s32 tiles;
        if (sprite == NULL || (sprite->table ? 25 : 7) != table) {
            continue;
        }
        tiles = sprite->tiles_x * sprite->tiles_y;
        if (tiles <= 0) {
            tiles = 1;
        }
        for (j = 0; j < sprite->image_count; j++) {
            if ((u16)sprite->images[j] == file_index) {
                ButtonIconRequest request;
                s32 size = bytes_for(sprite->codec, sprite->width * sprite->height);
                u8 *buffer;
                s32 tile = j % tiles;
                if (size <= 0) {
                    return NULL;
                }
                request.sprite_id = button_icon_sprites[i];
                request.width = sprite->width;
                request.height = sprite->height;
                request.codec = sprite->codec;
                request.tile_x = tile % (sprite->tiles_x > 0 ? sprite->tiles_x : 1);
                request.tile_y = tile / (sprite->tiles_x > 0 ? sprite->tiles_x : 1);
                request.tiles_x = sprite->tiles_x > 0 ? sprite->tiles_x : 1;
                request.tiles_y = sprite->tiles_y > 0 ? sprite->tiles_y : 1;
                buffer = _malloc(size);
                if (buffer == NULL) {
                    return NULL;
                }
                if (recomp_fill_button_icon(&request, buffer)) {
                    return buffer;
                }
                _free(buffer);
                return NULL;
            }
        }
    }
    return NULL;
}
