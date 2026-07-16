#pragma once

#define MINIZ_BUFFER_SIZE 512

#if USE_MINIZ > 0
  Boolean initCompressor();
#else
  Boolean initCompressor() {return true};
#endif