#include "node.h"

extern const nf7_vtable_t* nf7;

static void* init() noexcept;
static void deinit(void*) noexcept;
static void handle(const nf7_node_msg_t*) noexcept;

#define PASSPAWN_DEFINE_NODE_META(N, D, I, O)  \
    extern "C" const nf7_node_t N = {  \
      .name    = #N,        \
      .desc    = D,         \
      .inputs  = I,         \
      .outputs = O,         \
      .init    = init,      \
      .deinit  = deinit,    \
      .handle  = handle,    \
    }
