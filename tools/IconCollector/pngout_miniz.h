#pragma once

#define MINIZ_BUFFER_SIZE 512

#if USE_MINIZ
  Boolean initCompressor();
#else
  Boolean initCompressor() {return true};
#endif