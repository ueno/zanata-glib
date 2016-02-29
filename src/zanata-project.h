#ifndef ZANATA_PROJECT_H
#define ZANATA_PROJECT_H

#include "zanata-iteration.h"

G_BEGIN_DECLS

#define ZANATA_TYPE_PROJECT (zanata_project_get_type ())

G_DECLARE_FINAL_TYPE (ZanataProject, zanata_project,
                      ZANATA, PROJECT, GObject)

void   zanata_project_get_iterations        (ZanataProject       *project,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data);
void   _zanata_project_add_iteration        (ZanataProject       *project,
                                             ZanataIteration     *iteration);
GList *zanata_project_get_iterations_finish (ZanataProject       *project,
                                             GAsyncResult        *result,
                                             GError             **error);

G_END_DECLS

#endif  /* ZANATA_PROJECT_H */
