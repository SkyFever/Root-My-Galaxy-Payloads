#include <unistd.h>

int main(void) {
  static const char message[] =
      "KernelSU is not available for this diagnostic target.\n";
  (void)write(STDERR_FILENO, message, sizeof(message) - 1);
  return 78;
}
