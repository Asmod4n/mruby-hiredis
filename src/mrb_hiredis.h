#ifndef MRB_HIREDIS_H
#define MRB_HIREDIS_H

#include "hiredis.h"
#include <mruby/data.h>
#include <errno.h>
#include <mruby/error.h>
#include <mruby/numeric.h>
#include <mruby/array.h>
#include <mruby/string.h>
#include <mruby/throw.h>
#include <string.h>
#include <mruby/class.h>

static void
mrb_redisFree(mrb_state *mrb, void *p)
{
  redisFree((redisContext *) p);
}

static const struct mrb_data_type mrb_redisContext_type = {
  "$i_mrb_redisContext_type", mrb_redisFree
};

#endif
