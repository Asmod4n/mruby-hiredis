#include "mruby/hiredis.h"
#include "mrb_hiredis.h"

static void
mrb_hiredis_check_error(const redisContext *context, mrb_state *mrb)
{
  switch (context->err) {
  case REDIS_ERR_IO:
    mrb_sys_fail(mrb, context->errstr);
    break;
  case REDIS_ERR_EOF:
    mrb_raise(mrb, E_EOF_ERROR, context->errstr);
    break;
  case REDIS_ERR_PROTOCOL:
    mrb_raise(mrb, E_HIREDIS_ERR_PROTOCOL, context->errstr);
    break;
  case REDIS_ERR_OOM:
    mrb_raise(mrb, E_HIREDIS_ERR_OOM, context->errstr);
    break;
  default:
    mrb_raise(mrb, E_HIREDIS_ERROR, context->errstr);
  }
}

static mrb_value
mrb_redisConnect(mrb_state *mrb, mrb_value self)
{
  char *host_or_path = (char *) "localhost";
  mrb_int port = 6379;

  mrb_get_args(mrb, "|zi", &host_or_path, &port);
  mrb_assert(port >= INT_MIN && port <= INT_MAX);

  redisContext *context = NULL;
  errno = 0;
  if (port == -1) {
    context = redisConnectUnix(host_or_path);
  } else {
    context = redisConnect(host_or_path, port);
  }
  if (likely(context != NULL)) {
    mrb_data_init(self, context, &mrb_redisContext_type);
    if (likely(context->err == 0)) {
      return self;
    } else {
      mrb_hiredis_check_error(context, mrb);
      return mrb_false_value();
    }
  } else {
    mrb_sys_fail(mrb, "redisConnect");
    return mrb_false_value();
  }
}

static mrb_value
mrb_redisFree(mrb_state *mrb, mrb_value self)
{
  redisContext *context = (redisContext *) DATA_PTR(self);
  if (likely(context)) {
    redisFree(context);
    mrb_data_init(self, NULL, NULL);
    return mrb_nil_value();
  } else {
    mrb_raise(mrb, E_IO_ERROR, "closed stream");
    return mrb_false_value();
  }
}

MRB_INLINE mrb_value
mrb_hiredis_get_ary_reply(redisReply *reply, mrb_state *mrb);

MRB_INLINE mrb_value
mrb_hiredis_get_map_reply(redisReply *reply, mrb_state *mrb);

MRB_INLINE mrb_value
mrb_hiredis_get_reply(redisReply *reply, mrb_state *mrb)
{
  if (likely(reply)) {
    switch (reply->type) {
      case REDIS_REPLY_STRING:
      case REDIS_REPLY_STATUS:
      case REDIS_REPLY_BIGNUM:
        return mrb_str_new(mrb, reply->str, reply->len);
        break;
      case REDIS_REPLY_ARRAY:
        return mrb_hiredis_get_ary_reply(reply, mrb);
        break;
      case REDIS_REPLY_MAP:
      case REDIS_REPLY_ATTR:
        return mrb_hiredis_get_map_reply(reply, mrb);
        break;
      case REDIS_REPLY_INTEGER: {
        if (FIXABLE(reply->integer))
          return mrb_fixnum_value(reply->integer);
        else
          return mrb_float_value(mrb, reply->integer);
      } break;
      case REDIS_REPLY_NIL:
        return mrb_nil_value();
        break;
      case REDIS_REPLY_ERROR:
        return mrb_exc_new_str(mrb, E_HIREDIS_REPLY_ERROR, mrb_str_new(mrb, reply->str, reply->len));
        break;
      case REDIS_REPLY_DOUBLE:
        return mrb_float_value(mrb, reply->dval);
        break;
      case REDIS_REPLY_BOOL:
        return mrb_bool_value(reply->integer);
        break;
      case REDIS_REPLY_VERB: {
        mrb_value argv[] = {
          mrb_str_new(mrb, reply->str, reply->len),
          mrb_str_new_cstr(mrb, reply->vtype)
        };
        return mrb_obj_new(mrb, mrb_class_get_under(mrb, mrb_class_get(mrb, "Hiredis"), "Verb"), 2, argv);
      } break;
      default:
        mrb_raise(mrb, E_HIREDIS_ERROR, "unknown reply type");
    }
  } else {
    return mrb_nil_value();
  }
}

