/**
 *
 * Copyright (c) 2010, Zed A. Shaw and Mongrel2 Project Contributors.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 *     * Neither the name of the Mongrel2 Project, Zed A. Shaw, nor the names
 *       of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written
 *       permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "dbg.h"
#include "config/module.h"
#include "tnetstrings_impl.h"

#include <libpq-fe.h>


/* Column Types */
#define BOOLEAN_OID   16
#define INT4_OID      23
#define TEXT_OID      25
#define FLOAT4_OID    700
#define TIMESTAMP_OID 1114

/* Quiet the warning about (intentionally) unused variables */
#define UNUSED(x) ((void)(x))


/* Connection global */
static PGconn *conn;


/* Log potential problems to the mongrel2 log */
static void
config_notice_processor( void *unused, const char *msg )
{
    UNUSED( unused );
    log_warn( "%s", msg );
}


/* Plugin startup */
int
config_init( const char *conninfo )
{
    conn = PQconnectdb( conninfo );

    if ( PQstatus(conn) != CONNECTION_OK ) {
        log_err( "Connection failed: %s", PQerrorMessage(conn) );
        return -1;
    }

    PQsetNoticeProcessor( conn, config_notice_processor, NULL );

    return 0;
}

/* Plugin shutdown */
void
config_close()
{
    if ( conn ) PQfinish( conn );
    conn = NULL;
}


/* Convert a result row to a row of tnetstring values */
static tns_value_t *
config_convert_row( const PGresult *pgres, int row )
{
    int col = 0;
    bstring value;
    tns_value_t *el = NULL;
    tns_value_t *res = tns_new_list();

    check_mem( res );

    debug( "Converting row %d.", row );
    for ( col = 0; col < PQnfields(pgres); col++ ) {
        debug( "  column %d (%d)", col, PQfname(pgres, col) );
        if ( PQgetisnull(pgres, row, col) ) {
            debug( "Column was null." );
            el = tns_get_null();
        }

        else {
            value = bfromcstr( PQgetvalue(pgres, row, col) );
            debug( "  column is: %s.", bdata(value) );

            switch ( PQftype(pgres, col) ) {
                case BOOLEAN_OID:
                    el = (bstrncmp(value, bfromcstr("t"), 1) == 0) ? tns_get_true() : tns_get_false();
                    break;
                case INT4_OID:
                    el = tns_parse_integer( bdata(value), blength(value) );
                    break;
                case FLOAT4_OID:
                    el = tns_parse_float( bdata(value), blength(value) );
                    break;
                case TEXT_OID:
                    el = tns_parse_string( bdata(value), blength(value) );
                    break;
                case TIMESTAMP_OID:
                    el = tns_parse_string( bdata(value), blength(value) );
                    break;
                default:
                    break;
            }

            bdestroy( value );
        }

        check( el != NULL, "Got a NULL for column %s (OID %d)",
               PQfname(pgres, col), PQftype(pgres, col) );
        tns_add_to_list( res, el );
    }

    return res;

error:
    if ( res ) tns_value_destroy( res );
    if ( el ) tns_value_destroy( el );
    if ( value ) bdestroy( value );
    return NULL;
}


/* Execute a query and return the results as a tnetstring value */
static tns_value_t *
config_pg_exec( const char *query,
                int nParams,
                const char * const *paramValues,
                const int *paramLengths,
                const int *paramFormats )
{
    PGresult *pgres = NULL;
    ExecStatusType pgstatus;
    tns_value_t *res = NULL;
    int nRows = 0;
    int i = 0;
    size_t len = 0;
    char *rendered;

    log_info( "Executing config query '%s' with %d parameters.", query, nParams );

    /* Execute the query */
    pgres = PQexecParams( conn, query, nParams, NULL, paramValues, paramLengths, paramFormats, 0 );
    check( pgres != NULL, "Query '%s' failed: \"%s\"", query, PQerrorMessage(conn) );

    pgstatus = PQresultStatus( pgres );
    check( pgstatus != PGRES_EMPTY_QUERY, "%s", PQresStatus(pgstatus) );
    check( pgstatus != PGRES_BAD_RESPONSE, "%s: %s",
           PQresStatus(pgstatus), PQresultErrorMessage(pgres) );
    check( pgstatus != PGRES_NONFATAL_ERROR, "%s: %s",
           PQresStatus(pgstatus), PQresultErrorMessage(pgres) );

    if ( pgstatus == PGRES_COMMAND_OK ) {
        debug( "Query succeeded, but returned no results." );
        res = tns_get_null();
    } else if ( pgstatus == PGRES_TUPLES_OK ) {
        res = tns_new_list();
        nRows = PQntuples( pgres );
        debug( "Query succeeded: %d rows in the result.", nRows );

        for ( i = 0; i < nRows; i++ ) {
            debug( "  fetching row %d.", i );
            tns_value_t *row = config_convert_row( pgres, i );
            check( row != NULL, "Failed to convert row for query: %s", query );
            tns_add_to_list( res, row );
        }
    }

    debug( "Clearing the result." );
    PQclear( pgres );

    rendered = tns_render( (void *)res, &len );
    debug( "Result %p (as a tnetstring) is: %s", res, rendered );
    free( rendered );

    return res;

error:
    if ( pgres ) PQclear( pgres );
    if ( res ) tns_value_destroy( res );
    return NULL;
}



