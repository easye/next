/*
Copyright © 2018-2019 Atlas Engineer LLC.
Use of this file is governed by the license that can be found in LICENSE.
*/
#pragma once

#include <stdarg.h>
#include <glib.h>
#include <libsoup/soup.h>

#include "window.h"

typedef GVariant * (*ServerCallback) (SoupXMLRPCParams *);

// The params variant type string is "av", and the embedded "v"'s type string
// can be anything.
// To ease params manipulation, we "unwrap" the variant array to return a tuple
// of all element types.
GVariant *server_unwrap_params(SoupXMLRPCParams *params) {
	GError *error = NULL;
	GVariant *variant = soup_xmlrpc_params_parse(params, NULL, &error);
	if (error) {
		g_warning("Malformed method parameters: %s", error->message);
		g_error_free(error);
		return NULL;
	}
	if (!g_variant_check_format_string(variant, "av", FALSE)) {
		g_warning("Malformed parameter value: %s", g_variant_get_type_string(variant));
		return NULL;
	}

	GVariantBuilder builder;
	g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);
	GVariantIter iter;
	g_variant_iter_init(&iter, variant);
	GVariant *child;
	while (g_variant_iter_loop(&iter, "v", &child)) {
		g_variant_builder_add_value(&builder, child);
	}

	return g_variant_builder_end(&builder);
}

static GVariant *server_window_make(SoupXMLRPCParams *params) {
	GVariant *unwrapped_params = server_unwrap_params(params);
	if (!unwrapped_params) {
		return g_variant_new_boolean(FALSE);
	}
	const char *a_key = NULL;
	g_variant_get(unwrapped_params, "(&s)", &a_key);
	g_message("Method parameter(s): %s", a_key);

	Window *window = window_init();
	g_hash_table_insert(state.windows, strdup(a_key), window);
	window->identifier = strdup(a_key);
	window->minibuffer->parent_window_identifier = strdup(window->identifier);
	g_message("Method result(s): window id %s", window->identifier);
	return g_variant_new_string(window->identifier);
}

static GVariant *server_window_set_title(SoupXMLRPCParams *params) {
	GVariant *unwrapped_params = server_unwrap_params(params);
	if (!unwrapped_params) {
		return g_variant_new_boolean(FALSE);
	}
	const char *a_key = NULL;
	const char *title = NULL;
	g_variant_get(unwrapped_params, "(&s&s)", &a_key, &title);
	g_message("Method parameter(s): %s, %s", a_key, title);

	Window *window = g_hash_table_lookup(state.windows, a_key);
	if (!window) {
		return g_variant_new_boolean(FALSE);
	}
	window_set_title(window, title);
	return g_variant_new_boolean(TRUE);
}

static GVariant *server_window_delete(SoupXMLRPCParams *params) {
	GVariant *unwrapped_params = server_unwrap_params(params);
	if (!unwrapped_params) {
		return g_variant_new_boolean(FALSE);
	}
	const char *a_key = NULL;
	g_variant_get(unwrapped_params, "(&s)", &a_key);
	g_message("Method parameter(s): %s", a_key);

	g_hash_table_remove(state.windows, a_key);
	return g_variant_new_boolean(TRUE);
}

static GVariant *server_window_active(SoupXMLRPCParams *_params) {
	// TODO: If we run a GTK application, then we could call
	// gtk_application_get_active_window() and get the identifier from there.
	// We could also lookup the active window in gtk_window_list_toplevels().
	GHashTableIter iter;
	gpointer key, value;

	char *id = "<no active window>";
	g_hash_table_iter_init(&iter, state.windows);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		Window *window = (Window *)value;
		if (gtk_window_is_active(GTK_WINDOW(window->base))) {
			id = window->identifier;
			break;
		}
	}

	g_message("Method parameter(s): %s", id);
	return g_variant_new_string(id);
}

