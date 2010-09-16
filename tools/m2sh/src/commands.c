#include "config_file.h"
#include "cli.h"
#include "commands.h"
#include <dbg.h>
#include <stdio.h>

typedef int (*Command_handler_cb)(Command *cmd);

typedef struct CommandHandler {
    const char *name;
    const char *help;
    Command_handler_cb cb;
} CommandHandler;


static inline bstring option(Command *cmd, const char *name, const char *def)
{
    hnode_t *val = hash_lookup(cmd->options, name);

    if(def != NULL) {
        return val == NULL ? bfromcstr(def) : hnode_get(val);
    } else {
        return hnode_get(val);
    }
}

static int Command_load(Command *cmd)
{
    bstring db_file = option(cmd, "db", "config.sqlite");
    bstring conf_file = option(cmd, "config", "mongrel2.conf");

    debug("LOADING db: %s, config: %s", bdata(db_file), bdata(conf_file));

    Config_load(bdata(conf_file), bdata(db_file));

    return 0;

error:
    return -1;
}


static int Command_shell(Command *cmd)
{

    return -1;
}

static int Command_dump(Command *cmd)
{

    return -1;
}

static int Command_uuid(Command *cmd)
{

    return -1;
}

static int Command_servers(Command *cmd)
{

    return -1;
}


static int Command_hosts(Command *cmd)
{
    return -1;
}


static int Command_init(Command *cmd)
{
    printf("init is deprecated and simply done for you.");
    return 0;
}


static int Command_commit(Command *cmd)
{
    return -1;
}


static int Command_log(Command *cmd)
{
    return -1;
}

static int Command_start(Command *cmd)
{
    return -1;
}

static int Command_stop(Command *cmd)
{
    return -1;
}

static int Command_reload(Command *cmd)
{
    return -1;
}

static int Command_running(Command *cmd)
{
    return -1;
}

static int Command_control(Command *cmd)
{
    return -1;
}

static int Command_version(Command *cmd)
{
    return -1;
}


static int Command_help(Command *cmd);

static CommandHandler COMMAND_MAPPING[] = {
    {.name = "load", .cb = Command_load,
        .help = "Load a config: load -db config.sqlite -config mongrel2.conf"},
    {.name = "config", .cb = Command_load,
        .help = "Alias for load: config -db config.sqlite -config mongrel2.conf" },
    {.name = "shell", .cb = Command_shell,
        .help = "Starts an interactive shell (not implemented)." },
    {.name = "servers", .cb = Command_servers,
        .help = "Lists the servers: servers " },
    {.name = "dump", .cb = Command_dump,
        .help = "Dumps a quick view of the db: dump " },
    {.name = "uuid", .cb = Command_uuid,
        .help = "Prints out a uuid: uuid " },
    {.name = "hosts", .cb = Command_hosts,
        .help = ": hosts " },
    {.name = "init", .cb = Command_init,
        .help = "deprecated, just use load" },
    {.name = "commit", .cb = Command_commit,
        .help = "Adds a message to the log: commit " },
    {.name = "log", .cb = Command_log,
        .help = "Prints the commit log: log " },
    {.name = "start", .cb = Command_start,
        .help = "Starts a server: start " },
    {.name = "stop", .cb = Command_stop,
        .help = "Stops a server: stop " },
    {.name = "reload", .cb = Command_reload,
        .help = "Reloads a server: reload " },
    {.name = "running", .cb = Command_running,
        .help = "Tells you what's running: running " },
    {.name = "control", .cb = Command_control,
        .help = "Connects to the control port: control " },
    {.name = "version", .cb = Command_version,
        .help = "Prints the Mongrel2 and m2sh version: version " },
    {.name = "help", .cb = Command_help,
        .help = "Get help, lists commands." },
    {.name = NULL, .cb = NULL, .help = NULL}
};


static int Command_help(Command *cmd)
{
    CommandHandler *handler;

    printf("Mongrel2 m2sh has these commands available:\n\t");
    
    for(handler = COMMAND_MAPPING; handler->name != NULL; handler++) {
        printf("%s\t%s", handler->name, handler->help);
    }

    printf("\nSorry there's not much better help yet.\n");

    return 0;
}

int Command_run(bstring arguments)
{
    Command cmd;
    CommandHandler *handler;

    check(cli_params_parse_args(arguments, &cmd) != -1, "Invalid arguments.");

    for(handler = COMMAND_MAPPING; handler->name != NULL; handler++)
    {
        if(biseqcstr(cmd.name, handler->name)) {
            return handler->cb(&cmd);
        }
    }

    log_err("INVALID COMMAND. Use m2sh help to find out what's available.");

error:  // fallthrough on purpose
    return -1;
}
