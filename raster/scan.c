#include "scan.h"
#include "raster.h"
#include "scale.h"
#include "bitmap.h"
#include "../base/consts.h"
#include "../utils/utils.h"
#include <math.h>
#include <stdlib.h>

#include <stdio.h>

static inline float quad_bezier(float t, float p0, float p1, float p2) {
	return (1-t)*(1-t)*p0 + 2*(1-t)*t*p1 + t*t*p2;
}

static int add_intersection(TTF_Scan_Line *scanline, float x, int status) {
	CHECKPTR(scanline);

	RETINIT(SUCCESS);

	if (scanline->num_intersections+1 > scanline->size_x) {
		/* Increase size of scan-line storage for new intersection. */
		scanline->iss = realloc(scanline->iss, (scanline->size_x * 2) * sizeof(*scanline->iss));
		CHECKFAIL(scanline->iss, warnerr("failed to adjust scanline size"));

		scanline->size_x *= 2;
	}
	scanline->iss[scanline->num_intersections].status = status;
	scanline->iss[scanline->num_intersections++].x = x;
	

	RET;
}

static int cmp_intersections(const void *p1, const void *p2) {
	float f1 = ((TTF_intersections *)p1)->x;
	float f2 = ((TTF_intersections *)p2)->x;
	if (f1 < f2) {
		return -1;
	} else if (f1 == f2) {
		return 0;
	} else {
		return 1;
	}
}

static int intersect_line_segment(TTF_Segment *segment, TTF_Scan_Line *scanline) {
	if (!segment || !scanline) {
		return 0;
	}
	float y = scanline->y;
	float x0 = segment->x[0], x1 = segment->x[1];
	float y0 = segment->y[0], y1 = segment->y[1];

	if ((fabsf(y - y0) + fabsf(y - y1) - fabsf(y1 - y0)) != 0) {
		/* The scan-line at y is not within the y-bounds of the line segment. */
		return 0;
	}

	if (y0 == y1) {
		/* Intersecting with horizontal line (degenerate case). */
		if (y == y0) {
			 /* There are two intersections, x0 and x1. */
			add_intersection(scanline, x0, 1);
			add_intersection(scanline, x1, 1);
			return 2;
		} else {
			/* No intersections. */
			return 0;
		}
	}

	/* Calculate x-intersection of scan-line and line segment. */
	float x = ((y - y0)*(x1 - x0)) / (y1 - y0) + x0;

	if ((fabsf(x - x0) + fabsf(x - x1) - fabsf(x1 - x0)) != 0) {
		/* The x-intersection is not within the x-bounds of the line segment. */
		return 0;
	}

	if(fabsf(y-MIN(y0,y1))<1.0)
		add_intersection(scanline, x, 0);
	else 
		add_intersection(scanline, x, 1);
	return 1;
}

static int intersect_curve_segment(TTF_Segment *segment, TTF_Scan_Line *scanline) {
	if (!segment || !scanline) {
		return 0;
	}
	float t;
	float y = scanline->y;
	float y0 = segment->y[0], y1 = segment->y[1], y2 = segment->y[2];
	float x0 = segment->x[0], x1 = segment->x[1], x2 = segment->x[2];

	float a = y0 - 2*y1 + y2;
	float b = 2*y1 - 2*y0;
	float c = y0 - y;

	/* Handle degenerate cases first. */
	if (a == 0) {
		/* Curve is a straight line. */
		if (b == 0) {
			/* Curve is a horizontal line. */
			if (c == 0) {
				/* There are two intersections, x0 and x2. */
				add_intersection(scanline, x0, 1);
				add_intersection(scanline, x2, 1);
				return 2;
			} else {
				/* No intersections. */
				return 0;
			}
		} else {
			t = -c / b;
			if (IN(t, 0.0, 1.0)) {
				/* One intersection. */
				float res = quad_bezier(t, x0, x1, x2);
				if((fabsf(y-y0)<1.0 && y0 > y1) || (fabsf(y-y2)<1.0 && y2 > y1))
					add_intersection(scanline, res, 0);
				else 
					add_intersection(scanline, res, 1);
				return 1;
			} else {
				/* No intersections. */
				return 0;
			}
		}
	}

	/* Test discriminant for number of roots. */
	float D = b*b - 4*a*c;
	if (D < 0) {
		/* No roots. */
		return 0;
	} else if (D == 0) {
		/* One distinct root. */
		t = -b / (2*a);
		if (IN(t, 0.0, 1.0)) {
			/* One intersection. */
			float res = quad_bezier(t, x0, x1, x2);
			if(fabsf(y-y0)<1.0 || fabsf(y-y2)<1.0)
			{
				if((fabsf(y-y0)<1.0 && y0 > y2) || (fabsf(y-y2)<1.0 && y2 > y0))
					add_intersection(scanline, res, 0);
				else
					add_intersection(scanline, res, 1);
					return 1;
			}else{
				if(y<=y0&&y<=y2)
					return 0;
				else {
					add_intersection(scanline, res, 1);
					add_intersection(scanline, res, 1);
				}
				return 2;
			}
		} else {
			/* No intersections. */
			return 0;
		}
	} else {
		/* Two distinct roots. */
		int num_intersections = 0;

		t = (-b + sqrt(D)) / (2*a);
		if (IN(t, 0.0, 1.0)) {
			/* Intersection point is within curve endpoints. */
			float res = quad_bezier(t, x0, x1, x2);
			if((fabsf(y-y0)<1.0 && y0 > y1) || (fabsf(y-y2)<1.0 && y2 > y1))
				add_intersection(scanline, res, 0);
			else
				add_intersection(scanline, res, 1);
			num_intersections++;
		}

		t = (-b - sqrt(D)) / (2*a);
		if (IN(t, 0.0, 1.0)) {
			/* Intersection point is within curve endpoints. */
			float res = quad_bezier(t, x0, x1, x2);
			if((fabsf(y-y0)<1.0 && y0 > y1) || (fabsf(y-y2)<1.0 && y2 > y1)){
				add_intersection(scanline, res, 0);
				//if(IN(res,552.68,552.69))
				//printf("status %d %d",scanline->num_intersections-1,scanline->iss[scanline->num_intersections-1].status );
			}
			else
				add_intersection(scanline, res, 1);
			num_intersections++;
		}
		return num_intersections;
	}
}