MRB_INLINE mrb_value
mrb_hiredis_get_ary_reply(redisReply *reply, mrb_state *mrb)
{
  mrb_value ary = mrb_ary_new_capa(mrb, reply->elements);
  int ai = mrb_gc_arena_save(mrb);

  size_t element_couter;
  for (element_couter = 0; element_couter < reply->elements; element_couter++) {
    mrb_ary_push(mrb, ary, mrb_hiredis_get_reply(reply->element[element_couter], mrb));
    mrb_gc_arena_restore(mrb, ai);
  }
  return ary;
}

MRB_INLINE mrb_value
mrb_hiredis_get_map_reply(redisReply *reply, mrb_state *mrb)
{
  mrb_value map = mrb_hash_new_capa(mrb, reply->elements / 2);
  int ai = mrb_gc_arena_save(mrb);

  size_t element_couter;
  for (element_couter = 0; element_couter < reply->elements; element_couter++) {
    mrb_value key = mrb_hiredis_get_reply(reply->element[element_couter], mrb);
    element_couter++;
    mrb_value value = mrb_hiredis_get_reply(reply->element[element_couter], mrb);
    mrb_hash_set(mrb, map, key, value);
    mrb_gc_arena_restore(mrb, ai);
  }
  return map;
}

MRB_INLINE mrb_value
mrb_redisCommandArgv_cb(mrb_state *mrb, mrb_value reply)
{
  return mrb_hiredis_get_reply(mrb_cptr(reply), mrb);
}

MRB_INLINE mrb_value
mrb_redisCommandArgv_ensure(mrb_state *mrb, mrb_value reply)
{
  freeReplyObject(mrb_cptr(reply));
  return mrb_nil_value();
}

static mrb_value
mrb_redisCommandArgv(mrb_state *mrb, mrb_value self)
{
  redisContext *context = (redisContext *) DATA_PTR(self);
  if (likely(context)) {
    if (likely(context->err == 0)) {
      mrb_sym command;
      mrb_value *mrb_argv;
      mrb_int argc = 0;

      mrb_get_args(mrb, "n*", &command, &mrb_argv, &argc);

      const char **argv = NULL;
      size_t *argvlen = NULL;
      mrb_hiredis_generate_argv_argc_array(mrb, command, mrb_argv, &argc, &argv, &argvlen);

      errno = 0;
      redisReply *reply = redisCommandArgv(context, argc, argv, argvlen);
      mrb_free(mrb, argv);
      mrb_free(mrb, argvlen);
      if (likely(reply != NULL)) {
        mrb_value reply_cptr_value = mrb_cptr_value(mrb, reply);
        return mrb_ensure(mrb, mrb_redisCommandArgv_cb, reply_cptr_value, mrb_redisCommandArgv_ensure, reply_cptr_value);
      } else {
        mrb_hiredis_check_error(context, mrb);
        return mrb_false_value();
      }
    } else {
      mrb_hiredis_check_error(context, mrb);
      return mrb_false_value();
    }
  } else {
    mrb_raise(mrb, E_IO_ERROR, "closed stream");
    return mrb_false_value();
  }
}

