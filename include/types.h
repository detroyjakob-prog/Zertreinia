/* Common fixed-width types and helpers.
 *
 * In a freestanding GCC environment these headers are provided by the
 * compiler itself (not the C library), so they are always safe to use. */
#ifndef ZERT_TYPES_H
#define ZERT_TYPES_H

#include <stdint.h>   /* uint8_t, uint16_t, uint32_t, uint64_t, ...        */
#include <stddef.h>   /* size_t, NULL                                      */
#include <stdbool.h>  /* bool, true, false                                 */
#include <stdarg.h>   /* va_list, va_start, va_arg, va_end                 */

/* A few convenience aliases used throughout the kernel. */
typedef uint32_t physaddr_t;   /* physical address     */
typedef uint32_t virtaddr_t;   /* virtual address      */

#define KERNEL_NAME    "Zertreinia OS"
#define KERNEL_VERSION "0.1.0"

#define UNUSED(x) ((void)(x))

#endif /* ZERT_TYPES_H */
