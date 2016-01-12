#include "config.h"

#include "zanata-authorizer.h"

static void
zanata_authorizer_default_init (ZanataAuthorizerInterface *iface)
{
}

G_DEFINE_INTERFACE (ZanataAuthorizer, zanata_authorizer, G_TYPE_OBJECT)

/**
 * zanata_authorizer_get_url:
 * @iface: a #ZanataAuthorizer
 * @domain: a domain name
 *
 * Get the URL to access @domain.
 *
 * Returns: (transfer full) (nullable): URI as a newly allocated string
 */
gchar *
zanata_authorizer_get_url (ZanataAuthorizer *iface,
                           const gchar *domain)
{
  g_return_val_if_fail (ZANATA_IS_AUTHORIZER (iface), NULL);
  return ZANATA_AUTHORIZER_GET_IFACE (iface)->get_url (iface, domain);
}

/**
 * zanata_authorizer_process_call:
 * @iface: a #ZanataAuthorizer
 * @domain: a domain name
 * @call: a #RestProxyCall
 *
 * Adds the necessary authorization to @call.
 *
 * This method modifies @call in place and is thread safe.
 */
void
zanata_authorizer_process_call (ZanataAuthorizer *iface,
                                const gchar *domain,
                                RestProxyCall *call)
{
  g_return_if_fail (ZANATA_IS_AUTHORIZER (iface));
  ZANATA_AUTHORIZER_GET_IFACE (iface)->process_call (iface, domain, call);
}

/**
 * zanata_authorizer_process_message:
 * @iface: a #ZanataAuthorizer
 * @domain: a domain name
 * @message: a #SoupMessage
 *
 * Adds the necessary authorization to @message. The type of @message
 * can be DELETE, GET and POST.
 *
 * This method modifies @message in place and is thread safe.
 */
void
zanata_authorizer_process_message (ZanataAuthorizer *iface,
                                   const gchar *domain,
                                   SoupMessage *message)
{
  g_return_if_fail (ZANATA_IS_AUTHORIZER (iface));
  ZANATA_AUTHORIZER_GET_IFACE (iface)->process_message (iface, domain, message);
}

static void
refresh_authorization_thread_func (GTask *task,
                                   gpointer source_object,
                                   gpointer task_data,
                                   GCancellable *cancellable)
{
  GError *error = NULL;
  gboolean result;

  result = ZANATA_AUTHORIZER_GET_IFACE (source_object)->
    refresh_authorization (ZANATA_AUTHORIZER (source_object),
                           cancellable,
                           &error);

  if (error != NULL)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, result);
}

/**
 * zanata_authorizer_refresh_authorization:
 * @iface: a #ZanataAuthorizer
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback
 * @user_data: a user data
 *
 * Asynchronously forces @iface to refresh any authorization tokens
 * held by it. See zanata_authorizer_refresh_authorization() for the
 * synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can
 * then call zanata_authorizer_refresh_authorization_finish() to get the
 * result of the operation.
 *
 * This method is thread safe.
 */
void
zanata_authorizer_refresh_authorization (ZanataAuthorizer *iface,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
  GTask *task;

  g_return_if_fail (ZANATA_IS_AUTHORIZER (iface));

  task = g_task_new (iface, cancellable, callback, user_data);
  g_task_run_in_thread (task, refresh_authorization_thread_func);
}

/**
 * zanata_authorizer_refresh_authorization_finish:
 * @iface: a #ZanataAuthorizer
 * @res: a #GAsyncResult
 * @error: (nullable): error location
 *
 * Finishes an asynchronous operation started with
 * zanata_authorizer_refresh_authorization().
 *
 * Returns: %TRUE if the authorizer now has a valid token.
 */
gboolean
zanata_authorizer_refresh_authorization_finish (ZanataAuthorizer *iface,
                                                GAsyncResult *res,
                                                GError **error)
{
  g_return_val_if_fail (g_task_is_valid (res, iface), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}