static mrb_value
mrb_redisAppendCommandArgv(mrb_state *mrb, mrb_value self)
{
  redisContext *context = (redisContext *) DATA_PTR(self);
  if (likely(context)) {
    if (likely(context->err == 0)) {
      mrb_sym command;
      mrb_value *mrb_argv;
      mrb_int argc = 0;

      mrb_get_args(mrb, "n*", &command, &mrb_argv, &argc);

      mrb_sym queue_counter_sym = mrb_intern_lit(mrb, "queue_counter");
      mrb_value queue_counter_val = mrb_iv_get(mrb, self, queue_counter_sym);
      mrb_int queue_counter = 1;
      if (mrb_fixnum_p(queue_counter_val)) {
        queue_counter = mrb_fixnum(queue_counter_val);
        if (unlikely(mrb_int_add_overflow(queue_counter, 1, &queue_counter))) {
          mrb_raise(mrb, E_RUNTIME_ERROR, "integer addition would overflow");
        }
      }

      const char **argv = NULL;
      size_t *argvlen = NULL;
      mrb_hiredis_generate_argv_argc_array(mrb, command, mrb_argv, &argc, &argv, &argvlen);

      errno = 0;
      int rc = redisAppendCommandArgv(context, argc, argv, argvlen);
      mrb_free(mrb, argv);
      mrb_free(mrb, argvlen);
      if (likely(rc == REDIS_OK)) {
        mrb_iv_set(mrb, self, queue_counter_sym, mrb_fixnum_value(queue_counter));
        return self;
      } else {
        mrb_hiredis_check_error(context, mrb);
        return mrb_false_value();
      }
    } else {
      mrb_hiredis_check_error(context, mrb);
      return mrb_false_value();
    }
  } else {
    mrb_raise(mrb, E_IO_ERROR, "closed stream");
    return mrb_false_value();
  }
}

MRB_INLINE mrb_value
mrb_redisGetReply_cb(mrb_state *mrb, mrb_value self_reply)
{
  mrb_redisGetReply_cb_data *cb_data = (mrb_redisGetReply_cb_data *)mrb_cptr(self_reply);
  mrb_value self = cb_data->self;
  mrb_sym queue_counter_sym = mrb_intern_lit(mrb, "queue_counter");
  mrb_value queue_counter_val = mrb_iv_get(mrb, self, queue_counter_sym);
  mrb_int queue_counter = -1;
  if (mrb_fixnum_p(queue_counter_val)) {
    queue_counter = mrb_fixnum(queue_counter_val);
  }

  mrb_value reply_val = mrb_hiredis_get_reply(cb_data->reply, mrb);
  if (queue_counter > 1) {
    mrb_iv_set(mrb, self, queue_counter_sym, mrb_fixnum_value(--queue_counter));
  } else {
    mrb_iv_remove(mrb, self, queue_counter_sym);
  }
  return reply_val;
}

MRB_INLINE mrb_value
mrb_redisGetReply_ensure(mrb_state *mrb, mrb_value self_reply)
{
  mrb_redisGetReply_cb_data *cb_data = (mrb_redisGetReply_cb_data *)mrb_cptr(self_reply);
  freeReplyObject(cb_data->reply);
  return mrb_nil_value();
}

static mrb_value
mrb_redisGetReply(mrb_state *mrb, mrb_value self)
{
  redisContext *context = (redisContext *) DATA_PTR(self);
  if (likely(context)) {
    if (likely(context->err == 0)) {
      redisReply *reply = NULL;
      errno = 0;
      int rc = redisGetReply(context, (void **) &reply);
      if (likely(rc == REDIS_OK)) {
        if (likely(reply != NULL)) {
          mrb_redisGetReply_cb_data cb_data = { self, reply };
          mrb_value cb_data_val = mrb_cptr_value(mrb, &cb_data);
          return mrb_ensure(mrb, mrb_redisGetReply_cb, cb_data_val, mrb_redisGetReply_ensure, cb_data_val);
        } else {
          return mrb_nil_value();
        }
      } else {
        mrb_hiredis_check_error(context, mrb);
        return mrb_false_value();
      }
    } else {
      mrb_hiredis_check_error(context, mrb);
      return mrb_false_value();
    }
  } else {
    mrb_raise(mrb, E_IO_ERROR, "closed stream");
    return mrb_false_value();
  }
}

