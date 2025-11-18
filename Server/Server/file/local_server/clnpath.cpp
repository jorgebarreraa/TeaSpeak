//
// Created by WolverinDEV on 29/04/2020.
//

#include "clnpath.h"
#include <cstring>

#define MAX_PATH_ELEMENTS   128  /* Number of levels of directory */
void ts_clnpath(char *path)
{
    char           *src;
    char           *dst;
    char            c;
    int             slash = 0;

    /* Convert multiple adjacent slashes to single slash */
    src = dst = path;
    while ((c = *dst++ = *src++) != '\0')
    {
        if (c == '/')
        {
            slash = 1;
            while (*src == '/')
                src++;
        }
    }

    if (slash == 0)
        return;

    /* Remove "./" from "./xxx" but leave "./" alone. */
    /* Remove "/." from "xxx/." but reduce "/." to "/". */
    /* Reduce "xxx/./yyy" to "xxx/yyy" */
    src = dst = (*path == '/') ? path + 1 : path;
    while (src[0] == '.' && src[1] == '/' && src[2] != '\0')
        src += 2;
    while ((c = *dst++ = *src++) != '\0')
    {
        if (c == '/' && src[0] == '.' && (src[1] == '\0' || src[1] == '/'))
        {
            src++;
            dst--;
        }
    }
    if (path[0] == '/' && path[1] == '.' &&
        (path[2] == '\0' || (path[2] == '/' && path[3] == '\0')))
        path[1] = '\0';

    /* Remove trailing slash, if any.  There is at most one! */
    /* dst is pointing one beyond terminating null */
    if ((dst -= 2) > path && *dst == '/')
        *dst++ = '\0';
}

