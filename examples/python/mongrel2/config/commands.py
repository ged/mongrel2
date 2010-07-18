from mongrel2 import config
from mongrel2.config import args
import mongrel2.config.commands
from uuid import uuid4
from mongrel2.config import model
import getpass
import sys
import os
import signal


def shell_command():
    """
    Starts an interactive shell with readline style input so you can
    work with Mongrel2 easier.
    """
    try:
        from pyrepl.unix_console import UnixConsole
        from pyrepl.historical_reader import HistoricalReader
    except:
        print "You don't have PyRepl installed, shell not available."

    reader = HistoricalReader(UnixConsole())
    reader.ps1 = "m2> "
    reader.ps2 = "..> "
    reader.ps3 = "...> "
    reader.ps4 = "....> "

    try:

        while True:
            cmd = reader.readline()
            if cmd:
                args.parse_and_run_command(
                    cmd.split(' '), mongrel2.config.commands,
                       default_command=None, exit_on_error=False)
    except EOFError:
        print "Bye."



def help_command(**options):
    """
    Prints out help for the commands. 

    m2sh help

    You can get help for one command with:

    m2sh help -for STR
    """
    if "for" in options:
        help_text = args.help_for_command(config.commands, options['for'])
        if help_text:
            print help_text
        else:
            args.invalid_command_message(config.commands, exit_on_error=True)
    else:
        print "Available commands:\n"
        print ", ".join(args.available_commands(config.commands))
        print "\nUse config help -for <command> to find out more."



def dump_command(db=None):
    """
    Simple dump of a config database:

        m2sh dump -db config.sqlite
    """

    print "LOADING DB: ", db
    
    store = model.begin(db)
    servers = store.find(model.Server)

    for server in servers:
        print server

        for host in server.hosts:
            print "\t", host

            for route in host.routes:
                print "\t\t", route


def uuid_command(hex=False):
    """
    Generates a UUID for you to use in your configurations:

        m2sh uuid
        m2sh uuid -hex

    The -hex means to print it as a big hex number, which is
    more efficient but harder to read.
    """
    if hex:
        print uuid4().hex
    else:
        print str(uuid4())


def servers_command(db=None):
    """
    Lists the servers that are configured in this setup:

        m2sh servers -db config.sqlite
    """
    store = model.begin(db)
    servers = store.find(model.Server)
    for server in servers:
        print "-------"
        print server.default_host, server.uuid

        for host in server.hosts:
            print "\t", host.id, ':', host.name


def hosts_command(db=None, uuid="", host=""):
    """
    List all the hosts in the given server identified by UUID or host.

        m2sh servers -db config.sqlite -uuid f400bf85-4538-4f7a-8908-67e313d515c2
        m2sh servers -db config.sqlite -host localhost

    The -host parameter is the default_host for the server.
    """
    store = model.begin(db)
    results = None

    if uuid:
        results = store.find(model.Server, model.Server.uuid == unicode(uuid))
    elif host:
        results = store.find(model.Server, model.Server.default_host == unicode(host))
    else:
        print "ERROR: Must give a -host or -uuid."
        return

    if results.count():
        server = results[0]
        hosts = store.find(model.Host, model.Host.server_id == server.id)
        for host in hosts:
            print "--------"
            print host, ":"
            for route in host.routes:
                print "\t", route.path, ':', route.target
            
    else:
        print "No servers found."


def init_command(db=None):
    """
    Initializes a new config database.

        m2sh init -db config.sqlite

    It will obliterate this config.
    """
    from pkg_resources import resource_stream
    import sqlite3

    sql = resource_stream('mongrel2', 'sql/config.sql').read()

    if model.store:
        model.store.close()
        model.store = None

    conn = sqlite3.connect(db)
    conn.executescript(sql)
   
    commit_command(db=db, what="init_command", why=" ".join(sys.argv))


def load_command(db=None, config=None, clear=True):
    """
    After using init you can use this to load a config:

        m2sh load -db config.sqlite -config tests/sample_conf.py 

    This will erase the previous config, but we'll make it
    safer later on.
    """
    import imp
    model.begin(db, clear=clear)
    imp.load_source('mongrel2_config_main', config)

    commit_command(db=db, what="load_command", why=" ".join(sys.argv))


def config_command(db=None, config=None, clear=True):
    """
    Effectively does an init then load of a config to get
    you started quicker:

        m2sh config -db config.sqlite -config tests/sample_conf.py

    Like the other two, this will nuke your config, but we'll
    make it safer later.
    """

    init_command(db=db)
    load_command(db=db, config=config, clear=clear)


def commit_command(db=None, what=None, why=None):
    """
    Used to a commit event to the database for other admins to know
    what is going on with the config.  The system logs quite a lot
    already for you, like your username, machine name, etc:

        m2sh commit -db test.sqlite -what mongrel2.org \
            -why "Needed to change paters."

    In future versions it will prevent you from committing as root,
    because only assholes commit from root.

    Both parameters are arbitrary, but I like to record what I did to
    different Hosts in servers.
    """
    import socket

    store = model.load_db("sqlite:" + db)
    who = unicode(getpass.getuser())

    if who == u'root':
        print "Commit from root eh?  Man, you're kind of a tool."

    log = model.Log()
    log.who = who
    log.what = unicode(what)
    log.why = unicode(why)
    log.location = unicode(socket.gethostname())
    log.how = u'm2sh'

    store.add(log)
    store.commit()
    

def log_command(db=None, count=20):
    """
    Dumps commit logs:

        m2sh log -db test.sqlite -count 20
        m2sh log -db test.sqlite

    So you know who to blame.
    """

    store = model.load_db("sqlite:" + db)
    logs = store.find(model.Log)

    for log in logs.order_by(model.Log.happened_at)[0:count]:
        print log


def start_command(db=None, host=None, sudo=False):
    """
    Does a simple start of the given server identified by the host
    (default_host) parameter:


        m2sh start -db config.sqlite -host localhost

    Give the -sudo options if you want it to start mongrel2 as
    root for you (must have sudo installed).
    """
    root_enabler = 'sudo' if sudo else ''

    os.system('%s mongrel2 %s %s' % (root_enabler, db, host))


def stop_command(db=None, host=None):
    """
    Stops a running mongrel2 process according to the host,
    and just like start will run sudo for you if you give -sudo:

        m2sh stop -db config.sqlite -host localhost

    You shouldn't need sudo to stop a running mongrel if you
    are also the user that owns the chroot directory.
    """
    pid = get_server_pid(db, host)
    os.kill(pid, signal.SIGTERM)


def running_command(db=None, host=None):
    """
    Tells you if the given server is still running:

        m2sh running -db config.sqlite -host localhost
    """

    pid = get_server_pid(db, host)
    try:
        os.kill(pid, 0)
        print "YES: Mongrel2 server %s running at PID %d" % (host, pid)
    except OSError:
        print "NO: Mongrel2 server %s NOT at PID %d" % (host, pid)


def get_server_pid(db, host):
    store = model.load_db("sqlite:" + db)
    results = store.find(model.Server, model.Server.default_host == unicode(host))

    if results.count() == 0:
        print "No servers found."
        return

    server = results[0]
    pid_file = os.path.realpath(server.chroot + server.pid_file)
    pid = int(open(pid_file, 'r').read())

    return pid

