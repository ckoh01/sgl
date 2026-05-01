/* source/widgets/sgl_scope.c
*
* MIT License
*
* Copyright(c) 2023-present All contributors of SGL  
* Document reference link: https://sgl-docs.readthedocs.io
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

#include <sgl_theme.h>
#include "sgl_scope.h"


/* use SGL coordinate limits instead of C standard limits */
#define SGL_SCOPE_INT16_MAX   (SGL_POS_MAX)
#define SGL_SCOPE_INT16_MIN   (SGL_POS_MIN)
#define SGL_SCOPE_DEFAULT_DIRTY_RECT_COUNT  (5)


// Draw a dashed line using Bresenham's algorithm with dash pattern
static void draw_dashed_line(sgl_surf_t *surf, sgl_area_t *area, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t gap, sgl_color_t color)
{
    int16_t dx = sgl_abs(x1 - x0);
    int16_t dy = sgl_abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;
    int16_t dash_len = 0;

    sgl_area_t clip_area = {
        .x1 = surf->x1,
        .y1 = surf->y1,
        .x2 = surf->x2,
        .y2 = surf->y2
    };

    sgl_area_selfclip(&clip_area, area);

    while (1) {
        // Draw dash segment
        if (dash_len < gap) {
            // Check if point is within clipping area
            if (x0 >= clip_area.x1 && x0 <= clip_area.x2 && y0 >= clip_area.y1 && y0 <= clip_area.y2) {
                sgl_color_t *buf = sgl_surf_get_buf(surf, x0 - surf->x1, y0 - surf->y1);
                *buf = color;
            }
            dash_len++;
        } else if (dash_len < 2 * gap) {
            // Skip drawing (gap segment)
            dash_len++;
        } else {
            // Reset dash counter
            dash_len = 0;
        }
        
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}


static inline void scope_blend_pixel(sgl_surf_t *surf, const sgl_area_t *clip, int16_t x, int16_t y, sgl_color_t color, uint8_t alpha)
{
    if (x < clip->x1 || x > clip->x2 || y < clip->y1 || y > clip->y2) {
        return;
    }

    sgl_color_t *buf = sgl_surf_get_buf(surf, x - surf->x1, y - surf->y1);
    *buf = (alpha == SGL_ALPHA_MAX) ? color : sgl_color_mixer(color, *buf, alpha);
}


static void scope_draw_hline_fast(sgl_surf_t *surf, const sgl_area_t *clip, int16_t y, int16_t x1, int16_t x2, sgl_color_t color, uint8_t alpha)
{
    if (y < clip->y1 || y > clip->y2) {
        return;
    }

    if (x1 > x2) {
        sgl_swap(&x1, &x2);
    }

    if (x2 < clip->x1 || x1 > clip->x2) {
        return;
    }

    if (x1 < clip->x1) x1 = clip->x1;
    if (x2 > clip->x2) x2 = clip->x2;

    sgl_color_t *buf = sgl_surf_get_buf(surf, x1 - surf->x1, y - surf->y1);
    for (int16_t x = x1; x <= x2; x++, buf++) {
        *buf = (alpha == SGL_ALPHA_MAX) ? color : sgl_color_mixer(color, *buf, alpha);
    }
}


static void scope_draw_vline_fast(sgl_surf_t *surf, const sgl_area_t *clip, int16_t x, int16_t y1, int16_t y2, sgl_color_t color, uint8_t alpha)
{
    if (x < clip->x1 || x > clip->x2) {
        return;
    }

    if (y1 > y2) {
        sgl_swap(&y1, &y2);
    }

    if (y2 < clip->y1 || y1 > clip->y2) {
        return;
    }

    if (y1 < clip->y1) y1 = clip->y1;
    if (y2 > clip->y2) y2 = clip->y2;

    sgl_color_t *buf = sgl_surf_get_buf(surf, x - surf->x1, y1 - surf->y1);
    for (int16_t y = y1; y <= y2; y++) {
        *buf = (alpha == SGL_ALPHA_MAX) ? color : sgl_color_mixer(color, *buf, alpha);
        buf += surf->w;
    }
}


static void scope_draw_point_fast(sgl_surf_t *surf, const sgl_area_t *clip, int16_t x, int16_t y, sgl_color_t color, uint8_t width, uint8_t alpha)
{
    if (width <= 1) {
        scope_blend_pixel(surf, clip, x, y, color, alpha);
        return;
    }

    int16_t half_before = (int16_t)(width - 1) / 2;
    int16_t half_after = (int16_t)width / 2;

    for (int16_t py = y - half_before; py <= y + half_after; py++) {
        for (int16_t px = x - half_before; px <= x + half_after; px++) {
            scope_blend_pixel(surf, clip, px, py, color, alpha);
        }
    }
}


// Fast waveform polyline drawing without anti-aliasing.
static void scope_draw_line_fast(sgl_surf_t *surf, sgl_area_t *area, sgl_pos_t start, sgl_pos_t end, sgl_color_t color, uint8_t width, uint8_t alpha)
{
    if (width == 0 || alpha == 0) {
        return;
    }

    sgl_area_t clip = {
        .x1 = surf->x1,
        .y1 = surf->y1,
        .x2 = surf->x2,
        .y2 = surf->y2,
    };

    if (!sgl_area_selfclip(&clip, area)) {
        return;
    }

    int16_t x0 = start.x;
    int16_t y0 = start.y;
    const int16_t x1 = end.x;
    const int16_t y1 = end.y;
    const int16_t dx = sgl_abs(x1 - x0);
    const int16_t dy = sgl_abs(y1 - y0);
    const int16_t sx = (x0 < x1) ? 1 : -1;
    const int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    if (width == 1) {
        if (x0 == x1) {
            scope_draw_vline_fast(surf, &clip, x0, y0, y1, color, alpha);
            return;
        }
        if (y0 == y1) {
            scope_draw_hline_fast(surf, &clip, y0, x0, x1, color, alpha);
            return;
        }
    }

    while (1) {
        scope_draw_point_fast(surf, &clip, x0, y0, color, width, alpha);

        if (x0 == x1 && y0 == y1) {
            break;
        }

        int16_t e2 = err << 1;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}


static inline int16_t scope_map_value_to_y(const sgl_obj_t *obj, int16_t value, int16_t display_min, int16_t display_max, int16_t height)
{
    return obj->coords.y2 - ((int32_t)(value - display_min) * height) / (display_max - display_min);
}


static void scope_get_display_range(const sgl_scope_t *scope, int16_t *display_min, int16_t *display_max)
{
    *display_min = scope->min_value;
    *display_max = scope->max_value;

    if (scope->auto_scale) {
        if (scope->display_counts[0] > 0 &&
            scope->running_min != SGL_SCOPE_INT16_MAX &&
            scope->running_max != SGL_SCOPE_INT16_MIN) {
            *display_min = scope->running_min;
            *display_max = scope->running_max;
        }

        int32_t margin = (int32_t)(*display_max - *display_min) / 10;
        if (margin == 0) margin = 1;

        *display_min = (*display_min > SGL_SCOPE_INT16_MIN + margin) ? (*display_min - margin) : SGL_SCOPE_INT16_MIN;
        *display_max = (*display_max < SGL_SCOPE_INT16_MAX - margin) ? (*display_max + margin) : SGL_SCOPE_INT16_MAX;
    }

    if (*display_min == *display_max) {
        if (*display_max < SGL_SCOPE_INT16_MAX) {
            (*display_max)++;
        } else {
            (*display_min)--;
        }
    }
}


static inline void scope_area_init_invalid(sgl_area_t *area)
{
    area->x1 = SGL_SCOPE_INT16_MAX;
    area->y1 = SGL_SCOPE_INT16_MAX;
    area->x2 = SGL_SCOPE_INT16_MIN;
    area->y2 = SGL_SCOPE_INT16_MIN;
}


static inline bool scope_area_is_valid(const sgl_area_t *area)
{
    return area->x1 <= area->x2 && area->y1 <= area->y2;
}


static inline void scope_area_include_point(sgl_area_t *area, int16_t x, int16_t y)
{
    if (x < area->x1) area->x1 = x;
    if (x > area->x2) area->x2 = x;
    if (y < area->y1) area->y1 = y;
    if (y > area->y2) area->y2 = y;
}


static void scope_push_waveform_dirty_rects(sgl_scope_t *scope,
                                            uint8_t channel,
                                            int16_t display_min,
                                            int16_t display_max)
{
    sgl_obj_t *obj = &scope->obj;
    uint8_t rect_count = scope->dirty_rect_count;
    sgl_area_t waveform_area = obj->coords;
    sgl_area_t bins[SGL_DIRTY_AREA_NUM_MAX];

    if (!sgl_area_selfclip(&waveform_area, &obj->area)) {
        return;
    }

    if (rect_count == 0) {
        rect_count = 1;
    }
    if (rect_count > SGL_DIRTY_AREA_NUM_MAX) {
        rect_count = SGL_DIRTY_AREA_NUM_MAX;
    }

    int16_t waveform_width = waveform_area.x2 - waveform_area.x1 + 1;
    if (waveform_width <= 0) {
        return;
    }
    if (rect_count > (uint8_t)waveform_width) {
        rect_count = (uint8_t)waveform_width;
    }

    for (uint8_t i = 0; i < rect_count; i++) {
        scope_area_init_invalid(&bins[i]);
    }

    uint32_t display_points = scope->max_display_points > 0 ? scope->max_display_points : scope->data_len;
    if (display_points > scope->data_len) {
        display_points = scope->data_len;
    }

    uint32_t data_points = scope->display_counts[channel] < display_points ? scope->display_counts[channel] : display_points;
    if (data_points <= 1 || !scope->data_buffers[channel]) {
        sgl_obj_set_dirty(obj);
        return;
    }

    int16_t width = waveform_area.x2 - waveform_area.x1;
    int16_t height = waveform_area.y2 - waveform_area.y1;
    uint32_t denom = data_points - 1;
    uint32_t current_index = scope->current_indices[channel];
    uint32_t last_index = (current_index == 0) ? scope->data_len - 1 : current_index - 1;
    int16_t value = sgl_clamp(scope->data_buffers[channel][last_index], display_min, display_max);
    int16_t prev_x = waveform_area.x2;
    int16_t prev_y = scope_map_value_to_y(obj, value, display_min, display_max, height);

    for (uint32_t i = 1; i < data_points; i++) {
        uint32_t prev_index = (current_index >= i + 1) ? (current_index - (i + 1)) : (scope->data_len - (i + 1 - current_index));
        int16_t current_value = sgl_clamp(scope->data_buffers[channel][prev_index], display_min, display_max);
        int16_t current_x = waveform_area.x2 - (int16_t)((i * width) / denom);
        int16_t current_y = scope_map_value_to_y(obj, current_value, display_min, display_max, height);
        int16_t seg_x1 = sgl_min(prev_x, current_x);
        int16_t seg_x2 = sgl_max(prev_x, current_x);
        int16_t seg_y1 = sgl_min(prev_y, current_y);
        int16_t seg_y2 = sgl_max(prev_y, current_y);

        for (uint8_t bin = 0; bin < rect_count; bin++) {
            int16_t bin_x1 = waveform_area.x1 + (int16_t)(((int32_t)(width + 1) * bin) / rect_count);
            int16_t bin_x2 = waveform_area.x1 + (int16_t)(((int32_t)(width + 1) * (bin + 1)) / rect_count) - 1;

            if (seg_x2 < bin_x1 || seg_x1 > bin_x2) {
                continue;
            }

            scope_area_include_point(&bins[bin], seg_x1, seg_y1);
            scope_area_include_point(&bins[bin], seg_x2, seg_y2);
        }

        prev_x = current_x;
        prev_y = current_y;
    }

    for (uint8_t i = 0; i < rect_count; i++) {
        if (!scope_area_is_valid(&bins[i])) {
            continue;
        }

        if (scope->line_width > 1) {
            int16_t pad = (int16_t)scope->line_width / 2 + 1;
            bins[i].x1 -= pad;
            bins[i].x2 += pad;
            bins[i].y1 -= pad;
            bins[i].y2 += pad;
        }

        sgl_area_selfclip(&bins[i], &waveform_area);
        if (scope_area_is_valid(&bins[i])) {
            sgl_obj_update_area(&bins[i]);
        }
    }

    sgl_system.fbdev.update_flag = 1;
}


static void scope_draw_waveform_channel(sgl_surf_t *surf,
                                        sgl_obj_t *obj,
                                        sgl_scope_t *scope,
                                        uint8_t ch,
                                        int16_t display_min,
                                        int16_t display_max,
                                        int16_t width,
                                        int16_t height)
{
    if (scope->display_counts[ch] <= 1 || !scope->data_buffers[ch]) {
        return;
    }

    uint32_t display_points = scope->max_display_points > 0 ? scope->max_display_points : scope->data_len;
    if (display_points > scope->data_len) {
        display_points = scope->data_len;
    }

    uint32_t data_points = scope->display_counts[ch] < display_points ? scope->display_counts[ch] : display_points;
    if (data_points <= 1) {
        return;
    }

    uint32_t denom = data_points - 1;
    uint32_t current_index = scope->current_indices[ch];
    uint32_t last_index = (current_index == 0) ? scope->data_len - 1 : current_index - 1;
    int16_t value = scope->data_buffers[ch][last_index];
    value = sgl_clamp(value, display_min, display_max);

    const sgl_area_t *dirty = surf->dirty;
    int16_t prev_x = obj->coords.x2;
    int16_t prev_y = scope_map_value_to_y(obj, value, display_min, display_max, height);
    int16_t column_min_y = prev_y;
    int16_t column_max_y = prev_y;
    int16_t column_last_y = prev_y;
    uint8_t can_merge_same_column = (scope->line_width == 1);

    for (uint32_t i = 1; i < data_points; i++) {
        uint32_t prev_index = (current_index >= i + 1) ? (current_index - (i + 1)) : (scope->data_len - (i + 1 - current_index));
        int16_t current_value = sgl_clamp(scope->data_buffers[ch][prev_index], display_min, display_max);
        int16_t current_x = obj->coords.x2 - (int16_t)((i * width) / denom);
        int16_t current_y = scope_map_value_to_y(obj, current_value, display_min, display_max, height);
        int16_t seg_x1 = sgl_min(prev_x, current_x);
        int16_t seg_x2 = sgl_max(prev_x, current_x);

        if (seg_x2 < dirty->x1 || seg_x1 > dirty->x2) {
            prev_x = current_x;
            prev_y = current_y;
            column_min_y = current_y;
            column_max_y = current_y;
            column_last_y = current_y;
            continue;
        }

        if (can_merge_same_column && current_x == prev_x) {
            if (current_y < column_min_y) column_min_y = current_y;
            if (current_y > column_max_y) column_max_y = current_y;
            column_last_y = current_y;
            continue;
        }

        if (can_merge_same_column) {
            scope_draw_vline_fast(surf, &obj->area, prev_x, column_min_y, column_max_y, scope->waveform_colors[ch], scope->alpha);
        }
        scope_draw_line_fast(surf, &obj->area,
                             (sgl_pos_t){ .x = prev_x, .y = column_last_y },
                             (sgl_pos_t){ .x = current_x, .y = current_y },
                             scope->waveform_colors[ch], scope->line_width, scope->alpha);

        prev_x = current_x;
        prev_y = current_y;
        column_min_y = current_y;
        column_max_y = current_y;
        column_last_y = current_y;
    }

    if (can_merge_same_column) {
        scope_draw_vline_fast(surf, &obj->area, prev_x, column_min_y, column_max_y, scope->waveform_colors[ch], scope->alpha);
    }
}


static void scope_draw_grid_lines(sgl_surf_t *surf,
                                  sgl_obj_t *obj,
                                  sgl_scope_t *scope,
                                  int16_t width,
                                  int16_t height,
                                  int16_t x_center,
                                  int16_t y_center)
{
    const sgl_area_t *dirty = surf->dirty;

    if (y_center >= dirty->y1 && y_center <= dirty->y2) {
        if (scope->grid_style) {
            draw_dashed_line(surf, &obj->area, obj->coords.x1, y_center, obj->coords.x2, y_center, scope->grid_style, scope->grid_color);
        } else {
            sgl_draw_fill_hline(surf, &obj->area, y_center, obj->coords.x1, obj->coords.x2, 1, scope->grid_color, scope->alpha);
        }
    }

    if (x_center >= dirty->x1 && x_center <= dirty->x2) {
        if (scope->grid_style) {
            draw_dashed_line(surf, &obj->area, x_center, obj->coords.y1, x_center, obj->coords.y2, scope->grid_style, scope->grid_color);
        } else {
            sgl_draw_fill_vline(surf, &obj->area, x_center, obj->coords.y1, obj->coords.y2, 1, scope->grid_color, scope->alpha);
        }
    }

    for (int i = 1; i < 10; i++) {
        int16_t x_pos = obj->coords.x1 + (width * i / 10);
        if (x_pos >= dirty->x1 && x_pos <= dirty->x2) {
            if (scope->grid_style) {
                draw_dashed_line(surf, &obj->area, x_pos, obj->coords.y1, x_pos, obj->coords.y2, scope->grid_style, scope->grid_color);
            } else {
                sgl_draw_fill_vline(surf, &obj->area, x_pos, obj->coords.y1, obj->coords.y2, 1, scope->grid_color, scope->alpha);
            }
        }
    }

    for (int i = 1; i < 10; i++) {
        int16_t y_pos = obj->coords.y1 + (height * i / 10);
        if (y_pos >= dirty->y1 && y_pos <= dirty->y2) {
            if (scope->grid_style) {
                draw_dashed_line(surf, &obj->area, obj->coords.x1, y_pos, obj->coords.x2, y_pos, scope->grid_style, scope->grid_color);
            } else {
                sgl_draw_fill_hline(surf, &obj->area, y_pos, obj->coords.x1, obj->coords.x2, 1, scope->grid_color, scope->alpha);
            }
        }
    }
}


static void scope_draw_dirty_background(sgl_surf_t *surf, sgl_obj_t *obj, sgl_scope_t *scope)
{
    sgl_area_t dirty_bg = *surf->dirty;
    if (!sgl_area_selfclip(&dirty_bg, &obj->coords)) {
        return;
    }

    sgl_draw_fill_rect(surf, &obj->area, &dirty_bg, 0, scope->bg_color, scope->alpha);
}

// Oscilloscope drawing callback function
static void scope_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_scope_t *scope = (sgl_scope_t*)obj;

    if (evt->type == SGL_EVENT_DESTROYED) {
        return;
    }

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        // Skip drawing if object is completely outside screen bounds
        if (obj->area.x2 < surf->x1 || obj->area.x1 > surf->x2 ||
            obj->area.y2 < surf->y1 || obj->area.y1 > surf->y2) {
            return; // Object is fully off-screen; no need to draw
        }

        bool full_redraw = (surf->dirty->x1 <= obj->coords.x1 && surf->dirty->y1 <= obj->coords.y1 &&
                            surf->dirty->x2 >= obj->coords.x2 && surf->dirty->y2 >= obj->coords.y2);

        if (full_redraw) {
            sgl_draw_rect_t bg_rect = {
                .color = scope->bg_color,
                .alpha = scope->alpha,
                .radius = 0,
                .border = scope->border_width,
            };

            sgl_draw_rect(surf, &obj->area, &obj->coords, &bg_rect);
        } else {
            scope_draw_dirty_background(surf, obj, scope);
        }

        // Compute waveform display parameters
        int16_t display_min;
        int16_t display_max;
        int16_t actual_min = scope->min_value;
        int16_t actual_max = scope->max_value;
        scope_get_display_range(scope, &display_min, &display_max);

        if (scope->auto_scale &&
            scope->display_counts[0] > 0 &&
            scope->running_min != SGL_SCOPE_INT16_MAX &&
            scope->running_max != SGL_SCOPE_INT16_MIN) {
            actual_min = scope->running_min;
            actual_max = scope->running_max;
        }
        
        // Draw grid lines
        int16_t width = obj->coords.x2 - obj->coords.x1;
        int16_t height = obj->coords.y2 - obj->coords.y1;
        int16_t x_center = (obj->coords.x1 + obj->coords.x2) / 2;
        int16_t y_center = obj->coords.y1 + (int32_t)(height * (display_max - (display_min + display_max) / 2)) / (display_max - display_min);

        scope_draw_grid_lines(surf, obj, scope, width, height, x_center, y_center);

        // Draw waveform data for each channel
        for (uint8_t ch = 0; ch < scope->channel_count; ch++) {
            scope_draw_waveform_channel(surf, obj, scope, ch, display_min, display_max, width, height);
        }

        // Draw Y-axis labels if enabled and font is set
        if (scope->show_y_labels && scope->y_label_font) {
            char label_text[16];
            sgl_area_t text_area = {
                .x1 = obj->coords.x1 + 2,
                .y1 = obj->coords.y1,
                .x2 = obj->coords.x1 + 50,
                .y2 = obj->coords.y2
            };

            sgl_area_selfclip(&text_area, &obj->area);
            
            // Display actual maximum value of waveform data
            sgl_snprintf(label_text, sizeof(label_text), "%d", actual_max);
            sgl_draw_string(surf, &text_area, obj->coords.x1 + 2, obj->coords.y1 + 2, 
                        label_text, scope->y_label_color, scope->alpha, scope->y_label_font);
            
            // Display actual minimum value of waveform data
            sgl_snprintf(label_text, sizeof(label_text), "%d", actual_min);
            sgl_draw_string(surf, &text_area, obj->coords.x1 + 2, obj->coords.y2 - scope->y_label_font->font_height - 2, 
                        label_text, scope->y_label_color, scope->alpha, scope->y_label_font);
            
            // Display mid-range value of actual waveform
            int16_t mid_value = (actual_max + actual_min) / 2;
            sgl_snprintf(label_text, sizeof(label_text), "%d", mid_value);
            sgl_draw_string(surf, &text_area, obj->coords.x1 + 2, y_center - scope->y_label_font->font_height/2, 
                        label_text, scope->y_label_color, scope->alpha, scope->y_label_font);
        }
    }
}


