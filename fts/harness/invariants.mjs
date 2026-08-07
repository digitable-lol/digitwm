/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - два обещания ленты, проверенные на живом оконном менеджере
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
 * Конформанс-харнесс отвечает на вопрос «то же ли число, что в модели».  Этот
 * отвечает на другой - тот, ради которого digitwm вообще написан, и который
 * ни одна из шести моделей не выражает, потому что он говорит не об одном
 * числе, а об отношении двух состояний ленты:
 *
 *   1. открытие окна не меняет геометрию ни одного окна, уже стоящего на
 *      ленте.  Соседей сдвигают вдоль ленты, но не сжимают;
 *   2. колонка с фокусом всегда целиком во вьюпорте по горизонтали; ниже
 *      кромки стопка выходить может - вертикального скролла нет.
 *
 * Оба обещания сняты не с модели, а с двоичного файла: probe строит ленту из
 * настоящих структур, зовёт ribbon_insert() - ту же функцию, что зовёт
 * обработчик MapRequest, - и печатает состояние до и после.  Харнесс сверяет
 * два состояния между собой.
 *
 * Граница честности проведена и проверяется отдельно.  Первое обещание в
 * полном виде верно для вставки новой колонки; когда окно кладут в стопку к
 * уже существующему - `insert=stack`, - высоты в этой колонке обязаны
 * пересчитаться, иначе новому окну негде появиться.  Проверяется тогда
 * ослабленная форма: не меняется ничего за пределами этой одной колонки.
 *
 * Запуск:
 *   node fts/harness/invariants.mjs --wm ./cwm
 *   node fts/harness/invariants.mjs --wm ./cwm --selfcheck
 */

import { probeStages } from "./probe.mjs"

/* ------------------------------------------------------------------ *
 * Ключи
 * ------------------------------------------------------------------ */

function parseArgs(argv) {
	const options = { wm: "./cwm", seed: 20260804, scenarios: 320, selfcheck: false, verbose: false }
	for (let index = 0; index < argv.length; index += 1) {
		const arg = argv[index]
		if (arg === "--wm") options.wm = argv[++index]
		else if (arg === "--seed") options.seed = Number(argv[++index])
		else if (arg === "--scenarios") options.scenarios = Number(argv[++index])
		else if (arg === "--selfcheck") options.selfcheck = true
		else if (arg === "--verbose") options.verbose = true
		else throw new Error(`неизвестный ключ ${arg}`)
	}
	return options
}

/* ------------------------------------------------------------------ *
 * Корпус сценариев
 * ------------------------------------------------------------------ */

/* Одна и та же лента при каждом запуске: падение обязано воспроизводиться. */
function random(seed) {
	let state = seed >>> 0
	return () => {
		state = (state + 0x6d2b79f5) >>> 0
		let t = Math.imul(state ^ (state >>> 15), 1 | state)
		t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t
		return ((t ^ (t >>> 14)) >>> 0) / 4294967296
	}
}

const VIEWPORTS = [
	[1280, 800],
	[1920, 1080],
	[3840, 2160],
	[1024, 600],
	[800, 600],
	[400, 300],
	[320, 200],
]
const GAPS = [0, 8, 20]
const BORDERS = [0, 1, 2]
const MINIMA = [
	[120, 60],
	[400, 300],
	[1, 1],
	[500, 400],
]

function corpus(seed, count) {
	const next = random(seed)
	const pick = (list) => list[Math.floor(next() * list.length)]
	const between = (low, high) => low + Math.floor(next() * (high - low + 1))
	const scenarios = []

	for (let index = 0; index < count; index += 1) {
		const ncol = between(1, 6)
		const columns = []
		const presets = []
		for (let column = 0; column < ncol; column += 1) {
			columns.push(between(1, 4))
			presets.push(between(0, 3))
		}
		/* -1 - лента без фокуса: она бывает сразу после ухода монитора. */
		const focus = next() < 0.1 ? -1 : between(0, ncol - 1)
		const [minWidth, minHeight] = pick(MINIMA)
		scenarios.push({
			name: `сценарий ${index + 1}`,
			viewport: pick(VIEWPORTS),
			origin: next() < 0.2 ? [1920, 40] : [0, 0],
			gap: pick(GAPS),
			border: pick(BORDERS),
			"min-width": minWidth,
			"min-height": minHeight,
			columns,
			presets,
			focus,
			offset: next() < 0.3 ? between(0, 4000) : 0,
		})
	}
	return scenarios
}

