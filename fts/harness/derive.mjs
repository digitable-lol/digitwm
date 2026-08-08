/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - от общего вектора к входу FTS и к аргументам layout-probe
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
 * Один вектор - две стороны сверки.  В `layout-probe` уходят ровно те поля,
 * которые принимает функция на C; в модель FTS уходят они же плюс несколько
 * разностей.
 *
 * Разности здесь не прихоть, а следствие языка: условие в FTS сравнивает одно
 * поле с константой или с процентом от одного поля, и сложить два поля в
 * условии нечем.  Спецификация `DGT-WM` формулирует лекарство так: «разность
 * считает приложение и передаёт полем».  Граница, которую этот файл держит:
 *
 *   - сюда попадают только линейные комбинации входных чисел;
 *   - ни одного `if`, ни одного `min`/`max`, ни одного порога - всё это
 *     политика, и она живёт в моделях `fts/*.fts`;
 *   - единственное исключение - целочисленное деление в «Высоте окна в
 *     колонке»: у FTS нет ни деления на поле, ни остатка, и обойти это
 *     разностью нельзя.  Оно помечено отдельно и разобрано в `fts/README.md`.
 *
 * Порядок полей в `fields` - канонический: обе поверхности модели обязаны
 * объявлять поля в этом же порядке и в этом же количестве, и харнесс это
 * проверяет.
 */

const int = (value) => {
	if (!Number.isInteger(value)) throw new Error(`ожидалось целое, получено ${value}`)
	return value
}

/* Деление как в C99: усечение к нулю. */
const cdiv = (a, b) => Math.trunc(a / b)

/**
 * Семь утилит.  `probe` - имена аргументов `layout-probe`, они же ключи
 * вектора; `fields` - каноническая последовательность полей модели.
 */
