#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <variant>

#include "nf7.hh"

#include "common/queue.hh"
#include "common/value.hh"

using namespace std::literals;


static void* init() noexcept;
static void deinit(void*) noexcept;
static void handle_read(const nf7_node_msg_t*) noexcept;
static void handle_write(const nf7_node_msg_t*) noexcept;

static const char* I_read[] = {"open", "read", "skip", "seek", "close", nullptr};
static const char* O_read[] = {"data", "done", "error", nullptr};
extern "C" const nf7_node_t nfile_read = {
  .name    = "nfile_read",
  .desc    = "reads data from a native file specified by path",
  .inputs  = I_read,
  .outputs = O_read,
  .init    = init,
  .deinit  = deinit,
  .handle  = handle_read,
};

static const char* I_write[] = {"open", "write", "skip", "seek", "close", nullptr};
static const char* O_write[] = {"done", "error", nullptr};
extern "C" const nf7_node_t nfile_write = {
  .name    = "nfile_write",
  .desc    = "writes data to a native file specified by path",
  .inputs  = I_write,
  .outputs = O_write,
  .init    = init,
  .deinit  = deinit,
  .handle  = handle_write,
};


struct Context final {
 public:
  struct ReadOpen final {
    std::filesystem::path npath;
  };
  struct ReadExec final {
    std::streamsize n;
    std::optional<std::ifstream::off_type> off;
  };
  struct ReadSkip final {
    std::ifstream::off_type n;
  };
  struct ReadSeek final {
    std::ifstream::off_type n;
  };

  struct WriteOpen final {
    std::filesystem::path npath;
  };
  struct WriteExec final {
    pp::UniqValue v;
    std::optional<std::ifstream::off_type> off;
  };
  struct WriteSkip final {
    std::ofstream::off_type n;
  };
  struct WriteSeek final {
    std::ofstream::off_type n;
  };

  struct Close final { };

  using V = std::variant<
      ReadOpen, ReadExec, ReadSkip, ReadSeek,
      WriteOpen, WriteExec, WriteSkip, WriteSeek,
      Close>;

  void operator()(nf7_ctx_t* ctx, const V& v) noexcept
  try {
    try {
      std::visit([&](auto& v) { Handle(ctx, v); }, v);
    } catch (std::bad_variant_access&) {
      throw std::runtime_error {"invalid state"};
    }
    pp::MutValue {ctx->value} = pp::MutValue::Pulse {};
    nf7->ctx.exec_emit(ctx, "done", ctx->value, 0);
  } catch (std::exception& e) {
    pp::MutValue {ctx->value} = e.what();
    nf7->ctx.exec_emit(ctx, "error", ctx->value, 0);
  }
  void Push(nf7_ctx_t* ctx, V&& v) noexcept {
    q_.PushAndVisit<Context>(ctx, std::move(v));
  }

  void Handle(nf7_ctx_t*, const ReadOpen& p) {
    st_ = std::monostate {};
    st_ = std::ifstream {p.npath, std::ios::binary};
    if (!std::get<std::ifstream>(st_)) {
      throw std::runtime_error {"failed to open"};
    }
  }
  void Handle(nf7_ctx_t* ctx, const ReadExec& p) {
    if (p.n == 0) return;
    auto& st = std::get<std::ifstream>(st_);

    if (p.off) {
      st.seekg(*p.off, std::ios_base::beg);
      if (!st) throw std::runtime_error {"failed to seek before reading"};
    }

    const auto max = st.rdbuf()->in_avail();
    const auto n   = std::min(max, p.n > 0? p.n: max);

    auto ptr = nf7->value.set_vector(ctx->value, static_cast<size_t>(n));
    st.read(reinterpret_cast<char*>(ptr), n);
    if (!st) throw std::runtime_error {"failed to read"};

    nf7->ctx.exec_emit(ctx, "data", ctx->value, 0);
  }
  void Handle(nf7_ctx_t*, const ReadSkip& p) {
    auto& st = std::get<std::ifstream>(st_);
    st.seekg(p.n, std::ios_base::cur);
    if (!st) throw std::runtime_error {"failed to skip"};
  }
  void Handle(nf7_ctx_t*, const ReadSeek& p) {
    auto& st = std::get<std::ifstream>(st_);
    st.seekg(p.n, std::ios_base::beg);
    if (!st) throw std::runtime_error {"failed to seek"};
  }