function args(scenario, insert) {
	const list = [
		`viewport=${scenario.viewport[0]}x${scenario.viewport[1]}`,
		`viewport-x=${scenario.origin[0]}`,
		`viewport-y=${scenario.origin[1]}`,
		`gap=${scenario.gap}`,
		`border=${scenario.border}`,
		`min-width=${scenario["min-width"]}`,
		`min-height=${scenario["min-height"]}`,
		`columns=${scenario.columns.join(",")}`,
		`presets=${scenario.presets.join(",")}`,
		`focus=${scenario.focus}`,
		`offset=${scenario.offset}`,
	]
	if (insert) list.push(`insert=${insert}`)
	return list
}

/* ------------------------------------------------------------------ *
 * Проверки одного состояния
 * ------------------------------------------------------------------ */

const MAX = (a, b) => (a > b ? a : b)

/* Колонки стоят вплотную через зазор, и длина ленты - это правый край. */
function checkTiling(state, scenario, complaints) {
	let expected = 0
	state.columns.forEach((column) => {
		if (column.x !== expected) {
			complaints.push(`колонка ${column.index} стоит на ${column.x}, а лента дошла до ${expected}`)
		}
		expected = column.x + column.width + scenario.gap
	})
	const length = state.columns.length > 0 ? expected - scenario.gap : 0
	if (state.length !== length) complaints.push(`длина ленты ${state.length}, а колонки занимают ${length}`)
}

/* Колонка с фокусом целиком во вьюпорте по горизонтали - второе обещание ленты. */
function checkViewport(state, complaints) {
	const [, , vw] = state.viewport
	const limit = MAX(0, state.length - vw)
	if (state.offset < 0 || state.offset > limit) {
		complaints.push(`смещение ${state.offset} вне [0, ${limit}]`)
	}
	if (state.focus < 0) return
	const column = state.columns[state.focus]
	if (column === undefined) {
		complaints.push(`фокус на колонке ${state.focus}, которой нет`)
		return
	}
	if (column.width >= vw) {
		/*
		 * Колонка шире вьюпорта не помещается в него ни при каком
		 * смещении; обещание тогда читается как «видно её начало».
		 */
		if (state.offset !== column.x) {
			complaints.push(`колонка ${state.focus} шире вьюпорта, но лента стоит на ${state.offset}, а не на ${column.x}`)
		}
		return
	}
	if (state.offset > column.x) {
		complaints.push(`левый край колонки ${state.focus} (${column.x}) левее вьюпорта (${state.offset})`)
	}
	if (column.x + column.width > state.offset + vw) {
		complaints.push(`правый край колонки ${state.focus} (${column.x + column.width}) правее вьюпорта (${state.offset + vw})`)
	}
}

/* Окна колонки делят её сверху вниз, и ни одно не ниже объявленного минимума. */
function checkStacks(state, scenario, complaints) {
	state.columns.forEach((column) => {
		const windows = state.windows.filter((window) => window.column === column.index)
		if (windows.length !== column.windows) {
			complaints.push(`в колонке ${column.index} объявлено ${column.windows} окон, напечатано ${windows.length}`)
			return
		}
		let y = 0
		windows.forEach((window) => {
			const [wx, wy, ww, wh] = window.ribbon
			if (wx !== column.x) complaints.push(`окно ${column.index}.${window.index} на ${wx}, колонка на ${column.x}`)
			if (ww !== column.width) complaints.push(`окно ${column.index}.${window.index} шириной ${ww}, колонка шириной ${column.width}`)
			if (wy !== y) complaints.push(`окно ${column.index}.${window.index} на высоте ${wy}, стопка дошла до ${y}`)
			if (wh < scenario["min-height"]) {
				complaints.push(`окно ${column.index}.${window.index} высотой ${wh} ниже минимума ${scenario["min-height"]}`)
			}
			y = wy + wh + scenario.gap
		})
		checkStackHeight(state, scenario, column, windows, y, complaints)
	})
}

