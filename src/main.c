#define _POSIX_C_SOURCE 200809L
#include "memory.c"
#include "ast.c"
#include "lexer.c"
#include "parser.c"
#include "interpreter.c"
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {
  lenv *e = lenv_new();

  if (argc >= 2) {
    clock_t start = clock();
    val *args = val_sexpr();
    val_add(args, val_str(argv[1]));
    val *x = builtin_load(e, args);
    // Silent mode: Only print if error
    if (x->type == VAL_ERR) {
      putchar('\n');
      val_println(x);
    }
    val_del(x);

    clock_t end = clock();
    double time_spent = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    printf("\n--- Runtime Statistics ---\n");
    printf("Execution Time: %.2f ms\n", time_spent);
    printf("Peak Memory: %ld KB\n", usage.ru_maxrss);
    printf("Total Allocated: %ld KB\n", vclang_total_mem / 1024);
  } else {
    puts("VClang Version 0.5");
    puts("Press Ctrl+C to Exit or type ';' to end expressions.\n");

    char buffer[2048];
    while (1) {
      printf("vclang> ");
      if (!fgets(buffer, 2048, stdin))
        break;
      val *x = val_read(buffer);
      val *r = val_eval(e, x);
      val_println(r);
      val_del(r);
    }
  }

  lenv_del(e);
  return 0;
}
