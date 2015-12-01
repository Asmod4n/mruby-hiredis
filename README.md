# mruby-hiredis
mruby bindings for https://github.com/redis/hiredis

Examples
========

Connect to the default redis-server
```ruby
hiredis = Hiredis.new
hiredis.call("COMMAND")
```

Connect to a tcp redis-server
```ruby
hiredis = Hiredis.new("localhost", 6379) #if you leave the port out it defaults to 6379
hiredis.call("COMMAND")
```

Connect to a unix redis-server
```ruby
hiredis = Hiredis.new("/tmp/redis.sock", -1)
hiredis.call("COMMAND")
```

Pipelining
```ruby
hiredis.append("SET", "foo", "bar")
hiredis.append("GET", "foo")
hiredis.reply
hiredis.reply
```

Transactions
```ruby
hiredis.multi(["INCR", "bar"], ["GET", "foo"])
```

Reply Error Handling
--------------

When you get a reply back, you have to check if it is a Subclass of "Hiredis::ReplyError", this was done so pipelined transactions can complete even if there are errors.

All other Errors throw Exceptions, which are Subclasses of "Hiredis::Error", except when it is a "EOFError".
