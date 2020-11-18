#include <upcxx/upcxx.hpp>
#include <cassert>
#include <iostream>

/*
 * This example illustrates the use of UPCXX_SERIALIZED_FIELDS to serialize a 
 * reference to a statically massive type.
 *
 * This is an example of subset serialization, in which a proper subset of
 * fields is sent without mutation in order to reduce communication costs.
 * Hence, the full object state is not transmitted and the RPCs transmitting
 * these objects must be aware of this so that they do not try to access
 * invalid/uninitialized state.
 */

#ifndef N
#define N (128 * 1024 * 1024)
#endif

class massive {
    public:
        /*
         * A flag used for testing whether an instance was
         * constructed by UPC++ deserialization logic or by user code.
         */
        bool deserialized;

        int value;
        int scratch_space[N];

        // Default constructor used by UPC++ serialization
        massive() {
            std::cout << "deserialization..." << std::endl;
            deserialized = true;
            scratch_space[N-1] = 27;
        }

        // Constructor to be used from user code
        massive(int val) {
            std::cout << "construction..." << std::endl;
            value = val;
            scratch_space[N-1] = 666;
            deserialized = false;
        }

        UPCXX_SERIALIZED_FIELDS(value)
};

int main(void) {
    upcxx::init();

    int rank = upcxx::rank_me();
    int nranks = upcxx::rank_n();

    massive *r = new massive(42);
    assert(!r->deserialized);

    std::cout << "serialization..." << std::endl;
    upcxx::rpc(0, [] (const massive& r_r) {
                /*
                 * Validate that this is a deserialized instance 
                 * and that the entire object was not sent.
                 */
                assert(r_r.deserialized);
                assert(r_r.value==42);
                assert(r_r.scratch_space[N-1] == 27);
            }, std::move(*r)).wait();

    upcxx::barrier();

    if (rank == 0) std::cout << "SUCCESS" << std::endl;

    upcxx::finalize();

    return 0;
}
