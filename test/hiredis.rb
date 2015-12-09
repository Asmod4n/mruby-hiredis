assert("Hiredis.new") do
  assert_raise(TypeError) { Hiredis.new(17) }
  assert_raise(TypeError) { Hiredis.new("localhost", "foo") }
  assert_nothing_raised { Hiredis.new }
  assert_nothing_raised { Hiredis.new("localhost", 6379) }
end

assert("unknown Redis command") do
  hiredis = Hiredis.new
  assert_kind_of(Hiredis::ReplyError, hiredis.call(:nonexistant))
end

assert("Pipelined commands") do
  hiredis = Hiredis.new
  hiredis.queue(:set, "mruby-hiredis-test:foo", "bar")
  hiredis.queue(:get, "mruby-hiredis-test:foo")
  assert_equal(:OK,   hiredis.reply)
  assert_equal("bar", hiredis.reply)

  hiredis.queue(:nonexistant)
  assert_kind_of(Hiredis::ReplyError, hiredis.reply)
  hiredis.del("mruby-hiredis-test:foo")
end

assert("Hiredis#transaction") do
  hiredis = Hiredis.new
  ret = hiredis.transaction([:set, "mruby-hiredis-test:foo", "bar"], [:get, "mruby-hiredis-test:foo"])
  assert_equal([:OK, :QUEUED, :QUEUED, [:OK, "bar"]], ret)
  hiredis.del("mruby-hiredis-test:foo")
end

assert("Hiredis#get") do
  hiredis = Hiredis.new
  assert_nil(hiredis.get("nonexistant"))
  hiredis["mruby-hiredis-test:foo"] = "bar"
  assert_equal("bar", hiredis.get("mruby-hiredis-test:foo"))
  hiredis.del("mruby-hiredis-test:foo")
end

assert("Hiredis#incr") do
  hiredis = Hiredis.new
  hiredis.del("mruby-hiredis-test:foo")
  assert_equal(1, hiredis.incr("mruby-hiredis-test:foo"))
  hiredis.del("mruby-hiredis-test:foo")
end

assert("Throws EOFError when Server closed connection") do
  hiredis = Hiredis.new
  hiredis.call(:quit)
  assert_raise(EOFError) { hiredis.get("foo") }
end

assert("Defines IOError when missing") do
  assert_equal(IOError.superclass, StandardError)
end

assert("Defines EOFError when missing") do
  assert_equal(EOFError.superclass, IOError)
end