static mrb_value
mrb_redisGetBulkReply(mrb_state *mrb, mrb_value self)
{
  mrb_value queue_counter_val = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "queue_counter"));

  if (likely(mrb_fixnum_p(queue_counter_val))) {
    mrb_int queue_counter = mrb_fixnum(queue_counter_val);

    mrb_value bulk_reply = mrb_ary_new_capa(mrb, queue_counter);
    int ai = mrb_gc_arena_save(mrb);

    do {
      mrb_value reply = mrb_redisGetReply(mrb, self);
      mrb_ary_push(mrb, bulk_reply, reply);
      mrb_gc_arena_restore(mrb, ai);
    } while (--queue_counter > 0);

    return bulk_reply;
  } else {
    mrb_raise(mrb, E_RUNTIME_ERROR, "nothing queued yet");
    return mrb_false_value();
  }
}

#if ((HIREDIS_MAJOR == 0) && (HIREDIS_MINOR >= 13) || (HIREDIS_MAJOR > 0))
static mrb_value
mrb_redisReconnect(mrb_state *mrb, mrb_value self)
{
  redisContext *context = (redisContext *) DATA_PTR(self);
  if (likely(context)) {
    int rc = redisReconnect(context);
    if (likely(rc == REDIS_OK)) {
      return self;
    } else {
      mrb_hiredis_check_error(context, mrb);
      return mrb_false_value();
    }
  } else {
    mrb_raise(mrb, E_IO_ERROR, "closed stream");
    return mrb_false_value();
  }
}
#endif

MRB_INLINE void
mrb_hiredis_dataCleanup(void *privdata)
{
  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) privdata;
  mrb_data_init(mrb_async_context->self, NULL, NULL);
  mrb_free(mrb_async_context->mrb, privdata);
}

MRB_INLINE void
mrb_hiredis_addRead(void *privdata)
{
  mrb_assert(privdata);

  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) privdata;
  mrb_state *mrb = mrb_async_context->mrb;
  mrb_assert(mrb);
  int ai = mrb_gc_arena_save(mrb);

  mrb_value block = mrb_iv_get(mrb, mrb_async_context->callbacks, mrb_intern_lit(mrb, "@addRead"));
  if (likely(mrb_type(block) == MRB_TT_PROC)) {
    mrb_value argv[] = {
      mrb_async_context->self,
      mrb_async_context->evloop,
      mrb_fixnum_value(mrb_async_context->fd)
    };
    mrb_yield_argv(mrb, block, 3, argv);
    mrb_gc_arena_restore(mrb, ai);
  }
}

MRB_INLINE void
mrb_hiredis_delRead(void *privdata)
{
  mrb_assert(privdata);

  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) privdata;
  mrb_state *mrb = mrb_async_context->mrb;
  mrb_assert(mrb);
  int ai = mrb_gc_arena_save(mrb);

  mrb_value block = mrb_iv_get(mrb, mrb_async_context->callbacks, mrb_intern_lit(mrb, "@delRead"));
  if (likely(mrb_type(block) == MRB_TT_PROC)) {
    mrb_value argv[] = {
      mrb_async_context->self,
      mrb_async_context->evloop,
      mrb_fixnum_value(mrb_async_context->fd)
    };
    mrb_yield_argv(mrb, block, 3, argv);
    mrb_gc_arena_restore(mrb, ai);
  }
}

MRB_INLINE void
mrb_hiredis_addWrite(void *privdata)
{
  mrb_assert(privdata);

  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) privdata;
  mrb_state *mrb = mrb_async_context->mrb;
  mrb_assert(mrb);
  int ai = mrb_gc_arena_save(mrb);

  mrb_value block = mrb_iv_get(mrb, mrb_async_context->callbacks, mrb_intern_lit(mrb, "@addWrite"));
  if (likely(mrb_type(block) == MRB_TT_PROC)) {
    mrb_value argv[] = {
      mrb_async_context->self,
      mrb_async_context->evloop,
      mrb_fixnum_value(mrb_async_context->fd)
    };
    mrb_yield_argv(mrb, block, 3, argv);
    mrb_gc_arena_restore(mrb, ai);
  }
}

