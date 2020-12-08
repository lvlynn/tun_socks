
#include <nt_core.h>


ssize_t
nt_parse_size(nt_str_t *line)
{
    u_char   unit;
    size_t   len;
    ssize_t  size, scale, max;

    len = line->len;

    if (len == 0) {
        return NT_ERROR;
    }

    unit = line->data[len - 1];

    switch (unit) {
    case 'K':
    case 'k':
        len--;
        max = NT_MAX_SIZE_T_VALUE / 1024;
        scale = 1024;
        break;

    case 'M':
    case 'm':
        len--;
        max = NT_MAX_SIZE_T_VALUE / (1024 * 1024);
        scale = 1024 * 1024;
        break;

#if (T_NT_HTTP_SYSGUARD)
    case 'G':
    case 'g':
        len--;
        max = NT_MAX_SIZE_T_VALUE / (1024 * 1024 * 1024);
        scale = 1024 * 1024 * 1024;
        break;
#endif

    default:
        max = NT_MAX_SIZE_T_VALUE;
        scale = 1;
    }

    size = nt_atosz(line->data, len);
    if (size == NT_ERROR || size > max) {
        return NT_ERROR;
    }

    size *= scale;

    return size;
}


off_t
nt_parse_offset(nt_str_t *line)
{
    u_char  unit;
    off_t   offset, scale, max;
    size_t  len;

    len = line->len;

    if (len == 0) {
        return NT_ERROR;
    }

    unit = line->data[len - 1];

    switch (unit) {
    case 'K':
    case 'k':
        len--;
        max = NT_MAX_OFF_T_VALUE / 1024;
        scale = 1024;
        break;

    case 'M':
    case 'm':
        len--;
        max = NT_MAX_OFF_T_VALUE / (1024 * 1024);
        scale = 1024 * 1024;
        break;

    case 'G':
    case 'g':
        len--;
        max = NT_MAX_OFF_T_VALUE / (1024 * 1024 * 1024);
        scale = 1024 * 1024 * 1024;
        break;

    default:
        max = NT_MAX_OFF_T_VALUE;
        scale = 1;
    }

    offset = nt_atoof(line->data, len);
    if (offset == NT_ERROR || offset > max) {
        return NT_ERROR;
    }

    offset *= scale;

    return offset;
}


nt_int_t
nt_parse_time(nt_str_t *line, nt_uint_t is_sec)
{
    u_char      *p, *last;
    nt_int_t    value, total, scale;
    nt_int_t    max, cutoff, cutlim;
    nt_uint_t   valid;
    enum {
        st_start = 0,
        st_year,
        st_month,
        st_week,
        st_day,
        st_hour,
        st_min,
        st_sec,
        st_msec,
        st_last
    } step;

    valid = 0;
    value = 0;
    total = 0;
    cutoff = NT_MAX_INT_T_VALUE / 10;
    cutlim = NT_MAX_INT_T_VALUE % 10;
    step = is_sec ? st_start : st_month;

    p = line->data;
    last = p + line->len;

    while (p < last) {

        if (*p >= '0' && *p <= '9') {
            if (value >= cutoff && (value > cutoff || *p - '0' > cutlim)) {
                return NT_ERROR;
            }

            value = value * 10 + (*p++ - '0');
            valid = 1;
            continue;
        }

        switch (*p++) {

        case 'y':
            if (step > st_start) {
                return NT_ERROR;
            }
            step = st_year;
            max = NT_MAX_INT_T_VALUE / (60 * 60 * 24 * 365);
            scale = 60 * 60 * 24 * 365;
            break;

        case 'M':
            if (step >= st_month) {
                return NT_ERROR;
            }
            step = st_month;
            max = NT_MAX_INT_T_VALUE / (60 * 60 * 24 * 30);
            scale = 60 * 60 * 24 * 30;
            break;

        case 'w':
            if (step >= st_week) {
                return NT_ERROR;
            }
            step = st_week;
            max = NT_MAX_INT_T_VALUE / (60 * 60 * 24 * 7);
            scale = 60 * 60 * 24 * 7;
            break;

        case 'd':
            if (step >= st_day) {
                return NT_ERROR;
            }
            step = st_day;
            max = NT_MAX_INT_T_VALUE / (60 * 60 * 24);
            scale = 60 * 60 * 24;
            break;

        case 'h':
            if (step >= st_hour) {
                return NT_ERROR;
            }
            step = st_hour;
            max = NT_MAX_INT_T_VALUE / (60 * 60);
            scale = 60 * 60;
            break;

        case 'm':
            if (p < last && *p == 's') {
                if (is_sec || step >= st_msec) {
                    return NT_ERROR;
                }
                p++;
                step = st_msec;
                max = NT_MAX_INT_T_VALUE;
                scale = 1;
                break;
            }

            if (step >= st_min) {
                return NT_ERROR;
            }
            step = st_min;
            max = NT_MAX_INT_T_VALUE / 60;
            scale = 60;
            break;

        case 's':
            if (step >= st_sec) {
                return NT_ERROR;
            }
            step = st_sec;
            max = NT_MAX_INT_T_VALUE;
            scale = 1;
            break;

        case ' ':
            if (step >= st_sec) {
                return NT_ERROR;
            }
            step = st_last;
            max = NT_MAX_INT_T_VALUE;
            scale = 1;
            break;

        default:
            return NT_ERROR;
        }

        if (step != st_msec && !is_sec) {
            scale *= 1000;
            max /= 1000;
        }

        if (value > max) {
            return NT_ERROR;
        }

        value *= scale;

        if (total > NT_MAX_INT_T_VALUE - value) {
            return NT_ERROR;
        }

        total += value;

        value = 0;

        while (p < last && *p == ' ') {
            p++;
        }
    }

    if (!valid) {
        return NT_ERROR;
    }

    if (!is_sec) {
        if (value > NT_MAX_INT_T_VALUE / 1000) {
            return NT_ERROR;
        }

        value *= 1000;
    }

    if (total > NT_MAX_INT_T_VALUE - value) {
        return NT_ERROR;
    }

    return total + value;
}
