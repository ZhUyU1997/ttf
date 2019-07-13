#include "utils.h"
#include "../glyph/glyph.h"
#include "../tables/tables.h"
#include "../raster/scale.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

int mod(int a, int b) {
	return ((a & b) + b) % b;
}

float symroundf(float f) {
	return (f < 0) ? floorf(f) : ceilf(f);
}

void warn(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");
}

void warnerr(const char *fmt, ...) {
	int errno_save = errno;
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (errno_save != 0) {
		fprintf(stderr, ": %s\n", strerror(errno_save));
	} else {
		fprintf(stderr, "\n");
	}
}

int get_text_width(TTF_Font *font, uint32_t *text) {
	if (!font || !text) {
		return 0;
	}

	int width = 0;
	/* Get advance width of each character's glyph. */
	for (int i = 0; text[i]; i++) {
		int32_t glyph_index = get_glyph_index(font, text[i]);
		if (glyph_index < 0) {
			continue;
		}
		TTF_Glyph *glyph = get_glyph(font, text[i]);
		width += roundf(funit_to_pixel(font, get_glyph_advance_width(font, glyph)));
	}

	return width;
}
