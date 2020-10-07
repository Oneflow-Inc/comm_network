#pragma once

#include "comm_network/common/preprocessor_internal.h"

// basic
#define CN_PP_CAT(a, b) CN_PP_INTERNAL_CAT(a, b)

#define CN_PP_STRINGIZE(x) CN_PP_INTERNAL_STRINGIZE(x)

#define CN_PP_PAIR_FIRST(pair) CN_PP_INTERNAL_PAIR_FIRST(pair)

#define CN_PP_PAIR_SECOND(pair) CN_PP_INTERNAL_PAIR_SECOND(pair)

#define CN_PP_TUPLE_SIZE(t) CN_PP_INTERNAL_TUPLE_SIZE(t)

#define CN_PP_TUPLE_ELEM(n, t) CN_PP_INTERNAL_TUPLE_ELEM(n, t)

#define CN_PP_MAKE_TUPLE_SEQ(...) CN_PP_INTERNAL_MAKE_TUPLE_SEQ(__VA_ARGS__)

#define CN_PP_FOR_EACH_TUPLE(macro, seq) CN_PP_INTERNAL_FOR_EACH_TUPLE(macro, seq)

#define CN_PP_OUTTER_FOR_EACH_TUPLE(macro, seq) CN_PP_INTERNAL_OUTTER_FOR_EACH_TUPLE(macro, seq)

#define CN_PP_SEQ_PRODUCT_FOR_EACH_TUPLE(macro, ...) \
  CN_PP_INTERNAL_SEQ_PRODUCT_FOR_EACH_TUPLE(macro, __VA_ARGS__)

// advanced

#define CN_PP_VARIADIC_SIZE(...) CN_PP_INTERNAL_VARIADIC_SIZE(__VA_ARGS__)

#define CN_PP_SEQ_SIZE(seq) CN_PP_INTERNAL_SEQ_SIZE(seq)

#define CN_PP_ATOMIC_TO_TUPLE(x) (x)

#define CN_PP_FOR_EACH_ATOMIC(macro, seq) \
  CN_PP_FOR_EACH_TUPLE(macro, CN_PP_SEQ_MAP(CN_PP_ATOMIC_TO_TUPLE, seq))

#define CN_PP_SEQ_PRODUCT(seq0, ...) CN_PP_INTERNAL_SEQ_PRODUCT(seq0, __VA_ARGS__)

#define CN_PP_SEQ_MAP(macro, seq) \
  CN_PP_SEQ_PRODUCT_FOR_EACH_TUPLE(CN_PP_I_SEQ_MAP_DO_EACH, (macro), seq)
#define CN_PP_I_SEQ_MAP_DO_EACH(macro, elem) (macro(elem))

#define CN_PP_JOIN(glue, ...) CN_PP_INTERNAL_JOIN(glue, __VA_ARGS__)

#define CN_PP_TUPLE_PUSH_FRONT(t, x) CN_PP_INTERNAL_TUPLE_PUSH_FRONT(t, x)

#define CN_PP_FORCE(...) CN_PP_TUPLE2VARADIC(CN_PP_CAT((__VA_ARGS__), ))
