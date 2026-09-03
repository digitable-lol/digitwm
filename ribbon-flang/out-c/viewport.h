/*
 * Сгенерировано flang (бэкенд C, flang/self/emit-c.flang). Не редактировать руками.
 * Модуль flang: «Viewport».
 * Файл: объявления: конструкторы значений и функции программы.
 * Правьте исходник на flang и печатайте заново: любая правка здесь потеряется.
 */
#ifndef VIEWPORT_H
#define VIEWPORT_H

#include "flang_runtime.h"

/*
 * Контракт вызова: функция кладёт результат в *result и возвращает FL_OK
 * либо НЕ трогает *result и возвращает FL_ERROR, заполнив *error (его можно
 * передать NULL). Результат живёт в арене контекста — до ближайшего
 * fl_arena_reset; чтобы сохранить его надолго, скопируйте в свою память.
 *
 *   fl_arena arena;
 *   fl_ctx ctx;
 *   fl_error error;
 *   fl_value result;
 *   fl_arena_init(&arena);
 *   fl_ctx_init(&ctx, &arena);
 *   if (…(&ctx, …, &result, &error) != FL_OK) { … error.code, error.message … }
 *   fl_arena_release(&arena);
 */

/*
 * Функция flang «Смещение после смены окна».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param okno — «окно»: число
 * @param smeschenie — «смещение»: число
 * @param holst — «холст»: число
 * @return значение: число
 */
fl_status viewport_smeschenie_posle_smeny_okna(fl_ctx *ctx, fl_value okno, fl_value smeschenie, fl_value holst, fl_value *result, fl_error *error);

/*
 * Функция flang «Смещение».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param okno — «окно»: число
 * @param kolonka — «колонка»: число
 * @param shirina — «ширина»: число
 * @param smeschenie — «смещение»: число
 * @param zazor — «зазор»: число
 * @param holst — «холст»: число
 * @return значение: число
 */
fl_status viewport_smeschenie(fl_ctx *ctx, fl_value okno, fl_value kolonka, fl_value shirina, fl_value smeschenie, fl_value zazor, fl_value holst, fl_value *result, fl_error *error);

/*
 * Функция flang «Смещение по стопке».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param okno — «окно»: число
 * @param verh — «верх»: число
 * @param vysota — «высота»: число
 * @param smeschenie — «смещение»: число
 * @param zazor — «зазор»: число
 * @param holst — «холст»: число
 * @return значение: число
 */
fl_status viewport_smeschenie_po_stopke(fl_ctx *ctx, fl_value okno, fl_value verh, fl_value vysota, fl_value smeschenie, fl_value zazor, fl_value holst, fl_value *result, fl_error *error);

/*
 * Вызов функции по её исходному имени flang. Нужен прогонщику и всякому,
 * кто связывает программу с внешним миром динамически (скрипт, FFI, тест).
 */
fl_status viewport_call(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error);

/*
 * ТО ЖЕ, НО ЧЕРЕЗ ГРАНИЦУ ВХОДА: объявленные типы сверяются ДО вызова.
 * Значения, пришедшие снаружи — из JSON, из другого языка, от человека, —
 * обязаны заходить ЗДЕСЬ: `_call` их не сверяет, а доказательство завершения
 * `тотальной` стоит НА ТИПЕ и вместе с типом теряется.
 */
fl_status viewport_enter(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error);

/*
 * Объявленные типы параметров — данными. Прогонщик сверяет по ним значения,
 * пришедшие снаружи, ДО вызова: доказательство завершения `тотальной` стоит
 * на типе, и значение вне типа выносит вместе с типом и доказательство.
 */
const fl_entry_table *viewport_entry(void);

#endif /* VIEWPORT_H */
