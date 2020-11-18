#include <upcxx/os_env.hpp>
#include <cstdlib>

char *(*upcxx::detail::getenv)(const char *key) = std::getenv;

void (*upcxx::detail::getenv_report)(const char *key, const char *val, bool is_dflt) =
                                 ([](const char *key, const char *val, bool is_dflt){});

