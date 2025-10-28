class Dote < Formula
  desc "dot e command"
  homepage "https://github.com/Ske-417/homebrew-dote"
  url "https://raw.githubusercontent.com/kmc2400/homebrew-dote/main/dote.c"
  sha256 "407a6e76c5fb79e041b56936926f90625748cc63369ce31292092dcea29a6ab1"
  version "1.0.0"

  def install
    system ENV.cc, "dote.c", "-o", "dote"
    bin.install "dote"
  end
end
