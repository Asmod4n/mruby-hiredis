[![Build Status](https://travis-ci.com/Asmod4n/mruby-hiredis.svg?branch=master)](https://travis-ci.com/Asmod4n/mruby-hiredis)
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

All [Redis Commands](http://redis.io/commands) are mapped to Ruby Methods, this happens automatically when you connect the first time to a Server.
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
hiredis.bulk_reply
```

Transactions
```ruby
hiredis.transaction([:incr, "bar"], [:get, "foo"])
```

Subscriptions
```ruby
hiredis.subscribe('channel')

loop do
  puts hiredis.reply
end
```

Async Client
------------

```ruby
async = Hiredis::Async.new #(callbacks = Hiredis::Async::Callbacks.new, evloop = RedisAe.new, host_or_path = "localhost", port = 6379)
async.evloop.run_once
async.queue(:set, "foo", "bar")
async.evloop.run_once
async.queue(:get, "foo") {|reply| puts reply}
async.evloop.run_once
```

Disque
------

https://github.com/antirez/disque made it mandatory to change the way mruby-hiredis handles status replies, they have been symbols in the past, but are Strings now. This brakes pipelining and transactions for existing applications. Because of that the major version got incremented.

Reply Error Handling
--------------

When you get a reply back, you have to check if it is a "Hiredis::ReplyError", this was done so pipelined transactions can complete even if there are errors.


Acknowledgements
----------------
This is using code from https://github.com/redis/hiredis

Copyright (c) 2009-2011, Salvatore Sanfilippo <antirez at gmail dot com>
Copyright (c) 2010-2011, Pieter Noordhuis <pcnoordhuis at gmail dot com>

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of Redis nor the names of its contributors may be used
  to endorse or promote products derived from this software without specific
  prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
