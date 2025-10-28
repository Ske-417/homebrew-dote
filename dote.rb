class Dote < Formula
  desc "dot e command"
  homepage "https://github.com/Ske-417/homebrew-dote"
  url "https://raw.githubusercontent.com/kmc2400/homebrew-dote/main/dote.c"
  sha256 "8d431774d17018b80c7bb0668fe5d0df4ab3bacb317067754eab4143772d30f5"
  version “1.0.0”

  def install
    system ENV.cc, "dote.c", "-o", "dote"
    bin.install "dote"
  end
end
