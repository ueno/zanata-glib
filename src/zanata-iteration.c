#include "config.h"

#include "zanata-iteration.h"

struct _ZanataIteration
{
  GObject parent_object;
};

G_DEFINE_TYPE (ZanataIteration, zanata_iteration, G_TYPE_OBJECT)

static void
zanata_iteration_class_init (ZanataIterationClass *klass)
{
}

static void
zanata_iteration_init (ZanataIteration *self)
{
}
