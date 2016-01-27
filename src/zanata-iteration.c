#include "config.h"

#include "zanata-iteration.h"
#include "zanata-session.h"
#include "zanata-enumtypes.h"

struct _ZanataIteration
{
  GObject parent_object;
  ZanataSession *session;
  gchar *id;
  ZanataIterationStatus status;
  gboolean loaded;
  GMutex lock;
};

G_DEFINE_TYPE (ZanataIteration, zanata_iteration, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SESSION,
  PROP_ID,
  PROP_STATUS,
  PROP_LOADED,
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
    case PROP_SESSION:
      self->session = g_value_dup_object (value);
      break;

    case PROP_ID:
      self->id = g_value_dup_string (value);
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

  g_clear_object (&self->session);

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

  iteration_pspecs[PROP_SESSION] =
    g_param_spec_object ("session",
                         "Session",
                         "Session",
                         ZANATA_TYPE_SESSION,
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
  iteration_pspecs[PROP_LOADED] =
    g_param_spec_boolean ("loaded",
                          "Loaded",
                          "Loaded",
                          FALSE,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);
  g_object_class_install_properties (object_class, LAST_PROP,
                                     iteration_pspecs);
}

static void
zanata_iteration_init (ZanataIteration *self)
{
}
