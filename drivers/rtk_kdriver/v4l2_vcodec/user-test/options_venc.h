
#include <getopt.h>

enum {
    OPT_INPUT,
    OPT_SOURCE,
    OPT_TEST,
    OPT_LIST_TESTS,
    OPT_OUTPUT,
    OPT_HELP
};

typedef struct {
    char* input;
    int   input_cnt;
    char* source;
    int   source_cnt;
    char* test;
    int   test_cnt;
    char  list_tests;
    int   list_tests_cnt;
    char* output;
    int   output_cnt;
} opts_t;

static struct option long_options[] = {
    {"input",      required_argument, 0, OPT_INPUT},
    {"source",     required_argument, 0, OPT_SOURCE},
    {"test",       required_argument, 0, OPT_TEST},
    {"list_tests",       no_argument, 0, OPT_LIST_TESTS},
	{"output",	   required_argument, 0, OPT_OUTPUT},
    {"help",             no_argument, 0, OPT_HELP},
    {0, 0, 0, 0}
};

#define OPTSTRING "i:s:t:o:"

static void printUsage ()
{
    printf("\t-i, --input file  : input file (Elementary Stream)\n");
    printf("\t-s, --source type : source type (mpeg2, avc, hevc, avs, avs2) (Default: avc)\n");
    printf("\t-t, --test IDs    : test IDs (Default: 1)\n");
    printf("\t--list_tests      : list tests\n");
}

static void parseArgs(opts_t *opts, int argc, char **argv)
{
    int opt;
    while ((opt = getopt_long (argc, argv, OPTSTRING, long_options, NULL)) != -1)
        switch (opt) {
            case 'i':
            case OPT_INPUT:
                opts->input = optarg;
                opts->input_cnt++;
                break;
            case 's':
            case OPT_SOURCE:
                opts->source = optarg;
                opts->source_cnt++;
                break;
            case 't':
            case OPT_TEST:
                opts->test = optarg;
                opts->test_cnt++;
                break;
            case OPT_LIST_TESTS:
                opts->list_tests = 1;
                opts->list_tests_cnt++;
                break;

			case 'o':
			case OPT_OUTPUT:
				opts->output = optarg;
				opts->output_cnt++;
				break;

            case OPT_HELP:
            default:
                printUsage();
                exit(0);
                break;
        }
}

opts_t opts = {
    .source = "avc",
    .test = "1",
};

