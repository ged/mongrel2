#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <task/task.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <dbg.h>
#include <proxy.h>
#include <assert.h>
#include <stdlib.h>
#include <mem/halloc.h>

enum
{
    STACK = 32768
};


void rwtask(void*);


ProxyConnect *ProxyConnect_create(int client_fd, char *buffer, size_t size, size_t n)
{
    ProxyConnect *conn = h_malloc(sizeof(ProxyConnect));
    check(conn, "Failed to allocate ProxyConnect.");
    conn->client_fd = client_fd;
    conn->proxy_fd = 0;
    conn->buffer = buffer;
    conn->size = size;
    conn->n = n; 

    return conn;
error:
    return NULL;
}

void ProxyConnect_destroy(ProxyConnect *conn) 
{
    if(conn) {
        if(conn->client_fd) shutdown(conn->client_fd, SHUT_WR);
        if(conn->proxy_fd) close(conn->proxy_fd);
        // We don't own the buf
    }
}


void Proxy_destroy(Proxy *proxy)
{
    if(proxy) {
        if(proxy->server) bdestroy(proxy->server);
        h_free(proxy);
    }
}

Proxy *Proxy_create(bstring server, int port)
{
    Proxy *proxy = h_calloc(sizeof(Proxy), 1);
    check(proxy, "Failed to create proxy, memory allocation fail.");
    
    proxy->server = server;
    proxy->port = port;

    return proxy;

error:
    Proxy_destroy(proxy);
    return NULL;
}

ProxyConnect *Proxy_connect_backend(Proxy *proxy, int fd, const char *buf, 
        size_t len, size_t nread)
{
    ProxyConnect *to_proxy = NULL;

    char *initial_buf = h_malloc(len);
    check(initial_buf, "Out of memory.");
    check(memcpy(initial_buf, buf, nread), "Failed to copy the initial buffer.");

    // this is used by both sides, but owned by the to_proxy task
    to_proxy = ProxyConnect_create(fd, initial_buf, len, nread);
    to_proxy->waiter = h_calloc(sizeof(Rendez), 1);
    hattach(to_proxy, to_proxy->waiter);

    check(to_proxy, "Could not create the connection to backend %s:%d",
            bdata(proxy->server), proxy->port);

    debug("Connecting to %s:%d", bdata(proxy->server), proxy->port);

    // TODO: create release style macros that compile these away taskstates
    taskstate("connecting");

    to_proxy->proxy_fd = netdial(TCP, bdata(proxy->server), proxy->port);
    check(to_proxy->proxy_fd >= 0, "Failed to connect to %s:%d", bdata(proxy->server), proxy->port);

    debug("Proxy connected to %s:%d", bdata(proxy->server), proxy->port);

    return to_proxy;
error:

    ProxyConnect_destroy(to_proxy);
    return NULL;
}


ProxyConnect *Proxy_sync_to_listener(ProxyConnect *to_proxy)
{
    ProxyConnect *to_listener = NULL;

    to_listener = ProxyConnect_create(to_proxy->client_fd, 
            h_malloc(to_proxy->size), to_proxy->size, 0);

    check(to_listener, "Could not create listener side proxy connect.");

    // halloc will make sure the rendez goes away when the to_listener does
    to_listener->waiter = to_proxy->waiter;
    to_listener->proxy_fd = to_proxy->proxy_fd;

    // kick off one side as a task to do its thing on the proxy
    taskcreate(rwtask, (void *)to_listener, STACK);

    return to_listener;

error:
    ProxyConnect_destroy(to_proxy);
    ProxyConnect_destroy(to_listener);
    return NULL;
}


void
rwtask(void *v)
{
    ProxyConnect *to_listener = (ProxyConnect *)v;
    int rc = 0;

    do {
        rc = fdsend(to_listener->client_fd, to_listener->buffer, to_listener->n);

        if(rc != to_listener->n) {
            break;
        }
        
    } while((to_listener->n = fdrecv(to_listener->proxy_fd, to_listener->buffer, to_listener->size)) > 0);

    taskbarrier(to_listener->waiter);

    ProxyConnect_destroy(to_listener);
}

