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
mrb_redisFree_gc(mrb_state *mrb, void *p)
{
  redisFree((redisContext *) p);
}

static const struct mrb_data_type mrb_redisContext_type = {
  "$i_mrb_redisContext_type", mrb_redisFree_gc
};

typedef struct {
  mrb_value self;
  redisReply *reply;
} mrb_redisGetReply_cb_data;

static const struct mrb_data_type mrb_redisCallbackFn_cb_data_type = {
  "$i_mrb_redisCallbackFn_cb_data_type", mrb_free
};

typedef struct {
  const mrb_sym command;
  const mrb_value *mrb_argv;
  mrb_int *argc_p;
  const char ***argv_p;
  size_t **argvlen_p;
} mrb_hiredis_generate_argv_argc_array_cb_data;

MRB_INLINE mrb_value
mrb_hiredis_generate_argv_argc_array_cb(mrb_state *mrb, const mrb_value command_argv_argc)
{
  mrb_hiredis_generate_argv_argc_array_cb_data *cb_data = (mrb_hiredis_generate_argv_argc_array_cb_data *) mrb_cptr(command_argv_argc);
  *cb_data->argv_p = NULL;
  *cb_data->argvlen_p = NULL;
  *cb_data->argc_p += 1;
  if (likely((*cb_data->argc_p <= SIZE_MAX / sizeof(const char *)) && (*cb_data->argc_p <= SIZE_MAX / sizeof(size_t)))) {
    const char **argv = *cb_data->argv_p = mrb_malloc(mrb, *cb_data->argc_p * sizeof(*argv));
    size_t *argvlen = *cb_data->argvlen_p = mrb_malloc(mrb, *cb_data->argc_p * sizeof(*argvlen));
    mrb_int command_len;
    argv[0] = mrb_sym2name_len(mrb, cb_data->command, &command_len);
    argvlen[0] = command_len;

    mrb_int argc_current;
    for (argc_current = 1; argc_current < *cb_data->argc_p; argc_current++) {
      mrb_value curr = mrb_str_to_str(mrb, cb_data->mrb_argv[argc_current - 1]);
      argv[argc_current] = RSTRING_PTR(curr);
      argvlen[argc_current] = RSTRING_LEN(curr);
    }
  } else {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Cannot allocate argv array");
  }

  return mrb_nil_value();
}

static void
mrb_hiredis_generate_argv_argc_array(mrb_state *mrb, const mrb_sym command, const mrb_value *mrb_argv, mrb_int *argc_p, const char ***argv_p, size_t **argvlen_p)
{
  mrb_hiredis_generate_argv_argc_array_cb_data cb_data = {command, mrb_argv, argc_p, argv_p, argvlen_p};

  mrb_bool error;
  mrb_value result = mrb_protect(mrb, mrb_hiredis_generate_argv_argc_array_cb, mrb_cptr_value(mrb, &cb_data), &error);
  if (unlikely(error)) {
    mrb_free(mrb, *argv_p);
    mrb_free(mrb, *argvlen_p);
    mrb_exc_raise(mrb, result);
  }
}

typedef struct {
  mrb_state *mrb;
  mrb_value self;
  mrb_value callbacks;
  mrb_value evloop;
  redisAsyncContext *async_context;
  mrb_value replies;
  mrb_value subscriptions;
} mrb_hiredis_async_context;

static void
mrb_redisAsyncFree_gc(mrb_state *mrb, void *p)
{
  redisAsyncContext *async_context = (redisAsyncContext *) p;
  mrb_free(mrb, async_context->ev.data);
  async_context->ev.data = NULL;
  redisAsyncFree(async_context);
}

static const struct mrb_data_type mrb_redisAsyncContext_type = {
  "$i_mrb_redisAsyncContext_type", mrb_redisAsyncFree_gc
};

#endif
