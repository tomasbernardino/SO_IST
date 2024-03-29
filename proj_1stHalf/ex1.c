#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
//#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if (argc > 2) {
    char *endptr;
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  char *dirpath = argv[1];
  DIR *dir = opendir(dirpath);

  if(dir == NULL) {
    fprintf(stderr, "Failed to open directory\n");
    return 1;
  }
  

  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL) {
    unsigned int event_id, delay;
    size_t num_rows, num_columns, num_coords;
    size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
    int fd;
    char filename[strlen(entry->d_name)+1];
    strcpy(filename, entry->d_name);

    char *filepath = (char *)malloc(strlen(dirpath) + strlen("/") + strlen(filename)+1);
    strcpy(filepath, dirpath);
    strcat(filepath, "/");
    strcat(filepath, filename);

    
    //printf("> ");
    //fflush(stdout);
    if(strstr(filename, ".jobs") == NULL){ 
      free(filepath);
      continue;
    }
    else if(strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0){
      free(filepath);
      continue;
    }
    else
      fd = open(filepath, O_RDONLY);
    
    if(fd == -1) {
      free(filepath);
      fprintf(stderr, "Failed to open file\n");
      continue;
    }

    int var = 1;
    while (var){
      switch (get_next(fd)) {
        case CMD_CREATE:
          if (parse_create(fd, &event_id, &num_rows, &num_columns) != 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (ems_create(event_id, num_rows, num_columns)) {
            fprintf(stderr, "Failed to create event\n");
          }

          break;

        case CMD_RESERVE:
          num_coords = parse_reserve(fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);

          if (num_coords == 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (ems_reserve(event_id, num_coords, xs, ys)) {
            fprintf(stderr, "Failed to reserve seats\n");
          }

          break;

        case CMD_SHOW:
          if (parse_show(fd, &event_id) != 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (ems_show(event_id, filepath)) {
            fprintf(stderr, "Failed to show event\n");
          }
          
          break;

        case CMD_LIST_EVENTS:
          if (ems_list_events()) {
            fprintf(stderr, "Failed to list events\n");
          }

          break;

        case CMD_WAIT:
          if (parse_wait(fd, &delay, NULL) == -1) {  // thread_id is not implemented
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (delay > 0) {
            printf("Waiting...\n");
            ems_wait(delay);
          }

          break;

        case CMD_INVALID:
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          break;

        case CMD_HELP:
          printf(
              "Available commands:\n"
              "  CREATE <event_id> <num_rows> <num_columns>\n"
              "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
              "  SHOW <event_id>\n"
              "  LIST\n"
              "  WAIT <delay_ms> [thread_id]\n"  // thread_id is not implemented
              "  BARRIER\n"                      // Not implemented
              "  HELP\n");

          break;

        case CMD_BARRIER:  // Not implemented
        case CMD_EMPTY:
          break;

        case EOC:
          var = 0;
          break;
      }
    }
    free(filepath);//FIXME
    close(fd);
  }
  ems_terminate();
  closedir(dir);
}