#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>          /* for ioctl() */

int main(int argc, char **argv) {
    int power_fd;

    if ((power_fd = open("/dev/empeg_power", O_RDONLY)) != -1) {
        ioctl( power_fd, _IO('p', 0), 0 );
    }

    return 0;
}
