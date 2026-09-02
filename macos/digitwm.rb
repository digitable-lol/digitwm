# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: ISC
#
# Формула homebrew для digitwm на macOS.
#
# ЭТА ФОРМУЛА НИКОГДА НЕ ИСПОЛНЯЛАСЬ. Мака в проекте нет; "brew install"
# отсюда не запускали ни разу, и синтаксис проверен только чтением
# документации homebrew. Что известно точно и почему формула выглядит так -
# в doc/macos-install.md, раздел про доставку.
#
# Почему именно brew, а не Mac App Store: Apple прямо перечисляет "Use of
# accessibility APIs in assistive apps" среди того, что запрещено в песочнице
# ("Protecting user data with App Sandbox"), а App Store без песочницы не
# принимает. Значит остаётся обычный исполняемый файл, и brew - его дорога.
#
# Установка, пока стабильного выпуска с маковской целью нет:
#
#   brew tap digitable-lol/digitwm https://github.com/digitable-lol/digitwm
#   brew install --HEAD digitable-lol/digitwm/digitwm
#
# или, без tap, прямо из файла:
#
#   brew install --HEAD --build-from-source ./macos/digitwm.rb
class Digitwm < Formula
  desc "Ribbon window manager for macOS: real windows, through the Accessibility API"
  homepage "https://digitable.life"
  head "https://github.com/digitable-lol/digitwm.git", branch: "main"
  license "ISC"

  depends_on :macos
  depends_on xcode: :build

  def install
    # Собирается только маковская цель: корневой Makefile - это X11-сборка,
    # которой на маке нечем управлять.
    system "make", "-C", "macos"
    bin.install "macos/digitwm"
    man5.install "cwmrc.5"
  end

  def caveats
    <<~EOS
      digitwm moves windows that belong to other applications, so macOS will
      not let it do anything at all until you allow it:

        System Settings > Privacy & Security > Accessibility > + > digitwm

      The binary to add is #{opt_bin}/digitwm. digitwm asks for this itself the
      first time it is started, and then exits - the grant is read once, at
      start-up, so start it again afterwards.

      The grant is remembered against the SIGNATURE of the binary. This formula
      builds from source and signs ad-hoc, which means every reinstall or
      upgrade produces a different signature and macOS asks again. A stable
      signing identity fixes it; doc/macos-install.md says how.

      What it does, before you press anything:

        digitwm -k    the key table (Control-Option-H/J/K/L moves the focus)
        digitwm -n    what it made of your ~/.cwmrc
        digitwm -N    every Apple call this port makes, one at a time, with
                      the ones that did not answer named
    EOS
  end

  test do
    # Ни одна из этих двух не трогает окна и не требует разрешения: одна
    # печатает таблицу клавиш, другая - разбор пустой конфигурации.
    assert_match "bind-key", shell_output("#{bin}/digitwm -k")
    (testpath/"cwmrc").write("ribbongap 12\n")
    assert_match "1 taken", shell_output("#{bin}/digitwm -n -c #{testpath}/cwmrc")
  end
end
