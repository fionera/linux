#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


typedef struct StringBuilder {
  char buffer[1024];
  size_t position;
} StringBuilder;

static
void StringBuilder_Append(StringBuilder *sb, const char *str, size_t length) {
  char *ptr = &sb->buffer[sb->position];
  size_t remain = sizeof(sb->buffer) - sb->position - 1;
  size_t l = length < remain ? length : remain;
  memcpy(ptr, str, l);
  sb->position += l;
  sb->buffer[sb->position] = '\0';
}

static
void StringBuilder_AppendString(StringBuilder *sb, const char *str) {
  StringBuilder_Append(sb, str, strlen(str));
}

static
void StringBuilder_AppendChar(StringBuilder *sb, char c) {
  StringBuilder_Append(sb, &c, 1);
}

static
void StringBuilder_Dump(StringBuilder *sb) {
  size_t i = 0;
  size_t s = 0;
  for (i = 0; i < sb->position; i++) {
    if (sb->buffer[i] == '\n') {
      sb->buffer[i] = '\0';
      printf ("%s", &sb->buffer[s]);
      sb->buffer[i] = '\n';
      s = i + 1;
    }
  }
  if (s < sb->position) {
    printf ("%s", &sb->buffer[s]);
  }
}

void dump_hex_helper(StringBuilder *sb, const char * name,
                     const uint8_t* vector, size_t length) {
  int a, b;
  size_t i;
  static char int_to_hexcar[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                   '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  if (vector == NULL) {
    printf (" NULL \n");
    return;
  }
  for (i = 0; i < length; i++) {
    if (i == 0) {
      StringBuilder_AppendString(sb, "\n    a2b_hex(\"");
    } else if (i % 32 == 0) {
      StringBuilder_AppendString(sb, "\"\n            \"");
    }
    a = vector[i] % 16;
    b = (vector[i] - a) / 16;
    StringBuilder_AppendChar(sb, int_to_hexcar[b]);
    StringBuilder_AppendChar(sb, int_to_hexcar[a]);
  }
}

void dump_hex (char* name, const void* data, size_t length) {
  StringBuilder sb;
  const uint8_t* vector = (const uint8_t*) data;

  printf (" ============ %s dump (len:%d) ===========\n", name, length);
  while (length > 0) {
    sb.position = 0;
    if (length > 256) {
      dump_hex_helper(&sb, name, vector, 256);
      vector += 256;
      length -= 256;
    } else {
      dump_hex_helper(&sb, name, vector, length);
      length = 0;
    }
    StringBuilder_Dump(&sb);
  }
  printf (" =========================================\n");
}
