#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <variant>

#include <zlib-ng.h>

#include "nf7.hh"

#include "common/queue.hh"
#include "common/value.hh"

using namespace std::literals;


static void* init() noexcept;
static void deinit(void*) noexcept;
static void handle_deflate(const nf7_node_msg_t*) noexcept;
static void handle_inflate(const nf7_node_msg_t*) noexcept;


static const char* I_inflate[] = {"init", "in", nullptr};
static const char* I_deflate[] = {"start", "in", "end", nullptr};
static const char* O        [] = {"out", "error", nullptr};

extern "C" const nf7_node_t zlib_inflate = {
  .name    = "zlib_inflate",
  .desc    = "inflates a gzip stream by zlib",
  .inputs  = I_inflate,
  .outputs = O,
  .init    = init,
  .deinit  = deinit,
  .handle  = handle_inflate,
};
extern "C" const nf7_node_t zlib_deflate = {
  .name    = "zlib_deflate",
  .desc    = "deflates a byte stream by zlib",
  .inputs  = I_deflate,
  .outputs = O,
  .init    = init,
  .deinit  = deinit,
  .handle  = handle_deflate,
};


struct Context {
 public:
  struct InflateInit final { };
  struct InflateExec final {
    pp::UniqValue v;
  };
  struct DeflateInit final {
    int lv;

    DeflateInit(int l) : lv(l) {
      if (lv < -1 || 9 < lv) {
        throw std::runtime_error {"compression level is out of range (0~9 or -1)"};
      }
    }
  };
  struct DeflateExec final {
    pp::UniqValue v;
  };
  struct DeflateEnd final {
  };
  using V = std::variant<
      InflateInit, InflateExec, DeflateInit, DeflateExec, DeflateEnd>;

  ~Context() noexcept {
    TearDown();
  }
  void operator()(nf7_ctx_t* ctx, const V& v) noexcept
  try {
    std::visit([&](auto& v) { Handle(ctx, v); }, v);
  } catch (std::runtime_error& e) {
    pp::MutValue {ctx->value} = e.what();
    nf7->ctx.exec_emit(ctx, "error", ctx->value, 0);
  }
  void Push(nf7_ctx_t* ctx, V&& v) noexcept {
    q_.PushAndVisit<Context>(ctx, std::move(v));
  }

  void Handle(nf7_ctx_t*, const InflateInit&) {
    TearDown();
    st_.zalloc = Z_NULL;
    st_.zfree  = Z_NULL;
    st_.opaque = Z_NULL;
    if (Z_OK != zng_inflateInit(&st_)) {
      throw std::runtime_error {st_.msg};
    }
    status_ = kInflate;
  }
  void Handle(nf7_ctx_t* ctx, const InflateExec& p) {
    if (status_ != kInflate) {
      Handle(ctx, InflateInit {});
    }
    const auto buf = p.v.vectorOrString();
    st_.next_in  = buf.data();
    st_.avail_in = static_cast<uint32_t>(buf.size());
    Feed(ctx, zng_inflate, Z_NO_FLUSH);
  }

  void Handle(nf7_ctx_t*, const DeflateInit& p) {
    TearDown();
    st_.zalloc = Z_NULL;
    st_.zfree  = Z_NULL;
    st_.opaque = Z_NULL;
    if (Z_OK != zng_deflateInit(&st_, p.lv)) {
      throw std::runtime_error {st_.msg};
    }
    status_ = kDeflate;
  }
  void Handle(nf7_ctx_t* ctx, const DeflateExec& p) {
    if (status_ != kDeflate) {
      Handle(ctx, DeflateInit {6});
    }
    const auto buf = p.v.vectorOrString();
    st_.next_in  = buf.data();
    st_.avail_in = static_cast<uint32_t>(buf.size());
    Feed(ctx, zng_deflate, Z_NO_FLUSH);
  }
  void Handle(nf7_ctx_t* ctx, const DeflateEnd&) {
    if (status_ != kDeflate) {
      throw std::runtime_error {"deflation not started"};
    }
    Feed(ctx, zng_deflate, Z_FINISH);
    TearDown();
  }

 private:
  pp::Queue<V> q_;

  enum { kInitial, kInflate, kDeflate, } status_;
  zng_stream st_;


  void TearDown() noexcept {
    switch (status_) {
    case kInflate: zng_inflateEnd(&st_); break;
    case kDeflate: zng_deflateEnd(&st_); break;
    default: break;
    }
    status_ = kInitial;
  }
  void Feed(nf7_ctx_t* ctx, auto f, auto p) {
    for (st_.avail_out = 0; st_.avail_out == 0;) {
      uint8_t buf[1024];

      st_.next_out  = buf;
      st_.avail_out = sizeof(buf);

      const int ret = f(&st_, p);
      if (ret == Z_STREAM_ERROR ||
          ret == Z_NEED_DICT    ||
          ret == Z_DATA_ERROR   ||
          ret == Z_MEM_ERROR) {
        throw std::runtime_error {st_.msg};
      }

      const auto n = sizeof(buf) - st_.avail_out;
      if (n > 0) {
        auto dst = nf7->value.set_vector(ctx->value, n);
        std::memcpy(dst, buf, n);
        nf7->ctx.exec_emit(ctx, "out", ctx->value, 0);
      }

      if (ret == Z_STREAM_END) break;
    }
  }
};


static void* init() noexcept {
  return new Context;
}
static void deinit(void* ptr) noexcept {
  delete reinterpret_cast<Context*>(ptr);
}

static void handle_inflate(const nf7_node_msg_t* in) noexcept {
  auto& ctx = *reinterpret_cast<Context*>(in->ctx->ptr);
  if (in->name == "init"s) {
    ctx.Push(in->ctx, Context::InflateInit {});
  } else if (in->name == "in"s) {
    ctx.Push(in->ctx, Context::InflateExec {.v = in->value});
  }
}

static void handle_deflate(const nf7_node_msg_t* in) noexcept
try {
  auto  v   = pp::ConstValue(in->value);
  auto& ctx = *reinterpret_cast<Context*>(in->ctx->ptr);
  if (in->name == "start"s) {
    ctx.Push(in->ctx, Context::DeflateInit {v.integerOrScalar<int>()});
  } else if (in->name == "in"s) {
    ctx.Push(in->ctx, Context::DeflateExec {.v = v});
  } else if (in->name == "end"s) {
    ctx.Push(in->ctx, Context::DeflateEnd {});
  }
} catch (std::exception& e) {
  pp::MutValue {in->value} = e.what();
  nf7->ctx.exec_emit(in->ctx, "error", in->value, 0);
}
