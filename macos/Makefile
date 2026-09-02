# SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
# SPDX-License-Identifier: BSD-2-Clause
#
# digitwm на macOS - сборка, подпись, установка.
#
# ЭТОТ ФАЙЛ ИСПОЛНЯЕТСЯ НА КАЖДЫЙ ТОЛЧОК В ДЕРЕВО - на маковских бегунках
# GitHub, обеих архитектур (.github/workflows/macos.yml). Первая такая сборка
# прошла 2 сентября 2026: 0 ошибок компилятора и 0 предупреждений на обеих
# машинах, оба файла на Objective-C собрались против настоящих заголовков Apple
# с первого раза. До неё здесь стояло «этот файл никогда не исполнялся», и это
# было правдой полгода.
#
# Мака у людей проекта по-прежнему нет, и часть проверок так и остаётся на
# Linux: macos/check.sh собирает ТОТ ЖЕ набор объектных файлов, что и цель ниже,
# и линкует из него настоящий main(), заменив ровно два файла - wsi_ax.m и
# wsi_key.m. Это дешевле бегунка и ловит другое: расхождение ленты поверх порта
# с лентой поверх X11.
#
#   make                собрать ./digitwm
#   make app            собрать ./digitwm.app - тот же файл в пакете, с
#                       LSUIElement; нужен затем, что запущенный из терминала
#                       digitwm разрешения на себя не получает вовсе (см. ниже)
#   make install        поставить в ${PREFIX}/bin (по умолчанию /usr/local)
#   make SIGNID=...     подписать своим удостоверением, а не ad-hoc - читайте
#                       про это в doc/macos-install.md, там объяснено, почему
#                       ad-hoc заставляет macOS спрашивать разрешение заново
#                       после каждой пересборки
#   make clean
#
# На Linux эта цель не собирается и не должна: здесь проверяется другое и
# другим - sh macos/check.sh и sh macos/stub-build.sh.
#
# ЗАГЛУШКА ЗАГОЛОВКОВ X11 НА МАКЕ - НЕ НЕДОРАЗУМЕНИЕ, А НАЗВАННЫЙ ДОЛГ.
# calmwm.h включает Xlib ради четырёх типов в описаниях структур, и пока он не
# разрезан (doc/macos.md, «два места, где контракт и macOS не встречаются»,
# пункт 2), всё, что включает calmwm.h, требует этих заголовков. На маке их
# нет, поэтому macos/fakex.sh кладёт в ${OBJDIR} те же двадцать строк, что
# подставляет macos/check.sh. Одна заглушка, два места, ни одной копии.

PROG=		digitwm
PREFIX?=	/usr/local
CC?=		cc
OBJDIR?=	obj
APPDIR?=	${PROG}.app

# Ad-hoc по умолчанию, потому что для сборки из исходников больше нечего
# предположить. Цена ad-hoc названа в doc/macos-install.md: разрешение
# Accessibility привязано к подписи, и после каждой пересборки система
# спросит его заново.
SIGNID?=	-

CFLAGS?=	-Wall -Wextra -O2 -g
INCS=		-I${OBJDIR}/fakex -I.. -I.
# Security - ради трёх строк, которые `digitwm -N` печатает ДО первого отказа:
# по какому файлу его запустили, кто его запустил и каким отпечатком система
# помнит выданное ему разрешение. Все три - те грабли, на которые владелец
# наступил первым же запуском на настоящем маке.
FRAMEWORKS=	-framework ApplicationServices \
		-framework AppKit \
		-framework Carbon \
		-framework Security

OBJS=		${OBJDIR}/ribbon.o \
		${OBJDIR}/confpath.o \
		${OBJDIR}/xmalloc.o \
		${OBJDIR}/strlcpy.o \
		${OBJDIR}/strlcat.o \
		${OBJDIR}/reallocarray.o \
		${OBJDIR}/wsi_core.o \
		${OBJDIR}/wsi_conf.o \
		${OBJDIR}/wsi_run.o \
		${OBJDIR}/wsi_main.o \
		${OBJDIR}/wsi_ax.o \
		${OBJDIR}/wsi_key.o

HDRS=		../calmwm.h ../queue.h ../wsi.h ../confpath.h \
		wsi_platform.h wsi_core.h wsi_conf.h wsi_key.h wsi_run.h

all: ${PROG}

