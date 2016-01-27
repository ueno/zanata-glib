#include "config.h"

#include "zanata-session.h"
#include "zanata-suggestion.h"
#include "zanata-enums.h"
#include "zanata-enumtypes.h"

#include <json-glib/json-glib.h>
#include <rest/rest-proxy.h>
#include <string.h>

G_DEFINE_QUARK (zanata-error-quark, zanata_error)

struct _ZanataSession
{
  GObject parent_object;
  ZanataAuthorizer *authorizer;
  gchar *domain;
};

G_DEFINE_TYPE (ZanataSession, zanata_session, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_AUTHORIZER,
  PROP_DOMAIN,
  LAST_PROP
};

static GParamSpec *session_pspecs[LAST_PROP] = { 0 };

static void
zanata_session_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ZanataSession *self = ZANATA_SESSION (object);

  switch (prop_id)
    {
    case PROP_AUTHORIZER:
      self->authorizer = g_value_dup_object (value);
      break;

    case PROP_DOMAIN:
      self->domain = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
zanata_session_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ZanataSession *self = ZANATA_SESSION (object);

  switch (prop_id)
    {
    case PROP_AUTHORIZER:
      g_value_set_object (value, self->authorizer);
      break;

    case PROP_DOMAIN:
      g_value_set_string (value, self->domain);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
zanata_session_dispose (GObject *object)
{
  ZanataSession *self = ZANATA_SESSION (object);

  g_clear_object (&self->authorizer);

  G_OBJECT_CLASS (zanata_session_parent_class)->dispose (object);
}

static void
zanata_session_class_init (ZanataSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = zanata_session_dispose;
  object_class->set_property = zanata_session_set_property;
  object_class->get_property = zanata_session_get_property;

  session_pspecs[PROP_AUTHORIZER] =
    g_param_spec_object ("authorizer",
                         "Authorizer",
                         "The authorizer used to create a session.",
                         ZANATA_TYPE_AUTHORIZER,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  session_pspecs[PROP_DOMAIN] =
    g_param_spec_string ("domain",
                         "Authorization domain",
                         "The authorizion domain used to create a session.",
                         "",
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_properties (object_class, LAST_PROP,
                                     session_pspecs);
}

static void
zanata_session_init (ZanataSession *self)
{
}

ZanataSession *
zanata_session_new (ZanataAuthorizer *authorizer,
                    const gchar      *domain)
{
  return g_object_new (ZANATA_TYPE_SESSION,
                       "authorizer", authorizer,
                       "domain", domain,
                       NULL);
}

static SoupURI *
zanata_session_get_endpoint (ZanataSession *session,
                             const gchar   *mountpoint)
{
  SoupURI *result;
  const gchar *orig_path;

  result =
    soup_uri_new (zanata_authorizer_get_url (session->authorizer,
                                             session->domain));
  orig_path = soup_uri_get_path (result);
  if (orig_path == NULL || *orig_path == '\0')
    soup_uri_set_path (result, mountpoint);
  else
    {
      gchar *copy_path = g_strdup (orig_path);
      gchar *path, *p = copy_path + strlen (copy_path) - 1;
      if (*p == '/')
        *p = '\0';
      path = g_strdup_printf ("%s%s", copy_path, mountpoint);
      g_free (copy_path);
      soup_uri_set_path (result, path);
      g_free (path);
    }
  return result;
}

static void
collect_suggestions (JsonArray *array,
                     guint      index_,
                     JsonNode  *element_node,
                     gpointer   user_data)
{
  GList **suggestions = user_data;
  JsonObject *object;
  JsonArray *value_array;
  ZanataSuggestion *suggestion;
  gchar **source_contents;
  gchar **target_contents;
  guint length, i, j;

  if (json_node_get_node_type (element_node) != JSON_NODE_OBJECT)
    return;

  object = json_node_get_object (element_node);
  value_array = json_object_get_array_member (object, "sourceContents");
  if (!value_array)
    return;

  length = json_array_get_length (value_array);
  source_contents = g_new0 (gchar *, length);
  for (i = 0, j = 0; i < length; i++)
    {
      const gchar *value = json_array_get_string_element (value_array, i);
      if (value)
        source_contents[j++] = g_strdup (value);
    }

  value_array = json_object_get_array_member (object, "targetContents");
  if (!value_array)
    return;

  length = json_array_get_length (value_array);
  target_contents = g_new0 (gchar *, length);
  for (i = 0, j = 0; i < length; i++)
    {
      const gchar *value = json_array_get_string_element (value_array, i);
      if (value)
        target_contents[j++] = g_strdup (value);
    }

  suggestion = g_object_new (ZANATA_TYPE_SUGGESTION,
                             "source-contents", source_contents,
                             "target-contents", target_contents,
                             NULL);
  *suggestions = g_list_append (*suggestions, suggestion);
}

static void
free_suggestions (GList *suggestions)
{
  g_list_free_full (suggestions, g_object_unref);
}

static void
get_suggestions_load_from_stream_cb (GObject      *source_object,
                                     GAsyncResult *res,
                                     gpointer      user_data)
{
  JsonParser *parser = JSON_PARSER (source_object);
  GTask *task = G_TASK (user_data);
  GError *error = NULL;
  JsonNode *node;
  JsonArray *array;
  GList *suggestions = NULL;

  if (!json_parser_load_from_stream_finish (parser, res, &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  node = json_parser_get_root (parser);
  if (json_node_get_node_type (node) != JSON_NODE_ARRAY)
    {
      g_task_return_new_error (task,
                               ZANATA_ERROR,
                               ZANATA_ERROR_INVALID_RESPONSE,
                               "root element is not an array");
      g_object_unref (task);
      return;
    }

  array = json_node_get_array (node);
  json_array_foreach_element (array, collect_suggestions, &suggestions);
  g_task_return_pointer (task, suggestions, (GDestroyNotify) free_suggestions);
  g_object_unref (task);
}

static void
get_suggestions_invoke_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  SoupSession *session = SOUP_SESSION (source_object);
  GTask *task = G_TASK (user_data);
  JsonParser *parser;
  GError *error = NULL;
  GInputStream *stream;

  stream = soup_session_send_finish (session, res, &error);
  if (!stream)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  parser = json_parser_new ();
  json_parser_load_from_stream_async (parser,
                                      stream,
                                      g_task_get_cancellable (task),
                                      get_suggestions_load_from_stream_cb,
                                      task);
}

/**
 * zanata_session_get_suggestions:
 * @session: a #ZanataSession
 * @query: (array zero-terminated=1) (element-type utf8): an array of
 *   query strings
 * @from_locale: a locale id of source contents
 * @to_locale: a locale id of target contents
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback
 * @user_data: (nullable): a user data
 *
 * Starts retrieving suggestions matching @query.  This operation is
 * asynchronous and shall be finished with
 * zanata_session_get_suggestions_finish().
 */
void
zanata_session_get_suggestions (ZanataSession       *session,
                                const gchar * const *query,
                                const gchar         *from_locale,
                                const gchar         *to_locale,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GTask *task;
  SoupSession *soup_session;
  SoupMessage *soup_message;
  SoupURI *uri;
  JsonBuilder *builder;
  JsonGenerator *generator;
  gchar *query_data;
  gsize query_data_length;

  builder = json_builder_new ();
  json_builder_begin_array (builder);
  while (*query)
    {
      json_builder_add_string_value (builder, *query);
      query++;
    }
  json_builder_end_array (builder);

  generator = json_generator_new ();
  json_generator_set_root (generator, json_builder_get_root (builder));
  g_object_unref (builder);
  query_data = json_generator_to_data (generator, &query_data_length);
  g_object_unref (generator);

  task = g_task_new (session, cancellable, callback, user_data);
  soup_session = soup_session_new ();
  uri = zanata_session_get_endpoint (session, "/rest/suggestions");

  soup_uri_set_query_from_fields (uri,
                                  "from", from_locale,
                                  "to", to_locale,
                                  NULL);
  soup_message = soup_message_new_from_uri ("POST", uri);
  soup_message_set_request (soup_message, "application/json", SOUP_MEMORY_COPY,
                            query_data, query_data_length);
  zanata_authorizer_process_message (session->authorizer,
                                     session->domain,
                                     soup_message);
  soup_message_headers_append (soup_message->request_headers,
                               "Accept", "application/json");
  soup_session_send_async (soup_session, soup_message, NULL,
                           get_suggestions_invoke_cb, task);
  g_object_unref (soup_message);
  g_object_unref (soup_session);
}

/**
 * zanata_session_get_suggestions_finish:
 * @session: a #ZanataSession
 * @result: a #GAsyncResult
 * @error: error location
 *
 * Finishes zanata_session_get_suggestions() operation.
 *
 * Returns: (transfer full) (element-type ZanataSuggestion): a list of
 * suggestions
 */
GList *
zanata_session_get_suggestions_finish (ZanataSession  *session,
                                       GAsyncResult   *result,
                                       GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, session), NULL);
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
collect_projects (JsonArray *array,
                  guint      index_,
                  JsonNode  *element_node,
                  gpointer   user_data)
{
  GList **projects = user_data;
  JsonObject *object;
  const gchar *id, *name, *status;
  gchar *status_nick;
  ZanataProjectStatus status_value;
  ZanataProject *project;
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  if (json_node_get_node_type (element_node) != JSON_NODE_OBJECT)
    return;

  object = json_node_get_object (element_node);
  id = json_object_get_string_member (object, "id");
  if (!id)
    return;

  name = json_object_get_string_member (object, "name");
  if (!name)
    return;

  status = json_object_get_string_member (object, "status");
  if (!status)
    return;

  enum_class = g_type_class_ref (ZANATA_TYPE_PROJECT_STATUS);
  status_nick = g_ascii_strdown (status, -1);
  enum_value = g_enum_get_value_by_nick (enum_class, status_nick);
  g_free (status_nick);
  if (enum_value)
    status_value = enum_value->value;
  else
    status_value = ZANATA_PROJECT_STATUS_UNKNOWN;

  project = g_object_new (ZANATA_TYPE_PROJECT,
                          "id", id,
                          "name", name,
                          "status", status_value,
                          "loaded", FALSE,
                          NULL);
  g_type_class_unref (enum_class);
  *projects = g_list_append (*projects, project);
}

static void
free_projects (GList *projects)
{
  g_list_free_full (projects, g_object_unref);
}

static void
get_projects_invoke_cb (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  RestProxyCall *call = REST_PROXY_CALL (source_object);
  GTask *task = G_TASK (user_data);
  JsonParser *parser;
  GError *error = NULL;
  GList *projects = NULL;
  JsonNode *node;
  JsonArray *array;

  if (!rest_proxy_call_invoke_finish (call, res, &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser,
                                   rest_proxy_call_get_payload (call),
                                   rest_proxy_call_get_payload_length (call),
                                   &error))
    {
      g_object_unref (parser);
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  node = json_parser_get_root (parser);
  if (json_node_get_node_type (node) != JSON_NODE_ARRAY)
    {
      g_task_return_new_error (task,
                               ZANATA_ERROR,
                               ZANATA_ERROR_INVALID_RESPONSE,
                               "root element is not an array");
      g_object_unref (task);
      return;
    }

  array = json_node_get_array (node);
  json_array_foreach_element (array, collect_projects, &projects);
  g_task_return_pointer (task, projects, (GDestroyNotify) free_projects);
  g_object_unref (task);
}

void
zanata_session_get_projects (ZanataSession       *session,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  GTask *task;
  RestProxy *proxy;
  RestProxyCall *call;
  SoupURI *uri;
  gchar *uri_string;

  task = g_task_new (session, cancellable, callback, user_data);
  uri = zanata_session_get_endpoint (session, "/rest/projects");
  uri_string = soup_uri_to_string (uri, FALSE);
  proxy = rest_proxy_new (uri_string, FALSE);
  g_free (uri_string);
  call = rest_proxy_new_call (proxy);
  zanata_authorizer_process_call (session->authorizer,
                                  session->domain,
                                  call);
  rest_proxy_call_add_header (call, "Accept", "application/json");
  rest_proxy_call_set_method (call, "GET");
  rest_proxy_call_invoke_async (call,
                                cancellable,
                                get_projects_invoke_cb,
                                task);
  g_object_unref (proxy);
  g_object_unref (call);
}

/**
 * zanata_session_get_projects_finish:
 * @session: a #ZanataSession
 * @result: a #GAsyncResult
 * @error: error location
 *
 * Finishes zanata_session_get_projects() operation.
 *
 * Returns: (transfer full) (element-type ZanataProject): a list of
 * #ZanataProject
 */
GList *
zanata_session_get_projects_finish (ZanataSession  *session,
                                    GAsyncResult   *result,
                                    GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, session), NULL);
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
collect_iterations (JsonArray *array,
                    guint      index_,
                    JsonNode  *element_node,
                    gpointer   user_data)
{
  ZanataProject *project = user_data;
  JsonObject *object;
  const gchar *id, *status;
  gchar *status_nick;
  ZanataProjectStatus status_value;
  ZanataIteration *iteration;
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  if (json_node_get_node_type (element_node) != JSON_NODE_OBJECT)
    return;

  object = json_node_get_object (element_node);
  id = json_object_get_string_member (object, "id");
  if (!id)
    return;

  /* FIXME: "links" */

  status = json_object_get_string_member (object, "status");
  if (!status)
    return;

  enum_class = g_type_class_ref (ZANATA_TYPE_ITERATION_STATUS);
  status_nick = g_ascii_strdown (status, -1);
  enum_value = g_enum_get_value_by_nick (enum_class, status_nick);
  g_free (status_nick);
  if (enum_value)
    status_value = enum_value->value;
  else
    status_value = ZANATA_ITERATION_STATUS_UNKNOWN;

  iteration = g_object_new (ZANATA_TYPE_ITERATION,
                            "id", id,
                            "status", status_value,
                            "loaded", FALSE,
                            NULL);
  zanata_project_add_iteration (project, iteration);
}

static void
get_project_invoke_cb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  RestProxyCall *call = REST_PROXY_CALL (source_object);
  GTask *task = G_TASK (user_data);
  JsonParser *parser;
  GError *error = NULL;
  JsonNode *node;
  JsonObject *object;
  JsonArray *array;
  const gchar *id, *name, *status;
  gchar *status_nick;
  ZanataProjectStatus status_value;
  ZanataProject *project;
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  if (!rest_proxy_call_invoke_finish (call, res, &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser,
                                   rest_proxy_call_get_payload (call),
                                   rest_proxy_call_get_payload_length (call),
                                   &error))
    {
      g_object_unref (parser);
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  node = json_parser_get_root (parser);
  if (json_node_get_node_type (node) != JSON_NODE_OBJECT)
    {
      g_object_unref (parser);
      g_task_return_new_error (task,
                               ZANATA_ERROR,
                               ZANATA_ERROR_INVALID_RESPONSE,
                               "root element is not an array");
      g_object_unref (task);
      return;
    }

  object = json_node_get_object (node);
  id = json_object_get_string_member (object, "id");
  if (!id)
    {
      g_object_unref (parser);
      g_task_return_new_error (task,
                               ZANATA_ERROR,
                               ZANATA_ERROR_INVALID_RESPONSE,
                               "\"id\" is not given");
      g_object_unref (task);
      return;
    }
  name = json_object_get_string_member (object, "name");
  if (!name)
    {
      g_object_unref (parser);
      g_task_return_new_error (task,
                               ZANATA_ERROR,
                               ZANATA_ERROR_INVALID_RESPONSE,
                               "\"name\" is not given");
      g_object_unref (task);
      return;
    }
  status = json_object_get_string_member (object, "status");
  if (!status)
    {
      g_object_unref (parser);
      g_task_return_new_error (task,
                               ZANATA_ERROR,
                               ZANATA_ERROR_INVALID_RESPONSE,
                               "\"status\" is not given");
      g_object_unref (task);
      return;
    }
  enum_class = g_type_class_ref (ZANATA_TYPE_PROJECT_STATUS);
  status_nick = g_ascii_strdown (status, -1);
  enum_value = g_enum_get_value_by_nick (enum_class, status_nick);
  g_free (status_nick);
  if (enum_value)
    status_value = enum_value->value;
  else
    status_value = ZANATA_PROJECT_STATUS_UNKNOWN;

  array = json_object_get_array_member (object, "iterations");
  if (!array)
    {
      g_object_unref (parser);
      g_task_return_new_error (task,
                               ZANATA_ERROR,
                               ZANATA_ERROR_INVALID_RESPONSE,
                               "\"iterations\" is not given");
      g_object_unref (task);
      return;
    }

  project = g_object_new (ZANATA_TYPE_PROJECT,
                          "id", id,
                          "name", name,
                          "status", status_value,
                          "loaded", TRUE,
                          NULL);
  g_type_class_unref (enum_class);

  json_array_foreach_element (array, collect_iterations, project);

  g_task_return_pointer (task, project, g_object_unref);
  g_object_unref (task);
}

void
zanata_session_get_project (ZanataSession       *session,
                            const gchar         *project_id,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  GTask *task;
  RestProxy *proxy;
  RestProxyCall *call;
  SoupURI *uri;
  gchar *uri_string, *uri_string_with_binding;

  task = g_task_new (session, cancellable, callback, user_data);
  uri = zanata_session_get_endpoint (session, "/rest/projects/p/");
  uri_string = soup_uri_to_string (uri, FALSE);
  uri_string_with_binding = g_strdup_printf ("%s%%s", uri_string);
  g_free (uri_string);
  proxy = rest_proxy_new (uri_string_with_binding, TRUE);
  g_free (uri_string_with_binding);
  rest_proxy_bind (proxy, project_id);
  call = rest_proxy_new_call (proxy);
  zanata_authorizer_process_call (session->authorizer,
                                  session->domain,
                                  call);
  rest_proxy_call_add_header (call, "Accept", "application/json");
  rest_proxy_call_set_method (call, "GET");
  rest_proxy_call_invoke_async (call, cancellable, get_project_invoke_cb, task);
  g_object_unref (proxy);
  g_object_unref (call);
}

/**
 * zanata_session_get_project_finish:
 * @session: a #ZanataSession
 * @result: a #GAsyncResult
 * @error: error location
 *
 * Finishes zanata_session_get_project() operation.
 *
 * Returns: (transfer full): a #ZanataProject
 */
ZanataProject *
zanata_session_get_project_finish (ZanataSession  *session,
                                   GAsyncResult   *result,
                                   GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, session), NULL);
  return g_task_propagate_pointer (G_TASK (result), error);
}
