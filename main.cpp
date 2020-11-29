#include <fcntl.h>
#include <mach-o/loader.h>
#include <stdio.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, const char *argv[]) {
  if (argc != 4) {
    printf("Usage: %s [path_to_macho] [old_dylib] [new_dylib]\n", argv[0]);
    return 1;
  }

  const char *input_file = argv[1];
  std::string old_dylib(argv[2]);
  std::string new_dylib(argv[3]);

  int fd = open(input_file, O_RDWR);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  struct stat file_stat;
  assert(fstat(fd, &file_stat) >= 0);
  void *file = mmap(nullptr, file_stat.st_size, PROT_READ | PROT_WRITE,
                    MAP_SHARED, fd, 0);
  assert(file != nullptr);
  printf("Reading %s\n", argv[1]);

  // ref:
  // https://stackoverflow.com/questions/48301420/how-to-edit-load-command-in-mach-o-file-and-successfully-link-it-on-tvos
  struct mach_header_64 *hdr = (struct mach_header_64 *)file;

  for (struct load_command *
           cmd = (struct load_command *)(hdr + 1),
          *end = (struct load_command *)((uintptr_t)cmd + hdr->sizeofcmds);
       cmd < end;
       cmd = (struct load_command *)((uintptr_t)cmd + cmd->cmdsize)) {
    if (cmd->cmd == LC_LOAD_DYLIB) {
      struct dylib_command *dylib_cmd = (struct dylib_command *)cmd;
      std::string name((char *)cmd + dylib_cmd->dylib.name.offset,
                       cmd->cmdsize - dylib_cmd->dylib.name.offset);
      printf("Found LC_LOAD_DYLIB: %s\n", name.c_str());
      // zero padded, so don't use '==' here
      if (strcmp(name.c_str(), old_dylib.c_str()) == 0) {
        printf("Patching %s -> %s\n", name.c_str(), new_dylib.c_str());

        // Calculate new cmdsize
        printf("New dylib length is %zu\n", new_dylib.length() + 1);
        uint32_t new_cmdsize = sizeof(dylib_command) + new_dylib.length() + 1;
        // Align up to multiples of 8
        new_cmdsize = (new_cmdsize + 7) & (~0b111);
        printf("Cmdsize is going to change from %d to %d\n", cmd->cmdsize,
               new_cmdsize);

        printf("Moving cmds after this\n");
        uintptr_t next_cmd = (uintptr_t)cmd + cmd->cmdsize;
        memmove((void *)((uintptr_t)cmd + new_cmdsize), (void *)next_cmd,
                (uintptr_t)end - next_cmd);

        // Update dylib path
        printf("Updating dylib path\n");
        memset((void *)((uintptr_t)cmd + sizeof(dylib_command)), 0,
               new_cmdsize - sizeof(dylib_command));
        memcpy((void *)((uintptr_t)cmd + sizeof(dylib_command)),
               new_dylib.data(), new_dylib.length());

        // Update cmd size
        printf("Updating header sizes\n");
        hdr->sizeofcmds = hdr->sizeofcmds - cmd->cmdsize + new_cmdsize;
        cmd->cmdsize = new_cmdsize;
        dylib_cmd->dylib.name.offset = sizeof(dylib_command);
        break;
      }
    }
  }

  return 0;
}