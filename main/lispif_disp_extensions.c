/*
	Copyright 2023 Benjamin Vedder	benjamin@vedder.se
	Copyright 2023 Joel Svensson    svenssonjoel@yahoo.se

	This file is part of the VESC firmware.

	The VESC firmware is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The VESC firmware is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "lispif_disp_extensions.h"
#include "lispif.h"
#include "lbm_utils.h"
#include "lbm_custom_type.h"
#include "commands.h"

#include <math.h>

static const char *image_buffer_desc = "Image-Buffer";


static lbm_uint symbol_indexed2 = 0;
static lbm_uint symbol_indexed4 = 0;
static lbm_uint symbol_rgb332 = 0;
static lbm_uint symbol_rgb565 = 0;
static lbm_uint symbol_rgb888 = 0;

static color_format_t sym_to_color_format(lbm_value v) {
	lbm_uint s = lbm_dec_sym(v);
	if (s == symbol_indexed2) return indexed2;
	if (s == symbol_indexed4) return indexed4;
	if (s == symbol_rgb332) return rgb332;
	if (s == symbol_rgb565) return rgb565;
	if (s == symbol_rgb888) return rgb888;
	return format_not_supported;

}

static bool image_buffer_destructor(lbm_uint value) {
	image_buffer_t *img = (image_buffer_t*)lbm_get_custom_value(value);
	lbm_free((void*)img->data);
	lbm_free((void*)value);
	return true;
}

static uint32_t image_dims_to_size_bytes(color_format_t fmt, uint16_t width, uint16_t height) {
	uint32_t num_pix = (uint32_t)width * (uint32_t)height;
	switch(fmt) {
	case indexed2:
		if (num_pix % 8 != 0) return (num_pix / 8) + 1;
		else return (num_pix / 8);
		break;
	case indexed4:
		if (num_pix % 4 != 0) return (num_pix / 4) + 1;
		else return (num_pix / 4);
		break;
	case rgb332:
		return num_pix;
		break;
	case rgb565:
		return num_pix * 2;
		break;
	case rgb888:
		return num_pix * 3;
	default:
		return 0;
	}

}

static lbm_value image_buffer_lift(uint8_t *buf, uint8_t buf_offset, color_format_t fmt, uint16_t width, uint16_t height) {

	image_buffer_t *img = lbm_malloc(sizeof(image_buffer_t));
	if (!img) return ENC_SYM_MERROR;

	img->data_offset = buf_offset;
	img->data = buf;
	img->fmt = fmt;
	img->width = width;
	img->height = height;

	lbm_value res;
	lbm_custom_type_create((lbm_uint)img,
			image_buffer_destructor,
			image_buffer_desc,
			&res);
	return res;
}

static lbm_value image_buffer_allocate(color_format_t fmt, uint16_t width, uint16_t height) {

	uint32_t size_bytes = image_dims_to_size_bytes(fmt, width, height);

	uint8_t *buf = lbm_malloc(size_bytes);
	if (!buf) {
		return ENC_SYM_MERROR;
	}
	memset(buf, 0, size_bytes);
	lbm_value res = image_buffer_lift(buf, 0, fmt, width, height);
	if (lbm_is_symbol(res)) { /* something is wrong, free */
		lbm_free(buf);
	}
	return res;
}

// Exported interface

bool lispif_disp_is_image_buffer(lbm_value v) {
	return ((lbm_uint)lbm_get_custom_descriptor(v) == (lbm_uint)image_buffer_desc);
}

// Register symbols

static bool register_symbols(void) {
	bool res = true;
	res = res && lbm_add_symbol_const("indexed2", &symbol_indexed2);
	res = res && lbm_add_symbol_const("indexed4", &symbol_indexed4);
	res = res && lbm_add_symbol_const("rgb332", &symbol_rgb332);
	res = res && lbm_add_symbol_const("rgb565", &symbol_rgb565);
	res = res && lbm_add_symbol_const("rgb888", &symbol_rgb888);
	return res;
}

// Internal functions

