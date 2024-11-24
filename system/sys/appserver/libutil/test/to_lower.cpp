#include <util/string.h>
#include <assert.h>

#define clean_errno() \
    (errno == 0 ? "None" : strerror(errno))

#define log_error(M, ...) \
    fprintf(stderr, "[ERROR] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)

#define assertf(A, ERROR, SUCCESS, ...) \
    if(!(A)) { \
        log_error(ERROR, ##__VA_ARGS__); \
        assert(A); \
    }                             \
    else{                         \
        fprintf(stdout,"%s\n",SUCCESS);  \
    }

int main() {
	os::String c("TEST");
	assertf(c.Lower() == "test", "Cannot lower case", "Lower case was successful");
	return 0;
}