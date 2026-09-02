# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# Формула homebrew для digitwm на macOS.
#
# ЧТО ЭТО. Исходник формулы. Правится здесь; в хранилище формул
# (digitable-lol/homebrew-tap, файл Formula/digitwm.rb) он выкладывается
# копией, и оттуда работает короткая строка
#
#     brew install digitable-lol/tap/digitwm
#
# ЗАПОЛНЕНИЕ. В дереве лежит заготовка: версия и оба отпечатка помечены
# словами-заглушками. Их подставляет scripts/make-formula.sh - с тех самых
# архивов, которые уезжают в выпуск, - и кладёт готовый файл в
# dist/homebrew/digitwm.rb. Отпечаток руками не вписывается: тот, кто впишет
# его по памяти, узнает об ошибке от чужой машины, которая уже не поставилась.
# Незаполненное место останавливает выпуск, а не уезжает в хранилище.
#
# ПОЧЕМУ ГОТОВЫЙ ДВОИЧНЫЙ ФАЙЛ, А НЕ --HEAD. До версии 0.1.0 эта формула умела
# только `brew install --HEAD`: выпусков не было ни одного, ставить было
# нечего. У сборки на машине поставившего есть цена, и она не в минутах.
# Разрешение Accessibility macOS помнит по ПОДПИСИ двоичного файла, а сборка
# из исходников подписывает ad-hoc - то есть каждая пересборка даёт другую
# подпись, и система спрашивает разрешение заново. Выпущенный файл собран один
# раз и подписан один раз: подпись у версии 0.1.0 одна на всех, и разрешение,
# данное однажды, переживает и переустановку, и `brew reinstall`. Новая версия -
# новый файл, новая подпись, и разрешение придётся дать ещё раз; иначе никак,
# это устройство macOS, а не наш выбор.
#
# `--HEAD` остался запасным путём и собирает из исходников, как раньше.
#
# ПОЧЕМУ ИМЕННО BREW, А НЕ MAC APP STORE. Apple прямо перечисляет "Use of
# accessibility APIs in assistive apps" среди того, что запрещено в песочнице
# ("Protecting user data with App Sandbox"), а App Store без песочницы не
# принимает. Значит остаётся обычный исполняемый файл, и brew - его дорога.
#
# ЧТО ПРОВЕРЕНО НА ЖИВОМ МАКЕ. Обе цели выпуска собраны и запущены на маковских
# бегунках GitHub - каждая на своей архитектуре, Apple Silicon и Intel: сборка
# из macos/Makefile, подпись, таблица клавиш (`-k`), разбор файла настроек
# (`-n`) и обход вызовов Apple (`-N`). Ровно то же самое повторяется на каждый
# толчок в дерево, а не однажды при выпуске.
#
# ЧТО НЕ ПРОВЕРЕНО НИГДЕ. Всё, что требует разрешения Accessibility: на бегунке
# его нет и быть не может - его даёт человек в System Settings, а бегунок
# человека не имеет. Значит ни одно окно этим кодом ещё не подвинуто ни разу.
# `-N` на бегунке отказывает на первом же вызове и честно это печатает;
# doc/macos-install.md разбирает, где проходит эта граница.
class Digitwm < Formula
  desc "Ribbon window manager for macOS: real windows, through the Accessibility API"
  homepage "https://digitable.life"
  # Apple Silicon - основной адрес, объявленный безусловно. Intel-срез
  # подменяет его ниже. Безусловный url нужен потому, что без него Homebrew
  # падает ещё до проверок системы, и человек видит след вызовов вместо
  # объяснения.
  #
  # Порядок здесь не на вкус: url обязан стоять до version, version до sha256,
  # sha256 до license. Это правило самой брю (FormulaAudit/ComponentsOrder), и
  # оно проверяется на бегунке - `brew style` покраснел ровно на нём.
  url "https://github.com/digitable-lol/digitwm/releases/download/vVERSION_PLACEHOLDER/digitwm-VERSION_PLACEHOLDER-darwin-arm64.tar.gz"
  version "VERSION_PLACEHOLDER"
  sha256 "SHA256_MACOS_ARM64_PLACEHOLDER"
  license "BSD-2-Clause"

  head do
    url "https://github.com/digitable-lol/digitwm.git", branch: "main"
    # Xcode нужен только этому пути: готовый двоичный файл уже собран.
    depends_on xcode: :build
  end

  on_macos do
    on_intel do
      url "https://github.com/digitable-lol/digitwm/releases/download/vVERSION_PLACEHOLDER/digitwm-VERSION_PLACEHOLDER-darwin-amd64.tar.gz"
      sha256 "SHA256_MACOS_AMD64_PLACEHOLDER"
    end
  end

  depends_on :macos

  def install
    if build.head?
      # Собирается только маковская цель: корневой Makefile - это X11-сборка,
      # которой на маке нечем управлять.
      system "make", "-C", "macos"
      bin.install "macos/digitwm"
      man5.install "cwmrc.5"
      doc.install "LICENSE", "LICENSE.upstream", "NOTICE", "README.md", "README.ru.md"
    else
      bin.install "digitwm"
      man5.install "cwmrc.5"
      doc.install "LICENSE", "LICENSE.upstream", "NOTICE", "README.md",
                  "README.ru.md", "doc/macos-install.md", "doc/macos-install.ru.md"
    end
  end

  def caveats
    <<~EOS
      digitwm moves windows that belong to other applications, so macOS will
      not let it do anything at all until you allow it:

        System Settings > Privacy & Security > Accessibility > + > digitwm

      The binary to add is #{opt_bin}/digitwm. digitwm asks for this itself the
      first time it is started, and then exits - the grant is read once, at
      start-up, so start it again afterwards.

      The grant is remembered against the SIGNATURE of the binary. A released
      build is signed once, so the grant survives `brew reinstall` and stays
      valid for as long as you stay on this version; upgrading to a new version
      is a new binary with a new signature, and macOS asks again. Building with
      `--HEAD` signs ad-hoc on your machine, and then every rebuild asks anew.

      What it does, before you press anything:

        digitwm -k    the key table (Control-Option-H/J/K/L moves the focus)
        digitwm -n    what it made of your ~/.cwmrc
        digitwm -N    every Apple call this port makes, one at a time, with
                      the ones that did not answer named

        man 5 cwmrc   the configuration file

      Only `-N` needs the Accessibility grant. It is also the only thing here
      that no machine without a human has ever been able to run past its first
      line: see doc/macos-install.md in #{doc}.
    EOS
  end

  test do
    # Ни одна из этих не трогает окна и не требует разрешения: одна печатает
    # таблицу клавиш, другая - разбор настроек, третья читает файл с диска.
    assert_match "bind-key", shell_output("#{bin}/digitwm -k")
    (testpath/"cwmrc").write("ribbongap 12\n")
    assert_match "1 taken", shell_output("#{bin}/digitwm -n -c #{testpath}/cwmrc")

    # Страница настроек поставлена туда, где её ищет man. Формула ставит
    # двоичный файл; страница едет с ним в том же архиве и без этой строки
    # молча осталась бы в нём.
    assert_path_exists man5/"cwmrc.5"
    assert_match "CWMRC 5", (man5/"cwmrc.5").read

    # А вот это - граница, и она проверяется, а не подразумевается: без
    # разрешения Accessibility программа обязана СКАЗАТЬ об этом и выйти, а не
    # молча притвориться работающей. Здесь разрешения нет никогда: `brew test`
    # человека не спрашивает.
    refused = shell_output("#{bin}/digitwm -N", 1)
    assert_match "Accessibility", refused
  end
end
