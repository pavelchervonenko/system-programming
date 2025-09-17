#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

typedef enum
{
    ACTION_XOR,
    ACTION_MASK,
    ACTION_COPY,
    ACTION_FIND,
    UNKNOWN
} Action;

typedef struct
{
    Action action;
    int xorN;
    const char *mask_hex;
    int copyN;
    const char *some_string;
    char *const *files;
    size_t file_count;
} Cmd;

int parse_int_suffix(const char *prefix, const char *last, int *n)
{
    size_t length = strlen(prefix);
    if (strncmp(last, prefix, length) != 0)
    {
        return 0;
    }
    if (last[length] == '\0')
    {
        return 0;
    }

    int value = 0;
    for (const char *p = length + last; *p != '\0'; ++p)
    {
        if (!isdigit(*p))
        {
            return 0;
        }
        value = value * 10 + (*p - '0');
        if (value > 100000000)
        {
            return 0;
        }
    }

    *n = value;
    return 1;
}

int parse_hex_u32(const char *s, uint32_t *out)
{
    if (!s || !*s)
    {
        return 0;
    }

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        s += 2;
    }

    int digits = 0;
    uint32_t res = 0;

    for (const char *p = s; *p != '\0'; ++p)
    {
        unsigned char c = (unsigned char)*p;
        uint32_t d;

        if (c >= '0' && c <= '9')
        {
            d = (uint32_t)(c - '0');
        }
        else if (c >= 'a' && c <= 'f')
        {
            d = (uint32_t)(c - 'a' + 10);
        }
        else if (c >= 'A' && c <= 'F')
        {
            d = (uint32_t)(c - 'A' + 10);
        }
        else
        {
            return 0;
        }

        if (digits >= 8)
        {
            return 0;
        }

        res = (res << 4) | d;
        digits++;
    }

    if (digits == 0)
    {
        return 0;
    }

    *out = res;
    return 1;
}

int parse_cmd(int argc, char **argv, Cmd *cmd)
{
    if (argc < 3)
    {
        return 0;
    }

    memset(cmd, 0, sizeof(*cmd));
    cmd->action = UNKNOWN;

    const char *last = argv[argc - 1];
    int n;

    if (parse_int_suffix("xor", last, &n))
    {
        if (n < 2 || n > 6)
        {
            fprintf(stderr, "'N' does not fall within the range");
            return 0;
        }

        // if (argc - 2 < 1)
        // {
        //     fprintf(stderr, "missing files\n");
        // }

        cmd->action = ACTION_XOR;
        cmd->xorN = n;
        cmd->files = argv + 1;
        cmd->file_count = (size_t)(argc - 2);
        return 1;
    }

    if (parse_int_suffix("copy", last, &n))
    {
        if (n <= 0)
        {
            fprintf(stderr, "error N in copyN\n");
            return 0;
        }

        // if (argc - 3 < 1)
        // {
        //     fprintf(stderr, "missing files\n");
        // }

        cmd->action = ACTION_COPY;
        cmd->copyN = n;
        cmd->files = argv + 1;
        cmd->file_count = (size_t)(argc - 2);
        return 1;
    }

    if (argc >= 4)
    {
        const char *prev = argv[argc - 2];
        if (strcmp(prev, "mask") == 0)
        {
            // if (argc - 3 < 1)
            // {
            //     fprintf(stderr, "missing files\n");
            // }

            cmd->action = ACTION_MASK;
            cmd->mask_hex = last;
            cmd->files = argv + 1;
            cmd->file_count = (size_t)(argc - 3);
            return 1;
        }

        if (strcmp(prev, "find") == 0)
        {
            cmd->action = ACTION_FIND;
            cmd->some_string = last;
            cmd->files = argv + 1;
            cmd->file_count = (size_t)(argc - 3);
            return 1;
        }
    }

    return 0;
}

