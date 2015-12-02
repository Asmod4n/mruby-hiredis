#include "mruby/hiredis.h"
#include "mrb_hiredis.h"

#if (__GNUC__ >= 3) || (__INTEL_COMPILER >= 800) || defined(__clang__)
# define likely(x) __builtin_expect(!!(x), 1)
# define unlikely(x) __builtin_expect(!!(x), 0)
#else
# define likely(x) (x)
# define unlikely(x) (x)
#endif

static inline void
mrb_hiredis_check_error(redisContext *context, mrb_state *mrb)
{
  if (context->err != 0) {
    if (errno != 0) {
      mrb_sys_fail(mrb, context->errstr);
    } else {
      switch (context->err) {
        case REDIS_ERR_EOF:
          mrb_raise(mrb, E_EOF_ERROR, context->errstr);
          break;
        case REDIS_ERR_PROTOCOL:
          mrb_raise(mrb, E_REDIS_ERR_PROTOCOL, context->errstr);
          break;
        case REDIS_ERR_OOM:
          mrb_raise(mrb, E_REDIS_ERR_OOM, context->errstr);
          break;
        default: {
          mrb_raise(mrb, E_REDIS_ERROR, context->errstr);
        }
      }
    }
  }
}

static mrb_value
mrb_hiredis_init(mrb_state *mrb, mrb_value self)
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
    mrb_hiredis_check_error(context, mrb);
    mrb_data_init(self, context, &mrb_redisContext_type);
  } else {
    mrb_sys_fail(mrb, "redisConnect");
  }

  return self;
}

static inline mrb_value
mrb_hiredis_get_ary_reply(redisReply *reply, mrb_state *mrb);

static inline mrb_value
mrb_hiredis_get_reply(redisReply *reply, mrb_state *mrb)
{
  switch (reply->type) {
    case REDIS_REPLY_STRING:
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
    }
      break;
    case REDIS_REPLY_NIL:
      return mrb_nil_value();
      break;
    case REDIS_REPLY_STATUS: {
      mrb_sym status = mrb_intern(mrb, reply->str, reply->len);
      return mrb_symbol_value(status);
    }
      break;
    case REDIS_REPLY_ERROR: {
      mrb_value err = mrb_str_new(mrb, reply->str, reply->len);
      return mrb_exc_new_str(mrb, E_REDIS_REPLY_ERROR, err);
    }
      break;
    default:
      mrb_raise(mrb, E_REDIS_ERROR, "unknown reply type");
  }
}

static inline mrb_value
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
  redisContext *context = (redisContext *) DATA_PTR(self);

  mrb_sym command;
  mrb_value *mrb_argv;
  mrb_int argc;

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

  errno = 0;
  redisReply *reply = redisCommandArgv(context, argc, argv, argvlen);
  if (likely(reply != NULL)) {
    struct mrb_jmpbuf* prev_jmp = mrb->jmp;
    struct mrb_jmpbuf c_jmp;

    MRB_TRY(&c_jmp)
    {
      mrb->jmp = &c_jmp;
      self = mrb_hiredis_get_reply(reply, mrb);
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

  return self;
}

static mrb_value
mrb_redisAppendCommandArgv(mrb_state *mrb, mrb_value self)
{
  redisContext *context = (redisContext *) DATA_PTR(self);

  mrb_sym command;
  mrb_value *mrb_argv;
  mrb_int argc;

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

  errno = 0;
  int rc = redisAppendCommandArgv(context, argc, argv, argvlen);
  if (unlikely(rc == REDIS_ERR)) {
    mrb_hiredis_check_error(context, mrb);
  }

  return self;
}

static mrb_value
mrb_redisGetReply(mrb_state *mrb, mrb_value self)
{
  redisContext *context = (redisContext *) DATA_PTR(self);

  redisReply *reply = NULL;
  errno = 0;
  int rc = redisGetReply(context, (void **) &reply);
  if (likely(rc == REDIS_OK && reply != NULL)) {
    struct mrb_jmpbuf* prev_jmp = mrb->jmp;
    struct mrb_jmpbuf c_jmp;

    MRB_TRY(&c_jmp)
    {
      mrb->jmp = &c_jmp;
      self = mrb_hiredis_get_reply(reply, mrb);
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

  return self;
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

void mrb_mruby_hiredis_gem_init(mrb_state* mrb)
{
  struct RClass *hiredis_class, *hiredis_error_class;
  hiredis_class = mrb_define_class(mrb, "Hiredis", mrb->object_class);
  MRB_SET_INSTANCE_TT(hiredis_class, MRB_TT_DATA);

  hiredis_error_class = mrb_define_class_under(mrb, hiredis_class, "Error", E_RUNTIME_ERROR);
  mrb_define_class_under(mrb, hiredis_class, "ReplyError",    hiredis_error_class);
  mrb_define_class_under(mrb, hiredis_class, "ProtocolError", hiredis_error_class);
  mrb_define_class_under(mrb, hiredis_class, "OOMError",      hiredis_error_class);

  mrb_define_method(mrb, hiredis_class, "initialize", mrb_hiredis_init,           MRB_ARGS_OPT(2));
  mrb_define_method(mrb, hiredis_class, "call",       mrb_redisCommandArgv,       (MRB_ARGS_REQ(1)|MRB_ARGS_REST()));
  mrb_define_method(mrb, hiredis_class, "append",     mrb_redisAppendCommandArgv, (MRB_ARGS_REQ(1)|MRB_ARGS_REST()));
  mrb_define_method(mrb, hiredis_class, "reply",      mrb_redisGetReply,          MRB_ARGS_NONE());
  mrb_define_method(mrb, hiredis_class, "reconnect",  mrb_redisReconnect,         MRB_ARGS_NONE());
}

void mrb_mruby_hiredis_gem_final(mrb_state* mrb)
{
}
