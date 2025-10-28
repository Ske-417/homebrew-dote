class Dote < Formula
  desc "Full-screen ANSI meteor shower animation (2 spaces = 1 pixel)"
  homepage "https://github.com/Ske-417/homebrew-meteor"
  url "https://raw.githubusercontent.com/Ske-417/homebrew-dote/main/dote.c"
  sha256 "2dfd8b37383019075e4ab9baf70ffc9893659966900f6e055f9d6d830e4d496d"
  license "MIT"
  version "1.0.0"

  def install
    system ENV.cc, "dote.c", "-o", "dote"
    bin.install "dote"
  end

  test do
    assert_match "dote", shell_output("#{bin}/dote --version")
  end
end