/*
 * Куда доходит низ стопки. Обещание «колонка заполняет вьюпорт ровно» верно
 * только пока равная доля не упёрлась в минимальную высоту: минимум объявлен
 * свойством модели (fts/window-height.fts, «Высота не меньше минимальной»), и
 * когда окон столько, что доля меньше минимума, модель сознательно выбирает
 * минимум, а не вьюпорт. Тогда стопка вылезает вниз, и ровно на предсказуемую
 * величину. Проверяем оба случая, чтобы исключение было записано, а не
 * подразумевалось: до этой проверки харнесс смотрел только горизонталь.
 */
function checkStackHeight(state, scenario, column, windows, y, complaints) {
	if (windows.length === 0) return
	const vh = state.viewport[3]
	const minh = scenario["min-height"]
	const gaps = (windows.length - 1) * scenario.gap
	const bottom = y - scenario.gap
	const share = Math.floor((vh - gaps) / windows.length)
	if (share >= minh) {
		if (bottom !== vh) {
			complaints.push(`колонка ${column.index}: доля ${share} не ниже минимума ${minh}, стопка обязана заполнить вьюпорт ${vh}, а дошла до ${bottom}`)
		}
		return
	}
	const expected = windows.length * minh + gaps
	if (bottom !== expected) {
		complaints.push(`колонка ${column.index}: доля ${share} ниже минимума ${minh}, стопка обязана дойти до ${expected} (${windows.length}x${minh} плюс зазоры), а дошла до ${bottom}`)
	}
}

/* Экранная геометрия - это геометрия ленты, сдвинутая на смещение. */
function checkScreen(state, scenario, complaints) {
	const [vx, vy] = state.viewport
	state.windows.forEach((window) => {
		const [rx, ry, rw, rh] = window.ribbon
		const expected = [
			vx + rx - state.offset,
			vy + ry,
			MAX(1, rw - scenario.border * 2),
			MAX(1, rh - scenario.border * 2),
		]
		if (JSON.stringify(window.screen) !== JSON.stringify(expected)) {
			complaints.push(`окно ${window.column}.${window.index} на экране ${window.screen}, а из ленты выходит ${expected}`)
		}
	})
}

function checkState(state, scenario, complaints) {
	checkTiling(state, scenario, complaints)
	checkViewport(state, complaints)
	checkStacks(state, scenario, complaints)
	checkScreen(state, scenario, complaints)
}

/* ------------------------------------------------------------------ *
 * Проверка отношения двух состояний
 * ------------------------------------------------------------------ */

function windowsOf(state, columnIndex) {
	return state.windows.filter((window) => window.column === columnIndex)
}

/*
 * Новая колонка.  Она встаёт сразу за колонкой с фокусом, поэтому колонки
 * левее места вставки не двигаются вовсе, а колонки правее сдвигаются вдоль
 * ленты - все на одно и то же число.  Ни одна не меняет ширины, ни одно окно
 * не меняет высоты: в этом всё обещание.
 */
