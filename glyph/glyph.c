#include "glyph.h"
#include "outline.h"
#include "../tables/tables.h"
#include "../raster/bitmap.h"
#include "../utils/utils.h"
#include <stdlib.h>

int32_t get_glyph_index(TTF_Font *font, uint16_t c) {
	if (!font) {
		return -1;
	}
	// Get cmap subtables
	cmap_Table *cmap = get_cmap_table(font);
	if (!cmap) {
		warn("failed to get cmap table");
		return -1;
	}

	int i;
	for (i = 0; i < cmap->num_subtables; i++) {
		cmap_subTable *subtable = &cmap->subtables[i];
		if(subtable->format == 0) {
			if (!subtable->glyph_index_array || c < 0 || c >= subtable->num_indices) {
				continue;
			}
			return subtable->glyph_index_array[c];
		} else if(subtable->format == 4) {
			for (int j = 0; j < subtable->seg_count; j++)
			{
				if(c>=subtable->start_code[j]&&c<=subtable->end_code[j])
				{
					if(subtable->id_range_offset[j]==0){
						return ((int16_t)c+(int16_t)subtable->id_delta[j])&0xffff;
					}else{
						int index = subtable->id_range_offset[j]/2 + c-subtable->start_code[j] - subtable->seg_count;
						return subtable->glyph_index_array[index];
					}
				}
			}
			
		}

		
	}
	
	return -1;
}

TTF_Glyph *get_glyph(TTF_Font *font, uint16_t c) {
	if (!font) {
		return NULL;
	}
	// Get glyf table
	glyf_Table *glyf = get_glyf_table(font);
	if (!glyf) {
		warn("failed to get glyf table");
		return NULL;
	}

	// Lookup glyph index in cmap table
	uint32_t glyph_index = get_glyph_index(font, c);
	printf("glyph_index %d\n",glyph_index);
	if (glyph_index < glyf->num_glyphs) {
		TTF_Glyph *glyph = &glyf->glyphs[glyph_index];
		glyph->index = glyph_index;
		return glyph;
	}

	return NULL;
}

uint16_t get_glyph_advance_width(TTF_Font *font, TTF_Glyph *glyph) {
	if (!font || !glyph) {
		return 0;
	}

	hmtx_Table *hmtx = get_hmtx_table(font);
	if (!hmtx) {
		warn("failed to get glyph advance width");
		return 0;
	}
	if(glyph->index >= hmtx->num_h_metrics)
		return glyph->x_max;
	else
		return hmtx->advance_width[glyph->index];
}

int16_t get_glyph_left_side_bearing(TTF_Font *font, TTF_Glyph *glyph) {
	if (!font || !glyph) {
		return 0;
	}

	hmtx_Table *hmtx = get_hmtx_table(font);
	if (!hmtx) {
		warn("failed to get glyph left side bearing");
		return 0;
	}

	if(glyph->index >= hmtx->num_h_metrics)
		return hmtx->non_horizontal_left_side_bearing[glyph->index - hmtx->num_h_metrics];
	else
		return hmtx->left_side_bearing[glyph->index];
}

void free_simple_glyph(TTF_Glyph *glyph) {
	if (!glyph) {
		return;
	}
	if (glyph->descrip.simple.end_pts_of_contours) {
		free(glyph->descrip.simple.end_pts_of_contours);
	}
	if (glyph->descrip.simple.flags) {
		free(glyph->descrip.simple.flags);
	}
	if (glyph->descrip.simple.x_coordinates) {
		free(glyph->descrip.simple.x_coordinates);
	}
	if (glyph->descrip.simple.y_coordinates) {
		free(glyph->descrip.simple.y_coordinates);
	}
}

void free_compound_glyph(TTF_Glyph *glyph) {
	if (!glyph) {
		return;
	}
	if (glyph->descrip.compound.comps) {
		free(glyph->descrip.compound.comps);
	}
}

void free_glyph(TTF_Glyph *glyph) {
	if (!glyph) {
		return;
	}
	if (glyph->number_of_contours > 0) {
		free_simple_glyph(glyph);
	} else if (glyph->number_of_contours < 0) {
		free_compound_glyph(glyph);
	}
	if (glyph->instructions) {
		free(glyph->instructions);
	}
	if (glyph->outline) {
		free_outline(glyph->outline);
	}
	if (glyph->bitmap) {
		free_bitmap(glyph->bitmap);
	}
}
