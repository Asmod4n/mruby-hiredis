#include "mruby/hiredis.h"
#include "mrb_hiredis.h"

#if (__GNUC__ >= 3) || (__INTEL_COMPILER >= 800) || defined(__clang__)
# define likely(x) __builtin_expect(!!(x), 1)
# define unlikely(x) __builtin_expect(!!(x), 0)
#else
# define likely(x) (x)
# define unlikely(x) (x)
#endif

MRB_INLINE void
mrb_hiredis_check_error(const redisContext *context, mrb_state *mrb)
{
  if (context->err != 0) {
    if (errno) {
      mrb_sys_fail(mrb, context->errstr);
    } else {
      switch (context->err) {
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
    mrb_hiredis_check_error(context, mrb);
  } else {
    mrb_sys_fail(mrb, "redisConnect");
  }

  return self;
}

MRB_INLINE mrb_value
mrb_hiredis_get_ary_reply(redisReply *reply, mrb_state *mrb);

MRB_INLINE mrb_value
mrb_hiredis_get_reply(redisReply *reply, mrb_state *mrb)
{
  if (likely(reply)) {
    switch (reply->type) {
      case REDIS_REPLY_STRING:
      case REDIS_REPLY_STATUS:
        return mrb_str_new(mrb, reply->str, reply->len);
        break;
      case REDIS_REPLY_ARRAY:
        return mrb_hiredis_get_ary_reply(reply, mrb);
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
      case REDIS_REPLY_ERROR: {
        mrb_value err = mrb_str_new(mrb, reply->str, reply->len);
        return mrb_exc_new_str(mrb, E_HIREDIS_REPLY_ERROR, err);
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
  for (size_t element_couter = 0; element_couter < reply->elements; element_couter++) {
    mrb_value element = mrb_hiredis_get_reply(reply->element[element_couter], mrb);
    mrb_ary_push(mrb, ary, element);
    mrb_gc_arena_restore(mrb, ai);
  }
  return ary;
}

static mrb_value
mrb_redisCommandArgv(mrb_state *mrb, mrb_value self)
{
  mrb_sym command;
  mrb_value *mrb_argv;
  mrb_int argc = 0;

  mrb_get_args(mrb, "n*", &command, &mrb_argv, &argc);
  argc++;

  const char *argv[argc];
  size_t argvlen[argc];
  mrb_int command_len;
  argv[0] = mrb_sym2name_len(mrb, command, &command_len);
  argvlen[0] = command_len;
  mrb_value reply_val = self;

  for (mrb_int argc_current = 1; argc_current < argc; argc_current++) {
    mrb_value curr = mrb_str_to_str(mrb, mrb_argv[argc_current - 1]);
    argv[argc_current] = RSTRING_PTR(curr);
    argvlen[argc_current] = RSTRING_LEN(curr);
  }

  redisContext *context = (redisContext *) DATA_PTR(self);
  errno = 0;
  redisReply *reply = redisCommandArgv(context, argc, argv, argvlen);
  if (likely(reply != NULL)) {
    struct mrb_jmpbuf* prev_jmp = mrb->jmp;
    struct mrb_jmpbuf c_jmp;

    MRB_TRY(&c_jmp)
    {
      mrb->jmp = &c_jmp;
      reply_val = mrb_hiredis_get_reply(reply, mrb);
      mrb->jmp = prev_jmp;
    }
    MRB_CATCH(&c_jmp)
    {
      mrb->jmp = prev_jmp;
      freeReplyObject(reply);
      MRB_THROW(mrb->jmp);
    }
    MRB_END_EXC(&c_jmp);

    freeReplyObject(reply);
  } else {
    mrb_hiredis_check_error(context, mrb);
  }

  return reply_val;
}

static mrb_value
mrb_redisAppendCommandArgv(mrb_state *mrb, mrb_value self)
{
  mrb_sym command;
  mrb_value *mrb_argv;
  mrb_int argc = 0;

  mrb_get_args(mrb, "n*", &command, &mrb_argv, &argc);
  argc++;

  const char *argv[argc];
  size_t argvlen[argc];
  mrb_int command_len;
  argv[0] = mrb_sym2name_len(mrb, command, &command_len);
  argvlen[0] = command_len;

  for (mrb_int argc_current = 1; argc_current < argc; argc_current++) {
    mrb_value curr = mrb_str_to_str(mrb, mrb_argv[argc_current - 1]);
    argv[argc_current] = RSTRING_PTR(curr);
    argvlen[argc_current] = RSTRING_LEN(curr);
  }

  mrb_sym queue_counter_sym = mrb_intern_lit(mrb, "queue_counter");
  mrb_value queue_counter_val = mrb_iv_get(mrb, self, queue_counter_sym);
  mrb_int queue_counter = 1;
  if (mrb_fixnum_p(queue_counter_val)) {
    queue_counter = mrb_fixnum(queue_counter_val);
    if (unlikely(mrb_int_add_overflow(queue_counter, 1, &queue_counter))) {
      mrb_raise(mrb, E_RUNTIME_ERROR, "integer addition would overflow");
    }
  }

  redisContext *context = (redisContext *) DATA_PTR(self);
  errno = 0;
  int rc = redisAppendCommandArgv(context, argc, argv, argvlen);
  if (likely(rc == REDIS_OK)) {
    mrb_iv_set(mrb, self, queue_counter_sym, mrb_fixnum_value(queue_counter));
  } else {
    mrb_hiredis_check_error(context, mrb);
  }

  return self;
}

static mrb_value
mrb_redisGetReply(mrb_state *mrb, mrb_value self)
{
  mrb_sym queue_counter_sym = mrb_intern_lit(mrb, "queue_counter");
  mrb_value queue_counter_val = mrb_iv_get(mrb, self, queue_counter_sym);
  mrb_int queue_counter = -1;
  if (mrb_fixnum_p(queue_counter_val)) {
    queue_counter = mrb_fixnum(queue_counter_val);
  }

  redisContext *context = (redisContext *) DATA_PTR(self);
  redisReply *reply = NULL;
  mrb_value reply_val = self;
  errno = 0;
  int rc = redisGetReply(context, (void **) &reply);
  if (likely(rc == REDIS_OK && reply != NULL)) {
    struct mrb_jmpbuf* prev_jmp = mrb->jmp;
    struct mrb_jmpbuf c_jmp;

    MRB_TRY(&c_jmp)
    {
      mrb->jmp = &c_jmp;
      reply_val = mrb_hiredis_get_reply(reply, mrb);
      if (queue_counter > 1) {
        mrb_iv_set(mrb, self, queue_counter_sym, mrb_fixnum_value(--queue_counter));
      } else {
        mrb_iv_remove(mrb, self, queue_counter_sym);
      }
      mrb->jmp = prev_jmp;
    }
    MRB_CATCH(&c_jmp)
    {
      mrb->jmp = prev_jmp;
      freeReplyObject(reply);
      MRB_THROW(mrb->jmp);
    }
    MRB_END_EXC(&c_jmp);

    freeReplyObject(reply);
  } else {
    mrb_hiredis_check_error(context, mrb);
  }

  return reply_val;
}

static mrb_value
mrb_redisGetBulkReply(mrb_state *mrb, mrb_value self)
{
  mrb_value queue_counter_val = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "queue_counter"));

  if (unlikely(!mrb_fixnum_p(queue_counter_val))) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "nothing queued yet");
  }

  mrb_int queue_counter = mrb_fixnum(queue_counter_val);

  mrb_value bulk_reply = mrb_ary_new_capa(mrb, queue_counter);
  int ai = mrb_gc_arena_save(mrb);

  do {
    mrb_value reply = mrb_redisGetReply(mrb, self);
    mrb_ary_push(mrb, bulk_reply, reply);
    mrb_gc_arena_restore(mrb, ai);
  } while (--queue_counter > 0);

  return bulk_reply;
}

static mrb_value
mrb_redisReconnect(mrb_state *mrb, mrb_value self)
{
  redisContext *context = (redisContext *) DATA_PTR(self);

  int rc = redisReconnect(context);
  if (unlikely(rc == REDIS_ERR)) {
    mrb_hiredis_check_error(context, mrb);
  }

  return self;
}

MRB_INLINE void
mrb_hiredis_addRead(void *privdata)
{
  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) privdata;
  mrb_state *mrb = mrb_async_context->mrb;
  int arena_index = mrb_gc_arena_save(mrb);

  mrb_value block = mrb_iv_get(mrb, mrb_async_context->callbacks, mrb_intern_lit(mrb, "@addRead"));
  if (unlikely(mrb_nil_p(block))) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "addRead callback missing");
    return;
  }

  mrb_value argv[3];
  argv[0] = mrb_async_context->self;
  argv[1] = mrb_async_context->evloop;
  argv[2] = mrb_fixnum_value(mrb_async_context->fd);

  mrb_yield_argv(mrb, block, 3, argv);
  mrb_gc_arena_restore(mrb, arena_index);
}