function checkInsertColumn(before, after, scenario, complaints) {
	if (after.columnCount !== before.columnCount + 1) {
		complaints.push(`колонок было ${before.columnCount}, стало ${after.columnCount}`)
		return
	}
	const at = before.focus >= 0 ? before.focus + 1 : before.columnCount
	if (after.focus !== at) complaints.push(`фокус после вставки на ${after.focus}, ожидался на ${at}`)

	const fresh = after.columns[at]
	if (fresh === undefined) {
		complaints.push(`новой колонки ${at} нет`)
		return
	}
	if (fresh.windows !== 1) complaints.push(`в новой колонке ${fresh.windows} окон, а вставляли одно`)

	const shift = fresh.width + scenario.gap
	before.columns.forEach((old) => {
		const moved = old.index < at ? old.index : old.index + 1
		const now = after.columns[moved]
		if (now === undefined) {
			complaints.push(`колонка ${old.index} пропала после вставки`)
			return
		}
		const expectedX = old.index < at ? old.x : old.x + shift
		if (now.width !== old.width) complaints.push(`колонка ${old.index} была шириной ${old.width}, стала ${now.width}`)
		if (now.preset !== old.preset) complaints.push(`колонка ${old.index} сменила пресет ${old.preset} на ${now.preset}`)
		if (now.windows !== old.windows) complaints.push(`в колонке ${old.index} было ${old.windows} окон, стало ${now.windows}`)
		if (now.x !== expectedX) complaints.push(`колонка ${old.index} стоит на ${now.x}, а сдвиг обещал ${expectedX}`)

		const was = windowsOf(before, old.index)
		const is = windowsOf(after, moved)
		was.forEach((window, index) => {
			const other = is[index]
			if (other === undefined) {
				complaints.push(`окно ${old.index}.${index} пропало после вставки`)
				return
			}
			const expected = [window.ribbon[0] + (old.index < at ? 0 : shift), window.ribbon[1], window.ribbon[2], window.ribbon[3]]
			if (JSON.stringify(other.ribbon) !== JSON.stringify(expected)) {
				complaints.push(`окно ${old.index}.${index} было ${window.ribbon}, стало ${other.ribbon}, а сдвиг обещал ${expected}`)
			}
		})
	})
}

/*
 * Стопка.  Здесь обещание слабее и названо вслух: окно кладут в колонку с
 * фокусом, её высоты обязаны пересчитаться, а всё остальное на ленте - нет.
 */
function checkInsertStack(before, after, complaints) {
	if (after.columnCount !== before.columnCount) {
		complaints.push(`колонок было ${before.columnCount}, стало ${after.columnCount}`)
		return
	}
	if (after.focus !== before.focus) complaints.push(`фокус ушёл с ${before.focus} на ${after.focus}`)

	before.columns.forEach((old) => {
		const now = after.columns[old.index]
		if (now.x !== old.x) complaints.push(`колонка ${old.index} переехала с ${old.x} на ${now.x}`)
		if (now.width !== old.width) complaints.push(`колонка ${old.index} была шириной ${old.width}, стала ${now.width}`)

		if (old.index === before.focus) {
			if (now.windows !== old.windows + 1) {
				complaints.push(`в колонке с фокусом было ${old.windows} окон, стало ${now.windows}`)
			}
			return
		}
		if (now.windows !== old.windows) complaints.push(`в колонке ${old.index} было ${old.windows} окон, стало ${now.windows}`)

		const was = windowsOf(before, old.index)
		const is = windowsOf(after, old.index)
		was.forEach((window, index) => {
			const other = is[index]
			if (other === undefined || JSON.stringify(other.ribbon) !== JSON.stringify(window.ribbon)) {
				complaints.push(`окно ${old.index}.${index} было ${window.ribbon}, стало ${other && other.ribbon} - а колонка не та, куда вставляли`)
			}
		})
	})
}

/* ------------------------------------------------------------------ *
 * Прогон
 * ------------------------------------------------------------------ */

function judge(scenario, insert, stages) {
	const complaints = []
	stages.forEach((state) => checkState(state, scenario, complaints))
	const [before, after] = stages
	if (after !== undefined) {
		if (insert === "column") checkInsertColumn(before, after, scenario, complaints)
		else checkInsertStack(before, after, complaints)
	}
	return complaints
}

function clone(value) {
	return JSON.parse(JSON.stringify(value))
}

