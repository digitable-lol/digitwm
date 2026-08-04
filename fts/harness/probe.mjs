/*
 * digitwm - вызов живого оконного менеджера через layout-probe
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
 * Ответ даёт тот самый двоичный файл, который потом ставится в систему:
 * `cwm -C "layout-probe ..."` не открывает дисплея и не трогает окон, но
 * считает раскладку кодом оконного менеджера, а не второй его копией.
 * Оболочка не участвует: аргумент уходит массивом, поэтому кавычки-ёлочки в
 * имени утилиты доживают до `probe_name()` в целости.
 */

import { execFileSync } from "node:child_process"

function run(wm, command) {
	let stdout
	try {
		stdout = execFileSync(wm, ["-C", command], {
			encoding: "utf8",
			stdio: ["ignore", "pipe", "pipe"],
		})
	} catch (error) {
		const detail = (error.stderr || error.message || "").toString().trim()
		throw new Error(`layout-probe отказал: ${detail || error}`)
	}
	return stdout
}

/** Одна скалярная утилита: `ok <имя> <число>`. */
export function probeScalar(wm, utility, args) {
	const command = `layout-probe ${quote(utility)} ${args.join(" ")}`
	const stdout = run(wm, command)
	const line = stdout.trim().split("\n").pop() ?? ""
	const parts = line.trim().split(/\s+/)
	if (parts[0] !== "ok" || parts.length !== 3) {
		throw new Error(`не разобран ответ layout-probe: ${JSON.stringify(line)}`)
	}
	const value = Number(parts[2])
	if (!Number.isInteger(value)) throw new Error(`layout-probe вернул не целое: ${line}`)
	return value
}

/**
 * Целый сценарий.  Возвращает разобранный вывод: вьюпорт, длина и смещение
 * ленты, колонки и геометрия каждого окна в координатах ленты и экрана.
 */
export function probeLayout(wm, args) {
	return parseLayout(run(wm, `layout-probe layout ${args.join(" ")}`))
}

/**
 * Тот же сценарий, но со всеми состояниями, которые он напечатал.  Их
 * больше одного, когда probe просили вставить окно: `initial` до вставки и
 * `column` либо `stack` после неё.
 */
export function probeStages(wm, args) {
	const stdout = run(wm, `layout-probe layout ${args.join(" ")}`)
	const stages = []
	let current = null
	for (const raw of stdout.split("\n")) {
		if (raw.trim().startsWith("stage ")) {
			if (current !== null) stages.push(current)
			current = []
		}
		if (current !== null) current.push(raw)
	}
	if (current !== null) stages.push(current)
	if (stages.length === 0) throw new Error(`не разобран вывод layout: ${stdout}`)
	return stages.map((lines) => parseLayout(lines.join("\n")))
}

/**
 * Смена мониторов: `layout-probe outputs`.  Возвращает состояния лент по
 * стадиям - до отключения, после него и после возврата монитора.
 */
export function probeOutputs(wm, args) {
	const stdout = run(wm, `layout-probe outputs ${args.join(" ")}`)
	const stages = []
	let current = null
	for (const raw of stdout.split("\n")) {
		const parts = raw.trim().split(/\s+/)
		switch (parts[0]) {
			case "stage":
				current = { stage: parts[1], ribbons: new Map() }
				stages.push(current)
				break
			case "ribbon":
				/* ribbon NAME active A view x y w h length L offset O columns N focus F */
				current.ribbons.set(parts[1], {
					name: parts[1],
					active: Number(parts[3]),
					view: numbers(parts.slice(5, 9), 4),
					length: Number(parts[10]),
					offset: Number(parts[12]),
					columnCount: Number(parts[14]),
					focus: Number(parts[16]),
					columns: [],
					windows: [],
				})
				break
			case "column":
				/* column NAME I ribbon-x X width W preset P windows N */
				current.ribbons.get(parts[1]).columns.push({
					index: Number(parts[2]),
					x: Number(parts[4]),
					width: Number(parts[6]),
					preset: Number(parts[8]),
					windows: Number(parts[10]),
				})
				break
			case "window":
				/* window NAME C I ribbon x y w h */
				current.ribbons.get(parts[1]).windows.push({
					column: Number(parts[2]),
					index: Number(parts[3]),
					ribbon: numbers(parts.slice(5, 9), 4),
				})
				break
			default:
				break
		}
	}
	if (stages.length === 0) throw new Error(`не разобран вывод outputs: ${stdout}`)
	return stages
}

function parseLayout(stdout) {
	const layout = { columns: [], windows: [] }
	for (const raw of stdout.split("\n")) {
		const parts = raw.trim().split(/\s+/)
		switch (parts[0]) {
			case "stage":
				layout.stage = parts[1]
				break
			case "viewport":
				layout.viewport = numbers(parts.slice(1), 4)
				break
			case "gap":
				layout.gap = Number(parts[1])
				break
			case "border":
				layout.border = Number(parts[1])
				break
			case "ribbon":
				/* ribbon length L offset O columns N focus F */
				layout.length = Number(parts[2])
				layout.offset = Number(parts[4])
				layout.columnCount = Number(parts[6])
				layout.focus = Number(parts[8])
				break
			case "column":
				/* column I ribbon-x X width W preset P windows N */
				layout.columns.push({
					index: Number(parts[1]),
					x: Number(parts[3]),
					width: Number(parts[5]),
					preset: Number(parts[7]),
					windows: Number(parts[9]),
				})
				break
			case "window":
				/* window C I ribbon x y w h screen x y w h */
				layout.windows.push({
					column: Number(parts[1]),
					index: Number(parts[2]),
					ribbon: numbers(parts.slice(4, 8), 4),
					screen: numbers(parts.slice(9, 13), 4),
				})
				break
			default:
				break
		}
	}
	if (layout.viewport === undefined) throw new Error(`не разобран вывод layout: ${stdout}`)
	return layout
}

function numbers(parts, count) {
	const values = parts.slice(0, count).map(Number)
	if (values.length !== count || values.some((value) => !Number.isInteger(value))) {
		throw new Error(`ожидалось ${count} целых, получено ${parts.join(" ")}`)
	}
	return values
}

/* Имя со пробелами уходит в ёлочках - в них FTS пишет имена утилит. */
function quote(name) {
	return /\s/.test(name) ? `«${name}»` : name
}
