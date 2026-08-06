/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - проверка того, что харнесс умеет падать
 *
 * Copyright (c) 2026 Digitable <https://digitable.life>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Зелёный харнесс ничего не значит, пока не показано, что он бывает
 * красным.  Здесь модели копируются во временный каталог, в копии портится
 * одна константа, и проверяется, что сверка на этом падает и называет
 * виновную утилиту.  Мутация, которую никто не заметил, - это дыра в
 * векторах, и она валит эту проверку так же, как расхождение валит саму
 * сверку.
 *
 * Последняя мутация правит только английскую поверхность: её обязана
 * поймать сверка поверхностей, даже если числа сошлись бы с C.
 *
 * Запуск:
 *   node fts/harness/selftest.mjs --fts ../fts --wm ./cwm
 */

import { execFileSync } from "node:child_process"
import { copyFileSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import { dirname, join, resolve } from "node:path"
import { fileURLToPath } from "node:url"

import { UTILITIES } from "./derive.mjs"

const here = dirname(fileURLToPath(import.meta.url))
const options = { fts: process.env.FTS_HOME ?? "../fts", wm: "./cwm", models: "fts" }
for (let index = 2; index < process.argv.length; index += 1) {
	if (process.argv[index] === "--fts") options.fts = process.argv[++index]
	else if (process.argv[index] === "--wm") options.wm = process.argv[++index]
	else if (process.argv[index] === "--models") options.models = process.argv[++index]
}

/*
 * Каждая мутация - одна испорченная константа и утилита, которая обязана
 * попасть в отчёт о расхождении.
 */
const MUTATIONS = [
	{
		title: "две трети колонки превращаются в 66 процентов",
		utility: "column-width",
		edits: [
			["column-width.fts", "67 процентов от поля «ширина без зазора»", "66 процентов от поля «ширина без зазора»"],
			["column-width.en.fts", "67 percent of field inner-width", "66 percent of field inner-width"],
		],
	},
	{
		title: "остаток достаётся последнему окну наполовину",
		utility: "window-height",
		edits: [
			["window-height.fts", "то добавить 100 процентов от поля остаток", "то добавить 50 процентов от поля остаток"],
			["window-height.en.fts", "then add 100 percent of field remainder", "then add 50 percent of field remainder"],
		],
	},
	{
		title: "полноэкранное окно получает код плавающего",
		utility: "insertion",
		edits: [
			["insertion.fts", "если полноэкранное равен да\n      то результат равен 3", "если полноэкранное равен да\n      то результат равен 2"],
			["insertion.en.fts", "if fullscreen equals true\n      then result equals 3", "if fullscreen equals true\n      then result equals 2"],
		],
	},
	{
		title: "поле у левого края становится вдвое уже зазора",
		utility: "scroll-offset",
		edits: [
			["scroll-offset.fts", "то добавить -100 процентов от поля зазор", "то добавить -50 процентов от поля зазор"],
			["scroll-offset.en.fts", "then add -100 percent of field gap", "then add -50 percent of field gap"],
		],
	},
	{
		title: "предел прокрутки после смены монитора занижен на десятую",
		utility: "output-change",
		edits: [
			["output-change.fts", "и смещение больше 100 процентов от поля «предел прокрутки»\n      то результат равен 100 процентов от поля «предел прокрутки»", "и смещение больше 100 процентов от поля «предел прокрутки»\n      то результат равен 90 процентов от поля «предел прокрутки»"],
			["output-change.en.fts", "and offset is greater than 100 percent of field scroll-limit\n      then result equals 100 percent of field scroll-limit", "and offset is greater than 100 percent of field scroll-limit\n      then result equals 90 percent of field scroll-limit"],
		],
	},
	{
		title: "фокус после закрытия уходит на колонку левее",
		utility: "focus-after-close",
		edits: [
			["focus-after-close.fts", "и «последняя колонка» равен да\n      то результат равен 100 процентов от поля «номер последней колонки после закрытия»", "и «последняя колонка» равен да\n      то результат равен 50 процентов от поля «номер последней колонки после закрытия»"],
			["focus-after-close.en.fts", "and last-column equals true\n      then result equals 100 percent of field last-remaining", "and last-column equals true\n      then result equals 50 percent of field last-remaining"],
		],
	},
	{
		title: "правку внесли только в английскую поверхность",
		utility: "window-height",
		surfacesOnly: true,
		edits: [
			["window-height.en.fts", "if window-count is at least 1\n      and window-index equals", "if window-count is at least 2\n      and window-index equals"],
		],
	},
]

function models(mutation) {
	const directory = mkdtempSync(join(tmpdir(), "digitwm-mutant-"))
	for (const utility of UTILITIES) {
		for (const suffix of [".fts", ".en.fts"]) {
			copyFileSync(join(options.models, utility.name + suffix), join(directory, utility.name + suffix))
		}
	}
	for (const [file, from, to] of mutation.edits) {
		const path = join(directory, file)
		const source = readFileSync(path, "utf8")
		if (!source.includes(from)) throw new Error(`в ${file} не нашлось «${from}»`)
		writeFileSync(path, source.replace(from, to))
	}
	return directory
}

/* Запуск проверки; возвращает код возврата и весь вывод. */
function run(script, directory) {
	const args = [
		join(here, script),
		"--fts", options.fts,
		"--models", directory,
	]
	if (script === "conformance.mjs") args.push("--wm", options.wm, "--vectors", join(options.models, "vectors"))
	try {
		const output = execFileSync(process.execPath, args, { encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] })
		return { code: 0, output }
	} catch (error) {
		return { code: error.status ?? 1, output: (error.stdout || "") + (error.stderr || "") }
	}
}

let failed = 0
for (const mutation of MUTATIONS) {
	const directory = models(mutation)
	const script = mutation.surfacesOnly ? "surfaces.mjs" : "conformance.mjs"
	const { code, output } = run(script, directory)
	if (code === 0) {
		failed += 1
		console.error(`НЕ ЗАМЕЧЕНО: ${mutation.title} — ${script} прошёл на испорченной модели`)
		continue
	}
	if (!output.includes(mutation.utility)) {
		failed += 1
		console.error(`НЕ НАЗВАНО: ${mutation.title} — ${script} упал, но не назвал «${mutation.utility}»`)
		continue
	}
	console.log(`ok ${mutation.title}: ${script} упал и назвал «${mutation.utility}»`)
}

if (failed > 0) {
	console.error(`\nмутаций без реакции: ${failed}; харнесс не доказал, что умеет падать`)
	process.exitCode = 1
} else {
	console.log(`\nвсе ${MUTATIONS.length} мутаций пойманы`)
}
