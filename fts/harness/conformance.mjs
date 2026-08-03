/*
 * digitwm - конформанс-харнесс: модели FTS против живого оконного менеджера
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
 * Мост между исполняемой спецификацией и реализацией.  Один набор векторов
 * идёт двумя дорогами:
 *
 *   вектор -> `fts generate` -> TypeScript -> число
 *   вектор -> `cwm -C "layout-probe ..."` -> то же число
 *
 * Расхождение хоть в одном векторе валит сборку и называет утилиту,
 * поверхность и вектор.  Node здесь нужен только сборке: оконный менеджер о
 * нём не знает и знать не будет - это и есть условие, при котором digitwm
 * остаётся собираемым на NetBSD.
 *
 * Четыре проверки:
 *
 *   1. поля модели совпадают с таблицей `derive.mjs` - именами и порядком,
 *      иначе харнесс подсовывал бы модели не то, что думает;
 *   2. скалярные утилиты на векторах, обе поверхности;
 *   3. целые сценарии: цикл по колонкам повторён здесь на JS, а все числа в
 *      нём - из моделей; сверяется с выводом `layout-probe layout`;
 *   4. обе поверхности зовутся в probe своими именами - русским и
 *      английским, - и отвечают одинаково.
 *
 * Запуск:
 *   node fts/harness/conformance.mjs --fts ../fts --wm ./cwm
 */

import { execFileSync } from "node:child_process"
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import { join, resolve } from "node:path"
import { pathToFileURL } from "node:url"

import { UTILITIES, byName, ftsInput, probeArgs } from "./derive.mjs"
import { probeLayout, probeScalar } from "./probe.mjs"

const SURFACES = [
	{ id: "ru", suffix: ".fts", label: "русская" },
	{ id: "en", suffix: ".en.fts", label: "english" },
]

function parseArgs(argv) {
	const options = {
		fts: process.env.FTS_HOME ?? "../fts",
		wm: "./cwm",
		models: "fts",
		vectors: null,
		build: null,
		verbose: false,
	}
	for (let index = 0; index < argv.length; index += 1) {
		const arg = argv[index]
		if (arg === "--fts") options.fts = argv[++index]
		else if (arg === "--wm") options.wm = argv[++index]
		else if (arg === "--models") options.models = argv[++index]
		else if (arg === "--vectors") options.vectors = argv[++index]
		else if (arg === "--build") options.build = argv[++index]
		else if (arg === "--verbose") options.verbose = true
		else throw new Error(`неизвестный ключ ${arg}`)
	}
	options.vectors ??= join(options.models, "vectors")
	options.build ??= mkdtempSync(join(tmpdir(), "digitwm-fts-"))
	return options
}

const options = parseArgs(process.argv.slice(2))
const cli = resolve(options.fts, "dist/src/cli.js")
const wm = resolve(options.wm)
const failures = []
let checks = 0

function fail(where, message) {
	failures.push(`${where}: ${message}`)
}

function ok(where) {
	checks += 1
	if (options.verbose) console.log(`  ok ${where}`)
}

function fts(args) {
	try {
		return execFileSync(process.execPath, [cli, ...args], {
			encoding: "utf8",
			stdio: ["ignore", "pipe", "pipe"],
		})
	} catch (error) {
		const detail = (error.stdout || "") + (error.stderr || "")
		throw new Error(`fts ${args.join(" ")} отказал: ${detail.trim()}`)
	}
}

/*
 * Сгенерированный из модели TypeScript, загруженный как модуль.  Типы
 * снимает сам Node (24 умеет это без ключей), поэтому ни tsc, ни сборочного
 * шага между `fts generate` и сверкой нет.
 */
async function generated(model, name, surface) {
	const out = join(options.build, surface.id, name)
	fts(["generate", model, "--out", out])
	mkdirSync(out, { recursive: true })
	writeFileSync(join(out, "package.json"), '{"type":"module"}\n')
	const module = await import(pathToFileURL(join(out, "fts.utilities.ts")).href)
	return module.ftsUtilities
}

function compiled(model) {
	return JSON.parse(fts(["compile", model]))
}

/* ------------------------------------------------------------------ *
 * 1. Поля модели против таблицы харнесса
 * ------------------------------------------------------------------ */

function checkFields(document, utility, surface, where) {
	const expected = utility.fields.map(([en, ru]) => (surface.id === "en" ? en : ru))
	const structure = document.structures[0]
	const actual = (structure?.fields ?? []).map((field) => field.name)
	if (actual.join("|") !== expected.join("|")) {
		fail(where, `поля модели не совпали с таблицей харнесса\n    модель:  ${actual.join(", ")}\n    харнесс: ${expected.join(", ")}`)
		return false
	}
	ok(`${where} поля`)
	return true
}

