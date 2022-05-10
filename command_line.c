#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "jumbo_file_system.h"

#define DISK_FILENAME "DISK"
#define MAX_CMD_LENGTH 2048
#define MAX_ARGS 2
#define WHITESPACE_DELIM " \t\r\n"


void print_error(int err, const char* name) {
    switch (err) {
    case E_SUCCESS:
      break; // do nothing
    case E_NOT_EXISTS:
      printf("directory not found: %s\n", name);
      break;
    case E_EXISTS:
      printf("file already exists: %s\n", name);
      break;
    case E_NOT_DIR:
      printf("%s is not a directory\n", name);
      break;
    case E_IS_DIR:
      printf("%s is a directory\n", name);
      break;
    case E_NOT_EMPTY:
      printf("directory %s is not empty\n", name);
      break;
    case E_MAX_NAME_LENGTH:
      printf("%s exceeds the maximum file name length\n", name);
      break;
    case E_MAX_DIR_ENTRIES:
      printf("directory is full (max entries reached)\n");
      break;
    case E_MAX_FILE_SIZE:
      printf("file is full (max file size reached)\n");
      break;
    case E_DISK_FULL:
      printf("disk is full");
      break;
    case E_UNKNOWN:
      printf("an unknown error occurred\n");
      break;
    default:
      printf("an unrecognized error (%d) occurred\n", err);
    }
}


/* run_command
 *   Runs one entire command line, which may include multiple pipeline stages
 */
void run_command(char* command_line) {
  /* Parse the arguments */
  char* saveptr = NULL; /* used internally by strtok_r */
  char* tokens[MAX_ARGS + 2]; // +1 for the command itself, +1 for a NULL
  memset(tokens, 0, sizeof(tokens));
  tokens[0] = strtok_r(command_line, WHITESPACE_DELIM, &saveptr);
  for (int i = 1; tokens[i-1] != NULL && i < (MAX_ARGS+2); ++i) {
    tokens[i] = strtok_r(NULL, WHITESPACE_DELIM, &saveptr);
  }
  if (NULL != tokens[MAX_ARGS+1]) {
    /* overwrote NULL => too many args error */
    fprintf(stderr, "ERROR: too many arguments on the command line\n");
    return;
  }

  if (NULL == tokens[0]) {
    // blank line; do nothing

  } else if (0 == strcmp(tokens[0], "cd")) {
    if (NULL != tokens[2]) {
      fprintf(stderr, "usage: cd [dir_name]\n(dir_name is optional; leaving it out will return to the root directory)\n");
      return;
    }

    // Note: tokens[1] == NULL is valid; this should return to the root directory
    int ret = jfs_chdir(tokens[1]);
    print_error(ret, tokens[1]);

  } else if (0 == strcmp(tokens[0], "mkdir")) {
    if (NULL == tokens[1] || NULL != tokens[2]) {
      fprintf(stderr, "usage: mkdir <dir_name>\n");
      return;
    }
    int ret = jfs_mkdir(tokens[1]);
    print_error(ret, tokens[1]);

  } else if (0 == strcmp(tokens[0], "rmdir")) {
    if (NULL == tokens[1] || NULL != tokens[2]) {
      fprintf(stderr, "usage: rmdir <dir_name>\n");
      return;
    }
    int ret = jfs_rmdir(tokens[1]);
    print_error(ret, tokens[1]);

  } else if (0 == strcmp(tokens[0], "ls")) {
    if (NULL != tokens[1]) {
      fprintf(stderr, "usage: ls\n");
      return;
    }

    char* directories[MAX_DIR_ENTRIES+1];
    char* files[MAX_DIR_ENTRIES+1];
    memset(directories, -1, MAX_DIR_ENTRIES * sizeof(const char*));
    memset(files,       -1, MAX_DIR_ENTRIES * sizeof(const char*));
    int ret = jfs_ls(directories, files);

    if (E_SUCCESS == ret) {
      for (int i = 0; NULL != directories[i]; i++) {
        printf("%s/\n", directories[i]);
        free(directories[i]);
      }
      for (int i = 0; NULL != files[i]; i++) {
        printf("%s\n", files[i]);
        free(files[i]);
      }
    } else {
      printf("ls failed - but ls should never fail!\n");
    }

  } else if (0 == strcmp(tokens[0], "touch")) {
    if (NULL == tokens[1] || NULL != tokens[2]) {
      fprintf(stderr, "usage: touch <file_name>\n");
      return;
    }
    int ret = jfs_creat(tokens[1]);
    print_error(ret, tokens[1]);

  } else if (0 == strcmp(tokens[0], "rm")) {
    if (NULL == tokens[1] || NULL != tokens[2]) {
      fprintf(stderr, "usage: rm <file_name>\n");
      return;
    }
    int ret = jfs_remove(tokens[1]);
    print_error(ret, tokens[1]);

  } else if (0 == strcmp(tokens[0], "stat")) {
    if (NULL == tokens[1] || NULL != tokens[2]) {
      fprintf(stderr, "usage: stat <file_name>\n");
      return;
    }

    struct stats file_stats;
    memset(&file_stats, -1, sizeof(file_stats));
    int ret = jfs_stat(tokens[1], &file_stats);

    if (E_SUCCESS == ret) {
      if (!file_stats.is_dir) {
        printf("Directory name: %s\n", file_stats.name);
        printf("Directory block number: %u\n", file_stats.block_num);
      } else {
        // is regular file
        printf("File name: %s\n", file_stats.name);
        printf("Inode block number: %u\n", file_stats.block_num);
        printf("Number of data blocks: %u\n", file_stats.num_data_blocks);
        printf("File size: %u\n", file_stats.file_size);
      }
    } else {
      print_error(ret, tokens[1]);
    }

  } else if (0 == strcmp(tokens[0], "cat")) {
    if (NULL == tokens[1] || NULL != tokens[2]) {
      fprintf(stderr, "usage: cat <file_name>\n");
      return;
    }

    unsigned short bytes_read = MAX_FILE_SIZE;
    char file_data[MAX_FILE_SIZE];
    memset(file_data, -1, MAX_FILE_SIZE);
    int ret = jfs_read(tokens[1], file_data, &bytes_read);

    if (E_SUCCESS == ret) {
      ret = write(STDOUT_FILENO, file_data, bytes_read);
      printf("\n");
      if (ret != bytes_read) {
        perror("Failed to write file data to stdout");
      }
    } else {
      print_error(ret, tokens[1]);
    }

  } else if (0 == strcmp(tokens[0], "append")) {
    if (NULL == tokens[1] || NULL == tokens[2]) {
      fprintf(stderr, "usage: append <file_name> <data>\n");
      return;
    }

    int ret = jfs_write(tokens[1], tokens[2], strlen(tokens[2]));
    print_error(ret, tokens[1]);

  } else {
    fprintf(stderr, "ERROR: unrecognized command\n");
  }
}