MRB_INLINE void
mrb_hiredis_delWrite(void *privdata)
{
  mrb_assert(privdata);

  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) privdata;
  mrb_state *mrb = mrb_async_context->mrb;
  mrb_assert(mrb);
  int ai = mrb_gc_arena_save(mrb);

  mrb_value block = mrb_iv_get(mrb, mrb_async_context->callbacks, mrb_intern_lit(mrb, "@delWrite"));
  if (likely(mrb_type(block) == MRB_TT_PROC)) {
    mrb_value argv[] = {
      mrb_async_context->self,
      mrb_async_context->evloop,
      mrb_fixnum_value(mrb_async_context->fd)
    };
    mrb_yield_argv(mrb, block, 3, argv);
    mrb_gc_arena_restore(mrb, ai);
  }
}

MRB_INLINE void
mrb_hiredis_cleanup(void *privdata)
{
  mrb_assert(privdata);

  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) privdata;
  mrb_state *mrb = mrb_async_context->mrb;
  mrb_assert(mrb);
  int ai = mrb_gc_arena_save(mrb);

  mrb_value block = mrb_iv_get(mrb, mrb_async_context->callbacks, mrb_intern_lit(mrb, "@cleanup"));
  if (likely(mrb_type(block) == MRB_TT_PROC)) {
    mrb_value argv[] = {
      mrb_async_context->self,
      mrb_async_context->evloop
    };
    mrb_yield_argv(mrb, block, 2, argv);
    mrb_gc_arena_restore(mrb, ai);
  }
}

MRB_INLINE void
mrb_redisDisconnectCallback(const struct redisAsyncContext *async_context, int status)
{
  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) async_context->data;
  mrb_state *mrb = mrb_async_context->mrb;
  mrb_assert(mrb);
  int ai = mrb_gc_arena_save(mrb);

  mrb_value block = mrb_iv_get(mrb, mrb_async_context->callbacks, mrb_intern_lit(mrb, "@disconnect"));
  if (likely(mrb_type(block) == MRB_TT_PROC)) {
    mrb_value argv[] = {
      mrb_async_context->self,
      mrb_async_context->evloop,
      mrb_fixnum_value(status)
    };

    mrb_yield_argv(mrb, block, 3, argv);
    mrb_gc_arena_restore(mrb, ai);
  }
}

MRB_INLINE void
mrb_redisConnectCallback(const struct redisAsyncContext *async_context, int status)
{
  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) async_context->data;
  mrb_state *mrb = mrb_async_context->mrb;
  mrb_assert(mrb);
  int ai = mrb_gc_arena_save(mrb);

  mrb_value block = mrb_iv_get(mrb, mrb_async_context->callbacks, mrb_intern_lit(mrb, "@connect"));
  if (likely(mrb_type(block) == MRB_TT_PROC)) {
    mrb_value argv[] = {
      mrb_async_context->self,
      mrb_async_context->evloop,
      mrb_fixnum_value(status)
    };

    mrb_yield_argv(mrb, block, 3, argv);
    mrb_gc_arena_restore(mrb, ai);
  }
}

