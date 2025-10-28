class Dote < Formula
  desc "dot e command"
  homepage "https://github.com/Ske-417/homebrew-dote"
  url "https://raw.githubusercontent.com/kmc2400/homebrew-dote/main/dote.c"
  sha256 "97b6cc9b526fafa5707660842c91c4ab96cf6be1966d67d1abd76a28be2fa2ea"
  version "1.0.0"

  def install
    system ENV.cc, "dote.c", "-o", "dote"
    bin.install "dote"
  end
end