# Каталог сборки и заглушка заголовков в нём. В дерево исходников не кладётся
# ничего: obj/ целиком отдан сборке и стоит в .gitignore.
${OBJDIR}/stamp:
	mkdir -p ${OBJDIR}
	sh fakex.sh ${OBJDIR}/fakex
	touch ${OBJDIR}/stamp

${OBJDIR}/ribbon.o: ../ribbon.c ${HDRS} ${OBJDIR}/stamp
	${CC} ${CFLAGS} ${INCS} -c ../ribbon.c -o $@

# Порядок поиска файла настроек - один на две сборки, и это тот же файл,
# который компилирует ../Makefile. Разъехаться им теперь негде: копии нет.
${OBJDIR}/confpath.o: ../confpath.c ../confpath.h ${OBJDIR}/stamp
	${CC} ${CFLAGS} ${INCS} -c ../confpath.c -o $@

${OBJDIR}/xmalloc.o: ../xmalloc.c ${HDRS} ${OBJDIR}/stamp
	${CC} ${CFLAGS} ${INCS} -c ../xmalloc.c -o $@

${OBJDIR}/strlcpy.o: ../strlcpy.c ${HDRS} ${OBJDIR}/stamp
	${CC} ${CFLAGS} ${INCS} -c ../strlcpy.c -o $@

${OBJDIR}/strlcat.o: ../strlcat.c ${HDRS} ${OBJDIR}/stamp
	${CC} ${CFLAGS} ${INCS} -c ../strlcat.c -o $@

${OBJDIR}/reallocarray.o: ../reallocarray.c ${OBJDIR}/stamp
	${CC} ${CFLAGS} ${INCS} -c ../reallocarray.c -o $@

${OBJDIR}/wsi_core.o: wsi_core.c ${HDRS} ${OBJDIR}/stamp
	${CC} ${CFLAGS} ${INCS} -c wsi_core.c -o $@

${OBJDIR}/wsi_conf.o: wsi_conf.c ${HDRS} ${OBJDIR}/stamp
	${CC} ${CFLAGS} ${INCS} -c wsi_conf.c -o $@

${OBJDIR}/wsi_run.o: wsi_run.c ${HDRS} ${OBJDIR}/stamp
	${CC} ${CFLAGS} ${INCS} -c wsi_run.c -o $@

${OBJDIR}/wsi_main.o: wsi_main.c ${HDRS} ${OBJDIR}/stamp
	${CC} ${CFLAGS} ${INCS} -c wsi_main.c -o $@

# Два файла ниже линии macos/wsi_platform.h и macos/wsi_key.h. Они не включают
# calmwm.h и потому не нуждаются в заглушке X11 - но и не мешают ей.
${OBJDIR}/wsi_ax.o: wsi_ax.m wsi_platform.h ${OBJDIR}/stamp
	${CC} ${CFLAGS} ${INCS} -fobjc-arc -c wsi_ax.m -o $@

${OBJDIR}/wsi_key.o: wsi_key.m wsi_key.h ${OBJDIR}/stamp
	${CC} ${CFLAGS} ${INCS} -fobjc-arc -c wsi_key.m -o $@

