/* source/widgets/sgl_textlist.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL  
 * Document reference link: docs directory
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_textlist.h"

#define  SGL_TEXTLIST_ITEM_PAD    (3)
#define  SGL_TEXTLIST_ITEM_SPACE  (3)

static void sgl_textlist_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_textlist_t *textlist = sgl_container_of(obj, sgl_textlist_t, obj);
    sgl_draw_rect_t bg_desc = {
        .alpha = textlist->alpha,
        .color = textlist->bg_color,
        .border = obj->border,
        .border_alpha = textlist->alpha,
        .border_color = textlist->border_color,
        .radius = obj->radius,
        .pixmap = textlist->pixmap,
    };

    const int list_h = obj->coords.y2 - obj->coords.y1 + 1;
    const int item_pad = sgl_max(obj->radius, obj->border + SGL_TEXTLIST_ITEM_PAD);
    const int item_height = sgl_font_get_height(textlist->font) + 2 * SGL_TEXTLIST_ITEM_SPACE;
    const int16_t text_pos_x1 = obj->coords.x1 + item_pad;
    const int16_t text_pos_x2 = obj->coords.x2 - item_pad;
    int16_t text_pos_y =  obj->coords.y1 + SGL_TEXTLIST_ITEM_SPACE;
    const int16_t hline_h = item_height - SGL_TEXTLIST_ITEM_SPACE;
    sgl_textlist_item_t *item = textlist->head;

    int item_idx = 0;
    sgl_rect_t select = {
        .x1 = obj->coords.x1,
        .y1 = obj->coords.y1,
        .x2 = obj->coords.x2,
        .y2 = obj->coords.y1 + item_height
    };

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        sgl_draw_rect(surf, &obj->area, &obj->coords, &bg_desc);

        text_pos_y += textlist->pos_y;
        sgl_draw_fill_hline(surf, &obj->area, text_pos_y - SGL_TEXTLIST_ITEM_SPACE, text_pos_x1, text_pos_x2, 1,
                                            textlist->item_text_color, textlist->alpha);
        while (item != NULL)
        {
            if (text_pos_y >= obj->area.y2) {
                break;
            }

            if (item_idx == textlist->item_selected) {
                select.y1 = text_pos_y - SGL_TEXTLIST_ITEM_SPACE;
                select.y2 = select.y1 + item_height;

                if ((select.y1 > (obj->area.y1 + obj->radius)) && (select.y2 <= (obj->area.y2 - obj->radius))) {
                    sgl_draw_fill_rect(surf, &obj->area, &select, 0, textlist->item_selected_color, textlist->alpha / 2);
                }
                else if (select.y1 <= (obj->area.y1 + obj->radius)) {
                    sgl_area_t select_coords = select;
                    select_coords.y1 = obj->coords.y1;
                    select_coords.y2 = select.y1 + item_height + obj->radius + 1;
                    sgl_draw_fill_rect(surf, &select, &select_coords, obj->radius, textlist->item_selected_color, textlist->alpha / 2);
                }
                else if (select.y2 >= (obj->area.y2 - obj->radius)) {
                    sgl_area_t select_coords = select;
                    select_coords.y1 = select.y1 - item_height - obj->radius - 1;
                    select_coords.y2 = obj->coords.y2;
                    sgl_draw_fill_rect(surf, &select, &select_coords, obj->radius, textlist->item_selected_color, textlist->alpha / 2);
                }
            }

            sgl_draw_string(surf, &obj->area, text_pos_x1, text_pos_y, item->text, textlist->item_text_color, textlist->alpha, textlist->font);

            sgl_draw_fill_hline(surf, &obj->area, text_pos_y + hline_h, text_pos_x1, text_pos_x2, 1,
                                            textlist->item_text_color, textlist->alpha);

            text_pos_y += item_height;
            item = item->next;
            item_idx++;
        }
    }
    else if (evt->type == SGL_EVENT_MOVE_UP) {
        if((textlist->pos_y + (textlist->item_num) * item_height) >= (list_h - item_height / 2)) {
            textlist->pos_y -= evt->distance;
        }
        sgl_obj_set_dirty(obj);
    }
    else if (evt->type == SGL_EVENT_MOVE_DOWN) {
        if(textlist->pos_y < item_height / 2) {
            textlist->pos_y += evt->distance;
        }
        sgl_obj_set_dirty(obj);
    }
    else if (evt->type == SGL_EVENT_RELEASED) {
        if (textlist->pos_y > 0) {
            textlist->pos_y = 0;
            sgl_obj_set_dirty(obj);
        }

        if((textlist->pos_y + textlist->item_num * item_height) < list_h) {
            textlist->pos_y = list_h - textlist->item_num * item_height;
            sgl_obj_set_dirty(obj);
        }
    }
    else if (evt->type == SGL_EVENT_CLICKED) {
        int16_t click_y = evt->pos.y;
        int16_t local_y = click_y - obj->coords.y1;

        int clicked_index = (local_y - textlist->pos_y + SGL_TEXTLIST_ITEM_SPACE) / item_height;
        if (clicked_index >= 0 && clicked_index < textlist->item_num) {

            select.y1 = obj->coords.y1 + textlist->item_selected * item_height + textlist->pos_y;
            select.y2 = select.y1 + item_height;
            sgl_obj_update_area(&select);

            textlist->item_selected = clicked_index;

            select.y1 = obj->coords.y1 + clicked_index * item_height + textlist->pos_y;
            select.y2 = select.y1 + item_height;
            sgl_obj_update_area(&select);
        }
    }
}

/**
 * @brief create a textlist object
 * @param parent parent of the textlist
 * @return textlist object
 */
