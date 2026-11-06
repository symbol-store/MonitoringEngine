/* Symbols chibi's bundled C modules link against but BOSS never exercises;
 * present only to satisfy the static linker. */
#include <errno.h>
#include <sys/time.h>

int settimeofday(const struct timeval* time_value, const struct timezone* time_zone) {
  (void)time_value;
  (void)time_zone;
  errno = ENOSYS;
  return -1;
}
