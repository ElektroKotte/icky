#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <curl/curl.h>

#define DEFAULT_CFG_DIR "~/.icky"
#define DEFAULT_SERVER "https://localhost:7010/sticky"
#define DEFAULT_CFG_FILE DEFAULT_CFG_DIR "/config.lua"
#define DEFAULT_CA_FILE DEFAULT_CFG_DIR "/ca.crt"
#define DEFAULT_CLIENT_FILE DEFAULT_CFG_DIR "/client.pem"

#define DEFAULT_VERBOSE 0l

#define CHUNK 4096
#define CAPACITY_MAX (100 * 1024 * 1024)
#define CAPACITY_INC (CHUNK * 10)

#define log(fmt, ...) \
    if (m_verbose) printf("[%s:%d] " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#define err(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

typedef struct {
    char *ca_file;
    char *client_file;
    char *server;
    long verbose;
} cfg_t;

static bool m_verbose = false;

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
            } else
                goto fail;
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
    if ((res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, buffer_size)) != CURLE_OK)
        goto cleanup;
    if ((res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers)) != CURLE_OK) goto cleanup;

    res = curl_easy_perform(curl);

cleanup:
    if (res != CURLE_OK) {
        err("There was a curl error: %s", curl_easy_strerror(res));
    }

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

static void cfg_free(cfg_t *cfg)
{
    free(cfg->client_file);
    free(cfg->ca_file);
    free(cfg->server);
}

static void cfg_setup(const char *config_file, cfg_t *cfg)
{
    wordexp_t exp_res;
    lua_State *state;

    char *client_cert = NULL;
    char *ca_cert = NULL;

    cfg->verbose = DEFAULT_VERBOSE;
    cfg->client_file = NULL;
    cfg->server = NULL;
    cfg->ca_file = NULL;

    if (!config_file || !cfg) {
        log("Invalid parameters for getting configuration");
        goto cfg_set;
    }

    state = luaL_newstate();

    luaopen_base(state);
    luaopen_io(state);
    luaopen_string(state);
    luaopen_math(state);

    wordexp(config_file, &exp_res, WRDE_NOCMD);
    if (luaL_loadfile(state, exp_res.we_wordv[0]) || lua_pcall(state, 0, 0, 0)) {
        log("Unable to run configuration file: %s", lua_tostring(state, -1));
        goto cfg_set;
    }

    lua_getglobal(state, "curl_verbose");
    lua_getglobal(state, "client_cert");
    lua_getglobal(state, "ca_cert");
    lua_getglobal(state, "server");

    if (lua_isboolean(state, -4)) {
        cfg->verbose = lua_toboolean(state, -4) ? 1l : 0l;
    }
    if (lua_isstring(state, -3)) {
        client_cert = strdup(lua_tostring(state, -3));
    }
    if (lua_isstring(state, -2)) {
        ca_cert = strdup(lua_tostring(state, -2));
    }
    if (lua_isstring(state, -1)) {
        cfg->server = strdup(lua_tostring(state, -1));
    }

    lua_close(state);

cfg_set:
    if (!client_cert) client_cert = strdup(DEFAULT_CLIENT_FILE);
    if (!wordexp(client_cert, &exp_res, WRDE_NOCMD | WRDE_REUSE)) {
        cfg->client_file = strdup(exp_res.we_wordv[0]);
    } else {
        err("Unable to expand client file");
        cfg->client_file = strdup(DEFAULT_CLIENT_FILE);
    }

    if (!ca_cert) ca_cert = strdup(DEFAULT_CA_FILE);
    if (!wordexp(ca_cert, &exp_res, WRDE_NOCMD | WRDE_REUSE)) {
        cfg->ca_file = strdup(exp_res.we_wordv[0]);
    } else {
        err("Unable to expand ca file");
        cfg->client_file = strdup(DEFAULT_CA_FILE);
    }

    if (!cfg->server) cfg->server = strdup(DEFAULT_SERVER);

    wordfree(&exp_res);
    free(ca_cert);
    free(client_cert);

    log("client file: %s", cfg->client_file);
    log("ca file: %s", cfg->ca_file);
    log("server: %s", cfg->server);
    log("curl verbose: %ld", cfg->verbose);
}

static void usage(int ec)
{
    // clang-format off
    fprintf(stderr,
            "Usage: icky [options]\n"
            "Configure icky by setting the following fields in the configuration file\n"
            "-- icky lua configuration file --\n"
            "-- curl_verbose = false\n"
            "-- client_cert = '" DEFAULT_CLIENT_FILE "'\n"
            "-- ca_cert = '" DEFAULT_CA_FILE "'\n"
            "-- server = '" DEFAULT_SERVER "'\n"
            "\n"
            "The following options control ickys behavior\n"
            "   -c <cfg file>   Config file to use\n"
            "   -h              Display this help\n"
            "   -v              Be verbose\n"
            "   -i              Read input and push to clipboard\n");
    // clang-format on
    exit(ec);
}

int main(int argc, char *argv[])
{
    bool input = false;
    int opt;
    cfg_t cfg;
    char *config_file = NULL;

    while ((opt = getopt(argc, argv, "vhic:")) != -1) {
        switch (opt) {
            case 'v':
                m_verbose = true;
                break;

            case 'h':
                usage(EXIT_SUCCESS);
                break;

            case 'i':
                input = true;
                break;

            case 'c':
                config_file = strdup(optarg);
                break;

            default:
                log("here: %d", opt);
                usage(EXIT_FAILURE);
        }
    }

    if (!config_file) config_file = strdup(DEFAULT_CFG_FILE);
    cfg_setup(config_file, &cfg);

    if (input) {
        post(&cfg);
    } else {
        get(&cfg);
    }

    curl_global_cleanup();
    free(config_file);
    cfg_free(&cfg);

    return EXIT_SUCCESS;
}