MRB_INLINE mrb_value
mrb_hiredis_setup_async_context(mrb_state *mrb, mrb_value self, mrb_value callbacks, mrb_value evloop, redisAsyncContext *async_context)
{
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@callbacks"), callbacks);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@evloop"), evloop);
  mrb_value replies = mrb_ary_new(mrb);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "replies"), replies);
  mrb_value subscriptions = mrb_hash_new(mrb);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "subscriptions"), subscriptions);

  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) mrb_malloc(mrb, sizeof(mrb_hiredis_async_context));
  mrb_async_context->mrb = mrb;
  mrb_async_context->self = self;
  mrb_async_context->callbacks = callbacks;
  mrb_async_context->evloop = evloop;
  mrb_async_context->fd = async_context->c.fd;
  mrb_async_context->replies = replies;
  mrb_async_context->subscriptions = subscriptions;

  async_context->data = async_context->ev.data = mrb_async_context;
  async_context->dataCleanup = mrb_hiredis_dataCleanup;
  async_context->ev.addRead = mrb_hiredis_addRead;
  async_context->ev.delRead = mrb_hiredis_delRead;
  async_context->ev.addWrite = mrb_hiredis_addWrite;
  async_context->ev.delWrite = mrb_hiredis_delWrite;
  async_context->ev.cleanup = mrb_hiredis_cleanup;
  redisAsyncSetDisconnectCallback(async_context, mrb_redisDisconnectCallback);
  redisAsyncSetConnectCallback(async_context, mrb_redisConnectCallback);

  return self;
}

static mrb_value
mrb_redisAsyncConnect(mrb_state *mrb, mrb_value self)
{
  mrb_value  callbacks = mrb_nil_value(), evloop = mrb_nil_value();
  char *host_or_path = (char *) "localhost";
  mrb_int port = 6379;

  mrb_get_args(mrb, "|oozi", &callbacks, &evloop, &host_or_path, &port);
  mrb_assert(port >= INT_MIN && port <= INT_MAX);

  if (mrb_nil_p(callbacks)) {
    callbacks = mrb_obj_new(mrb, mrb_class_get_under(mrb, mrb_class(mrb, self), "Callbacks"), 0, NULL);
  }

  if (mrb_nil_p(evloop)) {
    evloop = mrb_obj_new(mrb, mrb_class_get(mrb, "RedisAe"), 0, NULL);
  }

  redisAsyncContext *async_context = NULL;
  errno = 0;
  if (port == -1) {
    async_context = redisAsyncConnectUnix(host_or_path);
  } else {
    async_context = redisAsyncConnect(host_or_path, port);
  }
  if (likely(async_context != NULL)) {
    mrb_data_init(self, async_context, &mrb_redisAsyncContext_type);
    if (likely(async_context->c.err == 0)) {
      return mrb_hiredis_setup_async_context(mrb, self, callbacks, evloop, async_context);
    } else {
      mrb_hiredis_check_error(&async_context->c, mrb);
      return mrb_false_value();
    }
  } else {
    mrb_sys_fail(mrb, "redisAsyncConnect");
    return mrb_false_value();
  }
}

static mrb_value
mrb_redisAsyncHandleRead(mrb_state *mrb, mrb_value self)
{
  redisAsyncContext *async_context = (redisAsyncContext *) DATA_PTR(self);
  if (likely(async_context)) {
    redisAsyncHandleRead(async_context);
    return self;
  } else {
    mrb_raise(mrb, E_IO_ERROR, "closed stream");
    return mrb_false_value();
  }
}

static mrb_value
mrb_redisAsyncHandleWrite(mrb_state *mrb, mrb_value self)
{
  redisAsyncContext *async_context = (redisAsyncContext *) DATA_PTR(self);
  if (likely(async_context)) {
    redisAsyncHandleWrite(async_context);
    return self;
  } else {
    mrb_raise(mrb, E_IO_ERROR, "closed stream");
    return mrb_false_value();
  }
}

MRB_INLINE void
mrb_redisCallbackFn(struct redisAsyncContext *async_context, void *r, void *privdata)
{
  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) async_context->data;
  mrb_state *mrb = mrb_async_context->mrb;

  mrb_assert(mrb);
  int ai = mrb_gc_arena_save(mrb);

  mrb_value block_cb_data = mrb_obj_value(privdata);
  mrb_funcall(mrb, mrb_async_context->replies, "delete", 1, block_cb_data);
  mrb_value block = *(mrb_value*)DATA_PTR(block_cb_data);
  if (likely(mrb_type(block) == MRB_TT_PROC)) {
    mrb_value reply = mrb_nil_value();
    if (likely(r)) {
      reply = mrb_hiredis_get_reply((redisReply *)r, mrb);
    }
    mrb_yield(mrb, block, reply);
  }
  mrb_gc_arena_restore(mrb, ai);
}