// Create an oscilloscope object
sgl_obj_t* sgl_scope_create(sgl_obj_t* parent)
{
    sgl_scope_t *scope = sgl_malloc(sizeof(sgl_scope_t));
    if(scope == NULL) {
        return NULL;
    }
    
    memset(scope, 0, sizeof(sgl_scope_t));
    
    sgl_obj_t *obj = &scope->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = scope_construct_cb;
    sgl_obj_set_border_width(obj, SGL_THEME_BORDER_WIDTH);
    
    // Initialize default parameters for single channel
    scope->channel_count = 1;        // Default to 1 channel

    scope->data_buffers[0] = NULL;    // User must set via sgl_scope_set_channel_data_buffer
    scope->waveform_colors[0] = sgl_rgb(0, 255, 0);   // Green waveform for channel 0
    scope->current_indices[0] = 0;
    scope->display_counts[0] = 0;
    
    scope->bg_color = sgl_rgb(0, 0, 0);           // Black background
    scope->grid_color = sgl_rgb(50, 50, 50);      // Gray grid lines
    scope->border_width = 0;                      // border width is 0
    scope->border_color = sgl_rgb(150, 150, 150); // Light gray outer border
    scope->min_value = 0;
    scope->max_value = 0xFFFF;
    scope->running_min = SGL_SCOPE_INT16_MAX;   // Initialize runtime minimum to max range value
    scope->running_max = SGL_SCOPE_INT16_MIN;   // Initialize runtime maximum to min range value
    scope->auto_scale = 1;        // Enable auto-scaling by default
    scope->line_width = 2;        // Default line thickness
    scope->max_display_points = 0; // Display all points by default
    scope->show_y_labels = 0;      // Hide Y-axis labels by default
    scope->alpha = SGL_ALPHA_MAX;  // Fully opaque by default
    scope->grid_style = 0;         // Solid grid lines by default
    scope->dirty_rect_count = SGL_SCOPE_DEFAULT_DIRTY_RECT_COUNT;
    scope->y_label_font = NULL;    // No font by default
    scope->y_label_color = sgl_rgb(255, 255, 255); // White label color
    scope->data_len = 0;            // No data buffer initially

    return obj;
}

