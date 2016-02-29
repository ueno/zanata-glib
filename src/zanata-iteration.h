#ifndef ZANATA_ITERATION_H
#define ZANATA_ITERATION_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define ZANATA_TYPE_ITERATION (zanata_iteration_get_type ())
G_DECLARE_FINAL_TYPE (ZanataIteration, zanata_iteration,
                      ZANATA, ITERATION, GObject)

void          zanata_iteration_get_translated_documentation (ZanataIteration     *iteration,
                                                             const gchar         *domain,
                                                             const gchar         *locale,
                                                             GCancellable        *cancellable,
                                                             GAsyncReadyCallback  callback,
                                                             gpointer             user_data);

GInputStream *zanata_iteration_get_translated_documentation_finish
                                                            (ZanataIteration     *iteration,
                                                             GAsyncResult        *result,
                                                             GError             **error);

G_END_DECLS

#endif  /* ZANATA_ITERATION_H */
