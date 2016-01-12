#ifndef ZANATA_ITERATION_H
#define ZANATA_ITERATION_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define ZANATA_TYPE_ITERATION (zanata_iteration_get_type ())
G_DECLARE_FINAL_TYPE (ZanataIteration, zanata_iteration,
                      ZANATA, ITERATION, GObject)

G_END_DECLS

#endif  /* ZANATA_ITERATION_H */
