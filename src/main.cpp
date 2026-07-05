#include <unistd.h>

int main() {
    const char msg[] = "Workspace initialized. AoC Pipeline Online.\n";
    // Direct POSIX syscall file write to stdout (file descriptor 1)
    // Bypasses standard streams entirely
    write(1, msg, sizeof(msg) - 1);
    return 0;
}