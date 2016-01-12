#include "config.h"

#include "zanata-suggestion.h"

struct _ZanataSuggestion
{
  GObject parent;
  gchar **source_contents;
  gchar **target_contents;
};

G_DEFINE_TYPE (ZanataSuggestion, zanata_suggestion, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SOURCE_CONTENTS,
  PROP_TARGET_CONTENTS,
  LAST_PROP
};

static GParamSpec *suggestion_pspecs[LAST_PROP] = { 0 };

static void
zanata_suggestion_dispose (GObject *object)
{
  ZanataSuggestion *self = ZANATA_SUGGESTION (object);

  g_clear_pointer (&self->source_contents, g_strfreev);
  g_clear_pointer (&self->target_contents, g_strfreev);

  G_OBJECT_CLASS (zanata_suggestion_parent_class)->dispose (object);
}

static void
zanata_suggestion_set_property (GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  ZanataSuggestion *self = ZANATA_SUGGESTION (object);

  switch (prop_id)
    {
    case PROP_SOURCE_CONTENTS:
      self->source_contents = g_value_dup_boxed (value);
      break;

    case PROP_TARGET_CONTENTS:
      self->target_contents = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
zanata_suggestion_get_property (GObject *object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  ZanataSuggestion *self = ZANATA_SUGGESTION (object);

  switch (prop_id)
    {
    case PROP_SOURCE_CONTENTS:
      g_value_set_boxed (value, self->source_contents);
      break;

    case PROP_TARGET_CONTENTS:
      g_value_set_boxed (value, self->target_contents);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
zanata_suggestion_class_init (ZanataSuggestionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = zanata_suggestion_dispose;
  object_class->set_property = zanata_suggestion_set_property;
  object_class->get_property = zanata_suggestion_get_property;

  suggestion_pspecs[PROP_SOURCE_CONTENTS] =
    g_param_spec_boxed ("source-contents",
                        "Source contents",
                        "Source contents",
                        G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  suggestion_pspecs[PROP_TARGET_CONTENTS] =
    g_param_spec_boxed ("target-contents",
                        "Target contents",
                        "Target contents",
                        G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_properties (object_class, LAST_PROP,
                                     suggestion_pspecs);
}

static void
zanata_suggestion_init (ZanataSuggestion *self)
{
}