/**
 * @brief Append a new data point to the oscilloscope for a specific channel
 * @param obj The oscilloscope object
 * @param channel Channel number (0-based)
 * @param value The new data point
 * @note This function appends a new data point to the specified channel of the oscilloscope. 
 *       If the oscilloscope is configured to auto-scale, the function updates the running minimum and maximum values.
 *       The function also updates the display count and pushes local dirty areas for waveform redraw.
 */
void sgl_scope_append_data(sgl_obj_t* obj, uint8_t channel, int16_t value)
{
    sgl_scope_t *scope = (sgl_scope_t*)obj;
    int16_t display_min;
    int16_t display_max;
    
    if (channel >= scope->channel_count || !scope->data_buffers[channel] || scope->data_len == 0) {
        return;
    }

    // Append the data point to the buffer
    scope->data_buffers[channel][scope->current_indices[channel]] = value;

    // Update running min/max for auto-scale (cheap incremental path)
    if (scope->auto_scale) {
        if (scope->running_min == SGL_SCOPE_INT16_MAX && scope->running_max == SGL_SCOPE_INT16_MIN) {
            scope->running_min = value;
            scope->running_max = value;
        } else {
            if (value < scope->running_min) {
                scope->running_min = value;
            }
            if (value > scope->running_max) {
                scope->running_max = value;
            }
        }
    }

    // Advance write index (ring buffer)
    if (sgl_is_pow2(scope->data_len)) {
        scope->current_indices[channel] = (scope->current_indices[channel] + 1) & (scope->data_len - 1);
    } else {
        scope->current_indices[channel] = (scope->current_indices[channel] + 1) % scope->data_len;
    }

    // Update display count for this channel
    if (scope->display_counts[channel] < (uint16_t)scope->data_len) {
        scope->display_counts[channel]++;
    }

    // For auto-scale, periodically recompute running min/max when buffer wraps
    if (scope->auto_scale &&
        scope->display_counts[channel] >= scope->data_len &&
        scope->current_indices[channel] == 0) {

        int16_t new_min = scope->data_buffers[0][0];
        int16_t new_max = new_min;

        for (uint8_t ch = 0; ch < scope->channel_count; ch++) {
            if (!scope->data_buffers[ch]) {
                continue;
            }
            uint32_t end_index = (scope->display_counts[ch] < scope->data_len) ? scope->display_counts[ch] : scope->data_len;
            for (uint32_t i = 0; i < end_index; i++) {
                int16_t v = scope->data_buffers[ch][i];
                if (v < new_min) new_min = v;
                if (v > new_max) new_max = v;
            }
        }

        scope->running_min = new_min;
        scope->running_max = new_max;
    }

    scope_get_display_range(scope, &display_min, &display_max);
    scope_push_waveform_dirty_rects(scope, channel, display_min, display_max);
}

