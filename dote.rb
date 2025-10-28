class Dote < Formula
  desc "Full-screen ANSI meteor shower animation (2 spaces = 1 pixel)"
  homepage "https://github.com/Ske-417/homebrew-meteor"
  url "https://raw.githubusercontent.com/Ske-417/homebrew-dote/main/dote.c"
  sha256 "<後で差し替え>"
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
