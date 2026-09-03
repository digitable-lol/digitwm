/*
 * Сгенерировано flang (бэкенд C, flang/self/emit-c.flang). Не редактировать руками.
 * Модуль flang: «Geometry».
 * Файл: объявления: конструкторы значений и функции программы.
 * Правьте исходник на flang и печатайте заново: любая правка здесь потеряется.
 */
#ifndef GEOMETRY_H
#define GEOMETRY_H

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
 * Функция flang «Деление нацело».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param delimoe — «делимое»: число
 * @param delitel — «делитель»: число
 * @return значение: число
 */
fl_status geometry_delenie_nacelo(fl_ctx *ctx, fl_value delimoe, fl_value delitel, fl_value *result, fl_error *error);

/*
 * Функция flang «Пресет в пределах».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param preset — «пресет»: число
 * @return значение: число
 */
fl_status geometry_preset_v_predelah(fl_ctx *ctx, fl_value preset, fl_value *result, fl_error *error);

/*
 * Функция flang «Ширина колонки».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param okno — «окно»: число
 * @param dolya — «доля»: число
 * @param zazor — «зазор»: число
 * @param naimenshaya — «наименьшая»: число
 * @return значение: число
 */
fl_status geometry_shirina_kolonki(fl_ctx *ctx, fl_value okno, fl_value dolya, fl_value zazor, fl_value naimenshaya, fl_value *result, fl_error *error);

/*
 * Функция flang «Ширина колонки по пресету».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param okno — «окно»: число
 * @param preset — «пресет»: число
 * @param zazor — «зазор»: число
 * @param naimenshaya — «наименьшая»: число
 * @param dolya0 — «доля0»: число
 * @param dolya1 — «доля1»: число
 * @param dolya2 — «доля2»: число
 * @param dolya3 — «доля3»: число
 * @return значение: число
 */
fl_status geometry_shirina_kolonki_po_presetu(fl_ctx *ctx, fl_value okno, fl_value preset, fl_value zazor, fl_value naimenshaya, fl_value dolya0, fl_value dolya1, fl_value dolya2, fl_value dolya3, fl_value *result, fl_error *error);

/*
 * Функция flang «Высота окна».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param okno — «окно»: число
 * @param okon — «окон»: число
 * @param nomer — «номер»: число
 * @param zazor — «зазор»: число
 * @param naimenshaya — «наименьшая»: число
 * @return значение: число
 */
fl_status geometry_vysota_okna(fl_ctx *ctx, fl_value okno, fl_value okon, fl_value nomer, fl_value zazor, fl_value naimenshaya, fl_value *result, fl_error *error);

/*
 * Вызов функции по её исходному имени flang. Нужен прогонщику и всякому,
 * кто связывает программу с внешним миром динамически (скрипт, FFI, тест).
 */
fl_status geometry_call(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error);

/*
 * ТО ЖЕ, НО ЧЕРЕЗ ГРАНИЦУ ВХОДА: объявленные типы сверяются ДО вызова.
 * Значения, пришедшие снаружи — из JSON, из другого языка, от человека, —
 * обязаны заходить ЗДЕСЬ: `_call` их не сверяет, а доказательство завершения
 * `тотальной` стоит НА ТИПЕ и вместе с типом теряется.
 */
fl_status geometry_enter(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error);

/*
 * Объявленные типы параметров — данными. Прогонщик сверяет по ним значения,
 * пришедшие снаружи, ДО вызова: доказательство завершения `тотальной` стоит
 * на типе, и значение вне типа выносит вместе с типом и доказательство.
 */
const fl_entry_table *geometry_entry(void);

#endif /* GEOMETRY_H */