/**
 * @brief set scope channel count
 * @param obj scope object
 * @param channel_count number of channels (1-4)
 * @return none
 */
void sgl_scope_set_channel_count(sgl_obj_t* obj, uint8_t channel_count)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    
    if (channel_count == 0 || channel_count > SGL_SCOPE_MAX_CHANNELS) {
        SGL_LOG_ERROR("Invalid channel count: %d (must be 1-%d)\n", channel_count, SGL_SCOPE_MAX_CHANNELS);
        return;
    }
    
    // Update channel count
    scope->channel_count = channel_count;
     
    // Initialize channel data
    for (uint8_t i = 0; i < channel_count; i++) {
        scope->data_buffers[i] = NULL;  // User must set via sgl_scope_set_channel_data_buffer
        scope->current_indices[i] = 0;
        scope->display_counts[i] = 0;
        
        // Set default colors for each channel
        switch (i) {
            case 0:
                scope->waveform_colors[i] = sgl_rgb(0, 255, 0);   // Green
                break;
            case 1:
                scope->waveform_colors[i] = sgl_rgb(255, 0, 0);   // Red
                break;
            case 2:
                scope->waveform_colors[i] = sgl_rgb(0, 0, 255);   // Blue
                break;
            case 3:
                scope->waveform_colors[i] = sgl_rgb(255, 255, 0); // Yellow
                break;
            default:
                scope->waveform_colors[i] = sgl_rgb(255, 255, 255); // White
                break;
        }
    }

    for (uint8_t i = channel_count; i < SGL_SCOPE_MAX_CHANNELS; i++) {
        scope->data_buffers[i] = NULL;
        scope->current_indices[i] = 0;
        scope->display_counts[i] = 0;
    }
     
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope data buffer for a specific channel
 * @param obj scope object
 * @param channel channel number (0-based)
 * @param data_buffer data buffer
 * @param data_len data length
 * @return none
 */