function utilityName(utility, surface) {
	return surface.id === "en" ? utility.name : utility.ru
}

/* ------------------------------------------------------------------ *
 * 2. Скалярные утилиты на общих векторах
 * ------------------------------------------------------------------ */

function compare(utility, expected, actual) {
	if (!utility.truncate) return actual === expected ? null : `модель ${actual}, C ${expected}`
	/*
	 * Единственная поблажка во всём харнессе.  У FTS нет целочисленного
	 * деления: процент от ширины - точная дробь.  Усечение делает эта
	 * строка, и она же следит, чтобы поблажка не прикрыла настоящее
	 * расхождение: дробь обязана лежать внутри той же единицы.
	 */
	if (Math.trunc(actual) !== expected) return `модель ${actual} (усечение ${Math.trunc(actual)}), C ${expected}`
	if (Math.abs(actual - expected) >= 1) return `модель ${actual} слишком далеко от C ${expected}`
	return null
}

async function checkScalars(utility, surface, utilities, vectors) {
	const name = utilityName(utility, surface)
	const implementation = utilities[name]
	if (typeof implementation !== "function") {
		fail(`${utility.name} [${surface.id}]`, `в сгенерированном коде нет утилиты «${name}»`)
		return
	}
	for (const vector of vectors) {
		const where = `${utility.name} [${surface.id}] «${vector.name}»`
		let actual
		try {
			actual = implementation(ftsInput(utility, vector, surface.id))
		} catch (error) {
			fail(where, `модель отказала: ${error.message}`)
			continue
		}
		let expected
		try {
			/* Обе поверхности зовут probe своим именем: имена - часть модели. */
			expected = probeScalar(wm, name, probeArgs(utility, vector))
		} catch (error) {
			fail(where, `${error.message}`)
			continue
		}
		const complaint = compare(utility, expected, actual)
		if (complaint) fail(where, complaint)
		else ok(where)
	}
}

/* ------------------------------------------------------------------ *
 * 3. Целый сценарий: цикл здесь, числа - из моделей
 * ------------------------------------------------------------------ */

const MAX = (a, b) => (a > b ? a : b)

function buildLayout(policy, surface, scenario) {
	/* Вход утилиты собирается той же таблицей, что и для отдельных векторов. */
	const call = (name, vector) => policy[name](ftsInput(byName.get(name), vector, surface))

	const gap = scenario.gap
	const border = scenario.border
	const presets = scenario.presets ?? []
	const columns = scenario.columns
	let [vw, vh] = scenario.viewport
	const [vx, vy] = scenario["viewport-origin"] ?? [0, 0]
	let offset = scenario.offset ?? 0

	const measure = () => {
		/* ribbon_measure(): ширина каждой колонки, её место и длина ленты */
		const geometry = []
		let x = 0
		columns.forEach((count, index) => {
			const width = Math.trunc(call("column-width", {
				"viewport-width": vw,
				preset: presets[index] ?? 1,
				gap,
				"min-width": scenario["min-width"],
			}))
			geometry.push({ x, width, preset: presets[index] ?? 1, windows: count })
			x += width + gap
		})
		return { geometry, length: x > 0 ? x - gap : 0 }
	}

	const scroll = (state) => {
		/* ribbon_scroll(): либо на колонку с фокусом, либо просто в границы */
		const focused = state.geometry[scenario.focus]
		if (focused === undefined) {
			return call("output-change", { "viewport-width": vw, offset, "ribbon-length": state.length })
		}
		return call("scroll-offset", {
			"viewport-width": vw,
			"column-left": focused.x,
			"column-width": focused.width,
			offset,
			gap,
			"ribbon-length": state.length,
		})
	}

	let state = measure()
	offset = scroll(state)

	if (scenario.resize) {
		;[vw, vh] = scenario.resize
		state = measure()
		offset = call("output-change", { "viewport-width": vw, offset, "ribbon-length": state.length })
		state = measure()
		offset = scroll(state)
	}

	/* ribbon_place(): геометрия окна в координатах ленты и на экране */
	const windows = []
	state.geometry.forEach((column, index) => {
		let y = 0
		for (let j = 0; j < column.windows; j += 1) {
			const height = call("window-height", {
				"viewport-height": vh,
				"window-count": column.windows,
				"window-index": j,
				gap,
				"min-height": scenario["min-height"],
			})
			windows.push({
				column: index,
				index: j,
				ribbon: [column.x, y, column.width, height],
				screen: [
					vx + column.x - offset,
					vy + y,
					MAX(1, column.width - border * 2),
					MAX(1, height - border * 2),
				],
			})
			y += height + gap
		}
	})

	return {
		viewport: [vx, vy, vw, vh],
		gap,
		border,
		length: state.length,
		offset,
		columnCount: columns.length,
		focus: state.geometry[scenario.focus] === undefined ? -1 : scenario.focus,
		columns: state.geometry.map((column, index) => ({ index, ...column })),
		windows,
	}
}