export const UTILITIES = [
	{
		name: "scroll-offset",
		ru: "Смещение ленты после фокуса",
		probe: ["viewport-width", "column-left", "column-width", "offset", "gap", "ribbon-length"],
		truncate: false,
		fields: [
			["viewport-width", "ширина вьюпорта", (v) => int(v["viewport-width"])],
			["column-left", "левый край колонки", (v) => int(v["column-left"])],
			["column-width", "ширина колонки", (v) => int(v["column-width"])],
			["offset", "смещение", (v) => int(v.offset)],
			["gap", "зазор", (v) => int(v.gap)],
			["ribbon-length", "длина ленты", (v) => int(v["ribbon-length"])],
			/* ширина вьюпорта минус ширина колонки */
			["viewport-slack", "запас вьюпорта", (v) => v["viewport-width"] - v["column-width"]],
			/* левый край колонки минус смещение */
			["left-slack", "запас слева", (v) => v["column-left"] - v.offset],
			/* правый край вьюпорта минус правый край колонки */
			["right-slack", "запас справа", (v) =>
				v.offset + v["viewport-width"] - v["column-left"] - v["column-width"]],
			/* длина ленты минус ширина вьюпорта */
			["scroll-limit", "предел прокрутки", (v) => v["ribbon-length"] - v["viewport-width"]],
			/* правый край колонки минус ширина вьюпорта */
			["overhang", "выход за вьюпорт", (v) =>
				v["column-left"] + v["column-width"] - v["viewport-width"]],
			/* предел прокрутки минус левый край колонки */
			["limit-less-left", "предел минус левый край", (v) =>
				v["ribbon-length"] - v["viewport-width"] - v["column-left"]],
			/* предел прокрутки минус выход за вьюпорт */
			["limit-less-right", "предел минус правый край", (v) =>
				v["ribbon-length"] - v["column-left"] - v["column-width"]],
		],
	},
	{
		/*
		 * Вторая ось. Разности те же, что у «scroll-offset», повёрнутые:
		 * там вьюпорт догоняет колонку по горизонтали, здесь - окно с
		 * фокусом по вертикали. Единица разная не по недосмотру: колонку
		 * ограничивает её ширина, а стопка бывает выше любого вьюпорта, и
		 * обещать про неё целиком нечего.
		 */
		name: "stack-offset",
		ru: "Смещение полотна после фокуса",
		probe: ["viewport-height", "window-top", "window-height", "offset", "gap", "canvas-height"],
		truncate: false,
		fields: [
			["viewport-height", "высота вьюпорта", (v) => int(v["viewport-height"])],
			["window-top", "верхний край окна", (v) => int(v["window-top"])],
			["window-height", "высота окна", (v) => int(v["window-height"])],
			["offset", "смещение", (v) => int(v.offset)],
			["gap", "зазор", (v) => int(v.gap)],
			["canvas-height", "высота полотна", (v) => int(v["canvas-height"])],
			/* высота вьюпорта минус высота окна */
			["viewport-slack", "запас вьюпорта", (v) => v["viewport-height"] - v["window-height"]],
			/* верхний край окна минус смещение */
			["top-slack", "запас сверху", (v) => v["window-top"] - v.offset],
			/* нижний край вьюпорта минус нижний край окна */
			["bottom-slack", "запас снизу", (v) =>
				v.offset + v["viewport-height"] - v["window-top"] - v["window-height"]],
			/* высота полотна минус высота вьюпорта */
			["scroll-limit", "предел прокрутки", (v) => v["canvas-height"] - v["viewport-height"]],
			/* нижний край окна минус высота вьюпорта */
			["overhang", "выход за вьюпорт", (v) =>
				v["window-top"] + v["window-height"] - v["viewport-height"]],
			/* предел прокрутки минус верхний край окна */
			["limit-less-top", "предел минус верхний край", (v) =>
				v["canvas-height"] - v["viewport-height"] - v["window-top"]],
			/* предел прокрутки минус выход за вьюпорт */
			["limit-less-bottom", "предел минус нижний край", (v) =>
				v["canvas-height"] - v["window-top"] - v["window-height"]],
		],
	},
	{
		name: "column-width",
		ru: "Ширина колонки по пресету",
		probe: ["viewport-width", "preset", "gap", "min-width"],
		/*
		 * Единственная утилита, чей результат харнесс усекает перед
		 * сравнением: процент от ширины - точная дробь, а C делит нацело.
		 * Усечение именованное и проверяемое: расхождение допускается
		 * только внутри одной единицы, см. conformance.mjs.
		 */
		truncate: true,
		fields: [
			["viewport-width", "ширина вьюпорта", (v) => int(v["viewport-width"])],
			["preset", "номер пресета", (v) => int(v.preset)],
			["gap", "зазор", (v) => int(v.gap)],
			["min-width", "минимальная ширина", (v) => int(v["min-width"])],
			/* ширина вьюпорта минус зазор */
			["inner-width", "ширина без зазора", (v) => v["viewport-width"] - v.gap],
		],
	},
	{
		name: "window-height",
		ru: "Высота окна в колонке",
		probe: ["viewport-height", "window-count", "window-index", "gap", "min-height"],
		truncate: false,
		fields: [
			["viewport-height", "высота вьюпорта", (v) => int(v["viewport-height"])],
			["window-count", "число окон", (v) => int(v["window-count"])],
			["window-index", "номер окна", (v) => int(v["window-index"])],
			["gap", "зазор", (v) => int(v.gap)],
			["min-height", "минимальная высота", (v) => int(v["min-height"])],
			/* число окон минус одно */
			["last-index", "номер последнего окна", (v) => v["window-count"] - 1],
			/*
			 * Целая доля высоты и остаток от деления.  Здесь и только
			 * здесь харнесс делает то, чего у языка нет: делит на поле.
			 * Само решение - «остаток достаётся последнему окну» -
			 * осталось правилом модели.
			 */
			["even-share", "равная доля", (v) => share(v)],
			["remainder", "остаток", (v) => remainder(v)],
			["last-height", "высота последнего окна", (v) => share(v) + remainder(v)],
		],
	},
	{
		name: "insertion",
		ru: "Куда вставить окно",
		probe: ["has-focus", "transient", "dialog", "dock", "fullscreen", "rule"],
		truncate: false,
		fields: [
			["has-focus", "есть фокус", (v) => bool(v["has-focus"])],
			["transient", "дочернее окно", (v) => bool(v.transient)],
			["dialog", "диалог", (v) => bool(v.dialog)],
			["dock", "док", (v) => bool(v.dock)],
			["fullscreen", "полноэкранное", (v) => bool(v.fullscreen)],
			/*
			 * В probe это поле зовётся `rule`; на английской поверхности
			 * так его назвать нельзя.  Английские фразы переписываются в
			 * русские по началу строки и без оглядки на контекст, поэтому
			 * объявление поля `rule is number` превращается в объявление
			 * правила и разбор падает с FTS_NATURAL_FIELD.
			 */
			["config-rule", "правило конфигурации", (v) => int(v.rule)],
		],
	},
	{
		name: "focus-after-close",
		ru: "Фокус после закрытия",
		probe: ["column-index", "column-count", "last-column", "only-window"],
		truncate: false,
		fields: [
			["column-index", "номер колонки", (v) => int(v["column-index"])],
			["column-count", "число колонок", (v) => int(v["column-count"])],
			["last-column", "последняя колонка", (v) => bool(v["last-column"])],
			["only-window", "единственное окно", (v) => bool(v["only-window"])],
			/* число колонок минус закрывшаяся */
			["columns-left", "колонок после закрытия", (v) => v["column-count"] - 1],
			/* номер последней из оставшихся */
			["last-remaining", "номер последней колонки после закрытия", (v) => v["column-count"] - 2],
		],
	},
	{
		name: "output-change",
		ru: "Смещение после смены монитора",
		probe: ["viewport-width", "offset", "ribbon-length"],
		truncate: false,
		fields: [
			["viewport-width", "ширина вьюпорта", (v) => int(v["viewport-width"])],
			["offset", "смещение", (v) => int(v.offset)],
			["ribbon-length", "длина ленты", (v) => int(v["ribbon-length"])],
			/* длина ленты минус ширина вьюпорта */
			["scroll-limit", "предел прокрутки", (v) => v["ribbon-length"] - v["viewport-width"]],
		],
	},
]

