#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define BUF_SIZE	(128 * 1024)

struct block_list {
	char *txt;
	int len;
	int pfn;
};

static struct block_list *list;
static int list_size;
static int max_size;

static int read_block(char *buf, int buf_size, FILE *fin)
{
	char *curr = buf, *const buf_end = buf + buf_size;

	while (buf_end - curr > 1 && fgets(curr, buf_end - curr, fin)) {
		if (*curr == '\n')
			return curr - buf;
		curr += strlen(curr);
	}

	return -1;
}

static char *extract(char *buf, char *left, char *right)
{
	char *head, *tail, *string;
	int len;

	len = strlen(left);
	head = strstr(buf, left);
	head += len + 1;
	tail = strstr(buf, right);
	len = tail - head;

	string = malloc(len);
	memcpy(string, head, len);
	string[len] = '\0';

	return string;
}

static void add_list(char *buf, int len, char *fname)
{
	char *tmp;

	if (list_size != 0 &&
			len == list[list_size-1].len &&
			memcmp(buf, list[list_size-1].txt, len) == 0)
		return;

	if (list_size == max_size) {
		printf("max_size too small??\n");
		exit(1);
	}

	tmp = strstr(buf, fname);

	if (tmp) {
		if ((*(tmp - 2) == ']') && (*(tmp + strlen(fname)) == '+')) {
			list[list_size].txt = malloc(len+1);
			list[list_size].len = len;
			list[list_size].pfn =
				strtol(extract(buf, "PFN", "Block"), (char **)NULL, 10);

			memcpy(list[list_size].txt, buf, len);
			list[list_size].txt[len] = 0;
			list_size++;
		}
	}
}

static int check_contig_range(void)
{
	int i;
	unsigned int start, end, sum;

	start = end = list[0].pfn;
	sum = 0;
	printf("physical address\tsize(byte)\n");
	for (i = 0; i < list_size; i++) {
		if (list[i + 1].pfn != (list[i].pfn + 1)) {
			end = list[i].pfn;
			printf("0x%08x-0x%08x\t%d\n",
					start<<12, end<<12, (end - start + 1)*4096);
			sum += (end - start + 1)*4096;
			start = list[i + 1].pfn;
		}
	}

	return sum;
}

int main(int argc, char **argv)
{
	FILE *fin;
	char *buf;
	int ret;
	struct stat st;

	if (argc < 3) {
		printf("Usage: ./program <input> <function>\n");
		perror("open: ");
		exit(1);
	}

	fin = fopen(argv[1], "r");

	fstat(fileno(fin), &st);
	max_size = st.st_size / 100;

	list = malloc(max_size * sizeof(*list));
	buf = malloc(BUF_SIZE);

	for ( ; ; ) {
		ret = read_block(buf, BUF_SIZE, fin);
		if (ret < 0)
			break;

		add_list(buf, ret, argv[2]);
	}

	printf("function [%s] is used in total %d byte\n",
			argv[2], check_contig_range());

	return 0;
}
