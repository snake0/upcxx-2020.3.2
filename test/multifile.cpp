/* File does nothing except pull in a header. Nobs sees this and
 * automatically looks for the soruce file with a matching name
 * {.hpp -> .cpp}. This search mechanism is applied to every header
 * residing in "src/" and "test/".
 * 
 * In our case "multifile-buddy.cpp" has everything.
 */
 
// GOOD
#include "multifile-buddy.hpp"

// BAD (but works)
//#include "multifile-buddy.cpp"