/* prompt_for_input
 *   Prompts the user for a command, and takes a line of input
 */
void prompt_for_input(char* input_buffer, int buflen) {
  int done = 0; // FALSE
  while (!done) {
    printf("jfs$ ");  /* prompt */
    if (NULL == fgets(input_buffer, buflen, stdin)) {
      perror("FATAL ERROR: fgets failed");
      exit(1);
      /* alternatively, we could have looped to try input again
       * but that runs the risk of an infinite loop if input is totally broken */
    }

    /* Note that if buflen isn't long enough, that isn't an error.
     * fgets will simply get as many characters as there is room for,
     * and leave the rest still in the stdin buffer.
     * Our solution will be to detect the situation, report an error,
     * empty out the stdin buffer, and then re-prompt for a new input.
     */
    if (input_buffer[strlen(input_buffer)-1] != '\n') {
      fprintf(stderr, "ERROR: line exceeds maximum command line length\n");
      while('\n' != getc(stdin)) {} /* consume rest of line from the buffer */
    } else {
      done = 1; // TRUE
    }
  }
}


int main() {
  char input_buffer[MAX_CMD_LENGTH];

  /*
  printf("File system parameters:\n");
  printf("MAX_NAME_LENGTH = %d\n", MAX_NAME_LENGTH);
  printf("MAX_DIR_ENTRIES = %ld\n", MAX_DIR_ENTRIES);
  printf("MAX_DATA_BLOCKS = %ld\n", MAX_DATA_BLOCKS);
  printf("MAX_FILE_SIZE = %ld\n", MAX_FILE_SIZE);
  printf("NUM_BLOCKS = %d\n", NUM_BLOCKS);
  printf("BLOCK_SIZE = %d\n", BLOCK_SIZE);
  printf("sizeof block struct = %ld\n\n", sizeof(struct block));
  */

  jfs_mount(DISK_FILENAME);

  prompt_for_input(input_buffer, MAX_CMD_LENGTH);
  while (0 != strcmp(input_buffer, "exit\n")) {
    run_command(input_buffer); /* may alter input_buffer!! */
    prompt_for_input(input_buffer, MAX_CMD_LENGTH);
  }

  jfs_unmount();
  return 0;
}
