/*
    gcc -o demo test.c ../18/security.c -I../../include
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "test_security.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"

static int g_total_tests = 0;
static int g_passed_tests = 0;
static int g_failed_tests = 0;

typedef void (*test_func_t)(void);

typedef struct test_case {
    const char *name;
    test_func_t func;
    struct test_case *next;
} test_case_t;

static test_case_t *g_test_head = NULL;
static test_case_t *g_test_tail = NULL;

#define TEST_CASE(test_name) \
    static void test_##test_name(void); \
    static void __attribute__((constructor)) register_##test_name(void) { \
        test_case_t *tc = malloc(sizeof(test_case_t)); \
        tc->name = #test_name; \
        tc->func = test_##test_name; \
        tc->next = NULL; \
        if (g_test_head == NULL) { \
            g_test_head = tc; \
            g_test_tail = tc; \
        } else { \
            g_test_tail->next = tc; \
            g_test_tail = tc; \
        } \
    } \
    static void test_##test_name(void)


#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "  " COLOR_RED "✗ ASSERT_TRUE failed" COLOR_RESET " at %s:%d\n", __FILE__, __LINE__); \
            fprintf(stderr, "    Expression: %s\n", #expr); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_FALSE(expr) \
    do { \
        if (expr) { \
            fprintf(stderr, "  " COLOR_RED "✗ ASSERT_FALSE failed" COLOR_RESET " at %s:%d\n", __FILE__, __LINE__); \
            fprintf(stderr, "    Expression: %s\n", #expr); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            fprintf(stderr, "  " COLOR_RED "✗ ASSERT_EQ failed" COLOR_RESET " at %s:%d\n", __FILE__, __LINE__); \
            fprintf(stderr, "    Expected: %d\n", (int)(b)); \
            fprintf(stderr, "    Got:      %d\n", (int)(a)); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            fprintf(stderr, "  " COLOR_RED "✗ ASSERT_NE failed" COLOR_RESET " at %s:%d\n", __FILE__, __LINE__); \
            fprintf(stderr, "    Should not equal: %d\n", (int)(b)); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_STR_EQ(a, b) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            fprintf(stderr, "  " COLOR_RED "✗ ASSERT_STR_EQ failed" COLOR_RESET " at %s:%d\n", __FILE__, __LINE__); \
            fprintf(stderr, "    Expected: \"%s\"\n", (b)); \
            fprintf(stderr, "    Got:      \"%s\"\n", (a)); \
            exit(1); \
        } \
    } while(0)

int sanitize_path(const char *input, char *output, size_t outlen);
char *scanner(const char *filename, int *status_code);

TEST_CASE(sanitize_path_normal) {
    char output[256];
    int ret = sanitize_path("index.html", output, sizeof(output));
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(output, "index.html");
}

TEST_CASE(sanitize_path_duplicate_slashes) {
    char output[256];
    int ret = sanitize_path("a//b///c", output, sizeof(output));
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(output, "a/b/c");
}

TEST_CASE(sanitize_path_traversal_attack) {
    char output[256];
    int ret = sanitize_path("../etc/passwd", output, sizeof(output));
    ASSERT_EQ(ret, -1);
}

TEST_CASE(sanitize_path_dot_dot_in_middle) {
    char output[256];
    int ret = sanitize_path("a/../b", output, sizeof(output));
    ASSERT_EQ(ret, -1);
}

TEST_CASE(sanitize_path_url_encoded_dot) {
    char output[256];
    int ret = sanitize_path("test%2e%2e/etc", output, sizeof(output));
    ASSERT_EQ(ret, -1);
}

TEST_CASE(sanitize_path_null_byte_injection) {
    char output[256];
    int ret = sanitize_path("test%00/etc", output, sizeof(output));
    ASSERT_EQ(ret, -1);
}

TEST_CASE(sanitize_path_empty_input) {
    char output[256];
    int ret = sanitize_path("", output, sizeof(output));
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(output, "");
}

TEST_CASE(scanner_nonexistent_file) {
    int status_code = 0;
    char *result = scanner("nonexistent_file_12345.html", &status_code);
    
    ASSERT_NE(status_code, 200);
    ASSERT_TRUE(result != NULL);
    
    if (result) free(result);
}

TEST_CASE(scanner_path_traversal) {
    int status_code = 0;
    char *result = scanner("../../etc/passwd", &status_code);
    
    ASSERT_NE(status_code, 200);
    ASSERT_TRUE(result != NULL);
    
    if (result) free(result);
}

TEST_CASE(scanner_null_byte_attack) {
    int status_code = 0;
    char input[256];
    snprintf(input, sizeof(input), "test%cmalicious", '\0');
    
    char *result = scanner(input, &status_code);
    ASSERT_NE(status_code, 200);
    
    if (result) free(result);
}

TEST_CASE(scanner_valid_404_path) {
    int status_code = 0;
    char *result = scanner("404.html", &status_code);
    
    ASSERT_TRUE(result != NULL);
    ASSERT_TRUE(status_code == 200 || status_code == 404);
    
    if (result) free(result);
}

static void run_test_isolated(test_case_t *tc) {
    pid_t pid = fork();
    
    if (pid < 0) {
        fprintf(stderr, COLOR_RED "✗ Fork failed for test: %s" COLOR_RESET "\n", tc->name);
        g_failed_tests++;
        return;
    }
    
    if (pid == 0) {
        tc->func();
    } else {
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf(COLOR_GREEN "✓" COLOR_RESET " %s\n", tc->name);
            g_passed_tests++;
        } else {
            printf(COLOR_RED "✗" COLOR_RESET " %s " COLOR_RED "(FAILED)" COLOR_RESET "\n", tc->name);
            g_failed_tests++;
        }
    }
}

int main(void) {
    printf("\n" COLOR_CYAN "=== ArcCore Security Test Suite ===" COLOR_RESET "\n\n");
    
    test_case_t *tc = g_test_head;
    while (tc != NULL) {
        g_total_tests++;
        run_test_isolated(tc);
        tc = tc->next;
    }
    
    printf("\n" COLOR_CYAN "==================================" COLOR_RESET "\n");
    printf("Total:  %d\n", g_total_tests);
    printf(COLOR_GREEN "Passed: %d" COLOR_RESET "\n", g_passed_tests);
    
    if (g_failed_tests > 0) {
        printf(COLOR_RED "Failed: %d" COLOR_RESET "\n", g_failed_tests);
    } else {
        printf(COLOR_GREEN "Failed: 0" COLOR_RESET "\n");
    }
    
    printf(COLOR_CYAN "==================================" COLOR_RESET "\n\n");
    
    return (g_failed_tests == 0) ? 0 : 1;
}