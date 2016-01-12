#ifndef ZANATA_AUTHORIZER_H
#define ZANATA_AUTHORIZER_H

#include <gio/gio.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <rest/rest-proxy-call.h>

G_BEGIN_DECLS

#define ZANATA_TYPE_AUTHORIZER (zanata_authorizer_get_type ())

G_DECLARE_INTERFACE (ZanataAuthorizer, zanata_authorizer,
                     ZANATA, AUTHORIZER, GObject)

/**
 * ZanataAuthorizerInterface:
 * @parent_iface: The parent interface.
 * @get_url: A method to obtain a URL to access the domain.
 * @process_call: A method to append authorization headers to a
 *   #RestProxyCall.
 * @process_message: A method to append authorization headers to a
 *   #SoupMessage. Types of messages include DELETE, GET and POST.
 * @refresh_authorization: A synchronous method to force a refresh of
 *   any authorization tokens held by the authorizer. It should return
 *   %TRUE on success. An asynchronous version will be defined by
 *   invoking this in a thread.
 *
 * Interface structure for #ZanataAuthorizer. All methods should be
 * thread safe.
 */
struct _ZanataAuthorizerInterface
{
  GTypeInterface parent_iface;

  gchar   *(*get_url)               (ZanataAuthorizer *iface,
                                     const gchar      *domain);
  void     (*process_call)          (ZanataAuthorizer *iface,
                                     const gchar      *domain,
                                     RestProxyCall    *call);
  void     (*process_message)       (ZanataAuthorizer *iface,
                                     const gchar      *domain,
                                     SoupMessage      *message);
  gboolean (*refresh_authorization) (ZanataAuthorizer *iface,
                                     GCancellable     *cancellable,
                                     GError          **error);
};

gchar   *zanata_authorizer_get_url         (ZanataAuthorizer   *iface,
                                            const gchar        *domain);
void     zanata_authorizer_process_call    (ZanataAuthorizer   *iface,
                                            const gchar        *domain,
                                            RestProxyCall      *call);

void     zanata_authorizer_process_message (ZanataAuthorizer   *iface,
                                            const gchar        *domain,
                                            SoupMessage        *message);

void     zanata_authorizer_refresh_authorization
                                           (ZanataAuthorizer   *iface,
                                            GCancellable       *cancellable,
                                            GAsyncReadyCallback callback,
                                            gpointer            user_data);

gboolean zanata_authorizer_refresh_authorization_finish
                                           (ZanataAuthorizer   *iface,
                                            GAsyncResult       *res,
                                            GError            **error);

G_END_DECLS

#endif /* ZANATA_AUTHORIZER_H */