int xor_stream_nibble(char *const *paths, size_t count, uint8_t *out_nib)
{
    if (!paths || count == 0 || !out_nib)
    {
        return 0;
    }

    uint8_t acc = 0;
    uint8_t chunk[4096];

    for (size_t i = 0; i < count; ++i)
    {
        const char *path = paths[i];
        FILE *file = fopen(path, "rb");
        if (!file)
        {
            fprintf(stderr, "Error opening file\n");
            return 0;
        }

        while (1)
        {
            size_t n = fread(chunk, 1, sizeof(chunk), file);
            if (n == 0)
            {
                if (ferror(file))
                {
                    fprintf(stderr, "Error read file\n");
                    fclose(file);
                    return 0;
                }
                break;
            }

            for (size_t j = 0; j < n; ++j)
            {
                uint8_t byte = chunk[j];
                acc ^= (uint8_t)((byte >> 4) & 0x0F);
                acc ^= (uint8_t)(byte & 0x0F);
            }
        }

        if (fclose(file) != 0)
        {
            fprintf(stderr, "Error close file\n");
        }
    }
    *out_nib = (uint8_t)(acc & 0x0F);

    return 1;
}

int xor_stream_blocks(char *const *paths, size_t count, size_t block_bytes, uint8_t *out_acc)
{
    if (!paths || count == 0 || block_bytes == 0 || !out_acc)
    {
        return 0;
    }

    memset(out_acc, 0, block_bytes);

    uint8_t *buf = (uint8_t *)malloc(block_bytes);
    if (!buf)
    {
        fprintf(stderr, "Memory allocation error\n");
        free(buf);
        return 0;
    }

    size_t filled = 0;
    uint8_t chunk[4096];

    for (size_t i = 0; i < count; i++)
    {
        const char *path = paths[i];
        FILE *file = fopen(path, "rb");
        if (!file)
        {
            fprintf(stderr, "Error opening file\n");
            free(buf);
            return 0;
        }

        while (1)
        {
            size_t n = fread(chunk, 1, sizeof(chunk), file);
            if (n == 0)
            {
                if (ferror(file))
                {
                    fprintf(stderr, "Error read file\n");
                    fclose(file);
                    free(buf);
                    return 0;
                }
                break;
            }

            size_t pos = 0;
            while (pos < n)
            {
                size_t need = block_bytes - filled;
                size_t take;
                if (n - pos < need)
                {
                    take = n - pos;
                }
                else
                {
                    take = need;
                }

                memcpy(buf + filled, chunk + pos, take);
                filled += take;
                pos += take;

                if (filled == block_bytes)
                {
                    for (size_t b = 0; b < block_bytes; ++b)
                    {
                        out_acc[b] ^= buf[b];

                        fprintf(stderr, "[blk] ");
                        for (size_t b = 0; b < block_bytes; ++b)
                            fprintf(stderr, "%02X", buf[b]);
                        fprintf(stderr, "  => acc=");
                        for (size_t b = 0; b < block_bytes; ++b)
                            fprintf(stderr, "%02X", out_acc[b]);
                        fprintf(stderr, "\n");
                    }
                    filled = 0;
                }
            }
        }

        if (fclose(file) != 0)
        {
            fprintf(stderr, "Error close file\n");
        }
    }

    if (filled > 0)
    {
        memset(buf + filled, 0, block_bytes - filled);
        for (size_t b = 0; b < block_bytes; ++b)
        {
            out_acc[b] ^= buf[b];

            fprintf(stderr, "[tail] ");
            for (size_t b = 0; b < block_bytes; ++b)
                fprintf(stderr, "%02X", buf[b]);
            fprintf(stderr, "  => acc=");
            for (size_t b = 0; b < block_bytes; ++b)
                fprintf(stderr, "%02X", out_acc[b]);
            fprintf(stderr, "\n");
        }
    }

    free(buf);

    return 1;
}

uint32_t be32_from4(const uint8_t w[4])
{
    return ((uint32_t)w[0] << 24) | ((uint32_t)w[1] << 16) | ((uint32_t)w[2] << 8) | (uint32_t)w[3];
}