static mrb_value
mrb_redisAsyncCommandArgv(mrb_state *mrb, mrb_value self)
{
  redisAsyncContext *async_context = (redisAsyncContext *) DATA_PTR(self);
  if (likely(async_context)) {
    mrb_sym command;
    mrb_value *mrb_argv;
    mrb_int argc = 0;
    mrb_value block = mrb_nil_value();

    mrb_get_args(mrb, "n*&", &command, &mrb_argv, &argc, &block);

    errno = 0;
    int rc = REDIS_ERR;
    if (mrb_type(block) == MRB_TT_PROC) {
      mrb_value *block_p;
      struct RData *block_cb_data_p;
      Data_Make_Struct(mrb, mrb_class_get_under(mrb, mrb_obj_class(mrb, self), "_BlockData"), mrb_value, &mrb_redisCallbackFn_cb_data_type, block_p, block_cb_data_p);
      memcpy(block_p, &block, sizeof(block_p));
      mrb_value block_cb_data = mrb_obj_value(block_cb_data_p);
      mrb_iv_set(mrb, block_cb_data, mrb_intern_lit(mrb, "block"), block);
      const char **argv = NULL;
      size_t *argvlen = NULL;
      mrb_hiredis_generate_argv_argc_array(mrb, command, mrb_argv, &argc, &argv, &argvlen);
      rc = redisAsyncCommandArgv(async_context, mrb_redisCallbackFn, block_cb_data_p, argc, argv, argvlen);
      mrb_free(mrb, argv);
      mrb_int command_len = argvlen[0];
      mrb_free(mrb, argvlen);
      if (likely(rc == REDIS_OK)) {
        if ((command_len == 9 && strncasecmp(argv[0], "subscribe", command_len) == 0)||
          (command_len == 10 && strncasecmp(argv[0], "psubscribe", command_len) == 0)) {
          if (argc == 2) {
            mrb_hash_set(mrb, ((mrb_hiredis_async_context *) async_context->ev.data)->subscriptions, mrb_argv[0], block_cb_data);
          } else {
            mrb_raise(mrb, E_ARGUMENT_ERROR, "hiredis only supports one topic Subscribtions");
          }
        }
        else if ((command_len == 11 && strncasecmp(argv[0], "unsubscribe", command_len) == 0)||
          (command_len == 12 && strncasecmp(argv[0], "punsubscribe", command_len) == 0)) {
          if (argc == 2) {
            mrb_hash_delete_key(mrb, ((mrb_hiredis_async_context *) async_context->ev.data)->subscriptions, mrb_argv[0]);
          } else {
            mrb_raise(mrb, E_ARGUMENT_ERROR, "hiredis only supports one topic Subscribtions");
          }
        }
        else if (command_len == 7 && strncasecmp(argv[0], "monitor", command_len) == 0) {
          mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "monitor"), block_cb_data);
        }
        else {
          mrb_ary_push(mrb, ((mrb_hiredis_async_context *) async_context->ev.data)->replies, block_cb_data);
        }
      }
    } else {
      const char **argv = NULL;
      size_t *argvlen = NULL;
      mrb_hiredis_generate_argv_argc_array(mrb, command, mrb_argv, &argc, &argv, &argvlen);
      rc = redisAsyncCommandArgv(async_context, NULL, NULL, argc, argv, argvlen);
      mrb_free(mrb, argv);
      mrb_free(mrb, argvlen);
    }
    if (likely(rc == REDIS_OK)) {
      return self;
    } else {
      mrb_hiredis_check_error(&async_context->c, mrb);
      return mrb_false_value();
    }
  } else {
    mrb_raise(mrb, E_IO_ERROR, "closed stream");
    return mrb_false_value();
  }
}

