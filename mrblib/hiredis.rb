class Hiredis
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
end