static GVariant *server_window_exists(SoupXMLRPCParams *params) {
	GVariant *unwrapped_params = server_unwrap_params(params);
	if (!unwrapped_params) {
		return g_variant_new_boolean(FALSE);
	}
	const char *a_key = NULL;
	g_variant_get(unwrapped_params, "(&s)", &a_key);
	g_message("Method parameter(s): %s", a_key);

	Window *window = g_hash_table_lookup(state.windows, a_key);
	if (!window) {
		return g_variant_new_boolean(FALSE);
	}
	return g_variant_new_boolean(TRUE);
}

static GVariant *server_window_set_active_buffer(SoupXMLRPCParams *params) {
	GVariant *unwrapped_params = server_unwrap_params(params);
	if (!unwrapped_params) {
		return g_variant_new_boolean(FALSE);
	}
	const char *window_id = NULL;
	const char *buffer_id = NULL;
	g_variant_get(unwrapped_params, "(&s&s)", &window_id, &buffer_id);
	g_message("Method parameter(s): window id %s, buffer id %s", window_id, buffer_id);

	Window *window = g_hash_table_lookup(state.windows, window_id);
	Buffer *buffer = g_hash_table_lookup(state.buffers, buffer_id);
	if (window == NULL) {
		g_warning("Non-existent window %s", window_id);
		return g_variant_new_boolean(FALSE);
	}
	if (buffer == NULL) {
		g_warning("Non-existent buffer %s", buffer_id);
		return g_variant_new_boolean(FALSE);
	}
	window_set_active_buffer(window, buffer);
	return g_variant_new_boolean(TRUE);
}

static GVariant *server_buffer_make(SoupXMLRPCParams *params) {
	GVariant *unwrapped_params = server_unwrap_params(params);
	if (!unwrapped_params) {
		return g_variant_new_string("");
	}
	const char *a_key = NULL;
	GHashTable *options = g_hash_table_new(g_str_hash, g_str_equal);
	// Options are passed as a list of string.  We could have used a dictionary,
	// but the Cocoa XML-RPC library does not support it.
	{
		GVariantIter *iter;
		g_variant_get(unwrapped_params, "(&sav)", &a_key, &iter);

		GVariant *str_variant;
		gchar *key = NULL;
		gchar *value = NULL;
		while (g_variant_iter_loop(iter, "v", &str_variant)) {
			if (key == NULL) {
				g_variant_get(str_variant, "s", &key);
			} else {
				g_variant_get(str_variant, "s", &value);
				g_hash_table_insert(options, key, value);
			}
		}
		g_variant_iter_free(iter);
	}
	g_message("Method parameter(s): buffer ID %s, cookie file %s", a_key,
		g_hash_table_lookup(options, "COOKIES-PATH"));
	Buffer *buffer = buffer_init(g_hash_table_lookup(options, "COOKIES-PATH"));
	g_hash_table_insert(state.buffers, strdup(a_key), buffer);
	buffer->identifier = strdup(a_key);
	g_message("Method result(s): buffer id %s", buffer->identifier);
	return g_variant_new_string(buffer->identifier);
}

static GVariant *server_buffer_delete(SoupXMLRPCParams *params) {
	GVariant *unwrapped_params = server_unwrap_params(params);
	if (!unwrapped_params) {
		return g_variant_new_boolean(FALSE);
	}
	const char *a_key = NULL;
	g_variant_get(unwrapped_params, "(&s)", &a_key);
	g_message("Method parameter(s): %s", a_key);

	g_hash_table_remove(state.buffers, a_key);
	return g_variant_new_boolean(TRUE);
}

static GVariant *server_buffer_load(SoupXMLRPCParams *params) {
	GVariant *unwrapped_params = server_unwrap_params(params);
	if (!unwrapped_params) {
		return g_variant_new_boolean(FALSE);
	}
	const char *buffer_id = NULL;
	const char *uri = NULL;
	g_variant_get(unwrapped_params, "(&s&s)", &buffer_id, &uri);
	g_message("Method parameter(s): buffer id %s, URI %s", buffer_id, uri);

	Buffer *buffer = g_hash_table_lookup(state.buffers, buffer_id);
	if (!buffer) {
		g_warning("Non-existent buffer %s", buffer_id);
		return g_variant_new_boolean(FALSE);
	}

	buffer_load(buffer, uri);
	return g_variant_new_boolean(TRUE);
}

