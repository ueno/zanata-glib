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

ZanataParameter *
zanata_parameter_copy (ZanataParameter *parameter)
{
  ZanataParameter *copy = g_memdup (parameter, sizeof (ZanataParameter));
  copy->name = g_strdup (copy->name);
  copy->value = g_strdup (copy->value);
  return copy;
}

void
zanata_parameter_free (ZanataParameter *parameter)
{
  g_free (parameter->name);
  g_free (parameter->value);
  g_free (parameter);
}

G_DEFINE_BOXED_TYPE (ZanataParameter, zanata_parameter,
                     zanata_parameter_copy, zanata_parameter_free)

/**
 * zanata_session_get_endpoint:
 * @session: a #ZanataSession
 * @mountpoint: the mountpoint
 *
 * Compute the full endpoint URI for @mountpoint.
 * Returns: (transfer full): a #SoupURI
 */
SoupURI *
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
invoke_with_soup_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  SoupSession *session = SOUP_SESSION (source_object);
  GTask *task = G_TASK (user_data);
  GError *error = NULL;
  GInputStream *stream;

  stream = soup_session_send_finish (session, res, &error);
  if (!stream)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_task_return_pointer (task, stream, g_object_unref);
  g_object_unref (task);
}

static void
invoke_with_rest_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  RestProxyCall *call = REST_PROXY_CALL (source_object);
  GTask *task = G_TASK (user_data);
  GError *error = NULL;
  const gchar *payload;
  goffset payload_length;
  GInputStream *stream;

  if (!rest_proxy_call_invoke_finish (call, res, &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  payload = rest_proxy_call_get_payload (call);
  payload_length = rest_proxy_call_get_payload_length (call);
  stream = g_memory_input_stream_new_from_data (payload,
                                                payload_length,
                                                g_free);
  g_task_return_pointer (task, stream, g_object_unref);
  g_object_unref (task);
}

static void
zanata_session_invoke_with_soup (ZanataSession       *session,
                                 const gchar         *method,
                                 SoupURI             *endpoint,
                                 ZanataParameter    **parameters,
                                 const gchar         *request_content_type,
                                 const gchar         *request,
                                 gsize                request_length,
                                 const gchar         *response_content_type,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GTask *task;
  SoupSession *soup_session;
  SoupMessage *soup_message;
  SoupURI *uri;

  task = g_task_new (session, cancellable, callback, user_data);

  soup_session = soup_session_new ();
  uri = soup_uri_copy (endpoint);
  if (parameters != NULL)
    {
      GPtrArray *array;
      gchar **strv;
      gchar *query;

      array = g_ptr_array_new_with_free_func (g_free);
      while (*parameters)
        {
          ZanataParameter *parameter = *parameters;
          gchar *escaped_parameter;

          escaped_parameter =
            g_strdup_printf ("%s=%s",
                             soup_uri_encode (parameter->name, NULL),
                             soup_uri_encode (parameter->value, NULL));
          g_ptr_array_add (array, escaped_parameter);
          parameters++;
        }
      g_ptr_array_add (array, NULL);

      strv = (gchar **) g_ptr_array_free (array, FALSE);
      query = g_strjoinv ("&", strv);
      g_strfreev (strv);

      soup_uri_set_query (uri, query);
    }
  soup_message = soup_message_new_from_uri (method, uri);
  soup_uri_free (uri);

  if (request != NULL)
    soup_message_set_request (soup_message,
                              request_content_type,
                              SOUP_MEMORY_COPY,
                              request,
                              request_length);

  zanata_authorizer_process_message (session->authorizer,
                                     session->domain,
                                     soup_message);

  if (response_content_type != NULL)
    soup_message_headers_append (soup_message->request_headers,
                                 "Accept", response_content_type);

  soup_session_send_async (soup_session, soup_message, NULL,
                           invoke_with_soup_cb, task);

  g_object_unref (soup_message);
  g_object_unref (soup_session);
}

static void
zanata_session_invoke_with_rest (ZanataSession       *session,
                                 const gchar         *method,
                                 SoupURI             *endpoint,
                                 ZanataParameter    **parameters,
                                 const gchar         *request_content_type,
                                 const gchar         *request,
                                 gsize                request_length,
                                 const gchar         *response_content_type,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GTask *task;
  RestProxy *proxy;
  RestProxyCall *call;
  gchar *uri_string;

  task = g_task_new (session, cancellable, callback, user_data);
  uri_string = soup_uri_to_string (endpoint, FALSE);
  proxy = rest_proxy_new (uri_string, FALSE);
  g_free (uri_string);

  call = rest_proxy_new_call (proxy);
  zanata_authorizer_process_call (session->authorizer,
                                  session->domain,
                                  call);
  if (parameters != NULL)
    {
      while (*parameters)
        {
          ZanataParameter *parameter = *parameters;
          rest_proxy_call_add_param (call,
                                     (const gchar *) parameter->name,
                                     (const gchar *) parameter->value);
          parameters++;
        }
    }

  if (response_content_type != NULL)
    rest_proxy_call_add_header (call, "Accept", response_content_type);
  rest_proxy_call_set_method (call, method);
  rest_proxy_call_invoke_async (call, cancellable, invoke_with_rest_cb, task);

  g_object_unref (proxy);
  g_object_unref (call);
}

/**
 * zanata_session_invoke:
 * @session: a #ZanataSession
 * @method: a string
 * @endpoint: a #SoupURI
 * @parameters: (nullable): (array zero-terminated=1) (element-type ZanataParameter): an array of parameters
 * @request_content_type: (nullable): a string
 * @request: (nullable): a string
 * @request_length: the length of request or -1
 * @response_content_type: (nullable): a string
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback
 * @user_data: (nullable): a user data
 *
 * Starts invoking a REST call.  This operation is asynchronous and
 * shall be finished with zanata_session_invoke_finish().
 */
void
zanata_session_invoke (ZanataSession       *session,
                       const gchar         *method,
                       SoupURI             *endpoint,
                       ZanataParameter    **parameters,
                       const gchar         *request_content_type,
                       const gchar         *request,
                       gsize                request_length,
                       const gchar         *response_content_type,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  if (parameters != NULL)
    zanata_session_invoke_with_soup (session,
                                     method,
                                     endpoint,
                                     parameters,
                                     request_content_type,
                                     request,
                                     request_length,
                                     response_content_type,
                                     cancellable,
                                     callback,
                                     user_data);
  else
    zanata_session_invoke_with_rest (session,
                                     method,
                                     endpoint,
                                     parameters,
                                     request_content_type,
                                     request,
                                     request_length,
                                     response_content_type,
                                     cancellable,
                                     callback,
                                     user_data);
}

/**
 * zanata_session_invoke_finish:
 * @session: a #ZanataSession
 * @result: a #GAsyncResult
 * @error: error location
 *
 * Finishes zanata_session_invoke() operation.
 *
 * Returns: (transfer full): a #GInputStream.
 */
GInputStream *
zanata_session_invoke_finish (ZanataSession  *session,
                              GAsyncResult   *result,
                              GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, session), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
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
get_suggestions_load_cb (GObject      *source_object,
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
  ZanataSession *session = ZANATA_SESSION (source_object);
  GTask *task = G_TASK (user_data);
  JsonParser *parser;
  GError *error = NULL;
  GInputStream *stream;

  stream = zanata_session_invoke_finish (session, res, &error);
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
                                      get_suggestions_load_cb,
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
  GPtrArray *array;
  ZanataParameter *parameter;
  ZanataParameter **parameters;
  JsonBuilder *builder;
  JsonGenerator *generator;
  gchar *data;
  gsize data_length;

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
  data = json_generator_to_data (generator, &data_length);
  g_object_unref (generator);

  task = g_task_new (session, cancellable, callback, user_data);
  uri = zanata_session_get_endpoint (session, "/rest/suggestions");

  array =
    g_ptr_array_new_with_free_func ((GDestroyNotify) zanata_parameter_free);

  parameter = g_new0 (ZanataParameter, 1);
  parameter->name = g_strdup ("from");
  parameter->value = g_strdup (from_locale);
  g_ptr_array_add (array, parameter);

  parameter = g_new0 (ZanataParameter, 1);
  parameter->name = g_strdup ("to");
  parameter->value = g_strdup (to_locale);
  g_ptr_array_add (array, parameter);
  g_ptr_array_add (array, NULL);

  parameters = (ZanataParameter **) g_ptr_array_free (array, FALSE);

  zanata_session_invoke (session,
                         "POST",
                         uri,
                         parameters,
                         "application/json",
                         data,
                         data_length,
                         "application/json",
                         cancellable,
                         get_suggestions_invoke_cb,
                         task);

  soup_uri_free (uri);
  while (*parameters)
    {
      zanata_parameter_free (*parameters);
      parameters++;
    }
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
get_projects_load_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  JsonParser *parser = JSON_PARSER (source_object);
  GTask *task = G_TASK (user_data);
  GError *error = NULL;
  GList *projects = NULL;
  JsonNode *node;
  JsonArray *array;

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
  json_array_foreach_element (array, collect_projects, &projects);
  g_task_return_pointer (task, projects, (GDestroyNotify) free_projects);
  g_object_unref (task);
}

static void
get_projects_invoke_cb (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  ZanataSession *session = ZANATA_SESSION (source_object);
  GTask *task = G_TASK (user_data);
  JsonParser *parser;
  GError *error = NULL;
  GInputStream *stream;

  stream = zanata_session_invoke_finish (session, res, &error);
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
                                      get_projects_load_cb,
                                      task);
}