int count_mask_matches_be32(char *const *paths, size_t count, uint32_t mask, uint64_t *out_count)
{
    if (!paths || count == 0 || !out_count)
    {
        return 0;
    }
    *out_count = 0;

    uint8_t win[4];
    size_t filled = 0;
    uint8_t chunk[4096];

    for (size_t i = 0; i < count; ++i)
    {
        const char *path = paths[i];
        FILE *file = fopen(path, "rb");
        if (!file)
        {
            fprintf(stderr, "Error opening file\n");
            return 0;
        }

        while (1)
        {
            size_t n = fread(chunk, 1, sizeof(chunk), file);
            if (n == 0)
            {
                if (ferror(file))
                {
                    fprintf(stderr, "read error\n");
                    return 0;
                }
                break;
            }

            size_t pos = 0;
            while (pos < n)
            {
                size_t need = 4 - filled;
                size_t take;
                if ((n - pos) < need)
                {
                    take = n - pos;
                }
                else
                {
                    take = need;
                }

                memcpy(win + filled, chunk + pos, take);
                filled += take;
                pos += take;

                if (filled == 4)
                {
                    uint32_t v = be32_from4(win);
                    if ((v & mask) == mask)
                    {
                        (*out_count)++;
                    }
                    filled = 0;
                }
            }
        }

        if (fclose(file) != 0)
        {
            fprintf(stderr, "Close failed\n");
        }
    }

    return 1;
}

int copy_file_streamed(const char *src, const char *dst)
{
    int ok = 1;

    FILE *input = fopen(src, "rb");
    if (!input)
    {
        fprintf(stderr, "error opening file\n");
        return 0;
    }

    FILE *output = fopen(dst, "wb");
    if (!output)
    {
        fprintf(stderr, "error opening file\n");
        fclose(input);
        return 0;
    }

    uint8_t *buf = (uint8_t *)malloc(65536);
    if (!buf)
    {
        fprintf(stderr, "memory allocation error\n");
        fclose(input);
        fclose(output);
        return 0;
    }

    while (1)
    {
        size_t r = fread(buf, 1, 65536, input);
        if (r == 0)
        {
            if (ferror(input))
            {
                fprintf(stderr, "read error input file\n");
                ok = 0;
            }
            break;
        }

        size_t off = 0;
        while (off < r)
        {
            size_t w = fwrite(buf + off, 1, r - off, output);
            if (w == 0)
            {
                fprintf(stderr, "write error output file\n");
                ok = 0;
                break;
            }
            off += w;
        }

        if (!ok)
        {
            break;
        }
    }

    free(buf);

    if (fclose(input) != 0)
    {
        fprintf(stderr, "close input file failed\n");
        ok = 0;
    }

    if (fclose(output) != 0)
    {
        fprintf(stderr, "close output file failed\n");
        ok = 0;
    }

    return ok;
}

int build_copy_path(const char *src, int index, char **out)
{
    if (!src || index <= 0 || !out)
    {
        return 0;
    }

    size_t slen = strlen(src);
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%d", index);

    if (n <= 0)
    {
        return 0;
    }

    size_t newlen = slen + (size_t)n;
    char *dst = (char *)malloc(newlen + 1);

    if (!dst)
    {
        return 0;
    }

    memcpy(dst, src, slen);
    memcpy(dst + slen, buf, (size_t)n);

    dst[newlen] = '\0';
    *out = dst;
    return 1;
}

int do_copyN(char *const *files, size_t file_count, int N)
{
    if (!files || file_count == 0 || N <= 0)
    {
        return 0;
    }

    int overall_ok = 1;

    for (size_t i = 0; i < file_count; ++i)
    {
        const char *src = files[i];

        for (int k = 1; k <= N; ++k)
        {
            pid_t pid = fork();

            if (pid < 0)
            {
                fprintf(stderr, "fork failed\n");
                overall_ok = 0;
                continue;
            }

            if (pid == 0)
            {
                char *dst = NULL;
                if (!build_copy_path(src, k, &dst))
                {
                    fprintf(stderr, "failed build path\n");
                    _exit(1);
                }
                int ok = copy_file_streamed(src, dst);
                free(dst);
                if (ok)
                {
                    _exit(0);
                }
                else
                {
                    _exit(1);
                }
            }

            int status = 0;
            if (waitpid(pid, &status, 0) < 0)
            {
                fprintf(stderr, "waitpid failed\n");
                overall_ok = 0;
            }
            else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            {
                fprintf(stderr, "copy file failed\n");
                overall_ok = 0;
            }
        }
    }

    return overall_ok;
}