MRB_INLINE void
mrb_hiredis_delRead(void *privdata)
{
  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) privdata;
  mrb_state *mrb = mrb_async_context->mrb;
  int arena_index = mrb_gc_arena_save(mrb);

  mrb_value block = mrb_iv_get(mrb, mrb_async_context->callbacks, mrb_intern_lit(mrb, "@delRead"));
  if (unlikely(mrb_nil_p(block))) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "delRead callback missing");
    return;
  }

  mrb_value argv[3];
  argv[0] = mrb_async_context->self;
  argv[1] = mrb_async_context->evloop;
  argv[2] = mrb_fixnum_value(mrb_async_context->fd);

  mrb_yield_argv(mrb, block, 3, argv);
  mrb_gc_arena_restore(mrb, arena_index);
}

MRB_INLINE void
mrb_hiredis_addWrite(void *privdata)
{
  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) privdata;
  mrb_state *mrb = mrb_async_context->mrb;
  int arena_index = mrb_gc_arena_save(mrb);

  mrb_value block = mrb_iv_get(mrb, mrb_async_context->callbacks, mrb_intern_lit(mrb, "@addWrite"));
  if (unlikely(mrb_nil_p(block))) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "addWrite callback missing");
    return;
  }

  mrb_value argv[3];
  argv[0] = mrb_async_context->self;
  argv[1] = mrb_async_context->evloop;
  argv[2] = mrb_fixnum_value(mrb_async_context->fd);

  mrb_yield_argv(mrb, block, 3, argv);
  mrb_gc_arena_restore(mrb, arena_index);
}

