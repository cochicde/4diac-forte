
#ifndef UA_TYPES_FORDIACNAMESPACE_GENERATED_HANDLING_H_
#define UA_TYPES_FORDIACNAMESPACE_GENERATED_HANDLING_H_

#include "ua_types_fordiacNamespace_generated.h"

_UA_BEGIN_DECLS

#if defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wmissing-field-initializers"
# pragma GCC diagnostic ignored "-Wmissing-braces"
#endif


/* DatatypeTest */
static UA_INLINE void
UA_DatatypeTest_init(UA_DatatypeTest *p) {
    memset(p, 0, sizeof(UA_DatatypeTest));
}

static UA_INLINE UA_DatatypeTest *
UA_DatatypeTest_new(void) {
    return (UA_DatatypeTest*)UA_new(&UA_UA_TYPES_FORDIACNAMESPACE[UA_UA_TYPES_FORDIACNAMESPACE_DATATYPETEST]);
}

static UA_INLINE UA_StatusCode
UA_DatatypeTest_copy(const UA_DatatypeTest *src, UA_DatatypeTest *dst) {
    return UA_copy(src, dst, &UA_UA_TYPES_FORDIACNAMESPACE[UA_UA_TYPES_FORDIACNAMESPACE_DATATYPETEST]);
}

static UA_INLINE void
UA_DatatypeTest_deleteMembers(UA_DatatypeTest *p) {
    UA_clear(p, &UA_UA_TYPES_FORDIACNAMESPACE[UA_UA_TYPES_FORDIACNAMESPACE_DATATYPETEST]);
}

static UA_INLINE void
UA_DatatypeTest_clear(UA_DatatypeTest *p) {
    UA_clear(p, &UA_UA_TYPES_FORDIACNAMESPACE[UA_UA_TYPES_FORDIACNAMESPACE_DATATYPETEST]);
}

static UA_INLINE void
UA_DatatypeTest_delete(UA_DatatypeTest *p) {
    UA_delete(p, &UA_UA_TYPES_FORDIACNAMESPACE[UA_UA_TYPES_FORDIACNAMESPACE_DATATYPETEST]);
}

#if defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
# pragma GCC diagnostic pop
#endif

_UA_END_DECLS

#endif /* UA_TYPES_FORDIACNAMESPACE_GENERATED_HANDLING_H_ */