static GVariant *server_buffer_evaluate(SoupXMLRPCParams *params) {
	GVariant *unwrapped_params = server_unwrap_params(params);
	if (!unwrapped_params) {
		return g_variant_new_boolean(FALSE);
	}
	const char *buffer_id = NULL;
	const char *javascript = NULL;
	g_variant_get(unwrapped_params, "(&s&s)", &buffer_id, &javascript);
	g_message("Method parameter(s): buffer id %s", buffer_id);
	g_debug("Javascript: \"%s\"", javascript);

	Buffer *buffer = g_hash_table_lookup(state.buffers, buffer_id);
	if (!buffer) {
		g_warning("Non-existent buffer %s", buffer_id);
		return g_variant_new_string("");
	}
	char *callback_id = buffer_evaluate(buffer, javascript);
	g_message("Method result(s): callback id %s", callback_id);
	GVariant *callback_variant = g_variant_new_string(callback_id);
	g_free(callback_id);
	return callback_variant;
}

static GVariant *server_window_set_minibuffer_height(SoupXMLRPCParams *params) {
	GVariant *unwrapped_params = server_unwrap_params(params);
	if (!unwrapped_params) {
		return g_variant_new_boolean(FALSE);
	}
	const char *window_id = NULL;
	int minibuffer_height = 0;
	g_variant_get(unwrapped_params, "(&si)", &window_id,
		&minibuffer_height);
	g_message("Method parameter(s): window id %s, minibuffer height %i", window_id,
		minibuffer_height);

	Window *window = g_hash_table_lookup(state.windows, window_id);
	if (!window) {
		g_warning("Non-existent window %s", window_id);
		return g_variant_new_int64(0);
	}
	gint64 preferred_height = window_set_minibuffer_height(window, minibuffer_height);
	g_message("Method result(s): minibuffer preferred height %li", preferred_height);
	return g_variant_new_int64(preferred_height);
}

static GVariant *server_minibuffer_evaluate(SoupXMLRPCParams *params) {
	GVariant *unwrapped_params = server_unwrap_params(params);
	if (!unwrapped_params) {
		return g_variant_new_boolean(FALSE);
	}
	const char *window_id = NULL;
	const char *javascript = NULL;
	g_variant_get(unwrapped_params, "(&s&s)", &window_id, &javascript);
	g_message("Method parameter(s): window id %s", window_id);
	g_debug("Javascript: \"%s\"", javascript);

	Window *window = g_hash_table_lookup(state.windows, window_id);
	Minibuffer *minibuffer = window->minibuffer;
	char *callback_id = minibuffer_evaluate(minibuffer, javascript);
	g_message("Method result(s): callback id %s", callback_id);
	GVariant *callback_variant = g_variant_new_string(callback_id);
	g_free(callback_id);
	return callback_variant;
}