# Info.plist вшивается в секцию, а не кладётся рядом: digitwm - это один
# исполняемый файл без пакета, потому что Accessibility API нельзя вызывать
# из песочницы, а без песочницы в Mac App Store не пускают (doc/macos.md,
# «Delivery»). Apple документирует и саму секцию __TEXT,__info_plist, и
# настройку Xcode, которая её делает, - но НЕ флаг линкера, которым это
# делается без Xcode: страницу руководства ld Apple больше не хостит. То есть
# строка ниже - единственное место этого Makefile, написанное не по
# документации Apple, и если она не соберётся, это первое, на что смотреть.
#
# Подпись обязательна: без неё macOS не даст разрешения вовсе. Ad-hoc по
# умолчанию - и цена ad-hoc названа в шапке.
#
# ЛИНКУЕТСЯ И ПОДПИСЫВАЕТСЯ В ${OBJDIR}, А НЕ ЗДЕСЬ, И ЭТО НЕ ПРИДИРКА.
# codesign, увидев рядом с подписываемым файлом Info.plist, считает ВЕСЬ
# КАТАЛОГ старым бесструктурным пакетом: заводит macos/_CodeSignature/
# CodeResources и запечатывает туда всё, что в каталоге лежит. Первая же сборка
# на маке это и показала - `codesign -dvv` сказал «Format=app bundle with
# Mach-O thin (arm64)» и «Sealed Resources version=2 rules=10 files=42», то
# есть подпись двоичного файла оказалась подписью каталога из 42 файлов.
#
# Цена: формула выносит файл из каталога (`bin.install "macos/digitwm"`), и
# печать начинает ссылаться на то, чего рядом больше нет. В ${OBJDIR} никакого
# Info.plist нет, поэтому подпись выходит обычной - вложенной в сам файл, - и
# переживает и вынос, и упаковку в архив. Проверка ниже не даёт этому вернуться
# молча.
${PROG}: ${OBJS} Info.plist
	${CC} -o ${OBJDIR}/digitwm ${OBJS} ${FRAMEWORKS} \
	    -Wl,-sectcreate,__TEXT,__info_plist,Info.plist
	codesign --force --sign "${SIGNID}" ${OBJDIR}/digitwm
	@codesign -dvv ${OBJDIR}/digitwm 2>&1 | sed 's/^/  подпись: /'
	@if codesign -dvv ${OBJDIR}/digitwm 2>&1 | grep -q 'Format=.*bundle' || \
	    [ -e ${OBJDIR}/_CodeSignature ] || [ -e _CodeSignature ]; then \
		echo "codesign подписал КАТАЛОГ, а не файл: он ищет в каталоге" >&2; \
		echo "Info.plist с CFBundleExecutable=digitwm и, найдя, считает" >&2; \
		echo "каталог старым бесструктурным пакетом. Подпись обязана быть" >&2; \
		echo "вложенной в файл - иначе не переживёт ни bin.install, ни" >&2; \
		echo "упаковку в архив." >&2; \
		exit 1; \
	fi
	cp ${OBJDIR}/digitwm ${PROG}

# ПАКЕТ .app - ВТОРАЯ ФОРМА ТОГО ЖЕ ФАЙЛА, а не другая программа. Внутри
# лежит ровно тот двоичный файл, что собран выше, байт в байт; разница
# снаружи, и она ровно в двух вещах.
#
# 1. КОМУ macOS ПРИПИСЫВАЕТ РАЗРЕШЕНИЕ. Запущенный из терминала digitwm прав на
#    себя не получает: система приписывает их тому, кто его запустил. Владелец
#    напоролся на это первым же запуском на настоящем маке 2 сентября 2026:
#    разрешение выдано digitwm, а digitwm по-прежнему говорит, что его нет.
#    Приложение, запущенное из /Applications, - само себе хозяин, и разрешение
#    даётся ему. Это и есть довод за пакет; всё остальное - удобство.
#
# 2. LSUIElement. Единственный ключ, который добавляется к Info.plist здесь, а
#    не лежит в нём: пакет без него завёл бы иконку в Dock и строку меню, а у
#    digitwm нет своего окна - он расставляет чужие. Ключ добавляется, а не
#    хранится вторым файлом: два плиста разошлись бы молча, а CFBundleIdentifier
#    в них обязан совпадать - по нему система помнит разрешение.
#
# ЧЕГО ПАКЕТ НЕ ЧИНИТ, и это надо говорить вслух: разрешение помнится по
# ПОДПИСИ, а ad-hoc-подпись - хеш содержимого. Пересобрал - другая подпись -
# спрашивают заново, хоть в пакете, хоть без. Лечится только постоянным
# удостоверением (SIGNID=...), и пакет здесь ни при чём.
app: ${APPDIR}

${APPDIR}: ${PROG} Info.plist
	rm -rf ${APPDIR}
	mkdir -p ${APPDIR}/Contents/MacOS
	cp Info.plist ${APPDIR}/Contents/Info.plist
	/usr/libexec/PlistBuddy -c "Add :LSUIElement bool true" \
	    ${APPDIR}/Contents/Info.plist
	cp ${PROG} ${APPDIR}/Contents/MacOS/digitwm
	codesign --force --sign "${SIGNID}" ${APPDIR}
	codesign --verify --deep --strict ${APPDIR}

install: ${PROG}
	install -d ${DESTDIR}${PREFIX}/bin
	install -m 755 ${PROG} ${DESTDIR}${PREFIX}/bin

clean:
	rm -rf ${OBJDIR} ${PROG} ${APPDIR} _CodeSignature

.PHONY: all app install clean