static int intersect_segment(TTF_Segment *segment, TTF_Scan_Line *scanline) {
	if (!segment || !segment->x || !segment->y) {
		warn("failed to intersect uninitialized contour segment");
		return 0;
	}
	switch (segment->type) {
		case LINE_SEGMENT:
			return intersect_line_segment(segment, scanline);
			break;
		case CURVE_SEGMENT:
			return intersect_curve_segment(segment, scanline);
			break;
		default:
			break;
	}

	return 0;
}

int scan_glyph(TTF_Font *font, TTF_Glyph *glyph) {
	CHECKPTR(font);
	CHECKPTR(glyph);

	RETINIT(SUCCESS);

	if (!glyph->outline) {
		warn("failed to scan uninitialized glyph outline");
		return FAILURE;
	} else if (glyph->outline->point < 0) {
		warn("failed to scan unscaled glyph outline");
		return FAILURE;
	} else if (glyph->number_of_contours == 0) {
		/* Zero-length glyph with no outline. */
		return SUCCESS;
	}

	TTF_Outline *outline = glyph->outline;
	TTF_Bitmap *bitmap = NULL;
	uint32_t bg, fg;

	if (font->raster_flags & RENDER_FPAA) {
		/* Anti-aliased rendering - oversample outline then downsample. */
		bg = 0xFFFFFF;
		fg = 0x000000;
		if (!glyph->bitmap) {
			glyph->bitmap = create_bitmap((outline->x_max - outline->x_min) / 2,
					(outline->y_max - outline->y_min) / 2, bg);
		}

		/* Intermediate oversampled bitmap. */
		bitmap = create_bitmap(outline->x_max - outline->x_min,
				outline->y_max - outline->y_min, bg);
	} else if (font->raster_flags & RENDER_ASPAA) {
		/* Sub-pixel rendering - oversample in x-direction then downsample. */
		bg = 0xFFFFFF;
		fg = 0x000000;
		if (!glyph->bitmap) {
			glyph->bitmap = create_bitmap((outline->x_max - outline->x_min) / 3,
					outline->y_max - outline->y_min, bg);
		}

		/* Intermediate oversampled bitmap. */
		bitmap = create_bitmap(outline->x_max - outline->x_min,
				outline->y_max - outline->y_min, bg);
	} else {
		/* Normal rendering - write directly to glyph bitmap. */
		bg = 0xFFFFFF;
		fg = 0x000000;
		if (!glyph->bitmap) {
			glyph->bitmap = create_bitmap(outline->x_max - outline->x_min,
					outline->y_max - outline->y_min, bg);
		}

		bitmap = glyph->bitmap;
	}

	/* Create a scan-line for each row in the scaled outline. */
	int num_scanlines = outline->y_max - outline->y_min;
	TTF_Scan_Line *scanlines = malloc(num_scanlines * sizeof(*scanlines));
	CHECKFAIL(scanlines, warnerr("failed to alloc glyph scan lines"));

	/* Find intersections of each scan-line with contour segments. */
	for (int i = 0; i < num_scanlines; i++) {
		TTF_Scan_Line *scanline = &scanlines[i];
		init_scanline(scanline, outline->num_contours * 2);
		scanline->y = outline->y_max - i;

		for (int j = 0; j < outline->num_contours; j++) {
			TTF_Contour *contour = &outline->contours[j];
			int k;
			for (k = 0; k < contour->num_segments; k++) {
				TTF_Segment *segment = &contour->segments[k];
				intersect_segment(segment, scanline);
			}
		}

		/* Round pixel intersection values to 1/64 of a pixel. */
		for (int j = 0; j < scanline->num_intersections; j++) {
			scanline->iss[j].x = round_pixel(scanline->iss[j].x);
		}

		/* Sort intersections from left to right. */
		qsort(scanline->iss, scanline->num_intersections, sizeof(*scanline->iss), cmp_intersections);

		//printf("Intersections: ");

		int int_index = 0, fill = 0, status=0;
		for (int j = 0; j < bitmap->w; j++) {
			if (int_index < scanline->num_intersections) {
				if ((outline->x_min + j) >= scanline->iss[int_index].x) {

					/* Skip over duplicate intersections. */
					int k;
					status=0;
					for (k = 0; int_index+k < scanline->num_intersections; k++) {
						if (scanline->iss[int_index].x != scanline->iss[int_index+k].x) {
							break;
						}
						status+=scanline->iss[int_index+k].status;
						//printf("%f(%d [%d]), ", scanline->iss[int_index+k].x,scanline->iss[int_index+k].status,int_index+k);
					}
					if(k==1)
						status = 1;
					//printf("(%d), ",status);
					//TODO:https://www.cnblogs.com/icmzn/p/5053317.html
					int_index += k;
					if(status%2) {
						fill = !fill;
						//printf("<*>");
					}
						
				}
			}
			if (fill) {
				bitmap_set(bitmap, j, i, fg);
			}
		}
		//printf("\n");
	}

	if (font->raster_flags & RENDER_FPAA) {
		/* Downsample intermediate bitmap. */
		for (int y = 0; y < glyph->bitmap->h; y++) {
			for (int x = 0; x < glyph->bitmap->w; x++) {
				/* Calculate pixel coverage:
				 * 	coverage = number filled samples / number samples */
				float coverage = 0;
				if (bitmap_get(bitmap, (2*x), (2*y)) == 0x000000) coverage++;
				if (bitmap_get(bitmap, (2*x)+1, (2*y)) == 0x000000) coverage++;
				if (bitmap_get(bitmap, (2*x), (2*y)+1) == 0x000000) coverage++;
				if (bitmap_get(bitmap, (2*x)+1, (2*y)+1) == 0x000000) coverage++;
				coverage /= 4;
				uint8_t shade = 0xFF*(1-coverage);
				uint32_t pixel = (shade << 16) | (shade << 8) | (shade << 0);

				bitmap_set(glyph->bitmap, x, y, pixel);
			}
		}

		free_bitmap(bitmap);
	} else if (font->raster_flags & RENDER_ASPAA) {
		/* Perform five element low-pass filter. */
		TTF_Bitmap *temp = create_bitmap(bitmap->w, bitmap->h, bg);

		for (int y = 0; y < bitmap->h; y++) {
			for (int x = 0; x < bitmap->w; x++) {
				uint32_t pixel = 0x000000;

				pixel += bitmap_get(bitmap, x-2, y) * (1.0 / 9.0);
				pixel += bitmap_get(bitmap, x-1, y) * (2.0 / 9.0);
				pixel += bitmap_get(bitmap, x, y) * (3.0 / 9.0);
				pixel += bitmap_get(bitmap, x+1, y) * (2.0 / 9.0);
				pixel += bitmap_get(bitmap, x+2, y) * (1.0 / 9.0);

				bitmap_set(temp, x, y, pixel);
			}
		}

		free_bitmap(bitmap);
		bitmap = temp;

		/* Downsample intermediate bitmap. */
		for (int y = 0; y < glyph->bitmap->h; y++) {
			for (int x = 0; x < glyph->bitmap->w; x++) {
				uint8_t r, g, b;
				r = (bitmap_get(bitmap, (3*x), y) * 0xFF) / 0xFFFFFF;
				g = (bitmap_get(bitmap, (3*x)+1, y) * 0xFF) / 0xFFFFFF;
				b = (bitmap_get(bitmap, (3*x)+2, y) * 0xFF) / 0xFFFFFF;
				uint32_t pixel = (r << 16) | (g << 8) | (b << 0);

				bitmap_set(glyph->bitmap, x, y, pixel);
			}
		}

		free_bitmap(bitmap);
	}

	RETRELEASE(free_scanlines(scanlines, num_scanlines));
}

int init_scanline(TTF_Scan_Line *scanline, int base_size) {
	CHECKPTR(scanline);

	RETINIT(SUCCESS);

	scanline->iss = malloc(base_size * sizeof(*scanline->iss));
	CHECKFAIL(scanline->iss, warnerr("failed to alloc scanline"));

	scanline->size_x = base_size;
	scanline->num_intersections = 0;

	RETFAIL(free_scanline(scanline));
}

void free_scanline(TTF_Scan_Line *scanline) {
	if (!scanline) {
		return;
	}
	if (scanline->iss) {
		free(scanline->iss);
	}
}

void free_scanlines(TTF_Scan_Line *scanlines, int num_scanlines) {
	if (!scanlines) {
		return;
	}
	int i;
	for (i = 0; i < num_scanlines; i++) {
		free_scanline(&scanlines[i]);
	}
	free(scanlines);
}
