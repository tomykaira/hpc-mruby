class Hello
  def initialize
    @x = 0
  end

  def x
    @x
  end

  def x=(other)
    @x = other
  end
end

h = Hello.new
h.x = 30
puts h.x
