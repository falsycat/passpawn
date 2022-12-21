#include "header.inc"

#include <cstring>
#include <iostream>

#include <stb_image.h>

using namespace std::literals;


static const char* I[] = {"input", nullptr};
static const char* O[] = {"img", "error", nullptr};
PASSPAWN_DEFINE_NODE_META(stb_image, "decodes an image by stb library", I, O);


struct CtxInfo final {
  // input
  std::string npath;
  int         comp = 0;

  // output
  bool success = false;
};


static void* init() noexcept {
  return nullptr;
}
static void deinit(void* ptr) noexcept {
}
static void handle(const nf7_node_msg_t* in) noexcept {
  if (in->name == "input"s) {
    CtxInfo info;

    try {
      const auto type = nf7->value.get_type(in->value);
      switch (type) {
      case NF7_STRING:
        info.npath = get<std::string>(in->value);
        break;
      case NF7_TUPLE:
        if (auto v = nf7->value.get_tuple(in->value, "npath")) {
          info.npath = get<std::string>(v);
        }
        if (auto v = nf7->value.get_tuple(in->value, "comp")) {
          info.comp = get<int>(v);
        }
        break;
      default:
        throw std::runtime_error {"incompatible input"};
      }
    } catch (std::exception& e) {
      set(in->value, e.what());
      goto ERROR;
    }

    if (info.npath.size() == 0) {
      set(in->value, "npath is empty");
      goto ERROR;
    }
    if (info.comp < 0 || 4 < info.comp) {
      set(in->value, "comp is out of range (0~4)");
      goto ERROR;
    }

    auto ptr = new CtxInfo {std::move(info)};
    nf7->ctx.exec_async(in->ctx, ptr, [](auto ctx, auto ptr) {
      auto& info = *reinterpret_cast<CtxInfo*>(ptr);

      int w, h, comp;
      if (uint8_t* src = stbi_load(info.npath.c_str(), &w, &h, &comp, info.comp)) {
        if (info.comp == 0) {
          info.comp = comp;
        }

        static const char* names[] = {"w", "h", "comp", "buf", nullptr};
        nf7_value_t* values[4];
        nf7->value.set_tuple(ctx->value, names, values);

        set(values[0], static_cast<int64_t>(w));
        set(values[1], static_cast<int64_t>(h));
        set(values[2], static_cast<int64_t>(info.comp));

        const auto size = static_cast<size_t>(w*h*info.comp);
        uint8_t*   dst  = nf7->value.set_vector(values[3], size);
        std::memcpy(dst, src, size);
        stbi_image_free(src);

        nf7->ctx.exec_emit(ctx, "img", ctx->value, 0);
      } else {
        set(ctx->value, "failed to load image");
        nf7->ctx.exec_emit(ctx, "error", ctx->value, 0);
      }
      delete &info;
    }, 0);
    return;

  } else {
    set(in->value, "unknown input");
    goto ERROR;
  }

ERROR:
  nf7->ctx.emit(in->ctx, "error", in->value);
  return;
}