static uint8_t rgb888to332(uint32_t rgb) {
	uint8_t r = (uint8_t)(rgb >> (16 + 5));
	uint8_t g = (uint8_t)(rgb >> (8 + 5));
	uint8_t b = (uint8_t)(rgb >> 6);
	r <<= 5;
	g = (g & 0x7) << 2;  ;
	b = (b & 0x3);
	uint8_t rgb332 = r | g | b;
	return rgb332;
}

static uint16_t rgb888to565(uint32_t rgb) {
	uint16_t r = (uint16_t)(rgb >> (16 + 3));
	uint16_t g = (uint16_t)(rgb >> (8 + 2));
	uint16_t b = (uint16_t)(rgb >> 3);
	r <<= 11;
	g = (g & 0x3F) << 5;
	b = (b & 0x1F);
	uint16_t rgb565 = r | g | b;
	return rgb565;
}

static uint32_t rgb332to888(uint8_t rgb) {
	uint32_t r = (uint32_t)((rgb>>5) & 0x7);
	uint32_t g = (uint32_t)((rgb>>2) & 0x7);
	uint32_t b = (uint32_t)(rgb & 0x3);
	uint32_t rgb888 = r << (16 + 5) | g << (8 + 5) | b << 6;
	return rgb888;
}

static uint32_t  rgb565to888(uint16_t rgb) {
	uint32_t r = (uint32_t)(rgb >> 11);
	uint32_t g = (uint32_t)((rgb >> 5) & 0x1F);
	uint32_t b = (uint32_t)(rgb & 0x1F);
	uint32_t rgb888 = r << (16 + 3) | g << (8 + 2) | b << 3;
	return rgb888;
}


static void image_buffer_clear(image_buffer_t *img, uint32_t cc) {
	uint32_t img_size = (uint32_t)img->width * img->height;
	switch (img->fmt) {
	case indexed2: {
		int extra = img_size & 0x7;
		int bytes = (img_size >> 3) + (extra ? 1 : 0);
		uint8_t c8 = (cc & 1) ? 0xFFFF : 0x0;
		memset(img->data+img->data_offset, c8, bytes);
	}
	break;
	case indexed4: {
		static const uint8_t index4_table[4] = {0x00, 0x55, 0xAA, 0xFF};
		int extra = img_size & 0x3;
		int bytes = (img_size >> 2) + (extra ? 1 : 0);
		uint8_t ix = (cc & 0x3);
		memset(img->data+img->data_offset, index4_table[ix], bytes);
	}
	break;
	case rgb332: {
		memset(img->data+img->data_offset, rgb888to332(cc), img_size);
	}
	break;
	case rgb565: {
		uint16_t c = rgb888to565(cc);
		uint8_t *dp = (uint8_t*)img->data+img->data_offset;
		for (int i = 0; i < img_size/2; i +=2) {
			dp[i] = (uint8_t)c >> 8;
			dp[i+1] = (uint8_t)c;
		}
	}
	break;
	case rgb888: {
		uint8_t *dp = (uint8_t*)img->data+img->data_offset;
		for (int i = 0; i < img_size * 3; i+= 3) {
			dp[i]   = (uint8_t)cc >> 16;
			dp[i+1] = (uint8_t)cc >> 8;
			dp[i+2] = (uint8_t)cc;
		}
	}
	break;
	default:
		break;
	}
}

static void putpixel(image_buffer_t* img, uint16_t x, uint16_t y, uint32_t c) {
	uint16_t w = img->width;
	uint16_t h = img->height;
	if (x < w && y < h) {
		uint8_t *data = img->data+img->data_offset;
		switch(img->fmt) {
		case indexed2: {
			uint32_t pos = y * w + x;
			uint32_t byte = pos >> 3;
			uint32_t bit  = (pos & 0x7);
			if (c) {
				data[byte] |= (1 << ( 7 - bit));
			} else {
				data[byte] &= ~(1 << ( 7 - bit));
			}
			break;
		}
		case indexed4: {
			int pos = y*(w<<1) + (x<<1);
			uint32_t byte = pos >> 3;
			uint32_t bit  = 7 - (pos & 0x7);
			uint8_t  val  = (uint8_t)(c & 0x3);
			data[byte] = (data[byte] & (0x3 << (7 - bit))) | val;
			break;
		}
		case rgb332: {
			int pos = y*w + x;
			data[pos] = rgb888to332(c);
			break;
		}
		case rgb565: {
			int pos = y*(w<<1) + (x<<1) ;
			//uint16_t *dp = (uint16_t*)img->data[pos];
			//dp[pos] = rgb888to565(c);
			data[pos] = (uint8_t)c >> 8;
			data[pos+1] = (uint8_t)c;
			break;
		}
		case rgb888: {
			int pos = y*(w*3) + (x*3);
			data[pos] = (uint8_t)c>>16;
			data[pos+1] = (uint8_t)c>>8;
			data[pos+2] = (uint8_t)c;
			break;
		}
		default:
			break;
		}
	}
}

