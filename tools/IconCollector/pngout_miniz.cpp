#if USE_MINIZ > 0
  static void start_chunk(struct pngout *s, const char *typecode, uint32_t size);
  static void end_chunk(struct pngout *s);
  static void b8s(struct pngout *s, const uint8_t *b, size_t n);

  #define MINIZ_NO_ARCHIVE_APIS
  #define MINIZ_NO_ARCHIVE_WRITING_APIS
  #define MINIZ_NO_TIME
  #define MINIZ_NO_ZLIB_APIS
  #define MINIZ_NO_MALLOC
  #define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 1
  #define MINIZ_LITTLE_ENDIAN 0
  #define MINIZ_HAS_64BIT_REGISTERS 0
  #define MINIZ_HAS_64BIT_INTEGERS 0

  #include "miniz.h"

  tdefl_compressor *g_deflator = NULL;
  size_t in_size = 0;
  unsigned char in_data[MINIZ_BUFFER_SIZE];
  size_t total_bytes_in = 0, total_bytes_out = 0;
  tdefl_status status;

  Boolean initCompressor() {
    const int level = USE_MINIZ;

    if (g_deflator == NULL) {
        g_deflator = (tdefl_compressor*)NewPtr(sizeof(tdefl_compressor));
        if ((g_deflator == NULL) || (MemError() != noErr)) {
          printf("Failed to allocate compressor object\n");
          return false;
        }
        printf("Reserved %ld bytes for ZLib compressor object\n", sizeof(tdefl_compressor));
    }

    // The number of dictionary probes to use at each compression level (0-10). 0=implies fastest/minimal possible probing.
    const mz_uint s_tdefl_num_probes[11] = { 0, 1, 6, 32,  16, 32, 128, 256,  512, 768, 1500 };

    // create tdefl() compatible flags (we have to compose the low-level flags ourselves, or use tdefl_create_comp_flags_from_zip_params() but that means MINIZ_NO_ZLIB_APIS can't be defined).
    mz_uint comp_flags = TDEFL_WRITE_ZLIB_HEADER |
                         s_tdefl_num_probes[MZ_MIN(10, level)] |
                         ((level <= 3) ? TDEFL_GREEDY_PARSING_FLAG : 0);
    if (level == 0) {
      comp_flags |= TDEFL_FORCE_ALL_RAW_BLOCKS;
    }

    // Initialize the low-level compressor.
    status = tdefl_init(g_deflator, NULL, NULL, comp_flags);
    if (status != TDEFL_STATUS_OKAY) {
      printf("Compressor failed to init\n");
      return false;
    }

    total_bytes_in = 0;
    total_bytes_out = 0;
    in_size = 0;

    return true;
  }

  static tdefl_status outputCompressedData(struct pngout *s, Boolean isEnd) {
    unsigned char out_data[MINIZ_BUFFER_SIZE];

    if (status != TDEFL_STATUS_OKAY) {
      return status;
    }

    if (!g_deflator) {
      return TDEFL_STATUS_BAD_PARAM;
    }

    size_t in_bytes  = in_size;
    size_t out_bytes = sizeof(out_data);
    status = tdefl_compress(g_deflator, in_data, &in_bytes, out_data, &out_bytes, isEnd ? TDEFL_FINISH : TDEFL_NO_FLUSH);
    switch (status) {
      case TDEFL_STATUS_OKAY:
      case TDEFL_STATUS_DONE:
        break;
      default:
        printf("Compressor error: %d, isEnd: %d\n", status, isEnd);
        return status;
    }

    total_bytes_in  += in_bytes;
    total_bytes_out += out_bytes;

    if (out_bytes) {
      start_chunk(s, "IDAT", out_bytes);
      b8s(s, out_data, out_bytes);
      end_chunk(s);
    }

    if (in_bytes) {
      memcpy(in_data, in_data + in_bytes, in_size - in_bytes);
      in_size -= in_bytes;
    }

    if (status == TDEFL_STATUS_DONE) {
      //printf("Compressed %ld bytes to %ld bytes\n", total_bytes_in, total_bytes_out);
    }
    return status;
  }
#endif