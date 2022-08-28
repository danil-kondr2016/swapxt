#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include <png.h>

#define N_ITERATIONS 6

typedef struct {
  char R;
  char G;
  char B;
} RGBTRIPLE;

RGBTRIPLE ** img_buffers;

void read_images(char * dir, int n_files, int c, int n);
void write_images(char * dir, int c, int n, int w, int h);

int main(int argc, char ** argv) {
  char * input_dir;
  char * output_dir;
  int width, height;
  int n_buffers;
  int output_dir_fd;
  int i;

  if (argc < 4) {
    printf("Usage: %s input_dir/ width height <output_dir/>\n", argv[0]);
    exit(-1);
  }
  
  input_dir = argv[1];
  width = atoi(argv[2]);
  height = atoi(argv[3]);
  if (argc < 5)
    output_dir = "o";
  else
    output_dir = argv[4];

  n_buffers = height / N_ITERATIONS;

  if (faccessat(AT_FDCWD, output_dir, F_OK, 0) != 0) {
    output_dir_fd = mkdirat(AT_FDCWD, output_dir, S_IRWXU | S_IRWXG | S_IRWXO);
    if (output_dir_fd == -1) {
      perror(output_dir);
      exit(-1);
    }
  }
  
  img_buffers = malloc(sizeof(RGBTRIPLE*) * n_buffers);
  for (i = 0; i < n_buffers; i++) {
    img_buffers[i] = calloc(sizeof(RGBTRIPLE), width * height);
  }

  for (i = 0; i < height; i += n_buffers) {
    fprintf(stderr, "Pass #%d\n", i / n_buffers + 1);
    read_images(input_dir, height, i, n_buffers);
    write_images(output_dir, i, n_buffers, width, height);
  }
 
  close(output_dir_fd);
  for (i = 0; i < n_buffers; i++)
    free(img_buffers[i]);
  free(img_buffers);
  return 0;
}

void read_images(char * dir, int n_files, int c, int n) {
  FILE * f;
  char fname[256];
  png_structp png  = NULL;
  png_infop   info = NULL;
  png_bytepp  rows;
  int i, x, y, w, h, p;
 
  for (int i = 0; i < n_files; i++) {
    sprintf(fname, "%s/%06d.png", dir, i + 1);
    f = fopen(fname, "rb");
    if (!f) {
      perror(fname);
      exit(-1);
    }

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
      fclose(f);
      fprintf(stderr, "%s: failed to create png_struct\n", fname);
      exit(-1);
    }

    info = png_create_info_struct(png);
    if (!info) {
      png_destroy_write_struct(&png, NULL);
      fclose(f);
      fprintf(stderr, "%s: failed to create png_info\n", fname);
      exit(-1);
    }

    if (setjmp(png_jmpbuf(png))) {
      png_destroy_read_struct(&png, NULL, NULL);
      fclose(f);
      fprintf(stderr, "%s: failed to setjmp\n", fname);
      exit(-1);
    }

    fprintf(stderr, "Reading %s... \r", fname);
    png_init_io(png, f);
    png_read_png(
      png, info, 
      PNG_TRANSFORM_STRIP_16 
    | PNG_TRANSFORM_STRIP_ALPHA 
    | PNG_TRANSFORM_EXPAND, 
      NULL
    );
    png_get_IHDR(png, info, &w, &h, NULL, NULL, NULL, NULL, NULL);
    rows = png_get_rows(png, info);
   
    for (y = c; y < c + n; y++) {
      png_bytep row = rows[y];
      for (x = 0; x < w; x++) {
        p = (i * w + x);
        img_buffers[y - c][p].R = row[x * 3];
        img_buffers[y - c][p].G = row[x * 3 + 1];
        img_buffers[y - c][p].B = row[x * 3 + 2]; 
      } 
    }
    png_destroy_read_struct(&png, &info, NULL);
    fclose(f);
  }
  fprintf(stderr, "Reading %s... done\n", fname);
}

void write_images(char * dir, int c, int n, int w, int h)
{
  png_structp png;
  png_infop   info;
  FILE * f;
  char fname[256];
  png_bytepp  rows;
  int i, x, y;

  rows = malloc(sizeof(png_bytep) * h);
  for (i = 0; i < h; i++)
    rows[i] = calloc(sizeof(png_byte), w * 3);

  for (i = c; i < c + n; i++) {
    sprintf(fname, "%s/%06d.png", dir, i + 1);
    
    f = fopen(fname, "wb");
    if (!f) {
      perror(fname);
      exit(-1);
    }

    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
      fclose(f);
      fprintf(stderr, "%s: failed to create png_struct\n", fname);
      exit(-1);
    }

    info = png_create_info_struct(png);
    if (!info) {
      png_destroy_write_struct(&png, NULL);
      fclose(f);
      fprintf(stderr, "%s: failed to create png_info\n", fname);
      exit(-1);
    }

    if (setjmp(png_jmpbuf(png))) {
      png_destroy_write_struct(&png, NULL);
      fclose(f);
      fprintf(stderr, "%s: failed to setjmp\n", fname);
      exit(-1);
    }

    fprintf(stderr, "Writing %s...\r", fname);
    png_init_io(png, f);
    png_set_IHDR(png, info, w, h, 8, 
      PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
      PNG_FILTER_TYPE_DEFAULT
    );

    for(y = 0; y < h; y++) {
      for (x = 0; x < w; x++) {
        rows[y][x * 3 + 0] = img_buffers[i - c][y * w + x].R;
        rows[y][x * 3 + 1] = img_buffers[i - c][y * w + x].G;
        rows[y][x * 3 + 2] = img_buffers[i - c][y * w + x].B;
      }
    }
    png_set_rows(png, info, rows);
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);
    png_write_end(png, info);
    png_destroy_write_struct(&png, NULL);
    fclose(f);
  }

  for (int i = 0; i < h; i++)
    free(rows[i]);
  free(rows);

  fprintf(stderr, "Writing %s... done\n", fname);
}