static uint32_t getpixel(image_buffer_t* img, uint16_t x, uint16_t y) {
	uint16_t w = img->width;
	uint16_t h = img->height;
	if (x < w && y < h) {
		uint8_t *data = img->data+img->data_offset;
		switch(img->fmt) {
		case indexed2: {
			uint32_t pos = y * w + x;
			uint32_t byte = pos >> 3;
			uint32_t bit  = 7 - (pos & 0x7);
			return (uint32_t)(data[byte] >> bit) & 0x1;
		}
		case indexed4: {
			int pos = y*(w<<1) + (x<<1);
			uint32_t byte = pos >> 3;
			uint32_t bit  = 7 - (pos & 0x7);
			return (uint32_t)(data[byte] >> bit) & 0x3;
		}
		case rgb332: {
			int pos = y*w + x;
			return rgb332to888(data[pos]);
		}
		case rgb565: {
			int pos = y*(w<<1) + (x<<1);
			uint16_t *dp = (uint16_t*)img->data[pos];
			uint16_t c = ((uint16_t)data[pos] << 8) | (uint16_t)data[pos+1];
			return rgb565to888(dp[pos]);
		}
		case rgb888: {
			int pos = y*(w*3) + (x*3);
			uint32_t r = data[pos];
			uint32_t g = data[pos+1];
			uint32_t b = data[pos+2];
			return (r << 16 | g << 8 | b);
		}
		default:
			break;
		}
	}
	return 0;
}


static void h_line(image_buffer_t* img, int16_t x, int16_t y, uint16_t len, uint32_t c) {

	if (len == 0 || y < 0 || y > img->height) return;
	if (x < 0) x = 0;
	if (x + len > img->width) len -= ((x + len) - img->width);

	for (int i = 0; i < len; i ++) {
		putpixel(img, x+i, y, c);
	}
}

static void draw_line(image_buffer_t *img, int x0, int y0, int x1, int y1, uint32_t c) {
	int dx = abs(x1 - x0);
	int sx = x0 < x1 ? 1 : -1;
	int dy = -abs(y1 - y0);
	int sy = y0 < y1 ? 1 : -1;
	int error = dx + dy;

	while (true) {
		putpixel(img, x0, y0,c);
		if (x0 == x1 && y0 == y1) {
			break;
		}
		if ((error * 2) >= dy) {
			if (x0 == x1) {
				break;
			}
			error += dy;
			x0 += sx;
		}
		if ((error * 2) <= dx) {
			if (y0 == y1) {
				break;
			}
			error += dx;
			y0 += sy;
		}
	}
}

static void circle(image_buffer_t *img, int x, int y, int radius, bool fill, uint32_t color) {
	if (fill) {
		for(int y1 = -radius;y1 <= 0;y1++) {
			for(int x1 =- radius;x1 <= 0;x1++) {
				if(x1 * x1 + y1 * y1 <= radius * radius) {
					h_line(img, x + x1, y + y1, 2 * (-x1), color);
					h_line(img, x + x1, y - y1, 2 * (-x1), color);
					break;
				}
			}
		}
	} else {
		int x0 = 0;
		int y0 = radius;
		int d = 5 - 4*radius;
		int da = 12;
		int db = 20 - 8*radius;
		while (x0 < y0) {
			putpixel(img, x + x0, y + y0, color);
			putpixel(img, x + x0, y - y0, color);
			putpixel(img, x - x0, y + y0, color);
			putpixel(img, x - x0, y - y0, color);
			putpixel(img, x + y0, y + x0, color);
			putpixel(img, x + y0, y - x0, color);
			putpixel(img, x - y0, y + x0, color);
			putpixel(img, x - y0, y - x0, color);
			if (d < 0) { d = d + da; db = db+8; }
			else  { y0 = y0 - 1; d = d+db; db = db + 16; }
			x0 = x0+1;
			da = da + 8;
		}
	}
}

