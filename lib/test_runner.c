/* SPDX-License-Identifier: Apache-2.0 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include "test_runner.h"

static int compare_tests(const void *a, const void *b)
{
    const struct vv_test_entry *const *left = a, *const *right = b;
    return strcmp((*left)->name, (*right)->name);
}

void vv_list_tests(const struct vv_test_suite *suite)
{
    for (const struct vv_test_entry *t = suite->begin; t < suite->end; ++t) {
        const char *version = t->spec_version ? t->spec_version : "";
        const char *section = t->spec_section ? t->spec_section : "";
        printf("%s\t%s\t%s\t%s\n", t->name, t->desc, version, section);
    }
}

int vv_run_suite(const struct vv_test_suite *suite, const char *selection,
                 const char *run_id, int infrastructure_ready)
{
    size_t count = suite->end - suite->begin;
    const struct vv_test_entry **tests = NULL;
    unsigned char *enabled = NULL;
    char *names = NULL;
    unsigned passed = 0, failed = 0, skipped = 0, infra = 0;
    int status = 2;
    const char *prefix = suite->protocol_prefix;
    printf("%s-BEGIN %s %s\n", prefix, run_id, suite->contract);
    if (!count || count > 4096) {
        printf("vv-infra invalid registry size: %zu\n", count);
        infra = 1;
        goto summary;
    }
    tests = calloc(count, sizeof(*tests));
    enabled = calloc(count, 1);
    if (!tests || !enabled) {
        printf("vv-infra test registry allocation failed\n");
        infra = 1;
        goto summary;
    }
    for (size_t i = 0; i < count; ++i)
        tests[i] = &suite->begin[i];
    qsort(tests, count, sizeof(*tests), compare_tests);
    for (size_t i = 1; i < count; ++i) {
        if (!strcmp(tests[i - 1]->name, tests[i]->name)) {
            printf("vv-infra duplicate registered test: %s\n", tests[i]->name);
            infra = 1;
            goto summary;
        }
    }
    if (selection) {
        size_t length = strlen(selection);
        if (!length || selection[0] == ',' || selection[length - 1] == ',' ||
            strstr(selection, ",,")) {
            printf("vv-infra malformed empty test selection\n");
            infra = 1;
            goto summary;
        }
        names = strdup(selection);
        if (!names) {
            printf("vv-infra selection allocation failed\n");
            infra = 1;
            goto summary;
        }
        char *save;
        for (char *name = strtok_r(names, ",", &save); name;
             name = strtok_r(NULL, ",", &save)) {
            size_t i;
            for (i = 0; i < count; ++i)
                if (!strcmp(tests[i]->name, name))
                    break;
            if (i == count || enabled[i]) {
                printf("vv-infra unknown or duplicate test selection: %s\n", name);
                infra = 1;
                goto summary;
            }
            enabled[i] = 1;
        }
    } else {
        memset(enabled, 1, count);
    }
    if (!infrastructure_ready || (suite->preflight && suite->preflight() != 0)) {
        printf("vv-infra suite prerequisites failed; no assertions executed\n");
        infra = 1;
        goto summary;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!enabled[i])
            continue;
        void *context = calloc(1, suite->context_size ? suite->context_size : 1);
        if (!context) {
            printf("vv-infra context allocation failed for %s\n", tests[i]->name);
            infra = 1;
            break;
        }
        if (suite->init_context)
            suite->init_context(context);
        printf("%s-RUN %s %s\n", prefix, run_id, tests[i]->name);
        if (tests[i]->spec_version && *tests[i]->spec_version &&
            tests[i]->spec_section && *tests[i]->spec_section)
            printf("vv-spec %s %s %s\n", tests[i]->name,
                   tests[i]->spec_version, tests[i]->spec_section);
        test_result_t result = tests[i]->fn(context);
        if (suite->cleanup && suite->cleanup(context) != TEST_PASS)
            result = TEST_FAIL;
        free(context);
        const char *verdict = "FAIL";
        if (result == TEST_PASS) {
            verdict = "PASS";
            ++passed;
        } else if (result == TEST_SKIP) {
            verdict = "SKIP";
            ++skipped;
        } else {
            ++failed;
        }
        printf("%s-RESULT %s %s %s\n", prefix, run_id, tests[i]->name, verdict);
    }
    if (!passed && !failed) {
        printf("vv-infra no assertions executed\n");
        infra = 1;
    }
    status = infra ? 2 : failed ? 1 : 0;
summary:
    printf("%s-SUMMARY %s %u %u %u %u\n", prefix, run_id, passed, failed, skipped, infra);
    printf("%s-END %s %d\n", prefix, run_id, status);
    free(names);
    free(enabled);
    free(tests);
    return status;
}
