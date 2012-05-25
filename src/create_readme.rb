require 'openssl'
require 'open-uri'
# http://stackoverflow.com/questions/1113422/how-to-bypass-ssl-certificate-verification-in-open-uri
OpenSSL::SSL::VERIFY_PEER = OpenSSL::SSL::VERIFY_NONE

require 'redcarpet'

class Package
  
  class R < Redcarpet::Render::XHTML
    attr_reader :downloads
    
    def initialize(*a)
      @downloads = []
      super
    end
    
    def link(link, title, content)
      tpl = '<a href="%s" title="%s">%s</a>'
      tpl % [rewrite(link), title, content]
    end

    def image(link, title, alt_text)
      tpl = '<img style="max-width: 912px" src="%s" title="%s" alt="%s" />'
      tpl % [rewrite(link), title, alt_text]
    end

    private

    # Ideally this will make links relative
    def rewrite(link)
      return link unless link =~ /^https\:\/\/github/
      @downloads.push(link)
      File.basename(link)
    end
  end
  
  def self.for(*names)
    names.flatten.map{|name| new(name)}
  end
  
  def initialize(name)
    @name = name
  end
  
  def repo_url
    "https://github.com/julik/%s" % @name
  end
  
  def doco_dir
    File.join(File.dirname(__FILE__), "site", "scripts", @name)
  end
  
  def package!
    render_engine = R.new
    md = Redcarpet::Markdown.new(render_engine)
    
    # Download the readme
    readme_handle = open(File.join(repo_url, "/raw/master/README.markdown"))
    
    rewritten_readme_html = md.render(readme_handle.read)
    
    assets = render_engine.downloads.map do |e| 
      url = e
      url = url.gsub(/ /, '%20')
      begin
        [File.basename(e), open(url)]
      rescue OpenURI::HTTPError => e
        raise "Cannot download #{url.inspect}"
      end
    end
    
    FileUtils.mkdir_p(doco_dir)
    
    # Write out the readme
    File.open(File.join(doco_dir, "README.html"), "wb") do | index |
      index.write(rewritten_readme_html)
    end
    
    assets.each do | a |
      # Write out the index.erb
      File.open(File.join(doco_dir, a[0]), "wb") do | index |
        index.write(a[1].read)
      end
    end
  end
end


Package.for("sylens").each{|p| p.package! }