#include "config.h"

#include "zanata-iteration.h"
#include "zanata-session.h"
#include "zanata-enumtypes.h"
#include <json-glib/json-glib.h>

struct _ZanataIteration
{
  GObject parent_object;
  ZanataProject *project;
  gchar *id;
  ZanataIterationStatus status;
};

G_DEFINE_TYPE (ZanataIteration, zanata_iteration, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PROJECT,
  PROP_ID,
  PROP_STATUS,
  LAST_PROP
};

static GParamSpec *iteration_pspecs[LAST_PROP] = { 0 };

static void
zanata_iteration_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ZanataIteration *self = ZANATA_ITERATION (object);

  switch (prop_id)
    {
    case PROP_PROJECT:
      self->project = g_value_dup_object (value);
      break;

    case PROP_ID:
      self->id = g_value_dup_string (value);
      break;

    case PROP_STATUS:
      self->status = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
zanata_iteration_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  ZanataIteration *self = ZANATA_ITERATION (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    case PROP_STATUS:
      g_value_set_enum (value, self->status);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
zanata_iteration_dispose (GObject *object)
{
  ZanataIteration *self = ZANATA_ITERATION (object);

  g_clear_object (&self->project);

  G_OBJECT_CLASS (zanata_iteration_parent_class)->dispose (object);
}

static void
zanata_iteration_finalize (GObject *object)
{
  ZanataIteration *self = ZANATA_ITERATION (object);

  g_free (self->id);

  G_OBJECT_CLASS (zanata_iteration_parent_class)->finalize (object);
}

static void
zanata_iteration_class_init (ZanataIterationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->set_property = zanata_iteration_set_property;
  object_class->get_property = zanata_iteration_get_property;
  object_class->dispose = zanata_iteration_dispose;
  object_class->finalize = zanata_iteration_finalize;

  iteration_pspecs[PROP_PROJECT] =
    g_param_spec_object ("project",
                         "Project",
                         "Project",
                         ZANATA_TYPE_PROJECT,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);
  iteration_pspecs[PROP_ID] =
    g_param_spec_string ("id",
                         "ID",
                         "ID",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  iteration_pspecs[PROP_STATUS] =
    g_param_spec_enum ("status",
                       "Status",
                       "Status",
                       ZANATA_TYPE_ITERATION_STATUS,
                       ZANATA_ITERATION_STATUS_UNKNOWN,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_properties (object_class, LAST_PROP,
                                     iteration_pspecs);
}

static void
zanata_iteration_init (ZanataIteration *self)
{
}

static void
get_translated_documentation_invoke_cb (GObject      *source_object,
                                        GAsyncResult *res,
                                        gpointer      user_data)
{
  ZanataSession *session = ZANATA_SESSION (source_object);
  GTask *task = G_TASK (user_data);
  GError *error = NULL;
  GInputStream *stream;

  stream = zanata_session_invoke_finish (session, res, &error);
  if (!stream)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_task_return_pointer (task, stream, g_object_unref);
  g_object_unref (task);
}

void
zanata_iteration_get_translated_documentation (ZanataIteration     *iteration,
                                               const gchar         *domain,
                                               const gchar         *locale,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  GTask *task;
  SoupURI *uri;
  gchar *project_id, *iteration_id;
  gchar *escaped_project_id, *escaped_iteration_id;
  gchar *escaped_domain, *escaped_locale, *path;
  GPtrArray *array;
  ZanataParameter *parameter;
  ZanataParameter **parameters;
  ZanataSession *session;

  task = g_task_new (iteration, cancellable, callback, user_data);

  g_object_get (iteration->project, "id", &project_id, NULL);
  escaped_project_id = soup_uri_encode (project_id, NULL);
  escaped_iteration_id = soup_uri_encode (iteration->id, NULL);
  escaped_domain = soup_uri_encode (domain, NULL);
  escaped_locale = soup_uri_encode (locale, NULL);
  path = g_strdup_printf ("/rest/projects/p/%s/iterations/i/%s/r/%s/translations/%s",
                          escaped_project_id, escaped_iteration_id,
                          escaped_domain, escaped_locale);
  g_free (escaped_project_id);
  g_free (escaped_iteration_id);
  g_free (escaped_domain);
  g_free (escaped_locale);

  array =
    g_ptr_array_new_with_free_func ((GDestroyNotify) zanata_parameter_free);

  parameter = g_new0 (ZanataParameter, 1);
  parameter->name = g_strdup ("ext");
  parameter->value = g_strdup ("gettext");
  g_ptr_array_add (array, parameter);

#if 0
  parameter = g_new0 (ZanataParameter, 1);
  parameter->name = g_strdup ("ext");
  parameter->value = g_strdup ("comment");
  g_ptr_array_add (array, parameter);
#endif
  g_ptr_array_add (array, NULL);

  parameters = (ZanataParameter **) g_ptr_array_free (array, FALSE);

  g_object_get (iteration->project, "session", &session, NULL);
  uri = zanata_session_get_endpoint (session, path);
  g_free (path);

  zanata_session_invoke (session,
                         "GET",
                         uri,
                         NULL,
                         "application/json",
                         NULL,
                         -1,
                         "application/json",
                         cancellable,
                         get_translated_documentation_invoke_cb,
                         task);
  soup_uri_free (uri);
  g_object_unref (session);
  while (*parameters)
    {
      zanata_parameter_free (*parameters);
      parameters++;
    }
}

/**
 * zanata_iteration_get_translated_documentation_finish:
 * @iteration: a #ZanataIteration
 * @result: a #GAsyncResult
 * @error: error location
 *
 * Returns: (transfer full): a #GInputStream
 */
GInputStream *
zanata_iteration_get_translated_documentation_finish (ZanataIteration  *iteration,
                                                      GAsyncResult     *result,
                                                      GError          **error)
{
  g_return_val_if_fail (g_task_is_valid (result, iteration), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
