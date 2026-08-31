/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * digitwm - две поверхности одной модели дают один канонический документ
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
 * Русская и английская поверхности FTS - не перевод, а один парсер и одна
 * каноническая модель.  Отсюда проверяемое обещание: `правила`, `свойства` и
 * `примеры` в каноническом JSON обязаны совпасть числами и операторами, а
 * различаться могут только имена - категории, объекта, полей, утилиты и
 * самих правил.
 *
 * Поэтому сверяется не JSON целиком, а его скелет: имена полей заменены их
 * номерами в объекте, имена правил выброшены, числа и операторы оставлены
 * как есть.  Если правку внесли в одну поверхность и забыли в другую,
 * расходится ровно этот скелет.
 *
 * Запуск:
 *   node fts/harness/surfaces.mjs --fts ../fts
 */

import { execFileSync } from "node:child_process"
import { join, resolve } from "node:path"

import { UTILITIES } from "./derive.mjs"

const options = { fts: process.env.FTS_HOME ?? "../fts", models: "fts" }
for (let index = 0; index < process.argv.length - 2; index += 1) {
	const arg = process.argv[index + 2]
	if (arg === "--fts") options.fts = process.argv[index + 3]
	else if (arg === "--models") options.models = process.argv[index + 3]
}
const cli = resolve(options.fts, "dist/src/cli.js")

function compile(file) {
	try {
		return JSON.parse(execFileSync(process.execPath, [cli, "compile", file], {
			encoding: "utf8",
			stdio: ["ignore", "pipe", "pipe"],
		}))
	} catch (error) {
		throw new Error(`не компилируется ${file}: ${(error.stdout || error.stderr || "").toString().trim()}`)
	}
}

/* Скелет документа: всё, кроме имён. */
function skeleton(document) {
	const structure = document.structures[0]
	const index = new Map(structure.fields.map((field, position) => [field.name, position]))
	const field = (name) => {
		if (!index.has(name)) throw new Error(`поле «${name}» не объявлено в объекте`)
		return index.get(name)
	}
	const operand = (value) => {
		if (value.kind === "field") return { kind: "field", field: field(value.field) }
		if (value.kind === "percent") return { kind: "percent", percent: value.percent, field: field(value.field) }
		return value
	}
	return {
		fields: structure.fields.map((item) => item.type),
		utilities: (document.utilities ?? []).map((utility) => ({
			output: utility.output,
			initial: utility.initial,
			rules: utility.rules.map((rule) => ({
				when: rule.when.map((condition) => ({
					field: field(condition.field),
					operator: condition.operator,
					value: operand(condition.value),
				})),
				action: { kind: rule.action.kind, value: operand(rule.action.value) },
			})),
			properties: utility.properties.map((property) => ({
				operator: property.operator,
				value: operand(property.value),
			})),
			examples: utility.examples.map((example) => ({
				input: structure.fields.map((item) => {
					if (!(item.name in example.input)) throw new Error(`в примере нет поля «${item.name}»`)
					return example.input[item.name]
				}),
				expected: example.expected,
			})),
		})),
	}
}

/* Где именно разошлось - иначе сообщение «не совпало» бесполезно. */
function difference(left, right, path = "$") {
	if (JSON.stringify(left) === JSON.stringify(right)) return null
	if (Array.isArray(left) && Array.isArray(right)) {
		if (left.length !== right.length) return `${path}: ${left.length} против ${right.length}`
		for (let index = 0; index < left.length; index += 1) {
			const found = difference(left[index], right[index], `${path}[${index}]`)
			if (found) return found
		}
	}
	if (left && right && typeof left === "object" && typeof right === "object") {
		for (const key of new Set([...Object.keys(left), ...Object.keys(right)])) {
			const found = difference(left[key], right[key], `${path}.${key}`)
			if (found) return found
		}
	}
	return `${path}: ${JSON.stringify(left)} против ${JSON.stringify(right)}`
}

let failed = 0
for (const utility of UTILITIES) {
	const ru = join(options.models, `${utility.name}.fts`)
	const en = join(options.models, `${utility.name}.en.fts`)
	let complaint = null
	try {
		complaint = difference(skeleton(compile(ru)), skeleton(compile(en)))
	} catch (error) {
		complaint = error.message
	}
	if (complaint) {
		failed += 1
		console.error(`${utility.name}: поверхности разошлись — ${complaint}`)
	} else {
		console.log(`ok ${utility.name}: обе поверхности дают один канонический документ`)
	}
}

if (failed > 0) {
	console.error(`\nрасхождений: ${failed}; правка модели идёт в обе поверхности одним коммитом`)
	process.exitCode = 1
}