static GVariant *server_generate_input_event(SoupXMLRPCParams *params) {
	GVariant *unwrapped_params = server_unwrap_params(params);
	if (!unwrapped_params) {
		return g_variant_new_boolean(FALSE);
	}
	const char *window_id = NULL;
	guint hardware_keycode = 0; // We are given an integer "i", not a uint16.
	guint modifiers = 0; // modifiers
	guint keyval = 0;
	gdouble x = -1;
	gdouble y = -1;
	gboolean released = false;

	{
		if (!g_variant_check_format_string(unwrapped_params, "(siavidd)", FALSE)) {
			g_warning("Malformed input event: %s", g_variant_get_type_string(unwrapped_params));
			return g_variant_new_boolean(FALSE);
		}
		GVariantIter *iter;
		g_variant_get(unwrapped_params, "(siavidd)", &window_id, &hardware_keycode,
			&iter, &keyval, &x, &y);

		GVariant *str_variant;
		while (g_variant_iter_loop(iter, "v", &str_variant)) {
			gchar *str;
			g_variant_get(str_variant, "s", &str);
			if (g_strcmp0(str, "R") == 0) {
				released = true;
			} else {
				guint mod = window_string_to_modifier(str);
				if (mod != 0) {
					modifiers |= mod;
				}
			}
		}
		g_variant_iter_free(iter);
	}

	g_message("Method parameter(s): window id '%s', hardware_keycode %i, keyval %i, modifiers %i",
		window_id, hardware_keycode, keyval, modifiers);

	Window *window = g_hash_table_lookup(state.windows, window_id);
	if (!window) {
		g_warning("Non-existent window %s", window_id);
		return g_variant_new_boolean(FALSE);
	}

	GdkEvent event;
	if (x != -1) {
		// TODO: Rename hardware_keycode to something that fits mouse buttons.
		if (hardware_keycode != 0) {
			GdkEventScroll event_scroll = {
				.state = modifiers,
				.direction = hardware_keycode,
				.x = x,
				.y = y,
				.delta_x = -1,
				.delta_y = -1,
			};
			if (keyval == 5 || keyval == 7) {
				event_scroll.delta_x = 1;
				event_scroll.delta_y = 1;
			}
			event = (GdkEvent)event_scroll;
			event.type = GDK_SCROLL;
		} else {
			GdkEventButton event_button = {
				.button = keyval,
				.state = modifiers,
				.x = x,
				.y = y,
			};
			event = (GdkEvent)event_button;
			if (released) {
				event.type = GDK_BUTTON_RELEASE;
			} else {
				event.type = GDK_BUTTON_PRESS;
			}
		}
	} else {
		GdkEventKey event_key = {
			.hardware_keycode = hardware_keycode,
			.keyval = keyval,
			.state = modifiers,
			.group = 0,
			.string = NULL,
			.is_modifier = 0,
		};
		event = (GdkEvent)event_key;
		if (released) {
			event.type = GDK_KEY_RELEASE;
		} else {
			event.type = GDK_KEY_PRESS;
		}
	}

	WindowEvent *window_event = g_new(WindowEvent, 1);
	window_event->event = event;
	window_event->window = window;
	window_generate_input_event(window_event);

	return g_variant_new_boolean(TRUE);
}

static GVariant *server_list_methods(SoupXMLRPCParams *_params) {
	GHashTableIter iter;
	gpointer key;

	GVariantBuilder builder;
	g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);

	g_hash_table_iter_init(&iter, state.server_callbacks);
	while (g_hash_table_iter_next(&iter, &key, NULL)) {
		g_variant_builder_add_parsed(&builder, "%s", key);
	}

	return g_variant_builder_end(&builder);
}

/* ITER cannot be used after this call.  RESULT must be freed. */
GList *server_unwrap_string_list(GVariantIter *iter) {
	GVariant *str_variant;
	gchar *str;
	GList *result = NULL;
	while (g_variant_iter_loop(iter, "v", &str_variant)) {
		g_variant_get(str_variant, "s", &str);
		result = g_list_append(result, str);
	}
	g_variant_iter_free(iter);
	return result;
}

/* The GList can be freed but not its elements which are share with the result.
The result must be freed. */
char **server_string_list_to_array_pointer(GList *list) {
	guint length = g_list_length(list);
	if (length == 0) {
		return NULL;
	}

	char **result = g_new(char *, 1+length);
	GList *old_list = list;
	int i = 0;
	while (list != NULL) {
		result[i] = list->data;
		i++;
		list = list->next;
	}
	// Make sure it's NULL-terminated so that we can detect the end of the array.
	result[g_list_length(old_list)] = NULL;
	return result;
}

