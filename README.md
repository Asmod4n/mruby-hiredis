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