void sgl_scope_set_channel_data_buffer(sgl_obj_t* obj, uint8_t channel, int16_t *data_buffer, uint32_t data_len)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    
    if (channel >= scope->channel_count) {
        SGL_LOG_ERROR("Invalid channel: %d (max %d)\n", channel, scope->channel_count - 1);
        return;
    }
    
    scope->data_buffers[channel] = data_buffer;
    scope->data_len = data_len;
    scope->current_indices[channel] = 0;
    scope->display_counts[channel] = 0;
     
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get scope data for a specific channel
 * @param obj scope object
 * @param channel channel number (0-based)
 * @param index data index
 * @return data value
 */
int16_t sgl_scope_get_channel_data(sgl_obj_t* obj, uint8_t channel, uint32_t index)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    if (channel >= scope->channel_count || index >= scope->data_len) 
        return 0;

    return scope->data_buffers[channel][index];
}

/**
 * @brief set scope max display points
 * @param obj scope object
 * @param max_points max display points
 * @return none
 */
void sgl_scope_set_max_display_points(sgl_obj_t* obj, uint8_t max_points)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->max_display_points = max_points;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope waveform color for a specific channel
 * @param obj scope object
 * @param channel channel number (0-based)
 * @param color waveform color
 * @return none
 */
void sgl_scope_set_channel_waveform_color(sgl_obj_t* obj, uint8_t channel, sgl_color_t color)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    
    if (channel >= scope->channel_count) {
        SGL_LOG_ERROR("Invalid channel: %d (max %d)\n", channel, scope->channel_count - 1);
        return;
    }
    
    scope->waveform_colors[channel] = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope background color
 * @param obj scope object
 * @param color background color
 * @return none
 */