void
zanata_session_get_projects (ZanataSession       *session,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  GTask *task;
  SoupURI *uri;

  task = g_task_new (session, cancellable, callback, user_data);
  uri = zanata_session_get_endpoint (session, "/rest/projects");
  zanata_session_invoke (session,
                         "GET",
                         uri,
                         NULL,
                         "application/json",
                         NULL,
                         -1,
                         "application/json",
                         cancellable,
                         get_projects_invoke_cb,
                         task);
  soup_uri_free (uri);
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
                            "project", project,
                            "id", id,
                            "status", status_value,
                            NULL);
  _zanata_project_add_iteration (project, iteration);
}

static void
get_project_load_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  JsonParser *parser = JSON_PARSER (source_object);
  GTask *task = G_TASK (user_data);
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

  if (!json_parser_load_from_stream_finish (parser, res, &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  node = json_parser_get_root (parser);
  if (json_node_get_node_type (node) != JSON_NODE_OBJECT)
    {
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
      g_task_return_new_error (task,
                               ZANATA_ERROR,
                               ZANATA_ERROR_INVALID_RESPONSE,
                               "\"iterations\" is not given");
      g_object_unref (task);
      return;
    }

  project = g_object_new (ZANATA_TYPE_PROJECT,
                          "session", g_task_get_task_data (task),
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

static void
get_project_invoke_cb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  ZanataSession *session = ZANATA_SESSION (source_object);
  GTask *task = G_TASK (user_data);
  GError *error = NULL;
  GInputStream *stream;
  JsonParser *parser;

  stream = zanata_session_invoke_finish (session, res, &error);
  if (!stream)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_task_set_task_data (task, session, g_object_unref);
  parser = json_parser_new ();
  json_parser_load_from_stream_async (parser,
                                      stream,
                                      g_task_get_cancellable (task),
                                      get_project_load_cb,
                                      task);
}

void
zanata_session_get_project (ZanataSession       *session,
                            const gchar         *project_id,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  GTask *task;
  SoupURI *uri;
  gchar *escaped, *path;

  task = g_task_new (session, cancellable, callback, user_data);

  escaped = soup_uri_encode (project_id, NULL);
  path = g_strdup_printf ("/rest/projects/p/%s", escaped);
  g_free (escaped);

  uri = zanata_session_get_endpoint (session, path);
  zanata_session_invoke (session,
                         "GET",
                         uri,
                         NULL,
                         "application/json",
                         NULL,
                         -1,
                         "application/json",
                         cancellable,
                         get_project_invoke_cb,
                         task);
  soup_uri_free (uri);
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
