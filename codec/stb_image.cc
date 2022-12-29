#include <cstring>
#include <iostream>

#include <stb_image.h>

#include "nf7.hh"

#include "common/value.hh"

using namespace std::literals;


static void* init() noexcept;
static void deinit(void*) noexcept;
static void handle(const nf7_node_msg_t*) noexcept;

static const char* I[] = {"input", nullptr};
static const char* O[] = {"img", "error", nullptr};
extern "C" const nf7_node_t stb_image = {
  .name    = "stb_image",
  .desc    = "decodes an image by stb_image library",
  .inputs  = I,
  .outputs = O,
  .init    = init,
  .deinit  = deinit,
  .handle  = handle,
};


struct Session final {
  // input
  std::string npath;
  int         comp = 0;

  // output
  bool success = false;
};


static void* init() noexcept {
  return nullptr;
}
static void deinit(void*) noexcept {
}
static void handle(const nf7_node_msg_t* in) noexcept
try {
  pp::ConstValue v = in->value;
  if (in->name == "input"s) {
    Session ss;

    switch (v.type()) {
    case NF7_STRING:
      ss.npath = v.string();
      break;
    case NF7_TUPLE:
      try {
        ss.npath = v["npath"].string();
        ss.comp  = v["comp"].integerOrScalar<int>();
      } catch (...) {
        throw std::runtime_error {
          "incompatible tuple input (requires 'npath' and 'comp' fields)"};
      }
      break;
    default:
      throw std::runtime_error {"incompatible input"};
    }

    if (ss.npath.size() == 0) {
      throw std::runtime_error {"npath is empty"};
    }
    if (ss.comp < 0 || 4 < ss.comp) {
      throw std::runtime_error {"comp is out of range (0~4)"};
    }

    auto ptr = new Session {std::move(ss)};
    nf7->ctx.exec_async(in->ctx, ptr, [](auto ctx, auto ptr) {
      auto& ss = *reinterpret_cast<Session*>(ptr);

      int w, h, comp;
      if (uint8_t* src = stbi_load(ss.npath.c_str(), &w, &h, &comp, ss.comp)) {
        if (ss.comp == 0) {
          ss.comp = comp;
        }

        static const char* names[] = {"w", "h", "comp", "buf", nullptr};
        nf7_value_t* values[4];
        pp::MutValue {ctx->value}.AllocateTuple(names, values);

        pp::MutValue {values[0]} = static_cast<int64_t>(w);
        pp::MutValue {values[1]} = static_cast<int64_t>(h);
        pp::MutValue {values[2]} = static_cast<int64_t>(ss.comp);

        const auto size = static_cast<size_t>(w*h*ss.comp);
        uint8_t*   dst  = pp::MutValue {values[3]}.AllocateVector(size);
        std::memcpy(dst, src, size);
        stbi_image_free(src);

        nf7->ctx.exec_emit(ctx, "img", ctx->value, 0);
      } else {
        pp::MutValue {ctx->value} = "failed to load image";
        nf7->ctx.exec_emit(ctx, "error", ctx->value, 0);
      }
      delete &ss;
    }, 0);
  }
} catch (std::exception& e) {
  pp::MutValue {in->value} = e.what();
  nf7->ctx.exec_emit(in->ctx, "error", in->value, 0);
}
