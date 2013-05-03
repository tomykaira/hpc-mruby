MRuby::Gem::Specification.new('mruby-bin-hpcmrb') do |spec|
  spec.license = 'MIT'
  spec.authors = 'HPC mruby developers'
  spec.bins = %w(hpcmrb)
  spec.compilers.each do |compiler|
      compiler.defines += %w(HPC_MRUBY) 
  end
end
