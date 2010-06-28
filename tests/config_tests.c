#include "minunit.h"
#include <config/config.h>
#include <server.h>

FILE *LOG_FILE = NULL;

char *test_Config_load() 
{
    list_t *servers = Config_load_servers("tests/config.sqlite", "mongrel2.org");

    mu_assert(servers != NULL, "Should get a server list, is mongrel2 running already?");
    mu_assert(list_count(servers) == 1, "Failed to load the server.");

    return NULL;
}


char * all_tests() 
{
    mu_suite_start();

    Server_init();

    mu_run_test(test_Config_load);

    return NULL;
}

RUN_TESTS(all_tests);