function layoutArgs(scenario) {
	const args = [
		`viewport=${scenario.viewport[0]}x${scenario.viewport[1]}`,
		`gap=${scenario.gap}`,
		`border=${scenario.border}`,
		`min-width=${scenario["min-width"]}`,
		`min-height=${scenario["min-height"]}`,
		`columns=${scenario.columns.join(",")}`,
		`focus=${scenario.focus}`,
		`offset=${scenario.offset ?? 0}`,
	]
	if (scenario.presets) args.push(`presets=${scenario.presets.join(",")}`)
	if (scenario["viewport-origin"]) {
		args.push(`viewport-x=${scenario["viewport-origin"][0]}`)
		args.push(`viewport-y=${scenario["viewport-origin"][1]}`)
	}
	if (scenario.resize) args.push(`resize=${scenario.resize[0]}x${scenario.resize[1]}`)
	return args
}

function diffLayout(where, model, live) {
	const complain = (what, a, b) => fail(where, `${what}: модель ${JSON.stringify(a)}, C ${JSON.stringify(b)}`)
	let same = true
	const check = (what, a, b) => {
		if (JSON.stringify(a) !== JSON.stringify(b)) {
			complain(what, a, b)
			same = false
		}
	}
	check("вьюпорт", model.viewport, live.viewport)
	check("длина ленты", model.length, live.length)
	check("смещение", model.offset, live.offset)
	check("число колонок", model.columnCount, live.columnCount)
	check("колонка с фокусом", model.focus, live.focus)
	model.columns.forEach((column, index) => {
		const other = live.columns[index]
		check(`колонка ${index}`, [column.x, column.width, column.preset, column.windows],
			other && [other.x, other.width, other.preset, other.windows])
	})
	model.windows.forEach((window, index) => {
		const other = live.windows[index]
		check(`окно ${window.column}.${window.index} на ленте`, window.ribbon, other && other.ribbon)
		check(`окно ${window.column}.${window.index} на экране`, window.screen, other && other.screen)
	})
	if (same) ok(where)
}

/* ------------------------------------------------------------------ *
 * Прогон
 * ------------------------------------------------------------------ */

function vectorsFor(name) {
	const file = join(options.vectors, `${name}.json`)
	const parsed = JSON.parse(readFileSync(file, "utf8"))
	if (!Array.isArray(parsed) || parsed.length === 0) throw new Error(`${file}: ожидался непустой массив векторов`)
	return parsed
}

async function main() {
	console.log(`fts:      ${cli}`)
	console.log(`wm:       ${wm}`)
	console.log(`модели:   ${resolve(options.models)}`)
	console.log(`векторы:  ${resolve(options.vectors)}`)
	console.log("")

	const policies = { ru: {}, en: {} }

	for (const utility of UTILITIES) {
		const vectors = vectorsFor(utility.name)
		for (const surface of SURFACES) {
			const model = join(options.models, `${utility.name}${surface.suffix}`)
			const where = `${utility.name} [${surface.id}]`
			const document = compiled(model)
			if (!checkFields(document, utility, surface, where)) continue
			const utilities = await generated(model, utility.name, surface)
			policies[surface.id][utility.name] = utilities[utilityName(utility, surface)]
			await checkScalars(utility, surface, utilities, vectors)
		}
	}

	/* Целые сценарии - по одному разу на поверхность. */
	const scenarios = vectorsFor("layout")
	for (const surface of SURFACES) {
		const policy = policies[surface.id]
		const needed = ["scroll-offset", "column-width", "window-height", "output-change"]
		if (needed.some((name) => typeof policy[name] !== "function")) {
			fail(`layout [${surface.id}]`, "модели не сгенерировались, сценарии пропущены")
			continue
		}
		for (const scenario of scenarios) {
			const where = `layout [${surface.id}] «${scenario.name}»`
			try {
				diffLayout(where, buildLayout(policy, surface.id, scenario), probeLayout(wm, layoutArgs(scenario)))
			} catch (error) {
				fail(where, error.message)
			}
		}
	}

	console.log(`проверок: ${checks}`)
	if (failures.length > 0) {
		console.error(`\nрасхождений: ${failures.length}\n`)
		for (const failure of failures) console.error(`  ${failure}`)
		console.error("\nмодель и код разошлись - сборка остановлена")
		return 1
	}
	console.log("модели и живой оконный менеджер совпали во всех векторах")
	return 0
}

process.exitCode = await main()
