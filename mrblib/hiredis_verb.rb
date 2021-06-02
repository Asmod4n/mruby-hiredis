class Hiredis
  class Verb
    attr_reader :to_str, :type
    def initialize(str, type)
      @to_str, @type = str, type
    end
  end
end
