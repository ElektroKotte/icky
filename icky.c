#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>

#include <curl/curl.h>

#define DEFAULT_CFG_DIR "."
#define DEFAULT_SERVER "https://kottland.net:3000/sticky"
#define DEFAULT_CFG_FILE DEFAULT_CFG_DIR "/config"
#define DEFAULT_CA_FILE DEFAULT_CFG_DIR "/ca.crt"
#define DEFAULT_CLIENT_FILE DEFAULT_CFG_DIR "/client.pem"

#define DEFAULT_VERBOSE 1l

#define CHUNK 4096
#define CAPACITY_MAX (100 * 1024 * 1024)
#define CAPACITY_INC (CHUNK * 10)

#define log(fmt, ...) printf("[%s:%d] " fmt "\n",__func__, __LINE__, ## __VA_ARGS__)
#define err(fmt, ...) fprintf(stderr, fmt "\n", ## __VA_ARGS__)

typedef struct {
    char *ca_file;
    char *client_file;
    char *server;
    long verbose;
} cfg_t;

static bool read_input(char **buffer, size_t *buffer_size)
{
    size_t capacity = 0;
    char chunk[CHUNK] = "\0";
    size_t nbytes;

    *buffer_size = 0;
    *buffer = NULL;
    do {
        nbytes = fread(chunk, 1, CHUNK, stdin);
        if (*buffer_size + nbytes >= capacity) {
            capacity += CAPACITY_INC;
            if (capacity >= CAPACITY_MAX) goto fail;

            void *newptr = realloc(*buffer, capacity);
            if (newptr) {
                *buffer = newptr;
            } else goto fail;
        }
        memcpy(*buffer + *buffer_size, chunk, nbytes);
        *buffer_size += nbytes;
    } while (nbytes == CHUNK);

    return true;

fail:
    free(*buffer);
    *buffer = NULL;
    return false;
}

static void post(const cfg_t *cfg)
{
    int res = CURLE_FAILED_INIT;
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;

    char *buffer;
    size_t buffer_size;

    if (!read_input(&buffer, &buffer_size)) goto cleanup;

    curl = curl_easy_init();
    if (!curl) goto cleanup;

    headers = curl_slist_append(headers, "Content-Type: text/plain");

    if ((res = curl_easy_setopt(curl, CURLOPT_VERBOSE, cfg->verbose)) != CURLE_OK) goto cleanup;
    if ((res = curl_easy_setopt(curl, CURLOPT_URL, cfg->server)) != CURLE_OK) goto cleanup;
    if ((res = curl_easy_setopt(curl, CURLOPT_SSLCERT, cfg->client_file)) != CURLE_OK) goto cleanup;
    if ((res = curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM")) != CURLE_OK) goto cleanup;
    if ((res = curl_easy_setopt(curl, CURLOPT_CAINFO, cfg->ca_file)) != CURLE_OK) goto cleanup;
    if ((res = curl_easy_setopt(curl, CURLOPT_POST, 1l)) != CURLE_OK) goto cleanup;
    if ((res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer)) != CURLE_OK) goto cleanup;
    if ((res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, buffer_size)) != CURLE_OK) goto cleanup;
    if ((res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers)) != CURLE_OK) goto cleanup;

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        err("curl perform failed: %s", curl_easy_strerror(res));
    }

cleanup:
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(buffer);
}

static void get(const cfg_t *cfg)
{
    int res = CURLE_FAILED_INIT;
    CURL *curl;

    curl = curl_easy_init();
    if (!curl) {
	    goto cleanup;
    }

    if ((res = curl_easy_setopt(curl, CURLOPT_VERBOSE, cfg->verbose)) != CURLE_OK) goto cleanup;
    if ((res = curl_easy_setopt(curl, CURLOPT_URL, cfg->server)) != CURLE_OK) goto cleanup;
    if ((res = curl_easy_setopt(curl, CURLOPT_SSLCERT, cfg->client_file)) != CURLE_OK) goto cleanup;
    if ((res = curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM")) != CURLE_OK) goto cleanup;
    if ((res = curl_easy_setopt(curl, CURLOPT_CAINFO, cfg->ca_file)) != CURLE_OK) goto cleanup;

    res = curl_easy_perform(curl);

cleanup:
    if (res != CURLE_OK) err("Curl error: %s", curl_easy_strerror(res));
    curl_easy_cleanup(curl);
}

static void cfg_setup(cfg_t *cfg)
{
    cfg->client_file = DEFAULT_CLIENT_FILE;
    cfg->ca_file = DEFAULT_CA_FILE;
    cfg->server = DEFAULT_SERVER;
    cfg->verbose = DEFAULT_VERBOSE;
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