static GVariant *server_set_proxy(SoupXMLRPCParams *params) {
	GVariant *unwrapped_params = server_unwrap_params(params);
	if (!unwrapped_params) {
		return g_variant_new_boolean(FALSE);
	}
	GList *buffer_ids = NULL;
	const char *mode = NULL;
	const char *proxy_uri = NULL;
	char **ignore_hosts = NULL;
	// ignore_hosts is passed as a list of string variants.
	{
		GVariantIter *iter;
		GVariantIter *iter_buffers;
		g_variant_get(unwrapped_params, "(av&s&sav)", &iter_buffers, &mode, &proxy_uri, &iter);

		buffer_ids = server_unwrap_string_list(iter_buffers);
		GList *ignore_hosts_list = server_unwrap_string_list(iter);
		ignore_hosts = server_string_list_to_array_pointer(ignore_hosts_list);
		g_list_free(ignore_hosts_list);
	}

	{
		gchar *pretty_ignore_hosts = g_strjoinv(",", ignore_hosts);
		char **buffer_ids_buf = server_string_list_to_array_pointer(buffer_ids);
		gchar *pretty_buffer_ids = g_strjoinv(",", buffer_ids_buf);
		g_message("Method parameter(s): buffer ID(s) %s, set proxy=%s, URI=%s, ignore_hosts=%s",
			pretty_buffer_ids, mode, proxy_uri, pretty_ignore_hosts);
		g_free(buffer_ids_buf);
		g_free(pretty_buffer_ids);
		g_free(pretty_ignore_hosts);
	}

	int mode_enum = WEBKIT_NETWORK_PROXY_MODE_DEFAULT;
	if (g_strcmp0(mode, "custom") == 0) {
		mode_enum = WEBKIT_NETWORK_PROXY_MODE_CUSTOM;
	} else if (g_strcmp0(mode, "none") == 0) {
		mode_enum = WEBKIT_NETWORK_PROXY_MODE_NO_PROXY;
	}

	while (buffer_ids != NULL) {
		Buffer *buffer = g_hash_table_lookup(state.buffers, buffer_ids->data);
		buffer_set_proxy(buffer, mode_enum, proxy_uri, (const char *const *)ignore_hosts);
		buffer_ids = buffer_ids->next;
	}

	// TODO: Don't free ignore_hosts if we need to store them in the Buffer struct?
	// More elegant way to do this?
	/* g_free(ignore_hosts); */
	g_list_free(buffer_ids);
	return g_variant_new_boolean(TRUE);
}

static GVariant *server_get_proxy(SoupXMLRPCParams *params) {
	GVariant *unwrapped_params = server_unwrap_params(params);
	if (!unwrapped_params) {
		return g_variant_new_boolean(FALSE);
	}
	const char *a_key = NULL;
	g_variant_get(unwrapped_params, "(&s)", &a_key);
	g_message("Method parameter(s): %s", a_key);

	WebKitNetworkProxyMode mode;
	const gchar *proxy_uri = NULL;
	const gchar *const *ignore_hosts = NULL;
	buffer_get_proxy(g_hash_table_lookup(state.buffers, a_key),
		&mode, &proxy_uri, &ignore_hosts);

	char *mode_string = "default";
	if (mode == WEBKIT_NETWORK_PROXY_MODE_CUSTOM) {
		mode_string = "custom";
	} else if (mode == WEBKIT_NETWORK_PROXY_MODE_NO_PROXY) {
		mode_string = "none";
	}

	// TODO: Shouldn't the hosts be a sublist?
	GVariantBuilder builder;
	g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);
	g_variant_builder_add(&builder, "s", mode_string);
	g_variant_builder_add(&builder, "s", proxy_uri);
	while (ignore_hosts != NULL && ignore_hosts[0] != NULL) {
		g_variant_builder_add(&builder, "s", ignore_hosts[0]);
		ignore_hosts++;
	}

	return g_variant_builder_end(&builder);
}

