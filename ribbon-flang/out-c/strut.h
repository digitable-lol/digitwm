/*
 * Сгенерировано flang (бэкенд C, flang/self/emit-c.flang). Не редактировать руками.
 * Модуль flang: «Strut».
 * Файл: объявления: конструкторы значений и функции программы.
 * Правьте исходник на flang и печатайте заново: любая правка здесь потеряется.
 */
#ifndef STRUT_H
#define STRUT_H

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
 * Функция flang «Полоса встречает область».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param nachalo — «начало»: число
 * @param konec — «конец»: число
 * @param polozhenie — «положение»: число
 * @param oblast — «область»: число
 * @return значение: число
 */
fl_status strut_polosa_vstrechaet_oblast(fl_ctx *ctx, fl_value nachalo, fl_value konec, fl_value polozhenie, fl_value oblast, fl_value *result, fl_error *error);

/*
 * Функция flang «Сколько отнять».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param panel — «панель»: число
 * @param ekran — «экран»: число
 * @param polozhenie — «положение»: число
 * @param oblast — «область»: число
 * @param dalniy — «дальний»: число
 * @return значение: число
 */
fl_status strut_skolko_otnyat(fl_ctx *ctx, fl_value panel, fl_value ekran, fl_value polozhenie, fl_value oblast, fl_value dalniy, fl_value *result, fl_error *error);

/*
 * Функция flang «Доля пары».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param blizhnyaya — «ближняя»: число
 * @param dalnyaya — «дальняя»: число
 * @param oblast — «область»: число
 * @param sprashivayut — «спрашивают»: число
 * @return значение: число
 */
fl_status strut_dolya_pary(fl_ctx *ctx, fl_value blizhnyaya, fl_value dalnyaya, fl_value oblast, fl_value sprashivayut, fl_value *result, fl_error *error);

/*
 * Функция flang «Пара вместе».
 *
 * Тотальная: завершение доказано анализом завершаемости (totality.mjs).
 * @param blizhnyaya — «ближняя»: число
 * @param dalnyaya — «дальняя»: число
 * @param oblast — «область»: число
 * @return значение: число
 */
fl_status strut_para_vmeste(fl_ctx *ctx, fl_value blizhnyaya, fl_value dalnyaya, fl_value oblast, fl_value *result, fl_error *error);

/*
 * Вызов функции по её исходному имени flang. Нужен прогонщику и всякому,
 * кто связывает программу с внешним миром динамически (скрипт, FFI, тест).
 */
fl_status strut_call(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error);

/*
 * ТО ЖЕ, НО ЧЕРЕЗ ГРАНИЦУ ВХОДА: объявленные типы сверяются ДО вызова.
 * Значения, пришедшие снаружи — из JSON, из другого языка, от человека, —
 * обязаны заходить ЗДЕСЬ: `_call` их не сверяет, а доказательство завершения
 * `тотальной` стоит НА ТИПЕ и вместе с типом теряется.
 */
fl_status strut_enter(fl_ctx *ctx, const char *name, const fl_value *args, size_t count,
                    fl_value *result, fl_error *error);

/*
 * Объявленные типы параметров — данными. Прогонщик сверяет по ним значения,
 * пришедшие снаружи, ДО вызова: доказательство завершения `тотальной` стоит
 * на типе, и значение вне типа выносит вместе с типом и доказательство.
 */
const fl_entry_table *strut_entry(void);

#endif /* STRUT_H */