int read_next_path_line(FILE *file, char **out_line)
{
    if (!file || !out_line)
    {
        return 0;
    }

    size_t cap = 128;
    size_t len = 0;

    char *buf = (char *)malloc(cap);
    if (!buf)
    {
        fprintf(stderr, "error allocation memory\n");
        return -1;
    }

    int got_any = 0;
    while (1)
    {
        int c = fgetc(file);
        if (c == EOF)
        {
            if (ferror(file))
            {
                free(buf);
                return -1;
            }
            break;
        }

        got_any = 1;

        if (len + 1 >= cap)
        {
            size_t ncap = cap * 2;
            char *nbuf = (char *)realloc(buf, ncap);
            if (!nbuf)
            {
                fprintf(stderr, "error realloc\n");
                free(buf);
                return -1;
            }

            buf = nbuf;
            cap = ncap;
        }

        buf[len++] = (char)c;
        if (c == '\n')
        {
            break;
        }
    }

    if (!got_any)
    {
        free(buf);
        return 0;
    }

    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r' || buf[len - 1] == ' ' || buf[len - 1] == '\t'))
    {
        len--;
    }

    buf[len] = '\0';

    if (len == 0)
    {
        free(buf);
        return 2;
    }

    *out_line = buf;
    return 1;
}

int file_contains_substring(const char *path, const char *needle)
{
    if (!path || !needle)
    {
        return 0;
    }

    size_t nlen = strlen(needle);
    if (nlen == 0)
    {
        return 1;
    }

    FILE *file = fopen(path, "rb");
    if (!file)
    {
        // fprintf(stderr, "error opening file\n");
        // return -1;
        return 0;
    }

    size_t overlap = 0;
    if (nlen > 1)
    {
        overlap = nlen - 1;
    }

    size_t buf_size = 65536;
    uint8_t *buf = (uint8_t *)malloc(buf_size + overlap);
    if (!buf)
    {
        fprintf(stderr, "memory allocation error\n");
        fclose(file);
        return -1;
    }

    size_t tail = 0; // уже лежат в начале buf
    int found = 0;
    int err = 0;

    while (1)
    {
        size_t r = fread(buf + tail, 1, buf_size, file);
        if (r == 0)
        {
            if (ferror(file))
            {
                fprintf(stderr, "read error\n");
                err = 1;
            }
            break;
        }

        size_t span = tail + r;
        for (size_t i = 0; i + nlen <= span; ++i)
        {
            if (memcmp(buf + i, needle, nlen) == 0)
            {
                found = 1;
                break;
            }
        }

        if (found)
        {
            break;
        }

        if (overlap > 0)
        {
            size_t copy_len;
            if (span < overlap)
            {
                copy_len = span;
            }
            else
            {
                copy_len = overlap;
            }

            if (copy_len > 0)
            {
                memmove(buf, buf + span - copy_len, copy_len);
            }

            tail = copy_len;
        }
        else
        {
            tail = 0;
        }
    }

    free(buf);
    if (fclose(file) != 0)
    {
        fprintf(stderr, "error close file\n");
    }
    if (err)
    {
        return -1;
    }
    if (found)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int process_find_list_file(const char *list_path, const char *needle)
{
    if (!list_path || !needle)
    {
        fprintf(stderr, "find: null args\n");
        return 0;
    }

    FILE *file = fopen(list_path, "rb");
    if (!file)
    {
        fprintf(stderr, "error opening file\n");
        return 0;
    }

    int any_found = 0;
    int overall_ok = 1;

    while (1)
    {
        char *line = NULL;
        int rc = read_next_path_line(file, &line);
        if (rc == 0)
        {
            break;
        }
        if (rc == -1)
        {
            overall_ok = 0;
            break;
        }
        if (rc == 2)
        {
            continue;
        }

        // fprintf(stderr, "[find] from '%s' read path: '%s'\n", list_path, line);

        pid_t pid = fork();
        if (pid < 0)
        {
            fprintf(stderr, "error fork()\n");
            overall_ok = 0;
            free(line);
            continue;
        }
        if (pid == 0)
        {
            int r = file_contains_substring(line, needle);
            if (r == 1)
            {
                _exit(0);
            }
            else if (r == 0)
            {
                _exit(1);
            }
            else
            {
                _exit(2);
            }
        }

        int status = 0;
        if (waitpid(pid, &status, 0) < 0)
        {
            fprintf(stderr, "error waitpid()\n");

            overall_ok = 0;
        }
        else
        {
            if (WIFEXITED(status))
            {
                int code = WEXITSTATUS(status);
                // fprintf(stderr, "[find] '%s' exit code = %d\n", line, code);
                if (code == 0)
                {
                    printf("%s\n", line);
                    any_found = 1;
                }
                else if (code == 2)
                {
                    overall_ok = 0;
                }
            }
            else
            {
                overall_ok = 0;
            }
        }
        free(line);
    }

    if (fclose(file) != 0)
    {
        fprintf(stderr, "error close file\n");
    }

    if (!any_found)
    {
        printf("*Stirng not found in files*\n");
    }

    return overall_ok;
}

void print_hex_upper(const uint8_t *buf, size_t len)
{
    static const char HEX[] = "0123456789ABCDEF";
    for (size_t i = 0; i < len; ++i)
    {
        unsigned b = buf[i];
        putchar(HEX[(b >> 4) & 0x0F]);
        putchar(HEX[b & 0x0F]);
    }
    putchar('\n');
}

int main(int argc, char *argv[])
{
    Cmd cmd;
    if (!parse_cmd(argc, argv, &cmd))
    {
        fprintf(stderr, "error when parsing command line arguments\n");
        return -1;
    }

    switch (cmd.action)
    {
    case ACTION_XOR:
    {
        if (cmd.xorN == 2)
        {
            uint8_t nib = 0;
            if (!xor_stream_nibble(cmd.files, cmd.file_count, &nib))
            {
                fprintf(stderr, "error in xor2\n");
                return -1;
            }

            const char hex[] = "0123456789ABCDEF";
            putchar(hex[nib & 0x0F]);
            putchar('\n');

            return 0;
        }
        else
        {
            size_t block_bytes = (size_t)1u << (cmd.xorN - 3);
            uint8_t *acc = (uint8_t *)malloc(block_bytes);
            if (!acc)
            {
                fprintf(stderr, "memory allocation error\n");
                return -1;
            }

            if (!xor_stream_blocks(cmd.files, cmd.file_count, block_bytes, acc))
            {
                free(acc);
                return -1;
            }

            print_hex_upper(acc, block_bytes);
            free(acc);

            return 0;
        }
        break;
    }

    case ACTION_MASK:
    {
        uint32_t mask = 0;
        if (!parse_hex_u32(cmd.mask_hex, &mask))
        {
            fprintf(stderr, "error: invalid <hex> mask\n");
            return -1;
        }

        uint64_t cnt = 0;
        if (!count_mask_matches_be32(cmd.files, cmd.file_count, mask, &cnt))
        {
            return -1;
        }
        printf("%" PRIu64 "\n", cnt);

        return 0;
    }

    case ACTION_COPY:
    {
        if (cmd.copyN > 10000)
        {
            fprintf(stderr, "error large N\n");
            return -1;
        }

        int status = do_copyN(cmd.files, cmd.file_count, cmd.copyN);
        if (!status)
        {
            return -1;
        }

        return 0;
    }

    case ACTION_FIND:
    {
        int overall_ok = 1;
        for (size_t i = 0; i < cmd.file_count; ++i)
        {
            if (!process_find_list_file(cmd.files[i], cmd.some_string))
            {
                overall_ok = 0;
                fprintf(stderr, "error in find <some-string>\n");
            }
        }

        if (overall_ok)
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }

    default:
        return -1;
    }
}