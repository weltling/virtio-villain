/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <string.h>
#include "lib/test_runner.h"

struct context {
    int resource;
};

static const char *mode;
static unsigned acquired, released;

static void init_context(void *pointer)
{
    struct context *context = pointer;
    context->resource = 42;
    ++acquired;
}

static test_result_t cleanup(void *pointer)
{
    struct context *context = pointer;
    assert(context->resource == 42);
    context->resource = 0;
    ++released;
    if (!strcmp(mode, "cleanup"))
        TFAIL("injected cleanup failure");
    return TEST_PASS;
}

static test_result_t first(struct context *context)
{
    assert(context->resource == 42);
    if (!strcmp(mode, "fail"))
        TFAIL("injected assertion failure");
    if (!strcmp(mode, "skip"))
        TSKIP("optional capability unavailable");
    return TEST_PASS;
}

static test_result_t second(struct context *context)
{
    assert(context->resource == 42);
    if (!strcmp(mode, "skip"))
        TSKIP("optional capability unavailable");
    return TEST_PASS;
}

VV_REGISTER_TEST_SPEC(example_registry, EX001, first, "first example test", "Example 1.0", "2.3");
VV_REGISTER_TEST(example_registry, EX002, second, "second example test");

extern const struct vv_test_entry __start_example_registry[];
extern const struct vv_test_entry __stop_example_registry[];

static int preflight(void)
{
    return !strcmp(mode, "infra") ? -1 : 0;
}

int main(int argc, char **argv)
{
    const struct vv_test_suite suite = {
        .contract = "example-v1", .protocol_prefix = "VV",
        .begin = __start_example_registry, .end = __stop_example_registry,
        .context_size = sizeof(struct context), .preflight = preflight,
        .init_context = init_context, .cleanup = cleanup,
    };
    if (argc == 2 && !strcmp(argv[1], "--list")) {
        vv_list_tests(&suite);
        return 0;
    }
    int offset = argc > 1 && !strcmp(argv[1], "--launcher") ? 2 : 1;
    mode = argc > offset ? argv[offset] : "pass";
    const char *selection = argc > offset + 1 ? argv[offset + 1] : NULL;
    const char *token = argc > offset + 2 ? argv[offset + 2] : "token";
    int result = vv_run_suite(&suite, selection, token, 1);
    assert(acquired == released);
    return offset == 2 ? 0 : result;
}