  void Handle(nf7_ctx_t*, const WriteOpen& p) {
    st_ = std::monostate {};
    st_ = std::ofstream {p.npath, std::ios::binary};
    if (!std::get<std::ofstream>(st_)) {
      throw std::runtime_error {"failed to open"};
    }
  }
  void Handle(nf7_ctx_t*, const WriteExec& p) {
    auto& st  = std::get<std::ofstream>(st_);
    auto  str = p.v.stringOrVector();
    if (p.off) {
      st.seekp(*p.off, std::ios_base::beg);
      if (!st) throw std::runtime_error {"failed to seek before writing"};
    }
    st.write(str.data(), static_cast<std::streamsize>(str.size()));
    if (!st) throw std::runtime_error {"failed to write"};
  }
  void Handle(nf7_ctx_t*, const WriteSkip& p) {
    auto& st = std::get<std::ofstream>(st_);
    st.seekp(p.n, std::ios_base::cur);
    if (!st) throw std::runtime_error {"failed to skip"};
  }
  void Handle(nf7_ctx_t*, const WriteSeek& p) {
    auto& st = std::get<std::ofstream>(st_);
    st.seekp(p.n, std::ios_base::beg);
    if (!st) throw std::runtime_error {"failed to seek"};
  }

  void Handle(nf7_ctx_t*, const Close&) {
    st_ = std::monostate {};
  }

 private:
  pp::Queue<V> q_;

  std::variant<std::monostate, std::ifstream, std::ofstream> st_;
};

static void* init() noexcept { return new Context; }
static void deinit(void* ptr) noexcept { delete reinterpret_cast<Context*>(ptr); }

static void handle_read(const nf7_node_msg_t* in) noexcept
try {
  auto& ctx = *reinterpret_cast<Context*>(in->ctx->ptr);
  auto  v   = pp::ConstValue {in->value};
  if (in->name == "open"s) {
    // TODO: get Env::npath()
    ctx.Push(in->ctx, Context::ReadOpen {.npath = v.string()});
  } else if (in->name == "read"s) {
    Context::ReadExec p;
    switch (v.type()) {
    case NF7_INTEGER:
      p.n = v.integer<std::streamsize>();
      break;
    case NF7_SCALAR:
      p.n = v.scalar<std::streamsize>();
      break;
    case NF7_TUPLE:
      p.n   = v["size"].integerOrScalar<std::streamsize>();
      p.off = v["offset"].integerOrScalar<std::ifstream::off_type>();
      break;
    default:
      throw std::runtime_error {"invalid input"};
    }
    ctx.Push(in->ctx, std::move(p));
  } else if (in->name == "skip"s) {
    ctx.Push(in->ctx, Context::ReadSkip {.n = v.integerOrScalar<std::ifstream::off_type>()});
  } else if (in->name == "seek"s) {
    ctx.Push(in->ctx, Context::ReadSeek {.n = v.integerOrScalar<std::ifstream::off_type>()});
  } else if (in->name == "close"s) {
    ctx.Push(in->ctx, Context::Close {});
  }
} catch (std::exception& e) {
  pp::MutValue {in->value} = e.what();
  nf7->ctx.exec_emit(in->ctx, "error", in->value, 0);
}
static void handle_write(const nf7_node_msg_t* in) noexcept
try {
  auto  v   = pp::ConstValue {in->value};
  auto& ctx = *reinterpret_cast<Context*>(in->ctx->ptr);
  if (in->name == "open"s) {
    // TODO: get Env::npath()
    ctx.Push(in->ctx, Context::WriteOpen {.npath = v.string()});
  } else if (in->name == "write"s) {
    std::optional<Context::WriteExec> p;
    switch (v.type()) {
    case NF7_VECTOR:
    case NF7_STRING:
      p = Context::WriteExec {.v = v, .off = std::nullopt};
      break;
    case NF7_TUPLE:
      p = Context::WriteExec {
        .v   = v["buffer"],
        .off = v["offset"].integerOrScalar<std::ifstream::off_type>(),
      };
      break;
    default:
      throw std::runtime_error {"invalid input"};
    }
    ctx.Push(in->ctx, std::move(*p));
  } else if (in->name == "skip"s) {
    ctx.Push(in->ctx, Context::WriteSkip {.n = v.integerOrScalar<std::ifstream::off_type>()});
  } else if (in->name == "seek"s) {
    ctx.Push(in->ctx, Context::WriteSeek {.n = v.integerOrScalar<std::ifstream::off_type>()});
  } else if (in->name == "close"s) {
    ctx.Push(in->ctx, Context::Close {});
  }
} catch (std::exception& e) {
  pp::MutValue {in->value} = e.what();
  nf7->ctx.exec_emit(in->ctx, "error", in->value, 0);
}