bool ts_strequal(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

int ts_tokenise(char* ostring, const char* del, char** result, int max_tokens) {
    int num_tokens{0};

    char* token, *string, *tofree;
    tofree = string = strdup(ostring);
    while ((token = strsep(&string, del)) != nullptr) {
        result[num_tokens++] = strdup(token);
        if(num_tokens > max_tokens)
            break;
    }

    free(tofree);
    return num_tokens;
}

/*
** clnpath2() is not part of the basic clnpath() function because it can
** change the meaning of a path name if there are symbolic links on the
** system.  For example, suppose /usr/tmp is a symbolic link to /var/tmp.
** If the user supplies /usr/tmp/../abcdef as the directory name, clnpath
** would transform that to /usr/abcdef, not to /var/abcdef which is what
** the kernel would interpret it as.
*/
void ts_clnpath2(char *path)
{
    char *token[MAX_PATH_ELEMENTS], *otoken[MAX_PATH_ELEMENTS];
    int   ntok, ontok;

    ts_clnpath(path);

    /* Reduce "<name>/.." to "/" */
    ntok = ontok = ts_tokenise(path, "/", otoken, MAX_PATH_ELEMENTS);
    memcpy(token, otoken, sizeof(char*) * ntok);

    if (ntok > 1) {
        for (int i = 0; i < ntok - 1; i++)
        {
            if (!ts_strequal(token[i], "..") && ts_strequal(token[i + 1], ".."))
            {
                if (*token[i] == '\0')
                    continue;
                while (i < ntok - 1)
                {
                    token[i] = token[i + 2];
                    i++;
                }
                ntok -= 2;
                i = -1;     /* Restart enclosing for loop */
            }
        }
    }

    /* Reassemble string */
    char *dst = path;
    if (ntok == 0)
    {
        *dst++ = '.';
        *dst = '\0';
    }
    else
    {
        if (token[0][0] == '\0')
        {
            int   i;
            for (i = 1; i < ntok && ts_strequal(token[i], ".."); i++)
                ;
            if (i > 1)
            {
                int j;
                for (j = 1; i < ntok; i++)
                    token[j++] = token[i];
                ntok = j;
            }
        }
        if (ntok == 1 && token[0][0] == '\0')
        {
            *dst++ = '/';
            *dst = '\0';
        }
        else
        {
            for (int i = 0; i < ntok; i++)
            {
                char *src = token[i];
                while ((*dst++ = *src++) != '\0')
                    ;
                *(dst - 1) = '/';
            }
            *(dst - 1) = '\0';
        }
    }

    for(int i{0}; i < ontok; i++)
        ::free(otoken[i]);
}

std::string clnpath(const std::string_view& data) {
    std::string result{data};
    ts_clnpath2(result.data());
    auto index = result.find((char) 0);
    if(index != std::string::npos)
        result.resize(index);
    return result;
}

#ifdef CLN_EXEC
typedef struct p1_test_case
{
    const char *input;
    const char *output;
} p1_test_case;

/* This stress tests the cleaning, concentrating on the boundaries. */
static const p1_test_case p1_tests[] =
        {
                { "/",                                  "/",            },
                { "//",                                 "/",            },
                { "///",                                "/",            },
                { "/.",                                 "/",            },
                { "/./",                                "/",            },
                { "/./.",                               "/",            },
                { "/././.profile",                      "/.profile",    },
                { "./",                                 ".",            },
                { "./.",                                ".",            },
                { "././",                               ".",            },
                { "./././.profile",                     ".profile",     },
                { "abc/.",                              "abc",          },
                { "abc/./def",                          "abc/def",      },
                { "./abc",                              "abc",          },

                { "//abcd///./abcd////",                "/abcd/abcd",                   },
                { "//abcd///././../defg///ddd//.",      "/abcd/../defg/ddd",            },
                { "/abcd/./../././defg/./././ddd",      "/abcd/../defg/ddd",            },
                { "//abcd//././../defg///ddd//.///",    "/abcd/../defg/ddd",            },

                /* Most of these are minimal interest in phase 1 */
                { "/usr/tmp/clnpath.c",                 "/usr/tmp/clnpath.c",           },
                { "/usr/tmp/",                          "/usr/tmp",                     },
                { "/bin/..",                            "/bin/..",                      },
                { "bin/..",                             "bin/..",                       },
                { "/bin/.",                             "/bin",                         },
                { "sub/directory",                      "sub/directory",                },
                { "sub/directory/file",                 "sub/directory/file",           },
                { "/part1/part2/../.././../",           "/part1/part2/../../..",        },
                { "/.././../usr//.//bin/./cc",          "/../../usr/bin/cc",            },
        };

static void p1_tester(const void *data)
{
    const p1_test_case *test = (const p1_test_case *)data;
    char  buffer[256];

    strcpy(buffer, test->input);
    ts_clnpath(buffer);
    if (strcmp(buffer, test->output) == 0)
        printf("<<%s>> cleans to <<%s>>\n", test->input, buffer);
    else
    {
        fprintf(stderr, "<<%s>> - unexpected output from clnpath()\n", test->input);
        fprintf(stderr, "Wanted <<%s>>\n", test->output);
        fprintf(stderr, "Actual <<%s>>\n", buffer);
    }
}

typedef struct p2_test_case
{
    const char *input;
    const char *output;
} p2_test_case;

static const p2_test_case p2_tests[] =
{
    { "/abcd/../defg/ddd",              "/defg/ddd"         },
    { "/bin/..",                        "/"                 },
    { "bin/..",                         "."                 },
    { "/usr/bin/..",                    "/usr"              },
    { "/usr/bin/../..",                 "/"                 },
    { "usr/bin/../..",                  "."                 },
    { "../part/of/../the/way",          "../part/the/way"   },
    { "/../part/of/../the/way",         "/part/the/way"     },
    { "part1/part2/../../part3",        "part3"             },
    { "part1/part2/../../../part3",     "../part3"          },
    { "/part1/part2/../../../part3",    "/part3"            },
    { "/part1/part2/../../../",         "/"                 },
    { "/../../usr/bin/cc",              "/usr/bin/cc"       },
    { "../../usr/bin/cc",               "../../usr/bin/cc"  },
    { "part1/./part2/../../part3",      "part3"             },
    { "./part1/part2/../../../part3",   "../part3"          },
    { "/part1/part2/.././../../part3",  "/part3"            },
    { "/part1/part2/../.././../",       "/"                 },
    { "/.././..//./usr///bin/cc/",      "/usr/bin/cc"       },
    {nullptr, nullptr}
};

static void p2_tester(const void *data)
{
    auto test = (const p2_test_case *)data;
    char  buffer[256];

    strcpy(buffer, test->input);
    ts_clnpath2(buffer);
    if (strcmp(buffer, test->output) == 0)
        printf("<<%s>> cleans to <<%s>>\n", test->input, buffer);
    else
    {
        fprintf(stderr, "<<%s>> - unexpected output from clnpath2()\n", test->input);
        fprintf(stderr, "Wanted <<%s>>\n", test->output);
        fprintf(stderr, "Actual <<%s>>\n", buffer);
    }
}

int main() {
    for(const auto& test : p1_tests) {
        if(!test.input) break;
        p1_tester(&test);
    }

    printf("------------------------------\n");
    for(const auto& test : p2_tests) {
        if(!test.input) break;
        p2_tester(&test);
    }
}

#endif