MRB_INLINE void
mrb_hiredis_delWrite(void *privdata)
{
  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) privdata;
  mrb_state *mrb = mrb_async_context->mrb;
  int arena_index = mrb_gc_arena_save(mrb);

  mrb_value block = mrb_iv_get(mrb, mrb_async_context->callbacks, mrb_intern_lit(mrb, "@delWrite"));
  if (unlikely(mrb_nil_p(block))) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "delWrite callback missing");
    return;
  }

  mrb_value argv[3];
  argv[0] = mrb_async_context->self;
  argv[1] = mrb_async_context->evloop;
  argv[2] = mrb_fixnum_value(mrb_async_context->fd);

  mrb_yield_argv(mrb, block, 3, argv);
  mrb_gc_arena_restore(mrb, arena_index);
}

MRB_INLINE void
mrb_hiredis_cleanup(void *privdata)
{
  if (!privdata)
    return;

  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) privdata;
  mrb_state *mrb = mrb_async_context->mrb;
  if (!mrb)
    return;

  int arena_index = mrb_gc_arena_save(mrb);

  mrb_value block = mrb_iv_get(mrb, mrb_async_context->callbacks, mrb_intern_lit(mrb, "@cleanup"));
  if (unlikely(mrb_nil_p(block))) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "cleanup callback missing");
    return;
  }

  mrb_value argv[3];
  argv[0] = mrb_async_context->self;
  argv[1] = mrb_async_context->evloop;
  argv[2] = mrb_fixnum_value(mrb_async_context->fd);

  mrb_yield_argv(mrb, block, 3, argv);
  mrb_gc_arena_restore(mrb, arena_index);

  mrb_data_init(mrb_async_context->self, NULL, NULL);
  mrb_free(mrb, privdata);
  mrb_async_context->async_context->ev.data = NULL;
}

