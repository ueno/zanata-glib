#ifndef ZANATA_KEY_FILE_AUTHORIZER_H
#define ZANATA_KEY_FILE_AUTHORIZER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ZANATA_TYPE_KEY_FILE_AUTHORIZER (zanata_key_file_authorizer_get_type ())

G_DECLARE_FINAL_TYPE (ZanataKeyFileAuthorizer, zanata_key_file_authorizer,
                      ZANATA, KEY_FILE_AUTHORIZER, GObject)

ZanataKeyFileAuthorizer *zanata_key_file_authorizer_new (GKeyFile *key_file);

G_END_DECLS

#endif /* ZANATA_KEY_FILE_AUTHORIZER_H */
