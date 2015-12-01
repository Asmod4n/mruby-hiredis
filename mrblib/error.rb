unless Object.const_defined?("EOFError")
  class EOFError < StandardError; end
end
