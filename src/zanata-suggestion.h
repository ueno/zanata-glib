#ifndef ZANATA_SUGGESTION_H
#define ZANATA_SUGGESTION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ZANATA_TYPE_SUGGESTION (zanata_suggestion_get_type ())

G_DECLARE_FINAL_TYPE (ZanataSuggestion, zanata_suggestion,
                      ZANATA, SUGGESTION, GObject)

G_END_DECLS

#endif  /* ZANATA_SUGGESTION_H */
