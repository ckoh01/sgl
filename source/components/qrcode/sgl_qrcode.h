/* source/components/qrcode/sgl_qrcode.h
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

#ifndef __SGL_QRCODE_H__
#define __SGL_QRCODE_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "qrcodegen.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_SGL_QRCODE_QR_VERSION_MAX
#warning "CONFIG_SGL_QRCODE_QR_VERSION_MAX is not defined, using default value 5. Define it in CMakeLists.txt to override."
#define CONFIG_SGL_QRCODE_QR_VERSION_MAX    5
#endif

#ifndef SGL_QRCODE_QR_QUIET_ZONE_DEFAULT
#define SGL_QRCODE_QR_QUIET_ZONE_DEFAULT    1
#endif

#ifndef SGL_QRCODE_QR_SCALE_DEFAULT
#define SGL_QRCODE_QR_SCALE_DEFAULT         3
#endif

/**
 * @brief sgl qrcode struct
 * @param obj sgl general object
 * @param qr_buf pointer to the encoded QR matrix buffer
 * @param qr_version QR code version (1 to 40), Modules formula = 4 * version + 17
 * @param qr_quiet_zone quiet zone in modules (minimum 4 for standard QR codes, 2 is acceptable for internal use on a small screen where the scanner is held close)
 * @param qr_scale scale factor for each module (e.g., 2 means each module is drawn as a 2x2 pixel square)
 * @param alpha alpha value (0 to 255)
 */
typedef struct sgl_qrcode {
    sgl_obj_t          obj;
    sgl_obj_t         *canvas;
    uint16_t           width;
    uint16_t           height;
    uint8_t*           qr_buf;    /* freed on destroy event */
    uint8_t            qr_version;
    uint8_t            qr_quiet_zone : 4;
    uint8_t            qr_scale : 4;
    uint8_t            alpha;

} sgl_qrcode_t;

sgl_obj_t* sgl_qrcode_create(sgl_obj_t* parent);
void sgl_qrcode_set_url(sgl_obj_t *obj, const char *url);
void sgl_qrcode_set_qr_version(sgl_obj_t *obj, uint8_t version);
void sgl_qrcode_set_qr_scale(sgl_obj_t *obj, uint8_t scale);
void sgl_qrcode_set_qr_quiet_zone(sgl_obj_t *obj, uint8_t quiet_zone);
void sgl_qrcode_set_alpha(sgl_obj_t *obj, uint8_t alpha);
void sgl_qrcode_set_size(sgl_obj_t *obj, uint16_t width, uint16_t height);

#ifdef __cplusplus
}
#endif

#endif // __SGL_QRCODE_H__
