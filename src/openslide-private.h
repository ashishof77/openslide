/*
 *  OpenSlide, a library for reading whole slide image files
 *
 *  Copyright (c) 2007-2012 Carnegie Mellon University
 *  All rights reserved.
 *
 *  OpenSlide is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, version 2.1.
 *
 *  OpenSlide is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with OpenSlide. If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#ifndef OPENSLIDE_OPENSLIDE_PRIVATE_H_
#define OPENSLIDE_OPENSLIDE_PRIVATE_H_

#ifdef _WIN32
#define WIN32 1
#endif

#include <config.h>

#include "openslide.h"
#include "openslide-hash.h"

#include <glib.h>
#include <stdbool.h>
#include <setjmp.h>
#include <tiffio.h>

// jconfig.h redefines HAVE_STDLIB_H if libjpeg was not built with Autoconf
#undef HAVE_STDLIB_H
#include <jpeglib.h>
#undef HAVE_STDLIB_H
#include <config.h>  // again

#include <cairo.h>

#include <openjpeg.h>


/* the associated image structure */
struct _openslide_associated_image {
  int64_t w;
  int64_t h;
  void *ctx;

  // must fail if width or height doesn't match the image
  void (*get_argb_data)(openslide_t *osr, void *ctx, uint32_t *dest,
                        int64_t w, int64_t h);
  void (*destroy_ctx)(void *ctx);
};

/* the main structure */
struct _openslide {
  const struct _openslide_ops *ops;
  void *data;
  int32_t level_count;

  double *downsamples;  // zero values or NULL are filled in automatically from dimensions

  // associated images
  GHashTable *associated_images;  // created automatically
  const char **associated_image_names; // filled in automatically from hashtable

  // metadata
  GHashTable *properties; // created automatically
  const char **property_names; // filled in automatically from hashtable

  // cache
  struct _openslide_cache *cache;

  // error handling, NULL if no error
  gpointer error; // must use g_atomic_pointer!
};

/* the function pointer structure for backends */
struct _openslide_ops {
  void (*get_dimensions)(openslide_t *osr,
			 int32_t level,
			 int64_t *w, int64_t *h);
  // need not do anything, but must be consistent
  void (*get_tile_geometry)(openslide_t *osr,
			    int32_t level,
			    int64_t *w, int64_t *h);
  void (*paint_region)(openslide_t *osr, cairo_t *cr,
		       int64_t x, int64_t y,
		       int32_t level,
		       int32_t w, int32_t h);
  void (*destroy)(openslide_t *osr);
};

/* vendor detection and parsing */
typedef bool (*_openslide_vendor_fn)(openslide_t *osr, const char *filename,
				     struct _openslide_hash *quickhash1,
				     GError **err);
typedef bool (*_openslide_tiff_vendor_fn)(openslide_t *osr, TIFF *tiff,
					  struct _openslide_hash *quickhash1,
					  GError **err);
/*
 * A note on quickhash1: this should be a hash of data that
 * will not change with revisions to the openslide library. It should
 * also be quick to generate. It should be a way to uniquely identify
 * a particular slide by content, but does not need to be sensitive
 * to file corruption.
 *
 * It is called "quickhash1" so that we can create a "quickhash2" if needed.
 * The hash is stored in a property, it is expected that we will store
 * more hash properties if needed.
 *
 * Suggested data to hash:
 * easily available image metadata + raw compressed lowest resolution image
 */


bool _openslide_try_trestle(openslide_t *osr, TIFF *tiff,
			    struct _openslide_hash *quickhash1, GError **err);
bool _openslide_try_aperio(openslide_t *osr, TIFF *tiff,
			   struct _openslide_hash *quickhash1, GError **err);
bool _openslide_try_hamamatsu(openslide_t *osr, const char* filename,
			      struct _openslide_hash *quickhash1,
			      GError **err);
bool _openslide_try_hamamatsu_ndpi(openslide_t *osr, const char* filename,
				   struct _openslide_hash *quickhash1,
				   GError **err);
bool _openslide_try_mirax(openslide_t *osr, const char* filename,
			  struct _openslide_hash *quickhash1, GError **err);
bool _openslide_try_leica(openslide_t *osr, TIFF *tiff,
				 struct _openslide_hash *quickhash1,
				 GError **err);
bool _openslide_try_generic_tiff(openslide_t *osr, TIFF *tiff,
				 struct _openslide_hash *quickhash1,
				 GError **err);


/* GHashTable utils */
guint _openslide_int64_hash(gconstpointer v);
gboolean _openslide_int64_equal(gconstpointer v1, gconstpointer v2);
void _openslide_int64_free(gpointer data);

/* g_key_file_load_from_file wrapper */
gboolean _openslide_read_key_file(GKeyFile *key_file, const char *filename,
                                  GKeyFileFlags flags, GError **err);

/* fopen() wrapper which properly sets FD_CLOEXEC */
FILE *_openslide_fopen(const char *path, const char *mode, GError **err);

/* Returns the size of the file */
int64_t _openslide_fsize(const char *path, GError **err);

/* Serialize double to string */
char *_openslide_format_double(double d);

/* Duplicate OpenSlide properties */
void _openslide_duplicate_int_prop(GHashTable *ht, const char *src,
                                   const char *dest);
void _openslide_duplicate_double_prop(GHashTable *ht, const char *src,
                                      const char *dest);

// background color helper
void _openslide_set_background_color_prop(GHashTable *ht,
                                          uint8_t r, uint8_t g, uint8_t b);


