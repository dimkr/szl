#ifndef _SZL_TEST_H_INCLUDED
#	define _SZL_TEST_H_INCLUDED

#	include <assert.h>

#	define szl_test_begin() do {} while (0)

void szl_test_set_name(const char *new_name);

#	define szl_assert(exp) szl_assert_internal(exp, #exp, __FILE__, __LINE__)
void szl_assert_internal(const int res,
                         const char *exp,
                         const char *file,
                         const int line);

void szl_test_end(void);

#endif
