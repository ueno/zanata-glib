#include "config.h"

#include "zanata-project.h"
#include "zanata-session.h"
#include "zanata-enumtypes.h"

struct _ZanataProject
{
  GObject parent;
  ZanataSession *session;
  gchar *id;
  gchar *name;
  gchar *description;
  ZanataProjectStatus status;
  GList *iterations;
  gboolean loaded;
  GMutex lock;
};

G_DEFINE_TYPE (ZanataProject, zanata_project, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SESSION,
  PROP_ID,
  PROP_NAME,
  PROP_DESCRIPTION,
  PROP_STATUS,
  PROP_LOADED,
  LAST_PROP
};

static GParamSpec *project_pspecs[LAST_PROP] = { 0 };

static void
zanata_project_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ZanataProject *self = ZANATA_PROJECT(object);

  switch (prop_id)
    {
    case PROP_SESSION:
      self->session = g_value_dup_object (value);
      break;

    case PROP_ID:
      self->id = g_value_dup_string (value);
      break;

    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    case PROP_DESCRIPTION:
      self->description = g_value_dup_string (value);
      break;

    case PROP_STATUS:
      self->status = g_value_get_enum (value);
      break;

    case PROP_LOADED:
      self->loaded = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
zanata_project_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ZanataProject *self = ZANATA_PROJECT (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_DESCRIPTION:
      g_value_set_string (value, self->description);
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
zanata_project_dispose (GObject *object)
{
  ZanataProject *self = ZANATA_PROJECT (object);

  g_clear_object (&self->session);

  G_OBJECT_CLASS (zanata_project_parent_class)->dispose (object);
}

static void
zanata_project_finalize (GObject *object)
{
  ZanataProject *self = ZANATA_PROJECT (object);

  g_mutex_clear (&self->lock);
  g_list_free_full (self->iterations, g_object_unref);
  g_free (self->id);
  g_free (self->name);
  g_free (self->description);

  G_OBJECT_CLASS (zanata_project_parent_class)->finalize (object);
}

static void
zanata_project_class_init (ZanataProjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = zanata_project_set_property;
  object_class->get_property = zanata_project_get_property;
  object_class->dispose = zanata_project_dispose;
  object_class->finalize = zanata_project_finalize;

  project_pspecs[PROP_SESSION] =
    g_param_spec_object ("session",
                         "Session",
                         "Session",
                         ZANATA_TYPE_SESSION,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);
  project_pspecs[PROP_ID] =
    g_param_spec_string ("id",
                         "ID",
                         "ID",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  project_pspecs[PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Name",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  project_pspecs[PROP_DESCRIPTION] =
    g_param_spec_string ("description",
                         "Description",
                         "Description",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  project_pspecs[PROP_STATUS] =
    g_param_spec_enum ("status",
                       "Status",
                       "Status",
                       ZANATA_TYPE_PROJECT_STATUS,
                       ZANATA_PROJECT_STATUS_UNKNOWN,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  project_pspecs[PROP_LOADED] =
    g_param_spec_boolean ("loaded",
                          "Loaded",
                          "Loaded",
                          FALSE,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);
  g_object_class_install_properties (object_class, LAST_PROP,
                                     project_pspecs);
}

static void
zanata_project_init (ZanataProject *self)
{
  g_mutex_init (&self->lock);
}

static void
collect_iterations (gpointer data,
                    gpointer user_data)
{
  ZanataIteration *iteration = data;
  GList **iterations = user_data;
  *iterations = g_list_append (*iterations, g_object_ref (iteration));
}

static void
get_project_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
  ZanataSession *session = ZANATA_SESSION (source_object);
  GTask *task = G_TASK (user_data);
  ZanataProject *project = g_task_get_source_object (task);
  ZanataProject *loaded;
  GError *error = NULL;

  loaded = zanata_session_get_project_finish (session, res, &error);
  if (!loaded)
    {
      g_mutex_unlock (&project->lock);
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  project->iterations = NULL;
  g_list_foreach (loaded->iterations, collect_iterations, &project->iterations);
  g_object_unref (loaded);
  g_mutex_unlock (&project->lock);

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

void
zanata_project_get_iterations (ZanataProject       *project,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GTask *task;

  task = g_task_new (project, cancellable, callback, user_data);

  g_mutex_lock (&project->lock);
  if (!project->loaded)
    {
      zanata_session_get_project (project->session,
                                  project->id,
                                  cancellable,
                                  get_project_cb,
                                  task);
    }
  else
    {
      g_mutex_unlock (&project->lock);
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
    }
}

/**
 * zanata_project_get_iterations_finish:
 * @project: a #ZanataProject
 * @result: a #GAsyncResult
 * @error: a #GError
 *
 * Finishes zanata_project_get_iterations() operation.
 *
 * Returns: (transfer full) (element-type ZanataIteration): a list of
 * #ZanataIteration
 */
GList *
zanata_project_get_iterations_finish (ZanataProject  *project,
                                      GAsyncResult   *result,
                                      GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, project), NULL);

  if (g_task_propagate_boolean (G_TASK (result), error))
    return project->iterations;

  return NULL;
}

void
zanata_project_add_iteration (ZanataProject   *project,
                              ZanataIteration *iteration)
{
  project->iterations = g_list_append (project->iterations, iteration);
}
