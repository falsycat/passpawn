#include <filesystem>
#include <fstream>
#include <iostream>
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
static const char* O_read[] = {"data", "error", nullptr};
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
static const char* O_write[] = {"error", nullptr};
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
  };
  struct ReadSkip final {
    std::ofstream::off_type n;
  };
  struct ReadSeek final {
    std::ofstream::off_type n;
  };

  struct WriteOpen final {
    std::filesystem::path npath;
  };
  struct WriteExec final {
    pp::UniqValue v;
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
    std::visit([&](auto& v) { Handle(ctx, v); }, v);
  } catch (std::bad_variant_access&) {
    pp::Set(ctx->value, "invalid state");
    nf7->ctx.exec_emit(ctx, "error", ctx->value, 0);
  } catch (std::runtime_error& e) {
    pp::Set(ctx->value, e.what());
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
    auto& st = std::get<std::ofstream>(st_);
    const auto v = p.v.get();

    size_t n;
    const char* ptr;
    switch (nf7->value.get_type(v)) {
    case NF7_STRING:
      ptr = nf7->value.get_string(v, &n);
      break;
    case NF7_VECTOR:
      ptr = reinterpret_cast<const char*>(nf7->value.get_vector(v, &n));
      break;
    default:
      throw std::runtime_error {"expected string or vector"};
    }

    st.write(reinterpret_cast<const char*>(ptr), static_cast<std::streamsize>(n));
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
  if (in->name == "open"s) {
    // TODO: get Env::npath()
    ctx.Push(in->ctx, Context::ReadOpen {.npath = pp::Get<const char*>(in->value)});
  } else if (in->name == "read"s) {
    ctx.Push(in->ctx, Context::ReadExec {.n = pp::Get<std::streamsize>(in->value)});
  } else if (in->name == "skip"s) {
    const auto n = pp::Get<std::ifstream::off_type>(in->value);
    ctx.Push(in->ctx, Context::ReadSkip {.n = n});
  } else if (in->name == "seek"s) {
    const auto n = pp::Get<std::ifstream::off_type>(in->value);
    ctx.Push(in->ctx, Context::ReadSeek {.n = n});
  } else if (in->name == "close"s) {
    ctx.Push(in->ctx, Context::Close {});
  }
} catch (std::exception& e) {
  pp::Set(in->value, e.what());
  nf7->ctx.exec_emit(in->ctx, "error", in->value, 0);
}

static void handle_write(const nf7_node_msg_t* in) noexcept
try {
  auto& ctx = *reinterpret_cast<Context*>(in->ctx->ptr);
  if (in->name == "open"s) {
    // TODO: get Env::npath()
    ctx.Push(in->ctx, Context::WriteOpen {.npath = pp::Get<const char*>(in->value)});
  } else if (in->name == "write"s) {
    ctx.Push(in->ctx, Context::WriteExec {.v = in->value});
  } else if (in->name == "skip"s) {
    const auto n = pp::Get<std::ifstream::off_type>(in->value);
    ctx.Push(in->ctx, Context::WriteSkip {.n = n});
  } else if (in->name == "seek"s) {
    const auto n = pp::Get<std::ifstream::off_type>(in->value);
    ctx.Push(in->ctx, Context::WriteSeek {.n = n});
  } else if (in->name == "close"s) {
    ctx.Push(in->ctx, Context::Close {});
  }
} catch (std::exception& e) {
  pp::Set(in->value, e.what());
  nf7->ctx.exec_emit(in->ctx, "error", in->value, 0);
}