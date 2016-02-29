#ifndef ZANATA_SESSION_H
#define ZANATA_SESSION_H

#include <glib-object.h>
#include "zanata-authorizer.h"
#include "zanata-project.h"

G_BEGIN_DECLS

#define ZANATA_ERROR (zanata_error_quark ())
GQuark zanata_error_quark (void);

#define ZANATA_TYPE_SESSION (zanata_session_get_type ())

G_DECLARE_FINAL_TYPE (ZanataSession, zanata_session,
                      ZANATA, SESSION, GObject)

struct _ZanataParameter
{
  gchar *name;
  gchar *value;
};

typedef struct _ZanataParameter ZanataParameter;

ZanataParameter *zanata_parameter_copy (ZanataParameter *parameter);
void zanata_parameter_free (ZanataParameter *parameter);

ZanataSession *zanata_session_new (ZanataAuthorizer    *authorizer,
                                   const gchar         *domain);

SoupURI       *zanata_session_get_endpoint
                                  (ZanataSession       *session,
                                   const gchar         *mountpoint);
void           zanata_session_invoke
                                  (ZanataSession       *session,
                                   const gchar         *method,
                                   SoupURI             *endpoint,
                                   ZanataParameter    **parameters,
                                   const gchar         *request_content_type,
                                   const gchar         *request,
                                   gsize                request_length,
                                   const gchar         *response_content_type,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data);
GInputStream  *zanata_session_invoke_finish
                                  (ZanataSession       *session,
                                   GAsyncResult        *result,
                                   GError             **error);
void           zanata_session_get_suggestions
                                  (ZanataSession       *session,
                                   const gchar * const *query,
                                   const gchar         *from_locale,
                                   const gchar         *to_locale,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data);

GList         *zanata_session_get_suggestions_finish
                                  (ZanataSession       *session,
                                   GAsyncResult        *result,
                                   GError             **error);

void           zanata_session_get_projects
                                  (ZanataSession       *session,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data);

GList         *zanata_session_get_projects_finish
                                  (ZanataSession       *session,
                                   GAsyncResult        *result,
                                   GError             **error);

void           zanata_session_get_project
                                  (ZanataSession       *session,
                                   const gchar         *project_id,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data);

ZanataProject *zanata_session_get_project_finish
                                  (ZanataSession       *session,
                                   GAsyncResult        *result,
                                   GError             **error);

G_END_DECLS

#endif  /* ZANATA_SESSION_H */