static mrb_value
mrb_redisAsyncDisconnect(mrb_state *mrb, mrb_value self)
{
  redisAsyncContext *async_context = (redisAsyncContext *) DATA_PTR(self);
  if (likely(async_context)) {
    redisAsyncDisconnect(async_context);
    return mrb_nil_value();
  } else {
    mrb_raise(mrb, E_IO_ERROR, "closed stream");
    return mrb_false_value();
  }
}

void
mrb_mruby_hiredis_gem_init(mrb_state* mrb)
{
  struct RClass *hiredis_class, *hiredis_error_class, *hiredis_async_class, *hiredis_async_block_data_class;
  hiredis_class = mrb_define_class(mrb, "Hiredis", mrb->object_class);
  MRB_SET_INSTANCE_TT(hiredis_class, MRB_TT_DATA);

  hiredis_error_class = mrb_define_class_under(mrb, hiredis_class, "Error", E_RUNTIME_ERROR);
  mrb_define_class_under(mrb, hiredis_class, "ReplyError",    hiredis_error_class);
  mrb_define_class_under(mrb, hiredis_class, "ProtocolError", hiredis_error_class);
  mrb_define_class_under(mrb, hiredis_class, "OOMError",      hiredis_error_class);

  mrb_define_method(mrb, hiredis_class, "initialize", mrb_redisConnect,           MRB_ARGS_OPT(2));
  mrb_define_method(mrb, hiredis_class, "free",       mrb_redisFree,              MRB_ARGS_NONE());
  mrb_define_alias (mrb, hiredis_class, "close", "free");
  mrb_define_method(mrb, hiredis_class, "call",       mrb_redisCommandArgv,       (MRB_ARGS_REQ(1)|MRB_ARGS_REST()));
  mrb_define_method(mrb, hiredis_class, "queue",      mrb_redisAppendCommandArgv, (MRB_ARGS_REQ(1)|MRB_ARGS_REST()));
  mrb_define_method(mrb, hiredis_class, "reply",      mrb_redisGetReply,          MRB_ARGS_NONE());
  mrb_define_method(mrb, hiredis_class, "bulk_reply", mrb_redisGetBulkReply,      MRB_ARGS_NONE());
#if ((HIREDIS_MAJOR == 0) && (HIREDIS_MINOR >= 13) || (HIREDIS_MAJOR > 0))
  mrb_define_method(mrb, hiredis_class, "reconnect",  mrb_redisReconnect,         MRB_ARGS_NONE());
#endif

  hiredis_async_class = mrb_define_class_under(mrb, hiredis_class, "Async", mrb->object_class);
  MRB_SET_INSTANCE_TT(hiredis_async_class, MRB_TT_DATA);
  mrb_define_method(mrb, hiredis_async_class, "initialize", mrb_redisAsyncConnect,      MRB_ARGS_ARG(2, 2));
  mrb_define_method(mrb, hiredis_async_class, "read",       mrb_redisAsyncHandleRead,   MRB_ARGS_NONE());
  mrb_define_method(mrb, hiredis_async_class, "write",      mrb_redisAsyncHandleWrite,  MRB_ARGS_NONE());
  mrb_define_method(mrb, hiredis_async_class, "queue",      mrb_redisAsyncCommandArgv,  (MRB_ARGS_REQ(1)|MRB_ARGS_REST()|MRB_ARGS_BLOCK()));
  mrb_define_method(mrb, hiredis_async_class, "disconnect", mrb_redisAsyncDisconnect,   MRB_ARGS_NONE());
  hiredis_async_block_data_class = mrb_define_class_under(mrb, hiredis_async_class, "_BlockData", mrb->object_class);
  MRB_SET_INSTANCE_TT(hiredis_async_block_data_class, MRB_TT_DATA);
}

void mrb_mruby_hiredis_gem_final(mrb_state* mrb) {}
