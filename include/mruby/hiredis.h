#ifndef MRUBY_HIREDIS_H
#define MRUBY_HIREDIS_H

#include <mruby.h>

#ifdef __cplusplus
extern "C" {
#endif

#define E_HIREDIS_ERROR (mrb_class_get_under(mrb, mrb_class_get(mrb, "Hiredis"), "Error"))
#define E_HIREDIS_REPLY_ERROR (mrb_class_get_under(mrb, mrb_class_get(mrb, "Hiredis"), "ReplyError"))
#ifndef E_EOF_ERROR
#define E_EOF_ERROR (mrb_class_get(mrb, "EOFError"))
#endif
#define E_HIREDIS_ERR_PROTOCOL (mrb_class_get_under(mrb, mrb_class_get(mrb, "Hiredis"), "ProtocolError"))
#define E_HIREDIS_ERR_OOM (mrb_class_get_under(mrb, mrb_class_get(mrb, "Hiredis"), "OOMError"))

#ifdef __cplusplus
}
#endif

#endif

