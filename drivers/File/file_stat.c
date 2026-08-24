#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>


static void usage(const char *prog)
{
    printf("Usage: %s <file_path> <text>\n",prog);
}

static const char *file_type(mode_t mode)
{
    if(S_ISREG(mode)) return "regular file";
    else if(S_ISDIR(mode)) return "directory";
    else if(S_ISCHR(mode)) return "character device";
    else if(S_ISBLK(mode)) return "block device";
    else if(S_ISFIFO(mode)) return "FIFO/pipe";
    else if(S_ISLNK(mode)) return "symlink";
    else if(S_ISSOCK(mode)) return "socket";
    else return "unknown";
}

static void print_file_stat(const mode_t *mode)
{
    char perm[10];

    perm[0] = (S_ISDIR(*mode)) ? 'd' : '-';
    perm[1] = (*mode & S_IRUSR) ? 'r' : '-';
    perm[2] = (*mode & S_IWUSR) ? 'w' : '-';
    perm[3] = (*mode & S_IXUSR) ? 'x' : '-';
    perm[4] = (*mode & S_IRGRP) ? 'r' : '-';
    perm[5] = (*mode & S_IWGRP) ? 'w' : '-';
    perm[6] = (*mode & S_IXGRP) ? 'x' : '-';
    perm[7] = (*mode & S_IROTH) ? 'r' : '-';
    perm[8] = (*mode & S_IWOTH) ? 'w' : '-';
    perm[9] = '\0';
    printf("Permissions: %s\n", perm);
}

int main(int argc,char *argv[])
{
    const char *path;
    struct stat st;

    if(argc < 2)
    {
        usage(argv[0]);
        return 1;
    }

    path = argv[1];
    if(stat(path,&st) < 0)
    {
        perror("stat");
        return 1;
    }


    printf("path      : %s\n", path);
    printf("type      : %s\n", file_type(st.st_mode));
    printf("size      : %ld bytes\n", st.st_size);
    printf("inode     : %ld\n", st.st_ino);
    printf("mode      : %o\n", st.st_mode & 07777);
    print_file_stat(&st.st_mode);
    return 0;
}