static void img_putc(image_buffer_t *img, int x, int y, uint32_t fg, uint32_t bg, uint8_t *font_data, uint8_t ch) {

	uint8_t w = font_data[0];
	uint8_t h = font_data[1];
	uint8_t char_num = font_data[2];
//	uint8_t bits_per_pixel = font_data[3];

	if (char_num == 10) {
		ch -= '0';
	} else {
		ch -= ' ';
	}

	if (ch >= char_num) {
		return;
	}

	for (int i = 0; i < w; i ++) {
		for (int j = 0; j < h; j ++) {
			int f_ind = (j * w + i);
			int f_pos = 4 + ch * (w * h) / 8 + (f_ind / 8);
			int bit_pos = f_ind % 8;
			int bit = font_data[f_pos] & (1 << bit_pos);
			putpixel(img, x+i, y+j, bit ? fg : bg);
		}
	}
}

void blit_rot_scale( image_buffer_t *img_dest, image_buffer_t *img_src,
					 int x, int y, // Where on display
					 float xr, float yr, // Pixel to rotate around
					 float rot, // Rotation angle in degrees
					 float scale, // Scale factor
					 int32_t transparent_color) {

	int width = img_src->width;
	int height = img_src->height;
	int x_start = 0;
	int x_width = img_dest->width;
	int y_start = 0;
	int y_width = img_dest->height;

	float sr = sinf(-rot * M_PI / 180.0f);
	float cr = cosf(-rot * M_PI / 180.0f);

	xr *= scale;
	yr *= scale;

	int x_end = (x_start + x_width);
	int y_end = (y_start + y_width);

	if (x_start < 0) {
		x_start = 0;
	}

	if (x_end > x_width) {
		x_end = x_width;
	}

	if (y_start < 0) {
		y_start = 0;
	}

	if (y_end > y_width) {
		y_end = y_width;
	}

	const int fp_scale = 1000;

	int sr_i = sr * fp_scale;
	int cr_i = cr * fp_scale;
	int xr_i = xr;
	int yr_i = yr;
	int scale_i = scale * (float)fp_scale;

	for (int j = y_start;j < y_end;j++) {
		for (int i = x_start;i < x_end;i++) {
			int px = (i - x - xr_i) * cr_i + (j - y - yr_i) * sr_i;
			int py = -(i - x - xr_i) * sr_i + (j - y - yr_i) * cr_i;

			px += xr_i * fp_scale;
			py += yr_i * fp_scale;

			px /= scale_i;
			py /= scale_i;

			if (px >= 0 && px < width && py >= 0 && py < height) {
				uint32_t p = getpixel(img_src, px, py);

				if (p != (uint32_t)transparent_color) {
	 				putpixel(img_dest, i, j, p);
				}
			}
		}
	}
}

// Extensions

static lbm_value ext_image_dims(lbm_value *args, lbm_uint argn) {
	lbm_value res = ENC_SYM_TERROR;
	if (argn == 1 &&
		lispif_disp_is_image_buffer(args[0])) {
		image_buffer_t *img = (image_buffer_t*)lbm_get_custom_value(args[0]);

		lbm_value dims = lbm_heap_allocate_list(2);
		if (lbm_is_symbol(dims)) return dims;
		lbm_value curr = dims;
		lbm_set_car(curr, lbm_enc_i(img->width));
		curr = lbm_cdr(curr);
		lbm_set_car(curr, lbm_enc_i(img->height));
		res = dims;
	}
	return res;
}

static lbm_value ext_image_buffer(lbm_value *args, lbm_uint argn) {
	lbm_value res = ENC_SYM_TERROR;

	if (argn == 3 &&
		lbm_is_symbol(args[0]) &&
		lbm_is_number(args[1]) &&
		lbm_is_number(args[2])) {

		color_format_t fmt = sym_to_color_format(args[0]);
		if (fmt != format_not_supported) {
			res = image_buffer_allocate(fmt, lbm_dec_as_u32(args[1]), lbm_dec_as_u32(args[2]));
		}
	}
	return res;
}

