class Hiredis
  class Async
    class Callbacks
      def initialize
        @disconnect = lambda do |async, evloop, status|
          evloop.stop
        end

        @connect = lambda do |async, evloop, status|
          if status == -1
            evloop.stop
          end
        end

        @addRead = lambda do |async, evloop, fd|
          @read_cb = evloop.create_file_event(fd, RedisAe::READABLE) do |fd, mask|
            async.read
          end
        end

        @delRead = lambda do |async, evloop, fd|
          if @read_cb
            evloop.delete_file_event(@read_cb)
            remove_instance_variable(:@read_cb)
          end
        end

        @addWrite = lambda do |async, evloop, fd|
          @write_cb = evloop.create_file_event(fd, RedisAe::WRITABLE) do |fd, mask|
            async.write
          end
        end

        @delWrite = lambda do |async, evloop, fd|
          if @write_cb
            evloop.delete_file_event(@write_cb)
            remove_instance_variable(:@write_cb)
          end
        end
      end

      def disconnect(&block)
        raise ArgumentError, "no block given" unless block_given?
        @disconnect = block
      end

      def connect(&block)
        raise ArgumentError, "no block given" unless block_given?
        @connect = block
      end

      def addRead(&block)
        raise ArgumentError, "no block given" unless block_given?
        if @read_cb
          @evloop.delete_file_event(@read_cb)
          remove_instance_variable(:@read_cb)
        end
        @addRead = block
      end

      def delRead(&block)
        raise ArgumentError, "no block given" unless block_given?
        if @read_cb
          @evloop.delete_file_event(@read_cb)
          remove_instance_variable(:@read_cb)
        end
        @delRead = block
      end

      def addWrite(&block)
        raise ArgumentError, "no block given" unless block_given?
        if @write_cb
          @evloop.delete_file_event(@write_cb)
          remove_instance_variable(:@write_cb)
        end
        @addWrite = block
      end

      def delWrite(&block)
        raise ArgumentError, "no block given" unless block_given?
        if @write_cb
          @evloop.delete_file_event(@write_cb)
          remove_instance_variable(:@write_cb)
        end
        @delWrite = block
      end
    end
  end
end
