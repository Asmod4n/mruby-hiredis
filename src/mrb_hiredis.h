#ifndef MRB_HIREDIS_H
#define MRB_HIREDIS_H

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <mruby/data.h>
#include <errno.h>
#include <mruby/error.h>
#include <mruby/numeric.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/throw.h>
#include <string.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <mruby/hash.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

#if (__GNUC__ >= 3) || (__INTEL_COMPILER >= 800) || defined(__clang__)
# define likely(x) __builtin_expect(!!(x), 1)
# define unlikely(x) __builtin_expect(!!(x), 0)
#else
# define likely(x) (x)
# define unlikely(x) (x)
#endif

static void
mrb_redisFree(mrb_state *mrb, void *p)
{
  redisFree((redisContext *) p);
}

static const struct mrb_data_type mrb_redisContext_type = {
  "$i_mrb_redisContext_type", mrb_redisFree
};

typedef struct {
  mrb_value self;
  redisReply *reply;
} mrb_redisGetReply_cb_data;

static const struct mrb_data_type mrb_redisCallbackFn_cb_data_type = {
  "$i_mrb_redisCallbackFn_cb_data_type", mrb_free
};

typedef struct {
  mrb_state *mrb;
  mrb_value self;
  mrb_value callbacks;
  mrb_value evloop;
  int fd;
  mrb_value replies;
  mrb_value subscriptions;
} mrb_hiredis_async_context;


static void
mrb_redisAsyncFree(mrb_state *mrb, void *p)
{
  redisAsyncContext *async_context = (redisAsyncContext *) p;
  mrb_free(mrb, async_context->ev.data);
  async_context->ev.data = NULL;
  redisAsyncFree(async_context);
}

static const struct mrb_data_type mrb_redisAsyncContext_type = {
  "$i_mrb_redisContext_type", mrb_redisAsyncFree
};

#endif
