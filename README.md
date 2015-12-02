# mruby-hiredis
mruby bindings for https://github.com/redis/hiredis

Examples
========

Connect to the default redis-server
```ruby
hiredis = Hiredis.new
hiredis.call(:command)
```

Connect to a tcp redis-server
```ruby
hiredis = Hiredis.new("localhost", 6379) #if you leave the port out it defaults to 6379
hiredis.call(:command)
```

Connect to a unix redis-server
```ruby
hiredis = Hiredis.new("/tmp/redis.sock", -1)
hiredis.call(:command)
```

Pipelining
```ruby
hiredis.append(:set, "foo", "bar")
hiredis.append(:get, "foo")
hiredis.reply
hiredis.reply
```

Transactions
```ruby
hiredis.multi([:incr, "bar"], [:get, "foo"])
```

Reply Error Handling
--------------

When you get a reply back, you have to check if it is a "Hiredis::ReplyError", this was done so pipelined transactions can complete even if there are errors.
