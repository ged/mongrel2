begin transaction;

DROP TABLE IF EXISTS server;
DROP TABLE IF EXISTS host;
DROP TABLE IF EXISTS handler;
DROP TABLE IF EXISTS proxy;
DROP TABLE IF EXISTS route;
DROP TABLE IF EXISTS statistic;
DROP TABLE IF EXISTS mimetype;
DROP TABLE IF EXISTS setting;
DROP TABLE IF EXISTS directory;
DROP TABLE IF EXISTS filter;
DROP TABLE IF EXISTS log;

CREATE TABLE server (id serial,
    uuid TEXT,
    access_log TEXT,
    error_log TEXT,
    chroot TEXT DEFAULT '/var/www',
    pid_file TEXT,
    default_host TEXT,
    name TEXT DEFAULT '',
    bind_addr TEXT DEFAULT '0.0.0.0',
    port INTEGER,
    use_ssl INTEGER DEFAULT 0);

CREATE TABLE host (id serial,
    server_id INTEGER,
    maintenance BOOLEAN DEFAULT false,
    name TEXT,
    matching TEXT);

CREATE TABLE handler (id serial,
    send_spec TEXT,
    send_ident TEXT,
    recv_spec TEXT,
    recv_ident TEXT,
    raw_payload INTEGER DEFAULT 0,
    protocol TEXT DEFAULT 'json');

CREATE TABLE proxy (id serial,
    addr TEXT,
    port INTEGER);

CREATE TABLE directory (id serial,
    base TEXT,
    index_file TEXT,
    default_ctype TEXT,
    cache_ttl INTEGER DEFAULT 0);

CREATE TABLE route (id serial,
    path TEXT,
    reversed BOOLEAN DEFAULT false,
    host_id INTEGER,
    target_id INTEGER,
    target_type TEXT);


CREATE TABLE setting (id serial, key TEXT, value TEXT);


CREATE TABLE statistic (id SERIAL,
    other_type TEXT,
    other_id INTEGER,
    name text,
    sum REAL,
    sumsq REAL,
    n INTEGER,
    min REAL,
    max REAL,
    mean REAL,
    sd REAL,
    primary key (other_type, other_id, name));

CREATE TABLE filter (id serial,
    server_id INTEGER,
    name TEXT,
    settings TEXT);

CREATE TABLE mimetype (id serial, mimetype TEXT, extension TEXT);

CREATE TABLE IF NOT EXISTS log(id serial,
    who TEXT,
    what TEXT,
    location TEXT,
    happened_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    how TEXT,
    why TEXT);

commit;
