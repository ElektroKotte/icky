#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>

#include <curl/curl.h>

#define DEFAULT_CFG_DIR "~/.icky"
#define DEFAULT_SERVER "localhost:7777/sticky"
#define DEFAULT_CFG_FILE DEFAULT_CFG_DIR "/config"
#define DEFAULT_CA_FILE DEFAULT_CFG_DIR "/ca.crt"
#define DEFAULT_CLIENT_FILE DEFAULT_CFG_DIR "/client.crt"

#define log(fmt, ...) printf("[%s:%d] " fmt "\n",__func__, __LINE__, ## __VA_ARGS__)
#define err(fmt, ...) fprintf(stderr, fmt "\n", ## __VA_ARGS__)

typedef struct {
    char *ca_file;
    char *client_file;
    char *server;
} cfg_t;

static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
    (void) ptr;
    (void) size;
    (void) nmemb;
    (void) stream;
    log("random stuff");
    return 0;
}

static void post(const cfg_t *cfg)
{
    int res;
    CURL *curl;

    curl = curl_easy_init();
    if (!curl) goto cleanup;

    curl_easy_setopt(curl, CURLOPT_URL, cfg->server);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_PUT, 1l);

    log("Performing stuff");
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        err("curl perform failed: %s", curl_easy_strerror(res));
    }

cleanup:
    curl_easy_cleanup(curl);
}

static void get(const cfg_t *cfg)
{
    int res;
    CURL *curl;

    curl = curl_easy_init();
    if (!curl) goto cleanup;

    curl_easy_setopt(curl, CURLOPT_URL, cfg->server);
    
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        err("curl perform failed: %s", curl_easy_strerror(res));
    }

cleanup:
    curl_easy_cleanup(curl);
}

static void cfg_setup(cfg_t *cfg)
{
    cfg->client_file = DEFAULT_CLIENT_FILE;
    cfg->ca_file = DEFAULT_CA_FILE;
    cfg->server = DEFAULT_SERVER;
}

static void usage(int ec)
{
    fprintf(stderr,
            "Usage: icky [options]\n"
            "   -h          Display this help\n"
            "   -i          Read input and push to clipboard\n");
    exit(ec);
}

int main(int argc, char *argv[])
{
    bool input = false;
    int opt;
    cfg_t cfg;

    while ((opt = getopt(argc, argv, "hio:")) != -1) {
        switch (opt) {
            case 'h':
                usage(EXIT_SUCCESS);
                break;

            case 'i':
                input = true;
                break;

            default:
                log("here: %d", opt);
                usage(EXIT_FAILURE);
        }
    }

    cfg_setup(&cfg);

    if (input) {
        post(&cfg);
    } else {
        get(&cfg);
    }

    curl_global_cleanup();

    return EXIT_SUCCESS;
}
