#include <stdlib.h>
#include <stdio.h>

static int all_ok = 1;
static const char *name;

void szl_test_set_name(const char *new_name)
{
	name = new_name;
}

void szl_assert_internal(const int res,
                         const char *exp,
                         const char *file,
                         const int line)
{
	if (!res) {
		fprintf(stderr,
		        "test failed (%s, at %s:%d): %s\n",
		        name,
		        file,
		        line,
		        exp);
		all_ok = 0;
	} else
		putchar('.');
}

void szl_test_end(void)
{
	putchar('\n');
	if (!all_ok)
		exit(EXIT_FAILURE);
}
