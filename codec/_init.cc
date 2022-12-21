#include "nf7.hh"

const nf7_vtable_t* nf7;


extern "C" void nf7_init(nf7_init_t* init) noexcept {
  nf7 = init->vtable;

# define REGISTER_(name) do {  \
    extern const nf7_node_t name;  \
    nf7->init.register_node(init, &name);  \
  } while (0)

  REGISTER_(stb_image);

# undef REGISTER_
}
