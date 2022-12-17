#include "header.inc"

#include <iostream>

#include <stb_image.h>

static const char* I[] = {"npath", nullptr};
static const char* O[] = {"out",   nullptr};
PASSPAWN_DEFINE_NODE_META(stb_image, "decodes an image by stb library", I, O);

static void* init() noexcept {
  return nullptr;
}
static void deinit(void*) noexcept {
}
static void handle(const nf7_node_msg_t*) noexcept {
  std::cout << "helloworld" << std::endl;
}
