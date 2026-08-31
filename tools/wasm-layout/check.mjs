// SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life>
// SPDX-License-Identifier: BSD-2-Clause
//
// digitwm - та же ли это лента, что в двоичном файле
//
// Copyright (c) 2026 Digitable <https://digitable.life>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Сказать «в браузере считает digitwm» можно только доказав это, иначе это
// подделка - ровно та, из-за которой пересказ ленты на JavaScript был
// отвергнут.  Доказательство здесь одно и простое: тот же вопрос задаётся
// двоичному файлу и модулю WebAssembly, и ответы сравниваются по числам.
//
// Двоичный файл спрашивается тем же способом, что и в CI: `cwm -C
// 'layout-probe layout ...'`.  Модуль - вызовом dgt_probe(), который повторяет
// probe_layout() вызов в вызов (probe.c:542).  Расхождение хоть в одном числе
// - и сборка в WebAssembly не та же лента.
//
//   node tools/wasm-layout/check.mjs [--wm ./cwm] [--cases 2000]

import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, "..", "..");

let wm = resolve(root, "cwm");
let cases = 2000;
for (let i = 2; i < process.argv.length; i++) {
	if (process.argv[i] === "--wm") wm = resolve(process.argv[++i]);
	else if (process.argv[i] === "--cases") cases = Number(process.argv[++i]);
}

const WIDTHS = [33, 50, 67, 100];

// Тот же генератор, что и у остальных харнессов: зерно записано, чтобы набор
// повторялся, а не «был случайным» на словах.
let seed = 20260809;
function rnd(n) {
	seed = (seed * 1103515245 + 12345) & 0x7fffffff;
	return seed % n;
}

function makeCase() {
	const ncol = 1 + rnd(6);
	const cols = [];
	const presets = [];
	for (let i = 0; i < ncol; i++) {
		cols.push(1 + rnd(5));
		presets.push(rnd(4));
	}
	return {
		vw: 640 + rnd(1600),
		vh: 480 + rnd(1000),
		gap: rnd(17),
		minw: 60 + rnd(120),
		minh: 40 + rnd(80),
		border: rnd(4),
		cols,
		presets,
		focus: rnd(ncol),
		offset: rnd(600),
		voffset: rnd(400),
	};
}

function native(c) {
	const cmd =
		`layout-probe layout viewport=${c.vw}x${c.vh} gap=${c.gap} ` +
		`border=${c.border} min-width=${c.minw} min-height=${c.minh} ` +
		`widths=${WIDTHS.join(",")} columns=${c.cols.join(",")} ` +
		`presets=${c.presets.join(",")} focus=${c.focus} ` +
		`offset=${c.offset} voffset=${c.voffset}`;
	const out = execFileSync(wm, ["-C", cmd], { encoding: "utf8" });
	const rows = [];
	for (const line of out.split("\n")) {
		const f = line.trim().split(/\s+/);
		if (f[0] !== "window") continue;
		// window <col> <idx> ribbon x y w h screen x y w h
		rows.push([
			+f[1], +f[2],
			+f[4], +f[5], +f[6], +f[7],
			+f[9], +f[10], +f[11], +f[12],
		]);
	}
	return rows;
}

const bytes = readFileSync(resolve(here, "layout.wasm"));
const nop = () => {};
const { instance } = await WebAssembly.instantiate(bytes, {
	env: { js_move: nop, js_hide: nop, js_show: nop, js_focus: nop },
});
const api = instance.exports;
const mem = () => new Int32Array(api.memory.buffer);

function wasm(c) {
	const inp = api.dgt_in_ptr() >> 2;
	const m = mem();
	for (let i = 0; i < c.cols.length; i++) {
		m[inp + i] = c.cols[i];
		m[inp + 256 + i] = c.presets[i];
	}
	const n = api.dgt_probe(
		c.vw, c.vh, c.gap, c.minw, c.minh, c.border,
		c.cols.length, api.dgt_in_ptr(), api.dgt_in_ptr() + 1024,
		c.focus, c.offset, c.voffset,
		WIDTHS[0], WIDTHS[1], WIDTHS[2], WIDTHS[3]);
	const out = api.dgt_out_ptr() >> 2;
	const mm = mem();
	const rows = [];
	for (let i = 0; i < n; i++)
		rows.push(Array.from(mm.slice(out + i * 10, out + i * 10 + 10)));
	return rows;
}

let checked = 0;
let windows = 0;
let bad = 0;

for (let k = 0; k < cases; k++) {
	const c = makeCase();
	const a = native(c);
	const b = wasm(c);
	checked++;
	windows += a.length;

	let same = a.length === b.length;
	if (same) {
		for (let i = 0; i < a.length && same; i++)
			for (let j = 0; j < 10 && same; j++)
				if (a[i][j] !== b[i][j]) same = false;
	}
	if (!same && bad < 5) {
		console.log(`РАСХОЖДЕНИЕ на случае ${k}:`, JSON.stringify(c));
		console.log("  двоичный:", JSON.stringify(a.slice(0, 4)));
		console.log("  wasm:    ", JSON.stringify(b.slice(0, 4)));
	}
	if (!same) bad++;
}

console.log(`wm:       ${wm}`);
console.log(`модуль:   ${resolve(here, "layout.wasm")} ` +
	`(${bytes.length} байт)`);
console.log(`случаев:  ${checked}, окон в них: ${windows}`);
if (bad === 0)
	console.log("двоичный файл и WebAssembly ответили одинаково везде");
else
	console.log(`разошлись в ${bad} случаях из ${checked}`);

// Ещё одно число, ради которого всё затевалось: сколько браузер тратит на то,
// чтобы это заработало.  Модуль поднимается заново, чтобы не мерить прогретое.
const t0 = performance.now();
const fresh = await WebAssembly.instantiate(bytes, {
	env: { js_move: nop, js_hide: nop, js_show: nop, js_focus: nop },
});
const t1 = performance.now();
fresh.instance.exports.dgt_init(1280, 800, 8, 120, 60, 0, 33, 50, 67, 100);
for (let i = 0; i < 8; i++) fresh.instance.exports.dgt_open(1, 1);
const t2 = performance.now();

console.log("");
console.log(`поднять модуль:            ${(t1 - t0).toFixed(2)} мс`);
console.log(`лента из восьми окон:      ${(t2 - t1).toFixed(2)} мс`);
console.log("(Node, та же машина; в браузере добавится загрузка по сети.)");

process.exit(bad === 0 ? 0 : 1);
