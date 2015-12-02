class Hiredis
  class << self
    attr_accessor :created_shortcuts

    def new(*args)
      instance = super(*args)
      unless self.created_shortcuts
        create_shortcuts(instance)
        self.created_shortcuts = true
      end
      instance
    end

    def create_shortcuts(hiredis)
      hiredis.call(:command).each do |command|
        command = command.first.to_sym
        define_method(command) do |*args|
          call(command, *args)
        end
      end
    end
  end

  def multi(*commands)
    append(:multi)
    commands.each do |command|
      append(*command)
    end
    append(:exec)
    ret = []
    (2+commands.size).times do
      ret << reply
    end
    ret
  end

  def [](key)
    call(:get, key)
  end

  def []=(key, value)
    call(:set, key, value)
  end
end
