#define _POSIX_C_SOURCE 200809L
#include "../include/interpreter.h"
#include "../include/parser.h"
#include "../include/opcodes.h"
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {
  Env *e = env_new(NULL);

  if (argc >= 2) {
    arena_reset_total_allocated();
    clock_t start = clock();

    Value x = builtin_load(e, argv[1]);
    if (x.type == VAL_ERR) {
      putchar('\n');
      val_println(x);
    }
    val_clear(&x);

    clock_t end = clock();
    double time_spent = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    printf("\n--- Runtime Statistics ---\n");
    printf("Execution Time: %.2f ms\n", time_spent);
    printf("Peak Memory: %ld KB\n", usage.ru_maxrss);
    printf("Total Allocated: %zu KB\n", (vclang_total_mem + arena_get_total_allocated()) / 1024);
  } else {
    puts("VClang Version 0.7");
    puts("Press Ctrl+C to Exit or type ';' to end expressions.\n");

    // X-3 fix: REPL with multi-line input support (brace counting)
    char line_buf[2048];
    char buffer[16384];
    int brace_depth = 0;
    int buf_len = 0;
    buffer[0] = '\0';

    while (1) {
      if (brace_depth > 0)
        printf("...... ");
      else
        printf("vclang> ");

      if (!fgets(line_buf, sizeof(line_buf), stdin))
        break;

      // Count braces in this line
      for (int i = 0; line_buf[i]; i++) {
        if (line_buf[i] == '{') brace_depth++;
        if (line_buf[i] == '}') brace_depth--;
      }

      // Append to buffer
      int line_len = strlen(line_buf);
      if (buf_len + line_len < (int)sizeof(buffer) - 1) {
        memcpy(buffer + buf_len, line_buf, line_len);
        buf_len += line_len;
        buffer[buf_len] = '\0';
      }

      // Only evaluate when braces are balanced
      if (brace_depth <= 0) {
        brace_depth = 0;
        Arena *a = arena_new(4096);
        ASTNode *ast = parse_source(a, buffer);
        Value r = eval(e, ast);
        val_println(r);
        val_clear(&r);
        arena_free(a);

        // Reset buffer for next input
        buf_len = 0;
        buffer[0] = '\0';
      }
    }
  }

  env_del(e);
  return 0;
}