/* TIFF support */
typedef void (*_openslide_tiff_tilereader_fn)(openslide_t *osr,
					      TIFF *tiff,
					      uint32_t *dest,
					      int64_t x,
					      int64_t y,
					      int32_t w,
					      int32_t h);

void _openslide_add_tiff_ops(openslide_t *osr,
			     TIFF *tiff,
			     int32_t property_dir,
			     int32_t overlap_count,
			     int32_t *overlaps,
			     int32_t level_count,
			     int32_t *levels,
			     _openslide_tiff_tilereader_fn tileread,
			     struct _openslide_hash *quickhash1);

void _openslide_generic_tiff_tilereader(openslide_t *osr,
					TIFF *tiff,
					uint32_t *dest,
					int64_t x, int64_t y,
					int32_t w, int32_t h);

bool _openslide_add_tiff_associated_image(GHashTable *ht,
					  const char *name,
					  TIFF *tiff,
					  GError **err);

TIFF *_openslide_tiff_open(const char *filename);

/* JPEG support */
struct _openslide_jpeg_file {
  char *filename;

  int64_t start_in_file;
  int64_t end_in_file;

  // if known, put mcu starts here, set unknowns to -1,
  // and give dimensions and tile dimensions
  int64_t *mcu_starts;
  int32_t w;
  int32_t h;
  int32_t tw;
  int32_t th;
};

struct _openslide_jpeg_tile {
  // which tile and file?
  int32_t fileno;
  int32_t tileno;

  // bounds in the physical tile?
  double src_x;
  double src_y;
  double w;
  double h;

  // delta for this tile from the standard advance
  double dest_offset_x;
  double dest_offset_y;
};

struct _openslide_jpeg_level {
  GHashTable *tiles;

  // size of canvas
  int64_t level_w;
  int64_t level_h;

  int32_t tiles_across;
  int32_t tiles_down;

  // ONLY for convenience in checking even scale_denom division
  int32_t raw_tile_width;
  int32_t raw_tile_height;

  // standard advance
  double tile_advance_x;
  double tile_advance_y;

  // if zero, calculated automatically
  double downsample;
};

void _openslide_add_jpeg_ops(openslide_t *osr,
			     int32_t file_count,
			     struct _openslide_jpeg_file **files,
			     int32_t level_count,
			     struct _openslide_jpeg_level **levels);

GHashTable *_openslide_jpeg_create_tiles_table(void);

bool _openslide_add_jpeg_associated_image(GHashTable *ht,
					  const char *name,
					  const char *filename,
					  int64_t offset,
					  GError **err);

/*
 * On Windows, we cannot fopen a file and pass it to another DLL that does fread.
 * So we need to compile all our freading into the OpenSlide DLL directly.
 */
void _openslide_jpeg_stdio_src(j_decompress_ptr cinfo, FILE *infile);

// error function for libjpeg
struct _openslide_jpeg_error_mgr {
  struct jpeg_error_mgr pub;      // public fields

  jmp_buf *env;
  GError *err;
};

struct jpeg_error_mgr *_openslide_jpeg_set_error_handler(struct _openslide_jpeg_error_mgr *jerr,
							 jmp_buf *env);

// Hamamatsu NGR
struct _openslide_ngr {
  char *filename;

  int64_t start_in_file;

  int32_t w;
  int32_t h;

  int32_t column_width;
};

void _openslide_add_ngr_ops(openslide_t *osr,
			    int32_t ngr_count,
			    struct _openslide_ngr **ngrs);


// external error propagation
bool _openslide_set_error(openslide_t *osr, const char *format, ...);
bool _openslide_check_cairo_status_possibly_set_error(openslide_t *osr,
						      cairo_t *cr);

// internal error propagation
enum OpenSlideError {
  // file format unrecognized; try other formats
  OPENSLIDE_ERROR_FORMAT_NOT_SUPPORTED,
  // file corrupt; hard fail
  OPENSLIDE_ERROR_BAD_DATA,
};
#define OPENSLIDE_ERROR _openslide_error_quark()
GQuark _openslide_error_quark(void);

void _openslide_io_error(GError **err, const char *fmt, ...);
void _openslide_set_error_from_gerror(openslide_t *osr, GError *err);

// private properties, for now
#define _OPENSLIDE_PROPERTY_NAME_LEVEL_COUNT "openslide.level-count"
#define _OPENSLIDE_PROPERTY_NAME_TEMPLATE_LEVEL_WIDTH "openslide.level[%d].width"
#define _OPENSLIDE_PROPERTY_NAME_TEMPLATE_LEVEL_HEIGHT "openslide.level[%d].height"
#define _OPENSLIDE_PROPERTY_NAME_TEMPLATE_LEVEL_DOWNSAMPLE "openslide.level[%d].downsample"
#define _OPENSLIDE_PROPERTY_NAME_TEMPLATE_LEVEL_TILE_WIDTH "openslide.level[%d].tile-width"
#define _OPENSLIDE_PROPERTY_NAME_TEMPLATE_LEVEL_TILE_HEIGHT "openslide.level[%d].tile-height"

// deprecated prefetch stuff (maybe we'll undeprecate it someday),
// still needs these declarations for ABI compat
// TODO: remove if soname bump
#undef openslide_give_prefetch_hint
OPENSLIDE_PUBLIC()
int openslide_give_prefetch_hint(openslide_t *osr,
				 int64_t x, int64_t y,
				 int32_t level,
				 int64_t w, int64_t h);
#undef openslide_cancel_prefetch_hint
OPENSLIDE_PUBLIC()
void openslide_cancel_prefetch_hint(openslide_t *osr, int prefetch_id);


#endif
