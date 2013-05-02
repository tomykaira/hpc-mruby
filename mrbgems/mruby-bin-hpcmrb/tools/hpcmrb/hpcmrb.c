#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mruby.h"
#include "mruby/compile.h"
#include "mruby/dump.h"
#include "mruby/proc.h"
#include "mruby/string.h"

#include "hpcmrb.h"

#define C_EXT ".c"

struct _args {
  FILE *rfp;
  FILE *wfp;
  char *filename;
  mrb_bool check_syntax : 1;
  mrb_bool verbose      : 1;
  mrb_bool debug_info   : 1;
};

/* Ported from src/print.c */
static void
printstr(mrb_state *mrb, mrb_value obj)
{
#ifdef ENABLE_STDIO
  struct RString *str;
  char *s;
  int len;

  if (mrb_string_p(obj)) {
    str = mrb_str_ptr(obj);
    s = str->ptr;
    len = str->len;
    fwrite(s, len, 1, stdout);
  }
#endif
}

static void
show_version(mrb_state *mrb)
{
  static const char version_msg[] = "hpcmrb - HPC-Ruby compiler for Embeddable Ruby  Copyright (c) 2013 HPC-Ruby developers\n";
  mrb_value msg;

  msg = mrb_str_new(mrb, version_msg, sizeof(version_msg) - 1);
  printstr(mrb, msg);
}

static void
show_copyright(mrb_state *mrb)
{
  static const char copyright_msg[] = "hpcmrb - Copyright (c) 2013 HPC-Ruby developers\n";
  mrb_value msg;

  msg = mrb_str_new(mrb, copyright_msg, sizeof(copyright_msg) - 1);
  printstr(mrb, msg);
}

static void
usage(const char *name)
{
  static const char *const usage_msg[] = {
  "switches:",
  "-c           check syntax only",
  "-o<outfile>  place the output into <outfile>",
  "-v           print version number, then turn on verbose mode",
  "-g           produce debugging information",
  "-B<symbol>   binary <symbol> output in C language format",
  "--verbose    run at verbose mode",
  "--version    print the version",
  "--copyright  print the copyright",
  NULL
  };
  const char *const *p = usage_msg;

  printf("Usage: %s [switches] programfile\n", name);
  while(*p)
    printf("  %s\n", *p++);
}

static char *
get_outfilename(mrb_state *mrb, char *infile, char *ext)
{
  char *outfile;
  char *p;

  outfile = (char*)mrb_malloc(mrb, strlen(infile) + strlen(ext) + 1);
  strcpy(outfile, infile);
  if (*ext) {
    if ((p = strrchr(outfile, '.')) == NULL)
      p = &outfile[strlen(outfile)];
    strcpy(p, ext);
  }

  return outfile;
}

static int
parse_args(mrb_state *mrb, int argc, char **argv, struct _args *args)
{
  char *infile = NULL;
  char *outfile = NULL;
  char **origargv = argv;
  int result = EXIT_SUCCESS;
  static const struct _args args_zero = { 0 };

  *args = args_zero;

  for (argc--,argv++; argc > 0; argc--,argv++) {
    if (**argv == '-') {
      if (strlen(*argv) == 1) {
        args->filename = infile = "-";
        args->rfp = stdin;
        break;
      }

      switch ((*argv)[1]) {
      case 'o':
        if (outfile) {
          printf("%s: An output file is already specified. (%s)\n",
                 *origargv, outfile);
          result = EXIT_FAILURE;
          goto exit;
        }
        outfile = get_outfilename(mrb, (*argv) + 2, "");
        break;
      case 'c':
        args->check_syntax = 1;
        break;
      case 'v':
        if(!args->verbose) show_version(mrb);
        args->verbose = 1;
        break;
      case 'g':
        args->debug_info = 1;
        break;
      case '-':
        if (strcmp((*argv) + 2, "version") == 0) {
          show_version(mrb);
          exit(EXIT_SUCCESS);
        }
        else if (strcmp((*argv) + 2, "verbose") == 0) {
          args->verbose = 1;
          break;
        }
        else if (strcmp((*argv) + 2, "copyright") == 0) {
          show_copyright(mrb);
          exit(EXIT_SUCCESS);
        }
        result = EXIT_FAILURE;
        goto exit;
      default:
        break;
      }
    }
    else if (args->rfp == NULL) {
      args->filename = infile = *argv;
      if ((args->rfp = fopen(infile, "r")) == NULL) {
        printf("%s: Cannot open program file. (%s)\n", *origargv, infile);
        goto exit;
      }
    }
  }

  if (infile == NULL) {
    result = EXIT_FAILURE;
    goto exit;
  }
  if (!args->check_syntax) {
    if (outfile == NULL) {
      if (strcmp("-", infile) == 0) {
        outfile = infile;
      }
      else {
        outfile = get_outfilename(mrb, infile, C_EXT);
      }
    }
    if (strcmp("-", outfile) == 0) {
      args->wfp = stdout;
    }
    else if ((args->wfp = fopen(outfile, "wb")) == NULL) {
      printf("%s: Cannot open output file. (%s)\n", *origargv, outfile);
      result = EXIT_FAILURE;
      goto exit;
    }
  }

exit:
  if (outfile && infile != outfile) mrb_free(mrb, outfile);
  return result;
}

static void
cleanup(mrb_state *mrb, struct _args *args)
{
  if (args->rfp)
    fclose(args->rfp);
  if (args->wfp)
    fclose(args->wfp);
  mrb_close(mrb);
}

int
main(int argc, char **argv)
{
  mrb_state *mrb = mrb_open();
  int n = -1;
  struct _args args;
  mrbc_context *c;
  mrb_value result;
  struct HIR *hir;

  puts("Oh, this is just a bytecode compiler.");
  puts("You should implement a FAST HPC compiler.");
  puts("Do your best!! :P");

  if (mrb == NULL) {
    fputs("Invalid mrb_state, exiting mrbc\n", stderr);
    return EXIT_FAILURE;
  }

  n = parse_args(mrb, argc, argv, &args);
  if (n == EXIT_FAILURE || args.rfp == NULL) {
    cleanup(mrb, &args);
    usage(argv[0]);
    return n;
  }

  c = mrbc_context_new(mrb);
  if (args.verbose)
    c->dump_result = 1;
  c->no_exec = 1;
  c->filename = args.filename;

  hpc_state *p = hpc_state_new(mrb);
  init_hpc_compiler(p);
  hir = hpc_compile_file(p, args.rfp, c);

  if (!hir) {
    cleanup(mrb, &args);
    return EXIT_FAILURE;
  }
  if (args.check_syntax) {
    puts("Syntax OK");
    cleanup(mrb, &args);
    return EXIT_SUCCESS;
  }
  result = hpc_generate_code(p, args.wfp, hir, c);
  if (mrb_undef_p(result) || mrb_fixnum(result) < 0) {
    cleanup(mrb, &args);
    return EXIT_FAILURE;
  }

  puts("Done");

  cleanup(mrb, &args);
  return EXIT_SUCCESS;
}
