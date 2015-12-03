[![Build Status](https://travis-ci.org/Asmod4n/mruby-hiredis.svg)](https://travis-ci.org/Asmod4n/mruby-hiredis)
# mruby-hiredis
mruby bindings for https://github.com/redis/hiredis

Examples
========

Connect to the default redis-server
```ruby
hiredis = Hiredis.new
```

Connect to a tcp redis-server
```ruby
hiredis = Hiredis.new("localhost", 6379) #if you leave the port out it defaults to 6379
```

Connect to a unix redis-server
```ruby
hiredis = Hiredis.new("/tmp/redis.sock", -1) #set port to -1 so it connects to a unix socket
```

All Redis Commands are mapped to Ruby Methods, this happens automatically when you connect the first time to a Server.
```ruby
hiredis["foo"] = "bar"
hiredis["foo"]
hiredis.incr("bar")
```

If you later on want to add more shortcut methods, because the first Server you connected to is of a lower version than the others, you can call
```ruby
Hiredis.create_shortcuts(hiredis)
```

Pipelining
```ruby
hiredis.queue(:set, "foo", "bar")
hiredis.queue(:get, "foo")
hiredis.reply
hiredis.reply
```

Transactions
```ruby
hiredis.transaction([:incr, "bar"], [:get, "foo"])
```

Reply Error Handling
--------------

When you get a reply back, you have to check if it is a "Hiredis::ReplyError", this was done so pipelined transactions can complete even if there are errors.