/*
 * Зелёный харнесс, который не умеет краснеть, не доказывает ничего.  Три
 * поломки - по одной на каждое семейство проверок - вносятся в уже полученный
 * ответ живого оконного менеджера, и каждая обязана быть замечена.
 */
function selfcheck(wm, scenario) {
	const stages = probeStages(wm, args(scenario, "column"))
	/*
	 * Каждая поломка названа вместе с жалобой, которую она обязана вызвать:
	 * иначе харнесс мог бы «замечать» её посторонней проверкой и оставлять
	 * ту, ради которой поломка внесена, слепой.
	 */
	const mutations = [
		[
			"существующая колонка сменила пресет при вставке",
			"сменила пресет",
			(states) => {
				states[1].columns[0].preset += 1
			},
		],
		[
			"существующее окно стало ниже при вставке",
			"а сдвиг обещал",
			(states) => {
				states[1].windows[0].ribbon[3] -= 1
			},
		],
		[
			"колонка с фокусом выехала из вьюпорта",
			"смещение",
			(states) => {
				states[1].offset += states[1].columns[states[1].focus].width + 1
			},
		],
		[
			"колонка съехала с сетки",
			"а лента дошла до",
			(states) => {
				states[1].columns[states[1].columns.length - 1].x += 3
			},
		],
	]

	let missed = 0
	for (const [name, expected, breakIt] of mutations) {
		const broken = clone(stages)
		breakIt(broken)
		const complaints = judge(scenario, "column", broken)
		const found = complaints.find((complaint) => complaint.includes(expected))
		if (found === undefined) {
			console.error(`не замечено: ${name} — ждали жалобу со словами «${expected}», получили ${JSON.stringify(complaints)}`)
			missed += 1
		} else {
			console.log(`ok ${name}: ${found}`)
		}
	}
	if (missed > 0) {
		console.error(`\nпропущено поломок: ${missed}`)
		return 1
	}
	console.log(`\nвсе ${mutations.length} поломки замечены`)
	return 0
}

function main() {
	const options = parseArgs(process.argv.slice(2))
	const scenarios = corpus(options.seed, options.scenarios)

	if (options.selfcheck) {
		return selfcheck(options.wm, scenarios[0])
	}

	console.log(`wm:        ${options.wm}`)
	console.log(`сценариев: ${scenarios.length} (зерно ${options.seed})`)
	console.log("")

	const failures = []
	let checks = 0
	let windows = 0

	for (const scenario of scenarios) {
		/* Стопка требует колонки с фокусом - без него ribbon_insert() откажет. */
		const inserts = scenario.focus >= 0 ? ["column", "stack"] : ["column"]
		for (const insert of inserts) {
			let stages
			try {
				stages = probeStages(options.wm, args(scenario, insert))
			} catch (error) {
				failures.push(`${scenario.name} [${insert}]: ${error.message}`)
				continue
			}
			if (stages.length !== 2) {
				failures.push(`${scenario.name} [${insert}]: состояний ${stages.length}, ожидалось 2`)
				continue
			}
			const complaints = judge(scenario, insert, stages)
			checks += 1
			windows += stages[1].windows.length
			if (complaints.length > 0) {
				failures.push(`${scenario.name} [${insert}] ${JSON.stringify(args(scenario, insert).join(" "))}\n      ${complaints.join("\n      ")}`)
			} else if (options.verbose) {
				console.log(`  ok ${scenario.name} [${insert}]`)
			}
		}
	}

	console.log(`вставок проверено: ${checks}, окон в конечных состояниях: ${windows}`)
	if (failures.length > 0) {
		console.error(`\nнарушений: ${failures.length}\n`)
		for (const failure of failures.slice(0, 20)) console.error(`  ${failure}`)
		if (failures.length > 20) console.error(`  ... и ещё ${failures.length - 20}`)
		console.error("\nобещание ленты нарушено - сборка остановлена")
		return 1
	}
	console.log("открытие окна не тронуло ни одного существующего; колонка с фокусом всегда во вьюпорте")
	return 0
}

process.exitCode = main()