static lbm_value ext_image_buffer_from_bin(lbm_value *args, lbm_uint argn) {
	lbm_value res = ENC_SYM_TERROR;

	if (argn == 1 &&
		lbm_is_array(args[0])) {

		lbm_value arr = args[0];
		//color_format_t fmt = sym_to_color_format(args[1]);
		lbm_array_header_t *array = (lbm_array_header_t *)lbm_car(arr);
		uint8_t *data = (uint8_t*)array->data;
		uint16_t w = ((uint16_t)data[0]) << 8 | ((uint16_t)data[1]);
		uint16_t h = ((uint16_t)data[2]) << 8 | ((uint16_t)data[3]);
		uint8_t bits = data[4];

		color_format_t fmt;
		switch(bits) {
		case 1: fmt = indexed2; break;
		case 2: fmt = indexed4; break;
		case 8: fmt = rgb332; break;
		case 16: fmt = rgb565; break;
		case 24: fmt = rgb888; break;
		default: fmt = format_not_supported; break; // return ENC_SYM_TERROR;
		}

		res = image_buffer_lift((uint8_t*)array->data, 5, fmt,  w, h);
		if (!lbm_is_symbol(res)) {  // Take ownership of array
			lbm_set_car(arr,ENC_SYM_NIL);
			lbm_set_cdr(arr,ENC_SYM_NIL);
			lbm_set_ptr_type(arr, LBM_TYPE_CONS);
		}
	}
	return res;
}

static lbm_value ext_clear(lbm_value *args, lbm_uint argn) {
	lbm_value res = ENC_SYM_TERROR;
	if (argn == 2 &&
		lispif_disp_is_image_buffer(args[0]) &&
		lbm_is_number(args[1])){

		uint32_t c = lbm_dec_as_u32(args[1]);
		image_buffer_t *img = (image_buffer_t*)lbm_get_custom_value(args[0]);
		image_buffer_clear(img, c);
		res = ENC_SYM_TRUE;
	}
	return res;
}

static lbm_value ext_putpixel(lbm_value *args, lbm_uint argn) {
	lbm_value res = ENC_SYM_TERROR;
	if (argn == 4 &&
		lispif_disp_is_image_buffer(args[0]) &&
		lbm_is_number(args[1]) &&
		lbm_is_number(args[2]) &&
		lbm_is_number(args[3])) {

		uint16_t x = (uint16_t)lbm_dec_as_u32(args[1]);
		uint16_t y = (uint16_t)lbm_dec_as_u32(args[2]);
		uint32_t c = lbm_dec_as_u32(args[3]);
		image_buffer_t *img = (image_buffer_t*)lbm_get_custom_value(args[0]);
		putpixel(img, x, y, c);
		res = ENC_SYM_TRUE;
	}
	return res;
}

static lbm_value ext_line(lbm_value *args, lbm_uint argn) {
	lbm_value res = ENC_SYM_TERROR;
	if (argn == 6 &&
		lispif_disp_is_image_buffer(args[0]) &&
		lbm_is_number(args[1]) &&
		lbm_is_number(args[2]) &&
		lbm_is_number(args[3]) &&
		lbm_is_number(args[4]) &&
		lbm_is_number(args[5])) {

		image_buffer_t *img = (image_buffer_t*)lbm_get_custom_value(args[0]);

		int x0 = lbm_dec_as_u32(args[1]);
		int y0 = lbm_dec_as_u32(args[2]);
		int x1 = lbm_dec_as_u32(args[3]);
		int y1 = lbm_dec_as_u32(args[4]);
		uint32_t fg = lbm_dec_as_u32(args[5]);

		draw_line(img, x0, y0, x1, y1, fg);

		res = ENC_SYM_TRUE;
	}
	return res;
}