tns_value_t *
config_load_handler( int handler_id )
{
    const char *HANDLER_QUERY = "SELECT id, send_spec, send_ident, recv_spec, "
        "recv_ident, raw_payload, protocol FROM handler WHERE id = $1::int";
    const char *paramValues[1];
    int         paramLengths[1];
    int         paramFormats[1];
    uint32_t    binaryIntVal = htonl((uint32_t) handler_id);

    debug( "Loading handler %d.", handler_id );

    paramValues[0] = (char *)&binaryIntVal;
    paramLengths[0] = sizeof(binaryIntVal);
    paramFormats[0] = 1;

    return config_pg_exec( HANDLER_QUERY, 1, paramValues, paramLengths, paramFormats );
}

tns_value_t *
config_load_proxy( int proxy_id )
{
    const char *PROXY_QUERY = "SELECT id, addr, port FROM proxy WHERE id = $1::int";
    const char *paramValues[1];
    int         paramLengths[1];
    int         paramFormats[1];
    uint32_t    binaryIntVal = htonl((uint32_t) proxy_id);

    debug( "Loading proxy %d.", proxy_id );

    paramValues[0] = (char *)&binaryIntVal;
    paramLengths[0] = sizeof(binaryIntVal);
    paramFormats[0] = 1;

    return config_pg_exec( PROXY_QUERY, 1, paramValues, paramLengths, paramFormats );
}

tns_value_t *
config_load_dir( int dir_id )
{
    const char *DIR_QUERY = "SELECT id, base, index_file, default_ctype, cache_ttl "
        "FROM directory WHERE id = $1::int";
    const char *paramValues[1];
    int         paramLengths[1];
    int         paramFormats[1];
    uint32_t    binaryIntVal = htonl((uint32_t) dir_id);

    debug( "Loading dir %d.", dir_id );

    paramValues[0] = (char *)&binaryIntVal;
    paramLengths[0] = sizeof(binaryIntVal);
    paramFormats[0] = 1;

    return config_pg_exec(DIR_QUERY, 1, paramValues, paramLengths, paramFormats );
}

tns_value_t *
config_load_routes( int host_id, int server_id )
{
    const char *ROUTE_QUERY = "SELECT route.id, route.path, route.target_id, route.target_type "
        "FROM route, host WHERE host_id = $1::int AND "
        "host.server_id = $2::int AND host.id = route.host_id";
    const char *paramValues[2];
    int         paramLengths[2];
    int         paramFormats[2];
    uint32_t    hostIntVal = htonl((uint32_t) host_id);
    uint32_t    serverIntVal = htonl((uint32_t) server_id);

    debug( "Loading routes for server %d, host %d.", server_id, host_id );

    paramValues[0] = (char *)&hostIntVal;
    paramValues[1] = (char *)&serverIntVal;
    paramLengths[0] = sizeof(hostIntVal);
    paramLengths[1] = sizeof(serverIntVal);
    paramFormats[0] = 1;
    paramFormats[1] = 1;

    return config_pg_exec( ROUTE_QUERY, 2, paramValues, paramLengths, paramFormats );
}

tns_value_t *
config_load_hosts( int server_id )
{
    const char *HOST_QUERY = "SELECT id, name, matching, server_id FROM host "
        "WHERE server_id = $1::int";
    const char *paramValues[1];
    int         paramLengths[1];
    int         paramFormats[1];
    uint32_t    binaryIntVal = htonl((uint32_t) server_id);

    debug( "Loading hosts for server %d.", server_id );

    paramValues[0] = (char *)&binaryIntVal;
    paramLengths[0] = sizeof(binaryIntVal);
    paramFormats[0] = 1;

    return config_pg_exec( HOST_QUERY,  1, paramValues, paramLengths, paramFormats );
}

tns_value_t *
config_load_server( const char *uuid )
{
    const char *SERVER_QUERY = "SELECT id, uuid, default_host, bind_addr, port, chroot, "
        "access_log, error_log, pid_file, use_ssl FROM server WHERE uuid = $1::text";
    const char *paramValues[1] = { uuid };

    debug( "Loading server '%s'.", uuid );

    return config_pg_exec( SERVER_QUERY, 1, paramValues, NULL, NULL );
}


tns_value_t *
config_load_mimetypes()
{
    const char *MIME_QUERY = "SELECT id, extension, mimetype FROM mimetype";

    debug( "Loading mimetypes." );

    return config_pg_exec( MIME_QUERY, 0, NULL, NULL, NULL );
}

tns_value_t *
config_load_settings()
{
    const char *SETTINGS_QUERY = "SELECT id, key, value FROM setting";

    debug( "Loading settings." );

    return config_pg_exec( SETTINGS_QUERY, 0, NULL, NULL, NULL );
}

tns_value_t *
config_load_filters( int server_id )
{
    const char *FILTER_QUERY = "SELECT id, name, settings FROM filter WHERE server_id = $1::int";
    const char *paramValues[1];
    int         paramLengths[1];
    int         paramFormats[1];
    uint32_t    binaryIntVal = htonl((uint32_t) server_id);

    debug( "Loading filters." );

    paramValues[0] = (char *)&binaryIntVal;
    paramLengths[0] = sizeof(binaryIntVal);
    paramFormats[0] = 1;

    return config_pg_exec( FILTER_QUERY, 1, paramValues, paramLengths, paramFormats );
}

