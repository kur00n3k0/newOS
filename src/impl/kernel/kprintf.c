#include "kprintf.h"
#include "print.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

static void put_padded(const char* digits, size_t len, size_t width, bool zero_pad) {
    for (size_t i = len; i < width; i++) {
        print_char(zero_pad ? '0' : ' ');
    }
    for (size_t i = 0; i < len; i++) {
        print_char(digits[i]);
    }
}

static void put_unsigned(uint64_t value, unsigned base, bool upper, size_t width, bool zero_pad) {
    const char* set = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[24];
    size_t len = 0;

    do {
        tmp[len++] = set[value % base];
        value /= base;
    } while (value);

    // digits were produced in reverse order
    char out[24];
    for (size_t i = 0; i < len; i++) {
        out[i] = tmp[len - 1 - i];
    }
    put_padded(out, len, width, zero_pad);
}

void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            print_char(*fmt);
            continue;
        }
        fmt++;

        bool zero_pad = false;
        bool left = false;
        size_t width = 0;
        int longs = 0;

        if (*fmt == '-') {
            left = true;
            fmt++;
        }
        if (*fmt == '0') {
            zero_pad = true;
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (size_t) (*fmt - '0');
            fmt++;
        }
        while (*fmt == 'l') {
            longs++;
            fmt++;
        }

        switch (*fmt) {
        case 'c':
            print_char((char) va_arg(args, int));
            break;
        case 's': {
            const char* s = va_arg(args, const char*);
            if (!s) {
                s = "(null)";
            }
            size_t len = 0;
            while (s[len]) {
                len++;
            }
            for (size_t i = len; !left && i < width; i++) {
                print_char(' ');
            }
            print_str((char*) s);
            for (size_t i = len; left && i < width; i++) {
                print_char(' ');
            }
            break;
        }
        case 'd':
        case 'i': {
            int64_t v = longs ? va_arg(args, int64_t) : va_arg(args, int);
            if (v < 0) {
                print_char('-');
                v = -v;
            }
            put_unsigned((uint64_t) v, 10, false, width, zero_pad);
            break;
        }
        case 'u':
            put_unsigned(longs ? va_arg(args, uint64_t) : va_arg(args, unsigned), 10, false, width, zero_pad);
            break;
        case 'x':
        case 'X':
            put_unsigned(longs ? va_arg(args, uint64_t) : va_arg(args, unsigned), 16, *fmt == 'X', width, zero_pad);
            break;
        case 'p':
            print_str("0x");
            put_unsigned((uint64_t) va_arg(args, void*), 16, false, 16, true);
            break;
        case '%':
            print_char('%');
            break;
        case '\0':
            va_end(args);
            return;
        default:
            print_char('%');
            print_char(*fmt);
            break;
        }
    }

    va_end(args);
}