sgl_obj_t* sgl_textlist_create(sgl_obj_t* parent)
{
    sgl_textlist_t *textlist = sgl_malloc(sizeof(sgl_textlist_t));
    if(textlist == NULL) {
        SGL_LOG_ERROR("sgl_textlist_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(textlist, 0, sizeof(sgl_textlist_t));

    sgl_obj_t *obj = &textlist->obj;
    sgl_obj_init(&textlist->obj, parent);
    obj->construct_fn = sgl_textlist_construct_cb;
    sgl_obj_set_clickable(obj);
    sgl_obj_set_movable(obj);
    sgl_obj_set_border_width(obj, 1);

    textlist->alpha = SGL_THEME_ALPHA;
    textlist->bg_color = SGL_THEME_COLOR;
    textlist->border_color = SGL_THEME_BORDER_COLOR;
    textlist->item_selected = -1;
    textlist->pos_y = 0;

    return obj;
}

/**
 * @brief set the radius of the textlist
 * @param obj textlist object
 * @param radius radius of the textlist
 * @return none
 */
void sgl_textlist_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    obj->radius > 0 ? sgl_obj_size_zoom(obj, radius - obj->radius) : 0;
    obj->radius = radius;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the text color of the textlist
 * @param obj textlist object
 * @param color text color
 * @return none
 */
void sgl_textlist_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->item_text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the selected color of the textlist
 * @param obj textlist object
 * @param color selected color
 * @return none
 */
void sgl_textlist_set_selected_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->item_selected_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the border color of the textlist
 * @param obj textlist object
 * @param color border color
 * @return none
 */
void sgl_textlist_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the background color of the textlist
 * @param obj textlist object
 * @param color background color
 * @return none
 */
void sgl_textlist_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the font of the textlist
 * @param obj textlist object
 * @param font font of the textlist
 * @return none
 */
void sgl_textlist_set_text_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the pixmap of the textlist
 * @param obj textlist object
 * @param pixmap pixmap of the textlist
 * @return none
 */
void sgl_textlist_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the alpha of the textlist
 * @param obj textlist object
 * @param alpha alpha of the textlist
 * @return none
 */
void sgl_textlist_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the border width of the textlist
 * @param obj textlist object
 * @param width border width of the textlist
 * @return none
 */
void sgl_textlist_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_obj_set_border_width(obj, width);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the selected item of the textlist by index
 * @param obj textlist object
 * @param index index of the selected item
 * @return none
 */
void sgl_textlist_set_selected_by_index(sgl_obj_t *obj, int16_t index)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->item_selected = index;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the selected item of the textlist by text
 * @param obj textlist object
 * @param text text of the selected item
 * @return none
 */
void sgl_textlist_set_selected_by_text(sgl_obj_t *obj, char *text)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    sgl_textlist_item_t *item = textlist->head;
 
    for (int i = 0; i < textlist->item_num && item != NULL; i++, item = item->next) {
        if (strcmp(item->text, text) == 0) {
            textlist->item_selected = i;
            sgl_obj_set_dirty(obj);
            return;
        }
    }
}

/**
 * @brief add an item to the textlist
 * @param obj textlist object
 * @param text text of the item
 * @return none
 */
void sgl_textlist_add_item(sgl_obj_t *obj, char *text)
{
    sgl_textlist_t *textlist = sgl_container_of(obj, sgl_textlist_t, obj);
    sgl_textlist_item_t *item = sgl_malloc(sizeof(sgl_textlist_item_t));
    if(item == NULL) {
        SGL_LOG_ERROR("sgl_textlist_add_item: malloc failed");
        return;
    }

    textlist->item_num++;

    if (textlist->head == NULL) {
        textlist->head = item;
        textlist->tail = item;
        item->text = text;
        return;
    }

    textlist->tail->next = item;
    textlist->tail = item;
    item->next = NULL;
    item->text = text;
}

/**
 * @brief get the text that selected item of the textlist
 * @param obj textlist object
 * @return selected item text
 */
char* sgl_textlist_get_selected_text(sgl_obj_t *obj)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    sgl_textlist_item_t *item = textlist->head;
 
    for (int i = 0; i < textlist->item_num && item != NULL; i++, item = item->next) {
        if (i == textlist->item_selected) {
            return item->text;
        }
    }

    return NULL;
}

/**
 * @brief get the index of the selected item of the textlist
 * @param obj textlist object
 * @return selected item index
 */
int16_t sgl_textlist_get_selected_index(sgl_obj_t *obj)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    return textlist->item_selected;
}