static void server_handler(SoupServer *_server, SoupMessage *msg,
	const char *_path, GHashTable *_query,
	SoupClientContext *_context, gpointer _data) {
	// Log of the request.  This is quite verbose and not so useful, so we comment it out.
	/*
	{
	        const char *name, *value;
	        SoupMessageHeadersIter iter;
	        GString *pretty_message = g_string_new("HTTP request:\n");
	        g_string_append_printf(pretty_message, "%s %s HTTP/1.%d\n", msg->method, _path,
	                soup_message_get_http_version(msg));
	        soup_message_headers_iter_init(&iter, msg->request_headers);
	        while (soup_message_headers_iter_next(&iter, &name, &value)) {
	                g_string_append_printf(pretty_message, "%s: %s\n", name, value);
	        }
	        if (msg->request_body->length == 0) {
	                g_warning("Empty HTTP request");
	                return;
	        }
	        g_string_append_printf(pretty_message, "%s", msg->request_body->data);
	        g_debug("%s", pretty_message->str);
	        g_string_free(pretty_message, TRUE);
	}
	*/

	SoupXMLRPCParams *params = NULL;
	GError *error = NULL;
	char *method_name = soup_xmlrpc_parse_request(msg->request_body->data,
			msg->request_body->length,
			&params,
			&error);
	if (error) {
		g_warning("Malformed XML-RPC request: %s", error->message);
		g_error_free(error);
		return;
	}

	ServerCallback callback = NULL;
	gboolean found = g_hash_table_lookup_extended(state.server_callbacks, method_name,
			NULL, (gpointer *)&callback);
	if (!found) {
		soup_xmlrpc_message_set_fault(msg,
			SOUP_XMLRPC_FAULT_SERVER_ERROR_REQUESTED_METHOD_NOT_FOUND,
			"Unknown method: %s", method_name);
		g_warning("Unknown method: %s", method_name);
		return;
	}
	g_message("Method name: %s", method_name);

	GVariant *result = callback(params);
	soup_xmlrpc_params_free(params);

	// 'result' is floating and soup_xmlrpc_message_set_response will consume it.
	soup_xmlrpc_message_set_response(msg, result, &error);
	if (error) {
		g_warning("Failed to set XML-RPC response: %s", error->message);
		g_error_free(error);
	}
}

void start_server() {
	// TODO: libsoup's examples don't unref the server.  Should we?
	SoupServer *server = soup_server_new(
		SOUP_SERVER_SERVER_HEADER, APPNAME,
		NULL);

	GError *error = NULL;
	soup_server_listen_all(server, state.port, 0, &error);
	if (error) {
		g_printerr("Unable to create server: %s\n", error->message);
		g_error_free(error);
		exit(1);
	}
	g_debug("Starting XMLRPC server");
	soup_server_add_handler(server, NULL, server_handler, NULL, NULL);

	// Initialize global state.
	state.server_callbacks = g_hash_table_new(g_str_hash, g_str_equal);
	state.windows = g_hash_table_new_full(g_str_hash, g_str_equal,
			&g_free, (GDestroyNotify)&window_delete);
	state.buffers = g_hash_table_new_full(g_str_hash, g_str_equal,
			&g_free, (GDestroyNotify)&buffer_delete);

	// Register callbacks.
	g_hash_table_insert(state.server_callbacks, "listMethods", &server_list_methods);

	g_hash_table_insert(state.server_callbacks, "window.make", &server_window_make);
	g_hash_table_insert(state.server_callbacks, "window.set.title", &server_window_set_title);
	g_hash_table_insert(state.server_callbacks, "window.delete", &server_window_delete);
	g_hash_table_insert(state.server_callbacks, "window.active", &server_window_active);
	g_hash_table_insert(state.server_callbacks, "window.exists", &server_window_exists);
	g_hash_table_insert(state.server_callbacks, "window.set.active.buffer", &server_window_set_active_buffer);
	g_hash_table_insert(state.server_callbacks, "window.set.minibuffer.height", &server_window_set_minibuffer_height);
	g_hash_table_insert(state.server_callbacks, "buffer.make", &server_buffer_make);
	g_hash_table_insert(state.server_callbacks, "buffer.delete", &server_buffer_delete);
	g_hash_table_insert(state.server_callbacks, "buffer.load", &server_buffer_load);
	g_hash_table_insert(state.server_callbacks, "buffer.evaluate.javascript", &server_buffer_evaluate);
	g_hash_table_insert(state.server_callbacks, "minibuffer.evaluate.javascript", &server_minibuffer_evaluate);
	g_hash_table_insert(state.server_callbacks, "generate.input.event", &server_generate_input_event);
	g_hash_table_insert(state.server_callbacks, "set.proxy", &server_set_proxy);
	g_hash_table_insert(state.server_callbacks, "get.proxy", &server_get_proxy);
}

void stop_server() {
	g_hash_table_unref(state.windows);
	g_hash_table_unref(state.buffers);
	g_hash_table_unref(state.server_callbacks);
}
