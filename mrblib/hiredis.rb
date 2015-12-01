class Hiredis
  def multi(*commands)
    append('MULTI')
    commands.each do |command|
      append(*command)
    end
    append('EXEC')
    ret = []
    (2+commands.size).times do
      ret << reply
    end
    ret
  end
end
