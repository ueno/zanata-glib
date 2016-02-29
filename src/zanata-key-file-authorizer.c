#include "config.h"

#include <glib.h>
#include <libsoup/soup.h>
#include <rest/rest-proxy-call.h>

#include "zanata-authorizer.h"
#include "zanata-key-file-authorizer.h"


struct _ZanataKeyFileAuthorizer
{
  GObject parent_instance;
  GMutex mutex;
  GKeyFile *key_file;
};

static void zanata_authorizer_interface_init (ZanataAuthorizerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ZanataKeyFileAuthorizer, zanata_key_file_authorizer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ZANATA_TYPE_AUTHORIZER,
                                                zanata_authorizer_interface_init));

enum {
  PROP_0,
  PROP_KEY_FILE,
  LAST_PROP
};

static GParamSpec *key_file_authorizer_pspecs[LAST_PROP] = { 0 };

static gchar *
zanata_key_file_authorizer_get_url (ZanataAuthorizer *iface,
                                    const gchar *domain)
{
  ZanataKeyFileAuthorizer *self = ZANATA_KEY_FILE_AUTHORIZER (iface);
  gchar *key, *value;
  GError *error = NULL;

  g_mutex_lock (&self->mutex);

  key = g_strdup_printf ("%s.url", domain);
  value = g_key_file_get_string (self->key_file, "servers", key, &error);
  g_free (key);

  g_mutex_unlock (&self->mutex);

  if (!value)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      return NULL;
    }

  return value;
}

static void
zanata_key_file_authorizer_process_call (ZanataAuthorizer *iface,
                                         const gchar *domain,
                                         RestProxyCall *call)
{
  ZanataKeyFileAuthorizer *self = ZANATA_KEY_FILE_AUTHORIZER (iface);
  gchar *key, *value;
  GError *error = NULL;

  g_mutex_lock (&self->mutex);

  key = g_strdup_printf ("%s.username", domain);
  value = g_key_file_get_string (self->key_file, "servers", key, &error);
  g_free (key);

  if (value)
    rest_proxy_call_add_header (call, "X-Auth-User", value);
  else
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }
  g_free (value);

  key = g_strdup_printf ("%s.key", domain);
  value = g_key_file_get_string (self->key_file, "servers", key, &error);
  g_free (key);

  if (value)
    rest_proxy_call_add_header (call,  "X-Auth-Token", value);
  else
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }
  g_free (value);

  g_mutex_unlock (&self->mutex);
}

static void
zanata_key_file_authorizer_process_message (ZanataAuthorizer *iface,
                                            const gchar *domain,
                                            SoupMessage *message)
{
  ZanataKeyFileAuthorizer *self = ZANATA_KEY_FILE_AUTHORIZER (iface);
  gchar *key, *value;
  GError *error = NULL;

  g_mutex_lock (&self->mutex);

  key = g_strdup_printf ("%s.username", domain);
  value = g_key_file_get_string (self->key_file, "servers", key, &error);
  g_free (key);

  if (value)
    soup_message_headers_append (message->request_headers, "X-Auth-User",
                                 value);
  else
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }
  g_free (value);

  key = g_strdup_printf ("%s.key", domain);
  value = g_key_file_get_string (self->key_file, "servers", key, &error);
  g_free (key);

  if (value)
    soup_message_headers_append (message->request_headers, "X-Auth-Token",
                                 value);
  else
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }
  g_free (value);

  g_mutex_unlock (&self->mutex);
}

static gboolean
zanata_key_file_authorizer_refresh_authorization (ZanataAuthorizer *iface,
                                                  GCancellable *cancellable,
                                                  GError **error)
{
  return TRUE;
}

static void
zanata_key_file_authorizer_dispose (GObject *object)
{
  ZanataKeyFileAuthorizer *self = ZANATA_KEY_FILE_AUTHORIZER (object);

  g_clear_object (&self->key_file);

  G_OBJECT_CLASS (zanata_key_file_authorizer_parent_class)->dispose (object);
}

static void
zanata_key_file_authorizer_finalize (GObject *object)
{
  ZanataKeyFileAuthorizer *self = ZANATA_KEY_FILE_AUTHORIZER (object);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (zanata_key_file_authorizer_parent_class)->finalize (object);
}

static void
zanata_key_file_authorizer_init (ZanataKeyFileAuthorizer *self)
{
  g_mutex_init (&self->mutex);
}

static void
zanata_key_file_authorizer_set_property (GObject *object,
                                         guint prop_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
  ZanataKeyFileAuthorizer *self = ZANATA_KEY_FILE_AUTHORIZER (object);
  switch (prop_id)
    {
    case PROP_KEY_FILE:
      g_mutex_lock (&self->mutex);
      g_clear_pointer (&self->key_file, (GDestroyNotify) g_key_file_unref);
      self->key_file = g_value_dup_boxed (value);
      g_mutex_unlock (&self->mutex);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
zanata_key_file_authorizer_class_init (ZanataKeyFileAuthorizerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = zanata_key_file_authorizer_dispose;
  object_class->finalize = zanata_key_file_authorizer_finalize;
  object_class->set_property = zanata_key_file_authorizer_set_property;

  key_file_authorizer_pspecs[PROP_KEY_FILE] =
    g_param_spec_boxed ("key-file",
                        "Key file",
                        "A key file containing Zanata credentials",
                        G_TYPE_KEY_FILE,
                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT);
  g_object_class_install_properties (object_class, LAST_PROP,
                                     key_file_authorizer_pspecs);
}

static void
zanata_authorizer_interface_init (ZanataAuthorizerInterface *iface)
{
  iface->get_url = zanata_key_file_authorizer_get_url;
  iface->process_call = zanata_key_file_authorizer_process_call;
  iface->process_message = zanata_key_file_authorizer_process_message;
  iface->refresh_authorization = zanata_key_file_authorizer_refresh_authorization;
}

/**
 * zanata_key_file_authorizer_new:
 * @key_file: A #GKeyFile contains Zanata credentials
 *
 * Creates a new #ZanataKeyFileAuthorizer using @key_file.
 *
 * Returns: (transfer full): A new #ZanataKeyFileAuthorizer. Free the returned
 * object with g_object_unref().
 */
ZanataKeyFileAuthorizer *
zanata_key_file_authorizer_new (GKeyFile *key_file)
{
  return g_object_new (ZANATA_TYPE_KEY_FILE_AUTHORIZER,
                       "key-file", key_file,
                       NULL);
}