MRB_INLINE void
mrb_redisDisconnectCallback(const struct redisAsyncContext *async_context, int status)
{
  if (unlikely(!async_context->ev.data))
    return;

  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) async_context->ev.data;
  mrb_state *mrb = mrb_async_context->mrb;
  if (unlikely(!mrb))
    return;

  int arena_index = mrb_gc_arena_save(mrb);

  mrb_value block = mrb_iv_get(mrb, mrb_async_context->callbacks, mrb_intern_lit(mrb, "@disconnect"));
  if (unlikely(mrb_nil_p(block))) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "disconnect callback missing");
    return;
  }

  mrb_value argv[3];
  argv[0] = mrb_async_context->self;
  argv[1] = mrb_async_context->evloop;
  argv[2] = mrb_fixnum_value(status);

  mrb_yield_argv(mrb, block, 3, argv);
  mrb_gc_arena_restore(mrb, arena_index);
  if (unlikely(status == REDIS_ERR)) {
    mrb_hiredis_check_error(&async_context->c, mrb);
  }
}

MRB_INLINE void
mrb_redisConnectCallback(const struct redisAsyncContext *async_context, int status)
{
  if (unlikely(!async_context->ev.data))
    return;

  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) async_context->ev.data;
  mrb_state *mrb = mrb_async_context->mrb;
  if (unlikely(!mrb))
    return;

  int arena_index = mrb_gc_arena_save(mrb);

  mrb_value block = mrb_iv_get(mrb, mrb_async_context->callbacks, mrb_intern_lit(mrb, "@connect"));
  if (unlikely(mrb_nil_p(block))) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "connect callback missing");
    return;
  }

  mrb_value argv[3];
  argv[0] = mrb_async_context->self;
  argv[1] = mrb_async_context->evloop;
  argv[2] = mrb_fixnum_value(status);

  mrb_yield_argv(mrb, block, 3, argv);
  mrb_gc_arena_restore(mrb, arena_index);

  if (unlikely(status == REDIS_ERR)) {
    mrb_hiredis_check_error(&async_context->c, mrb_async_context->mrb);
  }
}

MRB_INLINE void
mrb_hiredis_setup_async_context(mrb_state *mrb, mrb_value self, mrb_value callbacks, mrb_value evloop, redisAsyncContext *async_context)
{
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@callbacks"), callbacks);
  mrb_iv_set(mrb, callbacks, mrb_intern_lit(mrb, "@evloop"), evloop);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "@evloop"), evloop);
  mrb_value replies = mrb_ary_new(mrb);
  mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "replies"), replies);

  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) mrb_malloc(mrb, sizeof(mrb_hiredis_async_context));
  mrb_async_context->mrb = mrb;
  mrb_async_context->self = self;
  mrb_async_context->callbacks = callbacks;
  mrb_async_context->evloop = evloop;
  mrb_async_context->async_context = async_context;
  mrb_async_context->fd = (&(async_context->c))->fd;
  mrb_async_context->replies = replies;
  mrb_async_context->subscribe = mrb_nil_value();

  async_context->ev.data = mrb_async_context;
  async_context->ev.addRead = mrb_hiredis_addRead;
  async_context->ev.delRead = mrb_hiredis_delRead;
  async_context->ev.addWrite = mrb_hiredis_addWrite;
  async_context->ev.delWrite = mrb_hiredis_delWrite;
  async_context->ev.cleanup = mrb_hiredis_cleanup;
  redisAsyncSetDisconnectCallback(async_context, mrb_redisDisconnectCallback);
  redisAsyncSetConnectCallback(async_context, mrb_redisConnectCallback);
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
    mrb_hiredis_check_error(&async_context->c, mrb);
    mrb_hiredis_setup_async_context(mrb, self, callbacks, evloop, async_context);
  } else {
    mrb_sys_fail(mrb, "redisAsyncConnect");
  }

  return self;
}

