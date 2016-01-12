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
  ZanataProjectStatus status;
};

G_DEFINE_TYPE (ZanataProject, zanata_project, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SESSION,
  PROP_ID,
  PROP_NAME,
  PROP_STATUS,
  LAST_PROP
};

static GParamSpec *project_pspecs[LAST_PROP] = { 0 };

static void
zanata_project_set_property (GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
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

    case PROP_STATUS:
      self->status = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
zanata_project_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
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
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (zanata_project_parent_class)->dispose (object);
}

static void
zanata_project_class_init (ZanataProjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = zanata_project_set_property;
  object_class->get_property = zanata_project_get_property;
  object_class->dispose = zanata_project_dispose;

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
  project_pspecs[PROP_STATUS] =
    g_param_spec_enum ("status",
                       "Status",
                       "Status",
                       ZANATA_TYPE_PROJECT_STATUS,
                       ZANATA_PROJECT_STATUS_UNKNOWN,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_properties (object_class, LAST_PROP,
                                     project_pspecs);
}

static void
zanata_project_init (ZanataProject *self)
{
}
