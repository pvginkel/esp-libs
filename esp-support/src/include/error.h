#pragma once

#include "esp_err.h"
#include "esp_log.h"

// Returns error with stringified expression if the call fails.
#if defined(CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_SILENT)
#define ESP_ERROR_RETURN(x)                \
    do {                                   \
        esp_err_t err_rc_ = (x);           \
        if (unlikely(err_rc_ != ESP_OK)) { \
            return err_rc_;                \
        }                                  \
    } while (0)
#else
#define ESP_ERROR_RETURN(x)                                               \
    do {                                                                  \
        esp_err_t err_rc_ = (x);                                          \
        if (unlikely(err_rc_ != ESP_OK)) {                                \
            ESP_LOGE(TAG, "%s failed: %s", #x, esp_err_to_name(err_rc_)); \
            return err_rc_;                                               \
        }                                                                 \
    } while (0)
#endif

// Returns specified error if condition is false.
#if defined(CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_SILENT)
#define ESP_ASSERT_RETURN(cond, err) \
    do {                             \
        if (unlikely(!(cond))) {     \
            return (err);            \
        }                            \
    } while (0)
#else
#define ESP_ASSERT_RETURN(cond, err)                      \
    do {                                                  \
        if (unlikely(!(cond))) {                          \
            ESP_LOGE(TAG, "Assertion failed: %s", #cond); \
            return (err);                                 \
        }                                                 \
    } while (0)
#endif

// Asserts a condition, aborts if false.
#if defined(CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_SILENT)
#define ESP_ASSERT_CHECK(x)   \
    do {                      \
        if (unlikely(!(x))) { \
            abort();          \
        }                     \
    } while (0)
#else
#define ESP_ASSERT_CHECK(x)                                                                                    \
    do {                                                                                                       \
        if (unlikely(!(x))) {                                                                                  \
            printf("ESP_ASSERT_CHECK failed");                                                                 \
            printf(" at %p\n", __builtin_return_address(0));                                                   \
            printf("file: \"%s\" line %d\nfunc: %s\nexpression: %s\n", __FILE__, __LINE__, __ASSERT_FUNC, #x); \
            abort();                                                                                           \
        }                                                                                                      \
    } while (0)
#endif
