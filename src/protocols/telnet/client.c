/*
 * Copyright (C) 2013 Glyptodon LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"
#include "client.h"
#include "clipboard.h"
#include "guac_handlers.h"
#include "telnet_client.h"
#include "terminal.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <guacamole/client.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>

#define GUAC_TELNET_DEFAULT_FONT_NAME "monospace" 
#define GUAC_TELNET_DEFAULT_FONT_SIZE 12
#define GUAC_TELNET_DEFAULT_PORT      "23"

/* Client plugin arguments */
const char* GUAC_CLIENT_ARGS[] = {
    "hostname",
    "port",
    "font-name",
    "font-size",
    NULL
};

enum __TELNET_ARGS_IDX {

    /**
     * The hostname to connect to. Required.
     */
    IDX_HOSTNAME,

    /**
     * The port to connect to. Optional.
     */
    IDX_PORT,

    /**
     * The name of the font to use within the terminal.
     */
    IDX_FONT_NAME,

    /**
     * The size of the font to use within the terminal, in points.
     */
    IDX_FONT_SIZE,

    TELNET_ARGS_COUNT
};

int guac_client_init(guac_client* client, int argc, char** argv) {

    guac_socket* socket = client->socket;

    guac_telnet_client_data* client_data = malloc(sizeof(guac_telnet_client_data));

    /* Init client data */
    client->data = client_data;
    client_data->telnet = NULL;
    client_data->socket_fd = -1;
    client_data->naws_enabled = 0;

    if (argc != TELNET_ARGS_COUNT) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR, "Wrong number of arguments");
        return -1;
    }

    /* Read parameters */
    strcpy(client_data->hostname,  argv[IDX_HOSTNAME]);

    /* Read port */
    if (argv[IDX_PORT][0] != 0)
        strcpy(client_data->port, argv[IDX_PORT]);
    else
        strcpy(client_data->port, GUAC_TELNET_DEFAULT_PORT);

    /* Read font name */
    if (argv[IDX_FONT_NAME][0] != 0)
        strcpy(client_data->font_name, argv[IDX_FONT_NAME]);
    else
        strcpy(client_data->font_name, GUAC_TELNET_DEFAULT_FONT_NAME );

    /* Read font size */
    if (argv[IDX_FONT_SIZE][0] != 0)
        client_data->font_size = atoi(argv[IDX_FONT_SIZE]);
    else
        client_data->font_size = GUAC_TELNET_DEFAULT_FONT_SIZE;

    /* Create terminal */
    client_data->term = guac_terminal_create(client,
            client_data->font_name, client_data->font_size,
            client->info.optimal_resolution,
            client->info.optimal_width, client->info.optimal_height);

    /* Fail if terminal init failed */
    if (client_data->term == NULL) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR, "Terminal initialization failed");
        return -1;
    }

    /* Send initial name */
    guac_protocol_send_name(socket, client_data->hostname);

    guac_socket_flush(socket);

    /* Set basic handlers */
    client->handle_messages   = guac_telnet_client_handle_messages;
    client->key_handler       = guac_telnet_client_key_handler;
    client->mouse_handler     = guac_telnet_client_mouse_handler;
    client->size_handler      = guac_telnet_client_size_handler;
    client->free_handler      = guac_telnet_client_free_handler;
    client->clipboard_handler = guac_telnet_clipboard_handler;

    /* Start client thread */
    if (pthread_create(&(client_data->client_thread), NULL, guac_telnet_client_thread, (void*) client)) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR, "Unable to start telnet client thread");
        return -1;
    }

    /* Success */
    return 0;

}

