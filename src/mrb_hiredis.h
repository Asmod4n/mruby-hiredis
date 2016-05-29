#ifndef MRB_HIREDIS_H
#define MRB_HIREDIS_H

#include "hiredis.h"
#include "async.h"
#include <mruby/data.h>
#include <errno.h>
#include <mruby/error.h>
#include <mruby/numeric.h>
#include <mruby/array.h>
#include <mruby/string.h>
#include <mruby/throw.h>
#include <string.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <strings.h>

static void
mrb_redisFree(mrb_state *mrb, void *p)
{
  redisFree((redisContext *) p);
}

static const struct mrb_data_type mrb_redisContext_type = {
  "$i_mrb_redisContext_type", mrb_redisFree
};

static void
mrb_redisAsyncFree(mrb_state *mrb, void *p)
{
  redisAsyncContext *async_context = (redisAsyncContext *) p;
  if (async_context->ev.data) {
    mrb_free(mrb, async_context->ev.data);
    async_context->ev.data = NULL;
  }
  redisAsyncFree(async_context);
}

static const struct mrb_data_type mrb_redisAsyncContext_type = {
  "$i_mrb_redisContext_type", mrb_redisAsyncFree
};

typedef struct {
  mrb_state *mrb;
  mrb_value self;
  mrb_value callbacks;
  mrb_value evloop;
  redisAsyncContext *async_context;
  int fd;
  mrb_value replies;
  mrb_value subscribe;
} mrb_hiredis_async_context;

#endif
