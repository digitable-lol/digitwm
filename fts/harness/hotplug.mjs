/*
 * digitwm - монитор ушёл и вернулся: что стало с лентами
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
 * DGT-WM-08 обещает три вещи: у каждого выхода своя лента и своё смещение,
 * отключение монитора не теряет колонок, возврат восстанавливает
 * конфигурацию.  Все три - об отношении состояний до и после события RandR,
 * поэтому проверяются здесь, а не моделью.
 *
 * Настоящего hotplug на программном X-сервере не бывает: Xvfb не умеет
 * приносить и уносить выходы.  Поэтому событие подаётся туда же, куда его
 * подаёт обработчик RRScreenChangeNotify, - в ribbon_screen_relayout(), из
 * которой вынуты все вызовы X.  Проверено этим ровно то, что считает лента;
 * что при этом делает X-сервер, здесь не проверено и в doc/monitors.md
 * сказано прямо.
 *
 * Запуск:
 *   node fts/harness/hotplug.mjs --wm ./cwm
 *   node fts/harness/hotplug.mjs --wm ./cwm --selfcheck
 */

import { probeOutputs } from "./probe.mjs"

function parseArgs(argv) {
	const options = { wm: "./cwm", selfcheck: false, verbose: false }
	for (let index = 0; index < argv.length; index += 1) {
		const arg = argv[index]
		if (arg === "--wm") options.wm = argv[++index]
		else if (arg === "--selfcheck") options.selfcheck = true
		else if (arg === "--verbose") options.verbose = true
		else throw new Error(`неизвестный ключ ${arg}`)
	}
	return options
}

/*
 * Сценарии названы тем, что в них происходит.  «then» - набор выходов сразу
 * после события, «after» - после второго события; так одним сценарием
 * описывается и отключение, и возврат.
 */
const SCENARIOS = [
	{
		name: "второй монитор отключили и вернули",
		outputs: "HDMI-1:1920x1080+0+0,DP-1:1280x800+1920+0",
		columns: "HDMI-1:2.1.3,DP-1:1.1",
		focus: "HDMI-1:2,DP-1:1",
		then: "HDMI-1:1920x1080+0+0",
		after: "HDMI-1:1920x1080+0+0,DP-1:1280x800+1920+0",
		detached: ["DP-1"],
		restored: true,
	},
	{
		name: "отключили первый, не второй",
		outputs: "HDMI-1:1920x1080+0+0,DP-1:1280x800+1920+0",
		columns: "HDMI-1:1.1,DP-1:2.2.2",
		focus: "HDMI-1:0,DP-1:2",
		then: "DP-1:1280x800+1920+0",
		after: "HDMI-1:1920x1080+0+0,DP-1:1280x800+1920+0",
		detached: ["HDMI-1"],
		restored: true,
	},
	{
		name: "три монитора, средний ушёл",
		outputs: "A:1920x1080+0+0,B:1280x800+1920+0,C:1024x768+3200+0",
		columns: "A:1.2,B:3,C:1.1.1",
		focus: "A:1,B:0,C:2",
		then: "A:1920x1080+0+0,C:1024x768+3200+0",
		after: "A:1920x1080+0+0,B:1280x800+1920+0,C:1024x768+3200+0",
		detached: ["B"],
		restored: true,
	},
	{
		name: "монитор вернулся другим разрешением",
		outputs: "HDMI-1:2560x1440+0+0,DP-1:1280x800+2560+0",
		columns: "HDMI-1:1.1.1.1,DP-1:2",
		focus: "HDMI-1:3,DP-1:0",
		then: "HDMI-1:2560x1440+0+0",
		after: "HDMI-1:2560x1440+0+0,DP-1:1920x1080+2560+0",
		detached: ["DP-1"],
		restored: false,
	},
	{
		name: "единственный монитор сменил разрешение",
		outputs: "eDP-1:1920x1080+0+0",
		columns: "eDP-1:1.2.1.3",
		focus: "eDP-1:3",
		then: "eDP-1:1280x800+0+0",
		after: "eDP-1:1920x1080+0+0",
		detached: [],
		restored: true,
	},
	{
		name: "все мониторы ушли разом",
		outputs: "A:1920x1080+0+0,B:1280x800+1920+0",
		columns: "A:2.2,B:1",
		focus: "A:1,B:0",
		then: "A:1x1+0+0",
		after: "A:1920x1080+0+0,B:1280x800+1920+0",
		detached: ["B"],
		restored: true,
	},
	{
		name: "лента с уехавшим смещением переживает отключение",
		outputs: "A:1280x800+0+0,B:1280x800+1280+0",
		columns: "A:1.1.1.1.1,B:1.1.1",
		focus: "A:4,B:2",
		offset: "A:3000,B:1200",
		then: "A:1280x800+0+0",
		after: "A:1280x800+0+0,B:1280x800+1280+0",
		detached: ["B"],
		restored: true,
	},
]

function args(scenario) {
	const list = [
		`outputs=${scenario.outputs}`,
		`columns=${scenario.columns}`,
		`focus=${scenario.focus}`,
		"gap=8",
		"min-width=120",
		"min-height=60",
	]
	if (scenario.offset) list.push(`offset=${scenario.offset}`)
	if (scenario.then) list.push(`then=${scenario.then}`)
	if (scenario.after) list.push(`after=${scenario.after}`)
	return list
}

function shape(ribbon) {
	return {
		columns: ribbon.columns.map((column) => [column.x, column.width, column.preset, column.windows]),
		windows: ribbon.windows.map((window) => [window.column, window.index, ...window.ribbon]),
		focus: ribbon.focus,
		offset: ribbon.offset,
		length: ribbon.length,
	}
}