function bool(value) {
	if (typeof value === "boolean") return value
	if (value === 0 || value === 1) return value === 1
	throw new Error(`ожидался признак, получено ${JSON.stringify(value)}`)
}

/* Высота колонки за вычетом зазоров между окнами. */
function total(vector) {
	const n = vector["window-count"]
	return vector["viewport-height"] - vector.gap * (n - 1)
}

function share(vector) {
	const n = vector["window-count"]
	return n > 0 ? cdiv(total(vector), n) : 0
}

/* Остаток от того же деления: колонка без него не заполняет вьюпорт ровно. */
function remainder(vector) {
	const n = vector["window-count"]
	return n > 0 ? total(vector) - share(vector) * n : 0
}

export const byName = new Map(UTILITIES.map((utility) => [utility.name, utility]))

/** Вход утилиты FTS на выбранной поверхности: "en" - имена полей латиницей. */
export function ftsInput(utility, vector, surface) {
	const input = {}
	for (const [en, ru, compute] of utility.fields) {
		input[surface === "en" ? en : ru] = compute(vector)
	}
	return input
}

/** Аргументы layout-probe: только то, что принимает функция на C. */
export function probeArgs(utility, vector) {
	return utility.probe.map((key) => {
		const value = vector[key]
		if (value === undefined) throw new Error(`в векторе нет поля "${key}"`)
		return `${key}=${typeof value === "boolean" ? String(value) : value}`
	})
}