void sgl_scope_set_bg_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope grid line color
 * @param obj scope object
 * @param color grid line color
 * @return none
 */
void sgl_scope_set_grid_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->grid_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope range
 * @param obj scope object
 * @param min_value minimum value
 * @param max_value maximum value
 * @return none
 */
void sgl_scope_set_range(sgl_obj_t* obj, uint16_t min_value, uint16_t max_value)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->min_value = min_value;
    scope->max_value = max_value;
    //scope->auto_scale = 0;  // disable auto scale
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope line width
 * @param obj scope object
 * @param width line width
 * @return none
 */
void sgl_scope_set_line_width(sgl_obj_t* obj, uint8_t width)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->line_width = width;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief enable/disable auto scale
 * @param obj scope object
 * @param enable enable/disable
 * @return none
 */
void sgl_scope_enable_auto_scale(sgl_obj_t* obj, bool enable)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->auto_scale = (uint8_t)enable;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope alpha
 * @param obj scope object
 * @param alpha alpha
 * @return none
 */
void sgl_scope_set_alpha(sgl_obj_t* obj, uint8_t alpha)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief show/hide Y axis labels
 * @param obj scope object
 * @param show show/hide
 * @return none
 */
void sgl_scope_show_y_labels(sgl_obj_t* obj, bool show)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->show_y_labels = (uint8_t)show;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope Y axis labels font
 * @param obj scope object
 * @param font font
 * @return none
 */
void sgl_scope_set_y_label_font(sgl_obj_t* obj, const sgl_font_t *font)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->y_label_font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope Y axis labels color
 * @param obj scope object
 * @param color color
 * @return none
 */
void sgl_scope_set_y_label_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->y_label_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope border color
 * @param obj scope object
 * @param color border color
 * @return none
 */
void sgl_scope_set_border_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope border width
 * @param obj scope object
 * @param width border width
 * @return none
 */
void sgl_scope_set_border_width(sgl_obj_t* obj, uint8_t width)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->border_width = width;
    sgl_obj_set_border_width(obj, width);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope grid line
 * @param obj scope object
 * @param style grid size, 0: solid line，other: dashed line
 * @return none
 */
void sgl_scope_set_grid_line(sgl_obj_t* obj, uint8_t grid)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->grid_style = grid;
    sgl_obj_set_dirty(obj);
}


void sgl_scope_set_dirty_rect_count(sgl_obj_t* obj, uint8_t count)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);

    if (count == 0) {
        count = 1;
    }
    if (count > SGL_DIRTY_AREA_NUM_MAX) {
        count = SGL_DIRTY_AREA_NUM_MAX;
    }

    scope->dirty_rect_count = count;
}