function same(left, right) {
	return JSON.stringify(left) === JSON.stringify(right)
}

function judge(scenario, stages) {
	const complaints = []
	const [before, then, after] = stages

	if (before === undefined || then === undefined) {
		complaints.push(`стадий ${stages.length}, ожидалось три`)
		return complaints
	}

	/* Ни одна лента не исчезает: монитор ушёл, колонки остались. */
	for (const [name, ribbon] of before.ribbons) {
		const later = then.ribbons.get(name)
		if (later === undefined) {
			complaints.push(`лента ${name} пропала вместе с монитором`)
			continue
		}
		const detached = scenario.detached.includes(name)
		if (detached && later.active !== 0) complaints.push(`лента ${name} осталась подключённой, хотя монитора нет`)
		if (!detached && later.active !== 1) complaints.push(`лента ${name} отвалилась вместе с чужим монитором`)

		if (detached && !same(shape(ribbon), shape(later))) {
			complaints.push(`лента ${name} без монитора изменилась: было ${JSON.stringify(shape(ribbon))}, стало ${JSON.stringify(shape(later))}`)
		}
		if (later.columnCount !== ribbon.columnCount) {
			complaints.push(`в ленте ${name} было ${ribbon.columnCount} колонок, стало ${later.columnCount}`)
		}
	}

	/* Ничего не переезжает с одной ленты на другую. */
	const windowsBefore = [...before.ribbons.values()].reduce((sum, ribbon) => sum + ribbon.windows.length, 0)
	for (const stage of stages) {
		const total = [...stage.ribbons.values()].reduce((sum, ribbon) => sum + ribbon.windows.length, 0)
		if (total !== windowsBefore) {
			complaints.push(`на стадии «${stage.stage}» окон ${total}, а было ${windowsBefore}`)
		}
	}

	/* Возврат монитора возвращает конфигурацию - если он вернулся тем же. */
	if (after !== undefined && scenario.restored) {
		for (const [name, ribbon] of before.ribbons) {
			const back = after.ribbons.get(name)
			if (back === undefined) {
				complaints.push(`лента ${name} не вернулась`)
				continue
			}
			if (back.active !== 1) complaints.push(`лента ${name} не подключилась обратно`)
			if (!same(shape(ribbon), shape(back))) {
				complaints.push(`лента ${name} вернулась другой: было ${JSON.stringify(shape(ribbon))}, стало ${JSON.stringify(shape(back))}`)
			}
		}
	}

	/* У каждой подключённой ленты свой вьюпорт и своё смещение. */
	for (const stage of stages) {
		for (const ribbon of stage.ribbons.values()) {
			const limit = Math.max(0, ribbon.length - ribbon.view[2])
			if (ribbon.offset < 0 || ribbon.offset > limit) {
				complaints.push(`лента ${ribbon.name} на стадии «${stage.stage}»: смещение ${ribbon.offset} вне [0, ${limit}]`)
			}
		}
		const views = [...stage.ribbons.values()].filter((ribbon) => ribbon.active === 1).map((ribbon) => ribbon.view.join(","))
		if (new Set(views).size !== views.length) {
			complaints.push(`на стадии «${stage.stage}» две подключённые ленты смотрят в один вьюпорт`)
		}
	}

	return complaints
}

function main() {
	const options = parseArgs(process.argv.slice(2))

	if (options.selfcheck) {
		/*
		 * Поломки вносятся в ответ, который живой оконный менеджер
		 * действительно дал: харнесс, не умеющий краснеть, ничего не
		 * доказывает.
		 */
		const scenario = SCENARIOS[0]
		const stages = probeOutputs(options.wm, args(scenario))
		const mutations = [
			[
				"колонки отключённой ленты потерялись",
				"изменилась",
				(states) => {
					states[1].ribbons.get("DP-1").columns.pop()
					states[1].ribbons.get("DP-1").windows.pop()
				},
			],
			[
				"окно переехало на чужую ленту",
				"окон",
				(states) => {
					states[1].ribbons.get("HDMI-1").windows.pop()
				},
			],
			[
				"монитор вернулся, а лента - нет",
				"вернулась другой",
				(states) => {
					states[2].ribbons.get("DP-1").offset += 17
				},
			],
			[
				"отключённая лента считает себя подключённой",
				"осталась подключённой",
				(states) => {
					states[1].ribbons.get("DP-1").active = 1
				},
			],
		]

		let missed = 0
		for (const [name, expected, breakIt] of mutations) {
			const broken = stages.map((stage) => ({
				stage: stage.stage,
				ribbons: new Map([...stage.ribbons].map(([key, value]) => [key, JSON.parse(JSON.stringify(value))])),
			}))
			breakIt(broken)
			const complaints = judge(scenario, broken)
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

	console.log(`wm:        ${options.wm}`)
	console.log(`сценариев: ${SCENARIOS.length}`)
	console.log("")

	const failures = []
	for (const scenario of SCENARIOS) {
		let stages
		try {
			stages = probeOutputs(options.wm, args(scenario))
		} catch (error) {
			failures.push(`«${scenario.name}»: ${error.message}`)
			continue
		}
		const complaints = judge(scenario, stages)
		if (complaints.length > 0) {
			failures.push(`«${scenario.name}»\n      ${complaints.join("\n      ")}`)
		} else if (options.verbose) {
			console.log(`  ok ${scenario.name}`)
		}
	}

	if (failures.length > 0) {
		console.error(`нарушений: ${failures.length}\n`)
		for (const failure of failures) console.error(`  ${failure}`)
		console.error("\nлента не пережила смены мониторов - сборка остановлена")
		return 1
	}
	console.log("монитор уходит и возвращается: колонки на месте, ленты не смешались, смещения восстановлены")
	return 0
}

process.exitCode = main()