static mrb_value
mrb_redisAsyncHandleRead(mrb_state *mrb, mrb_value self)
{
  redisAsyncHandleRead((redisAsyncContext *) DATA_PTR(self));

  return self;
}

static mrb_value
mrb_redisAsyncHandleWrite(mrb_state *mrb, mrb_value self)
{
  redisAsyncHandleWrite((redisAsyncContext *) DATA_PTR(self));

  return self;
}

MRB_INLINE void
mrb_redisCallbackFn(struct redisAsyncContext *async_context, void *r, void *privdata)
{
  if (unlikely(!async_context->ev.data))
    return;

  mrb_hiredis_async_context *mrb_async_context = (mrb_hiredis_async_context *) async_context->ev.data;
  mrb_state *mrb = mrb_async_context->mrb;
  if (unlikely(!mrb))
    return;

  int arena_index = mrb_gc_arena_save(mrb);
  mrb_value reply = mrb_hiredis_get_reply((redisReply *) r, mrb);
  mrb_value block = mrb_nil_value();
  if (((&(async_context->c))->flags & REDIS_SUBSCRIBED)||((&(async_context->c))->flags & REDIS_MONITORING)) {
    block = mrb_async_context->subscribe;
  } else {
    block = mrb_ary_shift(mrb, mrb_async_context->replies);
  }
  if (likely(!mrb_nil_p(block))) {
    mrb_yield(mrb, block, reply);
  }
  mrb_gc_arena_restore(mrb, arena_index);
}

static mrb_value
mrb_redisAsyncCommandArgv(mrb_state *mrb, mrb_value self)
{
  mrb_sym command;
  mrb_value *mrb_argv;
  mrb_int argc = 0;
  mrb_value block = mrb_nil_value();

  mrb_get_args(mrb, "n*&", &command, &mrb_argv, &argc, &block);
  argc++;

  const char *argv[argc];
  size_t argvlen[argc];
  mrb_int command_len;
  argv[0] = mrb_sym2name_len(mrb, command, &command_len);
  argvlen[0] = command_len;

  for (mrb_int argc_current = 1; argc_current < argc; argc_current++) {
    mrb_value curr = mrb_str_to_str(mrb, mrb_argv[argc_current - 1]);
    argv[argc_current] = RSTRING_PTR(curr);
    argvlen[argc_current] = RSTRING_LEN(curr);
  }

  redisAsyncContext *async_context = (redisAsyncContext *) DATA_PTR(self);
  errno = 0;
  int rc;
  if (mrb_nil_p(block)) {
    rc = redisAsyncCommandArgv(async_context, NULL, NULL, argc, argv, argvlen);
  } else {
    rc = redisAsyncCommandArgv(async_context, mrb_redisCallbackFn, NULL, argc, argv, argvlen);
    if (likely(rc == REDIS_OK)) {
      if ((command_len == 9 && strncasecmp(argv[0], "subscribe", command_len) == 0)||
        (command_len == 10 && strncasecmp(argv[0], "psubscribe", command_len) == 0)||
        (command_len == 7 && strncasecmp(argv[0], "monitor", command_len) == 0)) {
        ((mrb_hiredis_async_context *) async_context->ev.data)->subscribe = block;
        mrb_iv_set(mrb, self, mrb_intern_lit(mrb, "subscribe"), block);
      }
      else if ((command_len == 11 && strncasecmp(argv[0], "unsubscribe", command_len) == 0)||
        (command_len == 12 && strncasecmp(argv[0], "punsubscribe", command_len) == 0)) {
        ((mrb_hiredis_async_context *) async_context->ev.data)->subscribe = mrb_nil_value();
        mrb_iv_remove(mrb, self, mrb_intern_lit(mrb, "subscribe"));
      }
      else {
        mrb_ary_push(mrb, ((mrb_hiredis_async_context *) async_context->ev.data)->replies, block);
      }
    }
  }
  if (unlikely(rc == REDIS_ERR)) {
    mrb_hiredis_check_error(&async_context->c, mrb);
  }

  return self;
}