static lbm_value ext_circle(lbm_value *args, lbm_uint argn) {
	lbm_value res = ENC_SYM_TERROR;
	if (argn == 6 &&
		lispif_disp_is_image_buffer(args[0]) &&
		lbm_is_number(args[1]) &&
		lbm_is_number(args[2]) &&
		lbm_is_number(args[3]) &&
		lbm_is_number(args[4]) &&
		lbm_is_number(args[5])) {

		image_buffer_t *img = (image_buffer_t*)lbm_get_custom_value(args[0]);

		int x = lbm_dec_as_i32(args[1]);
		int y = lbm_dec_as_i32(args[2]);
		int radius = lbm_dec_as_u32(args[3]);
		int fill = lbm_dec_as_u32(args[4]);
		uint32_t fg = lbm_dec_as_u32(args[5]);
		circle(img, x, y, radius, fill, fg);
		res = ENC_SYM_TRUE;
	}
	return res;
}

static lbm_value ext_text(lbm_value *args, lbm_uint argn) {
	if (argn != 7) {
		return ENC_SYM_TERROR;
	}

	int x = lbm_dec_as_u32(args[1]);
	int y = lbm_dec_as_u32(args[2]);
	int fg = lbm_dec_as_u32(args[3]);
	int bg = lbm_dec_as_u32(args[4]);

	if (!lispif_disp_is_image_buffer(args[0])) return ENC_SYM_TERROR;
	image_buffer_t *img = (image_buffer_t*)lbm_get_custom_value(args[0]);

	lbm_array_header_t *font = 0;
	if (lbm_type_of(args[5]) == LBM_TYPE_ARRAY) {
		font = (lbm_array_header_t *)lbm_car(args[5]);
		if (font->elt_type != LBM_TYPE_BYTE) {
			font = 0;
		}
	}

	char *txt = lbm_dec_str(args[6]);

	if (!font || !txt || font->size < (4 + 5 * 5 * 10)) {
		return ENC_SYM_TERROR;
	}

	uint8_t *font_data = (uint8_t*)font->data;
	uint8_t w = font_data[0];

	int ind = 0;
	while (txt[ind] != 0) {
		img_putc(img, x + ind * w, y, fg, bg, font_data, txt[ind]);
		ind++;
	}

	return ENC_SYM_TRUE;
}

static lbm_value ext_blit(lbm_value *args, lbm_uint argn) {
	lbm_value res = ENC_SYM_TERROR;

	if (argn == 9 &&
		lispif_disp_is_image_buffer(args[0]) &&
		lispif_disp_is_image_buffer(args[1]) &&
		lbm_is_number(args[2]) &&
		lbm_is_number(args[3]) &&
		lbm_is_number(args[4]) &&
		lbm_is_number(args[5]) &&
		lbm_is_number(args[6]) &&
		lbm_is_number(args[7]) &&
		lbm_is_number(args[8])) {

		image_buffer_t *dest = (image_buffer_t*)lbm_get_custom_value(args[0]);
		image_buffer_t *src  = (image_buffer_t*)lbm_get_custom_value(args[1]);
		int32_t x = lbm_dec_as_i32(args[2]);
		int32_t y = lbm_dec_as_i32(args[3]);
		float xr = lbm_dec_as_float(args[4]);
		float yr = lbm_dec_as_float(args[5]);
		float rot = lbm_dec_as_float(args[6]);
		float scale = lbm_dec_as_float(args[7]);
		int32_t tc = lbm_dec_as_u32(args[8]);

		blit_rot_scale(dest,src,x,y,xr,yr,rot,scale, tc);
		res = ENC_SYM_TRUE;

	}

	return res;
}


// Init image_buffer extension_library

void lispif_load_disp_extensions(void) {
	register_symbols();
	lbm_add_extension("img-buffer", ext_image_buffer);
	lbm_add_extension("img-buffer-from-bin", ext_image_buffer_from_bin);
	lbm_add_extension("img-dims", ext_image_dims);
	lbm_add_extension("img-setpix", ext_putpixel);
	lbm_add_extension("img-line", ext_line);
	lbm_add_extension("img-text", ext_text);
	lbm_add_extension("img-clear", ext_clear);
	lbm_add_extension("img-circle", ext_circle);
	lbm_add_extension("img-blit", ext_blit);
}
