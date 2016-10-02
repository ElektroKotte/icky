#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>

#include <curl/curl.h>

#define DEFAULT_CFG_DIR "~/.icky"
#define DEFAULT_CFG_FILE DEFAULT_CFG_DIR "/cfg"
#define DEFAULT_CA_FILE DEFAULT_CFG_DIR "/ca.crt"
#define DEFAULT_CLIENT_FILE DEFAULT_CFG_DIR "/client.crt"

typedef struct {
    char *ca_file;
    char *client_file;
} cfg_t;

static void push(const char *data, const cfg_t *cfg)
{
    CURL *curl;

    curl = curl_easy_init();
    if (!curl) goto cleanup;

    curl_easy_setopt(curl, CURLOPT_URL, "https://" "localhost:8080/sticky");
}

static size_t get(char **data, const cfg_t *cfg)
{
}

static read_stream(char **buffer, FILE *stream)
{
    // TODO while not end of file, read block
}

static void cfg_setup(cfg_t *cfg)
{
    cfg->client_file = DEFAULT_CLIENT_FILE;
    cfg->ca_file = DEFAULT_CA_FILE;
}

static void usage(int ec)
{
    fprintf(stderr,
            "Usage: icky [options]\n"
            "   -h          Display this help\n"
            "   -i          Read input and push to clipboard\n"
    exit(ec);
}

int main(int argc, char *argv[])
{
    bool input = false;
    int opt;
    char *buffer = NULL;
    cfg_t;

    while ((opt = getopt(argc, argv, "hio:"))) {
        switch (opt) {
            case 'h':
                usage(EXIT_SUCCESS);
                break;

            case 'i':
                input = true;
                break;

            case 'o':
                output_file = strdup(optarg);
                break;

            default:
                usage(EXIT_FAILURE);
        }
    }

    cfg_setup(&cfg);

    if (input) {
        read_stream(&buffer, stdin);
        push(buffer, &cfg);
    } else {
        get(&buffer, &cfg);
        printf("%s", buffer);
    }


cleanup:
    free(output_file);
    free(buffer);

    return EXIT_SUCCESS;
}