static mrb_value
mrb_redisAsyncDisconnect(mrb_state *mrb, mrb_value self)
{
  redisAsyncDisconnect((redisAsyncContext *) DATA_PTR(self));

  return mrb_nil_value();
}

void
mrb_mruby_hiredis_gem_init(mrb_state* mrb)
{
  struct RClass *hiredis_class, *hiredis_error_class, *hiredis_async_class;
  hiredis_class = mrb_define_class(mrb, "Hiredis", mrb->object_class);
  MRB_SET_INSTANCE_TT(hiredis_class, MRB_TT_DATA);

  hiredis_error_class = mrb_define_class_under(mrb, hiredis_class, "Error", E_RUNTIME_ERROR);
  mrb_define_class_under(mrb, hiredis_class, "ReplyError",    hiredis_error_class);
  mrb_define_class_under(mrb, hiredis_class, "ProtocolError", hiredis_error_class);
  mrb_define_class_under(mrb, hiredis_class, "OOMError",      hiredis_error_class);

  mrb_define_method(mrb, hiredis_class, "initialize", mrb_redisConnect,           MRB_ARGS_OPT(2));
  mrb_define_method(mrb, hiredis_class, "call",       mrb_redisCommandArgv,       (MRB_ARGS_REQ(1)|MRB_ARGS_REST()));
  mrb_define_method(mrb, hiredis_class, "queue",      mrb_redisAppendCommandArgv, (MRB_ARGS_REQ(1)|MRB_ARGS_REST()));
  mrb_define_method(mrb, hiredis_class, "reply",      mrb_redisGetReply,          MRB_ARGS_NONE());
  mrb_define_method(mrb, hiredis_class, "bulk_reply", mrb_redisGetBulkReply,      MRB_ARGS_NONE());
  mrb_define_method(mrb, hiredis_class, "reconnect",  mrb_redisReconnect,         MRB_ARGS_NONE());

  hiredis_async_class = mrb_define_class_under(mrb, hiredis_class, "Async", mrb->object_class);
  MRB_SET_INSTANCE_TT(hiredis_async_class, MRB_TT_DATA);
  mrb_define_method(mrb, hiredis_async_class, "initialize", mrb_redisAsyncConnect,      MRB_ARGS_ARG(2, 2));
  mrb_define_method(mrb, hiredis_async_class, "read",       mrb_redisAsyncHandleRead,   MRB_ARGS_NONE());
  mrb_define_method(mrb, hiredis_async_class, "write",      mrb_redisAsyncHandleWrite,  MRB_ARGS_NONE());
  mrb_define_method(mrb, hiredis_async_class, "queue",      mrb_redisAsyncCommandArgv,  (MRB_ARGS_REQ(1)|MRB_ARGS_REST()|MRB_ARGS_BLOCK()));
  mrb_define_method(mrb, hiredis_async_class, "disconnect", mrb_redisAsyncDisconnect,   MRB_ARGS_NONE());
}

void mrb_mruby_hiredis_gem_final(mrb_state* mrb) {}
