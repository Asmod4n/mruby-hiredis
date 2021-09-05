class Hiredis
  class << self
    attr_accessor :created_shortcuts

    def new(*args)
      instance = super(*args)
      unless self.created_shortcuts
        create_shortcuts(instance)
        self.created_shortcuts = true
      end
      instance.call(:hello, "3")
      instance
    end

    def create_shortcuts(hiredis)
      hiredis.call(:command).each do |command|
        command = command.first.to_sym
        define_method(command) do |*args|
          call(command, *args)
        end
      end
      self
    end
  end #class << self

  def transaction(*commands)
    queue(:multi)
    commands.each do |command|
      queue(*command)
    end
    queue(:exec)
    bulk_reply
  rescue => e
    call(:discard)
    raise e
  end

  def [](key)
    call(:get, key)
  end

  def []=(key, value)
    call(:set, key, value)
  end
end
